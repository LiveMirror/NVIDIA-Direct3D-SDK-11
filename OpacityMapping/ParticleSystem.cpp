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

#include "ParticleSystem.h"

#include "SDKmisc.h"

#include <yvals.h>
#ifdef _GLOBAL_USING
    #undef _GLOBAL_USING
    #define _GLOBAL_USING 0
#endif
#include <algorithm>

#define _USE_MATH_DEFINES
#include <math.h>

struct ParticleInstance
{
    D3DXVECTOR4 positionAndOpacity;
    D3DXVECTOR4 velocityAndLifetime;
    double birthTime;

    BOOL IsDead(double fTime) const
    {
        return (fTime - birthTime) > velocityAndLifetime.w;
    }

    FLOAT LifeParam(double fTime) const
    {
        return FLOAT(fTime - birthTime)/FLOAT(velocityAndLifetime.w);
    }
};

namespace
{
    const LPCWSTR ParticleStateFileName = L"ParticleState.bin";

    struct ParticleVertex
    {
        FLOAT instanceIndex;
        FLOAT cornerIndex;

        enum { NumLayoutElements = 1 };

        static const D3D11_INPUT_ELEMENT_DESC* GetLayout()
        {
            static const D3D11_INPUT_ELEMENT_DESC theLayout[NumLayoutElements] = {
                // SemanticName  SemanticIndex  Format                        InputSlot    AlignedByteOffset              InputSlotClass               InstanceDataStepRate
                { "POSITION",    0,             DXGI_FORMAT_R32G32_FLOAT,     0,           D3D11_APPEND_ALIGNED_ELEMENT,  D3D11_INPUT_PER_VERTEX_DATA, 0                    }
            };

            return theLayout;
        }
    };

    FLOAT randf()
    {
        return FLOAT(rand())/FLOAT(RAND_MAX);
    }

    void randPointInUnitDisk(FLOAT& x, FLOAT& y)
    {
        const FLOAT angle = 2.f * FLOAT(M_PI) * randf();
        const FLOAT r = sqrtf(randf());

        x = r * cosf(angle);
        y = r * sinf(angle);
    }

    struct DepthSortFunctor
    {
        DepthSortFunctor(const D3DXVECTOR3& viewDirArg) :
            viewDirection(viewDirArg)
        {
        }

        bool operator()(const ParticleInstance& lhs, const ParticleInstance& rhs) const
        {
            const FLOAT lhsDepth = D3DXVec3Dot(&viewDirection, (const D3DXVECTOR3*)&lhs.positionAndOpacity);
            const FLOAT rhsDepth = D3DXVec3Dot(&viewDirection, (const D3DXVECTOR3*)&rhs.positionAndOpacity);

            return lhsDepth > rhsDepth;
        }

        D3DXVECTOR3 viewDirection;
    };
}

