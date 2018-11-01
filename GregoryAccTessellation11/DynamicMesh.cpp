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
#include "SDKmisc.h"
#include "DynamicMesh.h"
#include "GPUPatchUtils.h"
#include "Globals.h"
#include "typedef.h" 

#define FRAMENUM 100

const static float weights[16*16]= {
	0.444444, 0.444444, 0.222222, 0.111111, 0.444444, 0.444444, 0.222222, 0.111111, 0.222222, 0.222222, 0.111111, 0.055556, 0.111111, 0.111111, 0.055556, 0.027778, 
	0.111111, 0.222222, 0.444444, 0.444444, 0.111111, 0.222222, 0.444444, 0.444444, 0.055556, 0.111111, 0.222222, 0.222222, 0.027778, 0.055556, 0.111111, 0.111111, 
	0.027778, 0.055556, 0.111111, 0.111111, 0.055556, 0.111111, 0.222222, 0.222222, 0.111111, 0.222222, 0.444444, 0.444444, 0.111111, 0.222222, 0.444444, 0.444444,
	0.111111, 0.111111, 0.055556, 0.027778, 0.222222, 0.222222, 0.111111, 0.055556, 0.444444, 0.444444, 0.222222, 0.111111, 0.444444, 0.444444, 0.222222, 0.111111,
	0.027778, 0.000000, 0.000000, 0.000000, 0.055556, 0.000000, 0.000000, 0.000000, 0.111111, 0.000000, 0.000000, 0.000000, 0.111111, 0.000000, 0.000000, 0.000000,  
	0.111111, 0.000000, 0.000000, 0.000000, 0.111111, 0.000000, 0.000000, 0.000000, 0.055556, 0.000000, 0.000000, 0.000000, 0.027778, 0.000000, 0.000000, 0.000000,
	0.027778, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000,  
	0.111111, 0.111111, 0.055556, 0.027778, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 
	0.027778, 0.055556, 0.111111, 0.111111, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 
	0.000000, 0.000000, 0.000000, 0.027778, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000,  
	0.000000, 0.000000, 0.000000, 0.111111, 0.000000, 0.000000, 0.000000, 0.111111, 0.000000, 0.000000, 0.000000, 0.055556, 0.000000, 0.000000, 0.000000, 0.027778,  
	0.000000, 0.000000, 0.000000, 0.027778, 0.000000, 0.000000, 0.000000, 0.055556, 0.000000, 0.000000, 0.000000, 0.111111, 0.000000, 0.000000, 0.000000, 0.111111, 
	0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.027778, 
	0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.027778, 0.055556, 0.111111, 0.111111,
	0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.111111, 0.111111, 0.055556, 0.027778, 
	0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.027778, 0.000000, 0.000000, 0.000000, 
};

DynamicMesh::DynamicMesh( const wchar_t* filename )
: m_bzr(filename)
, m_pConstructionVertexBuffer(0)
, m_pRegIndexBuffer(0)
, m_pQuadIndexBuffer(0)
, m_pInputMeshIndexBuffer(0)
, m_pGregoryStencilTexture(0)
, m_pGregoryStencilTextureSRV(0)
, m_pcbPerPatch(0)
, m_pcbPerMesh(0)
, m_pQuadWatertightTex(0)
, m_pQuadWatertightTexSRV(0)
, m_pRegularWatertightTex(0)
, m_pRegularWatertightTexSRV(0)
{

}

DynamicMesh::~DynamicMesh(void)
{
	SAFE_RELEASE(m_pConstructionVertexBuffer);
	SAFE_RELEASE(m_pRegIndexBuffer);
	SAFE_RELEASE(m_pQuadIndexBuffer);
	SAFE_RELEASE(m_pInputMeshIndexBuffer);
	SAFE_RELEASE(m_pQuadIntDataTexture);
	SAFE_RELEASE(m_pQuadIntDataTextureSRV);
	SAFE_RELEASE(m_pGregoryStencilTexture);
	SAFE_RELEASE(m_pGregoryStencilTextureSRV);
	SAFE_RELEASE(m_pcbPerPatch);
	SAFE_RELEASE(m_pcbPerMesh);
	SAFE_RELEASE(m_pRegularWatertightTex);
	SAFE_RELEASE(m_pRegularWatertightTexSRV);
	SAFE_RELEASE(m_pQuadWatertightTex);
	SAFE_RELEASE(m_pQuadWatertightTexSRV);
}

