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
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "Strsafe.h"
#include <algorithm>
#include <limits>

#define MAX_NUM_MESHES 6
#define GROUND_DIFFUSE_MAP_FILENAME L"lichen6.dds"
#define GROUND_NORMAL_MAP_FILENAME  L"lichen6_normal.dds"
#define GROUND_PLANE_RADIUS 8.0f
#define LIGHT_ZFAR 32.0f

// Setting the shadow-map resolution to 512x512.
// Any resolution may be used as long as the shadow maps fit in video memory.
#define SHADOW_MAP_RES 512

// In this implementation, the number of shadow maps is hard-coded to 28.
// For changing this value, the Poisson disk and the shaders should be modified accordingly.
#define SHADOW_MAP_ARRAY_SIZE 28

//--------------------------------------------------------------------------------------
// Textures used for shading
//--------------------------------------------------------------------------------------
class SceneTextures
{
public:
    SceneTextures()
      : m_pQuadDiffuseTexture(NULL)
      , m_pQuadDiffuseSRV(NULL)
      , m_pQuadNormalTexture(NULL)
      , m_pQuadNormalSRV(NULL)
    {
    }

    void CreateResources(ID3D11Device *pDevice)
    {
        HRESULT hr;

        WCHAR str[256];
        V( DXUTFindDXSDKMediaFileCch( str, 256, GROUND_DIFFUSE_MAP_FILENAME ) );
        V( D3DX11CreateTextureFromFile( pDevice, str, NULL, NULL, (ID3D11Resource**)&m_pQuadDiffuseTexture, &hr ) );
        V( pDevice->CreateShaderResourceView( m_pQuadDiffuseTexture, NULL, &m_pQuadDiffuseSRV ) );

        V( DXUTFindDXSDKMediaFileCch( str, 256, GROUND_NORMAL_MAP_FILENAME ) );
        V( D3DX11CreateTextureFromFile( pDevice, str, NULL, NULL, (ID3D11Resource**)&m_pQuadNormalTexture, &hr ) );
        V( pDevice->CreateShaderResourceView( m_pQuadNormalTexture, NULL, &m_pQuadNormalSRV ) );
    }

    void ReleaseResources()
    {
        SAFE_RELEASE(m_pQuadDiffuseTexture);
        SAFE_RELEASE(m_pQuadDiffuseSRV);

        SAFE_RELEASE(m_pQuadNormalTexture);
        SAFE_RELEASE(m_pQuadNormalSRV);
    }

    ID3D11ShaderResourceView* GetGroundDiffuseSRV()
    {
        return m_pQuadDiffuseSRV;
    }

    ID3D11ShaderResourceView* GetGroundNormalSRV()
    {
        return m_pQuadNormalSRV;
    }

private:
    ID3D11Texture2D*            m_pQuadDiffuseTexture;
    ID3D11ShaderResourceView*   m_pQuadDiffuseSRV;
    ID3D11Texture2D*            m_pQuadNormalTexture;
    ID3D11ShaderResourceView*   m_pQuadNormalSRV;
};

//--------------------------------------------------------------------------------------
// Vertex buffer and index buffer for drawing the ground plane
//--------------------------------------------------------------------------------------
class SceneGroundPlane
{
public:
    SceneGroundPlane()
        : m_pVertexBuffer(NULL)
        , m_pIndexBuffer(NULL)
    {
    }

   struct SimpleVertex
    {
        D3DXVECTOR3 Pos;
        D3DXVECTOR3 Normal;
    };

    HRESULT CreateResources(ID3D11Device *pd3dDevice, float radius, float height)
    {
        HRESULT hr;

        SimpleVertex vertices[] =
        {
            { D3DXVECTOR3( -radius, height,  radius ), D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) },
            { D3DXVECTOR3(  radius, height,  radius ), D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) },
            { D3DXVECTOR3(  radius, height, -radius ), D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) },
            { D3DXVECTOR3( -radius, height, -radius ), D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) },
        };
        D3D11_BUFFER_DESC bd;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof( SimpleVertex ) * 4;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = 0;
        bd.MiscFlags = 0;
        D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = vertices;
        hr = pd3dDevice->CreateBuffer( &bd, &InitData, &m_pVertexBuffer );
        if( FAILED(hr) )
            return hr;

        DWORD indices[] =
        {
            0,1,2,
            0,2,3,
        };

        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof( DWORD ) * 6;
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bd.CPUAccessFlags = 0;
        bd.MiscFlags = 0;
        InitData.pSysMem = indices;
        hr = pd3dDevice->CreateBuffer( &bd, &InitData, &m_pIndexBuffer );
        if( FAILED(hr) )
            return hr;

        return S_OK;
    }

    void DrawPlane(ID3D11DeviceContext* pd3dContext)
    {
        UINT stride = sizeof( SimpleVertex );
        UINT offset = 0;
        pd3dContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
        pd3dContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
        pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pd3dContext->DrawIndexed(6, 0, 0);
    }

    void ReleaseResources()
    {
        SAFE_RELEASE(m_pVertexBuffer);
        SAFE_RELEASE(m_pIndexBuffer);
    }

private:
    ID3D11Buffer* m_pVertexBuffer;
    ID3D11Buffer* m_pIndexBuffer;
};

//--------------------------------------------------------------------------------------
// Ground plane and meshes for the demo scene
//--------------------------------------------------------------------------------------
class SceneGeometry
{
public:
    SceneGeometry()
        : m_NumCharacters(2)
    {
    }

    void CreateResources(ID3D11Device *pd3dDevice)
    {
        for (int i = 0; i < MAX_NUM_MESHES; ++i)
        {
            WCHAR str[100];
            StringCchPrintf(str, 100, L"..\\Media\\goof_knight%d.sdkmesh", i+1);

            HRESULT hr;
            V( m_CharacterMeshes[i].Create(pd3dDevice, str) );
        }

        D3DXVECTOR3 bbExtents = m_CharacterMeshes[0].GetMeshBBoxExtents(0);
        D3DXVECTOR3 bbCenter = m_CharacterMeshes[0].GetMeshBBoxCenter(0);
        float Height = bbCenter.y - bbExtents.y;
        m_GroundPlane.CreateResources(pd3dDevice, GROUND_PLANE_RADIUS, Height);
    }

    void ReleaseResources()
    {
        for (UINT i = 0; i < MAX_NUM_MESHES; ++i)
        {
            m_CharacterMeshes[i].Destroy();
        }
        m_GroundPlane.ReleaseResources();
    }

