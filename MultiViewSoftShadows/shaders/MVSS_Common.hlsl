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

#pragma pack_matrix( row_major )

// Must match the value from MultiViewSoftShadows.h
#define SHADOW_MAP_ARRAY_SIZE 28

//--------------------------------------------------------------------------------------
// Constant Buffer
//--------------------------------------------------------------------------------------

cbuffer globalconstants : register(b0)
{
    // With D3D 10/11, constant buffers are required to be 16-byte aligned.
    float4x4    g_WorldToEyeClip;
    float4x4    g_WorldToEyeView;
    float4x4    g_EyeClipToEyeView;
    float4x4    g_EyeViewToEyeClip;
    float4x4    g_EyeViewToLightTex[SHADOW_MAP_ARRAY_SIZE];
    float4      g_LightWorldPos;
    float       g_LightZNear;
    float       g_LightZFar;
    float2      g_Pad;
};

//--------------------------------------------------------------------------------------
// Samplers
//--------------------------------------------------------------------------------------

Texture2D<float3> tGroundDiffuse        : register(t0);
Texture2D<float3> tGroundNormal         : register(t1);
Texture2DArray<float> tShadowMapArray   : register(t2);

SamplerComparisonState PCF_Sampler      : register(s0);
sampler LinearWarpSampler               : register(s1);
sampler PointSampler                    : register(s2);

//--------------------------------------------------------------------------------------
// Shading functions
//--------------------------------------------------------------------------------------

float4 ShadeGround(float3 WorldPos, float3 normal)
{
    float3 LightDir = normalize(g_LightWorldPos.xyz - WorldPos.xyz);
    float2 uv = (WorldPos.xz * 0.5 + 0.5) * 2.0;
    float3 diffuse = tGroundDiffuse.Sample(LinearWarpSampler, uv);
    normal = tGroundNormal.Sample(LinearWarpSampler, uv).xzy * 2.0 - 1.0;
    diffuse *= max(dot(LightDir, normal), 0);
    diffuse *= pow(abs(dot(LightDir, normalize(g_LightWorldPos.xyz))), 40);
    return float4(diffuse, 1.0);
}

float4 ShadeCharacter(float3 WorldPos, float3 normal)
{
    float3 LightDir = normalize(g_LightWorldPos.xyz - WorldPos.xyz);
    float3 diffuse = max(dot(LightDir, normal), 0);
    return float4(diffuse, 1.0);
}
