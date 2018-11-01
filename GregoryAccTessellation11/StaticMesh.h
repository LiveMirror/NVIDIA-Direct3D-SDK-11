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

class StaticMesh : public IMesh
{
public:
	StaticMesh(const wchar_t* filename);
	~StaticMesh(void);

	BzrFile Bzr() const { return m_bzr; }
	HRESULT Init();

	void Render(ID3D11DeviceContext* pd3dDeviceContext, UINT32 mode, UINT32 offset );
	void adjustWorldPosition(D3DXMATRIX & mWorld);
	void GenerateShadowMap( ID3D11DeviceContext* pd3dDeviceContext, int offsetVertex );

private:
	// prevent copying
	StaticMesh(const StaticMesh&);

	StaticMesh& operator =(const StaticMesh&);

	// Data members
	BzrFile m_bzr;
	int m_regularPatchCount;
	int m_quadPatchCount;
	int m_triPatchCount;
	int m_vertexCount;
	int m_indexCount;
	int m_indexCount2;
	float3 m_center;
	ID3D11Buffer* m_pVertexBuffer;
	ID3D11Buffer* m_pRegIndexBuffer;
	ID3D11Buffer* m_pQuadIndexBuffer;
	ID3D11Buffer* m_pInputMeshIndexBuffer;
	ID3D11Buffer* m_pcbPerMesh;
	ID3D11ShaderResourceView* m_pControlPointsTextureSRV;
	ID3D11Texture2D* m_pControlPointsTexture;
	ID3D11ShaderResourceView* m_pGregoryControlPointsTextureSRV;
	ID3D11Texture2D* m_pGregoryControlPointsTexture;
};
