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

#include <d3dx10math.h>
#include <vector>
#include "mymath/mymath.h"
#include "meshloader/bone_weights.h"

struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3DX11Effect;
class CDXUTSDKMesh;
class biTracer;
class CharacterAnimation;
struct ID3D11UnorderedAccessView;

enum eRenderModes
{
    RENDER_DEFAULT,
    RENDER_NORMAL_MAP,
    RENDER_DISPLACEMENT_MAP,
    RENDER_TEXTURE_COORDINATES,
    RENDER_CHECKER_BOARD,
};

struct MESH_ANIMATION_CONSTANTS
{
    D3DXMATRIX frames[128];
};

struct RT_MESH_INTERFACE;
#include "mymath/mymath_ext.h"

struct MyMesh
{
	D3DXFLOAT16 *m_pDisplacement;

    static ID3D11Device*        m_pDevice;
    static ID3DX11Effect*       g_pEffect;

    static float g_TessellationFactor;
	static double g_TessellationFactorResScale;
    static bool g_UseTessellation;
    static bool g_RenderNormals;
	static bool g_bFixTheSeams;
    static bool g_bEnableAnimation;
    static bool g_bUpdateCamera;
    static bool s_EffectDirty;
    static bool g_RenderWireframe;
    static void Init(ID3D11Device *pd3dDevice);
    static void LoadEffect();
    static void Destroy();
    void RenderBasis(ID3D11DeviceContext *pd3dImmediateContext, D3DXMATRIX mViewProj, bool renderAnimated = false);
    void Render(ID3D11DeviceContext *pd3dImmediateContext, D3DXMATRIX& mView, D3DXMATRIX& mProj, D3DXVECTOR3& cameraPos);
    void RenderAnimated(ID3D11DeviceContext *pd3dContext, D3DXMATRIX& mView, D3DXMATRIX& mProj, D3DXVECTOR3& cameraPos, float time);
    void UpdateView(D3DXMATRIX& mView, D3DXMATRIX& mProj, D3DXVECTOR3& cameraPos);

    static eRenderModes s_RenderMode;
    static bool SetRenderMode(eRenderModes mode) { bool changed = s_RenderMode != mode; s_RenderMode = mode; return changed; }
    static eRenderModes GetRenderMode(void) { return s_RenderMode; }
    void CreateTracer();

    enum
    {
        NORMAL_MAP = 1,
        POSITIONS_MAP = 2
    };

    struct Boundary
    {
        D3DXVECTOR3 startPoint;
        D3DXVECTOR3 midPoint;
        D3DXVECTOR3 endPoint;
    };

    void GenerateDisplacementTiled(MyMesh* pReferenceMesh, int2 map_size, int tile_size = 256, int ss_scale = 1);

    void SetNormalMap(ID3D11ShaderResourceView* pShaderResourceView);
    void SetDisplacementMap(ID3D11ShaderResourceView* pShaderResourceView, float scale = 1.0f);
    float GetRelativeScale(MyMesh* pRefMesh);

public:
    HRESULT LoadMeshFromFile(WCHAR* fileName);

    int2 m_MapDimensions;

private:
	void GenerateRTInterface();
	void InitIndicesFromRTInterface();
    std::vector<float4> m_Vertices;
    std::vector<float2> m_TextureCoords;
    std::vector<int2>   m_TriIndices;
    std::vector<int2>   m_QuadIndices;

    std::vector<float4> m_Normals;
    std::vector<float4> m_Tangents;

    std::vector<float2> m_CornerCoordinates;
    std::vector<float4> m_QuadEdgeCoordinates;
    std::vector<float4> m_TriEdgeCoordinates;

    inline int NPolygons()
	{
		return m_QuadIndices.size() / 4 + m_TriIndices.size() / 3;
	}
	int GeneratePackedPosIndices(bool updatePositions = false); // fill m_Adj arrays with indices for packed positions, return a number of packed vertices
	inline void GetPolyVertIndicesFromBaseIndex(int iBaseInd, int &nVerts, int4 &Indices)
	{
		if (iBaseInd < m_AdjQuadIndices.size())
		{
			nVerts = 4;
			Indices[0] = m_AdjQuadIndices[iBaseInd];
			Indices[1] = m_AdjQuadIndices[iBaseInd + 1];
			Indices[2] = m_AdjQuadIndices[iBaseInd + 2];
			Indices[3] = m_AdjQuadIndices[iBaseInd + 3];
		}
		else
		{
			iBaseInd -= m_AdjQuadIndices.size();
			nVerts = 3;
			Indices[0] = m_AdjTriIndices[iBaseInd];
			Indices[1] = m_AdjTriIndices[iBaseInd + 1];
			Indices[2] = m_AdjTriIndices[iBaseInd + 2];
		}
	}
	inline void GetPolyVertIndicesFromPolyIndex(int iPolyInd, int &nVerts, int4 &Indices)
	{
		if (iPolyInd * 4 < m_AdjQuadIndices.size())
		{
			int iBaseInd = iPolyInd * 4;
			nVerts = 4;
			Indices[0] = m_AdjQuadIndices[iBaseInd];
			Indices[1] = m_AdjQuadIndices[iBaseInd + 1];
			Indices[2] = m_AdjQuadIndices[iBaseInd + 2];
			Indices[3] = m_AdjQuadIndices[iBaseInd + 3];
		}
		else
		{
			int iBaseInd = (iPolyInd - m_AdjQuadIndices.size() / 4) * 3;
			nVerts = 3;
			Indices[0] = m_AdjTriIndices[iBaseInd];
			Indices[1] = m_AdjTriIndices[iBaseInd + 1];
			Indices[2] = m_AdjTriIndices[iBaseInd + 2];
		}
	}
	void GenerateUnpackedPosIndices(); // fill m_Adj arrays with indices for unpacked positions
	std::vector<int> m_AdjTriIndices; // triangle indices which we use to build adjacency information
	std::vector<int> m_AdjQuadIndices; // quad indices which we use to build adjacency information

