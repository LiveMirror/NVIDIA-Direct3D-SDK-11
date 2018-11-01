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

//textures for initializing the LPV
Texture2D   g_txRSMColor                        : register( t0 );
Texture2D   g_txRSMNormal                       : register( t1 );
Texture2D   g_txRSMDepth                        : register( t2 );

Texture2DArray   g_txLPVRed                     : register( t3 );
Texture2DArray   g_txLPVGreen                   : register( t4 );
Texture2DArray   g_txLPVBlue                    : register( t5 );
RWTexture2DArray<float4>    g_uavLPVRed         : register( u0 );
RWTexture2DArray<float4>    g_uavLPVGreen       : register( u1 );
RWTexture2DArray<float4>    g_uavLPVBlue        : register( u2 );



cbuffer cbConstantsLPVinitialize : register( b0 )
{
    int RSMWidth          : packoffset(c0.x);
    int RSMHeight         : packoffset(c0.y);
    float gLightScale     : packoffset(c0.z);
    float temp7           : packoffset(c0.w);

    matrix  g_mInvProj    : packoffset( c1 );
}

cbuffer cbConstantsLPVinitialize3 : register( b5 )
{
    int g_numCols         : packoffset(c0.x);    //the number of columns in the flattened 2D LPV
    int g_numRows         : packoffset(c0.y);      //the number of columns in the flattened 2D LPV
    int LPV2DWidth        : packoffset(c0.z);   //the total width of the flattened 2D LPV
    int LPV2DHeight       : packoffset(c0.w);   //the total height of the flattened 2D LPV

    int LPV3DWidth        : packoffset(c1.x);      //the width of the LPV in 3D
    int LPV3DHeight       : packoffset(c1.y);     //the height of the LPV in 3D
    int LPV3DDepth        : packoffset(c1.z); 
    float g_fluxWeight    : packoffset(c1.w);

    int g_useFluxWeight   : packoffset(c2.x); //flux weight only needed for perspective light matrix not orthogonal
    float padding0        : packoffset(c2.y);
    float padding1        : packoffset(c2.z);
    float padding2        : packoffset(c2.w);

}

cbuffer cbConstantsLPVinitialize2 : register( b1 )
{
    matrix  g_ViewToLPV        : packoffset( c0 );
    matrix  g_LPVtoView        : packoffset( c4 );
    float3 lightDirGridSpace   : packoffset( c8 );   //light direction in the grid's space    
    float displacement         : packoffset( c8.w );   //amount to displace the VPL in the direction of the cell normal    
}

struct propagateValues
{
    float4 neighborOffsets;
    float solidAngle;
    float x;
    float y;
    float z;
};

//these precomputed arrays let us save time on the propapation step
//only the first 30 values from each array are used (the other two are used for alignment/padding)
//xyz give the normalized direction to each of the 5 pertinent faces of the 6 neighbors of a cube
//solid angle gives the fractional solid angle that that face subtends
cbuffer cbLPVPropagateGather : register( b2 )
{
    propagateValues g_propagateValues[32];
};

cbuffer cbGV : register( b3 )
{
    int g_useGVOcclusion;
    int temp;
    int g_useMultipleBounces;
    int g_copyPropToAccum;
}


cbuffer cbConstantsLPVReconstruct : register( b6 )
{
    float farClip      : packoffset(c0.x);   
    float temp2        : packoffset(c0.y); 
    float temp3        : packoffset(c0.z); 
    float temp4        : packoffset(c0.w); 
};

cbuffer cbConstantsLPVReconstruct2 : register( b7 )
{
    matrix  g_InverseProjection    : packoffset( c0 );
}


SamplerState samLinear : register( s1 )
{ 
    Filter = MIN_MAG_LINEAR_MIP_POINT; 
};



struct initLPV_VSOUT
{
    float4 pos : POSITION; // 2D slice vertex coordinates in homogenous clip space
    float3 normal : NORMAL;
    float3 color : COLOR;
    int BiLinearOffset : BILINEAROFFSET;
    float fluxWeight :FW ;
};


struct initLPV_GSOUT
{
    float4 pos : SV_Position; // 2D slice vertex coordinates in homogenous clip space
    float3 normal : NORMAL;
    float3 color : COLOR;
    float fluxWeight :FW ;
    uint RTIndex : SV_RenderTargetArrayIndex;  // used to choose the destination slice
};




//--------------------------------------------------------
//helper routines for SH
//--------------------------------------------------------
#define PI 3.14159265

