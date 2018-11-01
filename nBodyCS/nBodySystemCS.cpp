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

#include <d3dx11tex.h>

#include "nBodySystemCS.h"

#define MAX_PATH_STR 512

// constant buffer for rendering particles
struct CB_DRAW
{
	D3DXMATRIX   g_mWorldViewProjection;
	float		 g_fPointSize;
    unsigned int g_readOffset;
    float        dummy[2];
};

// constant buffer for the simulation update compute shader
struct CB_UPDATE
{
	float g_timestep;
    float g_softeningSquared;
	unsigned int g_numParticles;
    unsigned int g_readOffset;
    unsigned int g_writeOffset;
    float dummy[3];
};


struct CB_IMMUTABLE
{
	D3DXVECTOR4 g_positions[4];
	D3DXVECTOR4 g_texcoords[4];
};

CB_IMMUTABLE cbImmutable =
{
	D3DXVECTOR4(0.5f, -0.5f, 0, 0), D3DXVECTOR4(0.5f, 0.5f, 0, 0), 
    D3DXVECTOR4(-0.5f, -0.5f, 0, 0), D3DXVECTOR4(-0.5f,  0.5f, 0, 0),
	D3DXVECTOR4(1, 0, 0, 0), D3DXVECTOR4(1, 1, 0, 0), D3DXVECTOR4(0, 0, 0, 0), D3DXVECTOR4(0, 1, 0, 0),
};

// Input layout definition
D3D11_INPUT_ELEMENT_DESC indLayout[] =
{
	{ "POSINDEX", 0, DXGI_FORMAT_R32_UINT,	   0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },	
};

FLOAT blendFactors[] = {1.0f, 1.0f, 1.0f, 1.0f};

//--------------------------------------------------------------------------------------
// Helper function to compile an hlsl shader from file, 
// its binary compiled code is returned
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3D10Blob** ppBlobOut )
{
	HRESULT hr = S_OK;

	// find the file
	WCHAR str[MAX_PATH];
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

	// open the file
	HANDLE hFile = CreateFile( str, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_FLAG_SEQUENTIAL_SCAN, NULL );
	if( INVALID_HANDLE_VALUE == hFile )
		return E_FAIL;

	// Get the file size
	LARGE_INTEGER FileSize;
	GetFileSizeEx( hFile, &FileSize );

	// create enough space for the file data
	LPCSTR pFileData = new char[ FileSize.LowPart ];
	if( !pFileData )
		return E_OUTOFMEMORY;

	// read the data in
	DWORD BytesRead;
	if( !ReadFile( hFile, (LPVOID)pFileData, FileSize.LowPart, &BytesRead, NULL ) )
		return E_FAIL; 

	CloseHandle( hFile );

	// Compile the shader
	char pFilePathName[MAX_PATH];        
	WideCharToMultiByte(CP_ACP, 0, str, -1, pFilePathName, MAX_PATH, NULL, NULL);
	ID3D10Blob* pErrorBlob;
	hr = D3DCompile( pFileData, FileSize.LowPart, pFilePathName, NULL, NULL, szEntryPoint, szShaderModel, D3D10_SHADER_ENABLE_STRICTNESS, 0, ppBlobOut, &pErrorBlob );

	delete []pFileData;

	if( FAILED(hr) )
	{
		OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
		SAFE_RELEASE( pErrorBlob );
		return hr;
	}
	SAFE_RELEASE( pErrorBlob );

	return S_OK;
}

//-----------------------------------------------------------------------------
// Helper function to load a compute shader
//-----------------------------------------------------------------------------
HRESULT LoadComputeShader(ID3D11Device* pd3dDevice, WCHAR *szFilename, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3D11ComputeShader **ppComputeShader)
{
	HRESULT hr;
	ID3DBlob* pBlob = NULL;
	V_RETURN( CompileShaderFromFile( szFilename, szEntryPoint, "cs_4_0", &pBlob ) );
	V_RETURN( pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, ppComputeShader ) );
	SAFE_RELEASE( pBlob );
	return hr;
}


