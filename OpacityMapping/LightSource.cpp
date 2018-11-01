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

#include "LightSource.h"
#include "Util.h"

CLightSource::CLightSource() :
    m_pShadowDepthStencil(NULL),
    m_pShadowDepthStencilDSV(NULL),
    m_pShadowDepthStencilSRV(NULL),
    m_pShadowMap(NULL),
    m_pShadowMapSRV(NULL),
    m_pShadowMapRTV(NULL),
    m_OpacityMap0_EffectVar(NULL),
    m_OpacityMap1_EffectVar(NULL),
    m_OpacityMap2_EffectVar(NULL),
    m_OpacityMap3_EffectVar(NULL),
    m_ShadowMap_EffectVar(NULL),
    m_mViewToOpacityShadowTexture_EffectVar(NULL),
    m_mViewToShadowTexture_EffectVar(NULL),
    m_LightDirection_EffectVar(NULL),
    m_LightColor_EffectVar(NULL),
    m_mViewToLight_EffectVar(NULL),
    m_LightColor(1.f,1.f,1.f,1.f)
{
    D3DXMatrixIdentity(&m_mLightToWorld);
    D3DXMatrixIdentity(&m_mLightToWorldDelta);
    D3DXMatrixIdentity(&m_mOpacityShadowClipToWorld);

    for(int i = 0; i != NumOpacityMaps; ++i)
    {
        m_OpacityMaps4x16F[i] = NULL;
        m_OpacityMapSRVs4x16F[i] = NULL;
        m_OpacityRTVs4x16F[i] = NULL;
    }
}

CLightSources::CLightSources(UINT OpacityMapWH, UINT OpacityMapMipLevel, UINT ShadowMapWH) :
    m_SaveRenderTargets(FALSE),
    m_OpacityMapMipLevel(OpacityMapMipLevel),
    m_OpacityMapWH(OpacityMapWH),
    m_ShadowMapWH(ShadowMapWH),
    m_OpacityMapMipLevelWH(OpacityMapWH >> OpacityMapMipLevel),
    m_OpacityQuality(16),
    m_OpacityMultiplier(0.4f),
	m_OpacityShadowStrength(1.f),
    m_ControlledLight(0),
    m_LightOrbitDistance(5.f),
    m_mWorldToView_EffectVar(NULL),
    m_mViewToProj_EffectVar(NULL),
    m_mProjToView_EffectVar(NULL),
    m_mLocalToWorld_EffectVar(NULL),
    m_zScaleAndBias_EffectVar(NULL),
    m_OpacityMip_EffectVar(NULL),
    m_OpacityQuality_EffectVar(NULL),
    m_OpacityMultiplier_EffectVar(NULL),
	m_OpacityShadowStrength_EffectVar(NULL),
    m_pEffectPass_LinearizeShadowZ(NULL),
    m_pOpacityReadbackTexture(NULL)
{
}

void CLightSources::InitLight(UINT lightIx, const D3DXMATRIX& lightToWorld, const D3DXVECTOR4& color)
{
    m_LightSources[lightIx].m_mLightToWorld = lightToWorld;
    m_LightSources[lightIx].m_mLightToWorldDelta = lightToWorld;
    m_LightSources[lightIx].m_LightColor = color;
}

void CLightSources::CycleControlledLight(const D3DXMATRIX& controlMatrix)
{
    m_ControlledLight = (m_ControlledLight + 1) % MaxNumLights;

    D3DXMATRIXA16 invControlMatrix;
    D3DXMatrixInverse(&invControlMatrix, NULL, &controlMatrix);

    m_LightSources[m_ControlledLight].m_mLightToWorldDelta = m_LightSources[m_ControlledLight].m_mLightToWorld * invControlMatrix;
}