float SH(int l, int m, float3 xyz)
{
    if(l==0) return 0.282094792;//1/(2sqrt(PI);
    if(l==1 && m==-1) return -0.4886025119*xyz.y;
    if(l==1 && m==0)  return  0.4886025119*xyz.z;
    if(l==1 && m==1)  return -0.4886025119*xyz.x;
}

//polynomial form of SH basis functions
void SH(float3 xyz, inout float4 SHCoefficients)
{ 
    SHCoefficients.x =  0.282094792;         // 1/(2*sqrt(PI))     : l=0, m=0
    SHCoefficients.y = -0.4886025119*xyz.y;  // - sqrt(3/(4*PI))*y : l=1, m=-1
    SHCoefficients.z =  0.4886025119*xyz.z;  //   sqrt(3/(4*PI))*z : l=1, m=0
    SHCoefficients.w = -0.4886025119*xyz.x;  // - sqrt(3/(4*PI))*x : l=1, m=1
    
    //sqrt(15)*y*x/(2*sqrt(PI))        : l=2, m=-2
    //-sqrt(15)*y*z/(2*sqrt(PI))       : l=2, m=-1
    //sqrt(5)*(3*z*z-1)/(4*sqrt(PI))   : l=2, m=0
    //-sqrt(15)*x*z/(2*sqrt(PI))       : l=2, m=1
    //sqrt(15)*(x*x-y*y)/(4*sqrt(PI))  : l=2, m=2
}

//clamped cosine oriented in z direction expressed in zonal harmonics and rotated to direction 'd'
//   zonal harmonics of clamped cosine in z:
//     l=0 : A0 = 0.5*sqrt(PI)
//     l=1 : A1 = sqrt(PI/3)
//     l=2 : A2 = (PI/4)*sqrt(5/(4*PI))
//     l=3 : A3 = 0
//   to rotate zonal harmonics in direction 'd' : sqrt( (4*PI)/(2*l+1)) * zl * SH coefficients in direction 'd'
//     l=0 : PI * SH coefficients in direction 'd' 
//     l=1 : 2*PI/3 * SH coefficients in direction 'd' 
//     l=2 : PI/4 * SH coefficients in direction 'd' 
//     l=3 : 0
void clampledCosineCoeff(float3 xyz, inout float c0, inout float c1, inout float c2, inout float c3)
{
    c0 = PI * 0.282094792;                              
    c1 = ((2.0*PI)/3.0f) * -0.4886025119 * xyz.y;
    c2 = ((2.0*PI)/3.0f) *  0.4886025119 * xyz.z;
    c3 = ((2.0*PI)/3.0f) * -0.4886025119 * xyz.x;
}

float innerProductSH(float4 SH1, float4 SH2)
{
    return SH1.x*SH2.x + SH1.y*SH2.y + SH1.z*SH2.z + SH1.w*SH2.w;
}


//--------------------------------------------------------
//shaders
//--------------------------------------------------------

#define NORMAL_DEPTH_BIAS (0.5f)
#define LIGHT_DEPTH_BIAS (0.5f)

initLPV_VSOUT VS_initializeLPV(uint vertexID : SV_VertexID)
{
    initLPV_VSOUT output;
    bool outside = false;

    //read the attributres for this virtual point light (VPL)
    int x = (vertexID & RSM_RES_M_1);
    int y = (int)( (float)vertexID / RSMWidth); 
    if(y>=RSMHeight) outside = true;
    int3 uvw = int3(x,y,0);

    float3 normal = g_txRSMNormal.Load(uvw).rgb;
    //decode the normal:
    normal = normal*float3(2.0f,2.0f,2.0f) - 1.0f;
    normal = normalize(normal);

    float4 color = g_txRSMColor.Load(uvw);

    //unproject the depth to get the view space position of the texel
    float2 normalizedInputPos = float2((float)x/RSMWidth,(float)y/RSMHeight);
    float depthSample = g_txRSMDepth.Load(uvw).x;
    float2 inputPos = float2((normalizedInputPos.x*2.0)-1.0,((1.0f-normalizedInputPos.y)*2.0)-1.0);

    float4 vProjectedPos = float4(inputPos.x, inputPos.y, depthSample, 1.0f);
    float4 viewSpacePos = mul(vProjectedPos, g_InverseProjection);
    viewSpacePos.xyz = viewSpacePos.xyz / viewSpacePos.w; 
    if(g_useFluxWeight) 
        output.fluxWeight = viewSpacePos.z * viewSpacePos.z * g_fluxWeight; // g_fluxWeight is ((2 * tan_Fov_X_half)/RSMWidth) * ((2 * tan_Fov_Y_half)/RSMHeight)
    else
        output.fluxWeight = 1.0f;

    if(viewSpacePos.z >= farClip) outside=true;

    float3 LPVSpacePos = mul( float4(viewSpacePos.xyz,1), g_ViewToLPV ).xyz;

    //displace the position half a cell size along its normal
    LPVSpacePos += normal / float3(LPV3DWidth, LPV3DHeight, LPV3DDepth) * displacement;

    if(LPVSpacePos.x<0.0f || LPVSpacePos.x>=1.0f) outside = true;
    if(LPVSpacePos.y<0.0f || LPVSpacePos.y>=1.0f) outside = true;
    if(LPVSpacePos.z<0.0f || LPVSpacePos.z>=1.0f) outside = true;

    output.pos =float4(LPVSpacePos.x,LPVSpacePos.y, LPVSpacePos.z, 1.0f);

    output.color = color.rgb;
    output.normal = normal;

    //if(outside) kill the vertex
    if(outside) output.pos.x = LPV3DWidth*2.0f;

    return output;
}

