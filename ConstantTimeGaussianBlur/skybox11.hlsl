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

// RGB color space is device-dependent, but here we neglect this issue and just
// use RGB709 spec. More precise usage should be involved with a color profile
// for display device.
static const float3 RGB709_to_CIE_Y = float3(0.212671f, 0.715160f, 0.072169f);


cbuffer cbPerObject : register( b0 )
{
    row_major matrix    g_mWorldViewProjection	: packoffset( c0 );
}

TextureCube	g_EnvironmentTexture : register( t0 );
SamplerState g_sam : register( s0 );

struct SkyboxVS_Input
{
    float4 Pos : POSITION;
};

struct SkyboxVS_Output
{
    float4 Pos : SV_POSITION;
    float3 Tex : TEXCOORD0;
};

SkyboxVS_Output SkyboxVS( SkyboxVS_Input Input )
{
    SkyboxVS_Output Output;
    
    Output.Pos = Input.Pos;
    Output.Tex = normalize( mul(Input.Pos, g_mWorldViewProjection) );
    
    return Output;
}

float4 SkyboxPS_Color( SkyboxVS_Output Input ) : SV_TARGET
{
    float4 color = g_EnvironmentTexture.Sample( g_sam, Input.Tex );

    return color;
}

float4 SkyboxPS_Gray( SkyboxVS_Output Input ) : SV_TARGET
{
    float4 color = g_EnvironmentTexture.Sample( g_sam, Input.Tex );
	float gray = dot(color, RGB709_to_CIE_Y);

    return gray;
}