    void GenerateAdjacencyInfo(void);
	bool HaveCommonVerts(int iPoly1, int iPoly2);
	inline int nPolysAdjacentToVertex(int iVertex)
	{
		return m_FacesCollected[iVertex];
	}
	inline int iPolyBaseIndAdjacentToVertex(int iVertex, int index)
	{
		return m_AdjacencyFaces[m_FacesPerVertex[iVertex] - index - 1];
	}
    std::vector<int> m_FacesPerVertex;
    std::vector<int> m_FacesCollected;
    std::vector<int> m_AdjacencyFaces;

    // Animation stuff
    CharacterAnimation* m_pCharacterAnimation;
    BoneWeights         m_BoneWeights;
    
    HRESULT LoadMeshFromOBJ(WCHAR* path);
    HRESULT LoadMeshFromX(WCHAR* path);
    void ProcessMeshBuffers(void);
    void ReleaseMeshBuffers(void);
    void CreateBufferResources(void);
	void CreateGroupsBuffer(void);
    
    void ReleaseAnimation(void);
public:

    // Load mesh from file
    MyMesh();
    ~MyMesh();

    ID3D11Buffer*               g_pMeshVertexBuffer;
    ID3D11ShaderResourceView*   g_pMeshVertexBufferSRV;
    ID3D11Buffer*               g_pMeshCoordinatesBuffer;
    ID3D11ShaderResourceView*   g_pMeshCoordinatesBufferSRV;
    ID3D11Buffer*               g_pMeshQuadsIndexBuffer;
    ID3D11ShaderResourceView*   g_pMeshQuadsIndexBufferSRV;
    ID3D11Buffer*               g_pMeshTrgsIndexBuffer;
    ID3D11ShaderResourceView*   g_pMeshTrgsIndexBufferSRV;
    ID3D11Buffer*               g_pMeshNormalsBuffer;
    ID3D11ShaderResourceView*   g_pMeshNormalsBufferSRV;
    ID3D11Buffer*               g_pMeshTangentsBuffer;
    ID3D11ShaderResourceView*   g_pMeshTangentsBufferSRV;

    ID3D11Buffer*               g_pMeshCornerCoordinatesBuffer;
    ID3D11ShaderResourceView*   g_pMeshCornerCoordinatesBufferSRV;
    ID3D11Buffer*               g_pMeshQuadEdgeCoordinatesBuffer;
    ID3D11ShaderResourceView*   g_pMeshQuadEdgeCoordinatesBufferSRV;
    ID3D11Buffer*               g_pMeshTriEdgeCoordinatesBuffer;
    ID3D11ShaderResourceView*   g_pMeshTriEdgeCoordinatesBufferSRV;

    ID3D11ShaderResourceView*   g_pPositionMapSRV;
    ID3D11ShaderResourceView*   g_pNormalMapSRV;
    ID3D11ShaderResourceView*   g_pDisplacementMapSRV;

    ID3D11Buffer*               m_pDynamicIndexBuffer;
    ID3D11UnorderedAccessView*  m_pDynamicIndexBufferUAV;
    ID3D11ShaderResourceView*   m_pDynamicIndexBufferSRV;
    ID3D11Buffer*               m_pDynamicIndexArgBuffer;

    ID3D11Buffer*               m_pBoneIndicesBuffer;
    ID3D11ShaderResourceView*   m_pBoneIndicesBufferSRV;
    ID3D11Buffer*               m_pBoneWeightsBuffer;
    ID3D11ShaderResourceView*   m_pBoneWeightsBufferSRV;

    unsigned    g_MeshVerticesNum;
    unsigned    g_MeshQuadIndicesNum;
    unsigned    g_MeshTrgIndicesNum;
    unsigned    g_MeshMemorySize;

    float       m_DisplacementScale;
    float       m_TessellationFactor;
};
