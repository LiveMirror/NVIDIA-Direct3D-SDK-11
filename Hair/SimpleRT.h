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

class SimpleRT 
{
public:
	ID3D11Texture2D* pTexture;
	ID3D11RenderTargetView* pRTV;
	ID3D11ShaderResourceView* pSRV;

	SimpleRT( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, D3D11_TEXTURE2D_DESC* pTexDesc )
	{
		HRESULT hr;

		//These have to be set to have a render target
		pTexDesc->BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		pTexDesc->Usage = D3D11_USAGE_DEFAULT;
		pTexDesc->CPUAccessFlags = NULL;

		V( pd3dDevice->CreateTexture2D( pTexDesc, NULL, &pTexture ) );
		V( pd3dDevice->CreateShaderResourceView( pTexture, NULL, &pSRV ) );
		V( pd3dDevice->CreateRenderTargetView( pTexture, NULL, &pRTV ) );
	}

	~SimpleRT()
	{
		SAFE_RELEASE( pTexture );
		SAFE_RELEASE( pRTV );
		SAFE_RELEASE( pSRV );
	}

	operator ID3D11Texture2D*()
	{
		return pTexture;
	}

	operator ID3D11ShaderResourceView*()
	{
		return pSRV;
	}

	operator ID3D11RenderTargetView*()
	{
		return pRTV;
	}
};

class SimpleArrayRT : public SimpleRT
{
public:
	ID3D11RenderTargetView** ppRTVs;
	UINT iArraySize;

	SimpleArrayRT( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, D3D11_TEXTURE2D_DESC* pTexDesc )
		: SimpleRT( pd3dDevice, pd3dContext, pTexDesc )
	{
		HRESULT hr;

		D3D11_RENDER_TARGET_VIEW_DESC layerViewDesc;
		pRTV->GetDesc( &layerViewDesc );

		iArraySize = pTexDesc->ArraySize;
		ppRTVs = new ID3D11RenderTargetView*[iArraySize];
		switch (layerViewDesc.ViewDimension) {
			case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
				layerViewDesc.Texture2DArray.ArraySize = 1;
				for( UINT i = 0; i < iArraySize; ++i )
				{
					layerViewDesc.Texture2DArray.FirstArraySlice = i;
					V( pd3dDevice->CreateRenderTargetView( pTexture, &layerViewDesc, &ppRTVs[i] ) );
				}
				break;
			case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
				layerViewDesc.Texture2DMSArray.ArraySize = 1;
				for( UINT i = 0; i < iArraySize; ++i )
				{
					layerViewDesc.Texture2DMSArray.FirstArraySlice = i;
					V( pd3dDevice->CreateRenderTargetView( pTexture, &layerViewDesc, &ppRTVs[i] ) );
				}
				break;
		}
	}

	~SimpleArrayRT()
	{
		for (UINT i = 0; i < iArraySize; i++) {
			SAFE_RELEASE( ppRTVs[i] );
		}
		SAFE_DELETE(ppRTVs);
	}
};

class DepthRT 
{
public:
	ID3D11Texture2D* pTexture;
	ID3D11DepthStencilView*	pDSV;
	ID3D11ShaderResourceView* pSRV;

	DepthRT( ID3D11Device* pd3dDevice, D3D11_TEXTURE2D_DESC* pTexDesc )
	{
		HRESULT hr;
		V( pd3dDevice->CreateTexture2D( pTexDesc, NULL, &pTexture ) );

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		dsvDesc.Texture2D.MipSlice = 0;
		dsvDesc.Flags = 0;
		V( pd3dDevice->CreateDepthStencilView( pTexture, &dsvDesc, &pDSV ) );

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
		V( pd3dDevice->CreateShaderResourceView( pTexture, &srvDesc, &pSRV ) );
	}

	~DepthRT()
	{
		SAFE_RELEASE( pTexture );
		SAFE_RELEASE( pDSV );
		SAFE_RELEASE( pSRV );
	}

	operator ID3D11Texture2D*()
	{
		return pTexture;
	}

	operator ID3D11DepthStencilView*()
	{
		return pDSV;
	}

	operator ID3D11ShaderResourceView*()
	{
		return pSRV;
	}
};
