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
#include "NVSSAO/NVSDK_D3D11_SSAO.h"
#include <algorithm>
#include <math.h>
#include <assert.h>

#define SAFE_CALL(x)         { if (x != S_OK) assert(0); }

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)       { if (p) { delete (p);     (p)=NULL; } }
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef clamp
#define clamp(x,a,b) (min(max(x, a), b))
#endif

#ifndef D3DX_PI
#define D3DX_PI 3.1415926535897932384626433832795f
#endif

#ifndef INV_LN2
#define INV_LN2 1.44269504f
#endif

#ifndef SQRT_LN2
#define SQRT_LN2 0.832554611f
#endif

#define MAX_BLUR_RADIUS 16
#define NUM_BLUR_PERMUTATIONS (MAX_BLUR_RADIUS/2 + 1)

#define MAX_HBAO_STEP_SIZE 8
#define NUM_HBAO_PERMUTATIONS (MAX_HBAO_STEP_SIZE)

// Must match the values from Shaders/Source/HBAO_PS.hlsl
#define RANDOM_TEXTURE_WIDTH 4
#define NUM_DIRECTIONS 8

namespace NVSDK11
{

//--------------------------------------------------------------------------------
class HBAORenderOptions
{
public:
    void SetRenderOptions(const NVSDK_D3D11_AOParameters *pParams)
    {
        AOResolutionMode = pParams->resolutionMode;
        AOShaderType = pParams->shaderType;
        BlendAO = pParams->useBlending;
    }
    NVSDK_D3D11_AOResolutionMode AOResolutionMode;
    NVSDK_D3D11_AOShaderType AOShaderType;
    BOOL BlendAO;
};

//--------------------------------------------------------------------------------
class HBAOConstantBuffer
{
public:
    UINT GetBlurShaderId()
    {
        return m_BlurShaderId;
    }

    UINT GetHBAOShaderId()
    {
        return m_HBAOShaderId;
    }

    void SetFovy(float FovyRad)
    {
        m_FovyRad = FovyRad;
    }

    UINT GetAOWidth()
    {
        return m_AOWidth;
    }

    UINT GetAOHeight()
    {
        return m_AOHeight;
    }

    void SetAOParameters(const NVSDK_D3D11_AOParameters *params)
    {
        m_RadiusScale = max(params->radius, 1.e-8f);
        m_BlurSharpness = params->blurSharpness;
        m_BlurRadius = params->blurRadius;
        m_HBAOShaderId = clamp(params->stepSize - 1, 0, NUM_HBAO_PERMUTATIONS-1);
        m_BlurShaderId = min(m_BlurRadius >> 1, NUM_BLUR_PERMUTATIONS-1);

        data.PowExponent = params->powerExponent;
        data.Strength = params->strength;

        static const float DegToRad = D3DX_PI / 180.f;
        data.AngleBias = params->angleBias * DegToRad;
        data.TanAngleBias = tanf(data.AngleBias);

        float BlurSigma = ((float)m_BlurRadius+1.0f) * 0.5f;
        data.BlurFalloff = INV_LN2 / (2.0f*BlurSigma*BlurSigma);
    }

    void SetCameraParameters(float ZNear, float ZFar, float ZMin, float ZMax, float SceneScale)
    {
        m_ZNear = ZNear;
        m_ZFar = ZFar;
        m_SceneScale = (SceneScale != 0.0f) ? SceneScale : min(ZNear, ZFar);

        // The hardware depths are linearized in two steps:
        // 1. Inverse viewport from [zmin,zmax] to [0,1]: z' = (z - zmin) / (zmax - zmin)
        // 2. Inverse projection from [0,1] to [znear,zfar]: w = 1 / (a z' + b)

        // w = 1 / [(1/zfar - 1/znear) * z' + 1/znear]
        data.LinA = 1.0f/m_ZFar - 1.0f/m_ZNear;
        data.LinB = 1.0f/m_ZNear;

        // w = 1 / [(1/zfar - 1/znear) * (z - zmin)/(zmax - zmin) + 1/znear]
        float ZRange = ZMax - ZMin;
        data.LinA /= ZRange;
        data.LinB -= ZMin * data.LinA;
    }

    void SetFullResolution(UINT Width, UINT Height)
    {
        m_FullWidth  = Width;
        m_FullHeight = Height;
        data.FullResolution[0]    = (float)Width;
        data.FullResolution[1]    = (float)Height;
        data.InvFullResolution[0] = 1.0f / Width;
        data.InvFullResolution[1] = 1.0f / Height;
        data.MaxRadiusPixels = 0.1f * min(data.FullResolution[0], data.FullResolution[1]);
    }

