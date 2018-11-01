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

#pragma warning(disable:4995) // avoid warning for "...was marked as #pragma deprecated"
#pragma warning(disable:4244) // avoid warning for "conversion from 'float' to 'int'"
#pragma warning(disable:4201) //avoid warning from mmsystem.h "warning C4201: nonstandard extension used : nameless struct/union"

#include "Common.h"
#include "Hair.h"
#include "math.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <stdio.h>
using namespace std;


#define sqr(a) ( (a)*(a) )


struct vertex
{
    double x;
	double y;
	double z;
	vertex(double xi, double yi, double zi){x=xi; y=yi; z=zi;};
	vertex(){x=0;y=0;z=0;};
};

struct face
{
    int v1;
	int v2;
	int v3;
	face(double v1i, double v2i, double v3i){v1=v1i; v2=v2i; v3=v3i;};
	face(){v1=0;v2=0;v3=0;};

};

float vecDot(float3 vec1, float3 vec2)
{   
	return (vec1.x*vec2.x + vec1.y*vec2.y + vec1.z*vec2.z );
}
float vecLength(float3 vec1)
{
    return sqrt( sqr(vec1.x) + sqr(vec1.y) + sqr(vec1.z) );
}

void GramSchmidtOrthogonalize( float3& x, float3& y, float3& z )
{
    //x remains as it is
	y = y - vecDot(x,y)/sqr(vecLength(x))*x;
	z = z - vecDot(x,z)/sqr(vecLength(x))*x - vecDot(y,z)/sqr(vecLength(y))*y;
	y = y/vecLength(y);
	z = z/vecLength(z);
}


void GramSchmidtOrthoNormalize( float3& x, float3& y, float3& z )
{
    x = x/vecLength(x);

	y = y - vecDot(x,y)*x;
	y = y/vecLength(y);

	z = z - vecDot(x,z)*x - vecDot(y,z)*y;
	z = z/vecLength(z);
}


typedef vector<vertex> VertexVector;





