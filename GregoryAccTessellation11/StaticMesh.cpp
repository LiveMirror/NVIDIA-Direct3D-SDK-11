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
#include "StaticMesh.h"
#include "GPUPatchUtils.h"
#include "Globals.h"
#include "typedef.h" 

StaticMesh::StaticMesh( const wchar_t* filename )
: m_bzr(filename)
, m_pVertexBuffer(0)
, m_pRegIndexBuffer(0)
, m_pQuadIndexBuffer(0)
, m_pInputMeshIndexBuffer(0)
, m_pControlPointsTexture(0)
, m_pControlPointsTextureSRV(0)
, m_pGregoryControlPointsTexture(0)
, m_pGregoryControlPointsTextureSRV(0)
, m_pcbPerMesh(0)
{

}

StaticMesh::~StaticMesh(void)
{
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pRegIndexBuffer);
	SAFE_RELEASE(m_pQuadIndexBuffer);
	SAFE_RELEASE(m_pInputMeshIndexBuffer);

	SAFE_RELEASE(m_pControlPointsTexture);
	SAFE_RELEASE(m_pControlPointsTextureSRV);
	SAFE_RELEASE(m_pGregoryControlPointsTexture);
	SAFE_RELEASE(m_pGregoryControlPointsTextureSRV);
	SAFE_RELEASE(m_pcbPerMesh);

}
void StaticMesh::adjustWorldPosition(D3DXMATRIX & mWorld)
{
	D3DXMATRIX mSpinX, mSpinZ, mSpinY, mTranslate;
	D3DXMatrixScaling(&mWorld, 15,15,15);
	D3DXMatrixRotationY( &mSpinY, -D3DX_PI*0.05 );
	D3DXMatrixMultiply( &mWorld, &mWorld, &mSpinY );
	D3DXMatrixRotationX( &mSpinX, -D3DX_PI*0.02 );
	D3DXMatrixMultiply( &mWorld, &mWorld, &mSpinX );
	D3DXMatrixTranslation( &mTranslate, 30.0f, -18.0f, 0.0f );
	D3DXMatrixMultiply( &mWorld, &mWorld, &mTranslate );
}
void StaticMesh::Render( ID3D11DeviceContext* pd3dDeviceContext, UINT32 mode, UINT32 offset)
{
	D3DXMATRIX mWorld = *g_Camera.GetWorldMatrix();
    adjustWorldPosition(mWorld);

	D3DXMATRIX mView = *g_Camera.GetViewMatrix();
	D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
	D3DXMATRIX mWorldView = mWorld * mView;
	D3DXMATRIX mViewProjection = mWorldView * mProj;
	D3DXVECTOR3 vEyePt = *g_Camera.GetEyePt();
	D3DXVECTOR3 vEyeDir = *g_Camera.GetLookAtPt() - vEyePt;
	D3DXMATRIX mLightView = *g_Light.GetViewMatrix();
	D3DXMATRIX mLightProj = *g_Light.GetProjMatrix();

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dDeviceContext->Map( g_pcbPerDraw, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_PER_DRAW_CONSTANTS* pData = ( CB_PER_DRAW_CONSTANTS* )MappedResource.pData;
	pData = ( CB_PER_DRAW_CONSTANTS* )MappedResource.pData;
	D3DXMatrixTranspose( &pData->world, &mWorld );
	D3DXMatrixTranspose( &pData->worldView, &mWorldView );
	D3DXMatrixTranspose( &pData->viewProjection, &mViewProjection );
	D3DXMATRIX mLightViewProjection = (mWorld * mLightView) * mLightProj;
	D3DXMatrixTranspose( &pData->mLightViewProjection, &mLightViewProjection );
	pd3dDeviceContext->Unmap( g_pcbPerDraw, 0 );

	ID3D11PixelShader* pPixelShader;
	ID3D11HullShader* pHullShader;
	if (g_bFlatShading) pPixelShader = g_pFlatShadingPS;
	else pPixelShader = g_pPhongShadingPS;
	
	if (mode==2) pPixelShader = g_pSolidColorPS;
	pHullShader = g_pStaticObjectHS;
	if (g_iAdaptive == 1) pHullShader = g_pStaticObjectHS_Distance;

	// Bind all of the CBs
	pd3dDeviceContext->HSSetConstantBuffers( g_iBindPerDraw, 1, &g_pcbPerDraw );
	pd3dDeviceContext->DSSetConstantBuffers( g_iBindPerDraw, 1, &g_pcbPerDraw );
	pd3dDeviceContext->PSSetConstantBuffers( g_iBindPerDraw, 1, &g_pcbPerDraw );

	pd3dDeviceContext->HSSetConstantBuffers( g_iBindPerMesh, 1, &m_pcbPerMesh );
	pd3dDeviceContext->DSSetConstantBuffers( g_iBindPerMesh, 1, &m_pcbPerMesh );
	pd3dDeviceContext->PSSetConstantBuffers( g_iBindPerMesh, 1, &m_pcbPerMesh );


	// Render the input mesh of the character
	pd3dDeviceContext->VSSetShader( g_pStaticObjectVS, NULL, 0 );
	pd3dDeviceContext->HSSetShader( pHullShader, NULL, 0 );
	pd3dDeviceContext->DSSetShader( g_pStaticObjectDS, NULL, 0 );
	pd3dDeviceContext->PSSetShader( pPixelShader, NULL, 0 );
	pd3dDeviceContext->HSSetShaderResources( 5, 1, &m_pControlPointsTextureSRV );

	// Array of our samplers
	ID3D11SamplerState* ppSamplerStates[2] = { g_pSamplePoint, g_pSampleLinear};
	pd3dDeviceContext->HSSetSamplers( 0, 2, ppSamplerStates );
	pd3dDeviceContext->DSSetSamplers( 0, 2, ppSamplerStates );
	pd3dDeviceContext->PSSetSamplers( 0, 2, ppSamplerStates );

	// Set the input layout
	pd3dDeviceContext->IASetInputLayout( g_pConstructionLayout );

	// Set the input assembler
	pd3dDeviceContext->IASetIndexBuffer( m_pRegIndexBuffer, DXGI_FORMAT_R32_UINT, 0 ); 
	UINT Stride = sizeof( Vertex );
	UINT Offset = 0;
	pd3dDeviceContext->IASetVertexBuffers( 0, 1, &m_pVertexBuffer, &Stride, &Offset );
	pd3dDeviceContext->IASetPrimitiveTopology(  D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST );

	pd3dDeviceContext->DrawIndexed(4*m_regularPatchCount, 0, 0);

	// draw irregular patches
	pHullShader = g_pStaticObjectGregoryHS;
	if (g_iAdaptive == 1) pHullShader = g_pStaticObjectGregoryHS_Distance;
	pd3dDeviceContext->HSSetShader( pHullShader, NULL, 0 );
	if (g_bFlatShading)
	{
		if (g_bDrawGregoryACC)
			pPixelShader = g_pFlatShadingPS_I;
		else pPixelShader = g_pFlatShadingPS;
	}
	else
	{
		if (g_bDrawGregoryACC)
			pPixelShader = g_pPhongShadingPS_I;
		else pPixelShader = g_pPhongShadingPS;
	}
	if (mode==2) pPixelShader = g_pSolidColorPS;
	pd3dDeviceContext->HSSetShaderResources( 6, 1, &m_pGregoryControlPointsTextureSRV );
	pd3dDeviceContext->DSSetShader( g_pStaticObjectGregoryDS, NULL, 0 );
	pd3dDeviceContext->PSSetShader( pPixelShader, NULL, 0 );
	pd3dDeviceContext->IASetIndexBuffer( m_pQuadIndexBuffer, DXGI_FORMAT_R32_UINT, 0 ); 

	pd3dDeviceContext->DrawIndexed(4*m_quadPatchCount, 0, 0);

    ID3D11ShaderResourceView* pNull = NULL;
	pd3dDeviceContext->PSSetShaderResources( 3, 1, &pNull );

}
void StaticMesh::GenerateShadowMap( ID3D11DeviceContext* pd3dDeviceContext, int offsetVertex )
{

	D3DXMATRIX mWorld = *g_Camera.GetWorldMatrix();
	adjustWorldPosition(mWorld);

	D3DXMATRIX mView = *g_Camera.GetViewMatrix();
	D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
	D3DXMATRIX mWorldView = mWorld * mView;
	D3DXMATRIX mViewProjection = mWorldView * mProj;
	D3DXVECTOR3 vEyePt = *g_Camera.GetEyePt();
	D3DXVECTOR3 vEyeDir = *g_Camera.GetLookAtPt() - vEyePt;
	D3DXMATRIX mLightView = *g_Light.GetViewMatrix();
	D3DXMATRIX mLightProj = *g_Light.GetProjMatrix();

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dDeviceContext->Map( g_pcbPerDraw, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_PER_DRAW_CONSTANTS* pData = ( CB_PER_DRAW_CONSTANTS* )MappedResource.pData;
	pData = ( CB_PER_DRAW_CONSTANTS* )MappedResource.pData;
	D3DXMatrixTranspose( &pData->world, &mWorld );
	D3DXMatrixTranspose( &pData->worldView, &mWorldView );
	D3DXMatrixTranspose( &pData->viewProjection, &mViewProjection );
	D3DXMATRIX mLightViewProjection = mWorld * mLightView * mLightProj;
	D3DXMatrixTranspose( &pData->mLightViewProjection, &mLightViewProjection );

	pd3dDeviceContext->Unmap( g_pcbPerDraw, 0 );

	// Bind all of the CBs
	pd3dDeviceContext->VSSetConstantBuffers( g_iBindPerDraw, 1, &g_pcbPerDraw );
	// Render the character
	pd3dDeviceContext->VSSetShader( g_pShadowmapVS, NULL, 0 );
	pd3dDeviceContext->PSSetShader( g_pShadowmapPS, NULL, 0 );

	pd3dDeviceContext->IASetInputLayout( g_pConstructionLayout );
	pd3dDeviceContext->IASetIndexBuffer( m_pInputMeshIndexBuffer, DXGI_FORMAT_R32_UINT, 0 ); 
	UINT Stride = sizeof( Vertex );
	UINT Offset = 0;
	pd3dDeviceContext->IASetVertexBuffers( 0, 1, &m_pVertexBuffer, &Stride, &Offset );
	pd3dDeviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	pd3dDeviceContext->DrawIndexed( m_indexCount, 0, offsetVertex );

}
HRESULT StaticMesh::Init()
{
	HRESULT hr = S_OK;

	ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
	ID3D11Device* pd3dDevice = DXUTGetD3D11Device();

	// number of regular quads in the input mesh
	m_regularPatchCount=m_bzr.regularPatchCount(); 
	// number of irregular quads in the input mesh
    m_quadPatchCount=m_bzr.quadPatchCount();
	// number of indices for all regular quads
	m_indexCount=m_bzr.indexCount();
	// number of indices for all irregular quads
	m_indexCount2=m_bzr.indexCount2();
	// number of vertices in the input mesh
	m_vertexCount = m_bzr.vertexCount();

	m_center = m_bzr.center();
	assert (m_vertexCount>0);

	Vertex *  pVerts = new Vertex[m_vertexCount];
	const float3 * vPos = m_bzr.fetchVertices();
	for (int v=0; v<m_vertexCount; ++v)
	{
		pVerts[v].m_Position=vPos[v];               //vertices position
		pVerts[v].m_Texcoord=float2(0,0);
		pVerts[v].m_Tangent=float3(1,0,0);

		pVerts[v].m_Bitangent=float3(0,1,0);
	}

	// create a vertex buffer
	CreateABuffer(pd3dDevice, D3D11_BIND_VERTEX_BUFFER,  m_vertexCount*sizeof(Vertex), 
		D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC,  m_pVertexBuffer, (void *)& pVerts[0]);
	delete [] pVerts;

	// create an index buffer
	const unsigned int * indicesReg = m_bzr.fetchRegularVertexIndices(); 
	unsigned int * indices = new unsigned int[4*m_regularPatchCount];
	for (int i=0; i<m_regularPatchCount; i++)
	{
		indices[4*i]=*(indicesReg+16*i);
		indices[4*i+1]=*(indicesReg+16*i+1);
		indices[4*i+2]=*(indicesReg+16*i+2);
		indices[4*i+3]=*(indicesReg+16*i+3);
	}

	if ( m_regularPatchCount==0)
		CreateABuffer(pd3dDevice, D3D11_BIND_INDEX_BUFFER, sizeof(unsigned int), 0, D3D11_USAGE_DEFAULT,  m_pRegIndexBuffer, (void *)&indices[0]);
	else
		CreateABuffer(pd3dDevice, D3D11_BIND_INDEX_BUFFER, 4* m_regularPatchCount*sizeof(unsigned int), 0, D3D11_USAGE_DEFAULT,  m_pRegIndexBuffer, (void *)&indices[0]);
	indicesReg=NULL;
	delete [] indices;

	const unsigned int * indicesIrreg = m_bzr.fetchQuadVertexIndices(); 
	unsigned int * indices2 = new unsigned int[4*m_quadPatchCount];
	for (int i=0; i<m_quadPatchCount; i++)
	{
		indices2[4*i]=*(indicesIrreg+16*i);
		indices2[4*i+1]=*(indicesIrreg+16*i+1);
		indices2[4*i+2]=*(indicesIrreg+16*i+2);
		indices2[4*i+3]=*(indicesIrreg+16*i+3);
	}

	CreateABuffer(pd3dDevice, D3D11_BIND_INDEX_BUFFER, 4* m_quadPatchCount*sizeof(unsigned int), 0, D3D11_USAGE_DEFAULT,  m_pQuadIndexBuffer, (void *)&indices2[0]);
	indicesIrreg=NULL;
	delete [] indices2;

	m_indexCount=m_bzr.indexCount();
	const unsigned int* indicesInputMesh = m_bzr.fetchInputMeshVertexIndices(); 
	CreateABuffer(pd3dDevice, D3D11_BIND_INDEX_BUFFER, m_indexCount*sizeof(unsigned int), 0, D3D11_USAGE_DEFAULT,  m_pInputMeshIndexBuffer, (void *)&indicesInputMesh[0]);
	indicesInputMesh=NULL;

	// create texture
	const float3 * pRegularPatchControlPoints = m_bzr.fetchBezierControlPoints();
	V_RETURN( CreateDataTexture( pd3dDevice, 16, m_regularPatchCount, DXGI_FORMAT_R32G32B32_FLOAT, pRegularPatchControlPoints, 16 * sizeof(float3), 
		& m_pControlPointsTexture, & m_pControlPointsTextureSRV ));
	pRegularPatchControlPoints=NULL;

	const float3 * pQuadPatchControlPoints = m_bzr.fetchGregoryControlPoints();
	V_RETURN( CreateDataTexture( pd3dDevice, 20, m_quadPatchCount, DXGI_FORMAT_R32G32B32_FLOAT, pQuadPatchControlPoints, 20 * sizeof(float3), 
		& m_pGregoryControlPointsTexture, & m_pGregoryControlPointsTextureSRV ));
	pRegularPatchControlPoints=NULL;

	// Setup constant buffers
	D3D11_BUFFER_DESC Desc;
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.MiscFlags = 0;
	Desc.ByteWidth = sizeof( CB_PER_MESH_CONSTANTS );
	V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &m_pcbPerMesh ) );

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map( m_pcbPerMesh, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_PER_MESH_CONSTANTS* pData = ( CB_PER_MESH_CONSTANTS* )MappedResource.pData;
	pData->diffuse = (float4(0.4f, 0.25f, 0.2f, 0.0f) + float4(0.6f, 0.6f, 0.6f, 0.0f))/2;
	pData->center = float4(m_center,1);
	pd3dImmediateContext->Unmap( m_pcbPerMesh, 0 );

	return hr;
}