    void UpdateSceneScaleConstants()
    {
        data.R = m_RadiusScale * m_SceneScale;
        data.R2 = data.R * data.R;
        data.NegInvR2 = -1.0f / data.R2;
        data.BlurDepthThreshold = 2.0f * SQRT_LN2 * (m_SceneScale / m_BlurSharpness);
    }

    void UpdateAOResolutionConstants(NVSDK_D3D11_AOResolutionMode AOResolutionMode)
    {
        UINT AODownsampleFactor = (AOResolutionMode == NVSDK_HALF_RES_AO) ? 2 : 1;
        m_AOWidth  = m_FullWidth / AODownsampleFactor;
        m_AOHeight = m_FullHeight / AODownsampleFactor;

        float AOWidth           = (float)m_AOWidth;
        float AOHeight          = (float)m_AOHeight;
        data.AOResolution[0]    = AOWidth;
        data.AOResolution[1]    = AOHeight;
        data.InvAOResolution[0] = 1.0f / AOWidth;
        data.InvAOResolution[1] = 1.0f / AOHeight;

        data.FocalLen[0]      = 1.0f / tanf(m_FovyRad * 0.5f) * (AOHeight / AOWidth);
        data.FocalLen[1]      = 1.0f / tanf(m_FovyRad * 0.5f);
        data.InvFocalLen[0]   = 1.0f / data.FocalLen[0];
        data.InvFocalLen[1]   = 1.0f / data.FocalLen[1];

        data.UVToViewA[0] =  2.0f * data.InvFocalLen[0];
        data.UVToViewA[1] = -2.0f * data.InvFocalLen[1];
        data.UVToViewB[0] = -1.0f * data.InvFocalLen[0];
        data.UVToViewB[1] =  1.0f * data.InvFocalLen[1];
    }

    void Create(ID3D11Device *pD3DDevice)
    {
        static D3D11_BUFFER_DESC desc = 
        {
             sizeof(data), //ByteWidth
             D3D11_USAGE_DEFAULT, //Usage
             D3D11_BIND_CONSTANT_BUFFER, //BindFlags
             0, //CPUAccessFlags
             0  //MiscFlags
        };

        SAFE_CALL( pD3DDevice->CreateBuffer(&desc, NULL, &m_pParamsCB) );
    }

    void Release()
    {
        SAFE_RELEASE(m_pParamsCB);
    }

    void UpdateSubresource(ID3D11DeviceContext* pDeviceContext)
    {
        pDeviceContext->UpdateSubresource(m_pParamsCB, 0, NULL, &data, 0, 0);
    }
    
    ID3D11Buffer **GetCB() { return &m_pParamsCB; }

protected:
    // Must match the layout from Shaders/Source/ConstantBuffer.hlsl
    struct
    {
      float FullResolution[2];
      float InvFullResolution[2];

      float AOResolution[2];
      float InvAOResolution[2];

      float FocalLen[2];
      float InvFocalLen[2];

      float UVToViewA[2];
      float UVToViewB[2];

      float R;
      float R2;
      float NegInvR2;
      float MaxRadiusPixels;

      float AngleBias;
      float TanAngleBias;
      float PowExponent;
      float Strength;

      float BlurDepthThreshold;
      float BlurFalloff;
      float LinA;
      float LinB;
    } data;

