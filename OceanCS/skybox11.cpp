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
#include "skybox11.h"

HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut );

struct SKYBOX_VERTEX
{
    D3DXVECTOR4 pos;
};

const D3D11_INPUT_ELEMENT_DESC g_aVertexLayout[] =
{
    { "POSITION",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

CSkybox11::CSkybox11()
{
    m_pEnvironmentMap11 = NULL;
    m_pEnvironmentRV11 = NULL;
    m_pd3dDevice11 = NULL;
    m_pVertexShader = NULL;
    m_pPixelShader = NULL;
    m_pSam = NULL;
    m_pVertexLayout11 = NULL;
    m_pcbVSPerObject = NULL;
    m_pVB11 = NULL;
    m_pDepthStencilState11 = NULL;

    m_fSize = 1.0f;
}

HRESULT CSkybox11::OnD3D11CreateDevice( ID3D11Device* pd3dDevice, float fSize, 
                                        ID3D11Texture2D* pCubeTexture, ID3D11ShaderResourceView* pCubeRV )
{
    HRESULT hr;
    
    m_pd3dDevice11 = pd3dDevice;
    m_fSize = fSize;
    m_pEnvironmentMap11 = pCubeTexture;
    m_pEnvironmentRV11 = pCubeRV;

    ID3DBlob* pBlobVS = NULL;
    ID3DBlob* pBlobPS = NULL;

    // Create the shaders
    V_RETURN( CompileShaderFromFile( L"skybox11.hlsl", "SkyboxVS", "vs_4_0", &pBlobVS ) );
    V_RETURN( CompileShaderFromFile( L"skybox11.hlsl", "SkyboxPS", "ps_4_0", &pBlobPS ) );

    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &m_pVertexShader ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &m_pPixelShader ) );

    // Create an input layout
    V_RETURN( pd3dDevice->CreateInputLayout( g_aVertexLayout, 1, pBlobVS->GetBufferPointer(),
                                             pBlobVS->GetBufferSize(), &m_pVertexLayout11 ) );

    SAFE_RELEASE( pBlobVS );
    SAFE_RELEASE( pBlobPS );

    // Query support for linear filtering on DXGI_FORMAT_R32G32B32A32
    UINT FormatSupport = 0;
    V_RETURN( pd3dDevice->CheckFormatSupport( DXGI_FORMAT_R32G32B32A32_FLOAT, &FormatSupport ) );

    // Setup linear or point sampler according to the format Query result
    D3D11_SAMPLER_DESC SamDesc;
    SamDesc.Filter = ( FormatSupport & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE ) > 0 ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.MipLODBias = 0.0f;
    SamDesc.MaxAnisotropy = 1;
    SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
    SamDesc.MinLOD = 0;
    SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &m_pSam ) );  

    // Setup constant buffer
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;
    Desc.ByteWidth = sizeof( CB_VS_PER_OBJECT );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &m_pcbVSPerObject ) );
    
    // Depth stencil state
    D3D11_DEPTH_STENCIL_DESC DSDesc;
    ZeroMemory( &DSDesc, sizeof( D3D11_DEPTH_STENCIL_DESC ) );
    DSDesc.DepthEnable = FALSE;
    DSDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    DSDesc.DepthFunc = D3D11_COMPARISON_LESS;
    DSDesc.StencilEnable = FALSE;
    V_RETURN( pd3dDevice->CreateDepthStencilState( &DSDesc, &m_pDepthStencilState11 ) );

    return S_OK;
}