//-----------------------------------------------------------------------------
// Default Constructor
//-----------------------------------------------------------------------------
NBodySystemCS::NBodySystemCS()
{
	m_pIndLayout         = NULL;

	// The device
	m_pd3dDevice = NULL;
		
	// Particle texture and resource views
	m_pParticleTex    = NULL; 
    m_pParticleTexSRV = NULL;

	m_pVSDisplayParticleStructBuffer = NULL;
	m_pGSDisplayParticle       = NULL;
	m_pPSDisplayParticle       = NULL;
	m_pPSDisplayParticleTex    = NULL;

	m_pcbDraw                  = NULL;
	m_pcbUpdate                = NULL;
	m_pcbImmutable             = NULL;

	m_pParticleSamplerState    = NULL;
	m_pRasterizerState         = NULL;

    m_pStructuredBuffer        = NULL;
    m_pStructuredBufferSRV     = NULL;
    m_pStructuredBufferUAV     = NULL;

    m_readBuffer = 0;
}

//-----------------------------------------------------------------------------
// Render the bodies as particles using sprites
//-----------------------------------------------------------------------------
HRESULT NBodySystemCS::renderBodies(const D3DXMATRIX *p_mWorld, const D3DXMATRIX *p_mView, const D3DXMATRIX *p_mProj)
{
    HRESULT hr = S_OK;

	unsigned int particleCount = 0;

	D3DXMATRIX  mWorldViewProjection;
	D3DXMATRIX  mWorldView;
	D3DXMATRIX  mProjection;

	mProjection = *p_mProj;
	
	D3DXMatrixMultiply( &mWorldView, p_mWorld, p_mView );
	D3DXMatrixMultiply( &mWorldViewProjection, &mWorldView, p_mProj );
	
	// Set the input layout
	m_pd3dImmediateContext->IASetInputLayout( m_pIndLayout );

	m_pd3dImmediateContext->IASetIndexBuffer( NULL, DXGI_FORMAT_UNKNOWN, 0);

	D3DXMatrixTranspose(&mWorldViewProjection, &mWorldViewProjection);

	// constant buffers
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	V( m_pd3dImmediateContext->Map(m_pcbDraw, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource ) );
	CB_DRAW* pcbDraw = (CB_DRAW*) mappedResource.pData;
	pcbDraw->g_mWorldViewProjection = mWorldViewProjection;
	pcbDraw->g_fPointSize = m_fPointSize;
    pcbDraw->g_readOffset = m_readBuffer * m_numBodies;
	m_pd3dImmediateContext->Unmap(m_pcbDraw, 0);

	particleCount = m_numBodies;
	m_pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	
	m_pd3dImmediateContext->PSSetShaderResources( 0, 1, &m_pParticleTexSRV );
	m_pd3dImmediateContext->PSSetSamplers(0, 1, &m_pParticleSamplerState);

    m_pd3dImmediateContext->VSSetShaderResources(0, 1, &m_pStructuredBufferSRV);
	
	m_pd3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pcbDraw );
	m_pd3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pcbImmutable);
	m_pd3dImmediateContext->GSSetConstantBuffers(0, 1, &m_pcbImmutable );
	m_pd3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pcbDraw );

	// Set shaders
	m_pd3dImmediateContext->GSSetShader( m_pGSDisplayParticle, NULL, 0 );
	m_pd3dImmediateContext->VSSetShader( m_pVSDisplayParticleStructBuffer, NULL, 0 );
	m_pd3dImmediateContext->PSSetShader( m_pPSDisplayParticleTex, NULL, 0 );

	m_pd3dImmediateContext->RSSetState(m_pRasterizerState);

	// Set Blending State
    m_pd3dImmediateContext->OMSetBlendState(m_pBlendingEnabled, blendFactors, 0xFFFFFFFF);

	// Set Depth State
	m_pd3dImmediateContext->OMSetDepthStencilState(m_pDepthDisabled,0);

	m_pd3dImmediateContext->Draw(particleCount,  0);

	// Unbound all PS shader resources
	ID3D11ShaderResourceView *const pSRV[4] = { NULL, NULL, NULL, NULL };
	m_pd3dImmediateContext->PSSetShaderResources( 0, 4, pSRV );
	m_pd3dImmediateContext->VSSetShaderResources( 0, 4, pSRV );
	ID3D11Buffer *const pCB[4] = { NULL, NULL, NULL, NULL };
	m_pd3dImmediateContext->PSSetConstantBuffers(0, 4, pCB );
	m_pd3dImmediateContext->GSSetConstantBuffers(0, 4, pCB );

    return hr;   
}