    UINT m_FullWidth;
    UINT m_FullHeight;
    UINT m_AOWidth;
    UINT m_AOHeight;
    float m_RadiusScale;
    float m_BlurSharpness;
    UINT m_BlurRadius;
    float m_FovyRad;
    float m_ZNear;
    float m_ZFar;
    float m_SceneScale;
    UINT m_HBAOShaderId;
    UINT m_BlurShaderId;
    ID3D11Buffer *m_pParamsCB;
};

//--------------------------------------------------------------------------------
class HBAOInputDepthInfo
{
public:
    HBAOInputDepthInfo() :
      pLinearDepthSRV(NULL),
      pNonLinearDepthSRV(NULL)
    {
    }
    BOOL IsAvailable()
    {
        return (pLinearDepthSRV != NULL ||
                pNonLinearDepthSRV != NULL);
    }
    void SetResolution(UINT w, UINT h, UINT s)
    {
        Width = w;
        Height = h;
        SampleCount = s;
    }
    ID3D11ShaderResourceView *pLinearDepthSRV;
    ID3D11ShaderResourceView *pNonLinearDepthSRV;
    UINT Width;
    UINT Height;
    UINT SampleCount;
};

//--------------------------------------------------------------------------------
class AppState
{
public:
    void Save(ID3D11DeviceContext* pDeviceContext)
    {
        NumViewports = 1;
        pDeviceContext->RSGetViewports(&NumViewports, &Viewport);

        pDeviceContext->GetPredication(&pPredicate, &PredicateValue);
        pDeviceContext->IAGetPrimitiveTopology(&Topology);
        pDeviceContext->IAGetInputLayout(&pInputLayout);

        NumVSClassInstances = 
        NumHSClassInstances =
        NumDSClassInstances =
        NumGSClassInstances =
        NumPSClassInstances =
        NumCSClassInstances = 0;
        pDeviceContext->VSGetShader(&pVS, ppVSClassInstances, &NumVSClassInstances);
        pDeviceContext->HSGetShader(&pHS, ppHSClassInstances, &NumHSClassInstances);
        pDeviceContext->DSGetShader(&pDS, ppDSClassInstances, &NumDSClassInstances);
        pDeviceContext->GSGetShader(&pGS, ppGSClassInstances, &NumGSClassInstances);
        pDeviceContext->PSGetShader(&pPS, ppPSClassInstances, &NumPSClassInstances);
        pDeviceContext->CSGetShader(&pCS, ppCSClassInstances, &NumCSClassInstances);

        pDeviceContext->PSGetShaderResources(0, NumSRVs, ppPSShaderResourceViews);
        pDeviceContext->CSGetShaderResources(0, NumSRVs, ppCSShaderResourceViews);
        pDeviceContext->PSGetSamplers(0, NumSamplers, ppPSSamplers);
        pDeviceContext->CSGetSamplers(0, NumSamplers, ppCSSamplers);
        pDeviceContext->VSGetConstantBuffers(0, 1, &pVSConstantBuffer);
        pDeviceContext->PSGetConstantBuffers(0, 1, &pPSConstantBuffer);
        pDeviceContext->CSGetConstantBuffers(0, 1, &pCSConstantBuffer);
        pDeviceContext->RSGetState(&pRasterizerState);
        pDeviceContext->OMGetDepthStencilState(&pDepthStencilState, &StencilRef);
        pDeviceContext->OMGetBlendState(&pBlendState, BlendFactor, &SampleMask);
        pDeviceContext->OMGetRenderTargets(NumRTVs, pRenderTargetViews, &pDepthStencilView);
        pDeviceContext->CSGetUnorderedAccessViews(0, 1, &pUAV);
    }