CParticleSystem::CParticleSystem(UINT InstanceTextureWH) :
    m_InstanceTexture_EffectVar(0),
    m_ParticleScale_EffectVar(0),
    m_ParticleTexture_EffectVar(0),
    m_mLocalToWorld_EffectVar(0),
	m_ZLinParams_EffectVar(0),
	m_SoftParticlesFadeDistance_EffectVar(0),
	m_SoftParticlesDepthTexture_EffectVar(0),
    m_TessellationDensity_EffectVar(0),
    m_MaxTessellation_EffectVar(0),
    m_pEffectPass_ToSceneWireframeOverride(0),
    m_pEffectPass_ToSceneOverdrawMeteringOverride(0),
    m_pEffectPass_LowResToSceneOverride(0),
    m_pEffectPass_ToOpacity(0),
    m_bWireframe(FALSE),
    m_bMeteredOverdraw(FALSE),
    m_LightingMode(LM_Pixel),
    m_InstanceTextureWH(InstanceTextureWH),
    m_pInstanceTexture(NULL),
    m_pInstanceTextureSRV(NULL),
    m_AbsoluteMaxNumActiveParticles(InstanceTextureWH * InstanceTextureWH),
    m_MaxNumActiveParticles(0),
    m_NumActiveParticles(0),
    m_pParticleVertexBuffer(NULL),
    m_pParticleIndexBuffer(NULL),
    m_pParticleIndexBufferTessellated(NULL),
    m_pParticleVertexLayout(NULL),
    m_pParticleTextureSRV(NULL),
    m_ParticleLifeTime(3.f),
    m_TessellationDensity(300.f),
    m_MaxTessellation(8.f),
    m_ParticleScale(0.01f),
	m_SoftParticleFadeDistance(0.25f),
    m_pParticlePrevBuffer(NULL),
    m_pParticleCurrBuffer(NULL),
    m_FirstUpdate(TRUE),
    m_LastUpdateTime(0.0),
    m_EmissionRate(0.f)
{
    m_pParticlePrevBuffer= new ParticleInstance[m_AbsoluteMaxNumActiveParticles];
    m_pParticleCurrBuffer= new ParticleInstance[m_AbsoluteMaxNumActiveParticles];

	memset(m_pEffectPass_ToScene, 0, sizeof(m_pEffectPass_ToScene));
	memset(m_pEffectPass_SoftToScene, 0, sizeof(m_pEffectPass_SoftToScene));
}

CParticleSystem::~CParticleSystem()
{
    delete [] m_pParticlePrevBuffer;
    delete [] m_pParticleCurrBuffer;
}

void CParticleSystem::InitPlumeSim()
{
    m_MaxNumActiveParticles = 128 * 128;
    m_ParticleLifeTime = 10.f;
    m_ParticleScale = 0.05f;
    m_FirstUpdate = TRUE;
    m_LastUpdateTime = 0;

    m_EmissionRate = FLOAT(m_MaxNumActiveParticles)/m_ParticleLifeTime;
}

HRESULT CParticleSystem::LoadParticleState(double fTime)
{
	HRESULT hr;

	WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, ParticleStateFileName) );

    FILE* pFile = NULL;
    if(_wfopen_s(&pFile, str, L"rb"))
        return E_FAIL;

    // Read in the data
    UINT NumParticles = 0;
    if(fread(&NumParticles, sizeof(NumParticles), 1, pFile) < 1)
        return E_FAIL;
    if(NumParticles > m_MaxNumActiveParticles)
        return E_FAIL;

    D3DXVECTOR3 minCorner, maxCorner;
    if(fread(&minCorner, sizeof(minCorner), 1, pFile) < 1)
        return E_FAIL;
    if(fread(&maxCorner, sizeof(maxCorner), 1, pFile) < 1)
        return E_FAIL;

    if(fread(m_pParticlePrevBuffer, sizeof(m_pParticlePrevBuffer[0]), NumParticles, pFile) < NumParticles)
        return E_FAIL;

    fclose(pFile);

    // All is well, instate the new data
    m_NumActiveParticles = NumParticles;
    m_MinCorner = minCorner;
    m_MaxCorner = maxCorner;
    memcpy(m_pParticleCurrBuffer, m_pParticlePrevBuffer, m_NumActiveParticles * sizeof(m_pParticlePrevBuffer[0]));
    for(UINT i = 0; i != m_NumActiveParticles; ++i)
    {
        m_pParticleCurrBuffer[i].birthTime += fTime;
    }

    return S_OK;
}

HRESULT CParticleSystem::SaveParticleState(double fTime)
{
    // We temporarily mis-use the prev buffer so that we can rebase the birth times...
    memcpy(m_pParticlePrevBuffer, m_pParticleCurrBuffer, m_NumActiveParticles * sizeof(m_pParticleCurrBuffer[0]));
    for(UINT i = 0; i != m_NumActiveParticles; ++i)
    {
        m_pParticlePrevBuffer[i].birthTime -= fTime;
    }

    FILE* pFile = NULL;
    if(_wfopen_s(&pFile, ParticleStateFileName, L"wb+"))
        return E_FAIL;

    // Write out the data
    fwrite(&m_NumActiveParticles, sizeof(m_NumActiveParticles), 1, pFile);
    fwrite(&m_MinCorner, sizeof(m_MinCorner), 1, pFile);
    fwrite(&m_MaxCorner, sizeof(m_MaxCorner), 1, pFile);
    fwrite(m_pParticlePrevBuffer, sizeof(m_pParticlePrevBuffer[0]), m_NumActiveParticles, pFile);

    fclose(pFile);

    return S_OK;
}

