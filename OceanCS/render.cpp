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

#include "DXUT.h"
#include "DXUTCamera.h"
#include "SDKmisc.h"

#include "ocean_simulator.h"

#pragma warning(disable:4995)
#include <vector>
using namespace std;

#define FRESNEL_TEX_SIZE			256
#define PERLIN_TEX_SIZE				64

struct ocean_vertex
{
	float index_x;
	float index_y;
};

// Mesh properties:

// Mesh grid dimension, must be 2^n. 4x4 ~ 256x256
int g_MeshDim = 128;
// Side length of square shaped mesh patch
float g_PatchLength;
// Dimension of displacement map
int g_DisplaceMapDim;
// Subdivision thredshold. Any quad covers more pixels than this value needs to be subdivided.
float g_UpperGridCoverage = 64.0f;
// Draw distance = g_PatchLength * 2^g_FurthestCover
int g_FurthestCover = 8;


// Shading properties:
// Two colors for waterbody and sky color
D3DXVECTOR3 g_SkyColor= D3DXVECTOR3(0.38f, 0.45f, 0.56f);
D3DXVECTOR3 g_WaterbodyColor = D3DXVECTOR3(0.07f, 0.15f, 0.2f);
// Blending term for sky cubemap
float g_SkyBlending = 16.0f;

// Perlin wave parameters
float g_PerlinSize = 1.0f;
float g_PerlinSpeed = 0.06f;
D3DXVECTOR3 g_PerlinAmplitude = D3DXVECTOR3(35, 42, 57);
D3DXVECTOR3 g_PerlinGradient = D3DXVECTOR3(1.4f, 1.6f, 2.2f);
D3DXVECTOR3 g_PerlinOctave = D3DXVECTOR3(1.12f, 0.59f, 0.23f);
D3DXVECTOR2 g_WindDir;

D3DXVECTOR3 g_BendParam = D3DXVECTOR3(0.1f, -0.4f, 0.2f);

// Sunspot parameters
D3DXVECTOR3 g_SunDir = D3DXVECTOR3(0.936016f, -0.343206f, 0.0780013f);
D3DXVECTOR3 g_SunColor = D3DXVECTOR3(1.0f, 1.0f, 0.6f);
float g_Shineness = 400.0f;


// Quadtree structures & routines
struct QuadNode
{
	D3DXVECTOR2 bottom_left;
	float length;
	int lod;

	int sub_node[4];
};

struct QuadRenderParam
{
	UINT num_inner_verts;
	UINT num_inner_faces;
	UINT inner_start_index;

	UINT num_boundary_verts;
	UINT num_boundary_faces;
	UINT boundary_start_index;
};

// Quad-tree LOD, 0 to 9 (1x1 ~ 512x512) 
int g_Lods = 0;
// Pattern lookup array. Filled at init time.
QuadRenderParam g_mesh_patterns[9][3][3][3][3];
// Pick a proper mesh pattern according to the adjacent patches.
QuadRenderParam& selectMeshPattern(const QuadNode& quad_node);

// Rendering list
vector<QuadNode> g_render_list;
int buildNodeList(QuadNode& quad_node, const CBaseCamera& camera);

// D3D11 buffers and layout
ID3D11Buffer* g_pMeshVB = NULL;
ID3D11Buffer* g_pMeshIB = NULL;
ID3D11InputLayout* g_pMeshLayout = NULL;

// Color look up 1D texture
ID3D11Texture1D* g_pFresnelMap = NULL;
ID3D11ShaderResourceView* g_pSRV_Fresnel = NULL;

// Distant perlin wave
ID3D11ShaderResourceView* g_pSRV_Perlin = NULL;

// Environment maps
ID3D11ShaderResourceView* g_pSRV_ReflectCube = NULL;

// HLSL shaders
ID3D11VertexShader* g_pOceanSurfVS = NULL;
ID3D11PixelShader* g_pOceanSurfPS = NULL;
ID3D11PixelShader* g_pWireframePS = NULL;

// Samplers
ID3D11SamplerState* g_pHeightSampler = NULL;
ID3D11SamplerState* g_pGradientSampler = NULL;
ID3D11SamplerState* g_pFresnelSampler = NULL;
ID3D11SamplerState* g_pPerlinSampler = NULL;
ID3D11SamplerState* g_pCubeSampler = NULL;

// Constant buffer
struct Const_Per_Call
{
	D3DXMATRIX	g_matLocal;
	D3DXMATRIX	g_matWorldViewProj;
	D3DXVECTOR2 g_UVBase;
	D3DXVECTOR2 g_PerlinMovement;
	D3DXVECTOR3	g_LocalEye;
};

struct Const_Shading
{
	// Water-reflected sky color
	D3DXVECTOR3		g_SkyColor;
	float			unused0;
	// The color of bottomless water body
	D3DXVECTOR3		g_WaterbodyColor;

	// The strength, direction and color of sun streak
	float			g_Shineness;
	D3DXVECTOR3		g_SunDir;
	float			unused1;
	D3DXVECTOR3		g_SunColor;
	float			unused2;
	
	// The parameter is used for fixing an artifact
	D3DXVECTOR3		g_BendParam;

	// Perlin noise for distant wave crest
	float			g_PerlinSize;
	D3DXVECTOR3		g_PerlinAmplitude;
	float			unused3;
	D3DXVECTOR3		g_PerlinOctave;
	float			unused4;
	D3DXVECTOR3		g_PerlinGradient;

	// Constants for calculating texcoord from position
	float			g_TexelLength_x2;
	float			g_UVScale;
	float			g_UVOffset;
};

ID3D11Buffer* g_pPerCallCB = NULL;
ID3D11Buffer* g_pPerFrameCB = NULL;
ID3D11Buffer* g_pShadingCB = NULL;

// State blocks
ID3D11RasterizerState* g_pRSState_Solid = NULL;
ID3D11RasterizerState* g_pRSState_Wireframe = NULL;
ID3D11DepthStencilState* g_pDSState_Disable = NULL;
ID3D11BlendState* g_pBState_Transparent = NULL;

// init & cleanup
void initRenderResource(const OceanParameter& ocean_param, ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
void cleanupRenderResource();
// create a triangle strip mesh for ocean surface.
void createSurfaceMesh(ID3D11Device* pd3dDevice);
// create color/fresnel lookup table.
void createFresnelMap(ID3D11Device* pd3dDevice);
// create perlin noise texture for far-sight rendering
void loadTextures(ID3D11Device* pd3dDevice);
// Rendering routines
void renderShaded(const CBaseCamera& camera, ID3D11ShaderResourceView* displacemnet_map, ID3D11ShaderResourceView* gradient_map, ID3D11DeviceContext* pd3dContext);
void renderWireframe(const CBaseCamera& camera, ID3D11ShaderResourceView* displacemnet_map, ID3D11DeviceContext* pd3dContext);
// Shader compilation
HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut);


