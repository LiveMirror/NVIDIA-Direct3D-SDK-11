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

#include "Defines.h"

Texture2DArray   g_txLPVProp0                      : register( t0 );
Texture2DArray   g_txLPVProp1                      : register( t1 );
RWTexture2DArray<float>    g_uavAccumulate0        : register( u0 );
RWTexture2DArray<float>    g_uavAccumulate1        : register( u1 );
RWTexture2DArray<float>    g_uavAccumulate2        : register( u2 );
RWTexture2DArray<float>    g_uavAccumulate3        : register( u3 );
RWTexture2DArray<float>    g_uavAccumulate4        : register( u4 );
RWTexture2DArray<float>    g_uavAccumulate5        : register( u5 );
RWTexture2DArray<float>    g_uavAccumulate6        : register( u6 );
RWTexture2DArray<float>    g_uavAccumulate7        : register( u7 );



cbuffer cbConstantsLPVinitialize3 : register( b0 )
{
    int g_numCols         : packoffset(c0.x);    //the number of columns in the flattened 2D LPV
    int g_numRows         : packoffset(c0.y);      //the number of columns in the flattened 2D LPV
    int LPV2DWidth        : packoffset(c0.z);   //the total width of the flattened 2D LPV
    int LPV2DHeight       : packoffset(c0.w);   //the total height of the flattened 2D LPV

    int LPV3DWidth        : packoffset(c1.x);      //the width of the LPV in 3D
    int LPV3DHeight       : packoffset(c1.y);     //the height of the LPV in 3D
    int LPV3DDepth        : packoffset(c1.z); 
    float padding7        : packoffset(c1.w);
}

SamplerState samLinear : register( s0 )
{ 
    Filter = MIN_MAG_LINEAR_MIP_POINT; 
};

//----------------------------------------------------------
//helper function for loading texture
//----------------------------------------------------------

float4 loadOffsetTexValue(Texture2DArray tex, uint3 pos)
{
    if(pos.x >= LPV3DWidth  || pos.x < 1) return float4(0,0,0,0);
    if(pos.y >= LPV3DHeight || pos.y < 1) return float4(0,0,0,0);
    if(pos.z >= LPV3DDepth  || pos.z < 1) return float4(0,0,0,0);

    return tex.Load(int4(pos.x,pos.y,pos.z,0));
}


//--------------------------------------------------------
//accumulate shader
//--------------------------------------------------------

[numthreads(X_BLOCK_SIZE,Y_BLOCK_SIZE,Z_BLOCK_SIZE)]
void AccumulateLPV_singleFloats_8(  uint threadId        : SV_GroupIndex,
                                    uint3 groupId        : SV_GroupID,
                                    uint3 globalThreadId : SV_DispatchThreadID)
{

    //find where in the 2d texture that grid point is supposed to land
    bool outside = false;
    if(globalThreadId.x>=LPV3DWidth || globalThreadId.y>=LPV3DHeight || globalThreadId.z>=LPV3DDepth) outside = true;

    uint3 writePos = globalThreadId.xyz;
    int4 readPos = int4(globalThreadId.x,globalThreadId.y,globalThreadId.z,0);
    
    //write back the accumulated flux
    if(!outside)
    {
        float4 SHCoefficients0 = g_txLPVProp0.Load(readPos);
        float4 SHCoefficients1 = g_txLPVProp1.Load(readPos);

        g_uavAccumulate0[writePos] +=  SHCoefficients0.x;
        g_uavAccumulate1[writePos] +=  SHCoefficients0.y;
        g_uavAccumulate2[writePos] +=  SHCoefficients0.z;
        g_uavAccumulate3[writePos] +=  SHCoefficients0.w;

        g_uavAccumulate4[writePos] +=  SHCoefficients1.x;
        g_uavAccumulate5[writePos] +=  SHCoefficients1.y;
        g_uavAccumulate6[writePos] +=  SHCoefficients1.z;
        g_uavAccumulate7[writePos] +=  SHCoefficients1.w;
    }
    
}    

[numthreads(X_BLOCK_SIZE,Y_BLOCK_SIZE,Z_BLOCK_SIZE)]
void AccumulateLPV_singleFloats_4(    uint threadId        : SV_GroupIndex,
                                    uint3 groupId        : SV_GroupID,
                                    uint3 globalThreadId : SV_DispatchThreadID)
{

    //find where in the 2d texture that grid point is supposed to land
    bool outside = false;
    if(globalThreadId.x>=LPV3DWidth || globalThreadId.y>=LPV3DHeight || globalThreadId.z>=LPV3DDepth) outside = true;

    uint3 writePos = globalThreadId.xyz;
    int4 readPos = int4(globalThreadId.x,globalThreadId.y,globalThreadId.z,0);
    
    //write back the accumulated flux
    if(!outside)
    {
        float4 SHCoefficients0 = g_txLPVProp0.Load(readPos);

        g_uavAccumulate0[writePos] +=  SHCoefficients0.x;
        g_uavAccumulate1[writePos] +=  SHCoefficients0.y;
        g_uavAccumulate2[writePos] +=  SHCoefficients0.z;
        g_uavAccumulate3[writePos] +=  SHCoefficients0.w;
    }
    
}
