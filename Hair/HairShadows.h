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

#include "SimpleRT.h"

#define LIGHT_RES 1024

extern ID3DX11Effect* g_Effect;

class HairShadows
{
	D3D11_VIEWPORT			m_LViewport;
	CModelViewerCamera		m_LCamera;
	SimpleRT*				m_pDepthsRT;
	DepthRT*				m_pDepthsDS;

	//extra variables for DOM
	SimpleRT*          m_DOM;
	SimpleRT*		   m_pDepthsRT_DOM;
	//TEMP SARAH! SimpleArrayRT*          m_DOM; //if you need more than one RT
	float m_Zi[ARRAY_SIZE*4+1];
	static int m_NumLayers;
	bool m_useDOM;

	D3DXMATRIX mWorldI, mWorldIT;
	D3DXMATRIX mWorldView, mWorldViewI, mWorldViewIT;
	D3DXMATRIX mWorldViewProj, mWorldViewProjI;
	D3DXVECTOR3 vLightDirWorld, vLightCenterWorld;

public:
	HairShadows() {
		m_LViewport.Width  = LIGHT_RES;
		m_LViewport.Height = LIGHT_RES;
		m_LViewport.MinDepth = 0;
		m_LViewport.MaxDepth = 1;
		m_LViewport.TopLeftX = 0;
		m_LViewport.TopLeftY = 0;

		m_pDepthsRT = NULL;
		m_pDepthsDS = NULL;
		m_pDepthsRT_DOM = NULL;

		m_useDOM = false;
	}

	void OnD3D11CreateDevice(ID3D11Device* pd3dDevice, ID3D11DeviceContext *pd3dContext) {


		D3D11_TEXTURE2D_DESC texDesc;
		texDesc.Width = LIGHT_RES;
		texDesc.Height = LIGHT_RES;
		texDesc.ArraySize = 1;
		texDesc.MiscFlags = 0;
		texDesc.MipLevels = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Format = DXGI_FORMAT_R32_FLOAT;
		m_pDepthsRT = new SimpleRT( pd3dDevice, pd3dContext, &texDesc );

		texDesc.ArraySize          = 1;
		texDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags     = NULL;
		texDesc.Format             = DXGI_FORMAT_R32_TYPELESS;
		texDesc.Width              = LIGHT_RES;
		texDesc.Height             = LIGHT_RES;
		texDesc.MipLevels          = 1;
		texDesc.MiscFlags          = NULL;
		texDesc.SampleDesc.Count   = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage              = D3D11_USAGE_DEFAULT;
		m_pDepthsDS = new DepthRT( pd3dDevice, &texDesc );

		//create the resources for Opacity Shadow Maps
		D3D11_TEXTURE2D_DESC texDesc2;
		texDesc2.Width              = LIGHT_RES;
		texDesc2.Height             = LIGHT_RES;
		if(m_NumLayers==1)
		texDesc2.ArraySize          = 1;
		else
		texDesc2.ArraySize          = ARRAY_SIZE;
		texDesc2.MiscFlags          = 0;
		texDesc2.MipLevels          = 1;
		texDesc2.SampleDesc.Count   = 1;
		texDesc2.SampleDesc.Quality = 0;
		texDesc2.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		if(m_NumLayers==1)
			m_DOM = new SimpleRT( pd3dDevice, pd3dContext, &texDesc2 );
		else
			m_DOM = new SimpleArrayRT( pd3dDevice, pd3dContext, &texDesc2 );


		texDesc.Width = LIGHT_RES;
		texDesc.Height = LIGHT_RES;
		texDesc.ArraySize = 1;
		texDesc.MiscFlags = 0;
		texDesc.MipLevels = 1;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Format = DXGI_FORMAT_R32_FLOAT;
		m_pDepthsRT_DOM = new SimpleRT( pd3dDevice, pd3dContext, &texDesc );

    }

