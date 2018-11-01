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

#include "Defines.h"

Texture2DArray   g_txLPVRed                          : register( t3 );
Texture2DArray   g_txLPVGreen                        : register( t4 );
Texture2DArray    g_txLPVBlue                        : register( t5 );
Texture2DArray    g_txGV                             : register( t6 );
Texture2DArray    g_txGVColor                        : register( t7 );
Texture2DArray    g_txAccumulateRed                  : register( t8 );
Texture2DArray    g_txAccumulateGreen                : register( t9 );
Texture2DArray    g_txAccumulateBlue                 : register( t10 );
RWTexture2DArray<float4>    g_uavLPVRed              : register( u0 );
RWTexture2DArray<float4>    g_uavLPVGreen            : register( u1 );
RWTexture2DArray<float4>    g_uavLPVBlue             : register( u2 );
RWTexture2DArray<float4>    g_uavLPVRedAccum         : register( u3 );
RWTexture2DArray<float4>    g_uavLPVGreenAccum       : register( u4 );
RWTexture2DArray<float4>    g_uavLPVBlueAccum        : register( u5 );


cbuffer cbConstantsLPVinitialize : register( b0 )
{
    int RSMWidth         : packoffset(c0.x);
    int RSMHeight        : packoffset(c0.y);
    float tan_FovXhalf   : packoffset(c0.z);
    float tan_FovYhalf   : packoffset(c0.w);
}

cbuffer cbConstantsLPVinitialize3 : register( b5 )
{
    int g_numCols        : packoffset(c0.x);    //the number of columns in the flattened 2D LPV
    int g_numRows        : packoffset(c0.y);      //the number of columns in the flattened 2D LPV
    int LPV2DWidth       : packoffset(c0.z);   //the total width of the flattened 2D LPV
    int LPV2DHeight      : packoffset(c0.w);   //the total height of the flattened 2D LPV

    int LPV3DWidth       : packoffset(c1.x);     //the width of the LPV in 3D
    int LPV3DHeight      : packoffset(c1.y);     //the height of the LPV in 3D
    int LPV3DDepth       : packoffset(c1.z);        //the depth of the LPV in 3D
    int g_Accumulate     : packoffset(c1.w);     //bool to choose whether to accumulate the propagated wavefront into the accumulation buffer
}



cbuffer cbConstantsLPVinitialize2 : register( b1 )
{
    matrix  g_ViewToLPV        : packoffset( c0 );
    matrix  g_LPVtoView        : packoffset( c4 );
    float displacement         : packoffset( c8.x );   //amount to displace the VPL in the direction of the cell normal    
}


struct propagateValues
{
    float4 neighborOffsets;
    float solidAngle;
    float x;
    float y;
    float z;
};

struct propagateValues2
{
    int occlusionOffsetX;
    int occlusionOffsetY;
    int occlusionOffsetZ;
    int occlusionOffsetW;

    int multiBounceOffsetX;
    int multiBounceOffsetY;
    int multiBounceOffsetZ;
    int multiBounceOffsetW;
};

//these precomputed arrays let us save time on the propapation step
//only the first 30 values from each array are used (the other two are used for alignment/padding)
//xyz give the normalized direction to each of the 5 pertinent faces of the 6 neighbors of a cube
//solid angle gives the fractional solid angle that that face subtends
cbuffer cbLPVPropagateGather : register( b2 )
{
    propagateValues g_propagateValues[36];
};

cbuffer cbLPVPropagateGather2 : register( b6 )
{
    propagateValues2 g_propagateValues2[8];
};


cbuffer cbGV : register( b3 )
{
    int g_useGVOcclusion;
    int temp;
    int g_useMultipleBounces;
    int g_copyPropToAccum;

    float fluxAmplifier;//4
    float reflectedLightAmplifier; //4.8
    float occlusionAmplifier; //0.8f
    int temp2;

}


struct initLPV_VSOUT
{
    float4 pos : POSITION; // 2D slice vertex coordinates in homogenous clip space
    float3 normal : NORMAL;
    float3 color : COLOR;
};

struct initLPV_GSOUT
{
    float4 pos : SV_Position; // 2D slice vertex coordinates in homogenous clip space
    float3 normal : NORMAL;
    float3 color : COLOR;
    uint RTIndex : SV_RenderTargetArrayIndex;  // used to choose the destination slice
};




//--------------------------------------------------------
//helper routines for SH
//--------------------------------------------------------
#define PI 3.14159265


void SH(float3 xyz, inout float4 SHCoefficients)
{
    SHCoefficients.x =  0.282094792;     
    SHCoefficients.y = -0.4886025119*xyz.y;
    SHCoefficients.z =  0.4886025119*xyz.z;
    SHCoefficients.w = -0.4886025119*xyz.x;
}

float4 clampledCosineCoeff(float3 xyz)
{
    float4 c;
    c.x = PI * 0.282094792;
    c.y = ((2.0*PI)/3.0f) * -0.4886025119 * xyz.y;
    c.z = ((2.0*PI)/3.0f) *  0.4886025119 * xyz.z;
    c.w = ((2.0*PI)/3.0f) * -0.4886025119 * xyz.x;
    return c;
}

float innerProductSH(float4 SH1, float4 SH2)
{
    return SH1.x*SH2.x + SH1.y*SH2.y + SH1.z*SH2.z + SH1.w*SH2.w;
}


//----------------------------------------------------------
//helper function for loading texture
//----------------------------------------------------------

float4 loadOffsetTexValue(Texture2DArray tex, uint3 pos)
{
    if(pos.x >= LPV3DWidth  || pos.x < 1) return float4(0,0,0,0);
    if(pos.y >= LPV3DHeight || pos.y < 1) return float4(0,0,0,0);
    if(pos.z >= LPV3DDepth  || pos.z < 1) return float4(0,0,0,0);

    return tex.Load(int4(pos.x,pos.y,pos.z,0));
}

