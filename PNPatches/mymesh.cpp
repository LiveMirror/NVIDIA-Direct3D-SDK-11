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
#include "SDKmisc.h"
#include "d3dx11effect.h"
#include "mymesh.h"
#include "wsnormalmap.h"
#include "sdkmesh.h"

#include "meshloader/DX9AnimationLoader.h"

#include <vector>
#include <algorithm>

ID3D11Device* MyMesh::m_pDevice     = NULL;
ID3DX11Effect* MyMesh::g_pEffect    = NULL;
float MyMesh::g_TessellationFactor  = 256;
double MyMesh::g_TessellationFactorResScale = 1;
bool MyMesh::g_RenderWireframe      = false;
bool MyMesh::g_UseTessellation      = true;
bool MyMesh::g_RenderNormals        = false;
bool MyMesh::g_bFixTheSeams         = true;
bool MyMesh::g_bEnableAnimation    = false;
bool MyMesh::g_bUpdateCamera        = true;
bool MyMesh::s_EffectDirty          = true;
eRenderModes MyMesh::s_RenderMode   = RENDER_DEFAULT;

void MyMesh::Init(ID3D11Device* pd3dDevice)
{
	m_pDevice = pd3dDevice;
    m_pDevice->AddRef();

    LoadEffect();
}

bool MyMesh::HaveCommonVerts(int iPoly1, int iPoly2)
{
	if (iPoly1 == iPoly2)
		return true;
	int nVerts1, nVerts2;
	int4 Indices1, Indices2;
	GetPolyVertIndicesFromPolyIndex(iPoly1, nVerts1, Indices1);
	GetPolyVertIndicesFromPolyIndex(iPoly2, nVerts2, Indices2);
	for (int i1 = 0; i1 < nVerts1; ++i1)
	{
		for (int i2 = 0; i2 < nVerts2; ++i2)
		{
			if (Indices1[i1] == Indices2[i2])
				return true;
		}
	}
	return false;
}

void MyMesh::LoadEffect()
{
    ID3DXBuffer* pShader = NULL;
	WCHAR path[1024];

    DXUTFindDXSDKMediaFileCch(path, 1024, L"pnpatches.fx");
	std::vector<D3DXMACRO> macros;
	if (MyMesh::g_bFixTheSeams)
	{
		D3DXMACRO macro[] = { { "FIX_THE_SEAMS", "1" } };
		macros.push_back(macro[0]);
	}
	D3DXMACRO macro[] = { { NULL, NULL } };
	macros.push_back(macro[0]);
	D3DXCompileShaderFromFile(path, &macros[0], NULL, NULL, "fx_5_0", 0, &pShader, NULL, NULL);

    if (pShader)
    {
        SAFE_RELEASE(g_pEffect);    
        D3DX11CreateEffectFromMemory(pShader->GetBufferPointer(), pShader->GetBufferSize(), 0, m_pDevice, &g_pEffect);
        pShader->Release();

        s_EffectDirty = true;
    }
    else
    {
        MessageBox(NULL, L"D3DXCompileShaderFromFile", L"Error", MB_OK);    
    }
}

void MyMesh::Destroy()
{
	SAFE_RELEASE(g_pEffect);
    SAFE_RELEASE(m_pDevice);
}

void MyMesh::RenderBasis(ID3D11DeviceContext *pd3dImmediateContext, D3DXMATRIX mViewProj, bool renderAnimated)
{
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ID3DX11EffectTechnique* pTechnique = NULL;
        
    if (renderAnimated)
        pTechnique = g_pEffect->GetTechniqueByName("RenderVectorsAnimated");
    else
        pTechnique = g_pEffect->GetTechniqueByName("RenderVectors");
        
    // Render normals
    g_pEffect->GetVariableByName("g_PositionsBuffer")->AsShaderResource()->SetResource(g_pMeshVertexBufferSRV);
    g_pEffect->GetVariableByName("g_VectorsBuffer")->AsShaderResource()->SetResource(g_pMeshNormalsBufferSRV);
	pTechnique->GetPassByIndex(0)->Apply(0, pd3dImmediateContext);
    pd3dImmediateContext->Draw(g_MeshVerticesNum * 2, 0);

    // Render tangents
    g_pEffect->GetVariableByName("g_VectorsBuffer")->AsShaderResource()->SetResource(g_pMeshTangentsBufferSRV);
    pTechnique->GetPassByIndex(1)->Apply(0, pd3dImmediateContext);
    pd3dImmediateContext->Draw(g_MeshVerticesNum * 2, 0);
}

void MyMesh::UpdateView(D3DXMATRIX& mView, D3DXMATRIX& mProj, D3DXVECTOR3& cameraPos)
{
    static bool isFirstRun = true;
    static D3DXVECTOR4 planeNormals[4];
    static D3DXVECTOR3 cameraFrustumOrigin;
    
    if (g_bUpdateCamera || isFirstRun)
    {
        D3DXVECTOR3 worldRight = D3DXVECTOR3(mView._11, mView._21, mView._31);
        D3DXVECTOR3 worldUp = D3DXVECTOR3(mView._12, mView._22, mView._32);
        D3DXVECTOR3 worldForward = D3DXVECTOR3(mView._13, mView._23, mView._33);

        float clipNear = -mProj._43 / mProj._33;
        float clipFar = clipNear * mProj._33 / (mProj._33 - 1.0f);

        D3DXVECTOR3 cameraFrustum[4];

        D3DXVECTOR3 centerFar = cameraPos + worldForward * clipFar;
        D3DXVECTOR3 offsetH = (clipFar / mProj._11) * worldRight;
        D3DXVECTOR3 offsetV = (clipFar / mProj._22) * worldUp;
        cameraFrustum[0] = centerFar - offsetV - offsetH;
        cameraFrustum[1] = centerFar + offsetV - offsetH;
        cameraFrustum[2] = centerFar + offsetV + offsetH;
        cameraFrustum[3] = centerFar - offsetV + offsetH;

        // left/top planes normals
        D3DXVECTOR3 normal;
        D3DXVECTOR3 temp = cameraFrustum[1] - cameraPos;
        D3DXVec3Cross(&normal, &temp, &worldUp);
        D3DXVec3Normalize((D3DXVECTOR3*)&planeNormals[0], &normal);
        D3DXVec3Cross(&normal, &temp, &worldRight);
        D3DXVec3Normalize((D3DXVECTOR3*)&planeNormals[1], &normal);

        // right/bottom planes normals
        temp = cameraFrustum[3] - cameraPos;
        D3DXVec3Cross(&normal, &worldUp, &temp);
        D3DXVec3Normalize((D3DXVECTOR3*)&planeNormals[2], &normal);
        D3DXVec3Cross(&normal, &worldRight, &temp);
        D3DXVec3Normalize((D3DXVECTOR3*)&planeNormals[3], &normal);

        cameraFrustumOrigin = cameraPos;
        
        isFirstRun = false;
    }

    if (g_bUpdateCamera || s_EffectDirty)
    {
        g_pEffect->GetVariableByName("g_FrustumNormals")->AsVector()->SetFloatVectorArray((float*)planeNormals, 0, 4);
        g_pEffect->GetVariableByName("g_FrustumOrigin")->AsVector()->SetFloatVector(cameraFrustumOrigin);
    }
    
    s_EffectDirty = false;
}

void MyMesh::Render(ID3D11DeviceContext *pd3dImmediateContext, D3DXMATRIX& mView, D3DXMATRIX& mProj, D3DXVECTOR3& cameraPos)
{
    UpdateView(mView, mProj, cameraPos);

    ID3DX11EffectPass* pOverridePass = NULL;

    switch (s_RenderMode)
    {
        case RENDER_NORMAL_MAP:
            pOverridePass = g_pEffect->GetTechniqueByName("RenderDebug")->GetPassByName("RenderNormalMap");
            break;
        case RENDER_TEXTURE_COORDINATES:
            pOverridePass = g_pEffect->GetTechniqueByName("RenderDebug")->GetPassByName("RenderTexCoord");
            break;
        case RENDER_CHECKER_BOARD:
            pOverridePass = g_pEffect->GetTechniqueByName("RenderDebug")->GetPassByName("RenderCheckerBoard");
            break;
        case RENDER_DISPLACEMENT_MAP:
            pOverridePass = g_pEffect->GetTechniqueByName("RenderDebug")->GetPassByName("RenderDisplacementMap");
            break;
    }

    ID3DX11EffectPass* pPostProcessPass = NULL;
        
    if (g_RenderWireframe)
        pPostProcessPass = g_pEffect->GetTechniqueByName("RenderWireframe")->GetPassByIndex(0);

    // Render triangles
	pd3dImmediateContext->IASetInputLayout(NULL);

    // Constants
    D3DXMATRIX mViewProj = mView * mProj;
	g_pEffect->GetVariableByName("g_ModelViewProjectionMatrix")->AsMatrix()->SetMatrix(mViewProj);
    g_pEffect->GetVariableByName("g_CameraPosition")->AsVector()->SetFloatVector(cameraPos);
    g_pEffect->GetVariableByName("g_DisplacementScale")->AsScalar()->SetFloat(m_DisplacementScale);
    g_pEffect->GetVariableByName("g_MapDimensions")->AsVector()->SetIntVector(m_MapDimensions.vector);
	
	// Buffers & resources
	g_pEffect->GetVariableByName("g_PositionsBuffer")->AsShaderResource()->SetResource(g_pMeshVertexBufferSRV);
	g_pEffect->GetVariableByName("g_CoordinatesBuffer")->AsShaderResource()->SetResource(g_pMeshCoordinatesBufferSRV);
	g_pEffect->GetVariableByName("g_IndicesBuffer")->AsShaderResource()->SetResource(g_pMeshTrgsIndexBufferSRV);
	g_pEffect->GetVariableByName("g_NormalsBuffer")->AsShaderResource()->SetResource(g_pMeshNormalsBufferSRV);
    g_pEffect->GetVariableByName("g_TangentsBuffer")->AsShaderResource()->SetResource(g_pMeshTangentsBufferSRV);
	g_pEffect->GetVariableByName("g_CornerCoordinatesBuffer")->AsShaderResource()->SetResource(g_pMeshCornerCoordinatesBufferSRV);
    g_pEffect->GetVariableByName("g_EdgeCoordinatesBuffer")->AsShaderResource()->SetResource(g_pMeshTriEdgeCoordinatesBufferSRV);

    g_pEffect->GetVariableByName("g_DisplacementTexture")->AsShaderResource()->SetResource(g_pDisplacementMapSRV);
    g_pEffect->GetVariableByName("g_WSNormalMap")->AsShaderResource()->SetResource(g_pNormalMapSRV);

    if (g_UseTessellation && m_TessellationFactor != 1.0f)
    {
	    pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
	    g_pEffect->GetVariableByName("g_TessellationFactor")->AsScalar()->SetFloat(m_TessellationFactor * g_TessellationFactorResScale);
        g_pEffect->GetTechniqueByName("RenderTessellatedTriangles")->GetPassByIndex(0)->Apply(0, pd3dImmediateContext);
    }
    else
    {
	    pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	    g_pEffect->GetTechniqueByName("RenderTriangles")->GetPassByIndex(0)->Apply(0, pd3dImmediateContext);
    }

    if (pOverridePass)
        pOverridePass->Apply(0, pd3dImmediateContext);
    
    pd3dImmediateContext->Draw(g_MeshTrgIndicesNum, 0);

    if (pPostProcessPass)
    {
	    pPostProcessPass->Apply(0, pd3dImmediateContext);
	    pd3dImmediateContext->Draw(g_MeshTrgIndicesNum, 0);
    }

    // Disable tessellation
    g_pEffect->GetTechniqueByName("Default")->GetPassByIndex(0)->Apply(0, pd3dImmediateContext);

    // Render quads
    pd3dImmediateContext->IASetInputLayout(NULL);

    // Buffers & resources
    g_pEffect->GetVariableByName("g_IndicesBuffer")->AsShaderResource()->SetResource(g_pMeshQuadsIndexBufferSRV);
    g_pEffect->GetVariableByName("g_EdgeCoordinatesBuffer")->AsShaderResource()->SetResource(g_pMeshQuadEdgeCoordinatesBufferSRV);

    pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
    if (g_UseTessellation && m_TessellationFactor != 1.0f)
    {
	    g_pEffect->GetVariableByName("g_TessellationFactor")->AsScalar()->SetFloat(m_TessellationFactor * g_TessellationFactorResScale);
		g_pEffect->GetTechniqueByName("RenderTessellatedQuads")->GetPassByIndex(0)->Apply(0, pd3dImmediateContext);
    }
    else
    {
	    g_pEffect->GetVariableByName("g_TessellationFactor")->AsScalar()->SetFloat(1.0f);
	    g_pEffect->GetTechniqueByName("RenderQuads")->GetPassByIndex(0)->Apply(0, pd3dImmediateContext);
    }

    if (pOverridePass) pOverridePass->Apply(0, pd3dImmediateContext);
    
    pd3dImmediateContext->Draw(g_MeshQuadIndicesNum, 0);

    if (pPostProcessPass)
    {
	    pPostProcessPass->Apply(0, pd3dImmediateContext);
	    pd3dImmediateContext->Draw(g_MeshQuadIndicesNum, 0);
    }

	// Disable tessellation
	g_pEffect->GetTechniqueByName("Default")->GetPassByIndex(0)->Apply(0, pd3dImmediateContext);

	if (g_RenderNormals)
	{
		// Render tangent basis
        RenderBasis(pd3dImmediateContext, mViewProj);
	}
}

