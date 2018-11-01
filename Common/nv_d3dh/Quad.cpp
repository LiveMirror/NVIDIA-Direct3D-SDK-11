#include "Precompiledheader.h"
#include "Primitives.h"


//////////////////////////////////////////////////////////////////////////////////////////////////////////
////
d3d11::Quad::Quad(ID3D11Device* a_pDevice, ID3DX11EffectTechnique* a_pTechniqueDrawQuad)
{
    m_pDevice = a_pDevice;
    m_pContext = GetCtx(a_pDevice);
    m_pTechnique = a_pTechniqueDrawQuad;

    VPosTex vertices[] =
    {
        { D3DXVECTOR3( -1.0f, 1.0f, 0.0f ),  D3DXVECTOR2(0,1), },
        { D3DXVECTOR3( -1.0f, -1.0f, 0.0f ), D3DXVECTOR2(0,0), },
        { D3DXVECTOR3( 1.0f, 1.0f, 0.0f ),   D3DXVECTOR2(1,1), },
        { D3DXVECTOR3( 1.0f, -1.0f, 0.0f ),  D3DXVECTOR2(1,0), },
    };

    m_quadVB.SetFromData(vertices,4);
    m_quadVB.CopyHostToDevice(m_pDevice);

    uint indices[] =
    {
        1,0,2,
        1,2,3,
    };

    m_quadIB.SetFromData(indices,6);
    m_quadIB.CopyHostToDevice(m_pDevice);
    
    // Create input layout for quad
    //
    D3DX11_PASS_DESC passDesc;
    DX_SAFE_CALL(a_pTechniqueDrawQuad->GetPassByIndex(0)->GetDesc(&passDesc));
    
    const D3D11_INPUT_ELEMENT_DESC layout2[] =
    {
      { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };


    DX_SAFE_CALL(m_pDevice->CreateInputLayout(layout2, sizeof(layout2)/sizeof(D3D11_INPUT_ELEMENT_DESC), 
                                              passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &m_pQuadVertexlayout));

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
////
d3d11::Quad::~Quad()
{
    SAFE_RELEASE(m_pContext);
    SAFE_RELEASE(m_pQuadVertexlayout);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Quad::Draw()
{
    m_quadVB.Attach(m_pContext);
    m_quadIB.Attach(m_pContext);

    m_pContext->IASetInputLayout(m_pQuadVertexlayout);
    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_pTechnique->GetPassByIndex(0)->Apply(0,m_pContext);

    // draw 2 triangles (6 vertices)
    //
    m_pContext->DrawIndexed(6, 0, 0);    
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
////
void d3d11::Quad::DrawCall()
{
    m_quadVB.Attach(m_pContext);
    m_quadIB.Attach(m_pContext);

    m_pContext->IASetInputLayout(m_pQuadVertexlayout);
    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // draw 2 triangles (6 vertices)
    //
    m_pContext->DrawIndexed(6, 0, 0);    
}