void CParticleSystem::UpdateBounds(BOOL& firstUpdate, const D3DXVECTOR4& positionAndScale)
{
    D3DXVECTOR3 particleMin, particleMax;

    particleMin.x = positionAndScale.x - m_ParticleScale;
    particleMin.y = positionAndScale.y - m_ParticleScale;
    particleMin.z = positionAndScale.z - m_ParticleScale;

    particleMax.x = positionAndScale.x + m_ParticleScale;
    particleMax.y = positionAndScale.y + m_ParticleScale;
    particleMax.z = positionAndScale.z + m_ParticleScale;

    if(firstUpdate)
    {
        m_MinCorner = particleMin;
        m_MaxCorner = particleMax;
        firstUpdate = FALSE;
    }
    else
    {
        m_MinCorner.x = min(m_MinCorner.x, particleMin.x);
        m_MinCorner.y = min(m_MinCorner.y, particleMin.y);
        m_MinCorner.z = min(m_MinCorner.z, particleMin.z);

        m_MaxCorner.x = max(m_MaxCorner.x, particleMax.x);
        m_MaxCorner.y = max(m_MaxCorner.y, particleMax.y);
        m_MaxCorner.z = max(m_MaxCorner.z, particleMax.z);
    }
}

HRESULT CParticleSystem::PlumeSimTick(double fTime)
{
    if(m_FirstUpdate)
    {
        // Try loading the state
        LoadParticleState(fTime);

        m_LastUpdateTime = fTime;
        m_FirstUpdate = FALSE;
        return S_OK;
    }

    if(m_LastUpdateTime == fTime)
        return S_OK;

    const FLOAT timeDelta = FLOAT(fTime - m_LastUpdateTime);

    // Prepare to update bounds
    BOOL boundsFirstUpdate = TRUE;
    m_MinCorner = D3DXVECTOR3(0.f,0.f,0.f);
    m_MaxCorner = D3DXVECTOR3(0.f,0.f,0.f);

    // Flip the buffers ready for the simulation update
    ParticleInstance* pTemp = m_pParticleCurrBuffer;
    m_pParticleCurrBuffer = m_pParticlePrevBuffer;
    m_pParticlePrevBuffer = pTemp;

    // First, simulate the existing particles, killing as we go
    const ParticleInstance* pSrcEnd = m_pParticlePrevBuffer + m_NumActiveParticles;
    ParticleInstance* pDst = m_pParticleCurrBuffer;
    for(const ParticleInstance* pSrc = m_pParticlePrevBuffer; pSrc != pSrcEnd; ++pSrc)
    {
        if(!pSrc->IsDead(fTime))
        {
            D3DXVECTOR3 pos(pSrc->positionAndOpacity);
            pos += timeDelta * D3DXVECTOR3(pSrc->velocityAndLifetime);

            // Lifetime-driven opacity fade
            const FLOAT lifeParam = pSrc->LifeParam(fTime);
            FLOAT opacity = 1.f;
            if(lifeParam > 0.9f)
                opacity = 10.f * (1.f - lifeParam);

            pDst->positionAndOpacity = D3DXVECTOR4(pos, opacity);

            // Decel on upward motion
            pDst->velocityAndLifetime = pSrc->velocityAndLifetime;
            pDst->velocityAndLifetime.y -= 0.1f * timeDelta/m_ParticleLifeTime;

            // Copy remaining properties
            pDst->birthTime = pSrc->birthTime;

            UpdateBounds(boundsFirstUpdate, pDst->positionAndOpacity);

            ++pDst;
        }
    }

    const UINT DstParticlesLeft = UINT((m_pParticleCurrBuffer + m_MaxNumActiveParticles) - pDst);
    const UINT ParticleToEmitThisTick = min(DstParticlesLeft,UINT(m_EmissionRate * FLOAT(timeDelta)));
    for(UINT i = 0; i != ParticleToEmitThisTick; ++i, ++pDst)
    {
        FLOAT x, z;
        randPointInUnitDisk(x,z);

        pDst->positionAndOpacity.x = 0.1f * x;
        pDst->positionAndOpacity.y = m_ParticleScale;
        pDst->positionAndOpacity.z = 0.1f * z;
        pDst->positionAndOpacity.w = 1.f;

        FLOAT vx, vz;
        randPointInUnitDisk(vx,vz);

        pDst->velocityAndLifetime.x = 0.05f * vx;
        pDst->velocityAndLifetime.y = 0.1f + 0.05f * randf();
        pDst->velocityAndLifetime.z = 0.05f * vz;
        pDst->velocityAndLifetime.w = randf() * m_ParticleLifeTime;

        pDst->birthTime = fTime;

        UpdateBounds(boundsFirstUpdate, pDst->positionAndOpacity);
    }

    m_NumActiveParticles = UINT(pDst - m_pParticleCurrBuffer);

    m_LastUpdateTime= fTime;

    return S_OK;
}