void MyMesh::RenderAnimated(ID3D11DeviceContext *pd3dDeviceContext, D3DXMATRIX& mView, D3DXMATRIX& mProj, D3DXVECTOR3& cameraPos, float time)
{
    UpdateView(mView, mProj, cameraPos);

    ID3DX11EffectPass* pOverridePass = NULL;

    switch (s_RenderMode)
    {
        case RENDER_NORMAL_MAP:
            pOverridePass = g_pEffect->GetTechniqueByName("RenderDebug")->GetPassByName("RenderNormalMap");
            break;
        case RENDER_TEXTURE_COORDINATES:
            pOverridePass = g_pEffect->GetTechniqueByName("RenderDebug")->GetPassByName("RenderTexCoord");
            break;
        case RENDER_CHECKER_BOARD:
            pOverridePass = g_pEffect->GetTechniqueByName("RenderDebug")->GetPassByName("RenderCheckerBoard");
            break;
        case RENDER_DISPLACEMENT_MAP:
            pOverridePass = g_pEffect->GetTechniqueByName("RenderDebug")->GetPassByName("RenderDisplacementMap");
            break;
    }

    ID3DX11EffectPass* pPostProcessPass = NULL;
        
    if (g_RenderWireframe)
        pPostProcessPass = g_pEffect->GetTechniqueByName("RenderWireframe")->GetPassByIndex(0);

    // Render triangles
	pd3dDeviceContext->IASetInputLayout(NULL);

    // Constants
    D3DXMATRIX mViewProj = mView * mProj;
	g_pEffect->GetVariableByName("g_ModelViewProjectionMatrix")->AsMatrix()->SetMatrix(mViewProj);
    g_pEffect->GetVariableByName("g_CameraPosition")->AsVector()->SetFloatVector(cameraPos);
    g_pEffect->GetVariableByName("g_DisplacementScale")->AsScalar()->SetFloat(m_DisplacementScale);
	
	// Buffers & resources
	g_pEffect->GetVariableByName("g_PositionsBuffer")->AsShaderResource()->SetResource(g_pMeshVertexBufferSRV);
	g_pEffect->GetVariableByName("g_CoordinatesBuffer")->AsShaderResource()->SetResource(g_pMeshCoordinatesBufferSRV);
	g_pEffect->GetVariableByName("g_IndicesBuffer")->AsShaderResource()->SetResource(g_pMeshTrgsIndexBufferSRV);
	g_pEffect->GetVariableByName("g_NormalsBuffer")->AsShaderResource()->SetResource(g_pMeshNormalsBufferSRV);
    g_pEffect->GetVariableByName("g_TangentsBuffer")->AsShaderResource()->SetResource(g_pMeshTangentsBufferSRV);
	g_pEffect->GetVariableByName("g_CornerCoordinatesBuffer")->AsShaderResource()->SetResource(g_pMeshCornerCoordinatesBufferSRV);
    g_pEffect->GetVariableByName("g_EdgeCoordinatesBuffer")->AsShaderResource()->SetResource(g_pMeshTriEdgeCoordinatesBufferSRV);
    
    g_pEffect->GetVariableByName("g_DisplacementTexture")->AsShaderResource()->SetResource(g_pDisplacementMapSRV);
    g_pEffect->GetVariableByName("g_WSNormalMap")->AsShaderResource()->SetResource(g_pNormalMapSRV);

    // Animation stuff
    int bonesNum = m_pCharacterAnimation->GetBonesNum();
    D3DXMATRIX* pFrame = m_pCharacterAnimation->GetFrame(time);
    g_pEffect->GetVariableByName("mAnimationMatrixArray")->AsMatrix()->SetMatrixArray((float*)pFrame, 0, bonesNum);

    g_pEffect->GetVariableByName("g_BoneIndicesBuffer")->AsShaderResource()->SetResource(m_pBoneIndicesBufferSRV);
    g_pEffect->GetVariableByName("g_BoneWeightsBuffer")->AsShaderResource()->SetResource(m_pBoneWeightsBufferSRV);

    if (m_TessellationFactor != 1.0f)
    {
	    g_pEffect->GetVariableByName("g_TessellationFactor")->AsScalar()->SetFloat(m_TessellationFactor * g_TessellationFactorResScale);
        pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
        g_pEffect->GetTechniqueByName("RenderTessellatedTrianglesAnimated")->GetPassByIndex(0)->Apply(0, pd3dDeviceContext);
    }
    else
    {
	    g_pEffect->GetVariableByName("g_TessellationFactor")->AsScalar()->SetFloat(1.0f);
        pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_pEffect->GetTechniqueByName("RenderTrianglesAnimated")->GetPassByIndex(0)->Apply(0, pd3dDeviceContext);
    }
    
    if (pOverridePass) pOverridePass->Apply(0, pd3dDeviceContext);
    
    pd3dDeviceContext->Draw(g_MeshTrgIndicesNum, 0);

    if (g_RenderWireframe)
    {
        g_pEffect->GetTechniqueByName("RenderWireframe")->GetPassByIndex(0)->Apply(0, pd3dDeviceContext);
	    pd3dDeviceContext->Draw(g_MeshTrgIndicesNum, 0);
    }

    if (g_RenderNormals)
	{
		// Render tangent basis
        RenderBasis(pd3dDeviceContext, mViewProj, true);
	}
}

void GetTangentBasis(D3DXVECTOR4& tangent, D3DXVECTOR4& bitangent, D3DXVECTOR3& v0, D3DXVECTOR3& v1, D3DXVECTOR3& v2, D3DXVECTOR2& t0, D3DXVECTOR2& t1, D3DXVECTOR2& t2)
{
    // positions delta
    D3DXVECTOR3 v01 = v1 - v0;
	D3DXVECTOR3 v02 = v2 - v0;

    // texture coordinates delta
    D3DXVECTOR2 t01 = t1 - t0;
    D3DXVECTOR2 t02 = t2 - t0;

    float rValue = 1.0f / (t01.x * t02.y - t01.y * t02.x);

    tangent.x = (t02.y * v01.x - t01.y * v02.x) * rValue;
    tangent.y = (t02.y * v01.y - t01.y * v02.y) * rValue;
    tangent.z = (t02.y * v01.z - t01.y * v02.z) * rValue;
    tangent.w = D3DXVec3Length((D3DXVECTOR3*)&tangent);
    
    D3DXVec3Normalize((D3DXVECTOR3*)&tangent, (D3DXVECTOR3*)&tangent);

    bitangent.x = (t01.x * v02.x - t02.x * v01.x) * rValue;
    bitangent.y = (t01.x * v02.y - t02.x * v01.y) * rValue;
    bitangent.z = (t01.x * v02.z - t02.x * v01.z) * rValue;
    bitangent.w = D3DXVec3Length((D3DXVECTOR3*)&bitangent);
    
    D3DXVec3Normalize((D3DXVECTOR3*)&bitangent, (D3DXVECTOR3*)&bitangent);
}

void GetTangentBasis(D3DXVECTOR4& tangent, D3DXVECTOR3& v0, D3DXVECTOR3& v1, D3DXVECTOR3& v2, D3DXVECTOR2& t0, D3DXVECTOR2& t1, D3DXVECTOR2& t2)
{
    // positions delta
    D3DXVECTOR3 v01 = v1 - v0;
	D3DXVECTOR3 v02 = v2 - v0;

    // texture coordinates delta
    D3DXVECTOR2 t01 = t1 - t0;
    D3DXVECTOR2 t02 = t2 - t0;

    float rValue = 1.0f / (t01.x * t02.y - t01.y * t02.x);

    tangent.x = (t02.y * v01.x - t01.y * v02.x) * rValue;
    tangent.y = (t02.y * v01.y - t01.y * v02.y) * rValue;
    tangent.z = (t02.y * v01.z - t01.y * v02.z) * rValue;
    tangent.w = D3DXVec3Length((D3DXVECTOR3*)&tangent);
    
    D3DXVec3Normalize((D3DXVECTOR3*)&tangent, (D3DXVECTOR3*)&tangent);
}

// Load mesh from file
HRESULT MyMesh::LoadMeshFromFile(WCHAR* fileName)
{
    HRESULT hr = S_OK;
    
    char buffer[1024];
    sprintf(buffer, "Loading mesh: %S", fileName);
	SetProgressString(buffer);

    WCHAR filePath[1024];
	V_RETURN(DXUTFindDXSDKMediaFileCch(filePath, 1024, fileName));

    std::wstring fileExtension(fileName);
	std::string::size_type lastDotPos = fileExtension.find_last_of('.');
    if (lastDotPos != std::string::npos) fileExtension = fileExtension.substr(lastDotPos);

    std::transform(fileExtension.begin(), fileExtension.end(), fileExtension.begin(), tolower);
    
    if (fileExtension.compare(L".obj") == 0)
    {
        V_RETURN(LoadMeshFromOBJ(filePath));
    }
    else if (fileExtension.compare(L".x") == 0)
    {
        V_RETURN(LoadMeshFromX(filePath));
    }
    else
    {
        std::wstring message(L"Unknown format: ");
        message.append(fileName);
        MessageBoxW(NULL, &message[0], L"Error", MB_OK);
        return hr;
    }

    // Generate all necessary attributes
    ProcessMeshBuffers();

    // Create buffer resources
    CreateBufferResources();

	// we are done
	SetProgress(-1);

    return hr;
}