bool LoadMayaHair( char* directory, bool b_shortHair )
{
	HRESULT hr;

	char filename[MAX_PATH];
    WCHAR wfilename[MAX_PATH];

	TCHAR HairStrandsTextFile[MAX_PATH];
	sprintf_s(filename,100,"%s\\%s",directory,"hair_vertices.txt");
	mbstowcs_s(NULL, wfilename, MAX_PATH, filename, MAX_PATH);
	V(NVUTFindDXSDKMediaFileCchT( HairStrandsTextFile, MAX_PATH,wfilename));

	TCHAR OBJScalpMeshFile[MAX_PATH];
	sprintf_s(filename,100,"%s\\%s",directory,"scalp_mesh.obj");
	mbstowcs_s(NULL, wfilename, MAX_PATH, filename, MAX_PATH);
	V(NVUTFindDXSDKMediaFileCchT( OBJScalpMeshFile, MAX_PATH,wfilename));


	char c[MAX_PATH];
	char charTemp;
	int totalCVs = 0;

	//read the files ---------------------------------------------------------------------------

	//read the hair strands and push them into HairStrands
    vector<VertexVector> HairStrands;
	vector<float2> hairTextures;
	double x,y,z,u,v;
	char tempChar;
	ifstream infile;
	infile.open(HairStrandsTextFile);	

	if (!infile)
		return false;
	


	while(!infile.eof())
	{
		//find the first line with a comment "//", this is the beginning of a hair strand
        infile.getline(c,MAX_PATH);
        string s(c);
		string::size_type loc = s.find("//",0);
		if( loc != string::npos )
		{
		    istringstream vertexString(s);

			//number of vertices
			int numVertices;
			vertexString>>numVertices;

            //texcoords
            infile.getline(c,MAX_PATH);
            string s2(c);
			istringstream texString(s2);
			texString>>tempChar>>u>>v;
			hairTextures.push_back(float2(u,v));
 

			//read the hairStrand
			vector<vertex> hairStrand;
            for(int i=0;i<numVertices;i++)
			{
			    infile.getline(c,MAX_PATH); 
			    string h(c);
			    istringstream vertexString(h);
			    vertexString>>x>>y>>z;
				vertex hairVertex(x,y,-z);//flip z axis to go from MAYA RH coordinate system to D3D LH coordinate system
    			        
				if(!b_shortHair || i<13)
				{ hairStrand.push_back(hairVertex); 
		          totalCVs++;
				}

			}
		    HairStrands.push_back(hairStrand);
		}
	}
    infile.close();

	//read the mesh file, just reading the faces, and stick the faces into the index buffer
	vector<face> HairFaces;
	ifstream infileMesh;
	infileMesh.open(OBJScalpMeshFile);
	if (infileMesh) 
	{
		while(!infileMesh.eof())
		{        
			infileMesh.getline(c,MAX_PATH);
            string s(c);
			string::size_type locV = s.find("f ",0);
			string::size_type locHash = s.find("#",0);

			if( locV ==0 && locHash != 0 )
			{
				istringstream vertexString(s);
				face tempFace;
				vertexString>>charTemp>>tempFace.v1>>tempFace.v2>>tempFace.v3;
				//make the vertex index be 0 based which is what I'm using. Obj is 1 based - first vertex is 1
				tempFace.v1 -= 1;
				tempFace.v2 -= 1;
				tempFace.v3 -= 1;
				HairFaces.push_back(tempFace);
			}
		}
		infileMesh.close();
	} 


	//----------------------------------------------------------------------------------------------
    //transfer to the global variables--------------------------------------------------------------
	//----------------------------------------------------------------------------------------------

	g_NumWisps = int(HairFaces.size());
    g_NumMasterStrands = int(HairStrands.size());

	
	
	//fill g_MasterStrandControlVertices:------------------------------------------------------------
    
	g_MasterStrandControlVertices = new HairVertex[totalCVs];
    g_MasterStrandControlVertexOffsets = new Index[HairStrands.size()];
	g_NumMasterStrandControlVertices = totalCVs;

    int index = 0; 
    g_MasterStrandLengthMax = 0;
    g_maxHairLength = 0;
	float currentHairLength = 0;
	float averageHairLength = 0;

	for(int i=0;i<int(HairStrands.size());i++)
	{
		currentHairLength = 0;
	    for(int j=0;j<int(HairStrands.at(i).size());j++)
		{
		    HairVertex v;
			v.Position.x = HairStrands.at(i).at(j).x;
			v.Position.y = HairStrands.at(i).at(j).y;
			v.Position.z = HairStrands.at(i).at(j).z;
			if(j<2)
			    v.Position.w = -1.0;
			else if(j==int(HairStrands.at(i).size())-1)
				v.Position.w = 1.0;
			else
				v.Position.w = 0.0;
            g_MasterStrandControlVertices[index++] = v;

			if(j>0)
                currentHairLength += vecLength(g_MasterStrandControlVertices[index-1],g_MasterStrandControlVertices[index-2]); 
		}
		if(int(HairStrands.at(i).size()) > g_MasterStrandLengthMax)
            g_MasterStrandLengthMax = int(HairStrands.at(i).size());  
		if(currentHairLength > g_maxHairLength)
            g_maxHairLength = currentHairLength;
		averageHairLength += currentHairLength;

        g_MasterStrandControlVertexOffsets[i] = index;
	}

	averageHairLength /= int(HairStrands.size());

    // Compute tessellated master strand vertex offsets----------------------------------------------

	int beginningEndSegments = 2;	//need to add two extra segments near the beginning and end
	int extraEndVertex = 1;
	int numMasterStrandSegmentsBsplines = 0;
    Index* tessellatedMasterStrandVertexOffsets = new Index[g_NumMasterStrands];
    
	int segments = (g_MasterStrandControlVertexOffsets[0] - 3 + beginningEndSegments );
	tessellatedMasterStrandVertexOffsets[0] = ( NUM_TESSELLATION_SEGMENTS * segments ) + extraEndVertex;
	numMasterStrandSegmentsBsplines += segments ;
    int tessellatedMasterStrandLengthMax = tessellatedMasterStrandVertexOffsets[0];
    
	for (int o = 1; o < g_NumMasterStrands; ++o) 
    {
		int segments = g_MasterStrandControlVertexOffsets[o] - g_MasterStrandControlVertexOffsets[o - 1] - 3 + beginningEndSegments;
        Index lengthStrand = ( NUM_TESSELLATION_SEGMENTS * segments ) + extraEndVertex;
        numMasterStrandSegmentsBsplines += segments;
        tessellatedMasterStrandVertexOffsets[o] = tessellatedMasterStrandVertexOffsets[o - 1] + lengthStrand;

        if (lengthStrand > tessellatedMasterStrandLengthMax)
            tessellatedMasterStrandLengthMax = lengthStrand;
    }

    g_TessellatedMasterStrandLengthMax = tessellatedMasterStrandLengthMax;
	g_NumTessellatedMasterStrandVertices = (NUM_TESSELLATION_SEGMENTS * numMasterStrandSegmentsBsplines) + g_NumMasterStrands;
    g_TessellatedMasterStrandVertexOffsets = tessellatedMasterStrandVertexOffsets;

     
    //------------------------------------------------------------------------------------------------------
    //indices:

	g_indices = new int[HairFaces.size()*3];
	for(int i=0;i<int(HairFaces.size());i++)
	{
		g_indices[i*3] = HairFaces.at(i).v1;
		g_indices[i*3 + 1] = HairFaces.at(i).v2;
		g_indices[i*3 + 2] = HairFaces.at(i).v3;
	}

    //----------------------------------------------------------------------------------------------------------
	//coordinate frames:
	g_coordinateFrames = new float[totalCVs*3*4]; //3 vec4s per coordinate frame    
	index = 0;
//	int numTimes = 0;
	int CFNum = 0;
	for(int i=0;i<totalCVs*3*4;i++)
        g_coordinateFrames[i] = 0;

	for(int s=0;s<int(HairStrands.size());s++)
	{
		for(int i=0;i<int(HairStrands.at(s).size())-1;i++)
		{
			float3 x = float3( HairStrands.at(s).at(i+1).x - HairStrands.at(s).at(i).x,
							   HairStrands.at(s).at(i+1).y - HairStrands.at(s).at(i).y,
			    			   HairStrands.at(s).at(i+1).z - HairStrands.at(s).at(i).z);
			D3DXVec3Normalize(&x,&x);
			float3 y;
			float3 z;

   			if(i==0)
			{   
				//note: we could also load here the tangent vector of the root on the scalp rather than a random vector
				float3 randVec = float3( rand(),rand(),rand() );
				D3DXVec3Normalize(&randVec,&randVec);
				D3DXVec3Cross(&z,&x,&randVec);
				D3DXVec3Normalize(&z,&z);
				D3DXVec3Cross(&y,&x,&z);
				D3DXVec3Normalize(&y,&y);
			}
			else
			{

				float3 xRotationVector;
                float3 prevX = float3( g_coordinateFrames[(CFNum-1)*12 + 0] , g_coordinateFrames[(CFNum-1)*12 + 1] , g_coordinateFrames[(CFNum-1)*12 + 2] );
				float3 prevY = float3( g_coordinateFrames[(CFNum-1)*12 + 4] , g_coordinateFrames[(CFNum-1)*12 + 5] , g_coordinateFrames[(CFNum-1)*12 + 6] );
				float3 prevZ = float3( g_coordinateFrames[(CFNum-1)*12 + 8] , g_coordinateFrames[(CFNum-1)*12 + 9] , g_coordinateFrames[(CFNum-1)*12 + 10] );


				D3DXVec3Cross(&xRotationVector, &x, &prevX );
				float sinTheta = D3DXVec3Length(&xRotationVector);
				float theta = asin(sinTheta);
				D3DXVec3Normalize(&xRotationVector,&xRotationVector);
				rotateVector(xRotationVector, theta, prevY, y);
				D3DXVec3Normalize(&y,&y);
				rotateVector(xRotationVector, theta, prevZ, z);
				D3DXVec3Normalize(&z,&z);

				//do a gram-Schmidt orthogonalization on these vectors, just to be sure
#if 0
				float3 tempX = x;
				float3 tempY = y;
				float3 tempZ = z;
				GramSchmidtOrthoNormalize( tempX,tempY,tempZ );
	
				if( vecLength(x-tempX) > SMALL_NUMBER)
				    numTimes++;
				if( vecLength(y-tempY) > SMALL_NUMBER)
					numTimes++;
				if( vecLength(z-tempZ) > SMALL_NUMBER)
					numTimes++;
#endif				
				

			}
	      
			g_coordinateFrames[CFNum*12+0] = x.x; g_coordinateFrames[CFNum*12+1] = x.y; g_coordinateFrames[CFNum*12+2] = x.z; g_coordinateFrames[CFNum*12+3] = 0;  
			g_coordinateFrames[CFNum*12+4] = y.x; g_coordinateFrames[CFNum*12+5] = y.y; g_coordinateFrames[CFNum*12+6] = y.z; g_coordinateFrames[CFNum*12+7] = 0; 
			g_coordinateFrames[CFNum*12+8] = z.x; g_coordinateFrames[CFNum*12+9] = z.y; g_coordinateFrames[CFNum*12+10] = z.z; g_coordinateFrames[CFNum*12+11] = 0;  
			CFNum++;
		}
        
		//coordinate axis for last vertex in strand are undefined; setting them to zero
		for(int k=0;k<12;k++)
            g_coordinateFrames[CFNum*12+k] = 0;

		CFNum++;
	}
	


    //attributes------------------------------------------------------------------------------------------------
	g_MasterAttributes = new Attributes[g_NumMasterStrands]; 
    for (int s = 0; s < g_NumMasterStrands; ++s)
	    g_MasterAttributes[s].texcoord  = hairTextures[s];
	

	return true;
}