HRESULT CParticleSystem::SortByDepth(const D3DXMATRIX& mView)
{
    const D3DXVECTOR3 viewDir(mView._13, mView._23, mView._33);

    std::sort(m_pParticleCurrBuffer, m_pParticleCurrBuffer + m_NumActiveParticles, DepthSortFunctor(viewDir));

    return S_OK;
}

HRESULT CParticleSystem::UpdateD3D11Resources(ID3D11DeviceContext* pDC)
{
    HRESULT hr;

    D3D11_MAPPED_SUBRESOURCE mappy;
    V_RETURN(pDC->Map(m_pInstanceTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappy));

    BYTE* pCurrRowStart = (BYTE*)mappy.pData;
    D3DXFLOAT16* pCurrTexel = (D3DXFLOAT16*)pCurrRowStart;
    D3DXFLOAT16* pCurrRowEnd = pCurrTexel + (4 * m_InstanceTextureWH);
    const ParticleInstance* pCurrSrc = m_pParticleCurrBuffer;
    for(UINT ix = 0; ix != m_NumActiveParticles; ++ix)
    {
        pCurrTexel[0] = pCurrSrc->positionAndOpacity.x;
        pCurrTexel[1] = pCurrSrc->positionAndOpacity.y;
        pCurrTexel[2] = pCurrSrc->positionAndOpacity.z;
        pCurrTexel[3] = pCurrSrc->positionAndOpacity.w;

        ++pCurrSrc;
        pCurrTexel += 4; 
        if(pCurrTexel == pCurrRowEnd)
        {
            // New row!
            pCurrRowStart = pCurrRowStart + mappy.RowPitch;
            pCurrTexel = (D3DXFLOAT16*)pCurrRowStart;
            pCurrRowEnd = pCurrTexel + (4 * m_InstanceTextureWH);
        }
    }

    pDC->Unmap(m_pInstanceTexture,0);

    return S_OK;
}