void initRenderResource(const OceanParameter& ocean_param, ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
	g_PatchLength = ocean_param.patch_length;
	g_DisplaceMapDim = ocean_param.dmap_dim;
	g_WindDir = ocean_param.wind_dir;

	// D3D buffers
	createSurfaceMesh(pd3dDevice);
	createFresnelMap(pd3dDevice);
	loadTextures(pd3dDevice);

	// HLSL
	// Vertex & pixel shaders
	ID3DBlob* pBlobOceanSurfVS = NULL;
	ID3DBlob* pBlobOceanSurfPS = NULL;
	ID3DBlob* pBlobWireframePS = NULL;

	CompileShaderFromFile(L"ocean_shading.hlsl", "OceanSurfVS", "vs_4_0", &pBlobOceanSurfVS);
	CompileShaderFromFile(L"ocean_shading.hlsl", "OceanSurfPS", "ps_4_0", &pBlobOceanSurfPS);
	CompileShaderFromFile(L"ocean_shading.hlsl", "WireframePS", "ps_4_0", &pBlobWireframePS);
	assert(pBlobOceanSurfVS);
	assert(pBlobOceanSurfPS);
	assert(pBlobWireframePS);

	pd3dDevice->CreateVertexShader(pBlobOceanSurfVS->GetBufferPointer(), pBlobOceanSurfVS->GetBufferSize(), NULL, &g_pOceanSurfVS);
	pd3dDevice->CreatePixelShader(pBlobOceanSurfPS->GetBufferPointer(), pBlobOceanSurfPS->GetBufferSize(), NULL, &g_pOceanSurfPS);
	pd3dDevice->CreatePixelShader(pBlobWireframePS->GetBufferPointer(), pBlobWireframePS->GetBufferSize(), NULL, &g_pWireframePS);
	assert(g_pOceanSurfVS);
	assert(g_pOceanSurfPS);
	assert(g_pWireframePS);
	SAFE_RELEASE(pBlobOceanSurfPS);
	SAFE_RELEASE(pBlobWireframePS);

	// Input layout
	D3D11_INPUT_ELEMENT_DESC mesh_layout_desc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	pd3dDevice->CreateInputLayout(mesh_layout_desc, 1, pBlobOceanSurfVS->GetBufferPointer(), pBlobOceanSurfVS->GetBufferSize(), &g_pMeshLayout);
	assert(g_pMeshLayout);

	SAFE_RELEASE(pBlobOceanSurfVS);

	// Constants
	D3D11_BUFFER_DESC cb_desc;
	cb_desc.Usage = D3D11_USAGE_DYNAMIC;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cb_desc.MiscFlags = 0;    
	cb_desc.ByteWidth = PAD16(sizeof(Const_Per_Call));
	cb_desc.StructureByteStride = 0;
	pd3dDevice->CreateBuffer(&cb_desc, NULL, &g_pPerCallCB);
	assert(g_pPerCallCB);

	Const_Shading shading_data;
	// Grid side length * 2
	shading_data.g_TexelLength_x2 = g_PatchLength / g_DisplaceMapDim * 2;;
	// Color
	shading_data.g_SkyColor = g_SkyColor;
	shading_data.g_WaterbodyColor = g_WaterbodyColor;
	// Texcoord
	shading_data.g_UVScale = 1.0f / g_PatchLength;
	shading_data.g_UVOffset = 0.5f / g_DisplaceMapDim;
	// Perlin
	shading_data.g_PerlinSize = g_PerlinSize;
	shading_data.g_PerlinAmplitude = g_PerlinAmplitude;
	shading_data.g_PerlinGradient = g_PerlinGradient;
	shading_data.g_PerlinOctave = g_PerlinOctave;
	// Multiple reflection workaround
	shading_data.g_BendParam = g_BendParam;
	// Sun streaks
	shading_data.g_SunColor = g_SunColor;
	shading_data.g_SunDir = g_SunDir;
	shading_data.g_Shineness = g_Shineness;

	D3D11_SUBRESOURCE_DATA cb_init_data;
	cb_init_data.pSysMem = &shading_data;
	cb_init_data.SysMemPitch = 0;
	cb_init_data.SysMemSlicePitch = 0;

	cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	cb_desc.CPUAccessFlags = 0;
	cb_desc.ByteWidth = PAD16(sizeof(Const_Shading));
	cb_desc.StructureByteStride = 0;
	pd3dDevice->CreateBuffer(&cb_desc, &cb_init_data, &g_pShadingCB);
	assert(g_pShadingCB);

	// Samplers
	D3D11_SAMPLER_DESC sam_desc;
	sam_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sam_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sam_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sam_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sam_desc.MipLODBias = 0;
	sam_desc.MaxAnisotropy = 1;
	sam_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sam_desc.BorderColor[0] = 1.0f;
	sam_desc.BorderColor[1] = 1.0f;
	sam_desc.BorderColor[2] = 1.0f;
	sam_desc.BorderColor[3] = 1.0f;
	sam_desc.MinLOD = 0;
	sam_desc.MaxLOD = FLT_MAX;
	pd3dDevice->CreateSamplerState(&sam_desc, &g_pHeightSampler);
	assert(g_pHeightSampler);

	sam_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	pd3dDevice->CreateSamplerState(&sam_desc, &g_pCubeSampler);
	assert(g_pCubeSampler);

	sam_desc.Filter = D3D11_FILTER_ANISOTROPIC;
	sam_desc.MaxAnisotropy = 8;
	pd3dDevice->CreateSamplerState(&sam_desc, &g_pGradientSampler);
	assert(g_pGradientSampler);

	sam_desc.MaxLOD = FLT_MAX;
	sam_desc.MaxAnisotropy = 4;
	pd3dDevice->CreateSamplerState(&sam_desc, &g_pPerlinSampler);
	assert(g_pPerlinSampler);

	sam_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sam_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sam_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sam_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	pd3dDevice->CreateSamplerState(&sam_desc, &g_pFresnelSampler);
	assert(g_pFresnelSampler);

	// State blocks
	D3D11_RASTERIZER_DESC ras_desc;
	ras_desc.FillMode = D3D11_FILL_SOLID; 
	ras_desc.CullMode = D3D11_CULL_NONE; 
	ras_desc.FrontCounterClockwise = FALSE; 
	ras_desc.DepthBias = 0;
	ras_desc.SlopeScaledDepthBias = 0.0f;
	ras_desc.DepthBiasClamp = 0.0f;
	ras_desc.DepthClipEnable= TRUE;
	ras_desc.ScissorEnable = FALSE;
	ras_desc.MultisampleEnable = TRUE;
	ras_desc.AntialiasedLineEnable = FALSE;

	pd3dDevice->CreateRasterizerState(&ras_desc, &g_pRSState_Solid);
	assert(g_pRSState_Solid);

	ras_desc.FillMode = D3D11_FILL_WIREFRAME; 

	pd3dDevice->CreateRasterizerState(&ras_desc, &g_pRSState_Wireframe);
	assert(g_pRSState_Wireframe);

	D3D11_DEPTH_STENCIL_DESC depth_desc;
	memset(&depth_desc, 0, sizeof(D3D11_DEPTH_STENCIL_DESC));
	depth_desc.DepthEnable = FALSE;
	depth_desc.StencilEnable = FALSE;
	pd3dDevice->CreateDepthStencilState(&depth_desc, &g_pDSState_Disable);
	assert(g_pDSState_Disable);

	D3D11_BLEND_DESC blend_desc;
	memset(&blend_desc, 0, sizeof(D3D11_BLEND_DESC));
	blend_desc.AlphaToCoverageEnable = FALSE;
	blend_desc.IndependentBlendEnable = FALSE;
	blend_desc.RenderTarget[0].BlendEnable = TRUE;
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	pd3dDevice->CreateBlendState(&blend_desc, &g_pBState_Transparent);
	assert(g_pBState_Transparent);
}

