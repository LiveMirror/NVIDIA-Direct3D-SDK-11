// Copyright (c) 2011 NVIDIA Corporation. All rights reserved.
//
// TO  THE MAXIMUM  EXTENT PERMITTED  BY APPLICABLE  LAW, THIS SOFTWARE  IS PROVIDED
// *AS IS*  AND NVIDIA AND  ITS SUPPLIERS DISCLAIM  ALL WARRANTIES,  EITHER  EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED  TO, NONINFRINGEMENT,IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL  NVIDIA 
// OR ITS SUPPLIERS BE  LIABLE  FOR  ANY  DIRECT, SPECIAL,  INCIDENTAL,  INDIRECT,  OR  
// CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION,  DAMAGES FOR LOSS 
// OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY 
// OTHER PECUNIARY LOSS) ARISING OUT OF THE  USE OF OR INABILITY  TO USE THIS SOFTWARE, 
// EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
// Please direct any bugs or questions to SDKFeedback@nvidia.com

#include "ConstantBuffer.hlsl"

// Step size in number of pixels
#ifndef STEP_SIZE
#define STEP_SIZE 4
#endif

// Number of shared-memory samples per direction
#ifndef NUM_STEPS
#define NUM_STEPS 8
#endif

// Maximum kernel radius in number of pixels
#define KERNEL_RADIUS (NUM_STEPS*STEP_SIZE)

// The last sample has weight = exp(-KERNEL_FALLOFF)
#define KERNEL_FALLOFF 3.0f

// Must match the HBAO_TILE_WIDTH value from HBAO_DX11_LIB.cpp
#define HBAO_TILE_WIDTH 320

RWTexture2D<float> uOutputBuffer    : register(u0);
RWTexture2D<float2> uOutputBuffer2  : register(u0);
Texture2D<float> tLinearDepth       : register(t0);
Texture2D<float> tAOX               : register(t1);
sampler PointClampSampler           : register(s0);

#define SHARED_MEM_SIZE (KERNEL_RADIUS + HBAO_TILE_WIDTH + KERNEL_RADIUS)
groupshared float2 g_SharedMem[SHARED_MEM_SIZE];

//----------------------------------------------------------------------------------
float Tangent(float2 V)
{
    // Add an epsilon to avoid any division by zero.
    return -V.y / (abs(V.x) + 1.e-6f);
}

//----------------------------------------------------------------------------------
float TanToSin(float x)
{
    return x * rsqrt(x*x + 1.0f);
}

//----------------------------------------------------------------------------------
float Falloff(float sampleId)
{
    // Pre-computed by fxc.
    float r = sampleId / (NUM_STEPS-1);
    return exp(-KERNEL_FALLOFF*r*r);
}

//----------------------------------------------------------------------------------
float2 MinDiff(float2 P, float2 Pr, float2 Pl)
{
    float2 V1 = Pr - P;
    float2 V2 = P - Pl;
    return (dot(V1,V1) < dot(V2,V2)) ? V1 : V2;
}

//----------------------------------------------------------------------------------
// Load (X,Z) view-space coordinates from shared memory.
// On Fermi, such strided 64-bit accesses should not have any bank conflicts.
//----------------------------------------------------------------------------------
float2 SharedMemoryLoad(int centerId, int x)
{
    return g_SharedMem[centerId + x];
}

//----------------------------------------------------------------------------------
// Compute (X,Z) view-space coordinates from the depth texture.
//----------------------------------------------------------------------------------
float2 LoadXZFromTexture(int x, int y)
{ 
    float2 uv = float2(x+0.5, y+0.5) * g_InvAOResolution;
    float z_eye = tLinearDepth.SampleLevel(PointClampSampler, uv, 0);
    float x_eye = (g_UVToViewA.x * uv.x + g_UVToViewB.x) * z_eye;
    return float2(x_eye, z_eye);
}

//----------------------------------------------------------------------------------
// Compute (Y,Z) view-space coordinates from the depth texture.
//----------------------------------------------------------------------------------
float2 LoadYZFromTexture(int x, int y)
{
    float2 uv = float2(x+0.5, y+0.5) * g_InvAOResolution;
    float z_eye = tLinearDepth.SampleLevel(PointClampSampler, uv, 0);
    float y_eye = (g_UVToViewA.y * uv.y + g_UVToViewB.y) * z_eye;
    return float2(y_eye, z_eye);
}