HRESULT CParticleSystem::BindToEffect(ID3D11Device* pd3dDevice, ID3DX11Effect* pEffect)
{
	HRESULT hr;

    m_InstanceTexture_EffectVar = pEffect->GetVariableByName("g_InstanceTexture")->AsShaderResource();
    m_ParticleTexture_EffectVar = pEffect->GetVariableByName("g_ParticleTexture")->AsShaderResource();
    m_ParticleScale_EffectVar = pEffect->GetVariableByName("g_ParticleScale")->AsScalar();
    m_mLocalToWorld_EffectVar = pEffect->GetVariableByName("g_mLocalToWorld")->AsMatrix();
	m_ZLinParams_EffectVar = pEffect->GetVariableByName("g_ZLinParams")->AsVector();
	m_SoftParticlesFadeDistance_EffectVar = pEffect->GetVariableByName("g_SoftParticlesFadeDistance")->AsScalar();
	m_SoftParticlesDepthTexture_EffectVar = pEffect->GetVariableByName("g_SoftParticlesDepthTexture")->AsShaderResource();

    m_TessellationDensity_EffectVar = pEffect->GetVariableByName("g_TessellationDensity")->AsScalar();
    m_MaxTessellation_EffectVar = pEffect->GetVariableByName("g_MaxTessellation")->AsScalar();

    m_pEffectPass_ToScene[LM_Vertex] = pEffect->GetTechniqueByName("RenderParticlesToScene_LM_Vertex")->GetPassByIndex(0);
	m_pEffectPass_SoftToScene[LM_Vertex] = pEffect->GetTechniqueByName("RenderSoftParticlesToScene_LM_Vertex")->GetPassByIndex(0);
    m_pEffectPass_ToScene[LM_Pixel] = pEffect->GetTechniqueByName("RenderParticlesToScene_LM_Pixel")->GetPassByIndex(0);
	m_pEffectPass_SoftToScene[LM_Pixel] = pEffect->GetTechniqueByName("RenderSoftParticlesToScene_LM_Pixel")->GetPassByIndex(0);
    m_pEffectPass_LowResToSceneOverride = pEffect->GetTechniqueByName("RenderLowResParticlesToSceneOverride")->GetPassByIndex(0);
    m_pEffectPass_ToSceneWireframeOverride = pEffect->GetTechniqueByName("RenderParticlesToSceneWireframeOverride")->GetPassByIndex(0);
    m_pEffectPass_ToSceneOverdrawMeteringOverride = pEffect->GetTechniqueByName("RenderToSceneOverdrawMeteringOverride")->GetPassByIndex(0);

    m_pEffectPass_ToOpacity = pEffect->GetTechniqueByName("RenderParticlesToOpacity")->GetPassByIndex(0);
    m_pEffectPass_ToScene[LM_Tessellated] = NULL;
	m_pEffectPass_SoftToScene[LM_Tessellated] = NULL;
    if(pd3dDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0)
    {
        m_pEffectPass_ToScene[LM_Tessellated] = pEffect->GetTechniqueByName("RenderParticlesToScene_LM_Tessellated")->GetPassByIndex(0);
		m_pEffectPass_SoftToScene[LM_Tessellated] = pEffect->GetTechniqueByName("RenderSoftParticlesToScene_LM_Tessellated")->GetPassByIndex(0);
    }

	SAFE_RELEASE(m_pParticleVertexLayout);
    D3DX11_PASS_DESC passDesc;
    m_pEffectPass_ToScene[LM_Vertex]->GetDesc(&passDesc);
    V_RETURN( pd3dDevice->CreateInputLayout( ParticleVertex::GetLayout(), ParticleVertex::NumLayoutElements,
                                             passDesc.pIAInputSignature, passDesc.IAInputSignatureSize,
                                             &m_pParticleVertexLayout) );
	return S_OK;
}

