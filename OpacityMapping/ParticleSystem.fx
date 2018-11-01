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

#ifndef PARTICLE_SYSTEM_FX
#define PARTICLE_SYSTEM_FX

#include "LightSource.fx"
#include "LowResOffscreen.fx"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
Texture2D  g_InstanceTexture;
Texture2D  g_ParticleTexture;
Texture2D  g_SoftParticlesDepthTexture;

float    g_ParticleScale;
float    g_TessellationDensity;
float    g_MaxTessellation;
float    g_SoftParticlesFadeDistance;

static const float2 particleCornerCoords[4] = {
    {-1, 1},
    { 1, 1},
    {-1,-1},
    { 1,-1}
};

//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
SamplerState g_SamplerParticle
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
struct VS_SCENE_PARTICLE_OUTPUT_PS_LIGHTING
{
    float4 Position               : SV_Position;
    float3 TextureUVAndOpacity    : TEXCOORD0;
    float  ViewPosZ               : TEXCOORD1;
    LightSourceCoords LightCoords;
};

struct VS_SCENE_PARTICLE_CONTROL_POINT_OUTPUT
{
    float3 ViewPos                : VIEWPOS;
    float3 TextureUVAndOpacity    : TEXCOORD0;
};

struct HS_SCENE_PARTICLE_CONTROL_POINT_OUTPUT
{
    float3 ViewPos                : VIEWPOS;
    float3 TextureUVAndOpacity    : TEXCOORD0;
};

struct HS_SCENE_PARTICLE_CONSTANT_DATA_OUTPUT
{
    float Edges[4]        : SV_TessFactor;
    float Inside[2]       : SV_InsideTessFactor;

};

struct VS_DS_SCENE_PARTICLE_OUTPUT_VS_DS_LIGHTING
{
    float4 Position               : SV_Position;
    float3 TextureUVAndOpacity    : TEXCOORD0;
    float  ViewPosZ               : TEXCOORD1;
    AttenuationFactors LightAttenuationFactors;
};

struct VS_OPACITY_PARTICLE_OUTPUT
{
    float4 Position               : SV_Position;   // vertex position 
    float3 TextureUVAndOpacity    : TEXCOORD0;
    float3 ShadowCoords           : TEXCOORD1;
};

//--------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------
void CalcParticleCoords( in float4 vPos, out float3 ViewPos, out float2 UV, out float Opacity)
{
    int TextureW, TextureH;
    g_InstanceTexture.GetDimensions(TextureW, TextureH);
    float fTextureW = TextureW;

    // Calculate the UVs of the instance
    float InstanceIx = vPos.x;
    float row;
    float col = fTextureW * modf(InstanceIx/fTextureW,row);

    // Look up the instance data
    float4 InstanceData = g_InstanceTexture.Load(int3(round(col),round(row), 0));

    // Transform to camera space
    ViewPos = mul(float4(InstanceData.xyz,1), g_mWorldToView).xyz;

    // Inflate corners, applying scale from instance data
    float2 CornerCoord = particleCornerCoords[vPos.y];
    ViewPos.xy += g_ParticleScale * CornerCoord;

    UV = 0.5f * (CornerCoord + 1.f);
    
    Opacity = InstanceData.a;
}

float4 GetParticleRGBA(SamplerState s, float2 uv, float alphaMult)
{
    const float base_alpha = 16.f/255.f;

    float4 raw_tex = g_ParticleTexture.Sample(s, uv);
    raw_tex.a = saturate((raw_tex.a - base_alpha)/(1.f - base_alpha));
    raw_tex.a *= alphaMult * 0.3f;
    return raw_tex;
}

float GetSoftParticleAlphaMultiplier(int2 xy, float ParticleViewZ)
{
	float sampledDeviceZ = g_SoftParticlesDepthTexture.Load(int3(xy,0)).r;
	float sampledViewZ = g_ZLinParams.z / (g_ZLinParams.x - sampledDeviceZ * g_ZLinParams.y);

	float alphaMult = saturate((sampledViewZ - ParticleViewZ)/g_SoftParticlesFadeDistance);

	return alphaMult;
}

VS_SCENE_PARTICLE_OUTPUT_PS_LIGHTING RenderParticlesToScenePSLightingVS( float4 vPos : POSITION )
{
    VS_SCENE_PARTICLE_OUTPUT_PS_LIGHTING Output;

    float3 ViewPos;
    CalcParticleCoords(vPos, ViewPos, Output.TextureUVAndOpacity.xy, Output.TextureUVAndOpacity.z);
    
    // Transform to homogeneous projection space
    Output.Position = mul(float4(ViewPos,1.f), g_mViewToProj);
    Output.LightCoords = CalcLightSourceCoords(ViewPos);
    Output.ViewPosZ = ViewPos.z;
    
    return Output;    
}