HRESULT CLightSources::BindToEffect(ID3D11Device* pd3dDevice, ID3DX11Effect* pEffect)
{
	HRESULT hr;

    m_pEffectPass_LinearizeShadowZ = pEffect->GetTechniqueByName("LinearizeShadowZ")->GetPassByIndex(0);

    ID3DX11EffectVariable* OpacityMap0_EffectVar = pEffect->GetVariableByName("g_OpacityMap0");
    ID3DX11EffectVariable* OpacityMap1_EffectVar = pEffect->GetVariableByName("g_OpacityMap1");
    ID3DX11EffectVariable* OpacityMap2_EffectVar = pEffect->GetVariableByName("g_OpacityMap2");
    ID3DX11EffectVariable* OpacityMap3_EffectVar = pEffect->GetVariableByName("g_OpacityMap3");
    ID3DX11EffectVariable* ShadowMap_EffectVar = pEffect->GetVariableByName("g_ShadowMap");

    ID3DX11EffectVariable* mViewToOpacityShadowTexture_EffectVar = pEffect->GetVariableByName("g_mViewToOpacityShadowTexture");
    ID3DX11EffectVariable* mViewToShadowTexture_EffectVar = pEffect->GetVariableByName("g_mViewToShadowTexture");
    ID3DX11EffectVariable* LightDirection_EffectVar = pEffect->GetVariableByName("g_LightDirection");
    ID3DX11EffectVariable* LightColor_EffectVar = pEffect->GetVariableByName("g_LightColor");
    ID3DX11EffectVariable* mViewToLight_EffectVar = pEffect->GetVariableByName("g_mViewToLight");

    for(int light = 0; light != MaxNumLights; ++light)
    {
        m_LightSources[light].m_OpacityMap0_EffectVar = OpacityMap0_EffectVar->GetElement(light)->AsShaderResource();
        m_LightSources[light].m_OpacityMap1_EffectVar = OpacityMap1_EffectVar->GetElement(light)->AsShaderResource();
        m_LightSources[light].m_OpacityMap2_EffectVar = OpacityMap2_EffectVar->GetElement(light)->AsShaderResource();
        m_LightSources[light].m_OpacityMap3_EffectVar = OpacityMap3_EffectVar->GetElement(light)->AsShaderResource();
        m_LightSources[light].m_ShadowMap_EffectVar = ShadowMap_EffectVar->GetElement(light)->AsShaderResource();

        m_LightSources[light].m_mViewToOpacityShadowTexture_EffectVar = mViewToOpacityShadowTexture_EffectVar->GetElement(light)->AsMatrix();
        m_LightSources[light].m_mViewToShadowTexture_EffectVar = mViewToShadowTexture_EffectVar->GetElement(light)->AsMatrix();
        m_LightSources[light].m_LightDirection_EffectVar = LightDirection_EffectVar->GetElement(light)->AsVector();
        m_LightSources[light].m_LightColor_EffectVar = LightColor_EffectVar->GetElement(light)->AsVector();
        m_LightSources[light].m_mViewToLight_EffectVar = mViewToLight_EffectVar->GetElement(light)->AsMatrix();
    }

    m_mWorldToView_EffectVar = pEffect->GetVariableByName("g_mWorldToView")->AsMatrix();
    m_mViewToProj_EffectVar = pEffect->GetVariableByName("g_mViewToProj")->AsMatrix();
    m_mProjToView_EffectVar = pEffect->GetVariableByName("g_mProjToView")->AsMatrix();
    m_mLocalToWorld_EffectVar = pEffect->GetVariableByName("g_mLocalToWorld")->AsMatrix();
    m_zScaleAndBias_EffectVar = pEffect->GetVariableByName("g_zScaleAndBias")->AsVector();
    m_OpacityMip_EffectVar = pEffect->GetVariableByName("g_OpacityMip")->AsScalar();
    m_OpacityQuality_EffectVar = pEffect->GetVariableByName("g_OpacityQuality")->AsScalar();
    m_OpacityMultiplier_EffectVar = pEffect->GetVariableByName("g_OpacityMultiplier")->AsScalar();
	m_OpacityShadowStrength_EffectVar = pEffect->GetVariableByName("g_OpacityShadowStrength")->AsScalar();

    // Force through initial state
    V_RETURN(SetOpacityMapMipLevel(pd3dDevice, m_OpacityMapMipLevel));
    SetOpacityQuality(m_OpacityQuality);
    SetOpacityMultiplier(m_OpacityMultiplier);

	return S_OK;
}

