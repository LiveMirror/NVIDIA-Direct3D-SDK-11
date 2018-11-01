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

#include "LineDraw.h"
#include "Util.h"

CLineDraw::CLineDraw() :
    m_LineColor_EffectVar(NULL),
    m_mLocalToWorld_EffectVar(NULL),
    m_pEffectPass(NULL),
    m_pVertexLayout(NULL)
{
}

CLineDraw& CLineDraw::Instance()
{
    static CLineDraw theInstance;
    return theInstance;
}

HRESULT CLineDraw::BindToEffect(ID3D11Device* pd3dDevice, ID3DX11Effect* pEffect)
{
	HRESULT hr;

    m_pEffectPass = pEffect->GetTechniqueByName("RenderLines")->GetPassByIndex(0);
    m_LineColor_EffectVar = pEffect->GetVariableByName("g_LineColor")->AsVector();
    m_mLocalToWorld_EffectVar = pEffect->GetVariableByName("g_mLocalToWorld")->AsMatrix();

	SAFE_RELEASE(m_pVertexLayout);
    D3DX11_PASS_DESC passDesc;
    m_pEffectPass->GetDesc(&passDesc);
    V_RETURN( pd3dDevice->CreateInputLayout( PositionOnlyVertex::GetLayout(), PositionOnlyVertex::NumLayoutElements,
                                             passDesc.pIAInputSignature, passDesc.IAInputSignatureSize,
                                             &m_pVertexLayout) );

	return S_OK;
}

HRESULT CLineDraw::OnD3D11CreateDevice(ID3D11Device* pd3dDevice)
{
	return S_OK;
}

void CLineDraw::OnD3D11DestroyDevice()
{
    SAFE_RELEASE(m_pVertexLayout);
}

HRESULT CLineDraw::DrawArrow(ID3D11DeviceContext* pDC, const D3DXVECTOR4& start, const D3DXVECTOR4& end, const D3DXVECTOR4& throwVector)
{
    HRESULT hr;

    pDC->IASetInputLayout(m_pVertexLayout);

    PositionOnlyVertex vStart, vEnd, vLeft, vRight;
    vStart.position = D3DXVECTOR3(start);
    vEnd.position = D3DXVECTOR3(end);
    vLeft.position = (0.2f * vStart.position + 0.8f * vEnd.position) - D3DXVECTOR3(throwVector);
    vRight.position = vLeft.position + 2.f * D3DXVECTOR3(throwVector);

    PositionOnlyVertex verts[6];
    PositionOnlyVertex* pCurrLineVert = verts;
    *pCurrLineVert++ = vStart; *pCurrLineVert++ = vEnd;
    *pCurrLineVert++ = vLeft; *pCurrLineVert++ = vEnd;
    *pCurrLineVert++ = vRight; *pCurrLineVert++ = vEnd;

    pDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    V_RETURN( CDrawUP::Instance().DrawUP(pDC, verts, sizeof(verts[0]), sizeof(verts)/sizeof(verts[0])) );

    return S_OK;
}

