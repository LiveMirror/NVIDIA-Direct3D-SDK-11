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

#ifndef FLOOR_FX
#define FLOOR_FX

//--------------------------------------------------------------------------------------
// Includes
//--------------------------------------------------------------------------------------
#include "LightSource.fx"

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
struct VS_FLOOR_OUTPUT
{
    float4 Position   : SV_Position;   // vertex position
    float2 FaceAttributes : TEXCOORD0;
    float3 ViewPos        : TEXCOORD1;
    LightSourceCoords LightCoords;
};

struct PS_FLOOR_OUTPUT
{
    float4 RGBColor : SV_Target;  // Pixel color    
};

//--------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------
VS_FLOOR_OUTPUT RenderFloorVS( float3 vPos : POSITION, float2 vFaceAttributes : ATTRIB )
{
    VS_FLOOR_OUTPUT Output;

    float4 ViewPos = mul(float4(vPos.xyz,1.f), g_mWorldToView);
    Output.Position = mul(ViewPos,g_mViewToProj);
    Output.ViewPos = ViewPos.xyz;
    
    Output.FaceAttributes = vFaceAttributes;
    Output.LightCoords = CalcLightSourceCoords(ViewPos.xyz);

    return Output;    
}

void GetFloorAttributes(float2 faceAttributes, out float3 normal, out float3 color)
{
    // 'x' component is tile type
    color = faceAttributes.x > 0.5f ? float3(0.1,0.4,0.05) : float3(0.6,0.6,0.6);
    
    // 'y' component is bevel type
    if(0.5f > faceAttributes.y)
        normal = float3(0,1,0);
    else if(1.5f > faceAttributes.y)
        normal = float3(-0.707f,0.707f,0);
    else if(2.5f > faceAttributes.y)
        normal = float3(0,0.707f,0.707f);
    else if(3.5f > faceAttributes.y)
        normal = float3(0.707f,0.707f,0);
    else if(4.5f > faceAttributes.y)
        normal = float3(0,0.707f,-0.707f);
    else
        normal = float3(0,1,0);
}

float4 CalcFloorLighting(AttenuationFactors shadowAttn, VS_FLOOR_OUTPUT In)
{
    float3 floorNormal;
    float3 floorColor;
    GetFloorAttributes(In.FaceAttributes, floorNormal, floorColor);
    
    AttenuationFactors beamAttn = CalcBeamShapeAttenuations(In.LightCoords.BeamCoords);
    AttenuationFactors combinedAttn = Combine(beamAttn, shadowAttn);
    
    float4 result;
    result.rgb = CalcPhong(combinedAttn, In.ViewPos, floorNormal, floorColor, 100.f);
    result.a = 1.f;
    
    return result;
}

PS_FLOOR_OUTPUT RenderFloorPS(VS_FLOOR_OUTPUT In, bool isFront : SV_IsFrontFace) 
{ 
    PS_FLOOR_OUTPUT Output;
    
    AttenuationFactors opacityShadowAttn = CalcShadowFactorsAtFarFOM(In.LightCoords.OpacityShadowCoords);
    AttenuationFactors shadowAttn = CalcShadowFactorsFiltered(In.LightCoords.ShadowCoords);
    Output.RGBColor = isFront ? CalcFloorLighting(Combine(opacityShadowAttn,shadowAttn), In) : float4(0,0,0,1);

    return Output;
}

//--------------------------------------------------------------------------------------
// Techniques
//--------------------------------------------------------------------------------------
technique11 RenderFloor
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, RenderFloorVS() ) );
        SetGeometryShader( NULL );
        SetHullShader( NULL );
        SetDomainShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, RenderFloorPS() ) );

        SetBlendState( OpaqueBlendRGB, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DepthReadWrite, 0 );
        SetRasterizerState( SolidNoCull );
    }
}

#endif // FLOOR_FX