void loadOffsetTexValues(Texture2DArray tex1, Texture2DArray tex2, Texture2DArray tex3,
uint3 pos, inout float4 val1, inout float4 val2, inout float4 val3 )
{
    if(pos.x >= LPV3DWidth  || pos.x < 0) return;
    if(pos.y >= LPV3DHeight || pos.y < 0) return;
    if(pos.z >= LPV3DDepth  || pos.z < 0) return;

    val1 = tex1.Load(int4(pos.x,pos.y,pos.z,0));
    val2 = tex2.Load(int4(pos.x,pos.y,pos.z,0));
    val3 = tex3.Load(int4(pos.x,pos.y,pos.z,0));
}



//--------------------------------------------------------
//propagate compute shader
//--------------------------------------------------------

[numthreads(X_BLOCK_SIZE,Y_BLOCK_SIZE,Z_BLOCK_SIZE)]
void PropagateLPV(uint3 threadgroupId  :  SV_GroupThreadID,
                  uint threadId        : SV_GroupIndex,
                  uint3 groupId        : SV_GroupID,
                  uint3 globalThreadId : SV_DispatchThreadID)
{

    //find where in the 2d texture that grid point is supposed to land
    bool outside = false;
    if(globalThreadId.x>=LPV3DWidth || globalThreadId.y>=LPV3DHeight || globalThreadId.z>=LPV3DDepth) outside = true;

    int4 readIndex = int4(globalThreadId.x,globalThreadId.y,globalThreadId.z,0);
    uint3 writeIndex = globalThreadId.xyz;

    float3 offsets[6];
    offsets[0] = float3(0,0,1);
    offsets[1] = float3(1,0,0);
    offsets[2] = float3(0,0,-1);
    offsets[3] = float3(-1,0,0);
    offsets[4] = float3(0,1,0);
    offsets[5] = float3(0,-1,0);


    float4 faceCoeffs[6];
    faceCoeffs[0] = clampledCosineCoeff(offsets[0]);
    faceCoeffs[1] = clampledCosineCoeff(offsets[1]);
    faceCoeffs[2] = clampledCosineCoeff(offsets[2]);
    faceCoeffs[3] = clampledCosineCoeff(offsets[3]);
    faceCoeffs[4] = clampledCosineCoeff(offsets[4]);
    faceCoeffs[5] = clampledCosineCoeff(offsets[5]);



    float4 SHCoefficientsRed = float4(0,0,0,0);
    float4 SHCoefficientsGreen = float4(0,0,0,0);
    float4 SHCoefficientsBlue = float4(0,0,0,0);

    float4 GV[8];
    float4 BC[8];
    GV[0] = GV[1] = GV[2] = GV[3] = GV[4] = GV[5] = GV[6] = GV[7] = float4(0,0,0,0);
    BC[0] = BC[1] = BC[2] = BC[3] = BC[4] = BC[5] = BC[6] = BC[7] = float4(0,0,0,0); 

    
    if(g_useGVOcclusion || g_useMultipleBounces)
    {
        GV[0] = loadOffsetTexValue(g_txGV, globalThreadId); //GV_z_z_z
        GV[1] = loadOffsetTexValue(g_txGV, globalThreadId + float3(0,0,-1) ); //GV_z_z_m1
        GV[2] = loadOffsetTexValue(g_txGV, globalThreadId + float3(0,-1,0) ); //GV_z_m1_z
        GV[3] = loadOffsetTexValue(g_txGV, globalThreadId + float3(-1,0,0) ); //GV_m1_z_z
        GV[4] = loadOffsetTexValue(g_txGV, globalThreadId + float3(-1,-1,0) ); //GV_m1_m1_z
        GV[5] = loadOffsetTexValue(g_txGV, globalThreadId + float3(-1,0,-1) ); //GV_m1_z_m1
        GV[6] = loadOffsetTexValue(g_txGV, globalThreadId + float3(0,-1,-1) ); //GV_z_m1_m1
        GV[7] = loadOffsetTexValue(g_txGV, globalThreadId + float3(-1,-1,-1) ); //GV_m1_m1_m1
       
       #ifdef USE_MULTIPLE_BOUNCES
        BC[0] = loadOffsetTexValue(g_txGVColor, globalThreadId); //BC_z_z_z
        BC[1] = loadOffsetTexValue(g_txGVColor, globalThreadId + float3(0,0,-1) ); //BC_z_z_m1
        BC[2] = loadOffsetTexValue(g_txGVColor, globalThreadId + float3(0,-1,0) ); //BC_z_m1_z
        BC[3] = loadOffsetTexValue(g_txGVColor, globalThreadId + float3(-1,0,0) ); //BC_m1_z_z
        BC[4] = loadOffsetTexValue(g_txGVColor, globalThreadId + float3(-1,-1,0) ); //BC_m1_m1_z
        BC[5] = loadOffsetTexValue(g_txGVColor, globalThreadId + float3(-1,0,-1) ); //BC_m1_z_m1
        BC[6] = loadOffsetTexValue(g_txGVColor, globalThreadId + float3(0,-1,-1) ); //BC_z_m1_m1
        BC[7] = loadOffsetTexValue(g_txGVColor, globalThreadId + float3(-1,-1,-1) ); //BC_m1_m1_m1       
       #endif
    }

    int index = 0;
    for(int neighbor=0;neighbor<6;neighbor++)
    {
        float4 inSHCoefficientsRed = float4(0,0,0,0);
        float4 inSHCoefficientsGreen = float4(0,0,0,0);
        float4 inSHCoefficientsBlue = float4(0,0,0,0);

        float4 neighborOffset = g_propagateValues[neighbor*6].neighborOffsets;

        //load the light value in the neighbor cell
        loadOffsetTexValues(g_txLPVRed, g_txLPVGreen, g_txLPVBlue, globalThreadId + neighborOffset.xyz, inSHCoefficientsRed, inSHCoefficientsGreen, inSHCoefficientsBlue );

        #ifdef USE_MULTIPLE_BOUNCES
        float4 GVSHReflectCoefficients = float4(0,0,0,0);
        float4 GVReflectanceColor = float4(0,0,0,0);
        float4 wSH;
        if(g_useMultipleBounces)
        {
            SH( float3(neighborOffset.x,neighborOffset.y,neighborOffset.z),wSH );
            
            if(neighbor==1)//if(neighborOffset.x>0)
            { 
                 GVSHReflectCoefficients = (GV[7] + GV[5] + GV[4] + GV[3])*0.25;
                 GVReflectanceColor =      (BC[7] + BC[5] + BC[4] + BC[3])*0.25;
            }
            else if(neighbor==3)//else if(neighborOffset.x<0) 
            {
                GVSHReflectCoefficients = (GV[6] + GV[1] + GV[2] + GV[0])*0.25;
                GVReflectanceColor =      (BC[6] + BC[1] + BC[2] + BC[0])*0.25;
            }
            else if(neighbor==4)//else if(neighborOffset.y>0) 
            {
                GVSHReflectCoefficients = (GV[2] + GV[4] + GV[6] + GV[7])*0.25;
                GVReflectanceColor =      (BC[2] + BC[4] + BC[6] + BC[7])*0.25;
            }
            else if(neighbor==5)//else if(neighborOffset.y<0) 
            {
                GVSHReflectCoefficients = (GV[0] + GV[3] + GV[1] + GV[5])*0.25;
                GVReflectanceColor =      (BC[0] + BC[3] + BC[1] + BC[5])*0.25;
            }
            else if(neighbor==0)//else if(neighborOffset.z>0)
            { 
                GVSHReflectCoefficients = (GV[1] + GV[5] + GV[6] + GV[7])*0.25;
                GVReflectanceColor =      (BC[1] + BC[5] + BC[6] + BC[7])*0.25;
            }
            else if(neighbor==2)//else if(neighborOffset.z<0)
            {
                GVSHReflectCoefficients = (GV[0] + GV[3] + GV[2] + GV[4])*0.25;
                GVReflectanceColor =      (BC[0] + BC[3] + BC[2] + BC[4])*0.25;
            }        
        }
        #endif

        //find what the occlusion probability is in this given direction
        float4 GVSHCoefficients = float4(0,0,0,0);
        if(g_useGVOcclusion)
        {    
            if(neighbor==1)//if(neighborOffset.x>0)
            { 
                 GVSHCoefficients = (GV[6] + GV[1] + GV[2] + GV[0])*0.25;
            }
            else if(neighbor==3)//else if(neighborOffset.x<0) 
            {
                GVSHCoefficients = (GV[7] + GV[5] + GV[4] + GV[3])*0.25;
            }
            else if(neighbor==4)//else if(neighborOffset.y>0) 
            {
                GVSHCoefficients = (GV[0] + GV[3] + GV[1] + GV[5])*0.25;
            }
            else if(neighbor==5)//else if(neighborOffset.y<0) 
            {
                GVSHCoefficients = (GV[2] + GV[4] + GV[6] + GV[7])*0.25;
            }
            else if(neighbor==0)//else if(neighborOffset.z>0)
            { 
                GVSHCoefficients = (GV[0] + GV[3] + GV[2] + GV[4])*0.25;
            }
            else if(neighbor==2)//else if(neighborOffset.z<0)
            {
                GVSHCoefficients = (GV[1] + GV[5] + GV[6] + GV[7])*0.25;
            }
        }



        for(int face=0;face<6;face++)
        {
            //evaluate the SH approximation of the intensity coming from the neighboring cell to this face
            float3 dir;
            dir.x = g_propagateValues[index].x;
            dir.y = g_propagateValues[index].y;
            dir.z = g_propagateValues[index].z;
            dir = normalize(dir);
            float solidAngle = g_propagateValues[index].solidAngle;

            float4 dirSH;
            SH(dir,dirSH);

            float occlusion = 1.0f;
            //find the probability of occluding light 
            if(g_useGVOcclusion)
                occlusion = 1.0 - max(0,min(1.0f, occlusionAmplifier*innerProductSH(GVSHCoefficients , dirSH )));

            float inRedFlux = 0;
            float inGreenFlux = 0;
            float inBlueFlux = 0;

            {
                //approximate our SH coefficients in the direction dir.
                //to do this we sum the product of the stored SH coefficients with the SH basis function in the direction dir
                float redFlux = occlusion * solidAngle *    max(0,(inSHCoefficientsRed.x  *dirSH.x + inSHCoefficientsRed.y  *dirSH.y + inSHCoefficientsRed.z  *dirSH.z + inSHCoefficientsRed.w  *dirSH.w));
                float greenFlux = occlusion * solidAngle * max(0,(inSHCoefficientsGreen.x*dirSH.x + inSHCoefficientsGreen.y*dirSH.y + inSHCoefficientsGreen.z*dirSH.z + inSHCoefficientsGreen.w*dirSH.w)); 
                float blueFlux = occlusion * solidAngle *    max(0,(inSHCoefficientsBlue.x *dirSH.x + inSHCoefficientsBlue.y *dirSH.y + inSHCoefficientsBlue.z *dirSH.z + inSHCoefficientsBlue.w *dirSH.w)); 
                 
                inRedFlux += redFlux;
                inGreenFlux += greenFlux;
                inBlueFlux += blueFlux;
                
                
                #ifdef USE_MULTIPLE_BOUNCES
                float blockerDiffuseIllumination = innerProductSH(GVSHReflectCoefficients , wSH );
                if(blockerDiffuseIllumination>0.0f && g_useMultipleBounces)
                {
                    float weightedRedFlux =   blockerDiffuseIllumination * redFlux * reflectedLightAmplifier * GVReflectanceColor.r;
                    float weightedGreenFlux = blockerDiffuseIllumination * greenFlux * reflectedLightAmplifier * GVReflectanceColor.g;
                    float weightedBlueFlux =  blockerDiffuseIllumination * blueFlux * reflectedLightAmplifier * GVReflectanceColor.b;
                                                            
                    SHCoefficientsRed   += float4(weightedRedFlux*GVSHReflectCoefficients.x,weightedRedFlux*GVSHReflectCoefficients.y,weightedRedFlux*GVSHReflectCoefficients.z,weightedRedFlux*GVSHReflectCoefficients.w);
                    SHCoefficientsGreen   += float4(weightedGreenFlux*GVSHReflectCoefficients.x,weightedGreenFlux*GVSHReflectCoefficients.y,weightedGreenFlux*GVSHReflectCoefficients.z,weightedGreenFlux*GVSHReflectCoefficients.w);
                    SHCoefficientsBlue   += float4(weightedBlueFlux*GVSHReflectCoefficients.x,weightedBlueFlux*GVSHReflectCoefficients.y,weightedBlueFlux*GVSHReflectCoefficients.z,weightedBlueFlux*GVSHReflectCoefficients.w);                                    
                }
                
                #endif
            }
            
            float4 coeffs = faceCoeffs[face];

            inRedFlux *= fluxAmplifier;
            inGreenFlux *= fluxAmplifier;
            inBlueFlux *= fluxAmplifier;

            SHCoefficientsRed   +=  float4(inRedFlux   * coeffs.x, inRedFlux   * coeffs.y, inRedFlux   * coeffs.z, inRedFlux   * coeffs.w);
            SHCoefficientsGreen +=  float4(inGreenFlux * coeffs.x, inGreenFlux * coeffs.y, inGreenFlux * coeffs.z, inGreenFlux * coeffs.w);
            SHCoefficientsBlue  +=  float4(inBlueFlux  * coeffs.x, inBlueFlux  * coeffs.y, inBlueFlux  * coeffs.z, inBlueFlux  * coeffs.w);

            index++;
        }
    }
    
    //write back the updated flux
    if(!outside)
    {
        if(g_copyPropToAccum) 
        {
            SHCoefficientsRed   +=  g_txLPVRed.Load(readIndex);
            SHCoefficientsGreen +=  g_txLPVGreen.Load(readIndex);
            SHCoefficientsBlue  +=  g_txLPVBlue.Load(readIndex);    
        }
        g_uavLPVRed[writeIndex] = SHCoefficientsRed; 
        g_uavLPVGreen[writeIndex] = SHCoefficientsGreen; 
        g_uavLPVBlue[writeIndex] = SHCoefficientsBlue;

        if(g_Accumulate)
        {
            g_uavLPVRedAccum[writeIndex] =  g_txAccumulateRed.Load(readIndex) + SHCoefficientsRed;
            g_uavLPVGreenAccum[writeIndex] = g_txAccumulateGreen.Load(readIndex) + SHCoefficientsGreen;    
            g_uavLPVBlueAccum[writeIndex] = g_txAccumulateBlue.Load(readIndex) + SHCoefficientsBlue;    
        }
    }
    
}    