void cleanupRenderResource()
{
	SAFE_RELEASE(g_pMeshIB);
	SAFE_RELEASE(g_pMeshVB);
	SAFE_RELEASE(g_pMeshLayout);

	SAFE_RELEASE(g_pOceanSurfVS);
	SAFE_RELEASE(g_pOceanSurfPS);
	SAFE_RELEASE(g_pWireframePS);

	SAFE_RELEASE(g_pFresnelMap);
	SAFE_RELEASE(g_pSRV_Fresnel);
	SAFE_RELEASE(g_pSRV_Perlin);
	SAFE_RELEASE(g_pSRV_ReflectCube);

	SAFE_RELEASE(g_pHeightSampler);
	SAFE_RELEASE(g_pGradientSampler);
	SAFE_RELEASE(g_pFresnelSampler);
	SAFE_RELEASE(g_pPerlinSampler);
	SAFE_RELEASE(g_pCubeSampler);

	SAFE_RELEASE(g_pPerCallCB);
	SAFE_RELEASE(g_pPerFrameCB);
	SAFE_RELEASE(g_pShadingCB);

	SAFE_RELEASE(g_pRSState_Solid);
	SAFE_RELEASE(g_pRSState_Wireframe);
	SAFE_RELEASE(g_pDSState_Disable);
	SAFE_RELEASE(g_pBState_Transparent);

	g_render_list.clear();
}

#define MESH_INDEX_2D(x, y)	(((y) + vert_rect.bottom) * (g_MeshDim + 1) + (x) + vert_rect.left)