    void Restore(ID3D11DeviceContext* pDeviceContext)
    {
        pDeviceContext->RSSetViewports(1, &Viewport );
        pDeviceContext->SetPredication(pPredicate, PredicateValue);
        pDeviceContext->IASetPrimitiveTopology(Topology);
        pDeviceContext->IASetInputLayout(pInputLayout);
        pDeviceContext->VSSetShader(pVS, ppVSClassInstances, NumVSClassInstances);
        pDeviceContext->HSSetShader(pHS, ppHSClassInstances, NumHSClassInstances);
        pDeviceContext->DSSetShader(pDS, ppDSClassInstances, NumDSClassInstances);
        pDeviceContext->GSSetShader(pGS, ppGSClassInstances, NumGSClassInstances);
        pDeviceContext->PSSetShader(pPS, ppPSClassInstances, NumPSClassInstances);
        pDeviceContext->CSSetShader(pCS, ppCSClassInstances, NumCSClassInstances);
        pDeviceContext->PSSetShaderResources(0, NumSRVs, ppPSShaderResourceViews);
        pDeviceContext->CSSetShaderResources(0, NumSRVs, ppCSShaderResourceViews);
        pDeviceContext->PSSetSamplers(0, NumSamplers, ppPSSamplers);
        pDeviceContext->CSSetSamplers(0, NumSamplers, ppCSSamplers);
        pDeviceContext->VSSetConstantBuffers(0, 1, &pVSConstantBuffer);
        pDeviceContext->PSSetConstantBuffers(0, 1, &pPSConstantBuffer);
        pDeviceContext->CSSetConstantBuffers(0, 1, &pCSConstantBuffer);
        pDeviceContext->RSSetState(pRasterizerState);
        pDeviceContext->OMSetDepthStencilState(pDepthStencilState, StencilRef);
        pDeviceContext->OMSetBlendState(pBlendState, BlendFactor, SampleMask);
        pDeviceContext->OMSetRenderTargets(NumRTVs, pRenderTargetViews, pDepthStencilView);
        UINT initCounts = 0;
        pDeviceContext->CSSetUnorderedAccessViews(0, 1, &pUAV, &initCounts);

        SAFE_RELEASE(pInputLayout);
        SAFE_RELEASE(pVSConstantBuffer);
        SAFE_RELEASE(pPSConstantBuffer);
        SAFE_RELEASE(pCSConstantBuffer);
        SAFE_RELEASE(pRasterizerState);
        SAFE_RELEASE(pDepthStencilState);
        SAFE_RELEASE(pBlendState);
        SAFE_RELEASE(pDepthStencilView);
        SAFE_RELEASE(pUAV);
        SAFE_RELEASE(pVS);
        SAFE_RELEASE(pHS);
        SAFE_RELEASE(pDS);
        SAFE_RELEASE(pGS);
        SAFE_RELEASE(pPS);
        SAFE_RELEASE(pCS);
        for (int i = 0; i < NumRTVs; ++i)
        {
            SAFE_RELEASE(pRenderTargetViews[i]);
        }
        for (UINT i = 0; i < NumVSClassInstances; ++i)
        {
            SAFE_RELEASE(ppVSClassInstances[i]);
        }
        for (UINT i = 0; i < NumHSClassInstances; ++i)
        {
            SAFE_RELEASE(ppHSClassInstances[i]);
        }
        for (UINT i = 0; i < NumDSClassInstances; ++i)
        {
            SAFE_RELEASE(ppDSClassInstances[i]);
        }
        for (UINT i = 0; i < NumGSClassInstances; ++i)
        {
            SAFE_RELEASE(ppGSClassInstances[i]);
        }
        for (UINT i = 0; i < NumPSClassInstances; ++i)
        {
            SAFE_RELEASE(ppPSClassInstances[i]);
        }
        for (UINT i = 0; i < NumCSClassInstances; ++i)
        {
            SAFE_RELEASE(ppCSClassInstances[i]);
        }
        for (UINT i = 0; i < NumSRVs; ++i)
        {
            SAFE_RELEASE(ppPSShaderResourceViews[i]);
            SAFE_RELEASE(ppCSShaderResourceViews[i]);
        }
        for (int i = 0; i < NumSamplers; ++i)
        {
            SAFE_RELEASE(ppPSSamplers[i]);
            SAFE_RELEASE(ppCSSamplers[i]);
        }
    }

protected:
    static const int NumRTVs = 3;
    static const int NumSRVs = 4;
    static const int NumSamplers = 4;
    UINT NumViewports;
    D3D11_VIEWPORT Viewport;
    ID3D11Predicate *pPredicate;
    BOOL PredicateValue;
    D3D11_PRIMITIVE_TOPOLOGY Topology;
    ID3D11InputLayout *pInputLayout;
    ID3D11VertexShader *pVS;
    ID3D11ClassInstance *ppVSClassInstances[256];
    UINT NumVSClassInstances;
    ID3D11HullShader *pHS;
    ID3D11ClassInstance *ppHSClassInstances[256];
    UINT NumHSClassInstances;
    ID3D11DomainShader *pDS;
    ID3D11ClassInstance *ppDSClassInstances[256];
    UINT NumDSClassInstances;
    ID3D11GeometryShader *pGS;
    ID3D11ClassInstance *ppGSClassInstances[256];
    UINT NumGSClassInstances;
    ID3D11PixelShader *pPS;
    ID3D11ClassInstance *ppPSClassInstances[256];
    UINT NumPSClassInstances;
    ID3D11ComputeShader *pCS;
    ID3D11ClassInstance *ppCSClassInstances[256];
    UINT NumCSClassInstances;
    ID3D11ShaderResourceView *ppPSShaderResourceViews[NumSRVs];
    ID3D11ShaderResourceView *ppCSShaderResourceViews[NumSRVs];
    ID3D11SamplerState *ppPSSamplers[NumSamplers];
    ID3D11SamplerState *ppCSSamplers[NumSamplers];
    ID3D11Buffer *pVSConstantBuffer;
    ID3D11Buffer *pPSConstantBuffer;
    ID3D11Buffer *pCSConstantBuffer;
    ID3D11RasterizerState *pRasterizerState;
    ID3D11DepthStencilState *pDepthStencilState;
    UINT StencilRef;
    ID3D11BlendState *pBlendState;
    FLOAT BlendFactor[4];
    UINT SampleMask;
    ID3D11RenderTargetView *pRenderTargetViews[NumRTVs];
    ID3D11DepthStencilView *pDepthStencilView;
    ID3D11UnorderedAccessView *pUAV;
};

//--------------------------------------------------------------------------------
class VideoMemoryTracker
{
public:
    static UINT32 s_TotalAllocatedSizeInBytes;
};

//--------------------------------------------------------------------------------
class SimpleRT : public VideoMemoryTracker
{
public:
    ID3D11Texture2D* pTexture;
    ID3D11RenderTargetView* pRTV;
    ID3D11ShaderResourceView* pSRV;
    UINT32 m_AllocatedSizeInBytes;

