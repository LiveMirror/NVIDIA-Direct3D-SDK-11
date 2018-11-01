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

Texture2D g_StarColours, g_StarShape, g_fBmTexture;

float  g_HeightScale;
float  g_SizeScale;
float  g_AspectRatio;
float  g_Distortion;

float2 g_minClipSize = float2(1.0/2560.0, 1.0/1600);

// Texture samples
SamplerState SamplerClamp
{
    Filter = ANISOTROPIC;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState SamplerRepeat
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

row_major float4x4 g_WorldViewProj;

struct InstanceData
{
	float2 gLonLat   : GALACTIC_LATLON;		// polar coords
	float  magnitude : MAGNITUDE;
	float  BminusV   : B_MINUS_V;
};

struct ClipVertex
{
    float4 vPosition : SV_Position;
    float2 vUV : TEXCOORD0;
	float4 colour : COLOR0;
	float  shapeContrib : COLOR1;
};

float3 GalacticToCartesian(float2 gLonLat)
{
	float lat = gLonLat.y;

	// Arbitrary distortion in height and g_HeightScale for artistic effect on non-realistic renderings (Milky Way).
	const int mipLevel = 0;
	const float distortion = 2 * g_fBmTexture.SampleLevel(SamplerRepeat, float2(0.125 * gLonLat.x, 0), mipLevel).r - 1;

	// Galactic coords are just polar coords with reference to axes defined by the sun and our galaxy.
	// We ignore the reference axes.
	float3 result;
	const float infinity = 1000000;
	result.x = infinity *  cos(lat) * cos(gLonLat.x);
	result.y = infinity * (g_HeightScale * sin(lat) + g_Distortion * distortion);
	result.z = infinity *  cos(lat) * sin(gLonLat.x);
	return result;
}

// Data in the VB is real star data from the Bright Star Catalogue.  This shader converts it
// into something renderable.
ClipVertex StarDataVS(float2 vCorner : CORNER_OFFSET, InstanceData input)
{
	float3 worldPos = GalacticToCartesian(input.gLonLat);

    ClipVertex output = (ClipVertex) 0;
    output.vPosition = mul(float4(worldPos, 1.0f), g_WorldViewProj);

	// Place on the far plane.
	output.vPosition.z = output.vPosition.w;

	output.vUV = 0.5 * vCorner + 0.5;	// [-1,1] -> [0,1]
	output.shapeContrib = 1;
	output.colour.a = 1;

	// Visible mangitudes are about 6 to -1.5.  Turn this into a screenspace size - very ad hoc.
	float size = 0.4 * pow(2.6, clamp((3.8 - input.magnitude), 0.1, 1000));
	size = clamp(size, 0, 20);

	// vPosition.xy is in the clip-space cube.  To get squares in screenspace we need non-square
	// rectangles in clip.
	const float2 aspectAdjust = float2(1, g_AspectRatio);

	// Expand instance data points into quads by adding corners.
	float2 clipSize = aspectAdjust * g_SizeScale * 0.0011 * size;

	// Make sure that no stars disappear due to small size.  Clamp to a 1-pixel polygon 
	// size.  Should this vary with screen-res?  I'm not sure.
	if (clipSize.x < g_minClipSize.x || clipSize.y < g_minClipSize.y)
	{
		output.colour.a = 2560.0 * (g_minClipSize.x - clipSize.x);
		clipSize = g_minClipSize;	// one pixel-wide poly
		output.vUV = 0.5;			// dead centre => brightest point
		output.shapeContrib = 0;	// ignore the star-shape texture
	}

	output.vPosition.xy += clipSize * vCorner * output.vPosition.w;
	output.colour.a *= g_SizeScale;

	// The loading code for the star data normalizes the B-V colour so that all we need is a 
	// texture look-up.
	const float mip_level = 0;
	output.colour.rgb = g_StarColours.SampleLevel(SamplerClamp, float2(input.BminusV,0), mip_level).rgb;

    return output;
}
float4 StarPS(ClipVertex input) : SV_Target
{
	float4 shape = g_StarShape.Sample(SamplerClamp, input.vUV);
	shape = lerp(1, shape, input.shapeContrib);
	return shape * input.colour;
}

DepthStencilState DisableDepthWrites
{
    DepthEnable = false;
    StencilEnable = false;
};

RasterizerState Multisampling
{
    MultisampleEnable = true;
    CullMode = None;
};

BlendState NoBlending
{
    RenderTargetWriteMask[0] = 0xf;
    BlendEnable[0] = false;
};

BlendState AlphaBlending
{
    RenderTargetWriteMask[0] = 0xf;
    BlendEnable[0] = true;
    DestBlend = INV_SRC_ALPHA;
    SrcBlend  = SRC_ALPHA;
};

technique11 StarDataTechnique
{
    pass DrawStars
    {
        SetBlendState( AlphaBlending, float4( 0, 0, 0, 0 ), 0xffffffff );
        SetDepthStencilState( DisableDepthWrites, 0 );
        SetRasterizerState( Multisampling );

        SetVertexShader(CompileShader(vs_5_0, StarDataVS()));
        SetHullShader(NULL);
        SetDomainShader(NULL);
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, StarPS()));
    }
}
