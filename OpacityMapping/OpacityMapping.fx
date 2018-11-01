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

#ifndef OPACITY_MAPPING_FX
#define OPACITY_MAPPING_FX

//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
SamplerState g_SamplerOpacity
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

//--------------------------------------------------------------------------------------
// General-purpose render-state
//--------------------------------------------------------------------------------------
BlendState OSMBlend
{
    BlendEnable[0] = TRUE;
    BlendEnable[1] = TRUE;
    BlendEnable[2] = TRUE;
    BlendEnable[3] = TRUE;
    RenderTargetWriteMask[0] = 0xF;
    RenderTargetWriteMask[1] = 0xF;
    RenderTargetWriteMask[2] = 0xF;
    RenderTargetWriteMask[3] = 0xF;
    
    SrcBlend = Zero;
    DestBlend = Src_Color;
    BlendOp = Add;
    
    SrcBlendAlpha = Zero;
    DestBlendAlpha = Src_Alpha;
    BlendOpAlpha = Add;
};

BlendState FOMBlend
{
    BlendEnable[0] = TRUE;
    BlendEnable[1] = TRUE;
    BlendEnable[2] = TRUE;
    BlendEnable[3] = TRUE;
    RenderTargetWriteMask[0] = 0xF;
    RenderTargetWriteMask[1] = 0xF;
    RenderTargetWriteMask[2] = 0xF;
    RenderTargetWriteMask[3] = 0xF;
    
    SrcBlend = One;
    DestBlend = One;
    BlendOp = Add;
    
    SrcBlendAlpha = One;
    DestBlendAlpha = One;
    BlendOpAlpha = Add;
};

//--------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------
float CalcShadowFactorFOM(float Quality, float4 fom0, float4 fom1, float4 fom2, float4 fom3, float depth)
{
    float NumCoefficients = Quality;

    depth = saturate(depth);

    // DC component
    float lnTransmittance = fom0.g * depth;

    // Remaining outputs require sin/cos
    float2 cs1;
    sincos(M_TWO_PI * depth, cs1.y, cs1.x);
    
    if(NumCoefficients >= 4)
    {
        lnTransmittance += (1-cs1.x) * fom0.a;
        lnTransmittance += cs1.y * fom0.b;
    }

    float2 csn;
    if(NumCoefficients >= 6)
    {
        csn = float2(cs1.x*cs1.x-cs1.y*cs1.y, 2.f*cs1.y*cs1.x);
        lnTransmittance += (1-csn.x) * fom1.g;
        lnTransmittance += csn.y * fom1.r;
    }
    
    if(NumCoefficients >= 8)
    {
        csn = float2(csn.x*cs1.x-csn.y*cs1.y, csn.y*cs1.x+csn.x*cs1.y);
        lnTransmittance += (1-csn.x) * fom1.a;
        lnTransmittance += csn.y * fom1.b;
    }
    
    if(NumCoefficients >= 10)
    {
        csn = float2(cs1.x*cs1.x-cs1.y*cs1.y, 2.f*cs1.y*cs1.x);
        lnTransmittance += (1-csn.x) * fom2.g;
        lnTransmittance += csn.y * fom2.r;
    }
    
    if(NumCoefficients >= 12)
    {
        csn = float2(csn.x*cs1.x-csn.y*cs1.y, csn.y*cs1.x+csn.x*cs1.y);
        lnTransmittance += (1-csn.x) * fom2.a;
        lnTransmittance += csn.y * fom2.b;
    }   
    
    if(NumCoefficients >= 14)
    {
        csn = float2(cs1.x*cs1.x-cs1.y*cs1.y, 2.f*cs1.y*cs1.x);
        lnTransmittance += (1-csn.x) * fom3.g;
        lnTransmittance += csn.y * fom3.r;
    }
    
    if(NumCoefficients >= 16)
    {
        csn = float2(csn.x*cs1.x-csn.y*cs1.y, csn.y*cs1.x+csn.x*cs1.y);
        lnTransmittance += (1-csn.x) * fom3.a;
        lnTransmittance += csn.y * fom3.b;
    }   
    
    float shadowFactor = saturate(exp(-lnTransmittance));
    
    return shadowFactor;
}

float CalcShadowFactorOSM(float Quality, float4 osm0, float4 osm1, float4 osm2, float4 osm3, float depth)
{
    float NumSlices = Quality;

    float slice = depth * NumSlices;

    float shadowLow = 1.f;
    float shadowHigh = osm0.x;
    float shadowLerp = frac(slice);

    shadowLow = slice >= 1.f ? shadowHigh : shadowLow;
    shadowHigh = slice >= 1.f ? osm0.y : shadowHigh;

    shadowLow = slice >= 2.f ? shadowHigh : shadowLow;
    shadowHigh = slice >= 2.f ? osm0.z : shadowHigh;

    shadowLow = slice >= 3.f ? shadowHigh : shadowLow;
    shadowHigh = slice >= 3.f ? osm0.w : shadowHigh;

    shadowLow = slice >= 4.f ? shadowHigh : shadowLow;
    shadowHigh = slice >= 4.f ? osm1.x : shadowHigh;

    shadowLow = slice >= 5.f ? shadowHigh : shadowLow;
    shadowHigh = slice >= 5.f ? osm1.y : shadowHigh;

    shadowLow = slice >= 6.f ? shadowHigh : shadowLow;
    shadowHigh = slice >= 6.f ? osm1.z : shadowHigh;

    shadowLow = slice >= 7.f ? shadowHigh : shadowLow;
    shadowHigh = slice >= 7.f ? osm1.w : shadowHigh;
    
    shadowLow = slice >= 8.f ? shadowHigh : shadowLow;
    shadowHigh = slice >= 8.f ? osm2.x : shadowHigh;
    
    shadowLow = slice >= 9.f ? shadowHigh : shadowLow;
    shadowHigh = slice >= 9.f ? osm2.y : shadowHigh;
    
    shadowLow = slice >= 10.f ? shadowHigh : shadowLow;
    shadowHigh = slice >= 10.f ? osm2.z : shadowHigh;
    
    shadowLow = slice >= 11.f ? shadowHigh : shadowLow;
    shadowHigh = slice >= 11.f ? osm2.w : shadowHigh;
    
    shadowLow = slice >= 12.f ? shadowHigh : shadowLow;
    shadowHigh = slice >= 12.f ? osm3.x : shadowHigh;
    
    shadowLow = slice >= 13.f ? shadowHigh : shadowLow;
    shadowHigh = slice >= 13.f ? osm3.y : shadowHigh;
    
    shadowLow = slice >= 14.f ? shadowHigh : shadowLow;
    shadowHigh = slice >= 14.f ? osm3.z : shadowHigh;
    
    shadowLow = slice >= 15.f ? shadowHigh : shadowLow;
    shadowHigh = slice >= 15.f ? osm3.w : shadowHigh;
    
    float shadowFactor = lerp(shadowLow, shadowHigh, shadowLerp);
    
    return shadowFactor;
}