HRESULT CParticleSystem::OnD3D11CreateDevice(ID3D11Device* pd3dDevice)
{
    HRESULT hr;

    // Set up particle instance buffers
    {
        D3D11_TEXTURE2D_DESC texDesc;
        texDesc.Width = m_InstanceTextureWH;
        texDesc.Height = m_InstanceTextureWH;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        texDesc.SampleDesc.Count = 1; 
        texDesc.SampleDesc.Quality = 0; 
        texDesc.Usage = D3D11_USAGE_DYNAMIC;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        texDesc.MiscFlags = 0;

        V_RETURN( pd3dDevice->CreateTexture2D(&texDesc, NULL, &m_pInstanceTexture) );

        D3D11_SHADER_RESOURCE_VIEW_DESC rvDesc;
        rvDesc.Format = texDesc.Format;
        rvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        rvDesc.Texture2D.MostDetailedMip = 0;
        rvDesc.Texture2D.MipLevels = texDesc.MipLevels;

        V_RETURN( pd3dDevice->CreateShaderResourceView(m_pInstanceTexture, &rvDesc, &m_pInstanceTextureSRV) );
    }

    // Set up particle vertex/index buffers
    ParticleVertex* pVertices = new ParticleVertex[4 * m_AbsoluteMaxNumActiveParticles];
    DWORD* pIndices = new DWORD[6 * m_AbsoluteMaxNumActiveParticles];
    DWORD* pTessellatedIndices = new DWORD[4 * m_AbsoluteMaxNumActiveParticles];
    {
        ParticleVertex* pCurrVertex = pVertices;
        DWORD* pCurrIndex = pIndices;
        DWORD* pCurrTessellatedIndex = pTessellatedIndices;
        DWORD currBaseIndex = 0;
        for(UINT ix = 0; ix != m_AbsoluteMaxNumActiveParticles; ++ix)
        {
            pCurrIndex[0] = currBaseIndex + 0;
            pCurrIndex[1] = currBaseIndex + 1;
            pCurrIndex[2] = currBaseIndex + 2;
            pCurrIndex[3] = currBaseIndex + 2;
            pCurrIndex[4] = currBaseIndex + 1;
            pCurrIndex[5] = currBaseIndex + 3;

            pCurrTessellatedIndex[0] = currBaseIndex + 0;
            pCurrTessellatedIndex[1] = currBaseIndex + 1;
            pCurrTessellatedIndex[2] = currBaseIndex + 2;
            pCurrTessellatedIndex[3] = currBaseIndex + 3;

            pCurrVertex[0].instanceIndex = FLOAT(ix);
            pCurrVertex[1].instanceIndex = FLOAT(ix);
            pCurrVertex[2].instanceIndex = FLOAT(ix);
            pCurrVertex[3].instanceIndex = FLOAT(ix);

            pCurrVertex[0].cornerIndex = 0;
            pCurrVertex[1].cornerIndex = 1;
            pCurrVertex[2].cornerIndex = 2;
            pCurrVertex[3].cornerIndex = 3;
           
            currBaseIndex += 4;
            pCurrVertex += 4;
            pCurrIndex += 6;
            pCurrTessellatedIndex += 4;
        }
    }

    {
        D3D11_BUFFER_DESC vbDesc;
        vbDesc.ByteWidth = 4 * m_AbsoluteMaxNumActiveParticles * sizeof(ParticleVertex);
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.CPUAccessFlags = 0;
        vbDesc.MiscFlags = 0;
        vbDesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA vbSRD;
        vbSRD.pSysMem = pVertices;
        vbSRD.SysMemPitch = 0;
        vbSRD.SysMemSlicePitch = 0;

        V_RETURN( pd3dDevice->CreateBuffer(&vbDesc, &vbSRD, &m_pParticleVertexBuffer) );
    }

    {
        D3D11_BUFFER_DESC ibDesc;
        ibDesc.ByteWidth = 6 * m_AbsoluteMaxNumActiveParticles * sizeof(DWORD);
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibDesc.CPUAccessFlags = 0;
        ibDesc.MiscFlags = 0;
        ibDesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA ibSRD;
        ibSRD.pSysMem = pIndices;
        ibSRD.SysMemPitch = 0;
        ibSRD.SysMemSlicePitch = 0;

        V_RETURN( pd3dDevice->CreateBuffer(&ibDesc, &ibSRD, &m_pParticleIndexBuffer) );
    }

    {
        D3D11_BUFFER_DESC ibDesc;
        ibDesc.ByteWidth = 4 * m_AbsoluteMaxNumActiveParticles * sizeof(DWORD);
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibDesc.CPUAccessFlags = 0;
        ibDesc.MiscFlags = 0;
        ibDesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA ibSRD;
        ibSRD.pSysMem = pTessellatedIndices;
        ibSRD.SysMemPitch = 0;
        ibSRD.SysMemSlicePitch = 0;

        V_RETURN( pd3dDevice->CreateBuffer(&ibDesc, &ibSRD, &m_pParticleIndexBufferTessellated) );
    }

    delete [] pVertices;
    delete [] pIndices;
    delete [] pTessellatedIndices;

    // Set up particle texture
    {
        WCHAR str[MAX_PATH];
        V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"OpacityMapping\\smoke.dds" ) );

        ID3D11Resource* pParticleTextureRsrc = NULL;
        V_RETURN( D3DX11CreateTextureFromFile(pd3dDevice, str, NULL, NULL, &pParticleTextureRsrc, NULL ) );

        // Downcast to texture 2D
        ID3D11Texture2D* pParticleTexture = NULL;
        V_RETURN( pParticleTextureRsrc->QueryInterface( IID_ID3D11Texture2D, (LPVOID*)&pParticleTexture ) );
        SAFE_RELEASE(pParticleTextureRsrc);

        D3D11_TEXTURE2D_DESC texDesc;
        pParticleTexture->GetDesc(&texDesc);

        D3D11_SHADER_RESOURCE_VIEW_DESC rvDesc;
        rvDesc.Format = texDesc.Format;
        rvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        rvDesc.Texture2D.MostDetailedMip = 0;
        rvDesc.Texture2D.MipLevels = texDesc.MipLevels;

        V_RETURN( pd3dDevice->CreateShaderResourceView(pParticleTexture, &rvDesc, &m_pParticleTextureSRV) );

        SAFE_RELEASE(pParticleTexture);
    }

    return S_OK;
}

