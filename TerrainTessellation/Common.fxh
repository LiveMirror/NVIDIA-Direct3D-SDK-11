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

#ifndef INCLUDED_COMMON_FXH
#define INCLUDED_COMMON_FXH

static const int CONTROL_VTX_PER_TILE_EDGE = 9;
static const int PATCHES_PER_TILE_EDGE = 8;
static const float RECIP_CONTROL_VTX_PER_TILE_EDGE = 1.0 / 9;
static const float WORLD_SCALE = 400;
static const float VERTICAL_SCALE = 0.65;
static const float WORLD_UV_REPEATS = 8;	// How many UV repeats across the world for fractal generation.
static const float WORLD_UV_REPEATS_RECIP = 1.0 / WORLD_UV_REPEATS;

int3   g_FractalOctaves;		// ridge, fBm, uv twist
float3 g_TextureWorldOffset;	// Offset of fractal terrain in texture space.
float  g_CoarseSampleSpacing;	// World space distance between samples in the coarse height map.

struct Adjacency
{
	// These are the size of the neighbours along +/- x or y axes.  For interior tiles
	// this is 1.  For edge tiles it is 0.5 or 2.0.
	float neighbourMinusX : ADJACENCY_SIZES0;
	float neighbourMinusY : ADJACENCY_SIZES1;
	float neighbourPlusX  : ADJACENCY_SIZES2;
	float neighbourPlusY  : ADJACENCY_SIZES3;
};

struct AppVertex
{
	float2 position  : POSITION_2D;
	Adjacency adjacency;
    uint VertexId    : SV_VertexID;
    uint InstanceId  : SV_InstanceID;
};

SamplerState SamplerRepeatMaxAniso
{
    Filter = ANISOTROPIC;
	MaxAnisotropy = 16;
    AddressU = Wrap;
    AddressV = Wrap;
};

SamplerState SamplerRepeatMedAniso
{
    Filter = ANISOTROPIC;
	MaxAnisotropy = 4;
    AddressU = Wrap;
    AddressV = Wrap;
};

SamplerState SamplerRepeatLinear
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

SamplerState SamplerClampLinear
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
}; 

SamplerState SamplerRepeatPoint
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Wrap;
    AddressV = Wrap;
};

#endif	//INCLUDED_COMMON_FXH
