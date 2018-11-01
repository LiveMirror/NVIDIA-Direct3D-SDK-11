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

#ifndef HAIR_H
#define HAIR_H

#pragma warning(disable:4995) // avoid warning for "...was marked as #pragma deprecated"
#pragma warning(disable:4244) // avoid warning for "conversion from 'float' to 'int'"

extern int g_Width;
extern int g_Height;

#define NUM_MRTS 8
#define NUM_PASSES 1
#define ARRAY_SIZE (NUM_PASSES * NUM_MRTS)

//demo---------------------------------------------------------------

#define NUM_TESSELLATION_SEGMENTS 2
//#define HIGH_RES_MODEL
#define ADAPTIVE_HIGH_RES_MODEL

#ifdef HIGH_RES_MODEL
    #define NUM_STRANDS_PER_WISP 10
#elif defined (ADAPTIVE_HIGH_RES_MODEL)
    #define NUM_STRANDS_PER_WISP 1
#else
    #define NUM_STRANDS_PER_WISP 20
#endif

//#define USE_CSM
#define CSM_ZMAX 1.0
#define	CSM_HALF_NUM_TEX 4

//-------------------------------------------------------------------


/*
//testing -----------------------------------------------------------

#define NUM_TESSELLATION_SEGMENTS 1
//#define HIGH_RES_MODEL
#define ADAPTIVE_HIGH_RES_MODEL

#ifdef HIGH_RES_MODEL
    #define NUM_STRANDS_PER_WISP 10
#elif defined (ADAPTIVE_HIGH_RES_MODEL)
    #define NUM_STRANDS_PER_WISP 1
#else
    #define NUM_STRANDS_PER_WISP 20
#endif
*/

//-------------------------------------------------------------------
#define SMALL_NUMBER 0.00001

#ifndef SAFE_ACQUIRE
#define SAFE_ACQUIRE(dst, p)      { if(dst) { SAFE_RELEASE(dst); } if (p) { (p)->AddRef(); } dst = (p); }
#endif


#ifdef __cplusplus

#include "Fluid.h"
#include <string>
using namespace std;

extern Fluid* g_fluid;

typedef int int4[4];
typedef D3DXVECTOR2 float2;
typedef D3DXVECTOR3 float3;
typedef D3DXVECTOR4 float4;


struct HairVertex 
{
    float4 Position;
};



struct coordinateFrame
{
    float3 x;
	float3 y;
	float3 z;
};

struct CFVertex 
{
    float4 Position;
	float3 Color;
};

struct Attributes
{ 
    D3DXVECTOR2 texcoord;
}; 

struct collisionImplicit
{
    float3 center;
	float3 rotation;
	float3 scale;
};

struct StrandRoot 
{
    D3DXVECTOR3 Position;
    D3DXVECTOR3 Normal;
    D3DXVECTOR2 Texcoord;
    D3DXVECTOR3 Tangent;
};

struct GRID_TEXTURE_DISPLAY_STRUCT
{   
    D3DXVECTOR3 Pos; // Clip space position for slice vertices
    D3DXVECTOR3 Tex; // Cell coordinates in 0-"texture dimension" range
};

struct hairShadingParameters
{
	float m_baseColor[4];
	float m_specColor[4];
	float m_ksP;
	float m_ksS;
	float m_kd;
	float m_specPowerPrimary;
	float m_specPowerSecondary;    
	float m_ksP_sparkles;
    float m_specPowerPrimarySparkles;
    float m_ka;
	hairShadingParameters(){};
    void assignValues(float baseColor[4],float specColor[4],float ksP,float ksS,float kd,float specPowerPrimary,float specPowerSecondary,float ksP_sparkles,float specPowerPrimarySparkles,float ka)
	{
		m_baseColor[0] = baseColor[0]; m_baseColor[1] = baseColor[1]; m_baseColor[2] = baseColor[2]; m_baseColor[3] = baseColor[3];
		m_specColor[0] = specColor[0]; m_specColor[1] = specColor[1]; m_specColor[2] = specColor[2]; m_specColor[3] = specColor[3];
		m_ksP = ksP;
		m_ksS = ksS;
		m_kd = kd;
		m_specPowerPrimary = specPowerPrimary;
		m_specPowerSecondary = specPowerSecondary;    
		m_ksP_sparkles = ksP_sparkles;
		m_specPowerPrimarySparkles = specPowerPrimarySparkles;
		m_ka = ka;	
	};
	void setShaderVariables(ID3DX11Effect* pEffect)
	{
	    pEffect->GetVariableByName("g_ksP")->AsScalar()->SetFloat(m_ksP);
	    pEffect->GetVariableByName("g_ksS")->AsScalar()->SetFloat(m_ksS);
		pEffect->GetVariableByName("g_kd")->AsScalar()->SetFloat(m_kd);
		pEffect->GetVariableByName("g_ka")->AsScalar()->SetFloat(m_ka);
		pEffect->GetVariableByName("g_specPowerPrimary")->AsScalar()->SetFloat(m_specPowerPrimary);
		pEffect->GetVariableByName("g_specPowerSecondary")->AsScalar()->SetFloat(m_specPowerSecondary);
		pEffect->GetVariableByName("g_ksP_sparkles")->AsScalar()->SetFloat(m_ksP_sparkles);
		pEffect->GetVariableByName("g_specPowerPrimarySparkles")->AsScalar()->SetFloat(m_specPowerPrimarySparkles);
    	pEffect->GetVariableByName("g_baseColor")->AsVector()->SetFloatVector(m_baseColor);
		pEffect->GetVariableByName("g_specColor")->AsVector()->SetFloatVector(m_specColor);
	}

};