VS_DS_SCENE_PARTICLE_OUTPUT_VS_DS_LIGHTING RenderParticlesToSceneVSLightingVS( float4 vPos : POSITION )
{
    VS_DS_SCENE_PARTICLE_OUTPUT_VS_DS_LIGHTING Output;

    float3 ViewPos;
    CalcParticleCoords(vPos, ViewPos, Output.TextureUVAndOpacity.xy, Output.TextureUVAndOpacity.z);

	Output.ViewPosZ = ViewPos.z;
    
    // Transform to homogeneous projection space
    Output.Position = mul(float4(ViewPos,1.f), g_mViewToProj);
    
    LightSourceCoords LightCoords = CalcLightSourceCoords(ViewPos);
    AttenuationFactors beamAttn = CalcBeamShapeAttenuations(LightCoords.BeamCoords);
    AttenuationFactors opacityShadowAttn = CalcShadowFactorsFOM(LightCoords.OpacityShadowCoords);
    AttenuationFactors shadowAttn = CalcShadowFactorsFiltered(LightCoords.ShadowCoords);
    Output.LightAttenuationFactors = Combine(Combine(beamAttn,shadowAttn),opacityShadowAttn);

    return Output; 
}

VS_SCENE_PARTICLE_CONTROL_POINT_OUTPUT RenderParticlesToSceneDSLightingVS( float4 vPos : POSITION )
{
    VS_SCENE_PARTICLE_CONTROL_POINT_OUTPUT Output;
    CalcParticleCoords(vPos, Output.ViewPos, Output.TextureUVAndOpacity.xy, Output.TextureUVAndOpacity.z);
    return Output;  
}

HS_SCENE_PARTICLE_CONSTANT_DATA_OUTPUT RenderParticlesToSceneDSLightingConstantsHS( 
    InputPatch<VS_SCENE_PARTICLE_CONTROL_POINT_OUTPUT, 4> ip
    )
{   
    HS_SCENE_PARTICLE_CONSTANT_DATA_OUTPUT Output;
    
    // The particles are billboarded, so we just need to know the particle size and
    // the camera distance to decide on tessellation factors
    float size = length(ip[1].ViewPos - ip[0].ViewPos);
    float view_dist = ip[1].ViewPos.z;
    
    float tess = min(g_MaxTessellation,g_TessellationDensity * size/view_dist);

    Output.Edges[0] = tess;
    Output.Edges[1] = tess;
    Output.Edges[2] = tess;
    Output.Edges[3] = tess;
    
    Output.Inside[0] = tess;
    Output.Inside[1] = tess;
    
    return Output;
}

[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("RenderParticlesToSceneDSLightingConstantsHS")]
HS_SCENE_PARTICLE_CONTROL_POINT_OUTPUT RenderParticlesToSceneDSLightingHS( 
    const InputPatch<VS_SCENE_PARTICLE_CONTROL_POINT_OUTPUT, 4> ip, 
    uint i : SV_OutputControlPointID)
{
    HS_SCENE_PARTICLE_CONTROL_POINT_OUTPUT Output;

    Output.ViewPos = ip[i].ViewPos;
    Output.TextureUVAndOpacity = ip[i].TextureUVAndOpacity;
    
    return Output;
}

