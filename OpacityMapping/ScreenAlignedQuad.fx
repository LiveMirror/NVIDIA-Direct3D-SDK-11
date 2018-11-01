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

#ifndef SCREENALIGNEDQUAD_FX
#define SCREENALIGNEDQUAD_FX

#include "Globals.fx"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
Texture2D  g_ScreenAlignedQuadSrc;
float4   g_ScreenAlignedQuadScaleAndOffset;
float    g_ScreenAlignedQuadMip;

static const float2 screenAlignedQuadCornerCoords[4] = {
    { 0, 0 },
    { 1, 0 },
    { 0, 1 },
    { 1, 1 }
};

//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
SamplerState g_SamplerScreenAlignedQuad
{
    Filter = MIN_MAG_LINEAR_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
struct VS_SAQ_OUTPUT
{
    float4 Position   : SV_Position;
    float2 TexCoord   : TEXCOORD;
};

//--------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------
VS_SAQ_OUTPUT ScreenAlignedQuad_VS(uint vID : SV_VertexID)
{
    float2 SrcCoord = screenAlignedQuadCornerCoords[vID];

    VS_SAQ_OUTPUT Out;
    Out.Position = float4(SrcCoord.xy * g_ScreenAlignedQuadScaleAndOffset.xy + g_ScreenAlignedQuadScaleAndOffset.zw,0.5f,1.f);
    Out.TexCoord = SrcCoord.xy;
    return Out;
}

float4 ScreenAlignedQuad_PS(VS_SAQ_OUTPUT In) : SV_Target
{
    return g_ScreenAlignedQuadSrc.SampleLevel(g_SamplerScreenAlignedQuad, In.TexCoord, g_ScreenAlignedQuadMip);
}

float GetScreenAlignedQuadTranslucentOpacity() { return 0.5f; }

float4 ScreenAlignedQuadTranslucent_PS(VS_SAQ_OUTPUT In) : SV_Target
{
    float4 result;
    result.rgb = g_ScreenAlignedQuadSrc.SampleLevel(g_SamplerScreenAlignedQuad, In.TexCoord, g_ScreenAlignedQuadMip).rgb;
    result.a = GetScreenAlignedQuadTranslucentOpacity();
    return result;
}

//--------------------------------------------------------------------------------------
// Techniques
//--------------------------------------------------------------------------------------
technique11 ScreenAlignedQuad
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, ScreenAlignedQuad_VS() ) );
        SetGeometryShader( NULL );
        SetHullShader( NULL );
        SetDomainShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, ScreenAlignedQuad_PS() ) );

        SetBlendState( OpaqueBlendRGBA, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( NoDepthStencil, 0 );
        SetRasterizerState( SolidNoCull );
    }
}

technique11 ScreenAlignedQuadTranslucent
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, ScreenAlignedQuad_VS() ) );
        SetGeometryShader( NULL );
        SetHullShader( NULL );
        SetDomainShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, ScreenAlignedQuadTranslucent_PS() ) );

        SetBlendState( TranslucentBlendRGB, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( NoDepthStencil, 0 );
        SetRasterizerState( SolidNoCull );
    }
}

#endif // SCREENALIGNEDQUAD_FX