//a simpler version of the shader above, with no path for multiple bounces or occlusion
[numthreads(X_BLOCK_SIZE,Y_BLOCK_SIZE,Z_BLOCK_SIZE)]
void PropagateLPV_Simple(uint3 threadgroupId  :  SV_GroupThreadID,
                         uint threadId        : SV_GroupIndex,
                         uint3 groupId        : SV_GroupID,
                         uint3 globalThreadId : SV_DispatchThreadID)
{

    //find where in the 2d texture that grid point is supposed to land
    bool outside = false;
    if(globalThreadId.x>=LPV3DWidth || globalThreadId.y>=LPV3DHeight || globalThreadId.z>=LPV3DDepth) outside = true;

    int4 readIndex = int4(globalThreadId.x,globalThreadId.y,globalThreadId.z,0);
    uint3 writeIndex = globalThreadId.xyz;

    float3 offsets[6];
    offsets[0] = float3(0,0,1);
    offsets[1] = float3(1,0,0);
    offsets[2] = float3(0,0,-1);
    offsets[3] = float3(-1,0,0);
    offsets[4] = float3(0,1,0);
    offsets[5] = float3(0,-1,0);


    float4 faceCoeffs[6];
    faceCoeffs[0] = clampledCosineCoeff(offsets[0]);
    faceCoeffs[1] = clampledCosineCoeff(offsets[1]);
    faceCoeffs[2] = clampledCosineCoeff(offsets[2]);
    faceCoeffs[3] = clampledCosineCoeff(offsets[3]);
    faceCoeffs[4] = clampledCosineCoeff(offsets[4]);
    faceCoeffs[5] = clampledCosineCoeff(offsets[5]);

    float4 SHCoefficientsRed = float4(0,0,0,0);
    float4 SHCoefficientsGreen = float4(0,0,0,0);
    float4 SHCoefficientsBlue = float4(0,0,0,0);


    int index = 0;
    for(int neighbor=0;neighbor<6;neighbor++)
    {
        float4 inSHCoefficientsRed = float4(0,0,0,0);
        float4 inSHCoefficientsGreen = float4(0,0,0,0);
        float4 inSHCoefficientsBlue = float4(0,0,0,0);

        float4 neighborOffset = g_propagateValues[neighbor*6].neighborOffsets;

        //load the light value in the neighbor cell
        loadOffsetTexValues(g_txLPVRed, g_txLPVGreen, g_txLPVBlue, globalThreadId + neighborOffset.xyz, inSHCoefficientsRed, inSHCoefficientsGreen, inSHCoefficientsBlue );

        for(int face=0;face<6;face++)
        {
            //evaluate the SH approximation of the intensity coming from the neighboring cell to this face
            float3 dir;
            dir.x = g_propagateValues[index].x;
            dir.y = g_propagateValues[index].y;
            dir.z = g_propagateValues[index].z;
            dir = normalize(dir);
            float solidAngle = g_propagateValues[index].solidAngle;

            float4 dirSH;
            SH(dir,dirSH);


            float inRedFlux = 0;
            float inGreenFlux = 0;
            float inBlueFlux = 0;

            //approximate our SH coefficients in the direction dir.
            //to do this we sum the product of the stored SH coefficients with the SH basis function in the direction dir
            float redFlux =   solidAngle *    max(0,(inSHCoefficientsRed.x  *dirSH.x + inSHCoefficientsRed.y  *dirSH.y + inSHCoefficientsRed.z  *dirSH.z + inSHCoefficientsRed.w  *dirSH.w));
            float greenFlux = solidAngle * max(0,(inSHCoefficientsGreen.x*dirSH.x + inSHCoefficientsGreen.y*dirSH.y + inSHCoefficientsGreen.z*dirSH.z + inSHCoefficientsGreen.w*dirSH.w)); 
            float blueFlux =  solidAngle *    max(0,(inSHCoefficientsBlue.x *dirSH.x + inSHCoefficientsBlue.y *dirSH.y + inSHCoefficientsBlue.z *dirSH.z + inSHCoefficientsBlue.w *dirSH.w)); 
             
            inRedFlux += redFlux;
            inGreenFlux += greenFlux;
            inBlueFlux += blueFlux;
            
            float4 coeffs = faceCoeffs[face];

            inRedFlux *= fluxAmplifier;
            inGreenFlux *= fluxAmplifier;
            inBlueFlux *= fluxAmplifier;

            SHCoefficientsRed   +=  float4(inRedFlux   * coeffs.x, inRedFlux   * coeffs.y, inRedFlux   * coeffs.z, inRedFlux   * coeffs.w);
            SHCoefficientsGreen +=  float4(inGreenFlux * coeffs.x, inGreenFlux * coeffs.y, inGreenFlux * coeffs.z, inGreenFlux * coeffs.w);
            SHCoefficientsBlue  +=  float4(inBlueFlux  * coeffs.x, inBlueFlux  * coeffs.y, inBlueFlux  * coeffs.z, inBlueFlux  * coeffs.w);

            index++;
        }
    }
    
    //write back the updated flux
    if(!outside)
    {
        if(g_copyPropToAccum) 
        {
            SHCoefficientsRed   +=  g_txLPVRed.Load(readIndex);
            SHCoefficientsGreen +=  g_txLPVGreen.Load(readIndex);
            SHCoefficientsBlue  +=  g_txLPVBlue.Load(readIndex);    
        }
        g_uavLPVRed[writeIndex] = SHCoefficientsRed; 
        g_uavLPVGreen[writeIndex] = SHCoefficientsGreen; 
        g_uavLPVBlue[writeIndex] = SHCoefficientsBlue;

        if(g_Accumulate)
        {
            g_uavLPVRedAccum[writeIndex] =  g_txAccumulateRed.Load(readIndex) + SHCoefficientsRed;
            g_uavLPVGreenAccum[writeIndex] = g_txAccumulateGreen.Load(readIndex) + SHCoefficientsGreen;    
            g_uavLPVBlueAccum[writeIndex] = g_txAccumulateBlue.Load(readIndex) + SHCoefficientsBlue;    
        }
    }
    
}    



