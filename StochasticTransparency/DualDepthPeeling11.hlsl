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

#include "BaseTechnique.hlsl"

Texture2DMS<float4> tLayerColor : register(t0);
Texture2D<float2> tDepthBlender : register(t0);
Texture2D<float4> tFrontBlender : register(t1);
Texture2D<float4> tBackBlender  : register(t2);

#define MAX_DEPTH_FLOAT 1.0f

struct DDPOutputMRT
{
    float2 Depths       : SV_Target0;
    float4 FrontColor   : SV_Target1;
    float4 BackColor    : SV_Target2;
};

float4 DDPFirstPassPS ( Geometry_VSOut IN ) : SV_TARGET
{
    float z = IN.HPosition.z;
    return float4(-z, z, 0, 0);
}

DDPOutputMRT DDPDepthPeelPS ( Geometry_VSOut IN )
{
    DDPOutputMRT OUT = (DDPOutputMRT)0;

    // Window-space depth interpolated linearly in screen space
    float fragDepth = IN.HPosition.z;

    OUT.Depths.xy = tDepthBlender.Load( int3( IN.HPosition.xy, 0 ) ).xy;
    float nearestDepth = -OUT.Depths.x;
    float farthestDepth = OUT.Depths.y;

    if (fragDepth < nearestDepth || fragDepth > farthestDepth) {
        // Skip this depth in the peeling algorithm
        OUT.Depths.xy = -MAX_DEPTH_FLOAT;
        return OUT;
    }
    
    if (fragDepth > nearestDepth && fragDepth < farthestDepth) {
        // This fragment needs to be peeled again
        OUT.Depths.xy = float2(-fragDepth, fragDepth);
        return OUT;
    }
    
    // If we made it here, this fragment is on the peeled layer from last pass
    // therefore, we need to shade it, and make sure it is not peeled any farther
    float4 color = ShadeFragment(IN.Normal);
    OUT.Depths.xy = -MAX_DEPTH_FLOAT;
    
    if (fragDepth == nearestDepth)
    {
        color.rgb *= color.a;
        OUT.FrontColor = color;
    }
    else
    {
        OUT.BackColor = color;
    }
    return OUT;
}

float4 DDPBlendingPS ( FullscreenVSOut IN ) : SV_TARGET
{
    return tLayerColor.Load( int2( IN.pos.xy ), 0 );
}

float3 DDPFinalPS ( FullscreenVSOut IN ) : SV_TARGET
{
    float4 frontColor = tFrontBlender.Load( int3( IN.pos.xy, 0 ) );
    float3 backColor = tBackBlender.Load( int3( IN.pos.xy, 0 ) ).xyz;
    return frontColor.xyz + backColor * frontColor.w;
}