    SimpleRT(ID3D11Device* pd3dDevice, UINT Width, UINT Height, DXGI_FORMAT Format, UINT32 FormatSizeInBytes) :
        pTexture(NULL), pRTV(NULL), pSRV(NULL), m_AllocatedSizeInBytes(0)
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

        SAFE_CALL( pd3dDevice->CreateTexture2D( &desc, NULL, &pTexture ));
        SAFE_CALL( pd3dDevice->CreateShaderResourceView( pTexture, NULL, &pSRV ));
        SAFE_CALL( pd3dDevice->CreateRenderTargetView( pTexture, NULL, &pRTV ));

        m_AllocatedSizeInBytes = Width * Height * FormatSizeInBytes;
        s_TotalAllocatedSizeInBytes += m_AllocatedSizeInBytes;
    }

    SimpleRT(ID3D11Device* pd3dDevice, ID3D11Texture2D* pInTexture) :
        pTexture(NULL), pRTV(NULL), pSRV(NULL), m_AllocatedSizeInBytes(0)
    {
        SAFE_CALL( pd3dDevice->CreateShaderResourceView( pInTexture, NULL, &pSRV ));
        SAFE_CALL( pd3dDevice->CreateRenderTargetView( pInTexture, NULL, &pRTV ));
    }

    ~SimpleRT()
    {
        SAFE_RELEASE( pTexture );
        SAFE_RELEASE( pRTV );
        SAFE_RELEASE( pSRV );
        s_TotalAllocatedSizeInBytes -= m_AllocatedSizeInBytes;
    }
};

//--------------------------------------------------------------------------------
class SimpleUAV : public VideoMemoryTracker
{
public:
    ID3D11Texture2D* pTexture;
    ID3D11RenderTargetView* pRTV;
    ID3D11ShaderResourceView* pSRV;
    ID3D11UnorderedAccessView *pUAV;
    UINT32 m_AllocatedSizeInBytes;

    SimpleUAV(ID3D11Device* pd3dDevice, UINT Width, UINT Height, UINT ArraySize, DXGI_FORMAT Format, UINT32 FormatSizeInBytes) :
        pTexture(NULL), pRTV(NULL), pSRV(NULL), pUAV(NULL), m_AllocatedSizeInBytes(0)
    {
        D3D11_TEXTURE2D_DESC desc;
        desc.Width              = Width;
        desc.Height             = Height;
        desc.Format             = Format;
        desc.MipLevels          = 1;
        desc.ArraySize          = ArraySize;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage              = D3D11_USAGE_DEFAULT;
        desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS;
        desc.CPUAccessFlags     = 0;
        desc.MiscFlags          = 0;

        SAFE_CALL( pd3dDevice->CreateTexture2D( &desc, NULL, &pTexture ));
        SAFE_CALL( pd3dDevice->CreateShaderResourceView( pTexture, NULL, &pSRV ));
        SAFE_CALL( pd3dDevice->CreateRenderTargetView( pTexture, NULL, &pRTV ));
        SAFE_CALL( pd3dDevice->CreateUnorderedAccessView( pTexture, NULL, &pUAV ) );

        m_AllocatedSizeInBytes = Width * Height * ArraySize * FormatSizeInBytes;
        s_TotalAllocatedSizeInBytes += m_AllocatedSizeInBytes;
    }

    SimpleUAV(ID3D11Device* pd3dDevice, ID3D11Texture2D* pInTexture) :
        pTexture(NULL),pRTV(NULL), pSRV(NULL), pUAV(NULL), m_AllocatedSizeInBytes(0)
    {
        SAFE_CALL( pd3dDevice->CreateShaderResourceView( pInTexture, NULL, &pSRV ));
        SAFE_CALL( pd3dDevice->CreateRenderTargetView( pInTexture, NULL, &pRTV ));
        SAFE_CALL( pd3dDevice->CreateUnorderedAccessView( pInTexture, NULL, &pUAV ) );
    }