//----------------------------------------------------------------------------------
// Compute the HBAO contribution in a given direction on screen by fetching 2D 
// view-space coordinates available in shared memory:
// - (X,Z) for the horizontal directions (approximating Y by a constant).
// - (Y,Z) for the vertical directions (approximating X by a constant).
//----------------------------------------------------------------------------------
void IntegrateDirection(inout float ao,
                        float2 P,
                        float tanT,
                        int threadId,
                        int X0,
                        int deltaX)
{
    float tanH = tanT;
    float sinH = TanToSin(tanH);
    float sinT = TanToSin(tanT);

    [unroll]
    for (int sampleId = 0; sampleId < NUM_STEPS; ++sampleId)
    {
        float2 S = SharedMemoryLoad(threadId, sampleId * deltaX + X0);
        float2 V = S - P;
        float tanS = Tangent(V);
        float d2 = dot(V, V);

        [flatten]
        if ((d2 < g_R2) && (tanS > tanH))
        {
            // Accumulate AO between the horizon and the sample
            float sinS = TanToSin(tanS);
            ao += Falloff(sampleId) * (sinS - sinH);
            
            // Update the current horizon angle
            tanH = tanS;
            sinH = sinS;
        }
    }
}

//----------------------------------------------------------------------------------
// Bias tangent angle and compute HBAO in the +X/-X or +Y/-Y directions.
//----------------------------------------------------------------------------------
float ComputeHBAO(float2 P, float2 T, int centerId)
{
    float ao = 0;
    float tanT = Tangent(T);
    IntegrateDirection(ao, P,  tanT + g_TanAngleBias, centerId,  STEP_SIZE,  STEP_SIZE);
    IntegrateDirection(ao, P, -tanT + g_TanAngleBias, centerId, -STEP_SIZE, -STEP_SIZE);
    return ao;
}

//----------------------------------------------------------------------------------
// Compute HBAO for the left and right directions.
//----------------------------------------------------------------------------------
void HBAOX_CS(uint3 threadIdx, uint3 blockIdx)
{
    const int         tileStart = blockIdx.x * HBAO_TILE_WIDTH;
    const int           tileEnd = tileStart + HBAO_TILE_WIDTH;
    const int        apronStart = tileStart - KERNEL_RADIUS;
    const int          apronEnd = tileEnd   + KERNEL_RADIUS;

    const int x = apronStart + threadIdx.x;
    const int y = blockIdx.y;

    // Load float2 samples into shared memory
    g_SharedMem[threadIdx.x] = LoadXZFromTexture(x,y);
    g_SharedMem[min(2*KERNEL_RADIUS+threadIdx.x, SHARED_MEM_SIZE-1)] = LoadXZFromTexture(2*KERNEL_RADIUS+x,y);
    GroupMemoryBarrierWithGroupSync();

    const int writePos = tileStart + threadIdx.x;
    const int tileEndClamped = min(tileEnd, (int)g_AOResolution.x);
    [branch]
    if (writePos < tileEndClamped)
    {
        int centerId = threadIdx.x + KERNEL_RADIUS;
        uint ox = writePos;
        uint oy = blockIdx.y;

        // Fetch the 2D coordinates of the center point and its nearest neighbors
        float2 P = SharedMemoryLoad(centerId, 0);
        float2 Pr = SharedMemoryLoad(centerId, 1);
        float2 Pl = SharedMemoryLoad(centerId, -1);
        
        // Compute tangent vector using central differences
        float2 T = MinDiff(P, Pr, Pl);

        float ao = ComputeHBAO(P, T, centerId);
        uOutputBuffer[uint2(ox, oy)] = ao;
    }
}