HRESULT CParticleSystem::Draw(ID3D11DeviceContext* pDC, BOOL UseTessellation)
{
    pDC->IASetInputLayout(m_pParticleVertexLayout);

    UINT Stride = sizeof(ParticleVertex);
    UINT Offset = 0;
    pDC->IASetVertexBuffers(0, 1, &m_pParticleVertexBuffer, &Stride, &Offset);

    ID3D11Buffer* pIndexBuffer = UseTessellation ? m_pParticleIndexBufferTessellated : m_pParticleIndexBuffer;
    pDC->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    D3D11_PRIMITIVE_TOPOLOGY topology = UseTessellation ? D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    pDC->IASetPrimitiveTopology(topology);

    UINT NumIndices = UseTessellation ? m_NumActiveParticles * 4 : m_NumActiveParticles * 6;
    pDC->DrawIndexed(NumIndices, 0, 0);

    return S_OK;
}

HRESULT CParticleSystem::SetEffectVars()
{
    HRESULT hr;

    V_RETURN( m_InstanceTexture_EffectVar->SetResource(m_pInstanceTextureSRV) );
    V_RETURN( m_ParticleScale_EffectVar->SetFloat(m_ParticleScale) );
    V_RETURN( m_TessellationDensity_EffectVar->SetFloat(m_TessellationDensity) );
	V_RETURN( m_MaxTessellation_EffectVar->SetFloat(m_MaxTessellation) );
    V_RETURN( m_ParticleTexture_EffectVar->SetResource(m_pParticleTextureSRV) );

    D3DXMATRIXA16 mIdentity;
    D3DXMatrixIdentity(&mIdentity);
    V_RETURN( m_mLocalToWorld_EffectVar->SetMatrix((FLOAT*)&mIdentity) );

    return S_OK;
}

void CParticleSystem::OnD3D11DestroyDevice()
{
    SAFE_RELEASE( m_pParticleVertexLayout );

    SAFE_RELEASE( m_pInstanceTexture );
    SAFE_RELEASE( m_pInstanceTextureSRV );
    SAFE_RELEASE( m_pParticleVertexBuffer );
    SAFE_RELEASE( m_pParticleIndexBuffer );
    SAFE_RELEASE( m_pParticleIndexBufferTessellated );

    SAFE_RELEASE( m_pParticleTextureSRV );
}

