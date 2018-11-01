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

#include "Util.h"

void TransformBounds(const D3DXMATRIX& m, const D3DXVECTOR4& InMin, const D3DXVECTOR4& InMax, D3DXVECTOR4& OutMin, D3DXVECTOR4& OutMax)
{
    D3DXVECTOR4 SrcVerts[8];
    SrcVerts[0] = D3DXVECTOR4(InMin.x, InMin.y, InMin.z, 1.f);
    SrcVerts[1] = D3DXVECTOR4(InMin.x, InMin.y, InMax.z, 1.f);
    SrcVerts[2] = D3DXVECTOR4(InMax.x, InMin.y, InMax.z, 1.f);
    SrcVerts[3] = D3DXVECTOR4(InMax.x, InMin.y, InMin.z, 1.f);
    SrcVerts[4] = D3DXVECTOR4(InMin.x, InMax.y, InMin.z, 1.f);
    SrcVerts[5] = D3DXVECTOR4(InMin.x, InMax.y, InMax.z, 1.f);
    SrcVerts[6] = D3DXVECTOR4(InMax.x, InMax.y, InMax.z, 1.f);
    SrcVerts[7] = D3DXVECTOR4(InMax.x, InMax.y, InMin.z, 1.f);

    D3DXVECTOR4 DstVert;
    D3DXVec4Transform(&DstVert, &SrcVerts[0], &m);
    OutMin = DstVert;
    OutMax = DstVert;

    for(int i = 1; i != 8; ++i)
    {
        D3DXVec4Transform(&DstVert, &SrcVerts[i], &m);
        D3DXVec4Minimize(&OutMin,&OutMin,&DstVert);
        D3DXVec4Maximize(&OutMax,&OutMax,&DstVert);
    }
}

CDrawUP::CDrawUP() :
    m_pUPVertexBuffer(NULL),
    m_UPVertexBufferSize(4 * 1024),
    m_UPVertexBufferCursor(UINT(-1))
{
}

CDrawUP& CDrawUP::Instance()
{
    static CDrawUP theInstance;
    return theInstance;
}

HRESULT CDrawUP::OnD3D11CreateDevice(ID3D11Device* pd3dDevice)
{
    HRESULT hr;

    // Set up dynamic VB
    D3D11_BUFFER_DESC vbDesc;
    vbDesc.ByteWidth = m_UPVertexBufferSize;
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vbDesc.MiscFlags = 0;
    vbDesc.StructureByteStride = 0;

    V_RETURN( pd3dDevice->CreateBuffer(&vbDesc, NULL, &m_pUPVertexBuffer) );

    return S_OK;
}

void CDrawUP::OnD3D11DestroyDevice()
{
    SAFE_RELEASE( m_pUPVertexBuffer );
}

void CDrawUP::OnBeginFrame()
{
    m_UPVertexBufferCursor = UINT(-1);
}

HRESULT CDrawUP::DrawUP(ID3D11DeviceContext* pDC, const void* pVerts, UINT VertStride, UINT NumVerts)
{
    HRESULT hr;

    const UINT NumBytes = VertStride * NumVerts;

    bool discard = false;
    if(UINT(-1) == m_UPVertexBufferCursor)
    {
        discard = true;
    }
    else
    {
        UINT endCursor = m_UPVertexBufferCursor + NumBytes;
        if(endCursor > m_UPVertexBufferSize)
        {
            discard = true;
        }
    }

    D3D11_MAP mapType = D3D11_MAP_WRITE_NO_OVERWRITE;
    if(discard)
    {
        mapType = D3D11_MAP_WRITE_DISCARD;
        m_UPVertexBufferCursor=  0;
    }

    D3D11_MAPPED_SUBRESOURCE mappy;
    V_RETURN(pDC->Map(m_pUPVertexBuffer,0,mapType,0,&mappy));

    BYTE* pData = (reinterpret_cast<BYTE*>(mappy.pData) + m_UPVertexBufferCursor);

    memcpy(pData, pVerts, NumBytes);
    pDC->Unmap(m_pUPVertexBuffer, 0);

    pDC->IASetVertexBuffers(0, 1, &m_pUPVertexBuffer, &VertStride, &m_UPVertexBufferCursor);
    pDC->Draw(NumVerts, 0);

    m_UPVertexBufferCursor += NumBytes;

    return S_OK;
}

CScreenAlignedQuad& CScreenAlignedQuad::Instance()
{
    static CScreenAlignedQuad theInstance;
    return theInstance;
}

CScreenAlignedQuad::CScreenAlignedQuad() :
    m_ScaleAndOffset_EffectVar(NULL),
    m_Src_EffectVar(NULL),
    m_Mip_EffectVar(NULL),
    m_pEffectPass(NULL),
    m_pTranslucentEffectPass(NULL)
{
}

HRESULT CScreenAlignedQuad::BindToEffect(ID3DX11Effect* pEffect)
{
    m_ScaleAndOffset_EffectVar = pEffect->GetVariableByName("g_ScreenAlignedQuadScaleAndOffset")->AsVector();
    m_Src_EffectVar = pEffect->GetVariableByName("g_ScreenAlignedQuadSrc")->AsShaderResource();
    m_Mip_EffectVar = pEffect->GetVariableByName("g_ScreenAlignedQuadMip")->AsScalar();
    m_pEffectPass = pEffect->GetTechniqueByName("ScreenAlignedQuad")->GetPassByIndex(0);
    m_pTranslucentEffectPass = pEffect->GetTechniqueByName("ScreenAlignedQuadTranslucent")->GetPassByIndex(0);

    return S_OK;
}