[domain("quad")]
VS_DS_SCENE_PARTICLE_OUTPUT_VS_DS_LIGHTING RenderParticlesToSceneDSLightingDS(	HS_SCENE_PARTICLE_CONSTANT_DATA_OUTPUT unused,
																				float2 UV : SV_DomainLocation,
																				const OutputPatch<HS_SCENE_PARTICLE_CONTROL_POINT_OUTPUT, 4> ip )
{
    VS_DS_SCENE_PARTICLE_OUTPUT_VS_DS_LIGHTING Output;

    float bf0 = (1.f - UV.x) * (1.f - UV.y);
    float bf1 = UV.x * (1.f - UV.y);
    float bf2 = (1.f - UV.x) * UV.y;
    float bf3 = UV.x * UV.y;
    
    float3 ViewPos = bf0 * ip[0].ViewPos + bf1 * ip[1].ViewPos+ bf2 * ip[2].ViewPos + bf3 * ip[3].ViewPos;
    Output.TextureUVAndOpacity = bf0 * ip[0].TextureUVAndOpacity + bf1 * ip[1].TextureUVAndOpacity+ bf2 * ip[2].TextureUVAndOpacity + bf3 * ip[3].TextureUVAndOpacity;
	Output.ViewPosZ = ViewPos.z;
    
    // Transform to homogeneous projection space
    Output.Position = mul(float4(ViewPos,1.f), g_mViewToProj);
    
    LightSourceCoords LightCoords = CalcLightSourceCoords(ViewPos);
    AttenuationFactors beamAttn = CalcBeamShapeAttenuations(LightCoords.BeamCoords);
    AttenuationFactors opacityShadowAttn = CalcShadowFactorsFOM(LightCoords.OpacityShadowCoords);
    AttenuationFactors shadowAttn = CalcShadowFactorsFiltered(LightCoords.ShadowCoords);
    Output.LightAttenuationFactors = Combine(Combine(beamAttn,shadowAttn),opacityShadowAttn);

    return Output;    
}

VS_OPACITY_PARTICLE_OUTPUT RenderParticlesToOpacityVS( float4 vPos : POSITION )
{
    VS_OPACITY_PARTICLE_OUTPUT Output;

    float3 ViewPos;
    CalcParticleCoords(vPos, ViewPos, Output.TextureUVAndOpacity.xy, Output.TextureUVAndOpacity.z);

    // Finally, transform to homogeneous projection space
    Output.Position = mul(float4(ViewPos,1.f), g_mViewToProj);

    float linearZ = ViewPos.z * g_zScaleAndBias.x + g_zScaleAndBias.y;
    Output.ShadowCoords = float3(Output.Position.xy/Output.Position.w,linearZ);
    
    return Output;    
}

PS_OUTPUT RenderParticlesToScenePSLightingPS( VS_SCENE_PARTICLE_OUTPUT_PS_LIGHTING In ) 
{ 
    PS_OUTPUT Output;
    
    AttenuationFactors opacityShadowAttn = CalcShadowFactorsFOM(In.LightCoords.OpacityShadowCoords);
    AttenuationFactors shadowAttn = CalcShadowFactorsFiltered(In.LightCoords.ShadowCoords);
    AttenuationFactors beamAttn = CalcBeamShapeAttenuations(In.LightCoords.BeamCoords);
    AttenuationFactors combinedAttn = Combine(Combine(shadowAttn,beamAttn),opacityShadowAttn);

    float3 illumination = CalcIllumination(combinedAttn);
    
    float4 tex = GetParticleRGBA(g_SamplerParticle,In.TextureUVAndOpacity.xy,In.TextureUVAndOpacity.z);

    Output.RGBColor = float4(illumination * tex.rgb, tex.a);

    return Output;
}

PS_OUTPUT RenderSoftParticlesToScenePSLightingPS( VS_SCENE_PARTICLE_OUTPUT_PS_LIGHTING In )
{
	PS_OUTPUT Output = RenderParticlesToScenePSLightingPS(In);
	Output.RGBColor.a *= GetSoftParticleAlphaMultiplier(floor(In.Position.xy), In.ViewPosZ);
	return Output;
}

PS_OUTPUT RenderParticlesToSceneVSDSLighting( VS_DS_SCENE_PARTICLE_OUTPUT_VS_DS_LIGHTING In ) 
{ 
    PS_OUTPUT Output;

    float3 illumination = CalcIllumination(In.LightAttenuationFactors);
    
    float4 tex = GetParticleRGBA(g_SamplerParticle,In.TextureUVAndOpacity.xy,In.TextureUVAndOpacity.z);

    Output.RGBColor = float4(illumination * tex.rgb, tex.a);

    return Output;
}

PS_OUTPUT RenderSoftParticlesToSceneVSDSLighting( VS_DS_SCENE_PARTICLE_OUTPUT_VS_DS_LIGHTING In ) 
{
	PS_OUTPUT Output = RenderParticlesToSceneVSDSLighting(In);
	Output.RGBColor.a *= GetSoftParticleAlphaMultiplier(floor(In.Position.xy), In.ViewPosZ);
	return Output;
}

PS_OUTPUT RenderParticlesToSceneWireframePS() 
{ 
    PS_OUTPUT Output;
    Output.RGBColor = float4(1,0,0,1);
    return Output;
}