//--------------------------------------------------------
//propagate vertex, geometry and pixel shader
//--------------------------------------------------------

struct VS_PROP_OUTPUT
{
    float4 pos : POSITION; // 2D slice vertex coordinates in homogenous clip space
    float3 tex : MIAU;
};

struct GS_PROP_OUTPUT
{
    float4 pos   : SV_Position; // 2D slice vertex coordinates in homogenous clip space
    float3 tex   : MIAU;
    uint RTIndex : SV_RenderTargetArrayIndex;  // used to choose the destination slice
};



VS_PROP_OUTPUT PropagateLPV_VS(float3 Pos : POSITION, float3 Tex : TEXCOORD)
{
    VS_PROP_OUTPUT f;
    f.pos = float4(Pos.x, Pos.y, Pos.z, 1.0f);
    f.tex = Tex;
    return f;
}

//only needed for 3D texture and 2d texture array options
[maxvertexcount (3)]
void PropagateLPV_GS(triangle VS_PROP_OUTPUT In[3], inout TriangleStream<GS_PROP_OUTPUT> triStream)
{
    for(int v=0; v<3; v++)
    {   GS_PROP_OUTPUT Out;
        Out.RTIndex         = In[v].tex.z;
        Out.pos          = In[v].pos;
        Out.tex          = In[v].tex;
        triStream.Append( Out );
    }
    triStream.RestartStrip();
}


