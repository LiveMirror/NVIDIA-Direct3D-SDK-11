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

#pragma once

//constant data per frame
struct CB_PER_FRAME_CONSTANTS
{
	D3DXVECTOR4 vEyeDir;
	D3DXVECTOR4 vLightPos;
	D3DXVECTOR4 vLightDir;
	D3DXVECTOR4 vRandom;
	float  fLightAngle;
	float fTessellationFactor;
	float fDisplacementHeight;
	DWORD dwPadding[1];
};
struct CB_PER_DRAW_CONSTANTS
{
	D3DXMATRIX world;
	D3DXMATRIX worldView;
	D3DXMATRIX viewProjection;
	D3DXMATRIX mLightViewProjection;
};
//constant data per mesh
struct CB_PER_PATCH_CONSTANTS
{
	float weightsForRegularPatches[256][4];
};
struct CB_PER_MESH_CONSTANTS
{
	D3DXVECTOR4 diffuse;
	D3DXVECTOR4 center;
};
//vertex type for vertex shader 
struct Vertex
{
	D3DXVECTOR3 m_Position;
	D3DXVECTOR3 m_Tangent;
	D3DXVECTOR3 m_Bitangent;
	D3DXVECTOR2 m_Texcoord;
};
struct PlaneVertex
{
	D3DXVECTOR3 m_Position;
	D3DXVECTOR2 m_Texcoord;
};
struct TESSELLATION_MODEL_STRUCT
{
    WCHAR* modelFileName;                
    WCHAR* NormalHeightMap;             // Normal and height map (normal in .xyz, height in .w)
	bool   isAnimated;
    float3 diffuse;               
};
typedef enum _ADAPTIVETESSELLATION_METRIC
{
    MESH_TYPE_DISTANCE      = 0,
    MESH_TYPE_GEOMTRY       = 1,
    MESH_TYPE_IMAGESPACE    = 2,
}ADAPTIVETESSELLATION_METRIC;