//----------------------------------------------------------------------------------
// Compute HBAO for the up and down directions.
// Output the average AO for the 4 axis-aligned directions.
//----------------------------------------------------------------------------------
void HBAOY_CS(uint3 threadIdx, uint3 blockIdx)
{
    const int         tileStart = blockIdx.x * HBAO_TILE_WIDTH;
    const int           tileEnd = tileStart + HBAO_TILE_WIDTH;
    const int        apronStart = tileStart - KERNEL_RADIUS;
    const int          apronEnd = tileEnd   + KERNEL_RADIUS;

    const int x = blockIdx.y;
    const int y = apronStart + threadIdx.x;

    // Load float2 samples into shared memory
    g_SharedMem[threadIdx.x] = LoadYZFromTexture(x,y);
    g_SharedMem[min(2*KERNEL_RADIUS+threadIdx.x, SHARED_MEM_SIZE-1)] = LoadYZFromTexture(x,2*KERNEL_RADIUS+y);
    GroupMemoryBarrierWithGroupSync();

    const int writePos = tileStart + threadIdx.x;
    const int tileEndClamped = min(tileEnd, (int)g_AOResolution.y);
    [branch]
    if (writePos < tileEndClamped)
    {
        int centerId = threadIdx.x + KERNEL_RADIUS;
        uint ox = blockIdx.y;
        uint oy = writePos;

        // Fetch the 2D coordinates of the center point and its nearest neighbors
        float2 P = SharedMemoryLoad(centerId, 0);
        float2 Pt = SharedMemoryLoad(centerId, 1);
        float2 Pb = SharedMemoryLoad(centerId, -1);

        // Compute tangent vector using central differences
        float2 T = MinDiff(P, Pt, Pb);

        float aoy = ComputeHBAO(P, T, centerId);
        float aox = tAOX.Load(int3(blockIdx.y, writePos, 0));
        float ao = (aox + aoy) * 0.25;

        ao = saturate(1.0 - ao * g_Strength);
        uOutputBuffer2[uint2(ox, oy)] = float2(ao, P.y);
    }
}

[numthreads(HBAO_TILE_WIDTH, 1, 1)]
#if STEP_SIZE == 1
void HBAOX_CS11_1 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOX_CS(threadIdx, blockIdx);
}
#endif
#if STEP_SIZE == 2
void HBAOX_CS11_2 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOX_CS(threadIdx, blockIdx);
}
#endif
#if STEP_SIZE == 3
void HBAOX_CS11_3 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOX_CS(threadIdx, blockIdx);
}
#endif
#if STEP_SIZE == 4
void HBAOX_CS11_4 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOX_CS(threadIdx, blockIdx);
}
#endif
#if STEP_SIZE == 5
void HBAOX_CS11_5 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOX_CS(threadIdx, blockIdx);
}
#endif
#if STEP_SIZE == 6
void HBAOX_CS11_6 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOX_CS(threadIdx, blockIdx);
}
#endif
#if STEP_SIZE == 7
void HBAOX_CS11_7 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOX_CS(threadIdx, blockIdx);
}
#endif
#if STEP_SIZE == 8
void HBAOX_CS11_8 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOX_CS(threadIdx, blockIdx);
}
#endif

[numthreads(HBAO_TILE_WIDTH, 1, 1)]
#if STEP_SIZE == 1
void HBAOY_CS11_1 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOY_CS(threadIdx, blockIdx);
}
#endif
#if STEP_SIZE == 2
void HBAOY_CS11_2 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOY_CS(threadIdx, blockIdx);
}
#endif
#if STEP_SIZE == 3
void HBAOY_CS11_3 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOY_CS(threadIdx, blockIdx);
}
#endif
#if STEP_SIZE == 4
void HBAOY_CS11_4 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOY_CS(threadIdx, blockIdx);
}
#endif
#if STEP_SIZE == 5
void HBAOY_CS11_5 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOY_CS(threadIdx, blockIdx);
}
#endif
#if STEP_SIZE == 6
void HBAOY_CS11_6 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOY_CS(threadIdx, blockIdx);
}
#endif
#if STEP_SIZE == 7
void HBAOY_CS11_7 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOY_CS(threadIdx, blockIdx);
}
#endif
#if STEP_SIZE == 8
void HBAOY_CS11_8 (uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID)
{
    HBAOY_CS(threadIdx, blockIdx);
}
#endif
