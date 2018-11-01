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

#include "Bzrfile_stencil.h"
#include "IMesh.h"

class DynamicMesh : public IMesh
{
public:
	DynamicMesh(const wchar_t* filename);
	~DynamicMesh(void);

	BzrFile Bzr() const { return m_bzr; }
	HRESULT Init();

	void Render(ID3D11DeviceContext* pd3dDeviceContext, UINT32 mode, UINT32 offset);
	void GenerateShadowMap( ID3D11DeviceContext* pd3dDeviceContext, int offsetVertex );
	void adjustWorldPosition(D3DXMATRIX & mWorld);

private:
	// prevent copying
	DynamicMesh(const DynamicMesh&);

	DynamicMesh& operator =(const DynamicMesh&);

	// Data members
	BzrFile m_bzr;
	int m_RegularPatchCount;
	int m_QuadPatchCount;
	int m_TriPatchCount;
	int m_IndexCount;
	int m_IndexCount2;
	int m_VertexCount;
	float3 m_center;
	int m_faceTopologyCount;
	int m_primitiveSize;
	D3D11_PRIMITIVE_TOPOLOGY m_topologyType;
	ID3D11Buffer* m_pConstructionVertexBuffer;
	ID3D11Buffer* m_pRegIndexBuffer;
	ID3D11Buffer* m_pQuadIndexBuffer;
	ID3D11Buffer* m_pInputMeshIndexBuffer;
	ID3D11Buffer* m_pcbPerPatch;
	ID3D11Buffer* m_pcbPerMesh;
	ID3D11ShaderResourceView* m_pQuadIntDataTextureSRV;
	ID3D11Texture2D* m_pQuadIntDataTexture;
	ID3D11ShaderResourceView* m_pGregoryStencilTextureSRV;
	ID3D11Texture2D* m_pGregoryStencilTexture;
	ID3D11Texture2D* m_pQuadWatertightTex;
	ID3D11ShaderResourceView* m_pQuadWatertightTexSRV;
	ID3D11Texture2D* m_pRegularWatertightTex;
	ID3D11ShaderResourceView* m_pRegularWatertightTexSRV;
};
