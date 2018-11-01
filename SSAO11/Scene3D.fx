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

matrix g_WorldViewProjection;
bool   g_IsWhite;

Texture2D<float3> tColor;

SamplerState PointSampler
{
    Filter   = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

//-----------------------------------------------------------------------------
// Input/Output Attributes
//-----------------------------------------------------------------------------

struct VS_INPUT
{
    float3 Pos  : POSITION;
    float3 Norm : NORMAL;
};

struct VS_OUTPUT
{
    float4 HPosition    : SV_POSITION;
};

struct PS_INPUT
{
    float4 HPosition    : SV_POSITION;
};

struct PostProc_VSOut
{
    float4 pos : SV_Position;
    float2 uv  : TexCoord;
};

//-----------------------------------------------------------------------------
// States
//-----------------------------------------------------------------------------

BlendState NoBlending
{
    BlendEnable[0] = False;
};

DepthStencilState DepthState
{
    DepthEnable = True;
    DepthFunc = Less;
};

RasterizerState MS_RS
{
    CullMode = None; // Disable backface culling to avoid holes in AT-AT mesh
    MultisampleEnable = True;
};

//-----------------------------------------------------------------------------
// Geometry-rendering shaders
//-----------------------------------------------------------------------------

VS_OUTPUT GeometryVS( VS_INPUT input )
{
    VS_OUTPUT output;
    output.HPosition = mul( float4(input.Pos,1), g_WorldViewProjection );
    return output;
}

float4 GeometryPS( PS_INPUT In ) : SV_Target
{
    if (g_IsWhite) return float4(1,1,1,1);
    return float4(.457,.722, 0.0, 1);
}

//-----------------------------------------------------------------------------
// Post-processing shaders
//-----------------------------------------------------------------------------

// Vertex shader that generates a full screen quad with texcoords
// To use draw 3 vertices with primitive type triangle list
PostProc_VSOut FullScreenTriangleVS( uint id : SV_VertexID )
{
    PostProc_VSOut output = (PostProc_VSOut)0.0f;
    output.uv = float2( (id << 1) & 2, id & 2 );
    output.pos = float4( output.uv * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f), 0.0f, 1.0f );
    return output;
}

float3 Brighten(float3 color)
{
    return pow(saturate(color), 0.8);
}

float4 CopyColorPS( PostProc_VSOut IN ) : SV_TARGET
{
    float3 color = tColor.Sample(PointSampler, IN.uv);
    return float4(Brighten(color),1);
}

//-----------------------------------------------------------------------------
// Techniques
//-----------------------------------------------------------------------------

technique10 RenderSceneDiffuse
{
    pass p0
    {
        SetVertexShader  ( CompileShader( vs_5_0, GeometryVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader   ( CompileShader( ps_5_0, GeometryPS() ) );
        SetRasterizerState( MS_RS );
        SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState(DepthState, 0);
    }
}

technique10 CopyColor
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, FullScreenTriangleVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, CopyColorPS() ) );
        SetBlendState( NoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}