void DynamicMesh::Render( ID3D11DeviceContext* pd3dDeviceContext, UINT32 mode, UINT32 offset )
{
	D3DXMATRIX mWorld = *g_Camera.GetWorldMatrix();

	adjustWorldPosition(mWorld);

	D3DXMATRIX mView = *g_Camera.GetViewMatrix();
	D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
	D3DXMATRIX mWorldView = mWorld * mView;
	D3DXMATRIX mViewProjection = mWorldView * mProj;
	D3DXMATRIX mLightView = *g_Light.GetViewMatrix();
	D3DXMATRIX mLightProj = *g_Light.GetProjMatrix();

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dDeviceContext->Map( g_pcbPerDraw, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_PER_DRAW_CONSTANTS* pData = ( CB_PER_DRAW_CONSTANTS* )MappedResource.pData;
	pData = ( CB_PER_DRAW_CONSTANTS* )MappedResource.pData;
	D3DXMatrixTranspose( &pData->world, &mWorld );
	D3DXMatrixTranspose( &pData->worldView, &mWorldView );
	D3DXMatrixTranspose( &pData->viewProjection, &mViewProjection );

	mViewProjection = mWorld * mLightView * mLightProj;
	D3DXMatrixTranspose( &pData->mLightViewProjection, &mViewProjection );
	pd3dDeviceContext->Unmap( g_pcbPerDraw, 0 );

	// Render regular patches
	ID3D11PixelShader* pPixelShader;
	if (g_bFlatShading) pPixelShader = g_pFlatShadingPS;
	else pPixelShader = g_pPhongShadingPS;
	if (mode==2) pPixelShader = g_pSolidColorPS;
	ID3D11HullShader* hs = g_pSubDToBezierHS;
	if (g_iAdaptive == 1) hs = g_pSubDToBezierHS_Distance;

	// Bind all of the CBs
	pd3dDeviceContext->HSSetConstantBuffers( g_iBindPerPatch, 1, &m_pcbPerPatch);
	pd3dDeviceContext->HSSetConstantBuffers( g_iBindPerMesh, 1, &m_pcbPerMesh );
	pd3dDeviceContext->DSSetConstantBuffers( g_iBindPerMesh, 1, &m_pcbPerMesh );

	pd3dDeviceContext->HSSetConstantBuffers( g_iBindPerDraw, 1, &g_pcbPerDraw );
	pd3dDeviceContext->DSSetConstantBuffers( g_iBindPerDraw, 1, &g_pcbPerDraw );
	pd3dDeviceContext->PSSetConstantBuffers( g_iBindPerDraw, 1, &g_pcbPerDraw );

	// Set the shaders
	pd3dDeviceContext->VSSetShader( g_pSkinningVS, NULL, 0 );
	pd3dDeviceContext->HSSetShader( hs, NULL, 0 );
	pd3dDeviceContext->DSSetShader( g_pSurfaceEvalDS, NULL, 0 );
	pd3dDeviceContext->PSSetShader( pPixelShader, NULL, 0 );

	// Array of our samplers
	ID3D11SamplerState* ppSamplerStates[2] = { g_pSamplePoint, g_pSampleLinear};
	pd3dDeviceContext->HSSetSamplers( 0, 2, ppSamplerStates );
	pd3dDeviceContext->DSSetSamplers( 0, 2, ppSamplerStates );
	pd3dDeviceContext->PSSetSamplers( 0, 2, ppSamplerStates );

	ID3D11ShaderResourceView* Resources[] = {NULL};
	Resources[0] = m_pGregoryStencilTextureSRV;
	Resources[1] = m_pQuadIntDataTextureSRV;

	pd3dDeviceContext->HSSetShaderResources( 0, 2, Resources ); 
	pd3dDeviceContext->HSSetShaderResources( 8, 1, &m_pRegularWatertightTexSRV );
	pd3dDeviceContext->HSSetShaderResources( 9, 1, &m_pQuadWatertightTexSRV );

	// The domain shader only needs the heightmap, so we only set 1 slot here.
	pd3dDeviceContext->HSSetShaderResources( 2, 1, &pHeightMapTextureSRV );
	pd3dDeviceContext->DSSetShaderResources( 2, 1, &pHeightMapTextureSRV );
	pd3dDeviceContext->PSSetShaderResources( 2, 1, &pHeightMapTextureSRV );
	pd3dDeviceContext->PSSetShaderResources( 10, 1, &pNormalMapTextureSRV );
	pd3dDeviceContext->PSSetShaderResources( 3, 1, &pTexRenderRV11 );


	// Set the input layout
	pd3dDeviceContext->IASetInputLayout( g_pConstructionLayout );

	// Set the input assembler
	pd3dDeviceContext->IASetIndexBuffer( m_pRegIndexBuffer, DXGI_FORMAT_R32_UINT, 0 ); 
	UINT Stride = sizeof( Vertex );
	UINT Offset = 0;
	pd3dDeviceContext->IASetVertexBuffers( 0, 1, &m_pConstructionVertexBuffer, &Stride, &Offset );
	pd3dDeviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST );

	UINT32 drawFacets =  16*m_RegularPatchCount;

	pd3dDeviceContext->DrawIndexed( drawFacets, 0, m_VertexCount*offset );

	//Draw gregory patches

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
	hs = g_pSubDToGregoryHS;
	if (g_iAdaptive == 1) hs = g_pSubDToGregoryHS_Distance;
	pd3dDeviceContext->PSSetShader( pPixelShader, NULL, 0 );
	// Set the shaders
	pd3dDeviceContext->HSSetShader( hs, NULL, 0 );
	pd3dDeviceContext->DSSetShader( g_pGregoryEvalDS, NULL, 0 );

	// Set the input assembler
	pd3dDeviceContext->IASetIndexBuffer( m_pQuadIndexBuffer, DXGI_FORMAT_R32_UINT, 0 ); 
	pd3dDeviceContext->IASetPrimitiveTopology( m_topologyType );
	pd3dDeviceContext->DrawIndexed( m_primitiveSize*m_QuadPatchCount, 0, m_VertexCount*offset);

	ID3D11ShaderResourceView* pNull = NULL;
	pd3dDeviceContext->PSSetShaderResources( 3, 1, &pNull );
}