initLPV_VSOUT VS_initializeLPV_Bilnear(uint vertexIDin : SV_VertexID)
{
    initLPV_VSOUT output;
    bool outside = false;

    output.BiLinearOffset = floor((float)vertexIDin / (RSM_RES_SQR));
    int vertexID = vertexIDin & (RSM_RES_SQR_M_1);

    //read the attributres for this virtual point light (VPL)
    int x = (vertexID & RSM_RES_M_1);
    int y = (int)( (float)vertexID / RSMWidth); 
    if(y>=RSMHeight) outside = true;
    int3 uvw = int3(x,y,0);

    float3 normal = g_txRSMNormal.Load(uvw).rgb;
    //decode the normal:
    normal = normal*float3(2.0f,2.0f,2.0f) - 1.0f;
    normal = normalize(normal);

    float4 color = g_txRSMColor.Load(uvw);

    //unproject the depth to get the view space position of the texel
    float2 normalizedInputPos = float2((float)x/RSMWidth,(float)y/RSMHeight);
    float depthSample = g_txRSMDepth.Load(uvw).x;
    float2 inputPos = float2((normalizedInputPos.x*2.0)-1.0,((1.0f-normalizedInputPos.y)*2.0)-1.0);

    float4 vProjectedPos = float4(inputPos.x, inputPos.y, depthSample, 1.0f);
    float4 viewSpacePos = mul(vProjectedPos, g_InverseProjection);  
    viewSpacePos.xyz = viewSpacePos.xyz / viewSpacePos.w;  
    if(g_useFluxWeight) 
        output.fluxWeight = viewSpacePos.z * viewSpacePos.z * g_fluxWeight; // g_fluxWeight is ((2 * tan_Fov_X_half)/RSMWidth) * ((2 * tan_Fov_Y_half)/RSMHeight)
    else
        output.fluxWeight = 1.0f;

    if(viewSpacePos.z >= farClip) outside=true;

    float3 LPVSpacePos = mul( float4(viewSpacePos.xyz,1), g_ViewToLPV ).xyz;

    //displace the position half a cell size along its normal
    LPVSpacePos += normal / float3(LPV3DWidth, LPV3DHeight, LPV3DDepth) * displacement;

    if(LPVSpacePos.x<0.0f || LPVSpacePos.x>=1.0f) outside = true;
    if(LPVSpacePos.y<0.0f || LPVSpacePos.y>=1.0f) outside = true;
    if(LPVSpacePos.z<0.0f || LPVSpacePos.z>=1.0f) outside = true;

    output.pos =float4(LPVSpacePos.x,LPVSpacePos.y, LPVSpacePos.z, 1.0f);

    output.color = color.rgb;
    output.normal = normal;

    //if(outside) kill the vertex
    if(outside) output.pos.x = LPV3DWidth*2.0f;

    return output;
}


[maxvertexcount (1)]
void GS_initializeLPV(point initLPV_VSOUT In[1], inout PointStream<initLPV_GSOUT> pointStream)
{
    initLPV_GSOUT Out;
    Out.RTIndex         = floor( In[0].pos.z*LPV3DDepth + 0.5);
    Out.pos             = float4((In[0].pos.x)*2.0f-1.0f,(1.0f-(In[0].pos.y))*2.0f-1.0f,0.0f,1.0f);
    Out.normal         = In[0].normal;
    Out.color         = In[0].color;
    Out.fluxWeight     = In[0].fluxWeight;
    pointStream.Append( Out );
}