PS_OUTPUT_MRT CalcOpacityMap_OSM(float quality, float depth, float alpha)
{
    PS_OUTPUT_MRT Output;

    float invAlpha = 1.f - alpha;
    float NumSlices = quality;
    float SliceThickness = 1.f/NumSlices;

    Output.MRT0.x = depth < (1.f * SliceThickness) ? invAlpha : 1.f;
    Output.MRT0.y = depth < (2.f * SliceThickness) ? invAlpha : 1.f;
    Output.MRT0.z = depth < (3.f * SliceThickness) ? invAlpha : 1.f;
    Output.MRT0.w = depth < (4.f * SliceThickness) ? invAlpha : 1.f;

    Output.MRT1.x = depth < (5.f * SliceThickness) ? invAlpha : 1.f;
    Output.MRT1.y = depth < (6.f * SliceThickness) ? invAlpha : 1.f;
    Output.MRT1.z = depth < (7.f * SliceThickness) ? invAlpha : 1.f;
    Output.MRT1.w = depth < (8.f * SliceThickness) ? invAlpha : 1.f;
    
    Output.MRT2.x = depth < (9.f * SliceThickness) ? invAlpha : 1.f;
    Output.MRT2.y = depth < (10.f * SliceThickness) ? invAlpha : 1.f;
    Output.MRT2.z = depth < (11.f * SliceThickness) ? invAlpha : 1.f;
    Output.MRT2.w = depth < (12.f * SliceThickness) ? invAlpha : 1.f;
    
    Output.MRT3.x = depth < (13.f * SliceThickness) ? invAlpha : 1.f;
    Output.MRT3.y = depth < (14.f * SliceThickness) ? invAlpha : 1.f;
    Output.MRT3.z = depth < (15.f * SliceThickness) ? invAlpha : 1.f;
    Output.MRT3.w = depth < (16.f * SliceThickness) ? invAlpha : 1.f;

    return Output;
}

PS_OUTPUT_MRT CalcOpacityMap_FOM(float quality, float depth, float alpha)
{
    PS_OUTPUT_MRT Output;

    float invAlpha = 1.f - alpha;
    float lnTransmittance = -log(invAlpha);
    
    Output.MRT0.r = 0.f;
    
    // DC component
    Output.MRT0.g = lnTransmittance;
    
    // These constants need to be applied as the result of integration
    const float recipPi = 1.f/M_PI;
    const float halfRecipPi = recipPi * 0.5f;
    const float thirdRecipPi = recipPi/3.f;
    const float quarterRecipPi = recipPi * 0.25f;
    const float fifthRecipPi = recipPi * 0.2f;
    const float sixthRecipPi = recipPi/6.f;
    const float seventhRecipPi = recipPi/7.f;
    
    // Remaining outputs require sin/cos
    float2 cs1;
    sincos(M_TWO_PI * depth, cs1.y, cs1.x);
    Output.MRT0.ba = recipPi * lnTransmittance * cs1;
    
    float2 csn = float2(cs1.x*cs1.x-cs1.y*cs1.y, 2.f*cs1.y*cs1.x);
    Output.MRT1.rg = halfRecipPi * lnTransmittance * csn;
    
    csn = float2(csn.x*cs1.x-csn.y*cs1.y, csn.y*cs1.x+csn.x*cs1.y);
    Output.MRT1.ba = thirdRecipPi * lnTransmittance * csn;
    
    csn = float2(csn.x*cs1.x-csn.y*cs1.y, csn.y*cs1.x+csn.x*cs1.y);
    Output.MRT2.rg = quarterRecipPi * lnTransmittance * csn;
    
    csn = float2(csn.x*cs1.x-csn.y*cs1.y, csn.y*cs1.x+csn.x*cs1.y);
    Output.MRT2.ba = fifthRecipPi * lnTransmittance * csn;
    
    csn = float2(csn.x*cs1.x-csn.y*cs1.y, csn.y*cs1.x+csn.x*cs1.y);
    Output.MRT3.rg = sixthRecipPi * lnTransmittance * csn;
    
    csn = float2(csn.x*cs1.x-csn.y*cs1.y, csn.y*cs1.x+csn.x*cs1.y);
    Output.MRT3.ba = seventhRecipPi * lnTransmittance * csn;

    return Output;
}

float GetMaxOpacityQuality() { return 16.f; }

#endif // OPACITY_MAPPING_FX