HRESULT CScreenAlignedQuad::OnD3D11CreateDevice(ID3D11Device* pd3dDevice)
{
    return S_OK;
}

void CScreenAlignedQuad::OnD3D11DestroyDevice()
{
}

HRESULT CScreenAlignedQuad::DrawQuad(ID3D11DeviceContext* pDC, ID3D11ShaderResourceView* pSrc, UINT srcMip, const RECT* pDstRect, ID3DX11EffectPass* pCustomPass)
{
    HRESULT hr;

    // Set up effect params
    V_RETURN(m_Src_EffectVar->SetResource(pSrc));
    V_RETURN(m_Mip_EffectVar->SetFloat(FLOAT(srcMip)));

    // Do the draw
    V_RETURN(DrawQuad(pDC, pDstRect, pCustomPass));

    // Ensure src is unbound
    ID3DX11EffectPass* pPass = pCustomPass ? pCustomPass : m_pEffectPass;
    V_RETURN(m_Src_EffectVar->SetResource(NULL));
    V_RETURN(pPass->Apply(0, pDC));

    return S_OK;
}

HRESULT CScreenAlignedQuad::SetScaleAndOffset(ID3D11DeviceContext* pDC, const RECT* pDstRect)
{
    D3DXVECTOR4 vScaleAndOffset;
    if(pDstRect)
    {
        D3D11_VIEWPORT currViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        UINT numViewports = sizeof(currViewports)/sizeof(currViewports[0]);
        pDC->RSGetViewports(&numViewports, currViewports);

        const D3D11_VIEWPORT& currViewport = currViewports[0];
        vScaleAndOffset = D3DXVECTOR4(
             2.f * FLOAT(pDstRect->right - pDstRect->left)/currViewport.Width,
            -2.f * FLOAT(pDstRect->bottom - pDstRect->top)/currViewport.Height,
             2.f * FLOAT(pDstRect->left)/currViewport.Width - 1.f,
            -2.f * FLOAT(pDstRect->top)/currViewport.Height + 1.f
            );
    }
    else
    {
        // Use the whole dst
        vScaleAndOffset = D3DXVECTOR4(2.f,-2.f,-1.f,1.f);
    }

    // Tex-coords -> clip space
    return m_ScaleAndOffset_EffectVar->SetFloatVector((FLOAT*)vScaleAndOffset);
}

HRESULT CScreenAlignedQuad::DrawQuad(ID3D11DeviceContext* pDC, const RECT* pDstRect, ID3DX11EffectPass* pCustomPass)
{
    HRESULT hr;

    // Set up effect params
    V_RETURN(SetScaleAndOffset(pDC, pDstRect));

    ID3DX11EffectPass* pPass = pCustomPass ? pCustomPass : m_pEffectPass;
    V_RETURN(pPass->Apply(0, pDC));

    V_RETURN(DrawQuad(pDC));

    return S_OK;
}

HRESULT CScreenAlignedQuad::DrawQuadTranslucent(ID3D11DeviceContext* pDC, ID3D11ShaderResourceView* pSrc, UINT srcMip, const RECT* pDstRect)
{
    return DrawQuad(pDC, pSrc, srcMip, pDstRect, m_pTranslucentEffectPass);
}

HRESULT CScreenAlignedQuad::DrawQuad(ID3D11DeviceContext* pDC)
{
    pDC->IASetInputLayout(NULL);
    pDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    pDC->Draw( 4, 0 );

    return S_OK;
}

const D3D11_INPUT_ELEMENT_DESC* PositionOnlyVertex::GetLayout()
{
    static const D3D11_INPUT_ELEMENT_DESC theLayout[NumLayoutElements] = {
        // SemanticName  SemanticIndex  Format                        InputSlot    AlignedByteOffset              InputSlotClass               InstanceDataStepRate
        { "POSITION",    0,             DXGI_FORMAT_R32G32B32_FLOAT,  0,           D3D11_APPEND_ALIGNED_ELEMENT,  D3D11_INPUT_PER_VERTEX_DATA, 0                    }
    };

    return theLayout;
}

HRESULT GetAAModeFromSampleDesc(const DXGI_SAMPLE_DESC& inDesc, MSAAMode& outMode)
{
    switch(inDesc.Count)
    {
    case 1: outMode = NoAA;
        break;
    case 2: outMode = s2;
        break;
    case 4: outMode = s4;
        break;
    case 8: outMode = s8;
        break;
    case 16: outMode = s16;
        break;
    case 32: outMode = s32;
        break;
    case 48: outMode = s48;
        break;
    default:
        return E_FAIL;
    }

	return S_OK;
}

namespace
{
    LPCSTR kSampleDefines[NumMSAAModes] = {
        "1",
        "2",
        "4",
        "8",
        "16",
        "32",
        "48"
    };
}

LPCSTR GetStringFromAAMode(MSAAMode mode)
{
	return kSampleDefines[mode];
}