//-----------------------------------------------------------------------------
// Update the positions and velocities of all bodies in the system
// by invoking a Compute Shader that computes the gravitational force
// on all bodies caused by all other bodies in the system.  This computation
// is highly parallel and thus well-suited to DirectCompute shaders.
//-----------------------------------------------------------------------------
HRESULT NBodySystemCS::updateBodies(float dt)
{
    HRESULT hr = S_OK;

	// Set the time step
	dt = 0.016f;//min(0.166f, max(dt, 0.033f));

	// constant buffers
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	V( m_pd3dImmediateContext->Map( m_pcbUpdate, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource ) );
	CB_UPDATE* pcbUpdate = (CB_UPDATE*)mappedResource.pData;
	pcbUpdate->g_timestep = dt;
    pcbUpdate->g_softeningSquared = 0.01f;
	pcbUpdate->g_numParticles = m_numBodies;
    pcbUpdate->g_readOffset = m_readBuffer * m_numBodies;
    pcbUpdate->g_writeOffset = (1 - m_readBuffer) * m_numBodies;
	m_pd3dImmediateContext->Unmap(m_pcbUpdate, 0);

	UINT initCounts = 0;
	m_pd3dImmediateContext->CSSetShader( m_pCSUpdatePositionAndVelocity, NULL, 0 );
	m_pd3dImmediateContext->CSSetConstantBuffers( 0, 1, &m_pcbUpdate );
	m_pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, &m_pStructuredBufferUAV, &initCounts ); // CS output

	// Run the CS
	m_pd3dImmediateContext->Dispatch( m_numBodies / 256, 1, 1 );

	// Unbind resources for CS
	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { NULL };
	m_pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, ppUAViewNULL, &initCounts );
	ID3D11ShaderResourceView* ppSRVNULL[1] = { NULL };
	m_pd3dImmediateContext->CSSetShaderResources( 0, 1, ppSRVNULL );

    m_readBuffer = 1 - m_readBuffer;

    return hr;
}