HRESULT CLightSources::OnD3D11CreateDevice(ID3D11Device* pd3dDevice)
{
    HRESULT hr;

    for(int light = 0; light != MaxNumLights; ++light)
    {
        for(int i = 0; i != NumOpacityMapsPerLight; ++i)
        {
            D3D11_TEXTURE2D_DESC texDesc;
            texDesc.Width = m_OpacityMapWH;
            texDesc.Height = m_OpacityMapWH;
            texDesc.MipLevels = 0;
            texDesc.ArraySize = 1;
            texDesc.SampleDesc.Count = 1; 
            texDesc.SampleDesc.Quality = 0; 
            texDesc.Usage = D3D11_USAGE_DEFAULT;
            texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            texDesc.CPUAccessFlags = 0;
            texDesc.MiscFlags = 0;

            texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            V_RETURN( pd3dDevice->CreateTexture2D(&texDesc, NULL, &m_LightSources[light].m_OpacityMaps4x16F[i]) );

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;

            D3D11_TEXTURE2D_DESC actualTexDesc;
            m_LightSources[light].m_OpacityMaps4x16F[i]->GetDesc(&actualTexDesc);
            srvDesc.Format = actualTexDesc.Format;
            srvDesc.Texture2D.MipLevels = actualTexDesc.MipLevels;
            V_RETURN( pd3dDevice->CreateShaderResourceView(m_LightSources[light].m_OpacityMaps4x16F[i], &srvDesc, &m_LightSources[light].m_OpacityMapSRVs4x16F[i]) );
        }

        {
            D3D11_TEXTURE2D_DESC texDesc;
            texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
            texDesc.Width = m_ShadowMapWH;
            texDesc.Height = m_ShadowMapWH;
            texDesc.MipLevels = 1;
            texDesc.ArraySize = 1;
            texDesc.SampleDesc.Count = 1; 
            texDesc.SampleDesc.Quality = 0; 
            texDesc.Usage = D3D11_USAGE_DEFAULT;
            texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
            texDesc.CPUAccessFlags = 0;
            texDesc.MiscFlags = 0;

            V_RETURN( pd3dDevice->CreateTexture2D(&texDesc, NULL, &m_LightSources[light].m_pShadowDepthStencil) );

            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
            dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            dsvDesc.Flags = 0;
            dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
            V_RETURN( pd3dDevice->CreateDepthStencilView(m_LightSources[light].m_pShadowDepthStencil, &dsvDesc, &m_LightSources[light].m_pShadowDepthStencilDSV) );

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
            srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            V_RETURN( pd3dDevice->CreateShaderResourceView(m_LightSources[light].m_pShadowDepthStencil, &srvDesc, &m_LightSources[light].m_pShadowDepthStencilSRV) );

            texDesc.Format = DXGI_FORMAT_R32_FLOAT;
            texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

            V_RETURN( pd3dDevice->CreateTexture2D(&texDesc, NULL, &m_LightSources[light].m_pShadowMap) );

            srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
            srvDesc.Format = texDesc.Format;
            V_RETURN( pd3dDevice->CreateShaderResourceView(m_LightSources[light].m_pShadowMap, &srvDesc, &m_LightSources[light].m_pShadowMapSRV) );

            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = 0;
            rtvDesc.Format = texDesc.Format;
            V_RETURN( pd3dDevice->CreateRenderTargetView(m_LightSources[light].m_pShadowMap, &rtvDesc, &m_LightSources[light].m_pShadowMapRTV) );
        }
    }

    // Set up readback texture for opacity
    {
        D3D11_TEXTURE2D_DESC texDesc;
        texDesc.Width = m_OpacityMapWH;
        texDesc.Height = m_OpacityMapWH;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        texDesc.SampleDesc.Count = 1; 
        texDesc.SampleDesc.Quality = 0; 
        texDesc.Usage = D3D11_USAGE_STAGING;
        texDesc.BindFlags = 0;
        texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        texDesc.MiscFlags = 0;

        V_RETURN( pd3dDevice->CreateTexture2D(&texDesc, NULL, &m_pOpacityReadbackTexture) );
    }

    return S_OK;
}