struct PS_PROP_OUTPUT
{
    float4 RedOut    : SV_Target0;
    float4 GreenOut : SV_Target1;
    float4 BlueOut    : SV_Target2;

    float4 RedAccumOut    : SV_Target3;
    float4 GreenAccumOut: SV_Target4;
    float4 BlueAccumOut    : SV_Target5;
};




PS_PROP_OUTPUT PropagateLPV_PS(GS_PROP_OUTPUT input) : SV_Target
{

    uint3 globalThreadId = uint3(input.pos.x, input.pos.y, input.tex.z); 
    int4 readIndex = int4(globalThreadId.x,globalThreadId.y,globalThreadId.z,0);


    float3 offsets[6];
    offsets[0] = float3(0,0,1);
    offsets[1] = float3(1,0,0);
    offsets[2] = float3(0,0,-1);
    offsets[3] = float3(-1,0,0);
    offsets[4] = float3(0,1,0);
    offsets[5] = float3(0,-1,0);


    float4 faceCoeffs[6];
    faceCoeffs[0] = clampledCosineCoeff(offsets[0]);
    faceCoeffs[1] = clampledCosineCoeff(offsets[1]);
    faceCoeffs[2] = clampledCosineCoeff(offsets[2]);
    faceCoeffs[3] = clampledCosineCoeff(offsets[3]);
    faceCoeffs[4] = clampledCosineCoeff(offsets[4]);
    faceCoeffs[5] = clampledCosineCoeff(offsets[5]);



    float4 SHCoefficientsRed = float4(0,0,0,0);
    float4 SHCoefficientsGreen = float4(0,0,0,0);
    float4 SHCoefficientsBlue = float4(0,0,0,0);

    float4 GV[8];
    float4 BC[8];
    GV[0] = GV[1] = GV[2] = GV[3] = GV[4] = GV[5] = GV[6] = GV[7] = float4(0,0,0,0);
    BC[0] = BC[1] = BC[2] = BC[3] = BC[4] = BC[5] = BC[6] = BC[7] = float4(0,0,0,0); 

    
    if(g_useGVOcclusion || g_useMultipleBounces)
    {
        GV[0] = loadOffsetTexValue(g_txGV, globalThreadId); //GV_z_z_z
        GV[1] = loadOffsetTexValue(g_txGV, globalThreadId + float3(0,0,-1) ); //GV_z_z_m1
        GV[2] = loadOffsetTexValue(g_txGV, globalThreadId + float3(0,-1,0) ); //GV_z_m1_z
        GV[3] = loadOffsetTexValue(g_txGV, globalThreadId + float3(-1,0,0) ); //GV_m1_z_z
        GV[4] = loadOffsetTexValue(g_txGV, globalThreadId + float3(-1,-1,0) ); //GV_m1_m1_z
        GV[5] = loadOffsetTexValue(g_txGV, globalThreadId + float3(-1,0,-1) ); //GV_m1_z_m1
        GV[6] = loadOffsetTexValue(g_txGV, globalThreadId + float3(0,-1,-1) ); //GV_z_m1_m1
        GV[7] = loadOffsetTexValue(g_txGV, globalThreadId + float3(-1,-1,-1) ); //GV_m1_m1_m1
       
       #ifdef USE_MULTIPLE_BOUNCES
        BC[0] = loadOffsetTexValue(g_txGVColor, globalThreadId); //BC_z_z_z
        BC[1] = loadOffsetTexValue(g_txGVColor, globalThreadId + float3(0,0,-1) ); //BC_z_z_m1
        BC[2] = loadOffsetTexValue(g_txGVColor, globalThreadId + float3(0,-1,0) ); //BC_z_m1_z
        BC[3] = loadOffsetTexValue(g_txGVColor, globalThreadId + float3(-1,0,0) ); //BC_m1_z_z
        BC[4] = loadOffsetTexValue(g_txGVColor, globalThreadId + float3(-1,-1,0) ); //BC_m1_m1_z
        BC[5] = loadOffsetTexValue(g_txGVColor, globalThreadId + float3(-1,0,-1) ); //BC_m1_z_m1
        BC[6] = loadOffsetTexValue(g_txGVColor, globalThreadId + float3(0,-1,-1) ); //BC_z_m1_m1
        BC[7] = loadOffsetTexValue(g_txGVColor, globalThreadId + float3(-1,-1,-1) ); //BC_m1_m1_m1       
       #endif
    }


    int index = 0;
    for(int neighbor=0;neighbor<6;neighbor++)
    {
        float4 inSHCoefficientsRed = float4(0,0,0,0);
        float4 inSHCoefficientsGreen = float4(0,0,0,0);
        float4 inSHCoefficientsBlue = float4(0,0,0,0);

        float4 neighborOffset = g_propagateValues[neighbor*6].neighborOffsets;

        //load the light value in the neighbor cell
        loadOffsetTexValues(g_txLPVRed, g_txLPVGreen, g_txLPVBlue, globalThreadId + neighborOffset.xyz, inSHCoefficientsRed, inSHCoefficientsGreen, inSHCoefficientsBlue );

        #ifdef USE_MULTIPLE_BOUNCES
        float4 GVSHReflectCoefficients = float4(0,0,0,0);
        float4 GVReflectanceColor = float4(0,0,0,0);
        float4 wSH;
        if(g_useMultipleBounces)
        {
            SH( float3(neighborOffset.x,neighborOffset.y,neighborOffset.z),wSH );
            
            if(neighbor==1)//if(neighborOffset.x>0)
            { 
                 GVSHReflectCoefficients = (GV[7] + GV[5] + GV[4] + GV[3])*0.25;
                 GVReflectanceColor =      (BC[7] + BC[5] + BC[4] + BC[3])*0.25;
            }
            else if(neighbor==3)//else if(neighborOffset.x<0) 
            {
                GVSHReflectCoefficients = (GV[6] + GV[1] + GV[2] + GV[0])*0.25;
                GVReflectanceColor =      (BC[6] + BC[1] + BC[2] + BC[0])*0.25;                    
            }
            else if(neighbor==4)//else if(neighborOffset.y>0) 
            {
                GVSHReflectCoefficients = (GV[2] + GV[4] + GV[6] + GV[7])*0.25;
                GVReflectanceColor =      (BC[2] + BC[4] + BC[6] + BC[7])*0.25;
            }
            else if(neighbor==5)//else if(neighborOffset.y<0) 
            {
                GVSHReflectCoefficients = (GV[0] + GV[3] + GV[1] + GV[5])*0.25;
                GVReflectanceColor =      (BC[0] + BC[3] + BC[1] + BC[5])*0.25;
            }
            else if(neighbor==0)//else if(neighborOffset.z>0)
            { 
                GVSHReflectCoefficients = (GV[1] + GV[5] + GV[6] + GV[7])*0.25;
                GVReflectanceColor =      (BC[1] + BC[5] + BC[6] + BC[7])*0.25;
            }
            else if(neighbor==2)//else if(neighborOffset.z<0)
            {
                GVSHReflectCoefficients = (GV[0] + GV[3] + GV[2] + GV[4])*0.25;
                GVReflectanceColor =      (BC[0] + BC[3] + BC[2] + BC[4])*0.25;
            }        
        }
        #endif

        //find what the occlusion probability is in this given direction
        float4 GVSHCoefficients = float4(0,0,0,0);
        if(g_useGVOcclusion)
        {    
            if(neighbor==1)//if(neighborOffset.x>0)
            { 
                 GVSHCoefficients = (GV[6] + GV[1] + GV[2] + GV[0])*0.25;
            }
            else if(neighbor==3)//else if(neighborOffset.x<0) 
            {
                GVSHCoefficients = (GV[7] + GV[5] + GV[4] + GV[3])*0.25;
            }
            else if(neighbor==4)//else if(neighborOffset.y>0) 
            {
                GVSHCoefficients = (GV[0] + GV[3] + GV[1] + GV[5])*0.25;
            }
            else if(neighbor==5)//else if(neighborOffset.y<0) 
            {
                GVSHCoefficients = (GV[2] + GV[4] + GV[6] + GV[7])*0.25;
            }
            else if(neighbor==0)//else if(neighborOffset.z>0)
            { 
                GVSHCoefficients = (GV[0] + GV[3] + GV[2] + GV[4])*0.25;
            }
            else if(neighbor==2)//else if(neighborOffset.z<0)
            {
                GVSHCoefficients = (GV[1] + GV[5] + GV[6] + GV[7])*0.25;
            }
        }


        for(int face=0;face<6;face++)
        {
            //evaluate the SH approximation of the intensity coming from the neighboring cell to this face
            float3 dir;
            dir.x = g_propagateValues[index].x;
            dir.y = g_propagateValues[index].y;
            dir.z = g_propagateValues[index].z;
            dir = normalize(dir);
            float solidAngle = g_propagateValues[index].solidAngle;

            float4 dirSH;
            SH(dir,dirSH);

            float occlusion = 1.0f;
            //find the probability of occluding light 
            if(g_useGVOcclusion)
                occlusion = 1.0 - max(0,min(1.0f, occlusionAmplifier*innerProductSH(GVSHCoefficients , dirSH )));

            float inRedFlux = 0;
            float inGreenFlux = 0;
            float inBlueFlux = 0;

            {
                //approximate our SH coefficients in the direction dir.
                //to do this we sum the product of the stored SH coefficients with the SH basis function in the direction dir
                float redFlux = occlusion * solidAngle *    max(0,(inSHCoefficientsRed.x  *dirSH.x + inSHCoefficientsRed.y  *dirSH.y + inSHCoefficientsRed.z  *dirSH.z + inSHCoefficientsRed.w  *dirSH.w));
                float greenFlux = occlusion * solidAngle * max(0,(inSHCoefficientsGreen.x*dirSH.x + inSHCoefficientsGreen.y*dirSH.y + inSHCoefficientsGreen.z*dirSH.z + inSHCoefficientsGreen.w*dirSH.w)); 
                float blueFlux = occlusion * solidAngle *    max(0,(inSHCoefficientsBlue.x *dirSH.x + inSHCoefficientsBlue.y *dirSH.y + inSHCoefficientsBlue.z *dirSH.z + inSHCoefficientsBlue.w *dirSH.w)); 
                 
                inRedFlux += redFlux;
                inGreenFlux += greenFlux;
                inBlueFlux += blueFlux;
                
                
                #ifdef USE_MULTIPLE_BOUNCES
                float blockerDiffuseIllumination = innerProductSH(GVSHReflectCoefficients , wSH );
                if(blockerDiffuseIllumination>0.0f && g_useMultipleBounces)
                {
                    float weightedRedFlux =   blockerDiffuseIllumination * redFlux * reflectedLightAmplifier * GVReflectanceColor.r;
                    float weightedGreenFlux = blockerDiffuseIllumination * greenFlux * reflectedLightAmplifier * GVReflectanceColor.g;
                    float weightedBlueFlux =  blockerDiffuseIllumination * blueFlux * reflectedLightAmplifier * GVReflectanceColor.b;
                                                            
                    SHCoefficientsRed   += float4(weightedRedFlux*GVSHReflectCoefficients.x,weightedRedFlux*GVSHReflectCoefficients.y,weightedRedFlux*GVSHReflectCoefficients.z,weightedRedFlux*GVSHReflectCoefficients.w);
                    SHCoefficientsGreen   += float4(weightedGreenFlux*GVSHReflectCoefficients.x,weightedGreenFlux*GVSHReflectCoefficients.y,weightedGreenFlux*GVSHReflectCoefficients.z,weightedGreenFlux*GVSHReflectCoefficients.w);
                    SHCoefficientsBlue   += float4(weightedBlueFlux*GVSHReflectCoefficients.x,weightedBlueFlux*GVSHReflectCoefficients.y,weightedBlueFlux*GVSHReflectCoefficients.z,weightedBlueFlux*GVSHReflectCoefficients.w);                                    
                }
                
                #endif
            }
            
            float4 coeffs = faceCoeffs[face];

            inRedFlux *= fluxAmplifier;
            inGreenFlux *= fluxAmplifier;
            inBlueFlux *= fluxAmplifier;

            SHCoefficientsRed   +=  float4(inRedFlux   * coeffs.x, inRedFlux   * coeffs.y, inRedFlux   * coeffs.z, inRedFlux   * coeffs.w);
            SHCoefficientsGreen +=  float4(inGreenFlux * coeffs.x, inGreenFlux * coeffs.y, inGreenFlux * coeffs.z, inGreenFlux * coeffs.w);
            SHCoefficientsBlue  +=  float4(inBlueFlux  * coeffs.x, inBlueFlux  * coeffs.y, inBlueFlux  * coeffs.z, inBlueFlux  * coeffs.w);

            index++;
        }
    }
    
    //write back the updated flux
    if(g_copyPropToAccum) 
    {
        SHCoefficientsRed   +=  g_txLPVRed.Load(readIndex);
        SHCoefficientsGreen +=  g_txLPVGreen.Load(readIndex);
        SHCoefficientsBlue  +=  g_txLPVBlue.Load(readIndex);    
    }

    PS_PROP_OUTPUT output;
    
    output.RedOut = SHCoefficientsRed;
    output.GreenOut = SHCoefficientsGreen;
    output.BlueOut = SHCoefficientsBlue;
    
    float4 AccumSHCoefficientsRed, AccumSHCoefficientsGreen, AccumSHCoefficientsBlue;
    output.RedAccumOut = SHCoefficientsRed;
    output.GreenAccumOut = SHCoefficientsGreen;
    output.BlueAccumOut = SHCoefficientsBlue;

    return output;
}