    ~SimpleUAV()
    {
        SAFE_RELEASE(pTexture);
        SAFE_RELEASE(pRTV);
        SAFE_RELEASE(pSRV);
        SAFE_RELEASE(pUAV);
        s_TotalAllocatedSizeInBytes -= m_AllocatedSizeInBytes;
    }
};

//--------------------------------------------------------------------------------
class HBAORenderTargets
{
public:
    HBAORenderTargets() :
          m_pD3DDevice(NULL)
        , m_pCustomRTs(NULL)
        , m_FullWidth(0)
        , m_FullHeight(0)
        , m_FullResAOZBuffer(NULL)
        , m_FullResAOZBuffer2(NULL)
        , m_FullResAOBuffer(NULL)
        , m_FullResAOBuffer2(NULL)
        , m_HalfResAOBuffer(NULL)
        , m_HalfResAOBuffer2(NULL)
        , m_FullResLinDepthBuffer(NULL)
        , m_HalfResLinDepthBuffer(NULL)
    {
    }

    void Release()
    {
        SAFE_DELETE(m_FullResAOZBuffer);
        SAFE_DELETE(m_FullResAOZBuffer2);
        SAFE_DELETE(m_FullResAOBuffer);
        SAFE_DELETE(m_FullResAOBuffer2);
        SAFE_DELETE(m_HalfResAOBuffer);
        SAFE_DELETE(m_HalfResAOBuffer2);
        SAFE_DELETE(m_FullResLinDepthBuffer);
        SAFE_DELETE(m_HalfResLinDepthBuffer);
    }

    void SetDevice(ID3D11Device *pD3DDevice)
    {
        m_pD3DDevice = pD3DDevice;
    }

    void SetFullResolution(UINT Width, UINT Height,
                           NVSDK_D3D11_RenderTargetsForAO *pCustomRTs)
    {
        m_FullWidth = Width;
        m_FullHeight = Height;
        m_pCustomRTs = pCustomRTs;
    }

    UINT GetFullWidth()
    {
        return m_FullWidth;
    }

    UINT GetFullHeight()
    {
        return m_FullHeight;
    }

    const NVSDK11::SimpleUAV *GetFullResAOZBuffer()
    {
        if (!m_FullResAOZBuffer)
        {
            m_FullResAOZBuffer = new NVSDK11::SimpleUAV(m_pD3DDevice, m_FullWidth, m_FullHeight, 1, DXGI_FORMAT_R16G16_FLOAT, 4);
        }
        return m_FullResAOZBuffer;
    }

    const NVSDK11::SimpleUAV *GetFullResAOZBuffer2()
    {
        if (!m_FullResAOZBuffer2)
        {
            m_FullResAOZBuffer2 = new NVSDK11::SimpleUAV(m_pD3DDevice, m_FullWidth, m_FullHeight, 1, DXGI_FORMAT_R16G16_FLOAT, 4);
        }
        return m_FullResAOZBuffer2;
    }

    const NVSDK11::SimpleUAV *GetFullResAOBuffer()
    {
        if (!m_FullResAOBuffer)
        {
            m_FullResAOBuffer = new NVSDK11::SimpleUAV(m_pD3DDevice, m_FullWidth, m_FullHeight, 1, DXGI_FORMAT_R16_FLOAT, 2);
        }
        return m_FullResAOBuffer;
    }

    const NVSDK11::SimpleUAV *GetFullResAOBuffer2()
    {
        if (!m_FullResAOBuffer2)
        {
            m_FullResAOBuffer2 = new NVSDK11::SimpleUAV(m_pD3DDevice, m_FullWidth, m_FullHeight, 1, DXGI_FORMAT_R16_FLOAT, 2);
        }
        return m_FullResAOBuffer2;
    }

    const NVSDK11::SimpleUAV *GetHalfResAOBuffer()
    {
        if (!m_HalfResAOBuffer)
        {
            m_HalfResAOBuffer = new NVSDK11::SimpleUAV(m_pD3DDevice, m_FullWidth/2, m_FullHeight/2, 1, DXGI_FORMAT_R16_FLOAT, 2);
        }
        return m_HalfResAOBuffer;
    }