// Generate boundary mesh for a patch. Return the number of generated indices
int generateBoundaryMesh(int left_degree, int right_degree, int bottom_degree, int top_degree,
						 RECT vert_rect, DWORD* output)
{
	// Triangle list for bottom boundary
	int i, j;
	int counter = 0;
	int width = vert_rect.right - vert_rect.left;

	if (bottom_degree > 0)
	{
		int b_step = width / bottom_degree;

		for (i = 0; i < width; i += b_step)
		{
			output[counter++] = MESH_INDEX_2D(i, 0);
			output[counter++] = MESH_INDEX_2D(i + b_step / 2, 1);
			output[counter++] = MESH_INDEX_2D(i + b_step, 0);

			for (j = 0; j < b_step / 2; j ++)
			{
				if (i == 0 && j == 0 && left_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(i, 0);
				output[counter++] = MESH_INDEX_2D(i + j, 1);
				output[counter++] = MESH_INDEX_2D(i + j + 1, 1);
			}

			for (j = b_step / 2; j < b_step; j ++)
			{
				if (i == width - b_step && j == b_step - 1 && right_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(i + b_step, 0);
				output[counter++] = MESH_INDEX_2D(i + j, 1);
				output[counter++] = MESH_INDEX_2D(i + j + 1, 1);
			}
		}
	}

	// Right boundary
	int height = vert_rect.top - vert_rect.bottom;

	if (right_degree > 0)
	{
		int r_step = height / right_degree;

		for (i = 0; i < height; i += r_step)
		{
			output[counter++] = MESH_INDEX_2D(width, i);
			output[counter++] = MESH_INDEX_2D(width - 1, i + r_step / 2);
			output[counter++] = MESH_INDEX_2D(width, i + r_step);

			for (j = 0; j < r_step / 2; j ++)
			{
				if (i == 0 && j == 0 && bottom_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(width, i);
				output[counter++] = MESH_INDEX_2D(width - 1, i + j);
				output[counter++] = MESH_INDEX_2D(width - 1, i + j + 1);
			}

			for (j = r_step / 2; j < r_step; j ++)
			{
				if (i == height - r_step && j == r_step - 1 && top_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(width, i + r_step);
				output[counter++] = MESH_INDEX_2D(width - 1, i + j);
				output[counter++] = MESH_INDEX_2D(width - 1, i + j + 1);
			}
		}
	}

	// Top boundary
	if (top_degree > 0)
	{
		int t_step = width / top_degree;

		for (i = 0; i < width; i += t_step)
		{
			output[counter++] = MESH_INDEX_2D(i, height);
			output[counter++] = MESH_INDEX_2D(i + t_step / 2, height - 1);
			output[counter++] = MESH_INDEX_2D(i + t_step, height);

			for (j = 0; j < t_step / 2; j ++)
			{
				if (i == 0 && j == 0 && left_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(i, height);
				output[counter++] = MESH_INDEX_2D(i + j, height - 1);
				output[counter++] = MESH_INDEX_2D(i + j + 1, height - 1);
			}

			for (j = t_step / 2; j < t_step; j ++)
			{
				if (i == width - t_step && j == t_step - 1 && right_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(i + t_step, height);
				output[counter++] = MESH_INDEX_2D(i + j, height - 1);
				output[counter++] = MESH_INDEX_2D(i + j + 1, height - 1);
			}
		}
	}

	// Left boundary
	if (left_degree > 0)
	{
		int l_step = height / left_degree;

		for (i = 0; i < height; i += l_step)
		{
			output[counter++] = MESH_INDEX_2D(0, i);
			output[counter++] = MESH_INDEX_2D(1, i + l_step / 2);
			output[counter++] = MESH_INDEX_2D(0, i + l_step);

			for (j = 0; j < l_step / 2; j ++)
			{
				if (i == 0 && j == 0 && bottom_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(0, i);
				output[counter++] = MESH_INDEX_2D(1, i + j);
				output[counter++] = MESH_INDEX_2D(1, i + j + 1);
			}

			for (j = l_step / 2; j < l_step; j ++)
			{
				if (i == height - l_step && j == l_step - 1 && top_degree > 0)
					continue;

				output[counter++] = MESH_INDEX_2D(0, i + l_step);
				output[counter++] = MESH_INDEX_2D(1, i + j);
				output[counter++] = MESH_INDEX_2D(1, i + j + 1);
			}
		}
	}

	return counter;
}

// Generate boundary mesh for a patch. Return the number of generated indices
int generateInnerMesh(RECT vert_rect, DWORD* output)
{
	int i, j;
	int counter = 0;
	int width = vert_rect.right - vert_rect.left;
	int height = vert_rect.top - vert_rect.bottom;

	bool reverse = false;
	for (i = 0; i < height; i++)
	{
		if (reverse == false)
		{
			output[counter++] = MESH_INDEX_2D(0, i);
			output[counter++] = MESH_INDEX_2D(0, i + 1);
			for (j = 0; j < width; j++)
			{
				output[counter++] = MESH_INDEX_2D(j + 1, i);
				output[counter++] = MESH_INDEX_2D(j + 1, i + 1);
			}
		}
		else
		{
			output[counter++] = MESH_INDEX_2D(width, i);
			output[counter++] = MESH_INDEX_2D(width, i + 1);
			for (j = width - 1; j >= 0; j--)
			{
				output[counter++] = MESH_INDEX_2D(j, i);
				output[counter++] = MESH_INDEX_2D(j, i + 1);
			}
		}

		reverse = !reverse;
	}

	return counter;
}

void createSurfaceMesh(ID3D11Device* pd3dDevice)
{
	// --------------------------------- Vertex Buffer -------------------------------
	int num_verts = (g_MeshDim + 1) * (g_MeshDim + 1);
	ocean_vertex* pV = new ocean_vertex[num_verts];
	assert(pV);

	int i, j;
	for (i = 0; i <= g_MeshDim; i++)
	{
		for (j = 0; j <= g_MeshDim; j++)
		{
			pV[i * (g_MeshDim + 1) + j].index_x = (float)j;
			pV[i * (g_MeshDim + 1) + j].index_y = (float)i;
		}
	}

	D3D11_BUFFER_DESC vb_desc;
	vb_desc.ByteWidth = num_verts * sizeof(ocean_vertex);
	vb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vb_desc.CPUAccessFlags = 0;
	vb_desc.MiscFlags = 0;
	vb_desc.StructureByteStride = sizeof(ocean_vertex);

	D3D11_SUBRESOURCE_DATA init_data;
	init_data.pSysMem = pV;
	init_data.SysMemPitch = 0;
	init_data.SysMemSlicePitch = 0;
	
	SAFE_RELEASE(g_pMeshVB);
	pd3dDevice->CreateBuffer(&vb_desc, &init_data, &g_pMeshVB);
	assert(g_pMeshVB);

	SAFE_DELETE_ARRAY(pV);


	// --------------------------------- Index Buffer -------------------------------
	// The index numbers for all mesh LODs (up to 256x256)
	const int index_size_lookup[] = {0, 0, 4284, 18828, 69444, 254412, 956916, 3689820, 14464836};

	memset(&g_mesh_patterns[0][0][0][0][0], 0, sizeof(g_mesh_patterns));

	g_Lods = 0;
	for (i = g_MeshDim; i > 1; i >>= 1)
		g_Lods ++;

	// Generate patch meshes. Each patch contains two parts: the inner mesh which is a regular
	// grids in a triangle strip. The boundary mesh is constructed w.r.t. the edge degrees to
	// meet water-tight requirement.
	DWORD* index_array = new DWORD[index_size_lookup[g_Lods]];
	assert(index_array);

	int offset = 0;
	int level_size = g_MeshDim;

	// Enumerate patterns
	for (int level = 0; level <= g_Lods - 2; level ++)
	{
		int left_degree = level_size;

		for (int left_type = 0; left_type < 3; left_type ++)
		{
			int right_degree = level_size;

			for (int right_type = 0; right_type < 3; right_type ++)
			{
				int bottom_degree = level_size;

				for (int bottom_type = 0; bottom_type < 3; bottom_type ++)
				{
					int top_degree = level_size;

					for (int top_type = 0; top_type < 3; top_type ++)
					{
						QuadRenderParam* pattern = &g_mesh_patterns[level][left_type][right_type][bottom_type][top_type];

						// Inner mesh (triangle strip)
						RECT inner_rect;
						inner_rect.left   = (left_degree   == level_size) ? 0 : 1;
						inner_rect.right  = (right_degree  == level_size) ? level_size : level_size - 1;
						inner_rect.bottom = (bottom_degree == level_size) ? 0 : 1;
						inner_rect.top    = (top_degree    == level_size) ? level_size : level_size - 1;

						int num_new_indices = generateInnerMesh(inner_rect, index_array + offset);

						pattern->inner_start_index = offset;
						pattern->num_inner_verts = (level_size + 1) * (level_size + 1);
						pattern->num_inner_faces = num_new_indices - 2;
						offset += num_new_indices;

						// Boundary mesh (triangle list)
						int l_degree = (left_degree   == level_size) ? 0 : left_degree;
						int r_degree = (right_degree  == level_size) ? 0 : right_degree;
						int b_degree = (bottom_degree == level_size) ? 0 : bottom_degree;
						int t_degree = (top_degree    == level_size) ? 0 : top_degree;

						RECT outer_rect = {0, level_size, level_size, 0};
						num_new_indices = generateBoundaryMesh(l_degree, r_degree, b_degree, t_degree, outer_rect, index_array + offset);

						pattern->boundary_start_index = offset;
						pattern->num_boundary_verts = (level_size + 1) * (level_size + 1);
						pattern->num_boundary_faces = num_new_indices / 3;
						offset += num_new_indices;

						top_degree /= 2;
					}
					bottom_degree /= 2;
				}
				right_degree /= 2;
			}
			left_degree /= 2;
		}
		level_size /= 2;
	}

	assert(offset == index_size_lookup[g_Lods]);

	D3D11_BUFFER_DESC ib_desc;
	ib_desc.ByteWidth = index_size_lookup[g_Lods] * sizeof(DWORD);
	ib_desc.Usage = D3D11_USAGE_IMMUTABLE;
	ib_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ib_desc.CPUAccessFlags = 0;
	ib_desc.MiscFlags = 0;
	ib_desc.StructureByteStride = sizeof(DWORD);

	init_data.pSysMem = index_array;

	SAFE_RELEASE(g_pMeshIB);
	pd3dDevice->CreateBuffer(&ib_desc, &init_data, &g_pMeshIB);
	assert(g_pMeshIB);

	SAFE_DELETE_ARRAY(index_array);
}

void createFresnelMap(ID3D11Device* pd3dDevice)
{
	DWORD* buffer = new DWORD[FRESNEL_TEX_SIZE];
	for (int i = 0; i < FRESNEL_TEX_SIZE; i++)
	{
		float cos_a = i / (FLOAT)FRESNEL_TEX_SIZE;
		// Using water's refraction index 1.33
		DWORD fresnel = (DWORD)(D3DXFresnelTerm(cos_a, 1.33f) * 255);

		DWORD sky_blend = (DWORD)(powf(1 / (1 + cos_a), g_SkyBlending) * 255);

		buffer[i] = (sky_blend << 8) | fresnel;
	}

	D3D11_TEXTURE1D_DESC tex_desc;
	tex_desc.Width = FRESNEL_TEX_SIZE;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = 1;
	tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
	tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	tex_desc.CPUAccessFlags = 0;
	tex_desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA init_data;
	init_data.pSysMem = buffer;
	init_data.SysMemPitch = 0;
	init_data.SysMemSlicePitch = 0;

	pd3dDevice->CreateTexture1D(&tex_desc, &init_data, &g_pFresnelMap);
	assert(g_pFresnelMap);

	SAFE_DELETE_ARRAY(buffer);

	// Create shader resource
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
	srv_desc.Texture1D.MipLevels = 1;
	srv_desc.Texture1D.MostDetailedMip = 0;

	pd3dDevice->CreateShaderResourceView(g_pFresnelMap, &srv_desc, &g_pSRV_Fresnel);
	assert(g_pSRV_Fresnel);
}

void loadTextures(ID3D11Device* pd3dDevice)
{
    WCHAR strPath[MAX_PATH];
    DXUTFindDXSDKMediaFileCch(strPath, MAX_PATH, L"Media\\OceanCS\\perlin_noise.dds");
	D3DX11CreateShaderResourceViewFromFile(pd3dDevice, strPath, NULL, NULL, &g_pSRV_Perlin, NULL);
	assert(g_pSRV_Perlin);

    DXUTFindDXSDKMediaFileCch(strPath, MAX_PATH, L"Media\\OceanCS\\reflect_cube.dds");
	D3DX11CreateShaderResourceViewFromFile(pd3dDevice, strPath, NULL, NULL, &g_pSRV_ReflectCube, NULL);
	assert(g_pSRV_ReflectCube);
}

bool checkNodeVisibility(const QuadNode& quad_node, const CBaseCamera& camera)
{
	// Plane equation setup

	D3DXMATRIX matProj = *camera.GetProjMatrix();

	// Left plane
	float fov_x = atan(1.0f / matProj(0, 0));
	D3DXVECTOR4 plane_left(cos(fov_x), 0, sin(fov_x), 0);
	// Right plane
	D3DXVECTOR4 plane_right(-cos(fov_x), 0, sin(fov_x), 0);

	// Bottom plane
	float fov_y = atan(1.0f / matProj(1, 1));
	D3DXVECTOR4 plane_bottom(0, cos(fov_y), sin(fov_y), 0);
	// Top plane
	D3DXVECTOR4 plane_top(0, -cos(fov_y), sin(fov_y), 0);

	// Test quad corners against view frustum in view space
	D3DXVECTOR4 corner_verts[4];
	corner_verts[0] = D3DXVECTOR4(quad_node.bottom_left.x, quad_node.bottom_left.y, 0, 1);
	corner_verts[1] = corner_verts[0] + D3DXVECTOR4(quad_node.length, 0, 0, 0);
	corner_verts[2] = corner_verts[0] + D3DXVECTOR4(quad_node.length, quad_node.length, 0, 0);
	corner_verts[3] = corner_verts[0] + D3DXVECTOR4(0, quad_node.length, 0, 0);

	D3DXMATRIX matView = D3DXMATRIX(1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,1) * *camera.GetViewMatrix();
	D3DXVec4Transform(&corner_verts[0], &corner_verts[0], &matView);
	D3DXVec4Transform(&corner_verts[1], &corner_verts[1], &matView);
	D3DXVec4Transform(&corner_verts[2], &corner_verts[2], &matView);
	D3DXVec4Transform(&corner_verts[3], &corner_verts[3], &matView);

	// Test against eye plane
	if (corner_verts[0].z < 0 && corner_verts[1].z < 0 && corner_verts[2].z < 0 && corner_verts[3].z < 0)
		return false;

	// Test against left plane
	float dist_0 = D3DXVec4Dot(&corner_verts[0], &plane_left);
	float dist_1 = D3DXVec4Dot(&corner_verts[1], &plane_left);
	float dist_2 = D3DXVec4Dot(&corner_verts[2], &plane_left);
	float dist_3 = D3DXVec4Dot(&corner_verts[3], &plane_left);
	if (dist_0 < 0 && dist_1 < 0 && dist_2 < 0 && dist_3 < 0)
		return false;

	// Test against right plane
	dist_0 = D3DXVec4Dot(&corner_verts[0], &plane_right);
	dist_1 = D3DXVec4Dot(&corner_verts[1], &plane_right);
	dist_2 = D3DXVec4Dot(&corner_verts[2], &plane_right);
	dist_3 = D3DXVec4Dot(&corner_verts[3], &plane_right);
	if (dist_0 < 0 && dist_1 < 0 && dist_2 < 0 && dist_3 < 0)
		return false;

	// Test against bottom plane
	dist_0 = D3DXVec4Dot(&corner_verts[0], &plane_bottom);
	dist_1 = D3DXVec4Dot(&corner_verts[1], &plane_bottom);
	dist_2 = D3DXVec4Dot(&corner_verts[2], &plane_bottom);
	dist_3 = D3DXVec4Dot(&corner_verts[3], &plane_bottom);
	if (dist_0 < 0 && dist_1 < 0 && dist_2 < 0 && dist_3 < 0)
		return false;

	// Test against top plane
	dist_0 = D3DXVec4Dot(&corner_verts[0], &plane_top);
	dist_1 = D3DXVec4Dot(&corner_verts[1], &plane_top);
	dist_2 = D3DXVec4Dot(&corner_verts[2], &plane_top);
	dist_3 = D3DXVec4Dot(&corner_verts[3], &plane_top);
	if (dist_0 < 0 && dist_1 < 0 && dist_2 < 0 && dist_3 < 0)
		return false;

	return true;
}

float estimateGridCoverage(const QuadNode& quad_node, const CBaseCamera& camera, float screen_area)
{
	// Estimate projected area

	// Test 16 points on the quad and find out the biggest one.
	const static float sample_pos[16][2] =
	{
		{0, 0},
		{0, 1},
		{1, 0},
		{1, 1},
		{0.5f, 0.333f},
		{0.25f, 0.667f},
		{0.75f, 0.111f},
		{0.125f, 0.444f},
		{0.625f, 0.778f},
		{0.375f, 0.222f},
		{0.875f, 0.556f},
		{0.0625f, 0.889f},
		{0.5625f, 0.037f},
		{0.3125f, 0.37f},
		{0.8125f, 0.704f},
		{0.1875f, 0.148f},
	};

	D3DXMATRIX matProj = *camera.GetProjMatrix();
	D3DXVECTOR3 eye_point = *camera.GetEyePt();
	eye_point = D3DXVECTOR3(eye_point.x, eye_point.z, eye_point.y);
	float grid_len_world = quad_node.length / g_MeshDim;

	float max_area_proj = 0;
	for (int i = 0; i < 16; i++)
	{
		D3DXVECTOR3 test_point(quad_node.bottom_left.x + quad_node.length * sample_pos[i][0], quad_node.bottom_left.y + quad_node.length * sample_pos[i][1], 0);
		D3DXVECTOR3 eye_vec = test_point - eye_point;
		float dist = D3DXVec3Length(&eye_vec);

		float area_world = grid_len_world * grid_len_world;// * abs(eye_point.z) / sqrt(nearest_sqr_dist);
		float area_proj = area_world * matProj(0, 0) * matProj(1, 1) / (dist * dist);

		if (max_area_proj < area_proj)
			max_area_proj = area_proj;
	}

	float pixel_coverage = max_area_proj * screen_area * 0.25f;

	return pixel_coverage;
}

bool isLeaf(const QuadNode& quad_node)
{
	return (quad_node.sub_node[0] == -1 && quad_node.sub_node[1] == -1 && quad_node.sub_node[2] == -1 && quad_node.sub_node[3] == -1);
}

int searchLeaf(const vector<QuadNode>& node_list, const D3DXVECTOR2& point)
{
	int index = -1;
	
	int size = (int)node_list.size();
	QuadNode node = node_list[size - 1];

	while (!isLeaf(node))
	{
		bool found = false;

		for (int i = 0; i < 4; i++)
		{
			index = node.sub_node[i];
			if (index == -1)
				continue;

			QuadNode sub_node = node_list[index];
			if (point.x >= sub_node.bottom_left.x && point.x <= sub_node.bottom_left.x + sub_node.length &&
				point.y >= sub_node.bottom_left.y && point.y <= sub_node.bottom_left.y + sub_node.length)
			{
				node = sub_node;
				found = true;
				break;
			}
		}

		if (!found)
			return -1;
	}

	return index;
}

QuadRenderParam& selectMeshPattern(const QuadNode& quad_node)
{
	// Check 4 adjacent quad.
	D3DXVECTOR2 point_left = quad_node.bottom_left + D3DXVECTOR2(-g_PatchLength * 0.5f, quad_node.length * 0.5f);
	int left_adj_index = searchLeaf(g_render_list, point_left);

	D3DXVECTOR2 point_right = quad_node.bottom_left + D3DXVECTOR2(quad_node.length + g_PatchLength * 0.5f, quad_node.length * 0.5f);
	int right_adj_index = searchLeaf(g_render_list, point_right);

	D3DXVECTOR2 point_bottom = quad_node.bottom_left + D3DXVECTOR2(quad_node.length * 0.5f, -g_PatchLength * 0.5f);
	int bottom_adj_index = searchLeaf(g_render_list, point_bottom);

	D3DXVECTOR2 point_top = quad_node.bottom_left + D3DXVECTOR2(quad_node.length * 0.5f, quad_node.length + g_PatchLength * 0.5f);
	int top_adj_index = searchLeaf(g_render_list, point_top);

	int left_type = 0;
	if (left_adj_index != -1 && g_render_list[left_adj_index].length > quad_node.length * 0.999f)
	{
		QuadNode adj_node = g_render_list[left_adj_index];
		float scale = adj_node.length / quad_node.length * (g_MeshDim >> quad_node.lod) / (g_MeshDim >> adj_node.lod);
		if (scale > 3.999f)
			left_type = 2;
		else if (scale > 1.999f)
			left_type = 1;
	}

	int right_type = 0;
	if (right_adj_index != -1 && g_render_list[right_adj_index].length > quad_node.length * 0.999f)
	{
		QuadNode adj_node = g_render_list[right_adj_index];
		float scale = adj_node.length / quad_node.length * (g_MeshDim >> quad_node.lod) / (g_MeshDim >> adj_node.lod);
		if (scale > 3.999f)
			right_type = 2;
		else if (scale > 1.999f)
			right_type = 1;
	}

	int bottom_type = 0;
	if (bottom_adj_index != -1 && g_render_list[bottom_adj_index].length > quad_node.length * 0.999f)
	{
		QuadNode adj_node = g_render_list[bottom_adj_index];
		float scale = adj_node.length / quad_node.length * (g_MeshDim >> quad_node.lod) / (g_MeshDim >> adj_node.lod);
		if (scale > 3.999f)
			bottom_type = 2;
		else if (scale > 1.999f)
			bottom_type = 1;
	}

	int top_type = 0;
	if (top_adj_index != -1 && g_render_list[top_adj_index].length > quad_node.length * 0.999f)
	{
		QuadNode adj_node = g_render_list[top_adj_index];
		float scale = adj_node.length / quad_node.length * (g_MeshDim >> quad_node.lod) / (g_MeshDim >> adj_node.lod);
		if (scale > 3.999f)
			top_type = 2;
		else if (scale > 1.999f)
			top_type = 1;
	}

	// Check lookup table, [L][R][B][T]
	return g_mesh_patterns[quad_node.lod][left_type][right_type][bottom_type][top_type];
}

// Return value: if successful pushed into the list, return the position. If failed, return -1.
int buildNodeList(QuadNode& quad_node, const CBaseCamera& camera)
{
	// Check against view frustum
	if (!checkNodeVisibility(quad_node, camera))
		return -1;

	// Estimate the min grid coverage
	UINT num_vps = 1;
	D3D11_VIEWPORT vp;
	DXUTGetD3D11DeviceContext()->RSGetViewports(&num_vps, &vp);
	float min_coverage = estimateGridCoverage(quad_node, camera, (float)vp.Width * vp.Height);

	// Recursively attatch sub-nodes.
	bool visible = true;
	if (min_coverage > g_UpperGridCoverage && quad_node.length > g_PatchLength)
	{
		// Recursive rendering for sub-quads.
		QuadNode sub_node_0 = {quad_node.bottom_left, quad_node.length / 2, 0, {-1, -1, -1, -1}};
		quad_node.sub_node[0] = buildNodeList(sub_node_0, camera);

		QuadNode sub_node_1 = {quad_node.bottom_left + D3DXVECTOR2(quad_node.length/2, 0), quad_node.length / 2, 0, {-1, -1, -1, -1}};
		quad_node.sub_node[1] = buildNodeList(sub_node_1, camera);

		QuadNode sub_node_2 = {quad_node.bottom_left + D3DXVECTOR2(quad_node.length/2, quad_node.length/2), quad_node.length / 2, 0, {-1, -1, -1, -1}};
		quad_node.sub_node[2] = buildNodeList(sub_node_2, camera);

		QuadNode sub_node_3 = {quad_node.bottom_left + D3DXVECTOR2(0, quad_node.length/2), quad_node.length / 2, 0, {-1, -1, -1, -1}};
		quad_node.sub_node[3] = buildNodeList(sub_node_3, camera);

		visible = !isLeaf(quad_node);
	}

	if (visible)
	{
		// Estimate mesh LOD
		int lod = 0;
		for (lod = 0; lod < g_Lods - 1; lod++)
		{
			if (min_coverage > g_UpperGridCoverage)
				break;
			min_coverage *= 4;
		}

		// We don't use 1x1 and 2x2 patch. So the highest level is g_Lods - 2.
		quad_node.lod = min(lod, g_Lods - 2);
	}
	else
		return -1;

	// Insert into the list
	int position = (int)g_render_list.size();
	g_render_list.push_back(quad_node);

	return position;
}

void renderShaded(const CBaseCamera& camera, ID3D11ShaderResourceView* displacemnet_map, ID3D11ShaderResourceView* gradient_map, float time, ID3D11DeviceContext* pd3dContext)
{
	// Build rendering list
	g_render_list.clear();
	float ocean_extent = g_PatchLength * (1 << g_FurthestCover);
	QuadNode root_node = {D3DXVECTOR2(-ocean_extent * 0.5f, -ocean_extent * 0.5f), ocean_extent, 0, {-1,-1,-1,-1}};
	buildNodeList(root_node, camera);

	// Matrices
	D3DXMATRIX matView = D3DXMATRIX(1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,1) * *camera.GetViewMatrix();
	D3DXMATRIX matProj = *camera.GetProjMatrix();

	// VS & PS
	pd3dContext->VSSetShader(g_pOceanSurfVS, NULL, 0);
	pd3dContext->PSSetShader(g_pOceanSurfPS, NULL, 0);

	// Textures
	ID3D11ShaderResourceView* vs_srvs[2] = {displacemnet_map, g_pSRV_Perlin};
	pd3dContext->VSSetShaderResources(0, 2, &vs_srvs[0]);

	ID3D11ShaderResourceView* ps_srvs[4] = {g_pSRV_Perlin, gradient_map, g_pSRV_Fresnel, g_pSRV_ReflectCube};
    pd3dContext->PSSetShaderResources(1, 4, &ps_srvs[0]);

	// Samplers
	ID3D11SamplerState* vs_samplers[2] = {g_pHeightSampler, g_pPerlinSampler};
	pd3dContext->VSSetSamplers(0, 2, &vs_samplers[0]);

	ID3D11SamplerState* ps_samplers[4] = {g_pPerlinSampler, g_pGradientSampler, g_pFresnelSampler, g_pCubeSampler};
	pd3dContext->PSSetSamplers(1, 4, &ps_samplers[0]);

	// IA setup
	pd3dContext->IASetIndexBuffer(g_pMeshIB, DXGI_FORMAT_R32_UINT, 0);

	ID3D11Buffer* vbs[1] = {g_pMeshVB};
	UINT strides[1] = {sizeof(ocean_vertex)};
	UINT offsets[1] = {0};
	pd3dContext->IASetVertexBuffers(0, 1, &vbs[0], &strides[0], &offsets[0]);

	pd3dContext->IASetInputLayout(g_pMeshLayout);

	// State blocks
	pd3dContext->RSSetState(g_pRSState_Solid);

	// Constants
	ID3D11Buffer* cbs[1] = {g_pShadingCB};
	pd3dContext->VSSetConstantBuffers(2, 1, cbs);
	pd3dContext->PSSetConstantBuffers(2, 1, cbs);

	// We assume the center of the ocean surface at (0, 0, 0).
	for (int i = 0; i < (int)g_render_list.size(); i++)
	{
		QuadNode& node = g_render_list[i];
		
		if (!isLeaf(node))
			continue;

		// Check adjacent patches and select mesh pattern
		QuadRenderParam& render_param = selectMeshPattern(node);

		// Find the right LOD to render
		int level_size = g_MeshDim;
		for (int lod = 0; lod < node.lod; lod++)
			level_size >>= 1;

		// Matrices and constants
		Const_Per_Call call_consts;

		// Expand of the local coordinate to world space patch size
		D3DXMATRIX matScale;
		D3DXMatrixScaling(&matScale, node.length / level_size, node.length / level_size, 0);
		D3DXMatrixTranspose(&call_consts.g_matLocal, &matScale);

		// WVP matrix
		D3DXMATRIX matWorld;
		D3DXMatrixTranslation(&matWorld, node.bottom_left.x, node.bottom_left.y, 0);
		D3DXMATRIX matWVP = matWorld * matView * matProj;
		D3DXMatrixTranspose(&call_consts.g_matWorldViewProj, &matWVP);

		// Texcoord for perlin noise
		D3DXVECTOR2 uv_base = node.bottom_left / g_PatchLength * g_PerlinSize;
		call_consts.g_UVBase = uv_base;

		// Constant g_PerlinSpeed need to be adjusted mannually
		D3DXVECTOR2 perlin_move = -g_WindDir * time * g_PerlinSpeed;
		call_consts.g_PerlinMovement = perlin_move;

		// Eye point
		D3DXMATRIX matInvWV = matWorld * matView;
		D3DXMatrixInverse(&matInvWV, NULL, &matInvWV);
		D3DXVECTOR3 vLocalEye(0, 0, 0);
		D3DXVec3TransformCoord(&vLocalEye, &vLocalEye, &matInvWV);
		call_consts.g_LocalEye = vLocalEye;

		// Update constant buffer
		D3D11_MAPPED_SUBRESOURCE mapped_res;            
		pd3dContext->Map(g_pPerCallCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
		assert(mapped_res.pData);
		*(Const_Per_Call*)mapped_res.pData = call_consts;
		pd3dContext->Unmap(g_pPerCallCB, 0);

		cbs[0] = g_pPerCallCB;
		pd3dContext->VSSetConstantBuffers(4, 1, cbs);
		pd3dContext->PSSetConstantBuffers(4, 1, cbs);

		// Perform draw call
		if (render_param.num_inner_faces > 0)
		{
			// Inner mesh of the patch
			pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			pd3dContext->DrawIndexed(render_param.num_inner_faces + 2, render_param.inner_start_index, 0);
		}

		if (render_param.num_boundary_faces > 0)
		{
			// Boundary mesh of the patch
			pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pd3dContext->DrawIndexed(render_param.num_boundary_faces * 3, render_param.boundary_start_index, 0);
		}
	}

	// Unbind
	vs_srvs[0] = NULL;
	vs_srvs[1] = NULL;
	pd3dContext->VSSetShaderResources(0, 2, &vs_srvs[0]);

	ps_srvs[0] = NULL;
	ps_srvs[1] = NULL;
	ps_srvs[2] = NULL;
	ps_srvs[3] = NULL;
    pd3dContext->PSSetShaderResources(1, 4, &ps_srvs[0]);
}

void renderWireframe(const CBaseCamera& camera, ID3D11ShaderResourceView* displacemnet_map, float time, ID3D11DeviceContext* pd3dContext)
{
	// Build rendering list
	g_render_list.clear();
	float ocean_extent = g_PatchLength * (1 << g_FurthestCover);
	QuadNode root_node = {D3DXVECTOR2(-ocean_extent * 0.5f, -ocean_extent * 0.5f), ocean_extent, 0, {-1,-1,-1,-1}};
	buildNodeList(root_node, camera);

	// Matrices
	D3DXMATRIX matView = D3DXMATRIX(1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,1) * *camera.GetViewMatrix();
	D3DXMATRIX matProj = *camera.GetProjMatrix();

	// VS & PS
	pd3dContext->VSSetShader(g_pOceanSurfVS, NULL, 0);
	pd3dContext->PSSetShader(g_pWireframePS, NULL, 0);

	// Textures
	ID3D11ShaderResourceView* vs_srvs[2] = {displacemnet_map, g_pSRV_Perlin};
	pd3dContext->VSSetShaderResources(0, 2, &vs_srvs[0]);

	// Samplers
	ID3D11SamplerState* vs_samplers[2] = {g_pHeightSampler, g_pPerlinSampler};
	pd3dContext->VSSetSamplers(0, 2, &vs_samplers[0]);

	ID3D11SamplerState* ps_samplers[4] = {NULL, NULL, NULL, NULL};
	pd3dContext->PSSetSamplers(1, 4, &ps_samplers[0]);

	// IA setup
	pd3dContext->IASetIndexBuffer(g_pMeshIB, DXGI_FORMAT_R32_UINT, 0);

	ID3D11Buffer* vbs[1] = {g_pMeshVB};
	UINT strides[1] = {sizeof(ocean_vertex)};
	UINT offsets[1] = {0};
	pd3dContext->IASetVertexBuffers(0, 1, &vbs[0], &strides[0], &offsets[0]);

	pd3dContext->IASetInputLayout(g_pMeshLayout);

	// State blocks
	pd3dContext->RSSetState(g_pRSState_Wireframe);

	// Constants
	ID3D11Buffer* cbs[1] = {g_pShadingCB};
	pd3dContext->VSSetConstantBuffers(2, 1, cbs);
	pd3dContext->PSSetConstantBuffers(2, 1, cbs);

	// We assume the center of the ocean surface is at (0, 0, 0).
	for (int i = 0; i < (int)g_render_list.size(); i++)
	{
		QuadNode& node = g_render_list[i];
		
		if (!isLeaf(node))
			continue;

		// Check adjacent patches and select mesh pattern
		QuadRenderParam& render_param = selectMeshPattern(node);

		// Find the right LOD to render
		int level_size = g_MeshDim;
		for (int lod = 0; lod < node.lod; lod++)
			level_size >>= 1;

		// Matrices and constants
		Const_Per_Call call_consts;

		// Expand of the local coordinate to world space patch size
		D3DXMATRIX matScale;
		D3DXMatrixScaling(&matScale, node.length / level_size, node.length / level_size, 0);
		D3DXMatrixTranspose(&call_consts.g_matLocal, &matScale);

		// WVP matrix
		D3DXMATRIX matWorld;
		D3DXMatrixTranslation(&matWorld, node.bottom_left.x, node.bottom_left.y, 0);
		D3DXMATRIX matWVP = matWorld * matView * matProj;
		D3DXMatrixTranspose(&call_consts.g_matWorldViewProj, &matWVP);

		// Texcoord for perlin noise
		D3DXVECTOR2 uv_base = node.bottom_left / g_PatchLength * g_PerlinSize;
		call_consts.g_UVBase = uv_base;

		// Constant g_PerlinSpeed need to be adjusted mannually
		D3DXVECTOR2 perlin_move = -g_WindDir * time * g_PerlinSpeed;
		call_consts.g_PerlinMovement = perlin_move;

		// Eye point
		D3DXMATRIX matInvWV = matWorld * matView;
		D3DXMatrixInverse(&matInvWV, NULL, &matInvWV);
		D3DXVECTOR3 vLocalEye(0, 0, 0);
		D3DXVec3TransformCoord(&vLocalEye, &vLocalEye, &matInvWV);
		call_consts.g_LocalEye = vLocalEye;

		// Update constant buffer
		D3D11_MAPPED_SUBRESOURCE mapped_res;            
		pd3dContext->Map(g_pPerCallCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
		assert(mapped_res.pData);
		*(Const_Per_Call*)mapped_res.pData = call_consts;
		pd3dContext->Unmap(g_pPerCallCB, 0);

		cbs[0] = g_pPerCallCB;
		pd3dContext->VSSetConstantBuffers(4, 1, cbs);
		pd3dContext->PSSetConstantBuffers(4, 1, cbs);

		// Perform draw call
		if (render_param.num_inner_faces > 0)
		{
			// Inner mesh of the patch
			pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			pd3dContext->DrawIndexed(render_param.num_inner_faces + 2, render_param.inner_start_index, 0);
		}

		if (render_param.num_boundary_faces > 0)
		{
			// Boundary mesh of the patch
			pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pd3dContext->DrawIndexed(render_param.num_boundary_faces * 3, render_param.boundary_start_index, 0);
		}
	}

	// Unbind
	vs_srvs[0] = NULL;
	vs_srvs[1] = NULL;
	pd3dContext->VSSetShaderResources(0, 2, &vs_srvs[0]);

	// Restore states
	pd3dContext->RSSetState(g_pRSState_Solid);
}
