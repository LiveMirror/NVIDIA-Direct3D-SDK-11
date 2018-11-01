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

#ifndef LINEDRAW_H
#define LINEDRAW_H

#include "DXUT.h"

#include "D3DX11Effect.h"

//--------------------------------------------------------------------------------------
// Convenience class for drawing lines
//--------------------------------------------------------------------------------------
class CLineDraw
{
public:

    // Singleton
    static CLineDraw& Instance();

    HRESULT OnD3D11CreateDevice(ID3D11Device* pd3dDevice);
    void OnD3D11DestroyDevice();

    HRESULT DrawBounds(ID3D11DeviceContext* pDC, const D3DXMATRIX& mLocalToWorld, const D3DXVECTOR4& minCorner, const D3DXVECTOR4& maxCorner, const D3DXVECTOR4& rgba);
    HRESULT DrawBoundsArrows(ID3D11DeviceContext* pDC, const D3DXMATRIX& mLocalToWorld, const D3DXVECTOR4& minCorner, const D3DXVECTOR4& maxCorner, const D3DXVECTOR4& rgba);

    HRESULT SetLineColor(const D3DXVECTOR4& rgba);

	HRESULT BindToEffect(ID3D11Device* pd3dDevice, ID3DX11Effect* pEffect);

private:

    CLineDraw();

    HRESULT DrawArrow(ID3D11DeviceContext* pDC, const D3DXVECTOR4& start, const D3DXVECTOR4& end, const D3DXVECTOR4& throwVector);

    ID3DX11EffectVectorVariable* m_LineColor_EffectVar;
    ID3DX11EffectMatrixVariable* m_mLocalToWorld_EffectVar;
    ID3DX11EffectPass*           m_pEffectPass;
    ID3D11InputLayout*           m_pVertexLayout;

};



#endif // LINEDRAW_H
