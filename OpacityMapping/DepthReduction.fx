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

#include "Globals.fx"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
#if NUM_AA_SAMPLES > 1
	Texture2DMS<float2, NUM_AA_SAMPLES> g_DepthStencilSrc;
#else
	Texture2D<float2> g_DepthStencilSrc;
#endif

#if NUM_AA_SAMPLES > 2
#define DOWNSAMPLE_AA_INDEX (NUM_AA_SAMPLES - (NUM_AA_SAMPLES/4))
#else
#define DOWNSAMPLE_AA_INDEX 0
#endif

uint g_TileSize;

static const float2 cornerCoords[4] = {
    { -1,  1 },
    {  1,  1 },
    { -1, -1 },
    {  1, -1 }
};

//--------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------
float4 DownSampleDepthVS(uint vID : SV_VertexID) : SV_Position
{
    float4 coords = float4(cornerCoords[vID], 0.5f, 1.f);
    return coords;
}

#if NUM_AA_SAMPLES > 1
float4 DownSampleDepthPS(float4 fPixPos : SV_Position, out float depthOut : SV_Depth) : SV_Target
{
	uint2 tlPositon = floor(fPixPos.xy) * g_TileSize;
	
	// Point sampling
	float d = g_DepthStencilSrc.Load(tlPositon.xy,DOWNSAMPLE_AA_INDEX).x;
	
	depthOut = d;
	
	return float4(0,0,0,1);
}
#else
float4 DownSampleDepthPS(float4 fPixPos : SV_Position, out float depthOut : SV_Depth) : SV_Target
{
	uint2 tlPositon = floor(fPixPos.xy) * g_TileSize;
	
	// Point sampling
	float d = g_DepthStencilSrc.Load(int3(tlPositon.xy,0)).x;
	
	depthOut = d;
	
	return float4(0,0,0,1);
}
#endif

//--------------------------------------------------------------------------------------
// Techniques
//--------------------------------------------------------------------------------------
technique11 DownSampleDepth
{
    pass P0
    {          
        SetVertexShader( CompileShader( vs_4_0, DownSampleDepthVS() ) );
        SetGeometryShader( NULL );
        SetHullShader( NULL );
        SetDomainShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, DownSampleDepthPS() ) );

        SetBlendState( NoColorWrites, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DepthReadWrite, 0 );
        SetRasterizerState( SolidNoCull );
    }
}
