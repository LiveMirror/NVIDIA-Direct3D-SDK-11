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

//-----------------------------------------------------------------------------
// Macros and constant values
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Shader constant buffers
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Buffers, textures & samplers
//-----------------------------------------------------------------------------

// FP32 texture containing the filtered image
Texture2D<float>	g_texMonoInput;
// R11G11B10 texture containing the filtered image
Texture2D<uint>		g_texColorInput;

SamplerState g_samplerLinear
{
    Filter = MIN_MAG_LINEAR_MIP_POINT;
    AddressU = CLAMP;
    AddressV = CLAMP;
};


//-----------------------------------------------------------------------------
// Structures
//-----------------------------------------------------------------------------
struct QuadVS_Output
{
    float4 Position : SV_POSITION;              
    float2 TexCoord : TEXCOORD;
};


//-----------------------------------------------------------------------------
// Name: Quad_VS                                        
// Type: Vertex shader
//-----------------------------------------------------------------------------
QuadVS_Output Quad_VS(float4 Pos : POSITION, float2 Tex : TEXCOORD0)
{
    QuadVS_Output Output;

    Output.Position = Pos;
    Output.TexCoord = Tex;
    
	return Output;
}

//-----------------------------------------------------------------------------
// Name: BlitMono_PS
// Type: Pixel shader                                      
// Desc: 
//-----------------------------------------------------------------------------
float4 BlitMono_PS(QuadVS_Output In) : SV_TARGET
{
	return g_texMonoInput.Sample(g_samplerLinear, In.TexCoord);
}

//-----------------------------------------------------------------------------
// Name: BlitColor_PS
// Type: Pixel shader                                      
// Desc: 
//-----------------------------------------------------------------------------
float4 BlitColor_PS(QuadVS_Output In) : SV_TARGET
{
	uint2 tex_dim;
	g_texColorInput.GetDimensions(tex_dim.x, tex_dim.y);

	uint2 location = uint2((uint)(tex_dim.x * In.TexCoord.x), (uint)(tex_dim.y * In.TexCoord.y));
	uint int_color = g_texColorInput[location];

	// Convert R11G11B10 to float3
	float4 color;
	color.r = (float)(int_color >> 21) / 2047.0f;
	color.g = (float)((int_color >> 10) & 0x7ff) / 2047.0f;
	color.b = (float)(int_color & 0x0003ff) / 1023.0f;
	color.a = 1;

	return color;
}


//-----------------------------------------------------------------------------
// State blocks
//-----------------------------------------------------------------------------
DepthStencilState Depth_Disabled
{
	DepthEnable			= FALSE;
	DepthWriteMask		= ZERO;
	StencilEnable		= FALSE;
};


//-----------------------------------------------------------------------------
// Name: Tech_FSQuad
// Type: Technique
// Desc: Post processing with a full screen quad
//-----------------------------------------------------------------------------
technique11 Tech_ShowImage
{
	pass Pass_ShowMonoImage
	{
		SetVertexShader(CompileShader(vs_5_0, Quad_VS()));
		SetPixelShader(CompileShader(ps_5_0, BlitMono_PS()));

		SetDepthStencilState(Depth_Disabled, 0);
	}

	pass Pass_ShowColorImage
	{
		SetVertexShader(CompileShader(vs_5_0, Quad_VS()));
		SetPixelShader(CompileShader(ps_5_0, BlitColor_PS()));

		SetDepthStencilState(Depth_Disabled, 0);
	}
}
