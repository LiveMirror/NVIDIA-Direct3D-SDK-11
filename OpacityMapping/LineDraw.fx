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

#ifndef LINE_DRAW_FX
#define LINE_DRAW_FX

#include "Globals.fx"

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
float4 g_LineColor;

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
struct VS_LINEDRAW_OUTPUT
{
    float4 Position   : SV_Position;
};

//--------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------
VS_LINEDRAW_OUTPUT RenderLinesVS( float4 vPos : POSITION )
{
    VS_LINEDRAW_OUTPUT Output;

    float4 WorldPos = mul(vPos, g_mLocalToWorld);
    float4 ViewPos = mul(WorldPos, g_mWorldToView);
    Output.Position = mul(ViewPos,g_mViewToProj);
    
    return Output;    
}

PS_OUTPUT RenderLinesPS() 
{ 
    PS_OUTPUT Output;
    Output.RGBColor = g_LineColor;
    return Output;
}

//--------------------------------------------------------------------------------------
// Techniques
//--------------------------------------------------------------------------------------
technique11 RenderLines
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, RenderLinesVS() ) );
        SetGeometryShader( NULL );
        SetHullShader( NULL );
        SetDomainShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, RenderLinesPS() ) );

        SetBlendState( TranslucentBlendRGB, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DepthReadOnly, 0 );
        SetRasterizerState( SolidNoCull );
    }
}

#endif // LINE_DRAW_FX