	void OnD3D11DestroyDevice() {
		SAFE_DELETE(m_pDepthsRT);
		SAFE_DELETE(m_pDepthsDS);
		SAFE_DELETE(m_DOM);
		SAFE_DELETE(m_pDepthsRT_DOM);
	}

	CModelViewerCamera &GetCamera() {
		return m_LCamera;
	}

	D3DXMATRIX &GetLightWorldViewProj() {
		return mWorldViewProj;
	}

	D3DXVECTOR3 &GetLightWorldDir() {
		return vLightDirWorld;
	}
	
	D3DXVECTOR3 &GetLightCenterWorld() {
		return vLightCenterWorld;
	}
	
	void UpdateMatrices(D3DXVECTOR3 bbCenter, D3DXVECTOR3 bbExtents) {
		HRESULT hr;

		D3DXMATRIX mTrans, mWorld;
		D3DXMatrixTranslation( &mTrans, -bbCenter.x, -bbCenter.y, -bbCenter.z );
		D3DXMatrixMultiply( &mWorld, &mTrans, m_LCamera.GetWorldMatrix());

		D3DXMATRIX mView = *m_LCamera.GetViewMatrix();
		D3DXMatrixInverse(&mWorldI, NULL, &mWorld);
		D3DXMatrixTranspose(&mWorldIT, &mWorldI);
		D3DXMatrixMultiply( &mWorldView, &mWorld, &mView );
		D3DXMatrixInverse(&mWorldViewI, NULL, &mWorldView);
		D3DXMatrixTranspose(&mWorldViewIT, &mWorldViewI);

		D3DXVECTOR3 vBox[2];
		vBox[0] = bbCenter - bbExtents;
		vBox[1] = bbCenter + bbExtents;

		D3DXVECTOR3 BBox[2];
		BBox[0][0] = BBox[0][1] = BBox[0][2] =  FLT_MAX;
		BBox[1][0] = BBox[1][1] = BBox[1][2] = -FLT_MAX;
		for (int i = 0; i < 8; ++i)
		{
			D3DXVECTOR3 v, v1;
			v[0] = vBox[(i & 1) ? 0 : 1][0];
			v[1] = vBox[(i & 2) ? 0 : 1][1];
			v[2] = vBox[(i & 4) ? 0 : 1][2];
			D3DXVec3TransformCoord(&v1, &v, &mWorldView);
			for (int j = 0; j < 3; ++j)
			{
				BBox[0][j] = min(BBox[0][j], v1[j]);
				BBox[1][j] = max(BBox[1][j], v1[j]);
			}
		}

		float ZNear = BBox[0][2];
		float ZFar =  BBox[1][2];
		V(g_Effect->GetVariableByName("ZNear")->AsScalar()->SetFloat(ZNear));
		V(g_Effect->GetVariableByName("ZFar")->AsScalar()->SetFloat(ZFar));

		int NumSlices = m_NumLayers * 4 - 1;
		float Zn = 0;

		/*
		//constant spacing
		float DzInc = 6; //this value has to be exposed through a slider to the user
		float dz[33];
		for (int i = 0; i < NumSlices+1; i++) {
			dz[i] = DzInc;
			m_Zi[i] = Zn + dz[i] * i;
		}*/

		// linear spacing
		float dzInc = 3;
		float dz[33];
		for (int i = 0; i < NumSlices+1; i++) {
			dz[i] = dzInc*(i+1);
			if(i==0) m_Zi[0] = Zn;
			else m_Zi[i] = m_Zi[i-1] + dz[i-1];
		}


		//V(g_Effect->GetVariableByName("g_DZ")->AsScalar()->SetFloat(DZ));
		V(g_Effect->GetVariableByName("g_Dz")->AsVector()->SetFloatVectorArray(&dz[0],0,ARRAY_SIZE));
		V(g_Effect->GetVariableByName("g_Zi")->AsVector()->SetFloatVectorArray(&m_Zi[1],0,ARRAY_SIZE));
		//V(g_Effect->GetVariableByName("NumLayers")->AsScalar()->SetInt(m_NumLayers));


		float w = (FLOAT)max(abs(BBox[0][0]), (FLOAT)abs(BBox[1][0])) * 2;
		float h = (FLOAT)max(abs(BBox[0][1]), (FLOAT)abs(BBox[1][1])) * 2;
		D3DXMATRIX mProj;
		D3DXMatrixOrthoLH(&mProj, w, h, ZNear, ZFar);
		//D3DXMatrixPerspectiveLH(&mProj, w, h, ZNear, ZFar);

		D3DXMatrixMultiply(&mWorldViewProj, &mWorldView, &mProj);
		D3DXMatrixInverse(&mWorldViewProjI, NULL, &mWorldViewProj);
		V(g_Effect->GetVariableByName("mLightView")->AsMatrix()->SetMatrix(mWorldView));
		V(g_Effect->GetVariableByName("mLightProj")->AsMatrix()->SetMatrix(mProj));
		V(g_Effect->GetVariableByName("mLightViewProj")->AsMatrix()->SetMatrix(mWorldViewProj));
		V(g_Effect->GetVariableByName("mLightViewProjI")->AsMatrix()->SetMatrix(mWorldViewProjI));

		D3DXMATRIX mClip2Tex = D3DXMATRIX(
			0.5,    0,    0,   0,
			0,	   -0.5,  0,   0,
			0,     0,     1,   0,
			0.5,   0.5,   0,   1 );
		D3DXMATRIX mLightViewProjClip2Tex;
		D3DXMatrixMultiply(&mLightViewProjClip2Tex, &mWorldViewProj, &mClip2Tex);
		V(g_Effect->GetVariableByName("mLightViewProjClip2Tex")->AsMatrix()->SetMatrix(mLightViewProjClip2Tex));

		D3DXVECTOR3 vLightClip = D3DXVECTOR3(0, 0, 1);
		D3DXVec3TransformCoord(&vLightDirWorld, &vLightClip, &mWorldViewProjI);

		vLightClip = D3DXVECTOR3(0, 0, 0.5);
		D3DXVec3TransformCoord(&vLightCenterWorld, &vLightClip, &mWorldViewProjI);

        vLightDirWorld -= vLightCenterWorld;
		D3DXVec3Normalize(&vLightDirWorld,&vLightDirWorld);        
		V(g_Effect->GetVariableByName("vLightDir")->AsVector()->SetFloatVector(vLightDirWorld));
	}

