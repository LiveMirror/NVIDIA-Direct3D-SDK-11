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

#ifndef LIGHT_SOURCE_FX
#define LIGHT_SOURCE_FX

#include "Globals.fx"
#include "OpacityMapping.fx"
#include "ScreenAlignedQuad.fx"
#include "SharedDefines.h"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
#define NumLightSlots NumActiveLights

Texture2D  g_OpacityMap0[NumLightSlots];
Texture2D  g_OpacityMap1[NumLightSlots];
Texture2D  g_OpacityMap2[NumLightSlots];
Texture2D  g_OpacityMap3[NumLightSlots];
Texture2D  g_ShadowMap[NumLightSlots];
float4x4 g_mViewToOpacityShadowTexture[NumLightSlots];
float4x4 g_mViewToShadowTexture[NumLightSlots];
float3   g_LightDirection[NumLightSlots];
float3   g_LightColor[NumLightSlots];
float4x4 g_mViewToLight[NumLightSlots];
float    g_OpacityMip;
float    g_OpacityQuality;
float    g_OpacityMultiplier;
float	 g_OpacityShadowStrength;

static const float3 g_AmbientLighting = {0,0,0};
static const float LinearZMax = 1000000.f;

#define SHADOW_FILTER_STEPS 5
#define SHADOW_FILTER_STEP_SIZE 3
#define SHADOW_FILTER_LOOP_LIMIT (SHADOW_FILTER_STEP_SIZE * ((SHADOW_FILTER_STEPS-1)/2))
#define SHADOW_FILTER_SCALE (1.f/(float(SHADOW_FILTER_STEPS) * float(SHADOW_FILTER_STEPS)))

//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
SamplerComparisonState g_SamplerShadow
{
    Filter = COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    ComparisonFunc = LESS;
    AddressU = Clamp;
    AddressV = Clamp;
};

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
struct BeamShapeCoords
{
    float3 c[NumActiveLights] : PerLightBeamCoords;
};

struct OpacityMapCoords
{
    float3 c[NumActiveLights] : PerLightOpacityCoords;
};

struct ShadowMapCoords
{
    float3 c[NumActiveLights] : PerLightShadowCoords;
};

struct AttenuationFactors
{
    float af[NumActiveLights] : PerLightAttenuationFactors;
};

struct LightSourceCoords
{
    OpacityMapCoords OpacityShadowCoords;
    ShadowMapCoords ShadowCoords;
    BeamShapeCoords BeamCoords;
};

//--------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------
AttenuationFactors Combine(AttenuationFactors lhs, AttenuationFactors rhs)
{
    AttenuationFactors Output;

    [unroll]
    for(int light = 0; light != NumActiveLights; ++light)
    {
        Output.af[light] = lhs.af[light] * rhs.af[light];
    }

    return Output;
}

LightSourceCoords CalcLightSourceCoords(float3 ViewPos)
{
    LightSourceCoords Output;

    [unroll]
    for(int light = 0; light != NumActiveLights; ++light)
    {
        float4 OpacityShadowCoords = mul(float4(ViewPos,1.f), g_mViewToOpacityShadowTexture[light]);
        Output.OpacityShadowCoords.c[light] = float3(OpacityShadowCoords.xy/OpacityShadowCoords.w,OpacityShadowCoords.z);

        float4 ShadowCoords = mul(float4(ViewPos,1.f), g_mViewToShadowTexture[light]);
        Output.ShadowCoords.c[light] = float3(ShadowCoords.xy/ShadowCoords.w,ShadowCoords.z);

        Output.BeamCoords.c[light] = mul(float4(ViewPos,1.f), g_mViewToLight[light]).xyz;
    }

    return Output;
}

float BeamShapeAttenuation(float3 LightSpacePos)
{
    float r_beam = length(LightSpacePos.xy);
    float attn = smoothstep(-0.75f, -0.375f, -r_beam);
    return LightSpacePos.z > 0.f ? attn * attn : 0.f;
}

AttenuationFactors CalcBeamShapeAttenuations(BeamShapeCoords coords)
{
    AttenuationFactors Output;

    [unroll]
    for(int light = 0; light != NumActiveLights; ++light)
    {
        Output.af[light] = BeamShapeAttenuation(coords.c[light]);
    }

    return Output;
}

AttenuationFactors CalcShadowFactorsFOM(OpacityMapCoords coords)
{
    AttenuationFactors Output;

    [unroll]
    for(int light = 0; light != NumActiveLights; ++light)
    {
        float4 opacity0 = g_OpacityMap0[light].SampleLevel(g_SamplerOpacity, float2(coords.c[light].xy), g_OpacityMip);
        float4 opacity1 = g_OpacityMap1[light].SampleLevel(g_SamplerOpacity, float2(coords.c[light].xy), g_OpacityMip);
        float4 opacity2 = g_OpacityMap2[light].SampleLevel(g_SamplerOpacity, float2(coords.c[light].xy), g_OpacityMip);
        float4 opacity3 = g_OpacityMap3[light].SampleLevel(g_SamplerOpacity, float2(coords.c[light].xy), g_OpacityMip);
        
        float shadowFactor = CalcShadowFactorFOM(g_OpacityQuality, opacity0, opacity1, opacity2, opacity3, coords.c[light].z);
		Output.af[light] = lerp(1.f, shadowFactor, g_OpacityShadowStrength);
    }

    return Output;
}

