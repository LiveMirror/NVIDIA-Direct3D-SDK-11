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

#ifndef OPAQUEOBJECT_FX
#define OPAQUEOBJECT_FX

//--------------------------------------------------------------------------------------
// Includes
//--------------------------------------------------------------------------------------
#include "LightSource.fx"

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
struct VS_OPAQUE_OBJECT_OUTPUT
{
    float4 Position   : SV_Position;   // vertex position
    float3 ViewPos    : TEXCOORD1;
    float3 Normal     : TEXCOORD2;
    LightSourceCoords LightCoords;
};

struct VS_OPAQUE_OBJECT_SHADOW_OUTPUT
{
    float4 Position   : SV_Position;   // vertex position
    float  LinearZ    : TEXCOORD1;
};

struct PS_OPAQUE_OBJECT_OUTPUT
{
    float4 RGBColor : SV_Target;  // Pixel color    
};

//--------------------------------------------------------------------------------------
// Render state
//--------------------------------------------------------------------------------------
RasterizerState SolidNoCullWithBias
{
    FillMode = Solid;
    CullMode = None;
    MultisampleEnable = True;
    DepthBias = 1;
    SlopeScaledDepthBias = float(1 + SHADOW_FILTER_LOOP_LIMIT);
};

//--------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------
VS_OPAQUE_OBJECT_OUTPUT RenderOpaqueObjectToScene_VS( float3 vPos : POSITION, float3 vNormal : NORMAL )
{
    VS_OPAQUE_OBJECT_OUTPUT Output;

    float4 WorldPos = mul(float4(vPos.xyz,1.f), g_mLocalToWorld);
    float4 ViewPos = mul(WorldPos, g_mWorldToView);
    Output.Position = mul(ViewPos,g_mViewToProj);
    Output.ViewPos = ViewPos.xyz;
    
    Output.LightCoords = CalcLightSourceCoords(ViewPos.xyz);
    Output.Normal = mul(vNormal, (float3x3)g_mLocalToWorld);

    return Output;
}

VS_OPAQUE_OBJECT_OUTPUT RenderOpaqueObjectThread_VS( float3 vPos : POSITION, float3 vNormal : NORMAL )
{
    VS_OPAQUE_OBJECT_OUTPUT Output;
    
    float4 WorldPos = mul(float4(vPos.xyz,1.f), g_mLocalToWorld);
    float4 ViewPos = mul(WorldPos, g_mWorldToView);
    Output.Position = mul(ViewPos,g_mViewToProj);
    Output.ViewPos = ViewPos.xyz;
    
    Output.LightCoords = CalcLightSourceCoords(ViewPos.xyz);
    Output.Normal = mul(vNormal, (float3x3)g_mLocalToWorld);

    // We modify the normal based on viewing angle
    float3 PosToEye = g_EyePoint - WorldPos.xyz;
    float3 Perp = cross(PosToEye, Output.Normal);
    Output.Normal = normalize(cross(Output.Normal, Perp));

    return Output;
}

float4 CalcOpaqueObjectLighting(AttenuationFactors shadowAttn, VS_OPAQUE_OBJECT_OUTPUT In)
{
    float3 DiffuseColor = float3(0.1f,0.1f,0.1f);
    float3 PostMultilpier = float3(10.f,10.f,2.f);

    AttenuationFactors beamAttn = CalcBeamShapeAttenuations(In.LightCoords.BeamCoords);
    AttenuationFactors combinedAttn = Combine(beamAttn, shadowAttn);

    float3 nrmNml = normalize(In.Normal);
    
    float4 result;
    result.rgb = PostMultilpier * CalcPhong(combinedAttn, In.ViewPos, nrmNml, DiffuseColor, 100.f);
    result.a = 1.f;
    
    return result;
}

PS_OPAQUE_OBJECT_OUTPUT RenderOpaqueObjectToScene_PS(VS_OPAQUE_OBJECT_OUTPUT In, bool isFront : SV_IsFrontFace) 
{ 
    PS_OPAQUE_OBJECT_OUTPUT Output;
    
    AttenuationFactors opacityShadowAttn = CalcShadowFactorsFOM(In.LightCoords.OpacityShadowCoords);
    AttenuationFactors shadowAttn = CalcShadowFactorsUnfiltered(In.LightCoords.ShadowCoords);
    AttenuationFactors combinedAttn = Combine(opacityShadowAttn, shadowAttn);
    Output.RGBColor = isFront ? CalcOpaqueObjectLighting(combinedAttn, In) : float4(0,0,0,1);

    return Output;
}

VS_OPAQUE_OBJECT_SHADOW_OUTPUT RenderOpaqueObjectToShadowMap_VS( float3 vPos : POSITION )
{
    VS_OPAQUE_OBJECT_SHADOW_OUTPUT Output;

    float4 WorldPos = mul(float4(vPos.xyz,1.f), g_mLocalToWorld);
    float4 ViewPos = mul(WorldPos, g_mWorldToView);

    Output.LinearZ = ViewPos.z * g_zScaleAndBias.x + g_zScaleAndBias.y;
    Output.Position = mul(ViewPos,g_mViewToProj);

    return Output;
}

PS_OPAQUE_OBJECT_OUTPUT RenderOpaqueObjectToShadowMap_PS(VS_OPAQUE_OBJECT_SHADOW_OUTPUT In) 
{ 
    PS_OPAQUE_OBJECT_OUTPUT Output;
    Output.RGBColor = In.LinearZ;
    return Output;
}

//--------------------------------------------------------------------------------------
// Techniques
//--------------------------------------------------------------------------------------
technique11 RenderOpaqueObjectToScene
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, RenderOpaqueObjectToScene_VS() ) );
        SetGeometryShader( NULL );
        SetHullShader( NULL );
        SetDomainShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, RenderOpaqueObjectToScene_PS() ) );

        SetBlendState( OpaqueBlendRGB, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DepthReadWrite, 0 );
        SetRasterizerState( SolidNoCull );
    }
}

technique11 RenderOpaqueObjectThreadOverride
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, RenderOpaqueObjectThread_VS() ) );
    }
}

technique11 RenderOpaqueObjectToShadowMap
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, RenderOpaqueObjectToShadowMap_VS() ) );
        SetGeometryShader( NULL );
        SetHullShader( NULL );
        SetDomainShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, RenderOpaqueObjectToShadowMap_PS() ) );

        SetBlendState( OpaqueBlendRGB, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DepthReadWrite, 0 );
        SetRasterizerState( SolidNoCullWithBias );
    }
}

#endif // OPAQUEOBJECT_FX