enum BLURTYPE
{
    GAUSSIAN_BLUR,
	BOX_BLUR,
};

enum IMPLICIT_TYPE
{
    SPHERE,
	CYLINDER,
	SPHERE_NO_MOVE_CONSTRAINT,
	NOT_AN_IMPLICIT
};

struct collisionObject
{
	bool isHead;                   // if this is the head it is also used to transform the hair
    string boneName;                // the name of the bone that this object is attached. if there is no bone then we use the global transform for the object
	IMPLICIT_TYPE implicitType;    // type of implicit: sphere or cylinder
	D3DXMATRIX InitialTransform;   // the total initial transform; this is the transform that takes a unit implicit to the hair base pose
    D3DXMATRIX currentTransform;   // the transform that takes the implicit from base pose to the current hair space (different from world space)
    D3DXMATRIX objToMesh;          // the transform that goes from the hair base pose to the mesh bind pose, such that after multiplying with the meshWorldXForm we end up at the correct real world position 
};

enum RENDERTYPE
{
    INSTANCED_DEPTH,
    INSTANCED_DENSITY,
    INSTANCED_NORMAL_HAIR,
	INSTANCED_INTERPOLATED_COLLISION,
	INSTANCED_HAIR_DEPTHPASS,
	INSTANCED_COLLISION_RESULTS,
	SOATTRIBUTES,
	INSTANCED_DEPTH_DOM,
};
enum INTERPOLATE_MODEL
{
	HYBRID,
    MULTI_HYBRID,
	MULTISTRAND,
	SINGLESTRAND,
	NO_HAIR,

	NUM_INTERPOLATE_MODELS
};

typedef int Index;
typedef Index Wisp[4];

extern Attributes* g_MasterAttributes;
class CDXUTSDKMesh;
extern CDXUTSDKMesh* g_Scalp;
extern int g_NumWisps;
extern int g_NumMasterStrands;
extern unsigned int g_TessellatedMasterStrandLengthMax;
extern int g_NumMasterStrandControlVertices;
extern HairVertex* g_MasterStrandControlVertices;
extern Index* g_MasterStrandControlVertexOffsets;
extern Index* g_TessellatedMasterStrandVertexOffsets;
//extern StrandRoot (*g_StrandRoots)[NUM_STRANDS_PER_WISP];
// Rendering
extern int g_NumTessellatedMasterStrandVertices;
extern int g_NumTessellatedWisps;
extern int* g_indices;
extern int g_MasterStrandLengthMax;

extern float* g_coordinateFrames;

extern float g_maxHairLength;
extern float g_scalpMaxRadius;

extern float vecLength(HairVertex v1,HairVertex v2);

bool LoadMayaHair( char* directory, bool b_shortHair );

extern void rotateVector(const float3& rotationAxis,float theta,const float3& prevVec, float3& newVec);
extern void vectorMatrixMultiply(D3DXVECTOR3* vecOut,const D3DXMATRIX matrix,const D3DXVECTOR3 vecIn);
extern void vectorMatrixMultiply(D3DXVECTOR4* vecOut,const D3DXMATRIX matrix,const D3DXVECTOR4 vecIn);

#else

struct HairVertex {
    float4 Position : Position;
};

#endif

#define MAX_IMPLICITS 10

#define USETEXTURES 
#define USE_TEXTURE_COLOR

#endif