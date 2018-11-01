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
#include "DXUTcamera.h"
#include "SDKMisc.h"
#include "d3dx11effect.h"
#include "d3d11.h"


#define terrain_gridpoints					512
#define terrain_numpatches_1d				64
#define terrain_geometry_scale				1.0f
#define terrain_maxheight					30.0f 
#define terrain_minheight					-30.0f 
#define terrain_fractalfactor				0.68f;
#define terrain_fractalinitialvalue			100.0f
#define terrain_smoothfactor1				0.99f
#define terrain_smoothfactor2				0.10f
#define terrain_rockfactor					0.95f
#define terrain_smoothsteps					40

#define terrain_height_underwater_start		-100.0f
#define terrain_height_underwater_end		-8.0f
#define terrain_height_sand_start			-30.0f
#define terrain_height_sand_end				1.7f
#define terrain_height_grass_start			1.7f
#define terrain_height_grass_end			30.0f
#define terrain_height_rocks_start			-2.0f
#define terrain_height_trees_start			4.0f
#define terrain_height_trees_end			30.0f
#define terrain_slope_grass_start			0.96f
#define terrain_slope_rocks_start			0.85f

#define terrain_far_range terrain_gridpoints*terrain_geometry_scale

#define shadowmap_resource_buffer_size_xy				4096
#define water_normalmap_resource_buffer_size_xy			2048
#define terrain_layerdef_map_texture_size				1024
#define terrain_depth_shadow_map_texture_size			512

#define sky_gridpoints						10
#define sky_texture_angle					0.425f

#define main_buffer_size_multiplier			1.1f
#define reflection_buffer_size_multiplier   1.1f
#define refraction_buffer_size_multiplier   1.1f

#define scene_z_near						1.0f
#define scene_z_far							25000.0f
#define camera_fov							110.0f

class CTerrain
{
	public:


		void Initialize(ID3D11Device*, ID3DX11Effect*);
		void DeInitialize();
		void ReCreateBuffers();
		void LoadTextures();
		void Render(CFirstPersonCamera *);
		void CreateTerrain();

		float DynamicTesselationFactor;
		float StaticTesselationFactor;
		void SetupNormalView(CFirstPersonCamera *);
		void SetupReflectionView(CFirstPersonCamera *);
		void SetupRefractionView(CFirstPersonCamera *);
		void SetupLightView(CFirstPersonCamera * );
		float BackbufferWidth;
		float BackbufferHeight;

		UINT MultiSampleCount;
		UINT MultiSampleQuality;

		ID3D11Texture2D		*rock_bump_texture;
		ID3D11ShaderResourceView *rock_bump_textureSRV;

		ID3D11Texture2D		*rock_microbump_texture;
		ID3D11ShaderResourceView *rock_microbump_textureSRV;

		ID3D11Texture2D		*rock_diffuse_texture;
		ID3D11ShaderResourceView *rock_diffuse_textureSRV;	

		ID3D11Texture2D		*sand_bump_texture;
		ID3D11ShaderResourceView *sand_bump_textureSRV;

		ID3D11Texture2D		*sand_microbump_texture;
		ID3D11ShaderResourceView *sand_microbump_textureSRV;

		ID3D11Texture2D		*sand_diffuse_texture;
		ID3D11ShaderResourceView *sand_diffuse_textureSRV;	

		ID3D11Texture2D		*grass_diffuse_texture;
		ID3D11ShaderResourceView *grass_diffuse_textureSRV;	

		ID3D11Texture2D		*slope_diffuse_texture;
		ID3D11ShaderResourceView *slope_diffuse_textureSRV;	

		ID3D11Texture2D		*water_bump_texture;
		ID3D11ShaderResourceView *water_bump_textureSRV;	

		ID3D11Texture2D		*sky_texture;
		ID3D11ShaderResourceView *sky_textureSRV;	

		ID3D11Texture2D			 *reflection_color_resource;
		ID3D11ShaderResourceView *reflection_color_resourceSRV;
		ID3D11RenderTargetView   *reflection_color_resourceRTV;
		ID3D11Texture2D			 *refraction_color_resource;
		ID3D11ShaderResourceView *refraction_color_resourceSRV;
		ID3D11RenderTargetView   *refraction_color_resourceRTV;

		ID3D11Texture2D			 *shadowmap_resource;
		ID3D11ShaderResourceView *shadowmap_resourceSRV;
		ID3D11DepthStencilView   *shadowmap_resourceDSV;

		ID3D11Texture2D			 *reflection_depth_resource;
		ID3D11DepthStencilView   *reflection_depth_resourceDSV;


		ID3D11Texture2D			 *refraction_depth_resource;
		ID3D11RenderTargetView   *refraction_depth_resourceRTV;
		ID3D11ShaderResourceView *refraction_depth_resourceSRV;

		ID3D11Texture2D			 *water_normalmap_resource;
		ID3D11ShaderResourceView *water_normalmap_resourceSRV;
		ID3D11RenderTargetView   *water_normalmap_resourceRTV;

		ID3D11Texture2D			 *main_color_resource;
		ID3D11ShaderResourceView *main_color_resourceSRV;
		ID3D11RenderTargetView   *main_color_resourceRTV;
		ID3D11Texture2D			 *main_depth_resource;
		ID3D11DepthStencilView   *main_depth_resourceDSV;
		ID3D11ShaderResourceView *main_depth_resourceSRV;
		ID3D11Texture2D			 *main_color_resource_resolved;
		ID3D11ShaderResourceView *main_color_resource_resolvedSRV;

		ID3D11Device* pDevice;
		ID3DX11Effect* pEffect;




		float				height[terrain_gridpoints+1][terrain_gridpoints+1];
		D3DXVECTOR3			normal[terrain_gridpoints+1][terrain_gridpoints+1];
		D3DXVECTOR3			tangent[terrain_gridpoints+1][terrain_gridpoints+1];
		D3DXVECTOR3			binormal[terrain_gridpoints+1][terrain_gridpoints+1];

		ID3D11Texture2D		*heightmap_texture;
		ID3D11ShaderResourceView *heightmap_textureSRV;

		ID3D11Texture2D		*layerdef_texture;
		ID3D11ShaderResourceView *layerdef_textureSRV;

		ID3D11Texture2D		*depthmap_texture;
		ID3D11ShaderResourceView *depthmap_textureSRV;

		ID3D11Buffer		*heightfield_vertexbuffer;
		ID3D11Buffer		*sky_vertexbuffer;

		ID3D11InputLayout   *heightfield_inputlayout;
		ID3D11InputLayout   *trianglestrip_inputlayout;

};

float bilinear_interpolation(float fx, float fy, float a, float b, float c, float d);