    void DrawMesh(ID3D11DeviceContext* pd3dContext, CDXUTSDKMesh &mesh)
    {
        UINT Strides[1];
        UINT Offsets[1];
        ID3D11Buffer* pVB[1];
        pVB[0] = mesh.GetVB11(0,0);
        Strides[0] = (UINT)mesh.GetVertexStride(0,0);
        Offsets[0] = 0;
        pd3dContext->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
        pd3dContext->IASetIndexBuffer( mesh.GetIB11(0), mesh.GetIBFormat11(0), 0 );

        for (UINT subset = 0; subset < mesh.GetNumSubsets(0); ++subset)
        {
            SDKMESH_SUBSET* pSubset = mesh.GetSubset( 0, subset );
            D3D11_PRIMITIVE_TOPOLOGY PrimType = mesh.GetPrimitiveType11( (SDKMESH_PRIMITIVE_TYPE)pSubset->PrimitiveType );
            pd3dContext->IASetPrimitiveTopology( PrimType );
            pd3dContext->DrawIndexed( (UINT)pSubset->IndexCount, 0, (UINT)pSubset->VertexStart );
        }
    }

    void SetNumCharacters(UINT NumCharacters)
    {
        m_NumCharacters = NumCharacters;
    }

    void DrawCharacters(ID3D11DeviceContext* pd3dContext)
    {
        for (UINT i = 0; i < m_NumCharacters; ++i)
        {
            DrawMesh(pd3dContext, m_CharacterMeshes[i]);
        }
    }

    void DrawPlane(ID3D11DeviceContext* pd3dContext)
    {
        m_GroundPlane.DrawPlane(pd3dContext);
    }

    UINT GetNumMeshTriangles()
    {
        UINT NumIndices = 0;
        for (UINT i = 0; i < m_NumCharacters; ++i)
        {
            NumIndices += (UINT)m_CharacterMeshes[i].GetNumIndices(0);
        }
        return NumIndices / 3;
    }

    void ComputeWorldSpaceBoundingBox(D3DXVECTOR3 *BBox)
    {
        BBox[0] = D3DXVECTOR3(FLT_MAX, FLT_MAX, FLT_MAX);
        BBox[1] = D3DXVECTOR3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (UINT MeshIndex = 0; MeshIndex < MAX_NUM_MESHES; ++MeshIndex)
        {
            D3DXVECTOR3 bbCenter = m_CharacterMeshes[MeshIndex].GetMeshBBoxCenter(0);
            D3DXVECTOR3 bbExtents = m_CharacterMeshes[MeshIndex].GetMeshBBoxExtents(0);
            D3DXVECTOR3 bbMin = bbCenter - bbExtents;
            D3DXVECTOR3 bbMax = bbCenter + bbExtents;

            for (int k = 0; k < 3; ++k)
            {
                BBox[0][k] = std::min(BBox[0][k], bbMin[k]);
                BBox[1][k] = std::max(BBox[1][k], bbMax[k]);
            }
        }
    }

private:
    SceneGroundPlane m_GroundPlane;
    CDXUTSDKMesh m_CharacterMeshes[MAX_NUM_MESHES];
    UINT m_NumCharacters;
};

//--------------------------------------------------------------------------------------
// Global constant buffer
//--------------------------------------------------------------------------------------
class SceneConstantBuffer
{
public:
    ID3D11Buffer *pBuffer;

    SceneConstantBuffer() :
        pBuffer(NULL)
    {
    }

    struct
    {
        // With D3D 10/11, constant buffers are required to be 16-byte aligned.
        D3DXMATRIX  WorldToEyeClip;
        D3DXMATRIX  WorldToEyeView;
        D3DXMATRIX  EyeClipToEyeView;
        D3DXMATRIX  EyeViewToEyeClip;
        D3DXMATRIX  EyeViewToLightTex[SHADOW_MAP_ARRAY_SIZE];
        float       LightWorldPos[4];
        float       LightZNear;
        float       LightZFar;
        float       Pad[2];
    } CBData;

    void CreateResources(ID3D11Device *pd3dDevice)
    {
        HRESULT hr;

        static D3D11_BUFFER_DESC desc = 
        {
             sizeof(CBData), //ByteWidth
             D3D11_USAGE_DEFAULT, //Usage
             D3D11_BIND_CONSTANT_BUFFER, //BindFlags
             0, //CPUAccessFlags
             0, //MiscFlags
             0, //StructureByteStride (new in D3D11)
        };
        V( pd3dDevice->CreateBuffer(&desc, NULL, &pBuffer) );
    }

    void ReleaseResources()
    {
        SAFE_RELEASE(pBuffer);
    }
};

//--------------------------------------------------------------------------------------
// Shadow-map constant buffer
//--------------------------------------------------------------------------------------
class ShadowMapConstantBuffer
{
public:
    ID3D11Buffer *pBuffer;

    ShadowMapConstantBuffer() :
        pBuffer(NULL)
    {
    }

    struct
    {
        D3DXMATRIX  WorldToLightClip;
    } CBData;

    void CreateResources(ID3D11Device *pd3dDevice)
    {
        HRESULT hr;

        static D3D11_BUFFER_DESC desc = 
        {
             sizeof(CBData), //ByteWidth
             D3D11_USAGE_DEFAULT, //Usage
             D3D11_BIND_CONSTANT_BUFFER, //BindFlags
             0, //CPUAccessFlags
             0, //MiscFlags
             0, //StructureByteStride (new in D3D11)
        };
        V( pd3dDevice->CreateBuffer(&desc, NULL, &pBuffer) );
    }

    void ReleaseResources()
    {
        SAFE_RELEASE(pBuffer);
    }
};

//--------------------------------------------------------------------------------------
// Rasterizer states, blend states, depth-stencil states, and sampler states
//--------------------------------------------------------------------------------------
class SceneStates
{
public:
    ID3D11RasterizerState *pBackfaceCull_RS;
    ID3D11RasterizerState *pNoCull_RS;
    ID3D11RasterizerState *pDepthBiasBackfaceCull_RS;
    ID3D11DepthStencilState *pDepthNoStencil_DS;
    ID3D11DepthStencilState *pNoDepthNoStencil_DS;
    ID3D11BlendState *pNoBlend_BS;
    ID3D11SamplerState *pTrilinearWarpSampler;
    ID3D11SamplerState *pPointClampSampler;
    ID3D11SamplerState *pHwPcfSampler;

    void CreateResources(ID3D11Device *pd3dDevice)
    {
        CreateSamplers(pd3dDevice);
        CreateRasterizerStates(pd3dDevice);
        CreateDepthStencilStates(pd3dDevice);
        CreateBlendStates(pd3dDevice);
    }