[maxvertexcount (1)]
void GS_initializeLPV_Bilinear(point initLPV_VSOUT In[1], inout PointStream<initLPV_GSOUT> pointStream)
{
   initLPV_GSOUT Out;
    Out.normal         = In[0].normal;
    float2 pos;
    float weight;

    if(In[0].pos.x>1.0f) return;

    float3 InPos = float3(In[0].pos.x*LPV3DWidth+0.5f, In[0].pos.y*LPV3DHeight+0.5f, In[0].pos.z*LPV3DDepth);

    int xF = floor(InPos.x);
    int yF = floor(InPos.y);
    int zF = floor(InPos.z);
    int xH = xF+1;
    int yH = yF+1;
    int zH = zF+1;

    float wxH = InPos.x - xF;
    float wyH = InPos.y - yF;
    float wzH = InPos.z - zF;
    float wxL = 1.0f - wxH;
    float wyL = 1.0f - wyH;
    float wzL = 1.0f - wzH;

    int BiLinearOffset = (In[0].BiLinearOffset);
    int xHOn = BiLinearOffset&1;
    int yHOn = (BiLinearOffset&2)>>1;
    int zHOn = (BiLinearOffset&4)>>2;

    pos = float2(xF*(1-xHOn) + xH*(xHOn),
                 yF*(1-yHOn) + yH*(yHOn));
                 
    weight =    1.5f*(wxL*(1-xHOn) + wxH*(xHOn))*
                (wyL*(1-yHOn) + wyH*(yHOn))*
                (wzL*(1-zHOn) + wzH*(zHOn));

    Out.RTIndex         = zF*(1-zHOn) + zH*(zHOn);
    Out.pos          = float4((pos.x/LPV3DWidth)*2.0f-1.0f,(1.0f-(pos.y/LPV3DHeight))*2.0f-1.0f,0.0f,1.0f);
    Out.color         = In[0].color*weight;   
    Out.fluxWeight   = In[0].fluxWeight;
    pointStream.Append( Out );
    
}

struct PS_MRTOutput
{
    float4 coefficientsRed        : SV_Target0;
    float4 coefficientsGreen    : SV_Target1;
    float4 coefficientsBlue        : SV_Target2;
};

struct PS_MRTOutputGV
{
    float4 occlusionCoefficients    : SV_Target0;
    float4 occluderColor            : SV_Target1;
};

PS_MRTOutput PS_initializeLPV( initLPV_GSOUT input ) : SV_Target
{
    PS_MRTOutput output;
    
    //have to add VPLs represented as SH coefficients of clamped cosine lobes oriented in the direction of the normal.

    float3 Normal = normalize(input.normal);

    float coeffs0, coeffs1, coeffs2, coeffs3;
    clampledCosineCoeff(Normal, coeffs0, coeffs1, coeffs2, coeffs3);

    float flux_red = input.color.r * input.fluxWeight * gLightScale;
    float flux_green = input.color.g * input.fluxWeight * gLightScale;
    float flux_blue = input.color.b * input.fluxWeight * gLightScale; 

    output.coefficientsRed =   float4(flux_red  *coeffs0, flux_red  *coeffs1, flux_red  *coeffs2, flux_red  *coeffs3);
    output.coefficientsGreen = float4(flux_green*coeffs0, flux_green*coeffs1, flux_green*coeffs2, flux_green*coeffs3);
    output.coefficientsBlue =  float4(flux_blue *coeffs0, flux_blue *coeffs1, flux_blue *coeffs2, flux_blue *coeffs3);

    return output;
}


PS_MRTOutputGV PS_initializeGV( initLPV_GSOUT input ) : SV_Target
{
    PS_MRTOutputGV output;

    //have to add occluders represented as SH coefficients of clamped cosine lobes oriented in the direction of the normal.

    float3 Normal = normalize(input.normal);

    float coeffs0, coeffs1, coeffs2, coeffs3;
    clampledCosineCoeff(Normal, coeffs0, coeffs1, coeffs2, coeffs3);

    output.occlusionCoefficients = float4(coeffs0*input.fluxWeight,coeffs1*input.fluxWeight, coeffs2*input.fluxWeight, coeffs3*input.fluxWeight);
    output.occluderColor = float4(input.color.r,input.color.g,input.color.b,1.0f);
    return output;
}