//-----------------------------------------------------------------------------
// Reset the body system to its initial configuration
//-----------------------------------------------------------------------------
HRESULT NBodySystemCS::resetBodies(BodyData configData)
{
    HRESULT hr = S_OK;
    
	m_numBodies = configData.nBodies;

	// for compute shader on CS_4_0, we can only have a single UAV per shader, so we have to store particle
	// position and velocity in the same array: all positions followed by all velocities
	D3DXVECTOR4 *particleArray = new D3DXVECTOR4[m_numBodies * 3];

	for(unsigned int i=0; i < m_numBodies; i++){
		particleArray[i] = D3DXVECTOR4(configData.position[i*3 + 0], 
						 			   configData.position[i*3 + 1], 
									   configData.position[i*3 + 2], 
									   1.0);
		particleArray[i + m_numBodies] = particleArray[i];
        particleArray[i + 2 * m_numBodies] = D3DXVECTOR4(configData.velocity[i*3 + 0], 
                                                            configData.velocity[i*3 + 1], 
												            configData.velocity[i*3 + 2], 
												            1.0);
	}

	D3D11_SUBRESOURCE_DATA initData = { particleArray, 0, 0 };

    SAFE_RELEASE( m_pStructuredBuffer );
    SAFE_RELEASE( m_pStructuredBufferSRV );
    SAFE_RELEASE( m_pStructuredBufferUAV );

	// Create Structured Buffer
	D3D11_BUFFER_DESC sbDesc;
	sbDesc.BindFlags            = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	sbDesc.CPUAccessFlags       = 0;
	sbDesc.MiscFlags            = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	sbDesc.StructureByteStride  = sizeof(D3DXVECTOR4);
	sbDesc.ByteWidth            = sizeof(D3DXVECTOR4) * m_numBodies * 3;
	sbDesc.Usage                = D3D11_USAGE_DEFAULT;
	V_RETURN( m_pd3dDevice->CreateBuffer(&sbDesc, &initData, &m_pStructuredBuffer) );

	// create the Shader Resource View (SRV) for the structured buffer
	D3D11_SHADER_RESOURCE_VIEW_DESC sbSRVDesc;
	sbSRVDesc.Buffer.ElementOffset          = 0;
	sbSRVDesc.Buffer.ElementWidth           = sizeof(D3DXVECTOR4);
	sbSRVDesc.Buffer.FirstElement           = 0;
	sbSRVDesc.Buffer.NumElements            = m_numBodies * 3;
	sbSRVDesc.Format                        = DXGI_FORMAT_UNKNOWN;
	sbSRVDesc.ViewDimension                 = D3D11_SRV_DIMENSION_BUFFER;
	V_RETURN( m_pd3dDevice->CreateShaderResourceView(m_pStructuredBuffer, &sbSRVDesc, &m_pStructuredBufferSRV) );

	// create the UAV for the structured buffer
	D3D11_UNORDERED_ACCESS_VIEW_DESC sbUAVDesc;
	sbUAVDesc.Buffer.FirstElement       = 0;
	sbUAVDesc.Buffer.Flags              = 0;
	sbUAVDesc.Buffer.NumElements        = m_numBodies * 3;
	sbUAVDesc.Format                    = DXGI_FORMAT_UNKNOWN;
	sbUAVDesc.ViewDimension             = D3D11_UAV_DIMENSION_BUFFER;
	V_RETURN( m_pd3dDevice->CreateUnorderedAccessView(m_pStructuredBuffer, &sbUAVDesc, &m_pStructuredBufferUAV) );

	delete [] particleArray;
		
	return hr;
}