    void ReleaseResources()
    {
        SAFE_RELEASE(pBackfaceCull_RS);
        SAFE_RELEASE(pNoCull_RS);
        SAFE_RELEASE(pDepthBiasBackfaceCull_RS);
        SAFE_RELEASE(pDepthNoStencil_DS);
        SAFE_RELEASE(pNoDepthNoStencil_DS);
        SAFE_RELEASE(pNoBlend_BS);
        SAFE_RELEASE(pTrilinearWarpSampler);
        SAFE_RELEASE(pPointClampSampler);
        SAFE_RELEASE(pHwPcfSampler);
    }

private:
    void CreateSamplers(ID3D11Device *pd3dDevice)
    {
        HRESULT hr;

        // Sampler state for the ground plane
        D3D11_SAMPLER_DESC samplerDesc;
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.MaxAnisotropy = 16;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        memset(&samplerDesc.BorderColor, 0, sizeof(samplerDesc.BorderColor));
        samplerDesc.MinLOD = -FLT_MAX;
        samplerDesc.MaxLOD = FLT_MAX;
        V( pd3dDevice->CreateSamplerState(&samplerDesc, &pTrilinearWarpSampler) );

        // Sampler state for visualizing the shadow maps
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        V( pd3dDevice->CreateSamplerState(&samplerDesc, &pPointClampSampler) );

        // Percentage Closer Filtering (PCF) with bilinear filtering
        samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.BorderColor[0] = 1.0f;
        V( pd3dDevice->CreateSamplerState(&samplerDesc, &pHwPcfSampler) );
    }

    void CreateRasterizerStates(ID3D11Device *pd3dDevice)
    {
        HRESULT hr;

        // Rasterizer state with backface culling
        D3D11_RASTERIZER_DESC rasterizerState;
        rasterizerState.FillMode = D3D11_FILL_SOLID;
        rasterizerState.CullMode = D3D11_CULL_BACK;
        rasterizerState.FrontCounterClockwise = FALSE;
        rasterizerState.DepthBias = FALSE;
        rasterizerState.DepthBiasClamp = 0;
        rasterizerState.SlopeScaledDepthBias = 0;
        rasterizerState.DepthClipEnable = FALSE;
        rasterizerState.ScissorEnable = FALSE;
        // With D3D 11, MultisampleEnable has no effect when rasterizing triangles:
        // MSAA rasterization is implicitely enabled as soon as the render target is MSAA.
        rasterizerState.MultisampleEnable = FALSE;
        rasterizerState.AntialiasedLineEnable = FALSE;
        V( pd3dDevice->CreateRasterizerState(&rasterizerState, &pBackfaceCull_RS) );

        // Rasterizer state with no backface culling (for fullscreen passes)
        rasterizerState.CullMode = D3D11_CULL_NONE;
        V( pd3dDevice->CreateRasterizerState(&rasterizerState, &pNoCull_RS) );

        // Rasterizer state with hardware depth bias (for the shadow-map generation passes)
        rasterizerState.CullMode = D3D11_CULL_BACK;
        rasterizerState.DepthBias = 1000;
        rasterizerState.SlopeScaledDepthBias = 8.0f;
        V( pd3dDevice->CreateRasterizerState(&rasterizerState, &pDepthBiasBackfaceCull_RS) );
    }

    void CreateDepthStencilStates(ID3D11Device *pd3dDevice)
    {
        HRESULT hr;

        // Depth-stencil state for the depth-only passes
        D3D11_DEPTH_STENCIL_DESC depthstencilState;
        depthstencilState.DepthEnable = TRUE;
        depthstencilState.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthstencilState.DepthFunc = D3D11_COMPARISON_LESS;
        depthstencilState.StencilEnable = FALSE;
        V( pd3dDevice->CreateDepthStencilState(&depthstencilState, &pDepthNoStencil_DS) );

        // Depth-stencil state for the fullscreen quads
        depthstencilState.DepthEnable = FALSE;
        depthstencilState.StencilEnable = FALSE;
        V( pd3dDevice->CreateDepthStencilState(&depthstencilState, &pNoDepthNoStencil_DS) );
    }

    void CreateBlendStates(ID3D11Device *pd3dDevice)
    {
        HRESULT hr;

        // Blending state with blending disabled
        D3D11_BLEND_DESC blendState;
        blendState.AlphaToCoverageEnable = FALSE;
        blendState.IndependentBlendEnable = FALSE; // new in D3D11
        for (int i = 0; i < 8; ++i)
        {
            blendState.RenderTarget[i].BlendEnable = FALSE;
            blendState.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }
        V( pd3dDevice->CreateBlendState(&blendState, &pNoBlend_BS) );
    }
};

//--------------------------------------------------------------------------------------
// Input layouts, vertex shaders and pixel shaders
//--------------------------------------------------------------------------------------
class SceneShaders
{
public:
    ID3D11InputLayout *pEyeRenderIA;
    ID3D11VertexShader *pEyeRenderVS;
    ID3D11PixelShader *pEyeRenderGroundPS;
    ID3D11PixelShader *pEyeRenderCharacterPS;
    ID3D11VertexShader *pGBufferFillVS;
    ID3D11PixelShader *pGBufferFillGroundPS;
    ID3D11PixelShader *pGBufferFillCharacterPS;
    ID3D11InputLayout *pLightRenderIA;
    ID3D11VertexShader *pRenderShadowMapVS;
    ID3D11VertexShader *pFullScreenTriangleVS;
    ID3D11PixelShader *pVisShadowMapPS;

    SceneShaders()
        : pEyeRenderIA(NULL)
        , pEyeRenderVS(NULL)
        , pEyeRenderGroundPS(NULL)
        , pEyeRenderCharacterPS(NULL)
        , pGBufferFillVS(NULL)
        , pGBufferFillGroundPS(NULL)
        , pGBufferFillCharacterPS(NULL)
        , pLightRenderIA(NULL)
        , pRenderShadowMapVS(NULL)
        , pFullScreenTriangleVS(NULL)
        , pVisShadowMapPS(NULL)
    {
    }

