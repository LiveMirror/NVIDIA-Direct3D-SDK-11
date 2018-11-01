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

#include "Common.fxh"
#include "INoise.fxh"

// z in both cases is the height scale.
float3 g_DeformMin, g_DeformMax;

// Null in that there is no associated VB set by the API.
struct NullVertex
{
    uint VertexId   : SV_VertexID;
    uint InstanceId : SV_InstanceID;
};

struct DeformVertex
{
    float4 pos : SV_Position;
    float2 texCoord : TEXCOORD1;
};

DeformVertex InitializationVS( NullVertex input )
{
    DeformVertex output = (DeformVertex) 0;
    
	if (input.VertexId == 0)
	{
	    output.pos = float4(g_DeformMin.x, g_DeformMin.y, 0, 1);
	    output.texCoord = float2(0,0);
	}
	else if (input.VertexId == 1)
	{
	    output.pos = float4(g_DeformMax.x, g_DeformMin.y, 0, 1);
	    output.texCoord = float2(1,0);
	}
	else if (input.VertexId == 2)
	{
	    output.pos = float4(g_DeformMin.x, g_DeformMax.y, 0, 1);
	    output.texCoord = float2(0,1);
	}
	else if (input.VertexId == 3)
	{
	    output.pos = float4(g_DeformMax.x, g_DeformMax.y, 0, 1);
	    output.texCoord = float2(1,1);
	}
    
    return output;
}

float3 debugCubes(float2 uv)
{
	const float HORIZ_SCALE = 4, VERT_SCALE = 1;
	uv *= HORIZ_SCALE;
	return VERT_SCALE * floor(fmod(uv.x, 2.0)) * floor(fmod(uv.y, 2.0));
}

float3 debugXRamps(float2 uv)
{
	const float HORIZ_SCALE = 2, VERT_SCALE = 1;
	uv *= HORIZ_SCALE;
	return VERT_SCALE * frac(uv.x);
}

float3 debugSineHills(float2 uv)
{
	const float HORIZ_SCALE = 2 * 3.14159, VERT_SCALE = 0.5;
	uv *= HORIZ_SCALE;
	//uv += 2.8;			// arbitrarily not centered - test asymetric fns.
	return VERT_SCALE * (sin(uv.x) + 1) * (sin(uv.y) + 1);
}

float3 debugFlat(float2 uv)
{
	const float VERT_SCALE = 0.1;
	return VERT_SCALE;
}

float4 InitializationPS( DeformVertex input ) : SV_Target
{
	const float2 uv = g_TextureWorldOffset.xz + WORLD_UV_REPEATS * input.texCoord;
	//return float4(debugXRamps(uv),  1);
	//return float4(debugFlat(uv),  1);
	//return float4(debugSineHills(uv),  1);
	//return float4(debugCubes(uv), 1);
	return hybridTerrain(uv, g_FractalOctaves);
}

Texture2D g_InputTexture;

float4 GradientPS( DeformVertex input ) : SV_Target
{
	input.texCoord.y = 1 - input.texCoord.y;
	float x0 = g_InputTexture.Sample(SamplerClampLinear, input.texCoord, int2( 1,0)).x;
	float x1 = g_InputTexture.Sample(SamplerClampLinear, input.texCoord, int2(-1,0)).x;
	float y0 = g_InputTexture.Sample(SamplerClampLinear, input.texCoord, int2(0, 1)).x;
	float y1 = g_InputTexture.Sample(SamplerClampLinear, input.texCoord, int2(0,-1)).x;
	return float4(x0-x1, y0-y1, 0,0);
}

BlendState NoBlending
{
    RenderTargetWriteMask[0] = 0xf;
    BlendEnable[0] = false;
};

DepthStencilState DisableDepthWrites
{
    DepthEnable = false;
    DepthWriteMask = ZERO;
    DepthFunc = Less;

    StencilEnable = false;
    StencilReadMask = 0xFF;
    StencilWriteMask = 0x00;
};

RasterizerState NoMultisampling
{
    MultisampleEnable = false;
    CullMode = None;
};

technique10 InitializationTechnique
{
    pass Init
    {
		// Same as deformation, but with an alternate PS and without blending.
        SetBlendState( NoBlending, float4( 0, 0, 0, 0 ), 0xffffffff );
        SetDepthStencilState( DisableDepthWrites, 0 );
        SetRasterizerState( NoMultisampling );

        SetVertexShader( CompileShader( vs_4_0, InitializationVS()));
        SetGeometryShader(NULL);
        SetPixelShader( CompileShader( ps_4_0, InitializationPS()));
    }
}

technique10 GradientTechnique
{
    pass Grad
    {
		// Same as deformation, but with an alternate PS and without blending.
        SetBlendState( NoBlending, float4( 0, 0, 0, 0 ), 0xffffffff );
        SetDepthStencilState( DisableDepthWrites, 0 );
        SetRasterizerState( NoMultisampling );

        SetVertexShader( CompileShader( vs_4_0, InitializationVS()));
        SetGeometryShader(NULL);
        SetPixelShader( CompileShader( ps_4_0, GradientPS()));
    }
}
