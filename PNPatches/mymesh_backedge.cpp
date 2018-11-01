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

#include "mymesh.h"
#include <assert.h>

#include "mymath/tsort.hpp"
int MyMesh::GeneratePackedPosIndices(bool updatePositions)
{
    SetProgressString("Generating packed positions buffer...");
	SetProgress(0);
	static float4 *s_pVerts;
	struct POS_SORT
	{
		inline bool operator <(const POS_SORT &v) const
		{
			float4 &pVert0 = s_pVerts[index];
			float4 &pVert1 = s_pVerts[v.index];
			for (int i = 0; i < 3; ++i)
			{
				if (pVert0[i] < pVert1[i])
					return true;
				if (pVert1[i] < pVert0[i])
					return false;
			}
			return false;
		}
		int index;
	};
	std::vector<int> iSortedVerts;
	iSortedVerts.resize(m_Vertices.size());
	for (int i = 0; i < iSortedVerts.size(); ++i)
	{
		iSortedVerts[i] = i;
	}
	s_pVerts = &m_Vertices[0];
	TSort<POS_SORT>::QSort((POS_SORT *)&iSortedVerts[0], iSortedVerts.size());
	std::vector<int> iVertsRecode;
	iVertsRecode.resize(m_Vertices.size());
	for (int i = 0; i < iVertsRecode.size(); ++i)
	{
		iVertsRecode[i] = i;
	}

    int packedVerticesNum = 1;

	for (int i = 1; i < iSortedVerts.size(); ++i)
	{
		if ((float3 &)m_Vertices[iSortedVerts[i - 1]] == (float3 &)m_Vertices[iSortedVerts[i]])
		{
			iVertsRecode[iSortedVerts[i]] = iVertsRecode[iSortedVerts[i - 1]];
		}
        else
        {
            packedVerticesNum++;
        }
	}

	iSortedVerts.clear();
	m_AdjQuadIndices.resize(m_QuadIndices.size());
	m_AdjTriIndices.resize(m_TriIndices.size());
	
    // recode all indices
    for (int i = 0; i < m_QuadIndices.size(); ++i)
    {
	    m_AdjQuadIndices[i] = iVertsRecode[m_QuadIndices[i][0]];
    }
    for (int i = 0; i < m_TriIndices.size(); ++i)
    {
	    m_AdjTriIndices[i] = iVertsRecode[m_TriIndices[i][0]];
    }

    iVertsRecode.clear();

    if (updatePositions)
    {
        // Delete model normals and tangents if any
        if (m_Normals.empty() == false)
        {
            m_Normals.resize(0);
        }

        if (m_Tangents.empty() == false)
        {
            m_Tangents.resize(0);
        }

        std::vector<int> updatedVertices(m_Vertices.size(), -1);
        std::vector<float4> vertices(packedVerticesNum, float4(0,0,0,0));
        
        // Animation data
        std::vector<int4> bones(packedVerticesNum, int4(0,0,0,0));
        std::vector<float4> weights(packedVerticesNum, float4(0,0,0,0));
        
        int verticesNum = 0;

        for (int i=0; i<m_QuadIndices.size(); ++i)
        {
	        if (updatedVertices[m_AdjQuadIndices[i]] == -1)
            {
                vertices[verticesNum] = m_Vertices[m_AdjQuadIndices[i]];
                
                if (m_pCharacterAnimation)
                {
                    bones[verticesNum] = m_BoneWeights.bones[m_AdjQuadIndices[i]];
                    weights[verticesNum] = m_BoneWeights.weights[m_AdjQuadIndices[i]];
                }
                
                updatedVertices[m_AdjQuadIndices[i]] = verticesNum;
                verticesNum++;
            }
            
            m_QuadIndices[i][0] = m_AdjQuadIndices[i] = updatedVertices[m_AdjQuadIndices[i]];
        }
        
        for (int i=0; i<m_TriIndices.size(); ++i)
        {
	        if (updatedVertices[m_AdjTriIndices[i]] == -1)
            {
                vertices[verticesNum] = m_Vertices[m_AdjTriIndices[i]];
                
                if (m_pCharacterAnimation)
                {
                    bones[verticesNum] = m_BoneWeights.bones[m_AdjTriIndices[i]];
                    weights[verticesNum] = m_BoneWeights.weights[m_AdjTriIndices[i]];
                }
                
                updatedVertices[m_AdjTriIndices[i]] = verticesNum;
                verticesNum++;
            }

            m_TriIndices[i][0] = m_AdjTriIndices[i] = updatedVertices[m_AdjTriIndices[i]];
        }

        m_Vertices.resize(packedVerticesNum);
        std::copy(vertices.begin(), vertices.end(), m_Vertices.begin());
        vertices.clear();

        if (m_pCharacterAnimation)
        {
            m_BoneWeights.bones.resize(packedVerticesNum);
            std::copy(bones.begin(), bones.end(), m_BoneWeights.bones.begin());
            bones.clear();

            m_BoneWeights.weights.resize(packedVerticesNum);
            std::copy(weights.begin(), weights.end(), m_BoneWeights.weights.begin());
            weights.clear();
        }
    }

    return packedVerticesNum;
}
void MyMesh::GenerateUnpackedPosIndices()
{
	m_AdjQuadIndices.resize(m_QuadIndices.size());
	m_AdjTriIndices.resize(m_TriIndices.size());
	// recode all indices
	for (int i = 0; i < m_QuadIndices.size(); ++i)
	{
		m_AdjQuadIndices[i] = m_QuadIndices[i][0];
	}
	for (int i = 0; i < m_TriIndices.size(); ++i)
	{
		m_AdjTriIndices[i] = m_TriIndices[i][0];
	}
}