    const NVSDK11::SimpleUAV *GetHalfResAOBuffer2()
    {
        if (!m_HalfResAOBuffer2)
        {
            m_HalfResAOBuffer2 = new NVSDK11::SimpleUAV(m_pD3DDevice, m_FullWidth/2, m_FullHeight/2, 1, DXGI_FORMAT_R16_FLOAT, 2);
        }
        return m_HalfResAOBuffer2;
    }

    const NVSDK11::SimpleRT *GetFullResLinDepthBuffer()
    {
        if (!m_FullResLinDepthBuffer)
        {
            if (m_pCustomRTs && m_pCustomRTs->pFullResLinDepthBuffer)
            {
                m_FullResLinDepthBuffer = new NVSDK11::SimpleRT(m_pD3DDevice, m_pCustomRTs->pFullResLinDepthBuffer);
            }
            else
            {
                m_FullResLinDepthBuffer = new NVSDK11::SimpleRT(m_pD3DDevice, m_FullWidth, m_FullHeight, DXGI_FORMAT_R32_FLOAT, 4);
            }
        }
        return m_FullResLinDepthBuffer;
    }

    const NVSDK11::SimpleRT *GetHalfResLinDepthBuffer()
    {
        if (!m_HalfResLinDepthBuffer)
        {
            if (m_pCustomRTs && m_pCustomRTs->pHalfResLinDepthBuffer)
            {
                m_HalfResLinDepthBuffer = new NVSDK11::SimpleRT(m_pD3DDevice, m_pCustomRTs->pHalfResLinDepthBuffer);
            }
            else
            {
                m_HalfResLinDepthBuffer = new NVSDK11::SimpleRT(m_pD3DDevice, m_FullWidth/2, m_FullHeight/2, DXGI_FORMAT_R32_FLOAT, 4);
            }
        }
        return m_HalfResLinDepthBuffer;
    }

protected:
    ID3D11Device *m_pD3DDevice;
    NVSDK_D3D11_RenderTargetsForAO *m_pCustomRTs;
    UINT m_FullWidth;
    UINT m_FullHeight;
    NVSDK11::SimpleUAV *m_FullResAOZBuffer;
    NVSDK11::SimpleUAV *m_FullResAOZBuffer2;
    NVSDK11::SimpleUAV *m_FullResAOBuffer;
    NVSDK11::SimpleUAV *m_FullResAOBuffer2;
    NVSDK11::SimpleUAV *m_HalfResAOBuffer;
    NVSDK11::SimpleUAV *m_HalfResAOBuffer2;
    NVSDK11::SimpleRT *m_FullResLinDepthBuffer;
    NVSDK11::SimpleRT *m_HalfResLinDepthBuffer;
};

//--------------------------------------------------------------------------------
class TimestampQueries
{
public:
    void Create(ID3D11Device *pD3DDevice);
    void Release();
    ID3D11Query *pDisjointTimestampQuery;
    ID3D11Query *pTimestampQueryBegin;
    ID3D11Query *pTimestampQueryZ;
    ID3D11Query *pTimestampQueryAO;
    ID3D11Query *pTimestampQueryBlurX;
    ID3D11Query *pTimestampQueryBlurY;
    ID3D11Query *pTimestampQueryEnd;
};

//--------------------------------------------------------------------------------
class HBAORenderer
{
public:
    HBAORenderer()
        : m_pD3DDevice(NULL)
    {
        InitAOParameters();
    }

    void SetAOParameters(const NVSDK_D3D11_AOParameters *pParams);

    UINT32 GetAllocatedVideoMemoryBytes() { return VideoMemoryTracker::s_TotalAllocatedSizeInBytes; }

    NVSDK_Status SetHardwareDepthsForAO(ID3D11Device *pDevice,
                                        ID3D11Texture2D *pFullResNonLinearDepthTex,
                                        float ZNear,
                                        float ZFar,
                                        float ZMin,
                                        float ZMax,
                                        float SceneScale);

    NVSDK_Status SetViewSpaceDepthsForAO(ID3D11Device *pD3DDevice,
                                         ID3D11Texture2D *pFullResLinearDepthTex,
                                         float SceneScale);

    NVSDK_Status RenderAO(ID3D11Device *pD3DDevice,
                          ID3D11DeviceContext* pDeviceContext,
                          float FovyRad,
                          ID3D11RenderTargetView *pOutputColorRTV,
                          NVSDK_D3D11_RenderTargetsForAO *pCustomRTs,
                          NVSDK_D3D11_RenderTimesForAO *pRenderTimes);