PS_OUTPUT_MRT RenderParticlesToOpacityPS( VS_OPACITY_PARTICLE_OUTPUT In ) 
{ 
    PS_OUTPUT_MRT Output;

    float4 tex = GetParticleRGBA(g_SamplerParticle,In.TextureUVAndOpacity.xy,In.TextureUVAndOpacity.z);
    return CalcOpacityMap_FOM(GetOpacityQualityForLighting(), In.ShadowCoords.z, GetOpacityMultiplierForLighting() * tex.a);
}

//--------------------------------------------------------------------------------------
// Techniques
//--------------------------------------------------------------------------------------
technique11 RenderParticlesToScene_LM_Vertex
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, RenderParticlesToSceneVSLightingVS() ) );
        SetGeometryShader( NULL );
        SetHullShader( NULL );
        SetDomainShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, RenderParticlesToSceneVSDSLighting() ) );

        SetBlendState( TranslucentBlendRGB, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DepthReadOnly, 0 );
        SetRasterizerState( SolidNoCull );
    }
}

technique11 RenderSoftParticlesToScene_LM_Vertex
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, RenderParticlesToSceneVSLightingVS() ) );
        SetGeometryShader( NULL );
        SetHullShader( NULL );
        SetDomainShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, RenderSoftParticlesToSceneVSDSLighting() ) );

        SetBlendState( TranslucentBlendRGB, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DepthReadOnly, 0 );
        SetRasterizerState( SolidNoCull );
    }
}

technique11 RenderParticlesToScene_LM_Pixel
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, RenderParticlesToScenePSLightingVS() ) );
        SetGeometryShader( NULL );
        SetHullShader( NULL );
        SetDomainShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, RenderParticlesToScenePSLightingPS() ) );

        SetBlendState( TranslucentBlendRGB, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DepthReadOnly, 0 );
        SetRasterizerState( SolidNoCull );
    }
}

technique11 RenderSoftParticlesToScene_LM_Pixel
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, RenderParticlesToScenePSLightingVS() ) );
        SetGeometryShader( NULL );
        SetHullShader( NULL );
        SetDomainShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, RenderSoftParticlesToScenePSLightingPS() ) );

        SetBlendState( TranslucentBlendRGB, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DepthReadOnly, 0 );
        SetRasterizerState( SolidNoCull );
    }
}

technique11 RenderParticlesToScene_LM_Tessellated
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, RenderParticlesToSceneDSLightingVS() ) );
        SetGeometryShader( NULL );
        SetHullShader( CompileShader( hs_5_0, RenderParticlesToSceneDSLightingHS() ) );
        SetDomainShader( CompileShader( ds_5_0, RenderParticlesToSceneDSLightingDS() ) );
        SetPixelShader( CompileShader( ps_4_0, RenderParticlesToSceneVSDSLighting() ) );

        SetBlendState( TranslucentBlendRGB, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DepthReadOnly, 0 );
        SetRasterizerState( SolidNoCull );
    }
}

technique11 RenderSoftParticlesToScene_LM_Tessellated
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, RenderParticlesToSceneDSLightingVS() ) );
        SetGeometryShader( NULL );
        SetHullShader( CompileShader( hs_5_0, RenderParticlesToSceneDSLightingHS() ) );
        SetDomainShader( CompileShader( ds_5_0, RenderParticlesToSceneDSLightingDS() ) );
        SetPixelShader( CompileShader( ps_4_0, RenderSoftParticlesToSceneVSDSLighting() ) );

        SetBlendState( TranslucentBlendRGB, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DepthReadOnly, 0 );
        SetRasterizerState( SolidNoCull );
    }
}

technique11 RenderParticlesToSceneWireframeOverride
{
    pass P0
    {          
        SetPixelShader( CompileShader( ps_4_0, RenderParticlesToSceneWireframePS() ) );
        SetBlendState( OpaqueBlendRGB, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DepthReadOnly, 0 );
        SetRasterizerState( WireframeNoCull );
    }
}

technique11 RenderParticlesToOpacity
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, RenderParticlesToOpacityVS() ) );
        SetGeometryShader( NULL );
        SetHullShader( NULL );
        SetDomainShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, RenderParticlesToOpacityPS() ) );

        SetBlendState( FOMBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( NoDepthStencil, 0 );
        SetRasterizerState( SolidNoCull );
    }
}

technique11 RenderLowResParticlesToSceneOverride
{
    pass P0
    {          
        SetBlendState( LowResRenderBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}

#endif // PARTICLE_SYSTEM_FX
