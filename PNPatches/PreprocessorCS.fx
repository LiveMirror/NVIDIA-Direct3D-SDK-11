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

Buffer<float2>      g_CoordinatesBuffer;
Buffer<float4>      g_NormalsBuffer;
Buffer<int2>        g_IndicesBuffer;

Texture2D           g_CoarsePositions;
Texture2D           g_FinePositions;

RWStructuredBuffer<float4> RWNormalsBuffer;

SamplerState SamplerLinearClamp
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

cbuffer cb0
{
    uint g_PrimitivesNum = 0;
}

float3 GetNormal(int2 index)
{
    // Load texture coordinates
    float2 texCoord = g_CoordinatesBuffer.Load(index.x);
    
    // Sample positions
    float3 coarsePosition = g_CoarsePositions.SampleLevel(SamplerLinearClamp, texCoord, 0).xyz;
    float3 finePosition = g_FinePositions.SampleLevel(SamplerLinearClamp, texCoord, 0).xyz;

    float3 normal = finePosition - coarsePosition;

    // Load coarse normal
    float3 coarseNormal = g_NormalsBuffer.Load(index.x).xyz;
    normal *= sign(dot(normal, coarseNormal)); // Fix direction

    return normalize(normal);
}

//--------------------------------------------------------------------------------------
// Fixup normals(trianle patches)
//--------------------------------------------------------------------------------------
[numthreads(256, 1, 1)]
void FixupTriangleNormals(uint3 DTid : SV_DispatchThreadID)
{
    if(DTid.x < g_PrimitivesNum)
    {
        int primitiveOffset = DTid.x * 3;
        
        [unroll]
        for(int i=0; i<3; i++)
        {
            int2 index = g_IndicesBuffer.Load(primitiveOffset + i);
            RWNormalsBuffer[index.x] = float4(GetNormal(index), 0);
        }
    }
}

//--------------------------------------------------------------------------------------
// Fixup normals(quad patches)
//--------------------------------------------------------------------------------------
[numthreads(256, 1, 1)]
void FixupQuadNormals(uint3 DTid : SV_DispatchThreadID)
{
    if(DTid.x < g_PrimitivesNum)
    {
        int primitiveOffset = DTid.x * 4;
        
        [unroll]
        for(int i=0; i<4; i++)
        {
            int2 index = g_IndicesBuffer.Load(primitiveOffset + i);
            RWNormalsBuffer[index.x] = float4(GetNormal(index), 0);
        }
    }
}

technique11 FixupNormals
{
    pass p0
    {
        SetComputeShader(CompileShader(cs_5_0, FixupTriangleNormals()));
    }

    pass p1
    {
        SetComputeShader(CompileShader(cs_5_0, FixupQuadNormals()));
    }
}