//-----------------------------------------------------------------------------
// Handle device creation (create shaders and resources)
//-----------------------------------------------------------------------------
HRESULT NBodySystemCS::onCreateDevice(ID3D11Device *pd3dDevice, ID3D11DeviceContext *pd3dImmediateContext)
{
    HRESULT hr = S_OK;

	WCHAR str[MAX_PATH_STR+1];
	
	m_pd3dDevice = pd3dDevice;
	m_pd3dImmediateContext = pd3dImmediateContext;

	// Create shaders
	// VS
	ID3D10Blob* pBlobVS = NULL;
    V_RETURN( CompileShaderFromFile( L"RenderParticles.hlsl", "DisplayVS_StructBuffer", "vs_4_0", &pBlobVS ) );
    V_RETURN( m_pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &m_pVSDisplayParticleStructBuffer) );

	// Create vertex layout
	V_RETURN( pd3dDevice->CreateInputLayout( indLayout, 1, pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), &m_pIndLayout ) );
	SAFE_RELEASE( pBlobVS );

	// GS
	ID3D10Blob* pBlobGS = NULL;
	V_RETURN( CompileShaderFromFile( L"RenderParticles.hlsl", "DisplayGS", "gs_4_0", &pBlobGS ) );
	V_RETURN( pd3dDevice->CreateGeometryShader( pBlobGS->GetBufferPointer(), pBlobGS->GetBufferSize(), NULL, &m_pGSDisplayParticle ) );
	SAFE_RELEASE ( pBlobGS );

	// PS
	ID3D10Blob* pBlobPS = NULL;
	V_RETURN( CompileShaderFromFile( L"RenderParticles.hlsl", "DisplayPS", "ps_4_0", &pBlobPS ) );
	V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &m_pPSDisplayParticle ) );
	SAFE_RELEASE ( pBlobPS );

	V_RETURN( CompileShaderFromFile( L"RenderParticles.hlsl", "DisplayPSTex", "ps_4_0", &pBlobPS ) );
	V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &m_pPSDisplayParticleTex ) );
	SAFE_RELEASE ( pBlobPS );
	
	// Update CS
	V_RETURN( LoadComputeShader(pd3dDevice, L"nBodyCS.hlsl", "NBodyUpdate", "cs_4_0", &m_pCSUpdatePositionAndVelocity) );
	
	// Create constant buffers
	D3D11_BUFFER_DESC cbDesc;
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.MiscFlags = 0;
	cbDesc.ByteWidth = sizeof( CB_DRAW );
	V_RETURN( pd3dDevice->CreateBuffer( &cbDesc, NULL, &m_pcbDraw ) );

	cbDesc.ByteWidth = sizeof( CB_UPDATE );
	V_RETURN( pd3dDevice->CreateBuffer( &cbDesc, NULL, &m_pcbUpdate ) );
	
	cbDesc.Usage = D3D11_USAGE_IMMUTABLE;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = 0;
	cbDesc.ByteWidth = sizeof(CB_IMMUTABLE);
	D3D11_SUBRESOURCE_DATA initData = { &cbImmutable, 0, 0 };
	V_RETURN( pd3dDevice->CreateBuffer(&cbDesc, &initData, &m_pcbImmutable ) );

	// sampler state
	D3D11_SAMPLER_DESC sDesc;
	sDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sDesc.MinLOD = 0;
	sDesc.MaxLOD = 8;
	sDesc.MipLODBias = 0;
	sDesc.MaxAnisotropy = 1;
	V_RETURN( m_pd3dDevice->CreateSamplerState(&sDesc, &m_pParticleSamplerState) );

	// rasterizer state
    D3D11_RASTERIZER_DESC rsDesc;
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE;
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthBias = 0;
    rsDesc.DepthBiasClamp = 0.0f;
    rsDesc.SlopeScaledDepthBias = 0.0f;
    rsDesc.DepthClipEnable = TRUE;
    rsDesc.ScissorEnable =	FALSE;
    rsDesc.MultisampleEnable = FALSE;
    rsDesc.AntialiasedLineEnable = FALSE;
	V_RETURN( m_pd3dDevice->CreateRasterizerState(&rsDesc, &m_pRasterizerState) );
  
	// Load the particle texture
	D3DX11_IMAGE_LOAD_INFO itex;
	D3DX11_IMAGE_INFO      pSrcInfo;

    DXUTFindDXSDKMediaFileCch( str, MAX_PATH_STR, L"pointsprite_grey.dds" );
	hr = D3DX11GetImageInfoFromFile(str, NULL, &pSrcInfo, NULL);
 
	itex.Width          = pSrcInfo.Width;
	itex.Height         = pSrcInfo.Height;
	itex.Depth          = pSrcInfo.Depth;
    itex.FirstMipLevel  = 0;
	itex.MipLevels      = 8;
    itex.Usage          = D3D11_USAGE_DEFAULT;
    itex.BindFlags      = D3D11_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
    itex.CpuAccessFlags = 0;
    itex.MiscFlags      = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    itex.Format         = DXGI_FORMAT_R32_FLOAT;
    itex.Filter         = D3DX11_FILTER_LINEAR;
    itex.MipFilter      = D3DX11_FILTER_LINEAR;
    itex.pSrcInfo       = &pSrcInfo;

	ID3D11Resource *pRes = NULL;
    hr = D3DX11CreateTextureFromFile(m_pd3dDevice, str, &itex, NULL, &pRes, NULL);
	if( FAILED( hr ) ){ 
		MessageBox(NULL, L"Unable to create texture from file!!!", L"ERROR", MB_OK|MB_SETFOREGROUND|MB_TOPMOST);		
		return FALSE;
	}
	
	hr = pRes->QueryInterface( __uuidof( ID3D11Texture2D ), (LPVOID*)&m_pParticleTex );
    pRes->Release();

	D3D11_TEXTURE2D_DESC desc;
	m_pParticleTex->GetDesc(&desc);


	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
  
	SRVDesc.Format                    = itex.Format;
	SRVDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels       = itex.MipLevels;
    SRVDesc.Texture2D.MostDetailedMip = 0;
    
	hr |= m_pd3dDevice->CreateShaderResourceView( m_pParticleTex, &SRVDesc, &m_pParticleTexSRV );
	
	m_pd3dImmediateContext->GenerateMips(m_pParticleTexSRV);

	// Create the blending states
	D3D11_BLEND_DESC tmpBlendState;

	tmpBlendState.AlphaToCoverageEnable = false;
	tmpBlendState.IndependentBlendEnable = false;

	for(UINT i = 0; i < 8; i++){
		tmpBlendState.RenderTarget[i].BlendEnable = false;
		tmpBlendState.RenderTarget[i].SrcBlend       = D3D11_BLEND_ONE;
		tmpBlendState.RenderTarget[i].DestBlend      = D3D11_BLEND_ONE;
		tmpBlendState.RenderTarget[i].BlendOp        = D3D11_BLEND_OP_ADD;
		tmpBlendState.RenderTarget[i].SrcBlendAlpha  = D3D11_BLEND_ZERO;
		tmpBlendState.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
		tmpBlendState.RenderTarget[i].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
		tmpBlendState.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	}  

	tmpBlendState.RenderTarget[0].BlendEnable = true;

	hr = m_pd3dDevice->CreateBlendState( &tmpBlendState, &m_pBlendingEnabled);

	// Create the depth/stencil states
	D3D11_DEPTH_STENCIL_DESC tmpDsDesc;
    tmpDsDesc.DepthEnable    = false;
    tmpDsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    tmpDsDesc.DepthFunc      = D3D11_COMPARISON_LESS;
    tmpDsDesc.StencilEnable  = FALSE;

	hr = m_pd3dDevice->CreateDepthStencilState( &tmpDsDesc, &m_pDepthDisabled);
     
    return hr;
}