HRESULT CLightSources::SetOpacityMapMipLevel(ID3D11Device* pd3dDevice, UINT OpacityMipLevel)
{
    HRESULT hr;

    m_OpacityMapMipLevel = OpacityMipLevel;
    m_OpacityMapMipLevelWH = m_OpacityMapWH >> m_OpacityMapMipLevel;

    for(int light = 0; light != MaxNumLights; ++light)
    {
        for(int i = 0; i != NumOpacityMapsPerLight; ++i)
        {
            SAFE_RELEASE(m_LightSources[light].m_OpacityRTVs4x16F[i]);

            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = m_OpacityMapMipLevel;

            rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            V_RETURN( pd3dDevice->CreateRenderTargetView(m_LightSources[light].m_OpacityMaps4x16F[i], &rtvDesc, &m_LightSources[light].m_OpacityRTVs4x16F[i]) );
        }
    }

    V( m_OpacityMip_EffectVar->SetFloat(FLOAT(m_OpacityMapMipLevel)) );

    return S_OK;
}

void CLightSources::SetOpacityQuality(INT quality)
{
    HRESULT hr;

    m_OpacityQuality = quality;
    V( m_OpacityQuality_EffectVar->SetFloat(FLOAT(m_OpacityQuality)) );
}

void CLightSources::SetOpacityMultiplier(FLOAT mult)
{
    HRESULT hr;

    m_OpacityMultiplier = mult;
    V( m_OpacityMultiplier_EffectVar->SetFloat(FLOAT(m_OpacityMultiplier)) );
}

void CLightSources::UpdateControlledLight(const D3DXMATRIX& controlMatrix)
{
    m_LightSources[m_ControlledLight].m_mLightToWorld = m_LightSources[m_ControlledLight].m_mLightToWorldDelta * controlMatrix;
}

D3DXMATRIX CLightSources::CalcLightToWorld(UINT lightIx)
{
    CLightSource& lightSource = m_LightSources[lightIx];

    // Calculate the actual light transform (accounting for the orbit distance)
    D3DXMATRIXA16 mLightOrbitTranslation;
    D3DXMatrixTranslation(&mLightOrbitTranslation, 0.f, 0.f, -m_LightOrbitDistance);
    D3DXMATRIXA16 mLightToWorld = mLightOrbitTranslation * lightSource.m_mLightToWorld;

    return mLightToWorld;
}

void CLightSources::CalcShadowMappingParams(    UINT lightIx,
                                                const D3DXVECTOR3& SubjectBoundsMin, const D3DXVECTOR3& SubjectBoundsMax,
                                                const D3DXMATRIX& mViewToWorld,
                                                D3DXMATRIX& mWorldToLight,
                                                D3DXMATRIX& mLightToShadowClip,
                                                D3DXMATRIX& mViewToShadowTexture,
                                                D3DXMATRIX* mShadowClipToWorld,
                                                D3DXVECTOR4& vZScaleAndBias
                                                )
{
    // Calculate the actual light transform (accounting for the orbit distance)
    D3DXMATRIXA16 mLightToWorld = CalcLightToWorld(lightIx);
    D3DXMatrixInverse(&mWorldToLight, NULL, &mLightToWorld);

    // Calculate the light-space bounds of the particle system
    D3DXVECTOR4 LightSpaceMinCorner, LightSpaceMaxCorner;
    TransformBounds(mWorldToLight, D3DXVECTOR4(SubjectBoundsMin,0.f), D3DXVECTOR4(SubjectBoundsMax,0.f), LightSpaceMinCorner, LightSpaceMaxCorner);

    // We can then calculate the light-to-shadow-clip from the bounds
    const D3DXVECTOR4 LightSpaceCentre = 0.5f * (LightSpaceMinCorner + LightSpaceMaxCorner);
    const D3DXVECTOR4 LightSpaceExtents = LightSpaceMaxCorner - LightSpaceMinCorner;
    D3DXMATRIXA16 TranslationComponent, ScalingComponent;
    D3DXMatrixTranslation(&TranslationComponent, -LightSpaceCentre.x, -LightSpaceCentre.y, -LightSpaceMinCorner.z);
    D3DXMatrixScaling(&ScalingComponent, 2.f/LightSpaceExtents.x, 2.f/LightSpaceExtents.y, 1.f/LightSpaceExtents.z);
    mLightToShadowClip = TranslationComponent * ScalingComponent;
    D3DXMATRIXA16 mWorldToShadowClip = mWorldToLight * mLightToShadowClip;
    if(mShadowClipToWorld)
    {
        D3DXMatrixInverse(mShadowClipToWorld, NULL, &mWorldToShadowClip);
    }

    vZScaleAndBias.x = 1.f/(LightSpaceMaxCorner.z - LightSpaceMinCorner.z);
    vZScaleAndBias.y = -LightSpaceMinCorner.z * vZScaleAndBias.x;

    // Finally, we will also need to transform from clip space to texture space
    D3DXMatrixTranslation(&TranslationComponent, 1.f, - 1.f, 0.f);
    D3DXMatrixScaling(&ScalingComponent, 0.5f, -0.5f, 1.f);
    D3DXMATRIXA16 ClipToTexture = TranslationComponent * ScalingComponent;
    D3DXMATRIXA16 mWorldToShadowTexture = mWorldToShadowClip * ClipToTexture;
    mViewToShadowTexture = mViewToWorld * mWorldToShadowTexture;
}

