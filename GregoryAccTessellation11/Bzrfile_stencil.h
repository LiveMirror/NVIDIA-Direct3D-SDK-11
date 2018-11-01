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

#ifndef BZRFILE_STENCIL_H
#define BZRFILE_STENCIL_H

#define ENABLE_D3D10	1
#define ENABLE_GL		0

#if ENABLE_D3D9 || ENABLE_D3D10
#include <d3d10.h>
#include <d3d9.h>
#endif


#if ENABLE_D3D9 || ENABLE_D3D10
typedef D3DXVECTOR2 float2;
typedef D3DXVECTOR3 float3;
typedef D3DXVECTOR4 float4;
#endif
class BzrFile
{
public:
    BzrFile(const wchar_t * fileName);
    ~BzrFile();

    int regularPatchCount() const { return m_regularPatchCount; }
    int quadPatchCount() const { return m_quadPatchCount; }
    int triPatchCount() const { return m_triPatchCount; }
    int vertexCount() const { return m_vertexCount; }

    int totalPatchCount() const { return m_regularPatchCount + m_quadPatchCount + m_triPatchCount; }

    int indexCount() const { return m_indexCount; }
	int indexCount2() const { return m_indexCount2; }
    int maxValence() const { return m_maxValence; }

	int faceTopologyCount() const { return m_faceTopologyCount; }
	int primitiveSize() const { return m_primitiveSize; }

    const float3 & center() const { return m_center; }

	unsigned int *  fetchRegularFaceTopologyIndex()  {return m_regularFaceTopologyIndex;}
	unsigned int *  fetchRegularStecilIndices()  {return m_regularStecilIndices;}

	unsigned int *  fetchQuadVertexCount() const {return m_quadVertexCount;}
	unsigned int *  fetchQuadFaceTopologyIndex() const {return m_quadFaceTopologyIndex;}
	unsigned int *  fetchQuadStecilIndices() const {return m_quadStecilIndices;}

    const float3 *  fetchVertices () {return m_vertices;}
    const int *  fetchValences () {return m_valences;}
	const float2 *  fetchTexcoord () {return m_texcoord;}
	const float3 *  fetchTangentSpace() {return m_tangents;}
	const float3 *  fetchBezierControlPoints() {return m_regular_bezier_control_points;}
	const float3 *  fetchGregoryControlPoints() {return m_quad_gregory_control_points;}
	const float2 *  fetchQuadTexcoord() {return m_quad_tex_coords;}
	const float2 *  fetchRegularTexcoord() {return m_regular_tex_coords;}

	unsigned int *  fetchRegularVertexIndices()  {return m_regularVertexIndices;}
	unsigned int *  fetchQuadVertexIndices() const {return m_quadVertexIndices;}
    unsigned int *  fetchTriIndicies () {return m_triFaceIndices;}
    unsigned int *  fetchInputMeshVertexIndices() const {return m_indices;}
	unsigned int *  fetchInputMeshVertexIndices2() const {return m_indices2;}
	unsigned int *  fetchRegularInputMeshVertexIndices() const {return m_regularIndices;}
	unsigned int *  fetchQuadInputMeshVertexIndices() const {return m_quadIndices;}

	const float *  fetchBezierStencil() const {return m_bezierStencil;}
	const float *  fetchGregoryStencil() const {return m_gregoryStencil;}

private:
    
    int m_regularPatchCount;
    int m_quadPatchCount;
    int m_triPatchCount;

    struct {
        float3 * bezierControlPoints;
        float2 * texcoords;
    } m_regularPatches;

    struct {
        float3 * bezierControlPoints;
        float3 * gregoryControlPoints;
        float3 * pmControlPoints;
        float2 * texcoords;
    } m_quadPatches;

    struct {
        float3 * gregoryControlPoints;
        float3 * pmControlPoints;
        float2 * texcoords;
    } m_triPatches;

    int m_vertexCount;
    float3 * m_vertices;
	float3 * m_tangents;
    int * m_valences;
	float2 * m_texcoord;
	float4 * m_regular_face_tangents;
	float4 * m_quad_face_tangents;

    int m_maxValence;
    int m_faceTopologyCount;
	int m_primitiveSize;
	float * m_bezierStencil;
	float * m_gregoryStencil;
	unsigned int * m_regularFaceTopologyIndex;
	unsigned int * m_regularVertexIndices;
	unsigned int * m_regularStecilIndices;
    unsigned int * m_quadVertexCount;
	unsigned int * m_quadFaceTopologyIndex;
	unsigned int * m_quadVertexIndices;
	unsigned int * m_quadStecilIndices;

    unsigned int * m_regularFaceIndices;
    unsigned int * m_quadFaceIndices;
    unsigned int * m_triFaceIndices;
	unsigned int * m_regularIndices; // the mapping between m_regularFaceIndices and m_regularVertexIndices
	unsigned int * m_quadIndices;    // the mapping between m_quadFaceIndices and m_quadVertexIndices

    int m_indexCount;
    unsigned int * m_indices;
	int m_indexCount2;
    unsigned int * m_indices2;


    float3 *m_regular_bezier_control_points;
	float3 *m_gregory_control_points;
	float2 *m_regular_tex_coords;
	float3 *m_quad_bezier_control_points;
	float3 *m_quad_gregory_control_points;
	float3 *m_quad_pm_control_points;
	float2 *m_quad_tex_coords;
	float3 *m_tri_gregory_control_points;
	float3 *m_tri_pm_control_points;
	float2 *m_tri_tex_coords;


    float3 m_center;
};

#endif // BZRFILE_STENCIL_H