void DynamicMesh::adjustWorldPosition(D3DXMATRIX & mWorld)
{
	D3DXMATRIX mSpinX, mSpinZ, mSpinY, mTranslate;
	D3DXMatrixRotationY( &mSpinY, -D3DX_PI*0.05 );
	D3DXMatrixMultiply( &mWorld, &mWorld, &mSpinY );
	D3DXMatrixRotationX( &mSpinX, -D3DX_PI*0.02 );
	D3DXMatrixMultiply( &mWorld, &mWorld, &mSpinX );
	D3DXMatrixTranslation( &mTranslate, -20.0f, 0.0f, 0.0f );
	D3DXMatrixMultiply( &mWorld, &mWorld, &mTranslate );	

}
void DynamicMesh::GenerateShadowMap( ID3D11DeviceContext* pd3dDeviceContext, int offset )
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
	pd3dDeviceContext->IASetVertexBuffers( 0, 1, &m_pConstructionVertexBuffer, &Stride, &Offset );
	pd3dDeviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	pd3dDeviceContext->DrawIndexed( m_IndexCount, 0, m_VertexCount*offset);

}
HRESULT DynamicMesh::Init()
{
	HRESULT hr = S_OK;
	ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
	ID3D11Device* pd3dDevice = DXUTGetD3D11Device();

	/* =================load topology info of the base mesh ==========================*/
	// number of regular quads in the input mesh
	m_RegularPatchCount=m_bzr.regularPatchCount(); 
	// number of irregular quads in the input mesh
	m_QuadPatchCount=m_bzr.quadPatchCount();
	// number of triangles in the input mesh
	m_TriPatchCount=m_bzr.triPatchCount();
	m_IndexCount=m_bzr.indexCount();
	m_IndexCount2=m_bzr.indexCount2();
	// number of vertices in the input mesh
	m_VertexCount = m_bzr.vertexCount();
	m_center = m_bzr.center();
	m_faceTopologyCount =m_bzr.faceTopologyCount();
	m_primitiveSize =m_bzr.primitiveSize();

	switch(m_primitiveSize) {
		case 24: m_topologyType = D3D11_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST ;
			break;
		case 26: m_topologyType = D3D11_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST ;
			break;
		case 28: m_topologyType = D3D11_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST ;
			break;
		case 30: m_topologyType = D3D11_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST ;
			break;
		case 32: m_topologyType = D3D11_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST ;
			break;
		default: m_topologyType = D3D11_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST;
			break;
	}

	Vertex *  pVerts = new Vertex[FRAMENUM*m_VertexCount];
	const float2 * vTex = m_bzr.fetchTexcoord();
	const float3 * vTangents = m_bzr.fetchTangentSpace();

	FILE * fp = NULL;
	WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"GregoryAccTessellation11\\monsterfrog_anim.bzr" ));
	_wfopen_s(&fp, str, L"rb" );
	if (fp == NULL) return S_FALSE;
	float3 *animatedVertices = new float3[FRAMENUM*m_VertexCount];
	fread(animatedVertices, sizeof(float3), FRAMENUM*m_VertexCount, fp);
	for (int v=0; v<m_VertexCount; ++v)
	{
        float3 tmp = animatedVertices[99*m_VertexCount+v];
	}
	for (int j=0; j<FRAMENUM; j++)
	{
		for (int v=0; v<m_VertexCount; ++v)
		{

			pVerts[j*m_VertexCount+v].m_Position=animatedVertices[j*m_VertexCount+v];               //vertices position
			pVerts[j*m_VertexCount+v].m_Texcoord=vTex[v];
			pVerts[j*m_VertexCount+v].m_Tangent=vTangents[2*v];
			pVerts[j*m_VertexCount+v].m_Bitangent=vTangents[2*v+1];
		}
	}
	delete [] animatedVertices;
	// create a vertex buffer
	CreateABuffer(pd3dDevice, D3D11_BIND_VERTEX_BUFFER,  FRAMENUM*m_VertexCount*sizeof(Vertex), 
		D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC,  m_pConstructionVertexBuffer, (void *)& pVerts[0]);
	delete [] pVerts;

	// create an index buffer for regular patches
	const unsigned int* indicesReg = m_bzr.fetchRegularVertexIndices(); 
	if ( m_RegularPatchCount==0)
		CreateABuffer(pd3dDevice, D3D11_BIND_INDEX_BUFFER, sizeof(unsigned int), 0, D3D11_USAGE_DEFAULT,  m_pRegIndexBuffer, (void *)&indicesReg[0]);
	else
		CreateABuffer(pd3dDevice, D3D11_BIND_INDEX_BUFFER, 16* m_RegularPatchCount*sizeof(unsigned int), 0, D3D11_USAGE_DEFAULT,  m_pRegIndexBuffer, (void *)&indicesReg[0]);
	indicesReg=NULL;

	const unsigned int* indicesQuad = m_bzr.fetchQuadVertexIndices(); 
	CreateABuffer(pd3dDevice, D3D11_BIND_INDEX_BUFFER, m_primitiveSize* m_QuadPatchCount*sizeof(unsigned int), 0, D3D11_USAGE_DEFAULT,  m_pQuadIndexBuffer, (void *)&indicesQuad[0]);
	indicesQuad=NULL;

	const unsigned int* indicesInputMesh = m_bzr.fetchInputMeshVertexIndices(); 
	CreateABuffer(pd3dDevice, D3D11_BIND_INDEX_BUFFER, m_IndexCount*sizeof(unsigned int), 0, D3D11_USAGE_DEFAULT,  m_pInputMeshIndexBuffer, (void *)&indicesInputMesh[0]);
	indicesInputMesh=NULL;


	//create textures
	const unsigned int * quadTopologyIndex = m_bzr.fetchQuadFaceTopologyIndex();      
	const unsigned int * quadVertexCount = m_bzr.fetchQuadVertexCount();
	const unsigned int * quadFaceIndex = m_bzr.fetchQuadInputMeshVertexIndices();
	const unsigned int * quadStencilIndex = m_bzr.fetchQuadStecilIndices();


	int dataCount = (6+m_primitiveSize)*m_QuadPatchCount;
	unsigned int * textureData = new unsigned int[dataCount];
	//totpology index, vertex count, face index, stencil index
	int j=0;
	for (int i=0; i<m_QuadPatchCount; i++)
	{
		textureData[j++]=*(quadTopologyIndex+i);
	}
	for (int i=0; i<m_QuadPatchCount; i++)
	{
		textureData[j++]=*(quadVertexCount+i);
	}
	assert (j==2*m_QuadPatchCount);

	for (int i=0; i<m_QuadPatchCount; i++)
	{
		for (UINT k=0; k<4; k++)
		{
			textureData[j+k*m_QuadPatchCount+i]=*(quadFaceIndex+i*4+k);
		}
	}
	
	j = 6*m_QuadPatchCount;
	for (int i=0; i<m_QuadPatchCount; i++)
	{
		for (int k=0; k<m_primitiveSize; k++)
		{
			textureData[j+k*m_QuadPatchCount+i]=*(quadStencilIndex+i*m_primitiveSize+k);
		}
	}

	V_RETURN(CreateDataTexture( pd3dDevice, m_QuadPatchCount, 6+m_primitiveSize, DXGI_FORMAT_R32_UINT, textureData, m_QuadPatchCount*sizeof(unsigned int),
		& m_pQuadIntDataTexture, & m_pQuadIntDataTextureSRV  ));
	delete [] textureData;

	const float * pGregoryStencil = m_bzr.fetchGregoryStencil();
	//# of rows: g_faceTopologyCount *  g_primitiveSize
	//# of cols: 20
	V_RETURN( CreateDataTexture( pd3dDevice, 20, m_faceTopologyCount *  m_primitiveSize, DXGI_FORMAT_R32_FLOAT, pGregoryStencil, 20 * sizeof(float), 
		& m_pGregoryStencilTexture, & m_pGregoryStencilTextureSRV ));
	pGregoryStencil=NULL;

	const float2 * vQuadTex = m_bzr.fetchQuadTexcoord();
	V_RETURN( CreateDataTexture( pd3dDevice, 16, m_QuadPatchCount, DXGI_FORMAT_R32G32_FLOAT, vQuadTex, 16 * sizeof(float2), 
		& m_pQuadWatertightTex, & m_pQuadWatertightTexSRV ));
	vQuadTex=NULL;
	const float2 * vRegularTex = m_bzr.fetchRegularTexcoord();
	V_RETURN( CreateDataTexture( pd3dDevice, 16, m_RegularPatchCount, DXGI_FORMAT_R32G32_FLOAT, vRegularTex, 16 * sizeof(float2), 
		& m_pRegularWatertightTex, & m_pRegularWatertightTexSRV ));
	vRegularTex=NULL;

	// Setup constant buffers
	D3D11_BUFFER_DESC Desc;
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.MiscFlags = 0;

	Desc.ByteWidth = sizeof( CB_PER_PATCH_CONSTANTS );
	V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &m_pcbPerPatch ) );
	Desc.ByteWidth = sizeof( CB_PER_MESH_CONSTANTS );
	V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &m_pcbPerMesh ) );

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map( m_pcbPerPatch, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_PER_PATCH_CONSTANTS* pData = ( CB_PER_PATCH_CONSTANTS* )MappedResource.pData;
	for (int i=0; i<16*16; ++i) {
		pData->weightsForRegularPatches[i][0] = weights[i]; 
	}
	pd3dImmediateContext->Unmap( m_pcbPerPatch, 0 );

	pd3dImmediateContext->Map( m_pcbPerMesh, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_PER_MESH_CONSTANTS* pData2 = ( CB_PER_MESH_CONSTANTS* )MappedResource.pData;
    pData2->diffuse = float4(0.4f, 0.25f, 0.2f, 1.0f);
	pData2->center = float4(m_center,1);
	pd3dImmediateContext->Unmap( m_pcbPerMesh, 0 );

	return hr;
}