	void BeginShadowMapRendering(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, bool DOMPrepass=false)
	{
        float ClearColor[4] = { FLT_MAX };
		if(!DOMPrepass)
		pd3dContext->ClearRenderTargetView( *m_pDepthsRT, ClearColor );
		else
		pd3dContext->ClearRenderTargetView( *m_pDepthsRT_DOM, ClearColor );
			
		pd3dContext->ClearDepthStencilView( *m_pDepthsDS, D3D11_CLEAR_DEPTH, 1.0, 0 );

		g_Effect->GetVariableByName("tShadowMap")->AsShaderResource()->SetResource( NULL );

		ID3D11RenderTargetView* pRTVs[1];
		if(!DOMPrepass) pRTVs[0] = *m_pDepthsRT;
		else pRTVs[0] = *m_pDepthsRT_DOM;
		pd3dContext->OMSetRenderTargets( 1, pRTVs, *m_pDepthsDS );
		pd3dContext->RSSetViewports(1, &m_LViewport);
	}
	void EndShadowMapRendering(ID3D11DeviceContext* pd3dContext, bool DOMPrepass=false)
	{
		pd3dContext->OMSetRenderTargets(0, NULL, NULL);
	}

	void setupDOMShadowMapRendering(ID3D11DeviceContext* pd3dContext)
	{
		pd3dContext->RSSetViewports(1, &m_LViewport);

		float Zero[4] = { 0.0f, 0.0f, 0.0f, FLT_MAX};
		pd3dContext->ClearRenderTargetView( *m_DOM, Zero );

	    g_Effect->GetVariableByName("tShadowMap")->AsShaderResource()->SetResource( NULL );
		g_Effect->GetVariableByName("tShadowMapArray")->AsShaderResource()->SetResource( NULL ); 

		g_Effect->GetVariableByName("tHairDepthMap")->AsShaderResource()->SetResource( *m_pDepthsRT_DOM );
	}