void CSkybox11::OnD3D11ResizedSwapChain( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
{
    HRESULT hr;

    if ( m_pd3dDevice11 == NULL )
        return;

    // Fill the vertex buffer
    SKYBOX_VERTEX* pVertex = new SKYBOX_VERTEX[4];
    if ( !pVertex )
        return;

    // Map texels to pixels 
    float fHighW = -1.0f - ( 1.0f / ( float )pBackBufferSurfaceDesc->Width );
    float fHighH = -1.0f - ( 1.0f / ( float )pBackBufferSurfaceDesc->Height );
    float fLowW = 1.0f + ( 1.0f / ( float )pBackBufferSurfaceDesc->Width );
    float fLowH = 1.0f + ( 1.0f / ( float )pBackBufferSurfaceDesc->Height );
    
    pVertex[0].pos = D3DXVECTOR4( fLowW, fLowH, 1.0f, 1.0f );
    pVertex[1].pos = D3DXVECTOR4( fLowW, fHighH, 1.0f, 1.0f );
    pVertex[2].pos = D3DXVECTOR4( fHighW, fLowH, 1.0f, 1.0f );
    pVertex[3].pos = D3DXVECTOR4( fHighW, fHighH, 1.0f, 1.0f );

    UINT uiVertBufSize = 4 * sizeof( SKYBOX_VERTEX );
    //Vertex Buffer
    D3D11_BUFFER_DESC vbdesc;
    vbdesc.ByteWidth = uiVertBufSize;
    vbdesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbdesc.CPUAccessFlags = 0;
    vbdesc.MiscFlags = 0;    

    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVertex;    
    V( m_pd3dDevice11->CreateBuffer( &vbdesc, &InitData, &m_pVB11 ) );
    SAFE_DELETE_ARRAY( pVertex ); 
}

void CSkybox11::D3D11Render( D3DXMATRIX* pmWorldViewProj, ID3D11DeviceContext* pd3dImmediateContext )
{
    HRESULT hr;
    
    pd3dImmediateContext->IASetInputLayout( m_pVertexLayout11 );

    UINT uStrides = sizeof( SKYBOX_VERTEX );
    UINT uOffsets = 0;
    ID3D11Buffer* pBuffers[1] = { m_pVB11 };
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, pBuffers, &uStrides, &uOffsets );
    pd3dImmediateContext->IASetIndexBuffer( NULL, DXGI_FORMAT_R32_UINT, 0 );
    pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    pd3dImmediateContext->VSSetShader( m_pVertexShader, NULL, 0 );
    pd3dImmediateContext->PSSetShader( m_pPixelShader, NULL, 0 );

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( m_pcbVSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    CB_VS_PER_OBJECT* pVSPerObject = ( CB_VS_PER_OBJECT* )MappedResource.pData;  
    D3DXMatrixInverse( &pVSPerObject->m_WorldViewProj, NULL, pmWorldViewProj );
    pd3dImmediateContext->Unmap( m_pcbVSPerObject, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &m_pcbVSPerObject );

    pd3dImmediateContext->PSSetSamplers( 0, 1, &m_pSam );
    pd3dImmediateContext->PSSetShaderResources( 0, 1, &m_pEnvironmentRV11 );

    ID3D11DepthStencilState* pDepthStencilStateStored11 = NULL;
    UINT StencilRef;
    pd3dImmediateContext->OMGetDepthStencilState( &pDepthStencilStateStored11, &StencilRef );
    pd3dImmediateContext->OMSetDepthStencilState( m_pDepthStencilState11, 0 );

    pd3dImmediateContext->Draw( 4, 0 );

    pd3dImmediateContext->OMSetDepthStencilState( pDepthStencilStateStored11, StencilRef );
}

void CSkybox11::OnD3D11ReleasingSwapChain()
{
    SAFE_RELEASE( m_pVB11 );
}

void CSkybox11::OnD3D11DestroyDevice()
{
    m_pd3dDevice11 = NULL;
    SAFE_RELEASE( m_pEnvironmentMap11 );
    SAFE_RELEASE( m_pEnvironmentRV11 );
    SAFE_RELEASE( m_pSam );
    SAFE_RELEASE( m_pVertexShader );
    SAFE_RELEASE( m_pPixelShader );
    SAFE_RELEASE( m_pVertexLayout11 );
    SAFE_RELEASE( m_pcbVSPerObject );
    SAFE_RELEASE( m_pDepthStencilState11 );
}