HRESULT CLineDraw::DrawBoundsArrows(ID3D11DeviceContext* pDC, const D3DXMATRIX& mLocalToWorld, const D3DXVECTOR4& minCorner, const D3DXVECTOR4& maxCorner, const D3DXVECTOR4& rgba)
{
    HRESULT hr;

    V_RETURN( m_mLocalToWorld_EffectVar->SetMatrix( (FLOAT*)&mLocalToWorld) );
    V_RETURN( m_LineColor_EffectVar->SetFloatVector( (FLOAT*)&rgba ) );
    V_RETURN( m_pEffectPass->Apply(0, pDC) );

    D3DXVECTOR4 start;
    D3DXVECTOR4 end;
    D3DXVECTOR4 throwVec;

    const FLOAT loX = 0.75f * minCorner.x + 0.25f * maxCorner.x;
    const FLOAT midX = 0.5f * minCorner.x + 0.5f * maxCorner.x;
    const FLOAT hiX = 0.25f * minCorner.x + 0.75f * maxCorner.x;
    const FLOAT loY = 0.75f * minCorner.y + 0.25f * maxCorner.y;
    const FLOAT midY = 0.5f * minCorner.y + 0.5f * maxCorner.y;
    const FLOAT hiY = 0.25f * minCorner.y + 0.75f * maxCorner.y;
    const FLOAT loZ = 0.85f * minCorner.z + 0.15f * maxCorner.z;
    const FLOAT hiZ = 0.15f * minCorner.z + 0.85f * maxCorner.z;
    const FLOAT yThrow = 0.05f * (maxCorner.y - minCorner.y);
    const FLOAT xThrow = 0.05f * (maxCorner.x - minCorner.x);

    // Min x face
    throwVec = D3DXVECTOR4(0.f, yThrow, 0.f, 0.f);
    start = D3DXVECTOR4(minCorner.x, loY, loZ, 0.f);
    end   = D3DXVECTOR4(minCorner.x, loY, hiZ, 0.f);
    V_RETURN(DrawArrow(pDC, start, end, throwVec));

    start = D3DXVECTOR4(minCorner.x, midY, loZ, 0.f);
    end   = D3DXVECTOR4(minCorner.x, midY, hiZ, 0.f);
    V_RETURN(DrawArrow(pDC, start, end, throwVec));

    start = D3DXVECTOR4(minCorner.x, hiY, loZ, 0.f);
    end   = D3DXVECTOR4(minCorner.x, hiY, hiZ, 0.f);
    V_RETURN(DrawArrow(pDC, start, end, throwVec));

    // Max x face
    throwVec = D3DXVECTOR4(0.f, yThrow, 0.f, 0.f);
    start = D3DXVECTOR4(maxCorner.x, loY, loZ, 0.f);
    end   = D3DXVECTOR4(maxCorner.x, loY, hiZ, 0.f);
    V_RETURN(DrawArrow(pDC, start, end, throwVec));

    start = D3DXVECTOR4(maxCorner.x, midY, loZ, 0.f);
    end   = D3DXVECTOR4(maxCorner.x, midY, hiZ, 0.f);
    V_RETURN(DrawArrow(pDC, start, end, throwVec));

    start = D3DXVECTOR4(maxCorner.x, hiY, loZ, 0.f);
    end   = D3DXVECTOR4(maxCorner.x, hiY, hiZ, 0.f);
    V_RETURN(DrawArrow(pDC, start, end, throwVec));

    // Min y face
    throwVec = D3DXVECTOR4(xThrow, 0.f, 0.f, 0.f);
    start = D3DXVECTOR4(loX, minCorner.y, loZ, 0.f);
    end   = D3DXVECTOR4(loX, minCorner.y, hiZ, 0.f);
    V_RETURN(DrawArrow(pDC, start, end, throwVec));

    start = D3DXVECTOR4(midX, minCorner.y, loZ, 0.f);
    end   = D3DXVECTOR4(midX, minCorner.y, hiZ, 0.f);
    V_RETURN(DrawArrow(pDC, start, end, throwVec));

    start = D3DXVECTOR4(hiX, minCorner.y, loZ, 0.f);
    end   = D3DXVECTOR4(hiX, minCorner.y, hiZ, 0.f);
    V_RETURN(DrawArrow(pDC, start, end, throwVec));

    // Max y face
    throwVec = D3DXVECTOR4(xThrow, 0.f, 0.f, 0.f);
    start = D3DXVECTOR4(loX, maxCorner.y, loZ, 0.f);
    end   = D3DXVECTOR4(loX, maxCorner.y, hiZ, 0.f);
    V_RETURN(DrawArrow(pDC, start, end, throwVec));

    start = D3DXVECTOR4(midX, maxCorner.y, loZ, 0.f);
    end   = D3DXVECTOR4(midX, maxCorner.y, hiZ, 0.f);
    V_RETURN(DrawArrow(pDC, start, end, throwVec));

    start = D3DXVECTOR4(hiX, maxCorner.y, loZ, 0.f);
    end   = D3DXVECTOR4(hiX, maxCorner.y, hiZ, 0.f);
    V_RETURN(DrawArrow(pDC, start, end, throwVec));

    return S_OK;
}