//-----------------------------------------------------------------------------
// Handle device destruction
//-----------------------------------------------------------------------------
void NBodySystemCS::onDestroyDevice()
{
	SAFE_RELEASE( m_pIndLayout );
	
	SAFE_RELEASE( m_pBlendingEnabled );
	SAFE_RELEASE( m_pDepthDisabled );
	SAFE_RELEASE( m_pParticleSamplerState );
	SAFE_RELEASE( m_pRasterizerState );

	// Particle texture and resource views
	SAFE_RELEASE( m_pParticleTex ); 
    SAFE_RELEASE( m_pParticleTexSRV );
	
	// shaders
    SAFE_RELEASE( m_pVSDisplayParticleStructBuffer );
	SAFE_RELEASE( m_pGSDisplayParticle );
	SAFE_RELEASE( m_pPSDisplayParticle );
	SAFE_RELEASE( m_pPSDisplayParticleTex );
	SAFE_RELEASE( m_pCSUpdatePositionAndVelocity );

	// constant buffers
	SAFE_RELEASE( m_pcbDraw );
	SAFE_RELEASE( m_pcbUpdate );
	SAFE_RELEASE( m_pcbImmutable );

	// structured buffer
	SAFE_RELEASE( m_pStructuredBuffer );
	SAFE_RELEASE( m_pStructuredBufferSRV );
	SAFE_RELEASE( m_pStructuredBufferUAV );
}

//--------------------------------------------------------------------------------------
// Handle resize
//--------------------------------------------------------------------------------------
void NBodySystemCS::onResizedSwapChain(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC *pBackBufferSurfaceDesc)
{
	// Update the aspect ratio
	m_fAspectRatio = pBackBufferSurfaceDesc->Width * 1.0f / pBackBufferSurfaceDesc->Height; 
}