HRESULT MyMesh::LoadMeshFromOBJ(WCHAR* path)
{
    HRESULT hr = S_OK;
    FILE* pFile;
	
    if (_wfopen_s(&pFile, path, L"r") != 0) return DXUTERR_MEDIANOTFOUND;

    char buffer[1024];
    char* token;
	int index;
	
    // Initialize progress computation
	fseek(pFile, 0, SEEK_END);
	size_t fileSize = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);
	float fProgress = 0;
	int iLine = 0;
	
    while(!feof(pFile))
	{
		++iLine;
        fgets(buffer, 1024, pFile);

        // report progress
		if ((iLine & 1023) == 0)
		{
			size_t curPos = ftell(pFile);
			float fTmp = (float)curPos / fileSize;
			if (fTmp - fProgress > 0.05f)
			{
				SetProgress(fProgress = fTmp);
			}
		}

		if (buffer[0] == '#') continue; // skip comments

		if (buffer[0] == 'v' && buffer[1] == ' ')
        { // vertex
			index = 0;
			float4 element;

			token = strtok(buffer + 1, " ");

			while (token != NULL && index < 3)
			{
				element.vector[index++] = (float)atof(token);
				token = strtok(NULL, " ");
			}

			if (index == 3)
            {
                m_Vertices.push_back(element);
            }
		}
		else if (buffer[0] == 'v' && buffer[1] == 't' && buffer[2] == ' ')
        { // texture coordinate
			index = 0;
			float2 element;

			token = strtok(buffer + 2, " ");

			while (token != NULL && index < 2)
			{
				element.vector[index++] = (float)atof(token);
				token = strtok(NULL, " ");
			}

			if (index == 2) 
			{
				element.vector[1] = 1.0f - element.vector[1]; // flip v coordinate
				m_TextureCoords.push_back(element);
			}
		}
		else if (buffer[0] == 'f' && buffer[1] == ' ')
        { // face
			index = 0;
			int2 element[4];

			token = strtok(buffer + 1, " /");

			while (token != NULL)
			{
				if (index == 4)
				{
					// too many vertices - ignore this patch
					index = 0;
					break;
				}
				element[index].vector[0] = atoi(token) - 1; // .obj indices are 1 based
				element[index].vector[1] = atoi(strtok(NULL, " /")) - 1;
				token = strtok(NULL, " /");
				if (index > 0) // there are vertices - check that this new vertex does not repeat any of existing ones
				{
					for (int i = 0; ; )
					{
						if (m_Vertices[element[i][0]] == m_Vertices[element[index][0]]) // we already have such vertex
							break;
						if (++i == index)
						{ // we do not have such vertex - increment vertex count
							++index;
							break;
						}
					}
				}
				else index = 1; // first vertex
			}
			if (index == 4) // quad patch
			{ 
#if 1
				m_QuadIndices.push_back(element[0]);
				m_QuadIndices.push_back(element[1]);
				m_QuadIndices.push_back(element[2]);
				m_QuadIndices.push_back(element[3]);
#else
                m_TriIndices.push_back(element[0]);
				m_TriIndices.push_back(element[1]);
				m_TriIndices.push_back(element[2]);

                m_TriIndices.push_back(element[0]);
				m_TriIndices.push_back(element[2]);
				m_TriIndices.push_back(element[3]);
#endif
			}
			else if (index == 3) // triangle patch
			{
				m_TriIndices.push_back(element[0]);
				m_TriIndices.push_back(element[1]);
				m_TriIndices.push_back(element[2]);
			}
		}
	}

    fclose(pFile);

    return hr;
}

