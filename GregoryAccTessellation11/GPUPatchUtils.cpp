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
#include "GPUPatchUtils.h"
#include <assert.h>

void CreateABuffer( ID3D11Device* pd3dDevice, UINT bindFlags, UINT width, UINT cpuAccessFlag, 
                      D3D11_USAGE usage, ID3D11Buffer* &pBuffer, void* pData)
{
    HRESULT hr = S_OK;

    D3D11_BUFFER_DESC vbDesc    = {0};
    vbDesc.BindFlags            = bindFlags; // Type of resource
    vbDesc.ByteWidth            = width;       // size in bytes
    vbDesc.CPUAccessFlags       = cpuAccessFlag;     // D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ
    vbDesc.Usage                = usage;    // D3D11_USAGE_DEFAULT, D3D11_USAGE_IMMUTABLE, D3D11_USAGE_DYNAMIC, D3D11_USAGE_STAGING

    D3D11_SUBRESOURCE_DATA vbData = {0};
    vbData.pSysMem = pData;

    hr = pd3dDevice->CreateBuffer( &vbDesc, &vbData, &pBuffer );
    assert( SUCCEEDED( hr ) );
}

HRESULT CreateABufferWithView( ID3D11Device* pd3dDevice, UINT bindFlags, UINT width,  
                                       ID3D11Buffer* &pBuffer, void* pData, UINT elementWidth, ID3D11ShaderResourceView* &pBufferRV)
{
    HRESULT hr = S_OK;

    D3D11_BUFFER_DESC vbDesc    = {0};
    vbDesc.BindFlags            = bindFlags; // Type of resource
    vbDesc.ByteWidth            = width;       // size in bytes
    vbDesc.CPUAccessFlags       = 0;                        // No CPU access required
    vbDesc.Usage                = D3D11_USAGE_DEFAULT;    // Can't be changed after creation

    D3D11_SUBRESOURCE_DATA vbData = {0};
    vbData.pSysMem = pData;

    hr = pd3dDevice->CreateBuffer( &vbDesc, &vbData, &pBuffer );

    if (SUCCEEDED( hr ) )
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
        ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
        SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        SRVDesc.Buffer.ElementOffset = 0;
        SRVDesc.Buffer.ElementWidth = elementWidth;
        hr = pd3dDevice->CreateShaderResourceView( pBuffer, &SRVDesc, &pBufferRV );
    }

    return hr;
}

HRESULT CreateDataTexture( ID3D11Device* pd3dDevice, UINT width, UINT height, DXGI_FORMAT format, const void* pData, UINT size, ID3D11Texture2D** out_texture, ID3D11ShaderResourceView** out_ppRV)
{
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC desc;
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;

    hr = pd3dDevice->CreateTexture2D( &desc, NULL, out_texture );
    assert( SUCCEEDED( hr ) );

    ID3D11Texture2D* pTexture = *out_texture;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    // Map the texture
    D3D11_MAPPED_SUBRESOURCE mappedTex={0};
    if( SUCCEEDED( hr ) )
    {
        hr = pd3dImmediateContext->Map( pTexture, 0,  D3D11_MAP_WRITE_DISCARD, 0, &mappedTex );
        assert( mappedTex.RowPitch >= size );
    }

    // Copy the data and unmap the texture
    if( SUCCEEDED( hr ) )
    {
        BYTE* pSrc = (BYTE*)pData;
        BYTE* pDest = (BYTE*)mappedTex.pData;
 
        for( UINT i = 0; i < height; ++i )
        {
            memcpy( pDest, pSrc, size );
            pSrc += size;
            pDest += mappedTex.RowPitch;
        }

        pd3dImmediateContext->Unmap(pTexture, 0);
    }

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	SRVDesc.Format = desc.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;
	SRVDesc.Texture2D.MostDetailedMip = 0;
    hr = pd3dDevice->CreateShaderResourceView( pTexture, &SRVDesc, out_ppRV );
    assert( SUCCEEDED( hr ) );

    return hr;
}






 


