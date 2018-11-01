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

#ifndef LIGHT_SOURCE_H
#define LIGHT_SOURCE_H

#include "DXUT.h"

#include "D3DX11Effect.h"
#include "SharedDefines.h"

struct CLightSource
{
    CLightSource();

    enum { NumOpacityMaps = 4 };

    ID3D11Texture2D*            m_OpacityMaps4x16F[NumOpacityMaps];
    ID3D11ShaderResourceView*   m_OpacityMapSRVs4x16F[NumOpacityMaps];
    ID3D11RenderTargetView*     m_OpacityRTVs4x16F[NumOpacityMaps];

    ID3D11Texture2D*            m_pShadowDepthStencil;
    ID3D11DepthStencilView*     m_pShadowDepthStencilDSV;
    ID3D11ShaderResourceView*   m_pShadowDepthStencilSRV;

    ID3D11Texture2D*            m_pShadowMap;
    ID3D11ShaderResourceView*   m_pShadowMapSRV;
    ID3D11RenderTargetView*     m_pShadowMapRTV;

    ID3DX11EffectShaderResourceVariable* m_OpacityMap0_EffectVar;
    ID3DX11EffectShaderResourceVariable* m_OpacityMap1_EffectVar;
    ID3DX11EffectShaderResourceVariable* m_OpacityMap2_EffectVar;
    ID3DX11EffectShaderResourceVariable* m_OpacityMap3_EffectVar;
    ID3DX11EffectShaderResourceVariable* m_ShadowMap_EffectVar;

    ID3DX11EffectMatrixVariable* m_mViewToOpacityShadowTexture_EffectVar;
    ID3DX11EffectMatrixVariable* m_mViewToShadowTexture_EffectVar;
    ID3DX11EffectVectorVariable* m_LightDirection_EffectVar;
    ID3DX11EffectVectorVariable* m_LightColor_EffectVar;
    ID3DX11EffectMatrixVariable* m_mViewToLight_EffectVar;

    D3DXVECTOR4 m_LightColor;

    D3DXMATRIX m_mLightToWorld;
    D3DXMATRIX m_mLightToWorldDelta;
    D3DXMATRIX m_mOpacityShadowClipToWorld;
};

class CLightSources
{
public:

    CLightSources(UINT OpacityMapWH, UINT OpacityMapMipLevel, UINT ShadowMapWH);

    enum { MaxNumLights = NumActiveLights };
    enum { NumOpacityMapsPerLight = CLightSource::NumOpacityMaps };

    void InitLight(UINT lightIx, const D3DXMATRIX& lightToWorld, const D3DXVECTOR4& color);

    void CycleControlledLight(const D3DXMATRIX& controlMatrix);
    void UpdateControlledLight(const D3DXMATRIX& controlMatrix);
    ID3D11ShaderResourceView* GetControlledLightOpacityMap(UINT mapIx);
    const D3DXMATRIX& GetControlledLightOpacityShadowClipToWorld() const;

    HRESULT OnD3D11CreateDevice(ID3D11Device* pd3dDevice);
    void OnD3D11DestroyDevice();

    void BeginRenderToOpacityMaps(   ID3D11DeviceContext* pDC, UINT lightIx,
                                     const D3DXVECTOR3& SubjectBoundsMin, const D3DXVECTOR3& SubjectBoundsMax,
                                     const D3DXMATRIX& mViewToWorld
                                     );
    void EndRenderToOpacityMaps(ID3D11DeviceContext* pDC, UINT lightIx);

    void BeginRenderToShadowMap(    ID3D11DeviceContext* pDC, UINT lightIx,
                                    const D3DXVECTOR3& SubjectBoundsMin, const D3DXVECTOR3& SubjectBoundsMax,
                                    const D3DXMATRIX& mViewToWorld
                                    );
    void EndRenderToShadowMap(ID3D11DeviceContext* pDC, UINT lightIx);

    UINT GetNumLights() const { return MaxNumLights; }

    void SetSaveOpacityMaps() { m_SaveRenderTargets = TRUE; }

    HRESULT SetOpacityMapMipLevel(ID3D11Device* pd3dDevice, UINT OpacityMipLevel);
    UINT GetOpacityMapMipLevelWH() const { return m_OpacityMapMipLevelWH; }
    UINT GetOpacityMapMipLevel() const { return m_OpacityMapMipLevel; }

    void SetOpacityQuality(INT quality);
    INT GetOpacityQuality() const { return m_OpacityQuality; }

    void SetOpacityMultiplier(FLOAT mult);
    FLOAT GetOpacityMultiplier() const { return m_OpacityMultiplier; }

    HRESULT SetEffectParametersForSceneRendering(const D3DXMATRIX& mViewToWorld);

	HRESULT BindToEffect(ID3D11Device* pd3dDevice, ID3DX11Effect* pEffect);

	void SetOpacityShadowStrength(FLOAT f) { m_OpacityShadowStrength = f; }

private:

    void CalcShadowMappingParams(   UINT lightIx,
                                    const D3DXVECTOR3& SubjectBoundsMin, const D3DXVECTOR3& SubjectBoundsMax,
                                    const D3DXMATRIX& mViewToWorld,
                                    D3DXMATRIX& mWorldToLight,
                                    D3DXMATRIX& mLightToShadowClip,
                                    D3DXMATRIX& mViewToShadowTexture,
                                    D3DXMATRIX* mShadowClipToWorld,
                                    D3DXVECTOR4& vZScaleAndBias
                                    );

    D3DXMATRIX CalcLightToWorld(UINT lightIx);

    BOOL m_SaveRenderTargets;

    UINT m_OpacityMapWH;
    UINT m_ShadowMapWH;
    UINT m_OpacityMapMipLevelWH;
    UINT m_OpacityMapMipLevel;
    INT m_OpacityQuality;
    FLOAT m_OpacityMultiplier;
	FLOAT m_OpacityShadowStrength;
    CLightSource m_LightSources[MaxNumLights];

    ID3DX11EffectPass*           m_pEffectPass_LinearizeShadowZ;

    ID3DX11EffectMatrixVariable* m_mWorldToView_EffectVar;
    ID3DX11EffectMatrixVariable* m_mViewToProj_EffectVar;
    ID3DX11EffectMatrixVariable* m_mProjToView_EffectVar;
    ID3DX11EffectMatrixVariable* m_mLocalToWorld_EffectVar;
    ID3DX11EffectVectorVariable* m_zScaleAndBias_EffectVar;
    ID3DX11EffectScalarVariable* m_OpacityMip_EffectVar;
    ID3DX11EffectScalarVariable* m_OpacityQuality_EffectVar;
    ID3DX11EffectScalarVariable* m_OpacityMultiplier_EffectVar;
	ID3DX11EffectScalarVariable* m_OpacityShadowStrength_EffectVar;

    UINT m_ControlledLight;
    FLOAT m_LightOrbitDistance;

    ID3D11Texture2D*            m_pOpacityReadbackTexture;
};

#endif // LIGHT_SOURCE_H