void CLightSources::BeginRenderToOpacityMaps(   ID3D11DeviceContext* pDC, UINT lightIx,
                                                const D3DXVECTOR3& SubjectBoundsMin, const D3DXVECTOR3& SubjectBoundsMax,
                                                const D3DXMATRIX& mViewToWorld
                                                )
{
    HRESULT hr;

    const D3DXVECTOR4 OpacityClearColor(0.f,0.f,0.f,0.f);

    D3DXMATRIXA16 mWorldToLight;
    D3DXMATRIXA16 mLightToShadowClip;
    D3DXMATRIXA16 mViewToShadowTexture;
    D3DXVECTOR4 vZScaleAndBias;

    CLightSource& lightSource = m_LightSources[lightIx];

    CalcShadowMappingParams(    lightIx,
                                SubjectBoundsMin, SubjectBoundsMax, mViewToWorld,
                                mWorldToLight,
                                mLightToShadowClip,
                                mViewToShadowTexture,
                                &lightSource.m_mOpacityShadowClipToWorld,
                                vZScaleAndBias
                                );

    // Render opacity maps
    ID3D11RenderTargetView** pOpacityRTVs = lightSource.m_OpacityRTVs4x16F;
    pDC->OMSetRenderTargets(4, pOpacityRTVs, NULL);
    const D3D11_VIEWPORT opacityViewport = {0.f, 0.f, FLOAT(m_OpacityMapMipLevelWH), FLOAT(m_OpacityMapMipLevelWH), 0.f, 1.f };
    pDC->RSSetViewports(1, &opacityViewport);

    // Clear the opacity maps
    for(int mrt = 0; mrt != NumOpacityMapsPerLight; ++mrt)
    {
        pDC->ClearRenderTargetView(pOpacityRTVs[mrt], (FLOAT*)&OpacityClearColor);
    }

    // Set various vars (some aren't needed right now, but it's convenient to do this now while
    // the data is hanging round)
    D3DXMATRIXA16 mIdentity;
    D3DXMatrixIdentity(&mIdentity);
    V( m_mWorldToView_EffectVar->SetMatrix((FLOAT*)&mWorldToLight) );
    V( m_mViewToProj_EffectVar->SetMatrix((FLOAT*)&mLightToShadowClip) );
    V( m_mLocalToWorld_EffectVar->SetMatrix((FLOAT*)&mIdentity) );
    V( m_zScaleAndBias_EffectVar->SetFloatVector((FLOAT*)&vZScaleAndBias) );
    V( lightSource.m_mViewToOpacityShadowTexture_EffectVar->SetMatrix((FLOAT*)&mViewToShadowTexture) );
}