	void setShadowMapRT(ID3D11DeviceContext* pd3dContext)
	{
		assert(m_NumLayers == 1);
		pd3dContext->OMSetRenderTargets( 1, &m_DOM->pRTV, NULL );
		pd3dContext->RSSetViewports(1, &m_LViewport);
	}

	void endDOMShadowMapRendering(ID3D11DeviceContext* pd3dContext)
	{
	    g_Effect->GetVariableByName("tHairDepthMap")->AsShaderResource()->SetResource( NULL );
	}

	void renderDOMPass(ID3D11DeviceContext* pd3dContext, int pass)
	{
		g_Effect->GetVariableByName("g_Zmin")->AsVector()->SetFloatVectorArray(&m_Zi[pass*4*NUM_MRTS],0,NUM_MRTS);
		g_Effect->GetVariableByName("g_Zmax")->AsVector()->SetFloatVectorArray(&m_Zi[pass*4*NUM_MRTS+1],0,NUM_MRTS);

		if(m_NumLayers == 1)
			pd3dContext->OMSetRenderTargets( 1, &m_DOM->pRTV, NULL );
		else
			pd3dContext->OMSetRenderTargets( NUM_MRTS, &(static_cast<SimpleArrayRT*>(m_DOM))->ppRTVs[pass*NUM_MRTS], NULL );

	}
	int getNumDOMPasses()
	{
		return ceil((float)m_NumLayers / NUM_MRTS);
	}

	void SetHairShadowTexture()
	{
        HRESULT hr;

		if(m_useDOM)
		{ 
			if(m_NumLayers == 1)
			{	
				V(g_Effect->GetVariableByName("tShadowMap")->AsShaderResource()->SetResource( m_DOM->pSRV ));
			}
			else
			{	
				V(g_Effect->GetVariableByName("tShadowMapArray")->AsShaderResource()->SetResource( m_DOM->pSRV ));
			} 

			V(g_Effect->GetVariableByName("tHairDepthMap")->AsShaderResource()->SetResource( *m_pDepthsRT_DOM ));
		}
		else
		{ V(g_Effect->GetVariableByName("tShadowMap")->AsShaderResource()->SetResource( *m_pDepthsRT )); }	
	}

	void setHairDepthMap()
	{
		HRESULT hr;
		V(g_Effect->GetVariableByName("tHairDepthMap")->AsShaderResource()->SetResource( *m_pDepthsRT_DOM ));	
	}

	void unsetHairDepthMap()
	{
		HRESULT hr;
		V(g_Effect->GetVariableByName("tHairDepthMap")->AsShaderResource()->SetResource( NULL ));	
	}


	void UnSetHairShadowTexture()
	{
        HRESULT hr;
        V(g_Effect->GetVariableByName("tShadowMap")->AsShaderResource()->SetResource( NULL ));
		if(m_useDOM)
		{
			V(g_Effect->GetVariableByName("tHairDepthMap")->AsShaderResource()->SetResource( NULL ));
		}
		if(m_useDOM && m_NumLayers > 1)
		{
			V(g_Effect->GetVariableByName("tShadowMapArray")->AsShaderResource()->SetResource( NULL ));
		}
	}

	void VisualizeShadowMap(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext)
	{
		SetHairShadowTexture();
		pd3dContext->IASetInputLayout( NULL );
		pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
        g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("VisualizeShadowMap")->Apply(0, pd3dContext);
        pd3dContext->Draw( 3, 0 );
		UnSetHairShadowTexture();
        g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("VisualizeShadowMap")->Apply(0, pd3dContext);
	}
};

int HairShadows::m_NumLayers = 1;
//m_NumLayers = NUM_MRTS;
