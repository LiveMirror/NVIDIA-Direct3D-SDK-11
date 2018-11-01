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

#ifndef GLOBALS_FX
#define GLOBALS_FX

//--------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------
#ifndef NUM_AA_SAMPLES
#define NUM_AA_SAMPLES 1
#endif

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
#define M_PI 3.1415926535897932384626433832795
#define M_TWO_PI (2.f * M_PI)

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
float4x4 g_mLocalToWorld;
float4x4 g_mWorldToView;
float4x4 g_mViewToProj;
float4x4 g_mProjToView;
float2 g_zScaleAndBias;
float3 g_EyePoint;
float3 g_ZLinParams;

//--------------------------------------------------------------------------------------
// General-purpose structs etc.
//--------------------------------------------------------------------------------------
struct PS_OUTPUT
{
    float4 RGBColor : SV_Target;
};

struct PS_OUTPUT_MRT
{
    float4 MRT0 : SV_Target0;
    float4 MRT1 : SV_Target1;
    float4 MRT2 : SV_Target2;
    float4 MRT3 : SV_Target3;
};

//--------------------------------------------------------------------------------------
// Render state
//--------------------------------------------------------------------------------------
BlendState OpaqueBlendRGB
{
    BlendEnable[0] = FALSE;
    RenderTargetWriteMask[0] = 0x7;
};

BlendState OpaqueBlendRGBA
{
    BlendEnable[0] = FALSE;
    RenderTargetWriteMask[0] = 0xF;
};

BlendState TranslucentBlendRGB
{
    BlendEnable[0] = TRUE;
    RenderTargetWriteMask[0] = 0x7;
    
    SrcBlend = SRC_ALPHA;
    DestBlend = INV_SRC_ALPHA;
    BlendOp = Add;
};

BlendState AdditiveBlendRGB
{
    BlendEnable[0] = TRUE;
    RenderTargetWriteMask[0] = 0x7;
    
    SrcBlend = ONE;
    DestBlend = ONE;
    BlendOp = Add;
};

BlendState NoColorWrites
{
    BlendEnable[0] = FALSE;
    RenderTargetWriteMask[0] = 0x0;
};

DepthStencilState NoDepthStencil
{
    DepthEnable = FALSE;
    StencilEnable = FALSE;
};

DepthStencilState DepthReadWrite
{
    DepthEnable = TRUE;
    DepthWriteMask = ALL;
    DepthFunc = LESS_EQUAL;
    StencilEnable = FALSE;
};

DepthStencilState DepthReadOnly
{
    DepthEnable = TRUE;
    DepthWriteMask = ZERO;
    DepthFunc = LESS_EQUAL;
    StencilEnable = FALSE;
};

RasterizerState SolidNoCull
{
    FillMode = Solid;
    CullMode = None;
    MultisampleEnable = True;
};

RasterizerState WireframeNoCull
{
    FillMode = Wireframe;
    CullMode = None;
    MultisampleEnable = True;
};

//--------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------
PS_OUTPUT RenderToSceneOverdrawMeteringPS() 
{ 
    PS_OUTPUT Output;
    Output.RGBColor = 0.005f * float4(1,1,1,1);
    return Output;
}

//--------------------------------------------------------------------------------------
// Techniques
//--------------------------------------------------------------------------------------
technique11 RenderToSceneOverdrawMeteringOverride
{
    pass P0
    {          
        SetPixelShader( CompileShader( ps_4_0, RenderToSceneOverdrawMeteringPS() ) );
        SetBlendState( AdditiveBlendRGB, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}

#endif // GLOBALS_FX