HRESULT CLineDraw::DrawBounds(ID3D11DeviceContext* pDC, const D3DXMATRIX& mLocalToWorld, const D3DXVECTOR4& minCorner, const D3DXVECTOR4& maxCorner, const D3DXVECTOR4& rgba)
{
    HRESULT hr;

    V_RETURN( m_mLocalToWorld_EffectVar->SetMatrix( (FLOAT*)&mLocalToWorld) );
    V_RETURN( m_LineColor_EffectVar->SetFloatVector( (FLOAT*)&rgba ) );
    V_RETURN( m_pEffectPass->Apply(0, pDC) );

    pDC->IASetInputLayout(m_pVertexLayout);

    PositionOnlyVertex boundsVerts[8];
    boundsVerts[0].position = D3DXVECTOR3(minCorner.x, minCorner.y, minCorner.z);
    boundsVerts[1].position = D3DXVECTOR3(minCorner.x, minCorner.y, maxCorner.z);
    boundsVerts[2].position = D3DXVECTOR3(maxCorner.x, minCorner.y, maxCorner.z);
    boundsVerts[3].position = D3DXVECTOR3(maxCorner.x, minCorner.y, minCorner.z);
    boundsVerts[4].position = D3DXVECTOR3(minCorner.x, maxCorner.y, minCorner.z);
    boundsVerts[5].position = D3DXVECTOR3(minCorner.x, maxCorner.y, maxCorner.z);
    boundsVerts[6].position = D3DXVECTOR3(maxCorner.x, maxCorner.y, maxCorner.z);
    boundsVerts[7].position = D3DXVECTOR3(maxCorner.x, maxCorner.y, minCorner.z);

    PositionOnlyVertex verts[24];
    PositionOnlyVertex* pCurrLineVert = verts;

    *pCurrLineVert++ = boundsVerts[0]; *pCurrLineVert++ = boundsVerts[1];
    *pCurrLineVert++ = boundsVerts[1]; *pCurrLineVert++ = boundsVerts[2];
    *pCurrLineVert++ = boundsVerts[2]; *pCurrLineVert++ = boundsVerts[3];
    *pCurrLineVert++ = boundsVerts[3]; *pCurrLineVert++ = boundsVerts[0];
    *pCurrLineVert++ = boundsVerts[4]; *pCurrLineVert++ = boundsVerts[5];
    *pCurrLineVert++ = boundsVerts[5]; *pCurrLineVert++ = boundsVerts[6];
    *pCurrLineVert++ = boundsVerts[6]; *pCurrLineVert++ = boundsVerts[7];
    *pCurrLineVert++ = boundsVerts[7]; *pCurrLineVert++ = boundsVerts[4];
    *pCurrLineVert++ = boundsVerts[0]; *pCurrLineVert++ = boundsVerts[4];
    *pCurrLineVert++ = boundsVerts[1]; *pCurrLineVert++ = boundsVerts[5];
    *pCurrLineVert++ = boundsVerts[2]; *pCurrLineVert++ = boundsVerts[6];
    *pCurrLineVert++ = boundsVerts[3]; *pCurrLineVert++ = boundsVerts[7];

    pDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    V_RETURN( CDrawUP::Instance().DrawUP(pDC, verts, sizeof(verts[0]), sizeof(verts)/sizeof(verts[0])) );

    return S_OK;
}

HRESULT CLineDraw::SetLineColor(const D3DXVECTOR4& rgba)
{
    return m_LineColor_EffectVar->SetFloatVector( (FLOAT*)&rgba );
}
