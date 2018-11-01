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

#ifndef LOW_RES_OFFSCREEN_FX
#define LOW_RES_OFFSCREEN_FX

#include "ScreenAlignedQuad.fx"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
#if NUM_AA_SAMPLES > 1
	Texture2DMS<float2, NUM_AA_SAMPLES> g_FullResDepth : register(t0);
#else
	Texture2D<float2> g_FullResDepth : register(t0);
#endif

Texture2D  g_ReducedResDepth;

float g_DepthThreshold;
float g_ReductionFactor;

//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
SamplerState g_SamplerLoResBilinear
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState g_SamplerLoResNearest
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

//--------------------------------------------------------------------------------------
// Render state
//--------------------------------------------------------------------------------------
BlendState LowResUpsampleBlend
{
    BlendEnable[0] = TRUE;
    RenderTargetWriteMask[0] = 0x7;
    
    SrcBlend = ONE;
    DestBlend = SRC_ALPHA;
    BlendOp = Add;
};

BlendState LowResRenderBlend
{
    BlendEnable[0] = TRUE;
    RenderTargetWriteMask[0] = 0xF;
    
    SrcBlend = SRC_ALPHA;
    DestBlend = INV_SRC_ALPHA;
    BlendOp = Add;
    
    SrcBlendAlpha = Zero;
    DestBlendAlpha = INV_SRC_ALPHA;
    BlendOpAlpha = Add;
};

//--------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------
void UpdateNearestSample(	inout float MinDist,
							inout float2 NearestUV,
							float Z,
							float2 UV,
							float ZFull
							)
{
	float Dist = abs(Z - ZFull);
	if (Dist < MinDist)
	{
		MinDist = Dist;
		NearestUV = UV;
	}
}

float4 FetchLowResDepths(float2 UV)
{
	float4 deviceZ = g_ReducedResDepth.GatherRed(g_SamplerLoResBilinear, UV);
	
	// Linearize
	return deviceZ / (g_ZLinParams.x - deviceZ * g_ZLinParams.y);
}

#if NUM_AA_SAMPLES > 1
	float FetchFullResDepthAA(uint2 pixPos, uint sampleIndex)
	{
		float deviceZ = g_FullResDepth.Load(pixPos.xy, sampleIndex).x;
		
		// Linearize
		return deviceZ / (g_ZLinParams.x - deviceZ * g_ZLinParams.y);
	}
#else
	float FetchFullResDepth(uint2 pixPos)
	{
		float deviceZ = g_FullResDepth.Load(int3(pixPos.xy, 0)).x;
		
		// Linearize
		return deviceZ / (g_ZLinParams.x - deviceZ * g_ZLinParams.y);
	}
#endif

#if NUM_AA_SAMPLES > 1
float4 LowResUpsample_PS(VS_SAQ_OUTPUT In, uint sampleIndex : SV_SampleIndex) : SV_Target
#else
float4 LowResUpsample_PS(VS_SAQ_OUTPUT In) : SV_Target
#endif
{
	uint w, h;

	g_ReducedResDepth.GetDimensions(w,h);
	float2 LowResTexelSize = 1.f/float2(w,h);

#if NUM_AA_SAMPLES > 1
	uint NumSamples;
	g_FullResDepth.GetDimensions(w, h, NumSamples);
#else
	g_FullResDepth.GetDimensions(w, h);
#endif

	// This corrects for cases where the hi-res texture doesn't correspond to 100% of the low-res
	// texture (e.g. 1023x1023 vs 512x512)
	float2 LoResUV = g_ReductionFactor * LowResTexelSize * float2(w,h) * In.TexCoord;

#if NUM_AA_SAMPLES > 1
	float ZFull = FetchFullResDepthAA(In.Position.xy,sampleIndex);
#else
	float ZFull = FetchFullResDepth(In.Position.xy);
#endif

	float MinDist = 1.e8f;

	float4 ZLo = FetchLowResDepths(LoResUV);
	
	float2 UV00 = LoResUV - 0.5 * LowResTexelSize;
	float2 NearestUV = UV00;
	float Z00 = ZLo.w;
	UpdateNearestSample(MinDist, NearestUV, Z00, UV00, ZFull);
	
	float2 UV10 = float2(UV00.x+LowResTexelSize.x, UV00.y);
	float Z10 = ZLo.z;
	UpdateNearestSample(MinDist, NearestUV, Z10, UV10, ZFull);

	float2 UV01 = float2(UV00.x, UV00.y+LowResTexelSize.y);
	float Z01 = ZLo.x;
	UpdateNearestSample(MinDist, NearestUV, Z01, UV01, ZFull);
	
	float2 UV11 = UV00 + LowResTexelSize;
	float Z11 = ZLo.y;
	UpdateNearestSample(MinDist, NearestUV, Z11, UV11, ZFull);
	
	[branch]
	if (abs(Z00 - ZFull) < g_DepthThreshold &&
	    abs(Z10 - ZFull) < g_DepthThreshold &&
	    abs(Z01 - ZFull) < g_DepthThreshold &&
	    abs(Z11 - ZFull) < g_DepthThreshold)
	{
		return g_ScreenAlignedQuadSrc.SampleLevel(g_SamplerLoResBilinear, LoResUV, g_ScreenAlignedQuadMip);
	}
	else
	{
		return g_ScreenAlignedQuadSrc.SampleLevel(g_SamplerLoResNearest, NearestUV, g_ScreenAlignedQuadMip);
	}
}

//--------------------------------------------------------------------------------------
// Techniques
//--------------------------------------------------------------------------------------
technique11 LowResUpsample
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, ScreenAlignedQuad_VS() ) );
        SetGeometryShader( NULL );
        SetHullShader( NULL );
        SetDomainShader( NULL );
        SetPixelShader( CompileShader( ps_4_1, LowResUpsample_PS() ) );

        SetBlendState( LowResUpsampleBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( NoDepthStencil, 0 );
        SetRasterizerState( SolidNoCull );
    }
}

technique11 LowResUpsampleMetered
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, ScreenAlignedQuad_VS() ) );
        SetGeometryShader( NULL );
        SetHullShader( NULL );
        SetDomainShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, RenderToSceneOverdrawMeteringPS() ) );

        SetBlendState( AdditiveBlendRGB, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( NoDepthStencil, 0 );
        SetRasterizerState( SolidNoCull );
    }
}

#endif // LOW_RES_OFFSCREEN_FX