HRESULT MyMesh::LoadMeshFromX(WCHAR* path)
{
 	HRESULT hr = S_OK;

    DX9AnimationLoader loader;
    V_RETURN(loader.Load(path));

    D3DVERTEXELEMENT9 pDecl[] = 
        {
            { 0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
            { 0, 24, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            { 0, 32, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT,  0 },
            D3DDECL_END()
        };

    LPD3DXMESH pd3dMesh = NULL;
    D3DXMATRIX staticMatrix;
    V_RETURN(loader.CloneMesh(0, pDecl, &pd3dMesh, staticMatrix));
    
    if (pd3dMesh)
    {
        struct _vertex
        {
            D3DXVECTOR3 position;
            D3DXVECTOR3 normal;
            D3DXVECTOR2 textureCoord;
            D3DXVECTOR3 tangent;
        };

        _vertex* pSourceVertices = NULL;
        pd3dMesh->LockVertexBuffer(0, (void**)&pSourceVertices);

        g_MeshVerticesNum = pd3dMesh->GetNumVertices();
        m_Vertices.resize(g_MeshVerticesNum);
        m_Normals.resize(g_MeshVerticesNum);
        m_TextureCoords.resize(g_MeshVerticesNum);
        m_Tangents.resize(g_MeshVerticesNum);

        for (unsigned i=0; i<g_MeshVerticesNum; ++i)
        {
#if 0
            memcpy(&m_Vertices[i], &pSourceVertices[i].position, sizeof(D3DXVECTOR3));
#else
            m_Vertices[i].xyz() = (float3 &)pSourceVertices[i].position;
            m_Vertices[i][3] = 1;
#endif
            memcpy(&m_Normals[i], &pSourceVertices[i].normal, sizeof(D3DXVECTOR3));
            memcpy(&m_TextureCoords[i], &pSourceVertices[i].textureCoord, sizeof(D3DXVECTOR2));
            memcpy(&m_Tangents[i], &pSourceVertices[i].tangent, sizeof(D3DXVECTOR3));
        }
        
        pd3dMesh->UnlockVertexBuffer();

        g_MeshTrgIndicesNum = pd3dMesh->GetNumFaces() * 3;
        m_TriIndices.resize(g_MeshTrgIndicesNum, int2(0, 0));

        int* pIndexData = NULL;
        pd3dMesh->LockIndexBuffer(0, (void**)&pIndexData);
            
        if(pd3dMesh->GetOptions() & D3DXMESH_32BIT)
        {
            for (int i=0; i<g_MeshTrgIndicesNum; ++i)
                m_TriIndices[i].vector[0] = m_TriIndices[i].vector[1] = pIndexData[i];
        }
        else
        {
            MessageBoxW(NULL, L"Only 32-bit mesh indices are supported", L"Error", MB_OK);
        }

        pd3dMesh->UnlockIndexBuffer();

        SAFE_RELEASE(pd3dMesh);

        m_BoneWeights.bones.resize(g_MeshVerticesNum, int4(0, 0, 0, 0));
        m_BoneWeights.weights.resize(g_MeshVerticesNum, float4(0, 0, 0, 0));

        loader.GetAnimationData(&m_pCharacterAnimation, m_BoneWeights, 1.0f/30);
    }

    loader.Cleanup();

    return hr;
}

void MyMesh::CreateBufferResources(void)
{
    g_MeshVerticesNum = (UINT)m_Vertices.size();
	g_MeshQuadIndicesNum = (UINT)m_QuadIndices.size();
	g_MeshTrgIndicesNum = (UINT)m_TriIndices.size();

	// Positions
	D3D11_BUFFER_DESC bufDesc;
	memset(&bufDesc, 0, sizeof(bufDesc));
	bufDesc.ByteWidth = g_MeshVerticesNum * sizeof(float4);
	bufDesc.Usage = D3D11_USAGE_DEFAULT;
	bufDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA subresourceData;
	subresourceData.pSysMem = &m_Vertices[0];\
	m_pDevice->CreateBuffer(&bufDesc, &subresourceData, &g_pMeshVertexBuffer);

	D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
	memset(&descSRV, 0, sizeof(descSRV));
	descSRV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	descSRV.Buffer.FirstElement = 0;
	descSRV.Buffer.NumElements = g_MeshVerticesNum;
	descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	m_pDevice->CreateShaderResourceView(g_pMeshVertexBuffer, &descSRV, &g_pMeshVertexBufferSRV);

	// Normals
    if (m_Normals.empty() == false)
    {
	    subresourceData.pSysMem = m_Normals[0].vector;
	    m_pDevice->CreateBuffer(&bufDesc, &subresourceData, &g_pMeshNormalsBuffer);

	    descSRV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	    m_pDevice->CreateShaderResourceView(g_pMeshNormalsBuffer, &descSRV, &g_pMeshNormalsBufferSRV);
    }

    // Tangents
    if (m_Tangents.empty() == false)
    {
        subresourceData.pSysMem = m_Tangents[0].vector;
	    m_pDevice->CreateBuffer(&bufDesc, &subresourceData, &g_pMeshTangentsBuffer);

	    descSRV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	    m_pDevice->CreateShaderResourceView(g_pMeshTangentsBuffer, &descSRV, &g_pMeshTangentsBufferSRV);
    }

	// Texture coordinates
    if (m_TextureCoords.empty() == false)
    {
	    bufDesc.ByteWidth = (UINT)m_TextureCoords.size() * sizeof(float2);
	    subresourceData.pSysMem = &m_TextureCoords[0];
	    m_pDevice->CreateBuffer(&bufDesc, &subresourceData, &g_pMeshCoordinatesBuffer);

	    descSRV.Format = DXGI_FORMAT_R32G32_FLOAT;
	    descSRV.Buffer.NumElements = (UINT)m_TextureCoords.size();
	    m_TextureCoords.clear();
	    m_pDevice->CreateShaderResourceView(g_pMeshCoordinatesBuffer, &descSRV, &g_pMeshCoordinatesBufferSRV);
    }

	// Corner texture coordinates
    if (m_CornerCoordinates.empty() == false)
    {
	    bufDesc.ByteWidth = (UINT)m_CornerCoordinates.size() * sizeof(float2);
	    subresourceData.pSysMem = &m_CornerCoordinates[0];
	    m_pDevice->CreateBuffer(&bufDesc, &subresourceData, &g_pMeshCornerCoordinatesBuffer);

	    descSRV.Format = DXGI_FORMAT_R32G32_FLOAT;
	    descSRV.Buffer.NumElements = (UINT)m_CornerCoordinates.size();
	    m_CornerCoordinates.clear();
	    m_pDevice->CreateShaderResourceView(g_pMeshCornerCoordinatesBuffer, &descSRV, &g_pMeshCornerCoordinatesBufferSRV);
    }

    // Edge texture coordinates
    if (m_TriEdgeCoordinates.empty() == false)
    {
        bufDesc.ByteWidth = (UINT)m_TriEdgeCoordinates.size() * sizeof(float4);
        subresourceData.pSysMem = &m_TriEdgeCoordinates[0];
        m_pDevice->CreateBuffer(&bufDesc, &subresourceData, &g_pMeshTriEdgeCoordinatesBuffer);

        descSRV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        descSRV.Buffer.NumElements = (UINT)m_TriEdgeCoordinates.size();
        m_TriEdgeCoordinates.clear();
        m_pDevice->CreateShaderResourceView(g_pMeshTriEdgeCoordinatesBuffer, &descSRV, &g_pMeshTriEdgeCoordinatesBufferSRV);
    }

    if (m_QuadEdgeCoordinates.empty() == false)
    {
        bufDesc.ByteWidth = (UINT)m_QuadEdgeCoordinates.size() * sizeof(float4);
        subresourceData.pSysMem = &m_QuadEdgeCoordinates[0];
        m_pDevice->CreateBuffer(&bufDesc, &subresourceData, &g_pMeshQuadEdgeCoordinatesBuffer);

        descSRV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        descSRV.Buffer.NumElements = (UINT)m_QuadEdgeCoordinates.size();
        m_QuadEdgeCoordinates.clear();
        m_pDevice->CreateShaderResourceView(g_pMeshQuadEdgeCoordinatesBuffer, &descSRV, &g_pMeshQuadEdgeCoordinatesBufferSRV);
    }

	// Index buffers
	bufDesc.ByteWidth = g_MeshQuadIndicesNum * sizeof(int2);
	if (bufDesc.ByteWidth > 0)
	{
		subresourceData.pSysMem = &m_QuadIndices[0];
		m_pDevice->CreateBuffer(&bufDesc, &subresourceData, &g_pMeshQuadsIndexBuffer);

		descSRV.Format = DXGI_FORMAT_R32G32_SINT;
		descSRV.Buffer.NumElements = g_MeshQuadIndicesNum;
		m_QuadIndices.clear();
		m_pDevice->CreateShaderResourceView(g_pMeshQuadsIndexBuffer, &descSRV, &g_pMeshQuadsIndexBufferSRV);
	}

    bufDesc.ByteWidth = g_MeshTrgIndicesNum * sizeof(int2);
	if (bufDesc.ByteWidth > 0)
	{
		subresourceData.pSysMem = &m_TriIndices[0];
		m_pDevice->CreateBuffer(&bufDesc, &subresourceData, &g_pMeshTrgsIndexBuffer);

		descSRV.Format = DXGI_FORMAT_R32G32_SINT;
        descSRV.Buffer.NumElements = g_MeshTrgIndicesNum;
		m_TriIndices.clear();
		m_pDevice->CreateShaderResourceView(g_pMeshTrgsIndexBuffer, &descSRV, &g_pMeshTrgsIndexBufferSRV);
	}

    // Animation stuff
    if (m_BoneWeights.bones.empty() == false)
    {
        memset(&bufDesc, 0, sizeof(bufDesc));
	    bufDesc.ByteWidth = g_MeshVerticesNum * sizeof(int4);
	    bufDesc.Usage = D3D11_USAGE_DEFAULT;
	    bufDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	    D3D11_SUBRESOURCE_DATA subresourceData;
        subresourceData.pSysMem = &m_BoneWeights.bones[0];
	    m_pDevice->CreateBuffer(&bufDesc, &subresourceData, &m_pBoneIndicesBuffer);

        memset(&descSRV, 0, sizeof(descSRV));
        descSRV.Format = DXGI_FORMAT_R32G32B32A32_SINT;
	    descSRV.Buffer.FirstElement = 0;
	    descSRV.Buffer.NumElements = g_MeshVerticesNum;
	    descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	    m_pDevice->CreateShaderResourceView(m_pBoneIndicesBuffer, &descSRV, &m_pBoneIndicesBufferSRV);

	    bufDesc.ByteWidth = g_MeshVerticesNum * sizeof(float4);
        subresourceData.pSysMem = &m_BoneWeights.weights[0];
        m_pDevice->CreateBuffer(&bufDesc, &subresourceData, &m_pBoneWeightsBuffer);

        descSRV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	    m_pDevice->CreateShaderResourceView(m_pBoneWeightsBuffer, &descSRV, &m_pBoneWeightsBufferSRV);
    }
}

void MyMesh::GenerateAdjacencyInfo(void)
{
    SetProgressString("GenerateAdjacencyInfo()...");
	SetProgress(0);
    int verticesNum = (int)m_Vertices.size();
    if (verticesNum == 0)
		return;
    int triIndicesNum = (int)m_AdjTriIndices.size();
    int quadIndicesNum = (int)m_AdjQuadIndices.size();

    m_FacesPerVertex.resize(verticesNum);
    memset(&m_FacesPerVertex[0], 0, verticesNum * sizeof(int));

    for (int i=0; i < quadIndicesNum; ++i) {
		int vertex = m_AdjQuadIndices[i];
		m_FacesPerVertex[vertex]++;
	}
    for (int i=0; i < triIndicesNum; ++i) {
		int vertex = m_AdjTriIndices[i];
		m_FacesPerVertex[vertex]++;
	}    	
    // calculate prefix sum for the number of faces per vertex
    for (int i=0; i<verticesNum - 1; ++i)
	    m_FacesPerVertex[i+1] += m_FacesPerVertex[i];

    // collect all face indices per vertex
    m_FacesCollected.resize(verticesNum);
    memset(&m_FacesCollected[0], 0, verticesNum * sizeof(int));

    m_AdjacencyFaces.resize(m_FacesPerVertex[verticesNum-1]);

    // build list of faces for each vertex
	for (int i=0; i<quadIndicesNum; ++i) {
		int vertex = m_AdjQuadIndices[i];
		m_FacesCollected[vertex]++;
		m_AdjacencyFaces[m_FacesPerVertex[vertex] - m_FacesCollected[vertex]] = int(i / 4) * 4; // base quad index
	}
    for (int i=0; i<triIndicesNum; ++i) {
		int vertex = m_AdjTriIndices[i];
		m_FacesCollected[vertex]++;
        m_AdjacencyFaces[m_FacesPerVertex[vertex] - m_FacesCollected[vertex]] = quadIndicesNum + int(i / 3) * 3; // base triangle index
	}
}

void MyMesh::ProcessMeshBuffers(void)
{
    int triIndicesNum = (int)m_TriIndices.size();
    int quadIndicesNum = (int)m_QuadIndices.size();

    if (triIndicesNum == 0 && quadIndicesNum == 0) return;

	int packedVerticesNum = GeneratePackedPosIndices(true);
    GenerateAdjacencyInfo();
    SetProgressString("ProcessMeshBuffers()...");
	SetProgress(0);

    int verticesNum = (int)m_Vertices.size();
    assert(packedVerticesNum == verticesNum);
    
    m_CornerCoordinates.resize(verticesNum);

    // Corner coordinates
    for (int v=0; v<verticesNum; ++v)
    {
        int baseFaceIndex = m_AdjacencyFaces[m_FacesPerVertex[v] - 1];
        int texCoordIndex = -1;

		if (baseFaceIndex < quadIndicesNum) // quad patch
		{
			for (int i=0; i<4; ++i) {
				if (m_QuadIndices[baseFaceIndex + i][0] == v)
					texCoordIndex = m_QuadIndices[baseFaceIndex + i][1];
			}
		}
		else
		{
			baseFaceIndex -= quadIndicesNum;

			for (int i=0; i<3; ++i) {
				if (m_TriIndices[baseFaceIndex + i][0] == v)
					texCoordIndex = m_TriIndices[baseFaceIndex + i][1];
			}
		}

		assert(texCoordIndex != -1);

		if (texCoordIndex >= 0)
			m_CornerCoordinates[v] = m_TextureCoords[texCoordIndex];
	}

    // Edge coordinates    
    std::vector<int2> meshQuadEdgeIndices(quadIndicesNum, int2(-1, -1));
    std::vector<int2> meshTriEdgeIndices(triIndicesNum, int2(-1, -1));
        
    for (int f=0; f<quadIndicesNum; f+=4)
    {
        int masterFaceIndex = f;
        
        for (int i=0; i<4; i++)
        {
            int edge[2] = {f + i, f + (i+1)%4};
            int vertexIndices[2] = {m_AdjQuadIndices[edge[0]], m_AdjQuadIndices[edge[1]]};

            // Continue if the edge has been processed earlier
            if (meshQuadEdgeIndices[masterFaceIndex + i].vector[0] != -1) continue;

            meshQuadEdgeIndices[masterFaceIndex + i][0] = m_QuadIndices[edge[0]][1];
            meshQuadEdgeIndices[masterFaceIndex + i][1] = m_QuadIndices[edge[1]][1];

            bool neighbourFound = false;
            int vertex = vertexIndices[0];

            float2 masterCoordinatesDelta = m_TextureCoords[m_QuadIndices[edge[0]].vector[1]] - 
                                            m_TextureCoords[m_QuadIndices[edge[1]].vector[1]];
            float masterCoordinatesLength = D3DXVec2Length((D3DXVECTOR2*)&masterCoordinatesDelta);
                            
            // Search for neighbour around #0 vertex
            for (int j=0; j<m_FacesCollected[vertex]; j++)
            {
                int baseFaceIndex = m_AdjacencyFaces[m_FacesPerVertex[vertex]-j-1];
                
                //if (baseFaceIndex != masterFaceIndex)
                {
                    if (baseFaceIndex < quadIndicesNum) // Quad patch neighbour
                    {
                        if (baseFaceIndex == masterFaceIndex) continue;
                        
                        for (int k=0; k<4; k++)
                        {
                            if (m_AdjQuadIndices[baseFaceIndex+k] == vertexIndices[1])
                            {
                                neighbourFound = true;
                                int slaveEdgeIndex = k;

                                float2 slaveCoordinatesDelta =  m_TextureCoords[m_QuadIndices[baseFaceIndex + k][1]] - 
                                                                m_TextureCoords[m_QuadIndices[baseFaceIndex + (k+1)%4][1]];
                                float slaveCoordinatesLength =  D3DXVec2Length((D3DXVECTOR2*)&slaveCoordinatesDelta);

                                if (masterCoordinatesLength >= slaveCoordinatesLength)
                                {
                                    // Indices order is swapped for neighbouring patches
                                    meshQuadEdgeIndices[baseFaceIndex + slaveEdgeIndex][0] = m_QuadIndices[edge[1]][1];
                                    meshQuadEdgeIndices[baseFaceIndex + slaveEdgeIndex][1] = m_QuadIndices[edge[0]][1];
                                }
                                else
                                {
                                    edge[0] = m_QuadIndices[baseFaceIndex + k][1];
                                    edge[1] = m_QuadIndices[baseFaceIndex + (k+1)%4][1];

                                    meshQuadEdgeIndices[baseFaceIndex + slaveEdgeIndex][0] = edge[0];
                                    meshQuadEdgeIndices[baseFaceIndex + slaveEdgeIndex][1] = edge[1];

                                    meshQuadEdgeIndices[masterFaceIndex + i][0] = edge[1];
                                    meshQuadEdgeIndices[masterFaceIndex + i][1] = edge[0];
                                }

                                break;
                            }
                        }
                    }
                    else // Triangle patch neighbour
                    {
                        int _baseFaceIndex = baseFaceIndex - quadIndicesNum;
                        if (_baseFaceIndex == masterFaceIndex) continue;
                        
                        for (int k=0; k<3; k++)
                        {
                            if (m_AdjTriIndices[_baseFaceIndex+k] == vertexIndices[1])
                            {
                                neighbourFound = true;
                                int slaveEdgeIndex = k;
                                
                                float2 slaveCoordinatesDelta =  m_TextureCoords[m_TriIndices[_baseFaceIndex + slaveEdgeIndex].vector[1]] - 
                                                                m_TextureCoords[m_TriIndices[_baseFaceIndex + (slaveEdgeIndex+1)%3].vector[1]];
                                float slaveCoordinatesLength =  D3DXVec2Length((D3DXVECTOR2*)&slaveCoordinatesDelta);

                                if (masterCoordinatesLength >= slaveCoordinatesLength)
                                {
                                    // Indices order is swapped for neighbouring patches
                                    meshTriEdgeIndices[_baseFaceIndex + slaveEdgeIndex].vector[0] = m_QuadIndices[edge[1]].vector[1];
                                    meshTriEdgeIndices[_baseFaceIndex + slaveEdgeIndex].vector[1] = m_QuadIndices[edge[0]].vector[1];
                                }
                                else
                                {
                                    edge[0] = m_TriIndices[_baseFaceIndex + k].vector[1];
                                    edge[1] = m_TriIndices[_baseFaceIndex + (k+1)%3].vector[1];

                                    meshTriEdgeIndices[_baseFaceIndex + slaveEdgeIndex].vector[0] = edge[0];
                                    meshTriEdgeIndices[_baseFaceIndex + slaveEdgeIndex].vector[1] = edge[1];

                                    meshQuadEdgeIndices[masterFaceIndex + i].vector[0] = edge[1];
                                    meshQuadEdgeIndices[masterFaceIndex + i].vector[1] = edge[0];
                                }

                                break;
                            }
                        }
                    }
                }
            }

            if (neighbourFound) continue;

            // Search for neighbour around #1 vertex
            vertex = vertexIndices[1];
            for (int j=0; j<m_FacesCollected[vertex]; j++)
            {
                int baseFaceIndex = m_AdjacencyFaces[m_FacesPerVertex[vertex]-j-1];
                
                //if (baseFaceIndex != masterFaceIndex)
                {
                    if (baseFaceIndex < quadIndicesNum) // Quad patch neighbour
                    {
                        if (baseFaceIndex == masterFaceIndex) continue;
                        
                        for (int k=0; k<4; k++)
                        {
                            if (m_AdjQuadIndices[baseFaceIndex+k] == vertexIndices[0])
                            {
                                neighbourFound = true;
                                int slaveEdgeIndex = (k + 3) % 4;
                                
                                float2 slaveCoordinatesDelta =  m_TextureCoords[m_TriIndices[baseFaceIndex + slaveEdgeIndex][1]] - 
                                                                m_TextureCoords[m_TriIndices[baseFaceIndex + (slaveEdgeIndex+1)%4][1]];
                                float slaveCoordinatesLength =  D3DXVec2Length((D3DXVECTOR2*)&slaveCoordinatesDelta);

                                if (masterCoordinatesLength >= slaveCoordinatesLength)
                                {
                                    // Indices order is swapped for neighbouring patches
                                    meshQuadEdgeIndices[baseFaceIndex + slaveEdgeIndex][0] = m_QuadIndices[edge[1]][1];
                                    meshQuadEdgeIndices[baseFaceIndex + slaveEdgeIndex][1] = m_QuadIndices[edge[0]][1];
                                }
                                else
                                {
                                    edge[0] = m_QuadIndices[baseFaceIndex + k].vector[1];
                                    edge[1] = m_QuadIndices[baseFaceIndex + (k+1)%4].vector[1];

                                    meshQuadEdgeIndices[baseFaceIndex + slaveEdgeIndex][0] = edge[0];
                                    meshQuadEdgeIndices[baseFaceIndex + slaveEdgeIndex][1] = edge[1];

                                    meshQuadEdgeIndices[masterFaceIndex + i].vector[0] = edge[1];
                                    meshQuadEdgeIndices[masterFaceIndex + i].vector[1] = edge[0];
                                }

                                break;
                            }
                        }
                    }
                    else // Triangle patch neighbour
                    {
                        int _baseFaceIndex = baseFaceIndex - quadIndicesNum;
                        if (_baseFaceIndex == masterFaceIndex) continue;
                        
                        for(int k=0; k<3; k++)
                        {
                            if (m_AdjTriIndices[_baseFaceIndex+k] == vertexIndices[0])
                            {
                                neighbourFound = true;
                                int slaveEdgeIndex = (k + 2) % 3;

                                float2 slaveCoordinatesDelta =  m_TextureCoords[m_TriIndices[_baseFaceIndex + slaveEdgeIndex][1]] - 
                                                                m_TextureCoords[m_TriIndices[_baseFaceIndex + (slaveEdgeIndex+1)%3][1]];
                                float slaveCoordinatesLength =  D3DXVec2Length((D3DXVECTOR2*)&slaveCoordinatesDelta);

                                if (masterCoordinatesLength >= slaveCoordinatesLength)
                                {
                                    // Indices order is swapped for neighbouring patches
                                    meshTriEdgeIndices[_baseFaceIndex + slaveEdgeIndex][0] = m_QuadIndices[edge[1]][1];
                                    meshTriEdgeIndices[_baseFaceIndex + slaveEdgeIndex][1] = m_QuadIndices[edge[0]][1];
                                }
                                else
                                {
                                    edge[0] = m_TriIndices[_baseFaceIndex + k][1];
                                    edge[1] = m_TriIndices[_baseFaceIndex + (k+1)%3][1];

                                    meshTriEdgeIndices[_baseFaceIndex + slaveEdgeIndex][0] = edge[0];
                                    meshTriEdgeIndices[_baseFaceIndex + slaveEdgeIndex][1] = edge[1];

                                    meshQuadEdgeIndices[masterFaceIndex + i][0] = edge[1];
                                    meshQuadEdgeIndices[masterFaceIndex + i][1] = edge[0];
                                }

                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    for (int f=0; f<triIndicesNum; f+=3)
    {
        int masterFaceIndex = f;
        
        for (int i=0; i<3; i++)
        {
            int edge[2] = {f + i, f + (i+1)%3};
            int vertexIndices[2] = {m_AdjTriIndices[edge[0]], m_AdjTriIndices[edge[1]]};

            // Continue if the edge has been processed earlier
            if (meshTriEdgeIndices[masterFaceIndex + i].vector[0] != -1) continue;

            meshTriEdgeIndices[masterFaceIndex + i].vector[0] = m_TriIndices[edge[0]].vector[1];
            meshTriEdgeIndices[masterFaceIndex + i].vector[1] = m_TriIndices[edge[1]].vector[1];

            bool neighbourFound = false;
            int vertex = vertexIndices[0];
                            
            // Search for neighbour around #0 vertex
            for (int j=0; j<m_FacesCollected[vertex]; j++)
            {
                int baseFaceIndex = m_AdjacencyFaces[m_FacesPerVertex[vertex]-j-1];
                
                //if (baseFaceIndex != masterFaceIndex)
                {
                    if (baseFaceIndex < quadIndicesNum) // quad patch neighbour
                    {
                        if (baseFaceIndex == masterFaceIndex) continue;
                        
                        for (int k=0; k<4; k++)
                        {
                            if (m_AdjQuadIndices[baseFaceIndex+k] == vertexIndices[1])
                            {
                                neighbourFound = true;
                                int slaveEdgeIndex = k;
                                
                                // indices order is swapped for neighbouring patches
                                meshQuadEdgeIndices[baseFaceIndex + slaveEdgeIndex][0] = m_TriIndices[edge[1]][1];
                                meshQuadEdgeIndices[baseFaceIndex + slaveEdgeIndex][1] = m_TriIndices[edge[0]][1];
                            }
                        }
                    }
                    else // triangle patch neighbour
                    {
                        int _baseFaceIndex = baseFaceIndex - quadIndicesNum;
                        if (_baseFaceIndex == masterFaceIndex) continue;
                        
                        for (int k=0; k<3; k++)
                        {
                            if (m_AdjTriIndices[_baseFaceIndex+k] == vertexIndices[1])
                            {
                                neighbourFound = true;
                                int slaveEdgeIndex = k;
                                
                                // Indices order is swapped for neighbouring patches
                                meshTriEdgeIndices[_baseFaceIndex + slaveEdgeIndex][0] = m_TriIndices[edge[1]][1];
                                meshTriEdgeIndices[_baseFaceIndex + slaveEdgeIndex][1] = m_TriIndices[edge[0]][1];
                            }
                        }
                    }
                }
            }

            if (neighbourFound) continue;

            // Search for neighbour around #1 vertex
            vertex = vertexIndices[1];
            for (int j=0; j<m_FacesCollected[vertex]; j++)
            {
                int baseFaceIndex = m_AdjacencyFaces[m_FacesPerVertex[vertex]-j-1];
                
                //if (baseFaceIndex != masterFaceIndex)
                {
                    if (baseFaceIndex < quadIndicesNum) // Quad patch neighbour
                    {
                        if (baseFaceIndex == masterFaceIndex) continue;
                        
                        for (int k=0; k<4; k++)
                        {
                            if (m_AdjQuadIndices[baseFaceIndex+k] == vertexIndices[0])
                            {
                                neighbourFound = true;
                                int slaveEdgeIndex = (k + 3) % 4;
                                
                                // Indices order is swapped for neighbouring patches
                                meshQuadEdgeIndices[baseFaceIndex + slaveEdgeIndex][0] = m_TriIndices[edge[1]][1];
                                meshQuadEdgeIndices[baseFaceIndex + slaveEdgeIndex][1] = m_TriIndices[edge[0]][1];
                            }
                        }
                    }
                    else // Triangle patch neighbour
                    {
                        int _baseFaceIndex = baseFaceIndex - quadIndicesNum;
                        if (_baseFaceIndex == masterFaceIndex) continue;
                        
                        for(int k=0; k<3; k++)
                        {
                            if (m_TriIndices[_baseFaceIndex+k].vector[0] == vertexIndices[0])
                            {
                                neighbourFound = true;
                                int slaveEdgeIndex = (k + 2) % 3;
                                
                                // Indices order is swapped for neighbouring patches
                                meshTriEdgeIndices[_baseFaceIndex + slaveEdgeIndex][0] = m_TriIndices[edge[1]][1];
                                meshTriEdgeIndices[_baseFaceIndex + slaveEdgeIndex][1] = m_TriIndices[edge[0]][1];
                            }
                        }
                    }
                }
            }
        }
    }

    m_QuadEdgeCoordinates.resize(quadIndicesNum, float4(0,0,0,0));
    m_TriEdgeCoordinates.resize(triIndicesNum, float4(0,0,0,0));

    for (int f=0; f<quadIndicesNum; f++)
    {
        assert(meshQuadEdgeIndices[f].vector[0] != -1);
        
        if (meshQuadEdgeIndices[f].vector[0] != -1)
        {
            m_QuadEdgeCoordinates[f][0] = m_TextureCoords[meshQuadEdgeIndices[f][0]][0];
            m_QuadEdgeCoordinates[f][1] = m_TextureCoords[meshQuadEdgeIndices[f][0]][1];
            m_QuadEdgeCoordinates[f][2] = m_TextureCoords[meshQuadEdgeIndices[f][1]][0];
            m_QuadEdgeCoordinates[f][3] = m_TextureCoords[meshQuadEdgeIndices[f][1]][1];
        }
    }

    meshQuadEdgeIndices.clear();

    for (int f=0; f<triIndicesNum; f++)
    {
        assert(meshTriEdgeIndices[f].vector[0] != -1);
        
        if (meshTriEdgeIndices[f].vector[0] != -1)
        {
            m_TriEdgeCoordinates[f][0] = m_TextureCoords[meshTriEdgeIndices[f][0]][0];
            m_TriEdgeCoordinates[f][1] = m_TextureCoords[meshTriEdgeIndices[f][0]][1];
            m_TriEdgeCoordinates[f][2] = m_TextureCoords[meshTriEdgeIndices[f][1]][0];
            m_TriEdgeCoordinates[f][3] = m_TextureCoords[meshTriEdgeIndices[f][1]][1];
        }
    }

    meshTriEdgeIndices.clear();


    // Calculate mesh normals
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    if (m_Normals.empty())
    {
        m_Normals.resize(packedVerticesNum);

        std::vector<D3DXVECTOR3> normals;
        std::vector<float> areas;
#if 0
        // Build Catmull-Clark subdivision surface to calculate smooth normals
        for (int v=0; v<verticesNum; v++)
        {
	        int facesNum = m_FacesCollected[v];
	        D3DXVECTOR3 baseVertex = m_Vertices[v].vector;
	        std::vector<Boundary> boundaries;

	        for (int f=0; f<m_FacesCollected[v]; f++)
	        {
		        int baseFaceIndex = m_AdjacencyFaces[m_FacesPerVertex[v] - f - 1];
		        D3DXVECTOR3 vertices[4];
		        D3DXVECTOR3 centroidPoint(0, 0, 0);
		        int baseIndex;

                if (baseFaceIndex < m_QuadIndices.size()) // Quad patch
		        {
			        for (int i=0; i<4; i++)
			        {
				        vertices[i] = m_Vertices[m_QuadIndices[baseFaceIndex+i].vector[0]].vector;
				        centroidPoint += vertices[i];

				        if (vertices[i] == baseVertex) baseIndex = i;
			        }

			        centroidPoint /= 4;
			        int dummyIndex = (baseIndex + 2) % 4;
			        int edgeVerticesNum = 0;

			        Boundary boundary;
			        boundary.startPoint = vertices[(baseIndex + 1) % 4];
			        boundary.midPoint = centroidPoint;
			        boundary.endPoint = vertices[(baseIndex + 3) % 4];
			        boundaries.push_back(boundary);
		        }
		        else // Triangle patch
		        {
			        baseFaceIndex -= m_QuadIndices.size();

			        for (int i=0; i<3; i++)
			        {
                        vertices[i] = m_Vertices[m_TriIndices[baseFaceIndex+i].vector[0]].vector;
				        centroidPoint += vertices[i];

				        if(vertices[i] == baseVertex) baseIndex = i;
			        }

			        centroidPoint /= 3;
			        int edgeVerticesNum = 0;

			        Boundary boundary;
			        boundary.startPoint = vertices[(baseIndex + 1) % 3];
			        boundary.midPoint = centroidPoint;
			        boundary.endPoint = vertices[(baseIndex + 2) % 3];
			        boundaries.push_back(boundary);
		        }
	        }

	        std::vector<D3DXVECTOR3> boundaryVertices;
	        for (std::vector<Boundary>::iterator it = boundaries.begin(); it != boundaries.end(); ++it)
	        {
		        if (boundaryVertices.empty())
		        {
			        boundaryVertices.push_back(it->startPoint);
			        boundaryVertices.push_back(it->midPoint);
			        boundaryVertices.push_back(it->endPoint);
			        continue;
		        }

		        D3DXVECTOR3 endPoint = boundaryVertices[(int)boundaryVertices.size() - 1];

		        for (std::vector<Boundary>::iterator it2 = boundaries.begin(); it2 != boundaries.end(); ++it2)
		        {
			        if(it2->startPoint == endPoint)
			        {
				        boundaryVertices.push_back(it2->midPoint);

				        if(it2->endPoint != boundaryVertices[0])
					        boundaryVertices.push_back(it2->endPoint);

				        break;
			        }
		        }
	        }

	        int boundaryVerticesNum = (int)boundaryVertices.size();
	        D3DXVECTOR3 _baseVertex = baseVertex;

	        // Innner vertex
	        if (boundaryVerticesNum == m_FacesCollected[v] * 2)
	        {
		        int n = m_FacesCollected[v];              
		        const int iterationsNum = 3;

		        for (int i=0; i<iterationsNum; i++)
		        {
			        D3DXVECTOR3 F = D3DXVECTOR3(0,0,0);
			        for (int vertex=1; vertex<boundaryVerticesNum; vertex+=2) {
				        F += boundaryVertices[vertex];
			        }

			        F /= n;

			        D3DXVECTOR3 R = D3DXVECTOR3(0,0,0);
			        for (int vertex=0; vertex<boundaryVerticesNum; vertex+=2) {
				        R += (boundaryVertices[vertex] + _baseVertex) / 2;
			        }

			        R /= n;

			        _baseVertex = (F + 2 * R + (n-3) * baseVertex) / n;

			        //if ((i+1) < iterationsNum)
			        {
				        // Update "face" points
				        for (int vertex=1; vertex<boundaryVerticesNum; vertex+=2) {
					        boundaryVertices[vertex] = (boundaryVertices[vertex-1] + boundaryVertices[vertex] + boundaryVertices[(vertex+1) % boundaryVerticesNum] + _baseVertex) / 4;
				        }

				        // Update "edge" points
				        boundaryVertices[0] = (boundaryVertices[boundaryVerticesNum-1] + boundaryVertices[0] + boundaryVertices[1] + _baseVertex) / 4;

				        for (int vertex=2; vertex<boundaryVerticesNum; vertex+=2) {
					        boundaryVertices[vertex] = (boundaryVertices[vertex-1] + boundaryVertices[vertex] + boundaryVertices[vertex+1] + _baseVertex) / 4;
				        }
			        }
		        }

		        float totalArea = 0.0f;
		        areas.clear();
		        normals.clear();

		        for (int vertex=0; vertex<boundaryVerticesNum; vertex++)
		        {
			        D3DXVECTOR3 vertex0 = _baseVertex;
			        D3DXVECTOR3 vertex1 = boundaryVertices[vertex];
			        D3DXVECTOR3 vertex2 = boundaryVertices[(vertex+1) % boundaryVerticesNum];

			        D3DXVECTOR3 edge01 = vertex1 - vertex0;
			        D3DXVECTOR3 edge02 = vertex2 - vertex0;

			        D3DXVECTOR3 normal;
			        D3DXVec3Cross(&normal, &edge01, &edge02);
			        float area = D3DXVec3Length(&normal) * 0.5f;
			        areas.push_back(area);
			        totalArea += area;

			        D3DXVec3Normalize(&normal, &normal);
			        normals.push_back(normal);    
		        }

		        D3DXVECTOR3 normal(0,0,0);
		        for(int t=0; t<boundaryVerticesNum; t++)
			        normal += normals[t];

		        D3DXVec3Normalize(&normal, &normal);
		        memcpy(m_Normals[v].vector, &normal, sizeof(D3DXVECTOR3));
	        }
	        else
	        {
		        m_Normals[v] = float4(0,0,0,0);
	        }
        }
#else
        for (int i=0; i<packedVerticesNum; ++i)
        {
	        float totalArea = 0.0f;
	        areas.clear();
	        normals.clear();

	        for (int t=0; t<m_FacesCollected[i]; t++)
	        {
		        int baseFaceIndex = m_AdjacencyFaces[m_FacesPerVertex[i] - t - 1];

		        if (baseFaceIndex < quadIndicesNum)
		        { // quad patch
			        D3DXVECTOR3 vertex[4];
                    vertex[0] = m_Vertices[m_AdjQuadIndices[baseFaceIndex+0]].vector;
			        vertex[1] = m_Vertices[m_AdjQuadIndices[baseFaceIndex+1]].vector;
			        vertex[2] = m_Vertices[m_AdjQuadIndices[baseFaceIndex+2]].vector;
			        vertex[3] = m_Vertices[m_AdjQuadIndices[baseFaceIndex+3]].vector;
			        D3DXVECTOR3 center = (vertex[0] + vertex[1] + vertex[2] + vertex[3]) / 4;
			        D3DXVECTOR3 normal(0, 0, 0);
			        float area = 0;
			        for (int i = 0; i < 4; ++i)
			        {
				        D3DXVECTOR3 edge01 = vertex[i] - center;
				        D3DXVECTOR3 edge02 = vertex[(i + 1) & 3] - center;
				        D3DXVECTOR3 tmpNormal;
				        D3DXVec3Cross(&tmpNormal, &edge01, &edge02);
				        float tmpArea = D3DXVec3Length(&tmpNormal) * 0.5f;
				        area += tmpArea;
				        normal += tmpNormal;
			        }
			        areas.push_back(area);
			        totalArea += area;
			        D3DXVec3Normalize(&normal, &normal);
			        normals.push_back(normal);
		        }
		        else
		        { // triangle patch
			        baseFaceIndex -= quadIndicesNum;
			        D3DXVECTOR3 vertex0(m_Vertices[m_AdjTriIndices[baseFaceIndex+0]].vector);
			        D3DXVECTOR3 vertex1(m_Vertices[m_AdjTriIndices[baseFaceIndex+1]].vector);
			        D3DXVECTOR3 vertex2(m_Vertices[m_AdjTriIndices[baseFaceIndex+2]].vector);

			        D3DXVECTOR3 edge01 = vertex1 - vertex0;
			        D3DXVECTOR3 edge02 = vertex2 - vertex0;

			        D3DXVECTOR3 normal;
			        D3DXVec3Cross(&normal, &edge01, &edge02);
			        float area = D3DXVec3Length(&normal) * 0.5f;
			        areas.push_back(area);
			        totalArea += area;

			        D3DXVec3Normalize(&normal, &normal);
			        normals.push_back(normal);
		        }
	        }

	        D3DXVECTOR3 normal(0.0f, 0.0f, 0.0f);
    #if 1
	        float rArea = totalArea > 0.00001f ? 1.0f / totalArea : 1.0f;

	        for(int t=0; t<m_FacesCollected[i]; t++)
		        normal += normals[t] * areas[t] * rArea;
    #else
	        for(int t=0; t<m_FacesCollected[i]; t++)
		        normal += normals[t];
    #endif
	        D3DXVec3Normalize(&normal, &normal);
	        memcpy(m_Normals[i].vector, &normal, sizeof(D3DXVECTOR3));
        }
#endif
    }

    //if (m_Vertices.size() != m_TextureCoords.size())
    {
        int trianglesNum = (int)m_TriIndices.size() / 3;
        int quadsNum = (int)m_QuadIndices.size() / 4;
        
        // Arrays to store summetry flags
        std::vector<int> triangleFlags(trianglesNum, 1);
        std::vector<int> quadFlags(quadsNum, 1);

        for (int i=0; i<trianglesNum; ++i)
        {
            int baseFaceIndex = i*3;
            D3DXVECTOR2 texCoord[3];
            
            for (int j=0; j<3; ++j)
            {
                texCoord[j] = m_TextureCoords[m_TriIndices[baseFaceIndex+j].vector[1]].vector;
            }

            D3DXVECTOR2 t01 = texCoord[1] - texCoord[0];
            D3DXVECTOR2 t02 = texCoord[2] - texCoord[0];
            triangleFlags[i] = t01.x * t02.y - t01.y * t02.x > 0.0f ? 1 : -1;
        }

        for (int i=0; i<quadsNum; ++i)
        {
            int baseFaceIndex = i*4;
            D3DXVECTOR2 texCoord[3];
            
            for (int j=0; j<3; ++j)
            {
                texCoord[j] = m_TextureCoords[m_QuadIndices[baseFaceIndex+j].vector[1]].vector;
            }

            D3DXVECTOR2 t01 = texCoord[1] - texCoord[0];
            D3DXVECTOR2 t02 = texCoord[2] - texCoord[0];
            quadFlags[i] = t01.x * t02.y - t01.y * t02.x > 0.0f ? 1 : -1;
        }
                
        // Create new array to store texture coordinates
        std::vector<float2> textureCoords(verticesNum, float2(0, 0));

        int maxVertexExpansion = (int)m_AdjacencyFaces.size();
        std::vector<int> vertexRemapping(maxVertexExpansion, -1);
        std::vector<int> vertexFlags(maxVertexExpansion, 1);

        // Duplicate all vertices which have different texture coordinates
        for (int i=0; i<verticesNum; ++i)
        {
            for (int j=0; j<m_FacesCollected[i]; ++j)
		    {
		        int baseFaceIndex = m_AdjacencyFaces[m_FacesPerVertex[i] - j - 1];
                int vertexIndex = 0;

                if (baseFaceIndex < quadIndicesNum)
                { // quad patch
                    for (int l=0; l<4; ++l) {
                        if (m_QuadIndices[baseFaceIndex+l].vector[0] == i) {
                            vertexIndex = l;
                            break;
                        }
                    }
                }
	            else
	            { // triangle patch
                    for (int l=0; l<3; ++l) {
                        if (m_TriIndices[baseFaceIndex-quadIndicesNum+l].vector[0] == i) {
                            vertexIndex = l;
                            break;
                        }
                    }
                }

                for (int k=0; k<m_FacesCollected[i]; ++k)
                {
                    if (vertexRemapping[m_FacesPerVertex[i] - k - 1] == -1)
                    { // add new vertex if needed
                        int newIndex = i;
                        int flag = 1;
                        
                        if (baseFaceIndex < quadIndicesNum)
                        { // quad patch

                            if (k == 0)
                            {
                                textureCoords[i] = m_TextureCoords[m_QuadIndices[baseFaceIndex+vertexIndex].vector[1]];
                            }
                            else
                            {
                                newIndex = (int)m_Vertices.size();
                                
                                m_Vertices.push_back(m_Vertices[m_QuadIndices[baseFaceIndex+vertexIndex][0]]);
                                m_Normals.push_back(m_Normals[m_QuadIndices[baseFaceIndex+vertexIndex][0]]);
                                m_CornerCoordinates.push_back(m_CornerCoordinates[m_QuadIndices[baseFaceIndex+vertexIndex][0]]);
                                textureCoords.push_back(m_TextureCoords[m_QuadIndices[baseFaceIndex+vertexIndex][1]]);

                                if (m_pCharacterAnimation)
                                {
                                    m_BoneWeights.bones.push_back(m_BoneWeights.bones[m_QuadIndices[baseFaceIndex+vertexIndex][0]]);
                                    m_BoneWeights.weights.push_back(m_BoneWeights.weights[m_QuadIndices[baseFaceIndex+vertexIndex][0]]);
                                }
                                
                                m_QuadIndices[baseFaceIndex+vertexIndex][0] = newIndex;
                            }

                            flag = quadFlags[baseFaceIndex / 4];
                        }
			            else
			            { // triangle patch
			                int _baseFaceIndex = baseFaceIndex - quadIndicesNum;

                            if (k == 0)
                            {
                                textureCoords[i] = m_TextureCoords[m_TriIndices[_baseFaceIndex+vertexIndex].vector[1]];
                            }
                            else
                            {
                                newIndex = (int)m_Vertices.size();
                                
                                m_Vertices.push_back(m_Vertices[m_TriIndices[_baseFaceIndex+vertexIndex][0]]);
                                m_Normals.push_back(m_Normals[m_TriIndices[_baseFaceIndex+vertexIndex][0]]);
                                m_CornerCoordinates.push_back(m_CornerCoordinates[m_TriIndices[_baseFaceIndex+vertexIndex][0]]);
                                textureCoords.push_back(m_TextureCoords[m_TriIndices[_baseFaceIndex+vertexIndex][1]]);

                                if (m_pCharacterAnimation)
                                {
                                    m_BoneWeights.bones.push_back(m_BoneWeights.bones[m_TriIndices[_baseFaceIndex+vertexIndex][0]]);
                                    m_BoneWeights.weights.push_back(m_BoneWeights.weights[m_TriIndices[_baseFaceIndex+vertexIndex][0]]);
                                }

                                m_TriIndices[_baseFaceIndex+vertexIndex][0] = newIndex;
                            }

                            flag = triangleFlags[_baseFaceIndex / 3];
                        }

                        vertexRemapping[m_FacesPerVertex[i] - k - 1] = newIndex;
                        vertexFlags[m_FacesPerVertex[i] - k - 1] = flag;
                        
                        break;
                    }
                    else
                    { // check existing vertices
                        int index = vertexRemapping[m_FacesPerVertex[i] - k - 1];
                        int flag = vertexFlags[m_FacesPerVertex[i] - k - 1];
                        
                        if (baseFaceIndex < quadIndicesNum)
		                { // quad patch
                            if (quadFlags[baseFaceIndex / 4] == flag && m_TextureCoords[m_QuadIndices[baseFaceIndex+vertexIndex][1]] == textureCoords[index])
                            {
                                m_QuadIndices[baseFaceIndex+vertexIndex][0] = index;
                                break;
                            }
                        }
			            else
			            { // triangle patch
			                int _baseFaceIndex = baseFaceIndex - quadIndicesNum;

                            if (triangleFlags[_baseFaceIndex / 3] == flag && m_TextureCoords[m_TriIndices[_baseFaceIndex+vertexIndex][1]] == textureCoords[index])
                            {
                                m_TriIndices[_baseFaceIndex+vertexIndex][0] = index;
                                break;
                            }
                        }
                    }
                }
            }
        }

        m_TextureCoords.resize(textureCoords.size());
        std::copy(textureCoords.begin(), textureCoords.end(), m_TextureCoords.begin());
        textureCoords.clear();

		GenerateUnpackedPosIndices();
        GenerateAdjacencyInfo();
        verticesNum = (int)m_Vertices.size();
    }
    
    // Calculate mesh tangents
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    if (m_Tangents.empty())
    {
        m_Tangents.resize(verticesNum);
        
        std::vector<D3DXVECTOR4> tangents;

        for (int i=0; i<verticesNum; i++)
        {
            for (int t=0; t<m_FacesCollected[i]; t++)
		    {
			    int baseFaceIndex = m_AdjacencyFaces[m_FacesPerVertex[i] - t - 1];

			    if (baseFaceIndex < quadIndicesNum)
			    { // quad patch
				    D3DXVECTOR3 vertex[4];
                    D3DXVECTOR2 texCoord[4];

                    for (int j=0; j<4; ++j)
                    {
				        vertex[j] = m_Vertices[m_QuadIndices[baseFaceIndex+j].vector[0]].vector;
                        texCoord[j] = m_TextureCoords[m_QuadIndices[baseFaceIndex+j].vector[0]].vector;
                    }

                    int index0 = 0;
                    
                    for (int j=0; j<4; ++j)
                    {
                        if (vertex[j] == (D3DXVECTOR3&)m_Vertices[i])
                        {
                            index0 = j;
                            break;
                        }
                    }

                    int index1 = (index0 + 1) % 4;
                    int index2 = (index0 + 2) % 4;

                    D3DXVECTOR4 tangent;
                    GetTangentBasis(tangent, vertex[index0], vertex[index1], vertex[index2], texCoord[index0], texCoord[index1], texCoord[index2]);
				    tangents.push_back(tangent);

                    index1 = (index0 + 2) % 4;
                    index2 = (index0 + 3) % 4;

                    GetTangentBasis(tangent, vertex[index0], vertex[index1], vertex[index2], texCoord[index0], texCoord[index1], texCoord[index2]);
				    tangents.push_back(tangent);
			    }
			    else
			    { // triangle patch
				    baseFaceIndex -= quadIndicesNum;
                    D3DXVECTOR3 vertex[3];
                    D3DXVECTOR2 texCoord[3];
                    
                    for (int j=0; j<3; ++j)
                    {
                        vertex[j] = m_Vertices[m_TriIndices[baseFaceIndex+j].vector[0]].vector;
                        texCoord[j] = m_TextureCoords[m_TriIndices[baseFaceIndex+j].vector[0]].vector;
                    }
                    
                    D3DXVECTOR4 tangent;
                    GetTangentBasis(tangent, vertex[0], vertex[1], vertex[2], texCoord[0], texCoord[1], texCoord[2]);
				    tangents.push_back(tangent);
			    }
            }

            if (tangents.empty())
            {
                m_Tangents[i] = (float4&)D3DXVECTOR4(0, 0, 0, 0);
            }
            else
            {
                D3DXVECTOR4 tangent(0, 0, 0, 0);
                D3DXVECTOR4 bitangent(0, 0, 0, 0);
                int tangentsNum = (int)tangents.size();
                
                for (int j=0; j<tangentsNum; ++j) {
                    tangent += tangents[j];
                }

                float normalProjection = D3DXVec3Dot((D3DXVECTOR3*)&tangent, (D3DXVECTOR3*)m_Normals[i].vector);
                (D3DXVECTOR3&)tangent -= normalProjection * (D3DXVECTOR3&)m_Normals[i];
                D3DXVec3Normalize((D3DXVECTOR3*)&tangent, (D3DXVECTOR3*)&tangent);
                memcpy(m_Tangents[i].vector, &tangent, sizeof(D3DXVECTOR4));

                tangents.clear();
            }
        }
    }
}

void MyMesh::ReleaseMeshBuffers(void)
{
    m_Vertices.clear();
    m_TextureCoords.clear();
    m_TriIndices.clear();
    m_QuadIndices.clear();

    m_Normals.clear();
    m_Tangents.clear();

    m_CornerCoordinates.clear();
    m_QuadEdgeCoordinates.clear();
    m_TriEdgeCoordinates.clear();

    m_FacesPerVertex.clear();
    m_FacesCollected.clear();
    m_AdjacencyFaces.clear();
}

MyMesh::MyMesh(void)
{
	m_pDisplacement = NULL;

    g_pMeshVertexBuffer = NULL;
    g_pMeshVertexBufferSRV = NULL;
    g_pMeshCoordinatesBuffer = NULL;
    g_pMeshCoordinatesBufferSRV = NULL;
    g_pMeshCornerCoordinatesBuffer = NULL;
    g_pMeshCornerCoordinatesBufferSRV = NULL;
    g_pMeshQuadEdgeCoordinatesBuffer = NULL;
    g_pMeshQuadEdgeCoordinatesBufferSRV = NULL;
    g_pMeshTriEdgeCoordinatesBuffer = NULL;
    g_pMeshTriEdgeCoordinatesBufferSRV = NULL;
    g_pMeshQuadsIndexBuffer = NULL;
    g_pMeshQuadsIndexBufferSRV = NULL;
    g_pMeshTrgsIndexBuffer = NULL;
    g_pMeshTrgsIndexBufferSRV = NULL;
    g_pMeshNormalsBuffer = NULL;
    g_pMeshNormalsBufferSRV = NULL;
    g_pMeshTangentsBuffer = NULL;
    g_pMeshTangentsBufferSRV = NULL;
    g_pNormalMapSRV = NULL;
    g_pPositionMapSRV = NULL;
    g_pDisplacementMapSRV = NULL;

    m_pDynamicIndexBuffer       = NULL;
    m_pDynamicIndexBufferUAV    = NULL;
    m_pDynamicIndexBufferSRV    = NULL;
    m_pDynamicIndexArgBuffer    = NULL;

    m_pBoneIndicesBuffer        = NULL;
    m_pBoneIndicesBufferSRV     = NULL;
    m_pBoneWeightsBuffer        = NULL;
    m_pBoneWeightsBufferSRV     = NULL;

	m_TessellationFactor        = 1.0f;
    m_DisplacementScale         = 1.0f;

    m_pCharacterAnimation       = NULL;
}

MyMesh::~MyMesh()
{
	assert(_CrtCheckMemory());
    SAFE_DELETE_ARRAY(m_pDisplacement);

 	SAFE_RELEASE(g_pMeshVertexBuffer);
	SAFE_RELEASE(g_pMeshVertexBufferSRV);
	SAFE_RELEASE(g_pMeshCoordinatesBuffer);
	SAFE_RELEASE(g_pMeshCoordinatesBufferSRV);
	SAFE_RELEASE(g_pMeshCornerCoordinatesBuffer);
	SAFE_RELEASE(g_pMeshCornerCoordinatesBufferSRV);
    SAFE_RELEASE(g_pMeshQuadEdgeCoordinatesBuffer);
    SAFE_RELEASE(g_pMeshQuadEdgeCoordinatesBufferSRV);
    SAFE_RELEASE(g_pMeshTriEdgeCoordinatesBuffer);
    SAFE_RELEASE(g_pMeshTriEdgeCoordinatesBufferSRV);
	SAFE_RELEASE(g_pMeshQuadsIndexBuffer);
	SAFE_RELEASE(g_pMeshQuadsIndexBufferSRV);
	SAFE_RELEASE(g_pMeshTrgsIndexBuffer);
	SAFE_RELEASE(g_pMeshTrgsIndexBufferSRV);
	SAFE_RELEASE(g_pMeshNormalsBuffer);
	SAFE_RELEASE(g_pMeshNormalsBufferSRV);
    SAFE_RELEASE(g_pMeshTangentsBuffer);
	SAFE_RELEASE(g_pMeshTangentsBufferSRV);

	SAFE_RELEASE(g_pNormalMapSRV);
	SAFE_RELEASE(g_pPositionMapSRV);
	SAFE_RELEASE(g_pDisplacementMapSRV);

    SAFE_RELEASE(m_pBoneIndicesBuffer);
    SAFE_RELEASE(m_pBoneIndicesBufferSRV);
    SAFE_RELEASE(m_pBoneWeightsBuffer);
    SAFE_RELEASE(m_pBoneWeightsBufferSRV);

    SAFE_RELEASE(m_pDynamicIndexBuffer);
    SAFE_RELEASE(m_pDynamicIndexBufferUAV);
    SAFE_RELEASE(m_pDynamicIndexBufferSRV);
    SAFE_RELEASE(m_pDynamicIndexArgBuffer);

    ReleaseAnimation();
}

float MyMesh::GetRelativeScale(MyMesh* pRefMesh)
{
    ID3D11ShaderResourceView* pPositionMapSRV = NULL;
    ID3D11ShaderResourceView* pRefPositionMapSRV = NULL;
    WSNormalMap WSNormalMap(int2(1024, 1024), DXGI_FORMAT_R32G32B32A32_FLOAT);

    WSNormalMap.GenerateNormalMapAndPositionMap_GPU(this, NULL, &pPositionMapSRV);
    WSNormalMap.GenerateNormalMapAndPositionMap_GPU(pRefMesh, NULL, &pRefPositionMapSRV);

	ID3D11DeviceContext *pContext;
	m_pDevice->GetImmediateContext(&pContext);
	pContext->Release();
    
    ID3D11Texture2D *pStagingPositionMap = NULL;
    ID3D11Texture2D *pStagingRefPositionMap = NULL;
    D3D11_MAPPED_SUBRESOURCE mappedSubresource;

    ID3D11Texture2D *pPositionMap = NULL;
    pPositionMapSRV->GetResource((ID3D11Resource **)&pPositionMap);
	pPositionMap->Release();
	D3D11_TEXTURE2D_DESC desc;
    pPositionMap->GetDesc(&desc);
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;
	desc.Usage = D3D11_USAGE_STAGING;
    m_pDevice->CreateTexture2D(&desc, NULL, &pStagingPositionMap);
	pContext->CopyResource(pStagingPositionMap, pPositionMap);
    pContext->Map(pStagingPositionMap, 0, D3D11_MAP_READ, 0, &mappedSubresource);
	float4* pPositions = (float4*)mappedSubresource.pData;

    SAFE_RELEASE(pPositionMapSRV);

    D3D11_TEXTURE2D_DESC descRef;
    pRefPositionMapSRV->GetResource((ID3D11Resource **)&pPositionMap);
	pPositionMap->Release();
	pPositionMap->GetDesc(&descRef);
	descRef.BindFlags = 0;
	descRef.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	descRef.MiscFlags = 0;
	descRef.Usage = D3D11_USAGE_STAGING;
    m_pDevice->CreateTexture2D(&descRef, NULL, &pStagingRefPositionMap);
	pContext->CopyResource(pStagingRefPositionMap, pPositionMap);
    pContext->Map(pStagingRefPositionMap, 0, D3D11_MAP_READ, 0, &mappedSubresource);
	float4* pRefPositions = (float4*)mappedSubresource.pData;

    SAFE_RELEASE(pRefPositionMapSRV);

    std::vector<float> scales;
    
    if (desc.Width == descRef.Width && desc.Height == descRef.Height)
    {
        for (unsigned iy = 0; iy < desc.Height-8; iy+=8)
		{
			for (unsigned ix = 0; ix < desc.Width-8; ix+=8)
			{
                D3DXVECTOR4 points[3];
                points[0] = (D3DXVECTOR4&)pPositions[iy * desc.Width + ix];
                points[1] = (D3DXVECTOR4&)pPositions[iy * desc.Width + ix + 8];
                points[2] = (D3DXVECTOR4&)pPositions[(iy + 8) * desc.Width + ix];

                D3DXVECTOR4 refPoints[3];
                refPoints[0] = (D3DXVECTOR4&)pRefPositions[iy * desc.Width + ix];
                refPoints[1] = (D3DXVECTOR4&)pRefPositions[iy * desc.Width + ix + 8];
                refPoints[2] = (D3DXVECTOR4&)pRefPositions[(iy + 8) * desc.Width + ix];

                // Only count faces with texture complexity of 1
                if (points[0].w == 1.0f && refPoints[0].w == 1.0f)
                {
                    if (points[1].w == 1.0f && refPoints[1].w == 1.0f)
                    {
                        D3DXVECTOR3 direction = (D3DXVECTOR3&)points[1] - (D3DXVECTOR3&)points[0];
                        D3DXVECTOR3 refDirection = (D3DXVECTOR3&)refPoints[1] - (D3DXVECTOR3&)refPoints[0];
                        
                        scales.push_back(D3DXVec3Length(&direction) / D3DXVec3Length(&refDirection));
                    }
                
                    if (points[2].w == 1.0f && refPoints[2].w == 1.0f)
                    {
                        D3DXVECTOR3 direction = (D3DXVECTOR3&)points[2] - (D3DXVECTOR3&)points[0];
                        D3DXVECTOR3 refDirection = (D3DXVECTOR3&)refPoints[2] - (D3DXVECTOR3&)refPoints[0];
                        
                        scales.push_back(D3DXVec3Length(&direction) / D3DXVec3Length(&refDirection));
                    }
                }
            }
        }
    }

    pContext->Unmap(pStagingPositionMap, 0);
    SAFE_RELEASE(pStagingPositionMap);

    pContext->Unmap(pStagingRefPositionMap, 0);
    SAFE_RELEASE(pStagingRefPositionMap);

    if (scales.empty()) return 0.0f;
    
    int scalesNum = (int)scales.size();
    double averageScale = 0;

    for (int i=0; i<scalesNum; ++i)
    {
        averageScale += (double)scales[i] / scalesNum;
    }

    return (float)averageScale;
}

void MyMesh::SetNormalMap(ID3D11ShaderResourceView* pShaderResourceView)
{
	SAFE_RELEASE(g_pNormalMapSRV);
	g_pNormalMapSRV = pShaderResourceView;
	g_pNormalMapSRV->AddRef();

    ID3D11Texture2D* pResource = NULL;
    pShaderResourceView->GetResource((ID3D11Resource**)&pResource);
    D3D11_TEXTURE2D_DESC desc;
    pResource->GetDesc(&desc);
    pResource->Release();

    m_MapDimensions[0] = desc.Width;
    m_MapDimensions[1] = desc.Height;
}

void MyMesh::SetDisplacementMap(ID3D11ShaderResourceView* pShaderResourceView, float scale)
{
	SAFE_RELEASE(g_pDisplacementMapSRV);
	g_pDisplacementMapSRV = pShaderResourceView;
	g_pDisplacementMapSRV->AddRef();

    m_DisplacementScale = scale;

    ID3D11Texture2D* pResource = NULL;
    pShaderResourceView->GetResource((ID3D11Resource**)&pResource);
    D3D11_TEXTURE2D_DESC desc;
    pResource->GetDesc(&desc);
    pResource->Release();

    m_MapDimensions[0] = desc.Width;
    m_MapDimensions[1] = desc.Height;
}

void MyMesh::ReleaseAnimation(void)
{
    SAFE_DELETE(m_pCharacterAnimation);
}