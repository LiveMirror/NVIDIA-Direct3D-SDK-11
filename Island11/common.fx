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

cbuffer QuadObject
{
    const float2 QuadVertices[4] =
    {
        {-1.0, -1.0},
        { 1.0, -1.0},
        {-1.0,  1.0},
        { 1.0,  1.0}
    };

    const float2 QuadTexCoordinates[4] =
    {
        {0.0, 1.0},
        {1.0, 1.0},
        {0.0, 0.0},
        {1.0, 0.0}
    };
}

SamplerState SamplerPointClamp
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState SamplerLinearClamp
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState SamplerLinearWrap
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

SamplerState SamplerAnisotropicWrap
{
    Filter = ANISOTROPIC;
    AddressU = Wrap;
    AddressV = Wrap;
	MaxAnisotropy = 16;
};

SamplerState SamplerCube
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
	AddressW = Clamp;
};

SamplerState SamplerLinearMirror
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Mirror;
    AddressV = Mirror;
};

SamplerState SamplerLinearBorderBlack
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Border;
    AddressV = Border;
    AddressW = Border;
    BorderColor = float4(0, 0, 0, 0);
};

SamplerComparisonState SamplerBackBufferDepth
{
    Filter = COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    AddressU = Border;
    AddressV = Border;
    BorderColor = float4(1, 1, 1, 1);
    ComparisonFunc = LESS_EQUAL;
};

SamplerComparisonState SamplerDepthAnisotropic
{
    Filter = COMPARISON_ANISOTROPIC;
    AddressU = Border;
    AddressV = Border;
    ComparisonFunc = LESS;
    BorderColor = float4(1, 1, 1, 1);
    MaxAnisotropy = 16;
};

RasterizerState CullBack
{
    CullMode = Back;
    FrontCounterClockwise = TRUE;
};

RasterizerState CullBackMS
{
    CullMode = Back;
    FrontCounterClockwise = TRUE;
    MultisampleEnable = TRUE;
};

RasterizerState CullFrontNoClip
{
    CullMode = Front;
    FrontCounterClockwise = TRUE;
    DepthClipEnable = FALSE;
};

RasterizerState CullFrontMS
{
    CullMode = Front;
    FrontCounterClockwise = TRUE;
    MultisampleEnable = TRUE;
};

RasterizerState NoCull
{
    CullMode = NONE;
};

RasterizerState NoCullMS
{
    CullMode = NONE;
    MultisampleEnable = TRUE;
};

RasterizerState Wireframe
{
    CullMode = NONE;
    FillMode = WIREFRAME;
};

RasterizerState WireframeMS
{
    CullMode = NONE;
    FillMode = WIREFRAME;
    MultisampleEnable = TRUE;
};

DepthStencilState DepthNormal
{
    DepthFunc = LESS_EQUAL;
};

DepthStencilState NoDepthStencil
{
    DepthEnable = FALSE;
};

DepthStencilState ReadDepthNoStencil
{
    DepthWriteMask = ZERO;
};

DepthStencilState DepthShadingPass
{
    DepthFunc = EQUAL;
    DepthWriteMask = ZERO;
};

BlendState NoBlending
{
    BlendEnable[0] = FALSE;
};

BlendState BlendingAdd
{
    BlendEnable[0] = TRUE;
    SrcBlend = ONE;
    DestBlend = ONE;
    BlendOp = ADD;
    SrcBlendAlpha = ZERO;
    DestBlendAlpha = DEST_ALPHA;
    BlendOpAlpha = ADD;
    RenderTargetWriteMask[0] = 0x0F;
};

BlendState AlphaBlending
{
    BlendEnable[0] = TRUE;
    SrcBlend = SRC_ALPHA;
    DestBlend = INV_SRC_ALPHA;
    BlendOp = ADD;
    SrcBlendAlpha = ZERO;
    DestBlendAlpha = DEST_ALPHA;
    BlendOpAlpha = ADD;
    RenderTargetWriteMask[0] = 0x0F;
};

float4 ColorPS(uniform float4 color) : SV_Target
{
    return color;
}

technique11 Default
{
    pass p0
    {
        SetRasterizerState(NoCull);
        SetDepthStencilState(DepthNormal, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(NULL);
        SetHullShader(NULL);
        SetDomainShader(NULL);
        SetGeometryShader(NULL);
        SetPixelShader(NULL);
    }
}
