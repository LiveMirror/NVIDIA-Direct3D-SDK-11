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

#ifndef UTIL_H
#define UTIL_H

#include "DXUT.h"

#include "D3DX11Effect.h"

//--------------------------------------------------------------------------------------
// Calculate the bounding box that bounds some other bounding box when transformed
//--------------------------------------------------------------------------------------
void TransformBounds(const D3DXMATRIX& m, const D3DXVECTOR4& InMin, const D3DXVECTOR4& InMax, D3DXVECTOR4& OutMin, D3DXVECTOR4& OutMax);

//--------------------------------------------------------------------------------------
// Convenience class for handling UP-style vertex buffer operations
//--------------------------------------------------------------------------------------
class CDrawUP
{
public:

    // Singleton
    static CDrawUP& Instance();

    HRESULT OnD3D11CreateDevice(ID3D11Device* pd3dDevice);
    void OnD3D11DestroyDevice();

    void OnBeginFrame();
    HRESULT DrawUP(ID3D11DeviceContext* pDC, const void* pVerts, UINT VertStride, UINT NumVerts);

private:

    CDrawUP();

    UINT m_UPVertexBufferCursor;
    UINT m_UPVertexBufferSize;
    ID3D11Buffer* m_pUPVertexBuffer;
};

//--------------------------------------------------------------------------------------
// Convenience class for handling screen aligned quads
//--------------------------------------------------------------------------------------
class CScreenAlignedQuad
{
public:

    // Singleton
    static CScreenAlignedQuad& Instance();

    HRESULT OnD3D11CreateDevice(ID3D11Device* pd3dDevice);
    void OnD3D11DestroyDevice();

    HRESULT DrawQuad(ID3D11DeviceContext* pDC, const RECT* pDstRect, ID3DX11EffectPass* pCustomPass);
    HRESULT DrawQuadTranslucent(ID3D11DeviceContext* pDC, ID3D11ShaderResourceView* pSrc, UINT srcMip, const RECT* pDstRect);
    HRESULT DrawQuad(ID3D11DeviceContext* pDC, ID3D11ShaderResourceView* pSrc, UINT srcMip, const RECT* pDstRect, ID3DX11EffectPass* pCustomPass);
    HRESULT DrawQuad(ID3D11DeviceContext* pDC);

    HRESULT SetScaleAndOffset(ID3D11DeviceContext* pDC, const RECT* pDstRect);

	HRESULT BindToEffect(ID3DX11Effect* pEffect);

private:

    CScreenAlignedQuad();

    ID3DX11EffectVectorVariable* m_ScaleAndOffset_EffectVar;
    ID3DX11EffectShaderResourceVariable* m_Src_EffectVar;
    ID3DX11EffectScalarVariable* m_Mip_EffectVar;
    ID3DX11EffectPass*           m_pEffectPass;
    ID3DX11EffectPass*           m_pTranslucentEffectPass;
};

//--------------------------------------------------------------------------------------
// General-purpose position-only vertex format
//--------------------------------------------------------------------------------------
struct PositionOnlyVertex
{
    D3DXVECTOR3 position;
    enum { NumLayoutElements = 1 };
    static const D3D11_INPUT_ELEMENT_DESC* GetLayout();
};

//--------------------------------------------------------------------------------------
// Enum for tracking AA modes
//--------------------------------------------------------------------------------------
enum MSAAMode {
    NoAA = 0,
    s2,
    s4,
    s8,
    s16,
    s32,
    s48,
    NumMSAAModes
};

HRESULT GetAAModeFromSampleDesc(const DXGI_SAMPLE_DESC& inDesc, MSAAMode& outMode);
LPCSTR GetStringFromAAMode(MSAAMode mode);

#endif // UTIL_H
