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
#include "BzrFile_stencil.h"


BzrFile::BzrFile(const wchar_t * fileName) :
    m_regularPatchCount(0),
    m_quadPatchCount(0),
    m_triPatchCount(0)
{

	m_faceTopologyCount = 0;
	m_primitiveSize = 0;
	m_bezierStencil = NULL;
	m_gregoryStencil = NULL;
	m_regularFaceTopologyIndex = NULL;
	m_regularVertexIndices = NULL;
	m_regularStecilIndices = NULL;
	m_quadVertexCount = NULL;
	m_quadFaceTopologyIndex = NULL;
	m_quadVertexIndices = NULL;
	m_quadStecilIndices = NULL;

    m_regular_bezier_control_points=NULL;
	m_quad_gregory_control_points=NULL;
	m_regular_tex_coords=NULL;
	m_quad_bezier_control_points=NULL;
	m_quad_gregory_control_points=NULL;
	m_quad_pm_control_points=NULL;
	m_quad_tex_coords=NULL;
	m_tri_gregory_control_points=NULL;
	m_tri_pm_control_points=NULL;
	m_tri_tex_coords=NULL;

	m_vertices = NULL;
	m_valences = NULL;
	m_texcoord = NULL;

	m_maxValence = 0;

	m_regularFaceIndices = NULL;
	m_quadFaceIndices = NULL;
	m_triFaceIndices = NULL;

	m_indexCount = 0;
	m_indices = NULL;
	m_indexCount2 = 0;
	m_indices2 = NULL;

	FILE * fp = NULL;
	_wfopen_s(&fp, fileName, L"rb" );

	if (fp == NULL) return;

	// Bezier File Format :
	//   Header ('BZR ')            | sizeof(uint)
	//   Version (1.0)              | sizeof(uint)
	//
	//     Regular patch count            | sizeof(uint)
	//     Quad patch count               | sizeof(uint)
	//     Triangle patch count           | sizeof(uint)

	int header, version;
	fread(&header, sizeof(int), 1, fp);
	fread(&version, sizeof(int), 1, fp);
	if (header != ' RZB' || version != 0x0100)
	{
		return;
	}

	fread(&m_regularPatchCount, sizeof(int), 1, fp);
	fread(&m_quadPatchCount, sizeof(int), 1, fp);
	fread(&m_triPatchCount, sizeof(int), 1, fp);

	// control points
	m_regular_bezier_control_points = new float3 [m_regularPatchCount*16];
	fread(m_regular_bezier_control_points, sizeof(float3), m_regularPatchCount*16, fp);
	m_regular_tex_coords = new float2 [m_regularPatchCount*16];
	fread(m_regular_tex_coords, sizeof(float2), m_regularPatchCount*16, fp);
	m_quad_bezier_control_points = new float3 [m_quadPatchCount*32];
	fread(m_quad_bezier_control_points, sizeof(float3), m_quadPatchCount*32, fp);
	m_quad_gregory_control_points = new float3 [m_quadPatchCount*20];
	fread(m_quad_gregory_control_points, sizeof(float3), m_quadPatchCount*20, fp);
	m_quad_pm_control_points = new float3 [m_quadPatchCount*24];
	fread(m_quad_pm_control_points, sizeof(float3), m_quadPatchCount*24, fp);
	m_quad_tex_coords = new float2 [m_quadPatchCount*16];
	fread(m_quad_tex_coords, sizeof(float2), m_quadPatchCount*16, fp);

	//stencils
	//     faceTopologyCount                   | sizeof(int)
	//     primitiveSize                       | sizeof(int)
	//     Bezier Stencil                      | faceTopologyCount * 32 * m_primitiveSize * sizeof(float)
	//     Gregory Stencil                     | faceTopologyCount * 20 * m_primitiveSize * sizeof(float)
	//     regularFaceTopologyIndex            | regularPatchCount * sizeof(uint)
	//     regularVertexIndices                | 16 * regularPatchCount * sizeof(uint)
	//     regularStecilIndices                | 16 * regularPatchCount * sizeof(uint)
	//     quadVertexCount                     | quadPatchCount * sizeof(uint)
	//     quadFaceTopologyIndex               | quadPatchCount * sizeof(uint)
	//     quadVertexIndices                   | primitiveSize * quadpatchCount * sizeof(uint)
	//     quadStecilIndices                   | primitiveSize * quadPatchCount * sizeof(uint)

	fread(&m_faceTopologyCount, sizeof(int), 1, fp);
	fread(&m_primitiveSize, sizeof(int), 1, fp);
	m_bezierStencil = new float [m_faceTopologyCount * 32 * m_primitiveSize];
	fread(m_bezierStencil, sizeof(float), m_faceTopologyCount * 32 * m_primitiveSize, fp);
	m_gregoryStencil = new float [m_faceTopologyCount * 20 * m_primitiveSize];
	fread(m_gregoryStencil, sizeof(float), m_faceTopologyCount * 20 * m_primitiveSize, fp);

	m_regularFaceTopologyIndex = new unsigned int [m_regularPatchCount];
	fread(m_regularFaceTopologyIndex, sizeof(unsigned int), m_regularPatchCount, fp);
	m_regularVertexIndices = new unsigned int [m_regularPatchCount*16];
	fread(m_regularVertexIndices, sizeof(unsigned int), m_regularPatchCount*16, fp);
	m_regularStecilIndices = new unsigned int [m_regularPatchCount*16];
	fread(m_regularStecilIndices, sizeof(unsigned int), m_regularPatchCount*16, fp);

	m_quadVertexCount = new unsigned int [m_quadPatchCount];
	fread(m_quadVertexCount, sizeof(unsigned int), m_quadPatchCount, fp);
	m_quadFaceTopologyIndex = new unsigned int [m_quadPatchCount];
	fread(m_quadFaceTopologyIndex, sizeof(unsigned int), m_quadPatchCount, fp);
	m_quadVertexIndices = new unsigned int [m_quadPatchCount*m_primitiveSize];
	fread(m_quadVertexIndices, sizeof(unsigned int), m_quadPatchCount*m_primitiveSize, fp);
	m_quadStecilIndices = new unsigned int [m_quadPatchCount*m_primitiveSize];
	fread(m_quadStecilIndices, sizeof(unsigned int), m_quadPatchCount*m_primitiveSize, fp);


	//   Input Meshe Topology:
	//     Vertex count                   | sizeof(uint)
	//     Vertices                       | vertexCount * sizeof(float3)
	//     tangents                       | vertexCount * 2 * sizeof(float3)
	//     Valences                       | vertexCount * sizeof(int)
	//     Texture coordinate             | vertexCount * sizeof(float2)
	//     Max valence                    | sizeof(uint)
	//     Regular face indices           | 4 * regularPatchCount * sizeof(uint)
	//     Quad face indices              | 4 * irregularpatchCount * sizeof(uint)
	//     Triangle face indices          | 3 * trianglePatchCount * sizeof(uint)
	//
	fread(&m_vertexCount, sizeof(int), 1, fp);
	m_vertices = new float3[m_vertexCount];
	m_tangents = new float3[2*m_vertexCount];
	m_valences = new int[m_vertexCount];
	m_texcoord = new float2[m_vertexCount];

	fread(m_vertices, sizeof(float3), m_vertexCount, fp);
	fread(m_tangents, sizeof(float3), 2*m_vertexCount, fp);
	fread(m_valences, sizeof(int), m_vertexCount, fp);
	fread(m_texcoord, sizeof(float2), m_vertexCount, fp);

	// Read topology info
	fread(&m_maxValence, sizeof(int), 1, fp);

	m_regularFaceIndices = new unsigned int [4 * m_regularPatchCount];
	m_quadFaceIndices = new unsigned int [4 * m_quadPatchCount];
	m_triFaceIndices = new unsigned int [3 * m_triPatchCount];
	fread(m_regularFaceIndices, sizeof(unsigned int), 4 * m_regularPatchCount, fp);
	fread(m_quadFaceIndices, sizeof(unsigned int), 4 * m_quadPatchCount, fp);
	fread(m_triFaceIndices, sizeof(unsigned int), 3 * m_triPatchCount, fp);

	m_regularIndices = new unsigned int [4 * m_regularPatchCount];
	for (int k = 0; k < m_regularPatchCount; k++)
	{
		for (int j=0; j<4; j++)
		{
			for (int i = 0; i < 16; i++)
			{
				if (m_regularVertexIndices[k*16+i] ==  m_regularFaceIndices[k*4+j])
					m_regularIndices[k*4+j] = i;
			}
		}
	}

	m_quadIndices = new unsigned int [4 * m_quadPatchCount];
	for (int k = 0; k < m_quadPatchCount; k++)
	{
		for (int j=0; j<4; j++)
		{
			for (int i = 0; i < m_primitiveSize; i++)
			{
				if (m_quadVertexIndices[k*m_primitiveSize+i] ==  m_quadFaceIndices[k*4+j])
					m_quadIndices[k*4+j] = i;  
			}
		}
	}

	// Compute input mesh indices
	m_indexCount = 6 * m_regularPatchCount + 6 * m_quadPatchCount + 3 * m_triPatchCount;
	m_indexCount2 = 8 * m_regularPatchCount + 8 * m_quadPatchCount;
	m_indices = new unsigned int[m_indexCount];
	m_indices2 = new unsigned int[m_indexCount2];

	int idx = 0;
	for (int i = 0; i < m_regularPatchCount; i++)
	{
		m_indices[idx++] = m_regularFaceIndices[4 * i + 2];
		m_indices[idx++] = m_regularFaceIndices[4 * i + 0];
		m_indices[idx++] = m_regularFaceIndices[4 * i + 1];

		m_indices[idx++] = m_regularFaceIndices[4 * i + 0];
		m_indices[idx++] = m_regularFaceIndices[4 * i + 2];
		m_indices[idx++] = m_regularFaceIndices[4 * i + 3];

	}
	for (int i = 0; i < m_quadPatchCount; i++)
	{
		m_indices[idx++] = m_quadFaceIndices[4 * i + 2];
		m_indices[idx++] = m_quadFaceIndices[4 * i + 0];
		m_indices[idx++] = m_quadFaceIndices[4 * i + 1];

		m_indices[idx++] = m_quadFaceIndices[4 * i + 0];
		m_indices[idx++] = m_quadFaceIndices[4 * i + 2];
		m_indices[idx++] = m_quadFaceIndices[4 * i + 3];

	}
	for (int i = 0; i < m_triPatchCount; i++)
	{
		m_indices[idx++] = m_triFaceIndices[3 * i + 0];
		m_indices[idx++] = m_triFaceIndices[3 * i + 1];
		m_indices[idx++] = m_triFaceIndices[3 * i + 2];
	}
	// for drawing input mesh
	idx = 0;
	for (int i = 0; i < m_regularPatchCount; i++)
	{
		m_indices2[idx++] = m_regularFaceIndices[4 * i + 0];
		m_indices2[idx++] = m_regularFaceIndices[4 * i + 1];
		m_indices2[idx++] = m_regularFaceIndices[4 * i + 1];
		m_indices2[idx++] = m_regularFaceIndices[4 * i + 2];
		m_indices2[idx++] = m_regularFaceIndices[4 * i + 2];
		m_indices2[idx++] = m_regularFaceIndices[4 * i + 3];
		m_indices2[idx++] = m_regularFaceIndices[4 * i + 3];
		m_indices2[idx++] = m_regularFaceIndices[4 * i + 0];
	}
	for (int i = 0; i < m_quadPatchCount; i++)
	{				
		m_indices2[idx++] = m_quadFaceIndices[4 * i + 0];
		m_indices2[idx++] = m_quadFaceIndices[4 * i + 1];
		m_indices2[idx++] = m_quadFaceIndices[4 * i + 1];
		m_indices2[idx++] = m_quadFaceIndices[4 * i + 2];
		m_indices2[idx++] = m_quadFaceIndices[4 * i + 2];
		m_indices2[idx++] = m_quadFaceIndices[4 * i + 3];
		m_indices2[idx++] = m_quadFaceIndices[4 * i + 3];
		m_indices2[idx++] = m_quadFaceIndices[4 * i + 0];
	}
	// Compute bbox.
	float3 minCorner = m_vertices[0];
	float3 maxCorner = m_vertices[0];

	for (int i = 0; i < m_vertexCount; i++)
	{
		const float3 & v = m_vertices[i];

		if (minCorner.x > v.x) minCorner.x = v.x;
		else if (maxCorner.x < v.x) maxCorner.x = v.x;

		if (minCorner.y > v.y) minCorner.y = v.y;
		else if (maxCorner.y < v.y) maxCorner.y = v.y;

		if (minCorner.z > v.z) minCorner.z = v.z;
		else if (maxCorner.z < v.z) maxCorner.z = v.z;
	}

	m_center = (minCorner + maxCorner) * 0.5f;
	//float3 extents = (minCorner - maxCorner) * 0.5f;

	fclose(fp);

}


BzrFile::~BzrFile()
{

    delete [] m_vertices;
	delete [] m_tangents;
    delete [] m_valences;
	delete [] m_texcoord;

    delete [] m_regularFaceIndices;
    delete [] m_quadFaceIndices;
    delete [] m_triFaceIndices;

	delete [] m_regularIndices;
	delete [] m_quadIndices;

	delete [] m_bezierStencil ;
	delete [] m_gregoryStencil ;

	delete [] m_regularFaceTopologyIndex ;
	delete [] m_regularVertexIndices ;
	delete [] m_regularStecilIndices ;

    delete [] m_quadVertexCount ;
	delete [] m_quadFaceTopologyIndex ;
	delete [] m_quadVertexIndices ;
	delete [] m_quadStecilIndices ;

    delete [] m_indices;
	delete [] m_indices2;

	delete [] m_regular_bezier_control_points;
	delete [] m_quad_gregory_control_points;
	delete [] m_regular_tex_coords;
	delete [] m_quad_bezier_control_points;
	delete [] m_quad_pm_control_points;
	delete [] m_quad_tex_coords;
	delete [] m_tri_gregory_control_points;
	delete [] m_tri_pm_control_points;
	delete [] m_tri_tex_coords;
}