void CLightSources::EndRenderToOpacityMaps(ID3D11DeviceContext* pDC, UINT lightIx)
{
    HRESULT hr;

    CLightSource& lightSource = m_LightSources[lightIx];

    // If requested, grab the opacity textures and save to disk
    if(m_SaveRenderTargets && lightIx == m_ControlledLight)
    {
        ID3D11Texture2D** pOpacityMaps = lightSource.m_OpacityMaps4x16F;
        pDC->CopySubresourceRegion(m_pOpacityReadbackTexture, 0, 0, 0, 0, pOpacityMaps[0], m_OpacityMapMipLevel, NULL);
        V( D3DX11SaveTextureToFile(pDC, m_pOpacityReadbackTexture, D3DX11_IFF_DDS, L"OpacityTarget0.dds") );
        pDC->CopySubresourceRegion(m_pOpacityReadbackTexture, 0, 0, 0, 0, pOpacityMaps[1], m_OpacityMapMipLevel, NULL);
        V( D3DX11SaveTextureToFile(pDC, m_pOpacityReadbackTexture, D3DX11_IFF_DDS, L"OpacityTarget1.dds") );
        pDC->CopySubresourceRegion(m_pOpacityReadbackTexture, 0, 0, 0, 0, pOpacityMaps[2], m_OpacityMapMipLevel, NULL);
        V( D3DX11SaveTextureToFile(pDC, m_pOpacityReadbackTexture, D3DX11_IFF_DDS, L"OpacityTarget2.dds") );
        pDC->CopySubresourceRegion(m_pOpacityReadbackTexture, 0, 0, 0, 0, pOpacityMaps[3], m_OpacityMapMipLevel, NULL);
        V( D3DX11SaveTextureToFile(pDC, m_pOpacityReadbackTexture, D3DX11_IFF_DDS, L"OpacityTarget3.dds") );
        m_SaveRenderTargets = FALSE;
    }

    // Set the opacity textures into the effect
    ID3D11ShaderResourceView** pOpacityMapSRVs = lightSource.m_OpacityMapSRVs4x16F;
    V( lightSource.m_OpacityMap0_EffectVar->SetResource(pOpacityMapSRVs[0] ) );
    V( lightSource.m_OpacityMap1_EffectVar->SetResource(pOpacityMapSRVs[1] ) );
    V( lightSource.m_OpacityMap2_EffectVar->SetResource(pOpacityMapSRVs[2] ) );
    V( lightSource.m_OpacityMap3_EffectVar->SetResource(pOpacityMapSRVs[3] ) );
}

void CLightSources::BeginRenderToShadowMap(    ID3D11DeviceContext* pDC, UINT lightIx,
                                                const D3DXVECTOR3& SubjectBoundsMin, const D3DXVECTOR3& SubjectBoundsMax,
                                                const D3DXMATRIX& mViewToWorld
                                                )
{
    HRESULT hr;

    D3DXMATRIXA16 mWorldToLight;
    D3DXMATRIXA16 mLightToShadowClip;
    D3DXMATRIXA16 mViewToShadowTexture;
    D3DXVECTOR4 vZScaleAndBias;

    CLightSource& lightSource = m_LightSources[lightIx];

    CalcShadowMappingParams(    lightIx,
                                SubjectBoundsMin, SubjectBoundsMax, mViewToWorld,
                                mWorldToLight,
                                mLightToShadowClip,
                                mViewToShadowTexture,
                                NULL,
                                vZScaleAndBias
                                );

    // Render opacity maps
    pDC->OMSetRenderTargets(0, NULL, lightSource.m_pShadowDepthStencilDSV);
    const D3D11_VIEWPORT shadowViewport = {0.f, 0.f, FLOAT(m_ShadowMapWH), FLOAT(m_ShadowMapWH), 0.f, 1.f };
    pDC->RSSetViewports(1, &shadowViewport);

    // Clear to render
    pDC->ClearDepthStencilView(lightSource.m_pShadowDepthStencilDSV, D3D11_CLEAR_DEPTH, 1.f, 0);

    // Set various vars (some aren't needed right now, but it's convenient to do this now while
    // the data is hanging round)
    D3DXMATRIXA16 mIdentity;
    D3DXMatrixIdentity(&mIdentity);
    D3DXMATRIXA16 mShadowClipToLight;
    D3DXMatrixInverse(&mShadowClipToLight, NULL, &mLightToShadowClip);
    V( m_mWorldToView_EffectVar->SetMatrix((FLOAT*)&mWorldToLight) );
    V( m_mViewToProj_EffectVar->SetMatrix((FLOAT*)&mLightToShadowClip) );
    V( m_mProjToView_EffectVar->SetMatrix((FLOAT*)&mShadowClipToLight) );
    V( m_mLocalToWorld_EffectVar->SetMatrix((FLOAT*)&mIdentity) );
    V( m_zScaleAndBias_EffectVar->SetFloatVector((FLOAT*)&vZScaleAndBias) );
    V( lightSource.m_mViewToShadowTexture_EffectVar->SetMatrix((FLOAT*)&mViewToShadowTexture) );
}