AttenuationFactors CalcShadowFactorsAtFarFOM(OpacityMapCoords coords)
{
    AttenuationFactors Output;

    [unroll]
    for(int light = 0; light != NumActiveLights; ++light)
    {
        float4 opacity = g_OpacityMap0[light].SampleLevel(g_SamplerOpacity, float2(coords.c[light].xy), g_OpacityMip);

        // DC component-only!
        float shadowFactor = saturate(exp(-opacity.g));
		Output.af[light] = lerp(1.f, shadowFactor, g_OpacityShadowStrength);
    }
    
    return Output;
}

float SampleShadowMapUnfiltered(Texture2D shadowMap, float3 uvz)
{
    float result = shadowMap.SampleCmpLevelZero(g_SamplerShadow, uvz.xy, uvz.z);
    return result;
}

float SampleShadowMapFiltered(Texture2D shadowMap, float3 uvz)
{
    float result = 0.f;

    [unroll]
    for(int xOff = -SHADOW_FILTER_LOOP_LIMIT; xOff <= SHADOW_FILTER_LOOP_LIMIT; xOff += SHADOW_FILTER_STEP_SIZE)
    {
        [unroll]for(int yOff = -SHADOW_FILTER_LOOP_LIMIT; yOff <= SHADOW_FILTER_LOOP_LIMIT; yOff += SHADOW_FILTER_STEP_SIZE)
        {
            result += shadowMap.SampleCmpLevelZero(g_SamplerShadow, uvz.xy, uvz.z, int2(xOff,yOff)).r;
        }
    }

    result *= SHADOW_FILTER_SCALE;

    return result;
}

AttenuationFactors CalcShadowFactorsFiltered(ShadowMapCoords coords)
{
    AttenuationFactors Output;

    [unroll]
    for(int light = 0; light != NumActiveLights; ++light)
    {
        Output.af[light] = SampleShadowMapFiltered(g_ShadowMap[light], coords.c[light]);
    }

    return Output;
}

AttenuationFactors CalcShadowFactorsUnfiltered(ShadowMapCoords coords)
{
    AttenuationFactors Output;

    [unroll]
    for(int light = 0; light != NumActiveLights; ++light)
    {
        Output.af[light] = SampleShadowMapUnfiltered(g_ShadowMap[light], coords.c[light]);
    }

    return Output;
}

float3 CalcIllumination(AttenuationFactors attn)
{
    float3 illumination = g_AmbientLighting;

    [unroll]
    for(int light = 0; light != NumActiveLights; ++light)
    {
        illumination += g_LightColor[light] * attn.af[light];
    }
    
    return illumination;
}

float3 CalcPhong(AttenuationFactors attn, float3 ViewPos, float3 Normal, float3 Diffuse, float SpecularPower)
{
    float3 result;
    result.rgb = Diffuse * g_AmbientLighting;

    float3 Normal_ViewSpace = mul(Normal,(float3x3)g_mWorldToView);

    [unroll]
    for(int light = 0; light != NumActiveLights; ++light)
    {
        // Specular
        float3 LightDir_ViewSpace = mul(g_LightDirection[light],(float3x3)g_mWorldToView);

        float dp = saturate(-dot(Normal,g_LightDirection[light]));

        float3 HalfVec = normalize(LightDir_ViewSpace + ViewPos);
        float Spec = dp * pow(saturate(-dot(HalfVec,Normal_ViewSpace)),SpecularPower);

        // Diffuse
        float3 Diff = Diffuse * dp;

        result.rgb += (Spec.xxx + Diff) * g_LightColor[light] * attn.af[light];
    }

    return result;
}

float GetOpacityQualityForLighting()
{
    return g_OpacityQuality;
}

float GetOpacityMultiplierForLighting()
{
    return g_OpacityMultiplier;
}

float4 LinearizeShadowZ_PS(VS_SAQ_OUTPUT In) : SV_Target
{
    float RawZ = g_ScreenAlignedQuadSrc.SampleLevel(g_SamplerScreenAlignedQuad, In.TexCoord, g_ScreenAlignedQuadMip).r;

    float4 UnprojectZ = mul(float4(0, 0, RawZ, 1), g_mProjToView);
    float ViewZ = UnprojectZ.z / UnprojectZ.w;

    // Note that we push RawZ = 1.f values back to LinearZMax, to reflect the fact that
    // the scene continues beyond LinearZ = 1.f
    float LinearZ = RawZ < 1.f ? ViewZ * g_zScaleAndBias.x + g_zScaleAndBias.y : LinearZMax;

    return LinearZ;
}

//--------------------------------------------------------------------------------------
// Techniques
//--------------------------------------------------------------------------------------
technique11 LinearizeShadowZ
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, ScreenAlignedQuad_VS() ) );
        SetGeometryShader( NULL );
        SetHullShader( NULL );
        SetDomainShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, LinearizeShadowZ_PS() ) );

        SetBlendState( OpaqueBlendRGBA, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( NoDepthStencil, 0 );
        SetRasterizerState( SolidNoCull );
    }
}

#endif // LIGHT_SOURCE_FX