    void ReleaseAOResources();

protected:
    void InitAOParameters();
    void CreateStates(ID3D11Device *pD3DDevice);
    void CreateRandomTexture(ID3D11Device *pD3DDevice);
    void ReleaseStates();
    void CreateTimestampQueries(ID3D11Device *pD3DDevice);
    void CreateShaders(ID3D11Device *pD3DDevice);
    void ReleaseShaders();
    void InitAOResources(ID3D11Device *pD3DDevice);
    void UnbindCSResources(ID3D11DeviceContext* pDeviceContext);
    void SetFullscreenState(ID3D11DeviceContext* pDeviceContext);
    void ComputeRenderTimes(ID3D11DeviceContext* pDeviceContext, UINT64 Frequency, NVSDK_D3D11_RenderTimesForAO *pRenderTimes);
    void RenderLinearDepthPS(ID3D11DeviceContext* pDeviceContext);
    void RenderHBAOPS(ID3D11DeviceContext* pDeviceContext);
    void RenderHBAOCS(ID3D11DeviceContext* pDeviceContext);
    void RenderBlurXCS(ID3D11DeviceContext* pDeviceContext);
    void RenderBlurYCS(ID3D11DeviceContext* pDeviceContext);
    void RenderCompositePS(ID3D11DeviceContext* pDeviceContext, ID3D11RenderTargetView *pOutputColorRTV);

    inline bool CheckRTSizeNonMSAA(const D3D11_TEXTURE2D_DESC *desc, UINT Width, UINT Height);
    inline bool IsLinearDepthFormat(const D3D11_TEXTURE2D_DESC *desc);
    inline bool CheckLinearDepthRenderTarget(ID3D11Texture2D *pRT, UINT Width, UINT Height);
    inline bool CheckCustomRTs(UINT FullWidth, UINT FullHeight, const NVSDK_D3D11_RenderTargetsForAO *pCustomRTs);

    // Round a / b to nearest higher integer value
    inline UINT iDivUp(UINT a, UINT b)
    {
        return (a % b != 0) ? (a / b + 1) : (a / b);
    }

    ID3D11Device *m_pD3DDevice;
    NVSDK11::HBAOConstantBuffer m_ConstantBuffer;
    NVSDK11::HBAORenderOptions m_Options;
    NVSDK11::HBAOInputDepthInfo m_InputDepth;
    NVSDK11::HBAORenderTargets m_RTs;
    NVSDK11::TimestampQueries m_TimestampQueries;
    D3D11_VIEWPORT m_FullViewport;
    ID3D11ShaderResourceView *m_FullResLinDepthSRV;
    ID3D11ShaderResourceView *m_BlurXAOInputSRV;
    ID3D11Texture2D* m_RandomTexture;
    ID3D11ShaderResourceView* m_RandomTextureSRV;

    ID3D11BlendState *m_BlendState_Disabled;
    ID3D11BlendState *m_BlendState_Multiply;
    ID3D11DepthStencilState *m_DepthStencilState_Disabled;
    ID3D11RasterizerState *m_RasterizerState_Fullscreen_NoScissor;
    ID3D11SamplerState *m_SamplerState_PointClamp;
    ID3D11SamplerState *m_SamplerState_LinearClamp;
    ID3D11SamplerState *m_SamplerState_PointWrap;

    ID3D11VertexShader *m_pVS_FullScreenTriangle;
    ID3D11PixelShader *m_pPS_LinearizeDepthNoMSAA;
    ID3D11PixelShader *m_pPS_ResolveAndLinearizeDepthMSAA;
    ID3D11PixelShader *m_pPS_DownsampleDepth;
    ID3D11PixelShader *m_pPS_HBAO;
    ID3D11ComputeShader *m_pCS_HBAOX[NUM_HBAO_PERMUTATIONS];
    ID3D11ComputeShader *m_pCS_HBAOY[NUM_HBAO_PERMUTATIONS];
    ID3D11ComputeShader *m_pCS_HBAOXH[NUM_HBAO_PERMUTATIONS];
    ID3D11ComputeShader *m_pCS_HBAOYH[NUM_HBAO_PERMUTATIONS];
    ID3D11ComputeShader *m_pCS_UpsampleAndBlurX[NUM_BLUR_PERMUTATIONS];
    ID3D11ComputeShader *m_pCS_BlurX[NUM_BLUR_PERMUTATIONS];
    ID3D11ComputeShader *m_pCS_BlurY[NUM_BLUR_PERMUTATIONS];
    ID3D11PixelShader *m_pPS_BlurComposite;
};

}