void CLightSources::EndRenderToShadowMap(ID3D11DeviceContext* pDC, UINT lightIx)
{
    HRESULT hr;

    CLightSource& lightSource = m_LightSources[lightIx];

    // Linearize the depth buffer (viewport from BeginRenderToShadowMap() still applies)
    pDC->OMSetRenderTargets(1, &lightSource.m_pShadowMapRTV, NULL);
    V(CScreenAlignedQuad::Instance().DrawQuad(pDC, lightSource.m_pShadowDepthStencilSRV, 0, NULL, m_pEffectPass_LinearizeShadowZ));

    // Set the shadow texture into the effect
    V( lightSource.m_ShadowMap_EffectVar->SetResource(lightSource.m_pShadowMapSRV) );
}

HRESULT CLightSources::SetEffectParametersForSceneRendering(const D3DXMATRIX& mViewToWorld)
{
    HRESULT hr;

    for(int light = 0; light != MaxNumLights; ++light)
    {
        CLightSource& lightSource = m_LightSources[light];

        D3DXMATRIXA16 mWorldToLight;
        D3DXVECTOR4 vLightDirection;

        // Calculate the actual light transform (accounting for the orbit distance)
        D3DXMATRIXA16 mLightToWorld = CalcLightToWorld(light);

        // Calculate the light direction vector, in world space
        const D3DXVECTOR4 LightSpaceLightVector(0.f,0.f,1.f,0.f);
        D3DXVec4Transform(&vLightDirection, &LightSpaceLightVector, &mLightToWorld);
        D3DXMatrixInverse(&mWorldToLight, NULL, &mLightToWorld);

        // Set the light color
        V_RETURN( lightSource.m_LightColor_EffectVar->SetFloatVector(lightSource.m_LightColor) );    // Subject-independent

        // Set the light direction / shadow transform
        V_RETURN( lightSource.m_LightDirection_EffectVar->SetFloatVector((FLOAT*)vLightDirection) );    // Subject-independent

        // Set the view -> light transform
        D3DXMATRIXA16 mViewToLight = mViewToWorld * mWorldToLight;
        V_RETURN( lightSource.m_mViewToLight_EffectVar->SetMatrix((FLOAT*)&mViewToLight) );    // Subject-independent
    }

	V_RETURN(m_OpacityShadowStrength_EffectVar->SetFloat(m_OpacityShadowStrength));

    return S_OK;
}

ID3D11ShaderResourceView* CLightSources::GetControlledLightOpacityMap(UINT mapIx)
{
    return m_LightSources[m_ControlledLight].m_OpacityMapSRVs4x16F[mapIx];
}

void CLightSources::OnD3D11DestroyDevice()
{
    SAFE_RELEASE( m_pOpacityReadbackTexture );

    for(int light = 0; light != MaxNumLights; ++light)
    {
        for(int i = 0; i != NumOpacityMapsPerLight; ++i)
        {
            SAFE_RELEASE( m_LightSources[light].m_OpacityMaps4x16F[i] );
            SAFE_RELEASE( m_LightSources[light].m_OpacityMapSRVs4x16F[i] );
            SAFE_RELEASE( m_LightSources[light].m_OpacityRTVs4x16F[i] );
        }

        SAFE_RELEASE(m_LightSources[light].m_pShadowDepthStencil);
        SAFE_RELEASE(m_LightSources[light].m_pShadowDepthStencilDSV);
        SAFE_RELEASE(m_LightSources[light].m_pShadowDepthStencilSRV);

        SAFE_RELEASE(m_LightSources[light].m_pShadowMap);
        SAFE_RELEASE(m_LightSources[light].m_pShadowMapSRV);
        SAFE_RELEASE(m_LightSources[light].m_pShadowMapRTV);
    }
}

const D3DXMATRIX& CLightSources::GetControlledLightOpacityShadowClipToWorld() const
{
    return m_LightSources[m_ControlledLight].m_mOpacityShadowClipToWorld;
}