//a simpler version of the shader above, which does not use multiple bounces or occlusion
PS_PROP_OUTPUT PropagateLPV_PS_Simple(GS_PROP_OUTPUT input) : SV_Target
{
    uint3 globalThreadId = uint3(input.pos.x, input.pos.y, input.tex.z); 
    int4 readIndex = int4(globalThreadId.x,globalThreadId.y,globalThreadId.z,0);

    float3 offsets[6];
    offsets[0] = float3(0,0,1);
    offsets[1] = float3(1,0,0);
    offsets[2] = float3(0,0,-1);
    offsets[3] = float3(-1,0,0);
    offsets[4] = float3(0,1,0);
    offsets[5] = float3(0,-1,0);


    float4 faceCoeffs[6];
    faceCoeffs[0] = clampledCosineCoeff(offsets[0]);
    faceCoeffs[1] = clampledCosineCoeff(offsets[1]);
    faceCoeffs[2] = clampledCosineCoeff(offsets[2]);
    faceCoeffs[3] = clampledCosineCoeff(offsets[3]);
    faceCoeffs[4] = clampledCosineCoeff(offsets[4]);
    faceCoeffs[5] = clampledCosineCoeff(offsets[5]);



    float4 SHCoefficientsRed = float4(0,0,0,0);
    float4 SHCoefficientsGreen = float4(0,0,0,0);
    float4 SHCoefficientsBlue = float4(0,0,0,0);
    
    int index = 0;
    for(int neighbor=0;neighbor<6;neighbor++)
    {
        float4 inSHCoefficientsRed = float4(0,0,0,0);
        float4 inSHCoefficientsGreen = float4(0,0,0,0);
        float4 inSHCoefficientsBlue = float4(0,0,0,0);

        float4 neighborOffset = g_propagateValues[neighbor*6].neighborOffsets;

        //load the light value in the neighbor cell
        loadOffsetTexValues(g_txLPVRed, g_txLPVGreen, g_txLPVBlue, globalThreadId + neighborOffset.xyz, inSHCoefficientsRed, inSHCoefficientsGreen, inSHCoefficientsBlue );

        for(int face=0;face<6;face++)
        {
            //evaluate the SH approximation of the intensity coming from the neighboring cell to this face
            float3 dir;
            dir.x = g_propagateValues[index].x;
            dir.y = g_propagateValues[index].y;
            dir.z = g_propagateValues[index].z;
            dir = normalize(dir);
            float solidAngle = g_propagateValues[index].solidAngle;

            float4 dirSH;
            SH(dir,dirSH);

            float inRedFlux = 0;
            float inGreenFlux = 0;
            float inBlueFlux = 0;

            //approximate our SH coefficients in the direction dir.
            //to do this we sum the product of the stored SH coefficients with the SH basis function in the direction dir
            float redFlux =   solidAngle * max(0,(inSHCoefficientsRed.x  *dirSH.x + inSHCoefficientsRed.y  *dirSH.y + inSHCoefficientsRed.z  *dirSH.z + inSHCoefficientsRed.w  *dirSH.w));
            float greenFlux = solidAngle * max(0,(inSHCoefficientsGreen.x*dirSH.x + inSHCoefficientsGreen.y*dirSH.y + inSHCoefficientsGreen.z*dirSH.z + inSHCoefficientsGreen.w*dirSH.w)); 
            float blueFlux =  solidAngle * max(0,(inSHCoefficientsBlue.x *dirSH.x + inSHCoefficientsBlue.y *dirSH.y + inSHCoefficientsBlue.z *dirSH.z + inSHCoefficientsBlue.w *dirSH.w)); 
             
            inRedFlux += redFlux;
            inGreenFlux += greenFlux;
            inBlueFlux += blueFlux;
                            
            float4 coeffs = faceCoeffs[face];

            inRedFlux *= fluxAmplifier;
            inGreenFlux *= fluxAmplifier;
            inBlueFlux *= fluxAmplifier;

            SHCoefficientsRed   +=  float4(inRedFlux   * coeffs.x, inRedFlux   * coeffs.y, inRedFlux   * coeffs.z, inRedFlux   * coeffs.w);
            SHCoefficientsGreen +=  float4(inGreenFlux * coeffs.x, inGreenFlux * coeffs.y, inGreenFlux * coeffs.z, inGreenFlux * coeffs.w);
            SHCoefficientsBlue  +=  float4(inBlueFlux  * coeffs.x, inBlueFlux  * coeffs.y, inBlueFlux  * coeffs.z, inBlueFlux  * coeffs.w);

            index++;
        }
    }
    
    //write back the updated flux
    if(g_copyPropToAccum) 
    {
        SHCoefficientsRed   +=  g_txLPVRed.Load(readIndex);
        SHCoefficientsGreen +=  g_txLPVGreen.Load(readIndex);
        SHCoefficientsBlue  +=  g_txLPVBlue.Load(readIndex);    
    }

    PS_PROP_OUTPUT output;
    
    output.RedOut = SHCoefficientsRed;
    output.GreenOut = SHCoefficientsGreen;
    output.BlueOut = SHCoefficientsBlue;
    
    float4 AccumSHCoefficientsRed, AccumSHCoefficientsGreen, AccumSHCoefficientsBlue;
    output.RedAccumOut = SHCoefficientsRed;
    output.GreenAccumOut = SHCoefficientsGreen;
    output.BlueAccumOut = SHCoefficientsBlue;

    return output;
}