    void CreateResources(ID3D11Device *pd3dDevice)
    {
        HRESULT hr;

        const D3D11_INPUT_ELEMENT_DESC InputLayoutDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        UINT NumElements = sizeof(InputLayoutDesc)/sizeof(InputLayoutDesc[0]);

        //--------------------------------------------------------------------------------------
        // Load the shaders for the forward rendering
        //--------------------------------------------------------------------------------------

        WCHAR ShaderPath[MAX_PATH];
        V( DXUTFindDXSDKMediaFileCch( ShaderPath, MAX_PATH, L"Shaders\\MVSS_ForwardRendering.hlsl" ) );

        ID3DBlob *pBlob;
        V( CompileShaderFromFile(ShaderPath, "EyeRenderVS", "vs_5_0", &pBlob) );
        V( pd3dDevice->CreateVertexShader((DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &pEyeRenderVS) );
        V( pd3dDevice->CreateInputLayout(InputLayoutDesc, NumElements, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &pEyeRenderIA) );
        pBlob->Release();

        V( CompileShaderFromFile(ShaderPath, "EyeRenderGroundPS", "ps_5_0", &pBlob) );
        V( pd3dDevice->CreatePixelShader((DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &pEyeRenderGroundPS) );
        pBlob->Release();

        V( CompileShaderFromFile(ShaderPath, "EyeRenderCharacterPS", "ps_5_0", &pBlob ) );
        V( pd3dDevice->CreatePixelShader((DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &pEyeRenderCharacterPS) );
        pBlob->Release();

        //--------------------------------------------------------------------------------------
        // Load the shaders for the shadow-map generation and visualization passes
        //--------------------------------------------------------------------------------------

        V( DXUTFindDXSDKMediaFileCch( ShaderPath, MAX_PATH, L"Shaders\\MVSS_ShadowMapRendering.hlsl" ) );

        V( CompileShaderFromFile(ShaderPath, "RenderShadowMapVS", "vs_5_0", &pBlob ) );
        V( pd3dDevice->CreateVertexShader((DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &pRenderShadowMapVS) );
        V( pd3dDevice->CreateInputLayout(InputLayoutDesc, NumElements, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &pLightRenderIA) );
        pBlob->Release();

        V( DXUTFindDXSDKMediaFileCch( ShaderPath, MAX_PATH, L"Shaders\\MVSS_ShadowMapVisualization.hlsl" ) );

        V( CompileShaderFromFile(ShaderPath, "FullScreenTriangleVS", "vs_5_0", &pBlob ) );
        V( pd3dDevice->CreateVertexShader((DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &pFullScreenTriangleVS) );
        pBlob->Release();

        V( CompileShaderFromFile(ShaderPath, "VisShadowMapPS", "ps_5_0", &pBlob ) );
        V( pd3dDevice->CreatePixelShader((DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &pVisShadowMapPS) );
        pBlob->Release();
    }

    void ReleaseResources()
    {
        SAFE_RELEASE(pEyeRenderIA);
        SAFE_RELEASE(pEyeRenderVS);
        SAFE_RELEASE(pEyeRenderGroundPS);
        SAFE_RELEASE(pEyeRenderCharacterPS);
        SAFE_RELEASE(pGBufferFillVS);
        SAFE_RELEASE(pGBufferFillGroundPS);
        SAFE_RELEASE(pGBufferFillCharacterPS);
        SAFE_RELEASE(pLightRenderIA);
        SAFE_RELEASE(pRenderShadowMapVS);
        SAFE_RELEASE(pFullScreenTriangleVS);
        SAFE_RELEASE(pVisShadowMapPS);
    }

    HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
    {
        ID3D10Blob *pErrorMessages;
        HRESULT hr = D3DX11CompileFromFile(szFileName, NULL, NULL, szEntryPoint, szShaderModel, 0, 0, NULL, ppBlobOut, &pErrorMessages, NULL);
        SAFE_RELEASE(pErrorMessages);
        return hr;
    }
};

//--------------------------------------------------------------------------------------
// Depth texture array with fp32 precision (D32_FLOAT)
//--------------------------------------------------------------------------------------
class DepthRTArray
{
public:
    ID3D11Texture2D *pTexture;
    ID3D11ShaderResourceView *pSRV;
    ID3D11DepthStencilView *pDSV;
    ID3D11DepthStencilView *ppDSVs[SHADOW_MAP_ARRAY_SIZE];

   DepthRTArray()
        : pTexture(NULL)
        , pSRV(NULL)
        , pDSV(NULL)
    {
    }

    DepthRTArray(ID3D11Device *pd3dDevice)
    {
        HRESULT hr;

        // D3D11 allows the width and height to be in the range [1, 16384].
        // However, CreateTexture2D may fail if the texture array does not fit in video memory.
        D3D11_TEXTURE2D_DESC texDesc;
        texDesc.Width              = SHADOW_MAP_RES;
        texDesc.Height             = SHADOW_MAP_RES;
        texDesc.MipLevels          = 1;
        texDesc.ArraySize          = SHADOW_MAP_ARRAY_SIZE;
        texDesc.Format             = DXGI_FORMAT_R32_TYPELESS;
        texDesc.SampleDesc.Count   = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage              = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags     = NULL;
        texDesc.MiscFlags          = NULL;
        V( pd3dDevice->CreateTexture2D(&texDesc, NULL, &pTexture) );

        // Create a depth-stencil view for the whole texture array
        // This DSV is needed for clearing all the shadow maps at once.
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.Flags = 0; // new in D3D11
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.MipSlice = 0;
        dsvDesc.Texture2DArray.FirstArraySlice = 0;
        dsvDesc.Texture2DArray.ArraySize = texDesc.ArraySize;
        V( pd3dDevice->CreateDepthStencilView(pTexture, &dsvDesc, &pDSV) );

        // Create a shader resource view for the whole texture array
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MostDetailedMip = 0;
        srvDesc.Texture2DArray.MipLevels = 1;
        srvDesc.Texture2DArray.FirstArraySlice = 0;
        srvDesc.Texture2DArray.ArraySize = texDesc.ArraySize;
        V( pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &pSRV) );

        // Create depth-stencil views for the individual layers
        D3D11_DEPTH_STENCIL_VIEW_DESC layerDsvDesc;
        pDSV->GetDesc(&layerDsvDesc);
        layerDsvDesc.Texture2DArray.MipSlice = 0;
        layerDsvDesc.Texture2DArray.ArraySize = 1;
        for (UINT i = 0; i < SHADOW_MAP_ARRAY_SIZE; ++i)
        {
            layerDsvDesc.Texture2DArray.FirstArraySlice = i;
            V( pd3dDevice->CreateDepthStencilView(pTexture, &layerDsvDesc, &ppDSVs[i]) );
        }
    }

    ~DepthRTArray()
    {
        SAFE_RELEASE(pTexture);
        SAFE_RELEASE(pDSV);
        SAFE_RELEASE(pSRV);

        for (UINT i = 0; i < SHADOW_MAP_ARRAY_SIZE; ++i)
        {
            SAFE_RELEASE(ppDSVs[i]);
        }
    }
};

//--------------------------------------------------------------------------------------
// Shadow-map depth-buffer array and viewport
//--------------------------------------------------------------------------------------
class SceneShadowMap
{
public:
    SceneShadowMap()
        : pDepthRT(NULL)
    {
    }

    void CreateResources(ID3D11Device *pd3dDevice)
    {
        SAFE_DELETE(pDepthRT);
        pDepthRT = new DepthRTArray(pd3dDevice);

        Viewport.Width  = SHADOW_MAP_RES;
        Viewport.Height = SHADOW_MAP_RES;
        Viewport.MinDepth = 0.f;
        Viewport.MaxDepth = 1.f;
        Viewport.TopLeftX = 0.f;
        Viewport.TopLeftY = 0.f;
    }

    void ReleaseResouces()
    {
        SAFE_DELETE(pDepthRT);
    }

    DepthRTArray* pDepthRT;
    D3D11_VIEWPORT Viewport;
};

//--------------------------------------------------------------------------------------
// Timestamp queries used for profiling the GPU
//--------------------------------------------------------------------------------------
class TimestampQueries
{
public:
    TimestampQueries()
    {
        memset(m_TimestampValues, 0, sizeof(m_TimestampValues));
    }

    void CreateResources(ID3D11Device *pd3dDevice)
    {
        HRESULT hr;

        D3D11_QUERY_DESC queryDesc;
        queryDesc.MiscFlags = 0;

        queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        V( pd3dDevice->CreateQuery(&queryDesc, &m_DisjointTimestampQuery) );

        queryDesc.Query = D3D11_QUERY_TIMESTAMP;
        for (UINT i = 0; i < NumTimestamps; ++i)
        {
            V( pd3dDevice->CreateQuery(&queryDesc, &m_TimestampQuery[i]) );
        }
    }

    void ReleaseResouces()
    {
        SAFE_RELEASE(m_DisjointTimestampQuery);
        for (UINT i = 0; i < NumTimestamps; ++i)
        {
            SAFE_RELEASE(m_TimestampQuery[i]);
        }
    }

    void SetTimestamp(ID3D11DeviceContext* pd3dImmediateContext, UINT i)
    {
        assert(i < NumTimestamps);
        pd3dImmediateContext->End(m_TimestampQuery[i]);
    }

    UINT64 GetTimestampValue(ID3D11DeviceContext* pd3dImmediateContext, UINT i)
    {
        assert(i < NumTimestamps);
        while (S_OK != pd3dImmediateContext->GetData(m_TimestampQuery[i], &m_TimestampValues[i], sizeof(UINT64), 0)) {}
        return m_TimestampValues[i];
    }

    void BeginDisjointQuery(ID3D11DeviceContext* pd3dImmediateContext)
    {
        pd3dImmediateContext->Begin(m_DisjointTimestampQuery);
    }

    void EndDisjointQuery(ID3D11DeviceContext* pd3dImmediateContext)
    {
        pd3dImmediateContext->End(m_DisjointTimestampQuery);
        while (S_OK != pd3dImmediateContext->GetData(m_DisjointTimestampQuery, &m_DisjointTimestampData, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0)) {}
    }

    double GetTimeInMs(UINT64 TimestampValueEnd, UINT64 TimestampValueBegin)
    {
        if (m_DisjointTimestampData.Disjoint) return 0.0;
        return ((double)(TimestampValueEnd - TimestampValueBegin) / (double)m_DisjointTimestampData.Frequency) * 1.e3;
    }

private:
    static const UINT NumTimestamps = 3;
    ID3D11Query *m_DisjointTimestampQuery;
    ID3D11Query *m_TimestampQuery[NumTimestamps];
    UINT64 m_TimestampValues[NumTimestamps];
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT m_DisjointTimestampData;
};

//--------------------------------------------------------------------------------
class SimpleRT
{
public:
    ID3D11Texture2D* pTexture;
    ID3D11RenderTargetView* pRTV;
    ID3D11ShaderResourceView* pSRV;

    SimpleRT(ID3D11Device* pd3dDevice, UINT Width, UINT Height, DXGI_FORMAT Format) :
        pTexture(NULL), pRTV(NULL), pSRV(NULL)
    {
        D3D11_TEXTURE2D_DESC desc;
        desc.Width              = Width;
        desc.Height             = Height;
        desc.Format             = Format;
        desc.MipLevels          = 1;
        desc.ArraySize          = 1;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.CPUAccessFlags     = 0;
        desc.MiscFlags          = 0;

        pd3dDevice->CreateTexture2D(&desc, NULL, &pTexture);
        pd3dDevice->CreateShaderResourceView(pTexture, NULL, &pSRV);
        pd3dDevice->CreateRenderTargetView(pTexture, NULL, &pRTV);
    }

    ~SimpleRT()
    {
        SAFE_RELEASE(pTexture);
        SAFE_RELEASE(pRTV);
        SAFE_RELEASE(pSRV);
    }
};

//--------------------------------------------------------------------------------------
struct SceneParameters
{
    float LightRadiusWorld; // radius of the disk area light, in world units
    UINT NumCharacters; // number of meshes to render
};

//--------------------------------------------------------------------------------------
// Multi-View Soft Shadow renderer
//--------------------------------------------------------------------------------------
class Scene
{
public:
    Scene() : m_DepthMRT(NULL), m_ColorMRT(NULL) {}

    void CreateResources(ID3D11Device *pd3dDevice)
    {
        m_ShadowMap.CreateResources(pd3dDevice);
        m_Textures.CreateResources(pd3dDevice);
        m_Geometry.CreateResources(pd3dDevice);
        m_Shaders.CreateResources(pd3dDevice);
        m_States.CreateResources(pd3dDevice);
        m_GlobalCB.CreateResources(pd3dDevice);
        m_ShadowMapCB.CreateResources(pd3dDevice);
        m_TimestampQueries.CreateResources(pd3dDevice);
    }

    void ReleaseResources()
    {
        m_Geometry.ReleaseResources();
        m_Textures.ReleaseResources();
        m_Shaders.ReleaseResources();
        m_States.ReleaseResources();
        m_GlobalCB.ReleaseResources();
        m_ShadowMapCB.ReleaseResources();
        m_ShadowMap.ReleaseResouces();
        m_TimestampQueries.ReleaseResouces();
        SAFE_DELETE(m_DepthMRT);
        SAFE_DELETE(m_ColorMRT);
    }

    // Generate the shadow maps and render the scene
    void Render(ID3D11DeviceContext* pd3dImmediateContext,
                ID3D11RenderTargetView* pOutputRTV,
                ID3D11DepthStencilView* pOutputDSV,
                const SceneParameters &args,
                double &ShadowMapRenderTimeMs,
                double &EyeRenderTimeMs)
    {
        m_LightRadiusWorld = args.LightRadiusWorld;
        m_Geometry.SetNumCharacters(args.NumCharacters);

        // Timestamp queries need to be wraped inside a disjoint query begin/end
        m_TimestampQueries.BeginDisjointQuery(pd3dImmediateContext);
        m_TimestampQueries.SetTimestamp(pd3dImmediateContext, 0);

        // These perf events can be visualized with NSight for instance
        DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Shadow Maps");

        // This SDK sample does not use any hull, domain or geometry shaders
        pd3dImmediateContext->HSSetShader(NULL, NULL, 0);
        pd3dImmediateContext->DSSetShader(NULL, NULL, 0);
        pd3dImmediateContext->GSSetShader(NULL, NULL, 0);

        //--------------------------------------------------------------------------------------
        // STEP 1: Render shadow maps
        //--------------------------------------------------------------------------------------

        pd3dImmediateContext->ClearDepthStencilView(m_ShadowMap.pDepthRT->pDSV, D3D11_CLEAR_DEPTH, 1.0, 0);

        // Poisson disk generated with http://www.coderhaus.com/?p=11 and min distance = 0.3
        assert(SHADOW_MAP_ARRAY_SIZE == 28);
        static D3DXVECTOR2 PoissonDisk28[SHADOW_MAP_ARRAY_SIZE] =
        {
            D3DXVECTOR2(-0.6905488f, 0.09492259f),
            D3DXVECTOR2(-0.7239041f, -0.3711901f),
            D3DXVECTOR2(-0.1990684f, -0.1351167f),
            D3DXVECTOR2(-0.8588699f, 0.4396836f),
            D3DXVECTOR2(-0.4826424f, 0.320396f),
            D3DXVECTOR2(-0.9968387f, 0.01040132f),
            D3DXVECTOR2(-0.5230064f, -0.596889f),
            D3DXVECTOR2(-0.2146133f, -0.6254999f),
            D3DXVECTOR2(-0.6389362f, 0.7377159f),
            D3DXVECTOR2(-0.1776157f, 0.6040277f),
            D3DXVECTOR2(-0.01479932f, 0.2212604f),
            D3DXVECTOR2(-0.3635045f, -0.8955025f),
            D3DXVECTOR2(0.3450507f, -0.7505886f),
            D3DXVECTOR2(0.1438699f, -0.1978877f),
            D3DXVECTOR2(0.06733564f, -0.9922826f),
            D3DXVECTOR2(0.1302602f, 0.758476f),
            D3DXVECTOR2(-0.3056195f, 0.9038011f),
            D3DXVECTOR2(0.387158f, 0.5397643f),
            D3DXVECTOR2(0.1010145f, -0.5530168f),
            D3DXVECTOR2(0.6531418f, 0.08325134f),
            D3DXVECTOR2(0.3876107f, -0.4529504f),
            D3DXVECTOR2(0.7198777f, -0.3464415f),
            D3DXVECTOR2(0.9582281f, -0.1639438f),
            D3DXVECTOR2(0.6608706f, -0.7009276f),
            D3DXVECTOR2(0.2853746f, 0.1097673f),
            D3DXVECTOR2(0.715556f, 0.3905755f),
            D3DXVECTOR2(0.6451758f, 0.7568412f),
            D3DXVECTOR2(0.4597791f, -0.1513058f)
        };

        for (int LightIndex = 0; LightIndex < SHADOW_MAP_ARRAY_SIZE; ++LightIndex)
        {
            float ViewSpaceJitterX = PoissonDisk28[LightIndex].x * args.LightRadiusWorld;
            float ViewSpaceJitterY = PoissonDisk28[LightIndex].y * args.LightRadiusWorld;
            UpdateShadowMapFrustum(ViewSpaceJitterX, ViewSpaceJitterY, LightIndex);
            pd3dImmediateContext->UpdateSubresource(m_ShadowMapCB.pBuffer, 0, NULL, &m_ShadowMapCB.CBData, 0, 0);

            pd3dImmediateContext->OMSetRenderTargets(0, NULL, m_ShadowMap.pDepthRT->ppDSVs[LightIndex]);
            RenderShadowMap(pd3dImmediateContext);
        }

        DXUT_EndPerfEvent();

        m_TimestampQueries.SetTimestamp(pd3dImmediateContext, 1);

        DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"Shading");

        //--------------------------------------------------------------------------------------
        // STEP 2: Render scene from the eye's point of view
        //--------------------------------------------------------------------------------------

        pd3dImmediateContext->UpdateSubresource(m_GlobalCB.pBuffer, 0, NULL, &m_GlobalCB.CBData, 0, 0);

        float BackgroundColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        pd3dImmediateContext->ClearDepthStencilView(pOutputDSV, D3D11_CLEAR_DEPTH, 1.0, 0);
        pd3dImmediateContext->ClearRenderTargetView(pOutputRTV, BackgroundColor);

        float BlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        pd3dImmediateContext->OMSetBlendState(m_States.pNoBlend_BS, BlendFactor, 0xffffffff);

        pd3dImmediateContext->RSSetViewports(1, &m_Viewport);
        pd3dImmediateContext->RSSetState(m_States.pBackfaceCull_RS);

        pd3dImmediateContext->IASetInputLayout(m_Shaders.pEyeRenderIA);
        pd3dImmediateContext->VSSetConstantBuffers(0, 1, &m_GlobalCB.pBuffer);

        //--------------------------------------------------------------------------------------
        // Shading pass
        //--------------------------------------------------------------------------------------

        pd3dImmediateContext->OMSetRenderTargets(1, &pOutputRTV, pOutputDSV);
        pd3dImmediateContext->OMSetDepthStencilState(m_States.pDepthNoStencil_DS, 0);

        pd3dImmediateContext->VSSetShader(m_Shaders.pEyeRenderVS, NULL, 0);

        // Bind the pixel-shader registers t0, t1 and t2
        ID3D11ShaderResourceView* pSRVs[3] = { m_Textures.GetGroundDiffuseSRV(), m_Textures.GetGroundNormalSRV(), m_ShadowMap.pDepthRT->pSRV };
        pd3dImmediateContext->PSSetShaderResources(0, 3, pSRVs);

        // Bind the pixel-shader registers s0 and s1
        ID3D11SamplerState* pSamplerStates[2] = { m_States.pHwPcfSampler, m_States.pTrilinearWarpSampler };
        pd3dImmediateContext->PSSetSamplers(0, 2, pSamplerStates);

        // Bind the constant buffer to pixel-shader register b0
        pd3dImmediateContext->PSSetConstantBuffers(0, 1, &m_GlobalCB.pBuffer);

        pd3dImmediateContext->PSSetShader(m_Shaders.pEyeRenderCharacterPS, NULL, 0);
        m_Geometry.DrawCharacters(pd3dImmediateContext);

        pd3dImmediateContext->PSSetShader(m_Shaders.pEyeRenderGroundPS, NULL, 0);
        m_Geometry.DrawPlane(pd3dImmediateContext);

        // Unbind shader resource views to avoid any D3D runtime warnings
        ID3D11ShaderResourceView* pNullSRVs[3] = { NULL };
        pd3dImmediateContext->PSSetShaderResources(0, 3, pNullSRVs);

        DXUT_EndPerfEvent();

        //--------------------------------------------------------------------------------------
        // Query GPU timestamps
        //--------------------------------------------------------------------------------------

        m_TimestampQueries.SetTimestamp(pd3dImmediateContext, 2);
        m_TimestampQueries.EndDisjointQuery(pd3dImmediateContext);

        // Performing the GetData calls at the end, to minimize the performance impact of the timestamp queries.
        UINT64 TimestampBegin = m_TimestampQueries.GetTimestampValue(pd3dImmediateContext, 0);
        UINT64 TimestampShadowMap = m_TimestampQueries.GetTimestampValue(pd3dImmediateContext, 1);
        UINT64 TimestampEnd = m_TimestampQueries.GetTimestampValue(pd3dImmediateContext, 2);

        ShadowMapRenderTimeMs = m_TimestampQueries.GetTimeInMs(TimestampShadowMap, TimestampBegin);
        EyeRenderTimeMs = m_TimestampQueries.GetTimeInMs(TimestampEnd, TimestampShadowMap);
    }

    void SetScreenResolution(ID3D11Device *pd3dDevice, float FovyRad, UINT Width, UINT Height)
    {
        m_Viewport.Width  = (FLOAT)Width;
        m_Viewport.Height = (FLOAT)Height;
        m_Viewport.MinDepth = 0;
        m_Viewport.MaxDepth = 1;
        m_Viewport.TopLeftX = 0;
        m_Viewport.TopLeftY = 0;

        SAFE_DELETE(m_DepthMRT);
        m_DepthMRT = new SimpleRT(pd3dDevice, Width, Height, DXGI_FORMAT_R32_FLOAT);

        SAFE_DELETE(m_ColorMRT);
        m_ColorMRT = new SimpleRT(pd3dDevice, Width, Height, DXGI_FORMAT_R8G8B8A8_UNORM);
    }

    void SetLightCamera(CModelViewerCamera &LCamera)
    {
        D3DXMATRIX Proj = *LCamera.GetProjMatrix();
        D3DXMATRIX View = *LCamera.GetViewMatrix();
        D3DXMATRIX World = *LCamera.GetWorldMatrix();

        D3DXMATRIX WorldView, WorldViewI;
        D3DXMatrixMultiply(&WorldView, &World, &View);
        D3DXMatrixInverse(&WorldViewI, NULL, &WorldView);

        D3DXVECTOR3 LightCenterWorld;
        D3DXVECTOR3 Zero = D3DXVECTOR3(0, 0, 0);
        D3DXVec3TransformCoord(&LightCenterWorld, &Zero, &WorldViewI);

        // If the light source is high enough above the ground plane
        if (LightCenterWorld.y > 1.0f)
        {
            UpdateLightPosition(Proj, View, World);
        }
        else
        {
            LCamera.SetWorldMatrix(m_LightWorld);
        }
    }

    void SetMainCamera(CModelViewerCamera &Camera)
    {
        D3DXMATRIX Proj = *Camera.GetProjMatrix();
        D3DXMATRIX View = *Camera.GetViewMatrix();
        D3DXMATRIX World = *Camera.GetWorldMatrix();

        D3DXMatrixMultiply(&m_GlobalCB.CBData.WorldToEyeView, &World, &View);
        D3DXMatrixMultiply(&m_GlobalCB.CBData.WorldToEyeClip, &m_GlobalCB.CBData.WorldToEyeView, &Proj);
        D3DXMatrixInverse(&m_WorldToEyeViewI, NULL, &m_GlobalCB.CBData.WorldToEyeView);

        D3DXMATRIX ProjInv;
        D3DXMatrixInverse(&ProjInv, NULL, &Proj);

        m_GlobalCB.CBData.EyeViewToEyeClip = Proj;
        m_GlobalCB.CBData.EyeClipToEyeView = ProjInv;
    }

    UINT GetNumShadowCastingTriangles()
    {
        return m_Geometry.GetNumMeshTriangles();
    }

    // For debugging, visualize the last-rendered shadow map
    void VisualizeShadowMap(ID3D11DeviceContext* pd3dImmediateContext,
                            ID3D11RenderTargetView* pOutputRTV)
    {
        pd3dImmediateContext->OMSetRenderTargets(1, &pOutputRTV, NULL );
        float BlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        pd3dImmediateContext->OMSetBlendState(m_States.pNoBlend_BS, BlendFactor, 0xffffffff);
        pd3dImmediateContext->OMSetDepthStencilState(m_States.pNoDepthNoStencil_DS, 0);

        pd3dImmediateContext->RSSetViewports(1, &m_Viewport);
        pd3dImmediateContext->RSSetState(m_States.pNoCull_RS);

        pd3dImmediateContext->VSSetShader(m_Shaders.pFullScreenTriangleVS, NULL, 0);
        pd3dImmediateContext->PSSetShader(m_Shaders.pVisShadowMapPS, NULL, 0);
        pd3dImmediateContext->PSSetSamplers(2, 1, &m_States.pPointClampSampler);
        pd3dImmediateContext->PSSetShaderResources(2, 1, &m_ShadowMap.pDepthRT->pSRV);
        pd3dImmediateContext->Draw(3, 0);
    }

private:
    // Generate a shadow map by drawing geometry into the current depth-stencil view
    void RenderShadowMap(ID3D11DeviceContext* pd3dImmediateContext)
    {
        pd3dImmediateContext->OMSetDepthStencilState(m_States.pDepthNoStencil_DS, 0);
        float BlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        pd3dImmediateContext->OMSetBlendState(m_States.pNoBlend_BS, BlendFactor, 0xffffffff );

        pd3dImmediateContext->RSSetViewports(1, &m_ShadowMap.Viewport);
        pd3dImmediateContext->RSSetState(m_States.pDepthBiasBackfaceCull_RS);

        pd3dImmediateContext->IASetInputLayout(m_Shaders.pLightRenderIA);
        pd3dImmediateContext->VSSetConstantBuffers(0, 1, &m_ShadowMapCB.pBuffer);
        pd3dImmediateContext->VSSetShader(m_Shaders.pRenderShadowMapVS, NULL, 0);

        // Unbind shader resource views to avoid any D3D runtime warnings
        ID3D11ShaderResourceView* pNullSRVs[3] = { NULL };
        pd3dImmediateContext->PSSetShaderResources(0, 3, pNullSRVs);

        // Using a NULL pixel shader (no pixel shader) to maximize performance
        pd3dImmediateContext->PSSetShader(NULL, NULL, 0);

        m_Geometry.DrawCharacters(pd3dImmediateContext);
    }

    // Transform the vertices of BBox1 and extend BBox2 accordingly
    void TransformBoundingBox(D3DXVECTOR3 BBox2[2], const D3DXVECTOR3 BBox1[2], const D3DXMATRIX &matrix)
    {
        BBox2[0][0] = BBox2[0][1] = BBox2[0][2] =  std::numeric_limits<float>::max();
        BBox2[1][0] = BBox2[1][1] = BBox2[1][2] = -std::numeric_limits<float>::max();
        for (int i = 0; i < 8; ++i)
        {
            D3DXVECTOR3 v, v1;
            v[0] = BBox1[(i & 1) ? 0 : 1][0];
            v[1] = BBox1[(i & 2) ? 0 : 1][1];
            v[2] = BBox1[(i & 4) ? 0 : 1][2];
            D3DXVec3TransformCoord(&v1, &v, &matrix);
            for (int j = 0; j < 3; ++j)
            {
                BBox2[0][j] = std::min(BBox2[0][j], v1[j]);
                BBox2[1][j] = std::max(BBox2[1][j], v1[j]);
            }
        }
    }

    // Compute the matrices for rendering from the current light's point of view
    void UpdateShadowMapFrustum(float ViewOffsetX, float ViewOffsetY, UINT LightIndex)
    {
        D3DXMATRIX LightWorld = m_LightWorld;
        D3DXMATRIX LightView = m_LightView;

        // Build a lookat matrix with center of projection = m_LightCenterView + jitter
        D3DXVECTOR3 ViewX = D3DXVECTOR3(LightView(0,0),LightView(0,1),LightView(0,2));
        D3DXVECTOR3 ViewY = D3DXVECTOR3(LightView(1,0),LightView(1,1),LightView(1,2));
        D3DXVECTOR3 Pos = m_LightCenterView + ViewX * ViewOffsetX + ViewY * ViewOffsetY;
        D3DXVECTOR3 Lookat = D3DXVECTOR3(0,0,0);
        D3DXVECTOR3 Up = D3DXVECTOR3(0,1,0);
        D3DXMatrixLookAtLH(&LightView, &Pos, &Lookat, &Up);

        D3DXMATRIX WorldToLightView;
        D3DXMatrixMultiply(&WorldToLightView, &LightWorld, &LightView);

        D3DXVECTOR3 WorldSpaceBBox[2];
        m_Geometry.ComputeWorldSpaceBoundingBox(WorldSpaceBBox);

        D3DXVECTOR3 LightViewSpaceBBox[2];
        TransformBoundingBox(LightViewSpaceBBox, WorldSpaceBBox, WorldToLightView);

        // Build a perspective projection matrix for the current point light sample
        D3DXMATRIX LightProj;
        float FrustumLeft   = LightViewSpaceBBox[0][0];
        float FrustumRight  = LightViewSpaceBBox[1][0];
        float FrustumBottom = LightViewSpaceBBox[0][1];
        float FrustumTop    = LightViewSpaceBBox[1][1];
        float FrustumZNear  = LightViewSpaceBBox[0][2];
        float FrustumZFar   = LIGHT_ZFAR;
        D3DXMatrixPerspectiveOffCenterLH(&LightProj, FrustumLeft, FrustumRight, FrustumBottom, FrustumTop, FrustumZNear, FrustumZFar);

        D3DXMATRIX EyeViewToLightView;
        D3DXMatrixMultiply(&EyeViewToLightView, &m_WorldToEyeViewI, &WorldToLightView);

        D3DXMATRIX EyeViewToLightClip;
        D3DXMatrixMultiply(&EyeViewToLightClip, &EyeViewToLightView, &LightProj);

        // Scale matrix to go from post-perspective space into texture space ([0,1]^2)
        D3DXMATRIX Clip2Tex = D3DXMATRIX(
            0.5,    0,    0,   0,
            0,	   -0.5,  0,   0,
            0,     0,     1,   0,
            0.5,   0.5,   0,   1 );
        D3DXMATRIX EyeViewToLightTex;
        D3DXMatrixMultiply(&EyeViewToLightTex, &EyeViewToLightClip, &Clip2Tex);

        // For the shadow-map generation vertex shader
        D3DXMATRIX WorldToLightClip;
        D3DXMatrixMultiply(&WorldToLightClip, &WorldToLightView, &LightProj);

        // Update constant buffer
        memcpy(&m_ShadowMapCB.CBData.WorldToLightClip, &WorldToLightClip, sizeof(D3DXMATRIX));
        memcpy(&m_GlobalCB.CBData.EyeViewToLightTex[LightIndex], &EyeViewToLightTex, sizeof(D3DXMATRIX));
        m_GlobalCB.CBData.LightZNear = FrustumZNear;
        m_GlobalCB.CBData.LightZFar = FrustumZFar;
    }

    // Compute the light position used for shading (center of the area light)
    void UpdateLightPosition(D3DXMATRIX &Proj, D3DXMATRIX &View, D3DXMATRIX &World)
    {
        D3DXMATRIX WorldI, ViewI;
        D3DXMATRIX WorldView, WorldViewI;
        D3DXMatrixInverse(&WorldI, NULL, &World);
        D3DXMatrixInverse(&ViewI, NULL, &View);
        D3DXMatrixMultiply(&WorldView, &World, &View);
        D3DXMatrixInverse(&WorldViewI, NULL, &WorldView);

        D3DXVECTOR3 Zero = D3DXVECTOR3(0, 0, 0);
        D3DXVec3TransformCoord(&m_LightCenterView, &Zero, &ViewI);

        D3DXVECTOR3 LightCenterWorld;
        D3DXVec3TransformCoord(&LightCenterWorld, &Zero, &WorldViewI);
        memcpy(&m_GlobalCB.CBData.LightWorldPos, &LightCenterWorld, 3*sizeof(float));

        m_LightWorld = World;
        m_LightView = View;
        m_LightProj = Proj;
    }

    float                   m_LightRadiusWorld;
    D3DXMATRIX              m_LightWorld;
    D3DXMATRIX              m_LightView;
    D3DXMATRIX              m_LightProj;
    D3DXMATRIX              m_WorldToEyeViewI;
    D3DXVECTOR3             m_LightCenterView;

    D3D11_VIEWPORT          m_Viewport;
    SceneTextures           m_Textures;
    SceneGeometry           m_Geometry;
    SceneStates             m_States;
    SceneConstantBuffer     m_GlobalCB;
    ShadowMapConstantBuffer m_ShadowMapCB;
    SceneShaders            m_Shaders;
    SceneShadowMap          m_ShadowMap;
    TimestampQueries        m_TimestampQueries;

    SimpleRT *m_DepthMRT;
    SimpleRT *m_ColorMRT;
};