HRESULT CParticleSystem::DrawToOpacity(ID3D11DeviceContext* pDC)
{
    HRESULT hr;
    V_RETURN( SetEffectVars() );
    V_RETURN( m_pEffectPass_ToOpacity->Apply(0, pDC) );
    V_RETURN( Draw(pDC, FALSE) );
    return S_OK;
}

HRESULT CParticleSystem::DrawToSceneLowRes(ID3D11DeviceContext* pDC)
{
    HRESULT hr;

    BOOL UseTessellation = FALSE;
    ID3DX11EffectPass* pPass = m_pEffectPass_ToScene[m_LightingMode];
	if(LM_Tessellated == m_LightingMode)
	{
		if(pPass)
			UseTessellation = TRUE;
		else
			pPass =  m_pEffectPass_ToScene[LM_Pixel];
	}

    V_RETURN( SetEffectVars() );
    V_RETURN( pPass->Apply(0, pDC) );

    if(m_bWireframe)
    {
        V_RETURN( m_pEffectPass_ToSceneWireframeOverride->Apply(0, pDC) );
    }

    V_RETURN( m_pEffectPass_LowResToSceneOverride->Apply(0, pDC) );

    V_RETURN( Draw(pDC, UseTessellation) );

    return S_OK;
}

HRESULT CParticleSystem::DrawSoftToSceneLowRes(	ID3D11DeviceContext* pDC,
												ID3D11ShaderResourceView* pDepthStencilSRV,
												FLOAT zNear, FLOAT zFar
												)
{
    HRESULT hr;

    BOOL UseTessellation = FALSE;
    ID3DX11EffectPass* pPass = m_pEffectPass_SoftToScene[m_LightingMode];
	if(LM_Tessellated == m_LightingMode)
	{
		if(pPass)
			UseTessellation = TRUE;
		else
			pPass =  m_pEffectPass_SoftToScene[LM_Pixel];
	}

	// Set soft-specific params
	FLOAT ZLinParams[4] = { zFar/zNear, (zFar/zNear - 1.f), zFar, 0.f };
	V_RETURN(m_ZLinParams_EffectVar->SetFloatVector(ZLinParams));
	V_RETURN(m_SoftParticlesFadeDistance_EffectVar->SetFloat(m_SoftParticleFadeDistance * m_ParticleScale));
	V_RETURN(m_SoftParticlesDepthTexture_EffectVar->SetResource(pDepthStencilSRV));

    V_RETURN( SetEffectVars() );
    V_RETURN( pPass->Apply(0, pDC) );

    if(m_bWireframe)
    {
        V_RETURN( m_pEffectPass_ToSceneWireframeOverride->Apply(0, pDC) );
    }

    V_RETURN( m_pEffectPass_LowResToSceneOverride->Apply(0, pDC) );

    V_RETURN( Draw(pDC, UseTessellation) );

    return S_OK;
}

HRESULT CParticleSystem::DrawToScene(ID3D11DeviceContext* pDC)
{
    HRESULT hr;

    BOOL UseTessellation = FALSE;
    ID3DX11EffectPass* pPass = m_pEffectPass_ToScene[m_LightingMode];
	if(LM_Tessellated == m_LightingMode)
	{
		if(pPass)
			UseTessellation = TRUE;
		else
			pPass =  m_pEffectPass_ToScene[LM_Pixel];
	}

    V_RETURN( SetEffectVars() );
    V_RETURN( pPass->Apply(0, pDC) );

    if(m_bWireframe)
    {
        V_RETURN( m_pEffectPass_ToSceneWireframeOverride->Apply(0, pDC) );
    }
    else if(m_bMeteredOverdraw)
    {
        V_RETURN( m_pEffectPass_ToSceneOverdrawMeteringOverride->Apply(0, pDC) );
    }

    V_RETURN( Draw(pDC, UseTessellation) );

    return S_OK;
}

void CParticleSystem::SetLightingMode(LightingMode lm)
{
    m_LightingMode = lm;
}

void CParticleSystem::SetWireframe(BOOL f)
{
    m_bWireframe = f;
}
