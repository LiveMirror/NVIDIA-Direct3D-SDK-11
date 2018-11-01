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

#include "ConstantBuffer.hlsl"

#define M_PI 3.14159265f

#define SAMPLE_FIRST_STEP 1
#define USE_NORMAL_FREE_HBAO 0

#define NUM_DIRECTIONS 8
#define NUM_STEPS 6
#define RANDOM_TEXTURE_WIDTH 4

Texture2D<float>  tLinearDepth      : register(t0);
Texture2D<float3> tRandom           : register(t1);
sampler PointClampSampler           : register(s0);
sampler PointWrapSampler            : register(s1);

//----------------------------------------------------------------------------------
struct PostProc_VSOut
{
    float4 pos   : SV_Position;
    float2 texUV : TEXCOORD0;
};

//----------------------------------------------------------------------------------
float InvLength(float2 v)
{
    return rsqrt(dot(v,v));
}

//----------------------------------------------------------------------------------
float Tangent(float3 P, float3 S)
{
    return (P.z - S.z) * InvLength(S.xy - P.xy);
}

//----------------------------------------------------------------------------------
float3 UVToEye(float2 uv, float eye_z)
{
    uv = g_UVToViewA * uv + g_UVToViewB;
    return float3(uv * eye_z, eye_z);
}

//----------------------------------------------------------------------------------
float3 FetchEyePos(float2 uv)
{
    float z = tLinearDepth.SampleLevel(PointClampSampler, uv, 0);
    return UVToEye(uv, z);
}

//----------------------------------------------------------------------------------
float Length2(float3 v)
{
    return dot(v, v);
}

//----------------------------------------------------------------------------------
float3 MinDiff(float3 P, float3 Pr, float3 Pl)
{
    float3 V1 = Pr - P;
    float3 V2 = P - Pl;
    return (Length2(V1) < Length2(V2)) ? V1 : V2;
}

//----------------------------------------------------------------------------------
float Falloff(float d2)
{
    // 1 scalar mad instruction
    return d2 * g_NegInvR2 + 1.0f;
}

//----------------------------------------------------------------------------------
float2 SnapUVOffset(float2 uv)
{
    return round(uv * g_AOResolution) * g_InvAOResolution;
}

//----------------------------------------------------------------------------------
float TanToSin(float x)
{
    return x * rsqrt(x*x + 1.0f);
}

//----------------------------------------------------------------------------------
float3 TangentVector(float2 deltaUV, float3 dPdu, float3 dPdv)
{
    return deltaUV.x * dPdu + deltaUV.y * dPdv;
}

//----------------------------------------------------------------------------------
float Tangent(float3 T)
{
    return -T.z * InvLength(T.xy);
}

//----------------------------------------------------------------------------------
float BiasedTangent(float3 T)
{
    // Do not use atan() because it gets expanded by fxc to many math instructions
    return Tangent(T) + g_TanAngleBias;
}

#if USE_NORMAL_FREE_HBAO

//----------------------------------------------------------------------------------
void IntegrateDirection(inout float ao, float3 P, float2 uv, float2 deltaUV,
                         float numSteps, float tanH, float sinH)
{
    for (float j = 1; j <= numSteps; ++j)
    {
        uv += deltaUV;
        float3 S = FetchEyePos(uv);
        float tanS = Tangent(P, S);
        float d2 = Length2(S - P);
        
        [branch]
        if ((d2 < g_R2) && (tanS > tanH))
        {
            // Accumulate AO between the horizon and the sample
            float sinS = TanToSin(tanS);
            ao += Falloff(d2) * (sinS - sinH);
            
            // Update the current horizon angle
            tanH = tanS;
            sinH = sinS;
        }
    }
}

//----------------------------------------------------------------------------------
float NormalFreeHorizonOcclusion(float2 deltaUV,
                                 float2 texelDeltaUV,
                                 float2 uv0,
                                 float3 P,
                                 float numSteps,
                                 float randstep)
{

    float tanT = tan(-M_PI*0.5 + g_AngleBias);
    float sinT = TanToSin(tanT);

#if SAMPLE_FIRST_STEP
    float ao1 = 0;

    // Take a first sample between uv0 and uv0 + deltaUV
    float2 deltaUV1 = SnapUVOffset( randstep * deltaUV + texelDeltaUV );
    IntegrateDirection(ao1, P, uv0, deltaUV1, 1, tanT, sinT);
    IntegrateDirection(ao1, P, uv0, -deltaUV1, 1, tanT, sinT);

    ao1 = max(ao1 * 0.5 - 1.0, 0.0);
    --numSteps;
#endif

    float ao = 0;
    float2 uv = uv0 + SnapUVOffset( randstep * deltaUV );
    deltaUV = SnapUVOffset( deltaUV );
    IntegrateDirection(ao, P, uv, deltaUV, numSteps, tanT, sinT);

    // Integrate opposite directions together
    deltaUV = -deltaUV;
    uv = uv0 + SnapUVOffset( randstep * deltaUV );
    IntegrateDirection(ao, P, uv, deltaUV, numSteps, tanT, sinT);

    // Divide by 2 because we have integrated 2 directions together
    // Subtract 1 and clamp to remove the part below the surface
#if SAMPLE_FIRST_STEP
    return max(ao * 0.5 - 1.0, ao1);
#else
    return max(ao * 0.5 - 1.0, 0.0);
#endif
}

#else //USE_NORMAL_FREE_HBAO

//----------------------------------------------------------------------------------
float IntegerateOcclusion(float2 uv0,
                          float2 snapped_duv,
                          float3 P,
                          float3 dPdu,
                          float3 dPdv,
                          inout float tanH)
{
    float ao = 0;

    // Compute a tangent vector for snapped_duv
    float3 T1 = TangentVector(snapped_duv, dPdu, dPdv);
    float tanT = BiasedTangent(T1);
    float sinT = TanToSin(tanT);

    float3 S = FetchEyePos(uv0 + snapped_duv);
    float tanS = Tangent(P, S);

    float sinS = TanToSin(tanS);
    float d2 = Length2(S - P);

    if ((d2 < g_R2) && (tanS > tanT))
    {
        // Compute AO between the tangent plane and the sample
        ao = Falloff(d2) * (sinS - sinT);

        // Update the horizon angle
        tanH = max(tanH, tanS);
    }

    return ao;
}

//----------------------------------------------------------------------------------
float horizon_occlusion(float2 deltaUV,
                        float2 texelDeltaUV,
                        float2 uv0,
                        float3 P,
                        float numSteps,
                        float randstep,
                        float3 dPdu,
                        float3 dPdv )
{
    float ao = 0;

    // Randomize starting point within the first sample distance
    float2 uv = uv0 + SnapUVOffset( randstep * deltaUV );

    // Snap increments to pixels to avoid disparities between xy
    // and z sample locations and sample along a line
    deltaUV = SnapUVOffset( deltaUV );

    // Compute tangent vector using the tangent plane
    float3 T = deltaUV.x * dPdu + deltaUV.y * dPdv;

    float tanH = BiasedTangent(T);

#if SAMPLE_FIRST_STEP
    // Take a first sample between uv0 and uv0 + deltaUV
    float2 snapped_duv = SnapUVOffset( randstep * deltaUV + texelDeltaUV );
    ao = IntegerateOcclusion(uv0, snapped_duv, P, dPdu, dPdv, tanH);
    --numSteps;
#endif

    float sinH = tanH / sqrt(1.0f + tanH*tanH);

    for (float j = 1; j <= numSteps; ++j)
    {
        uv += deltaUV;
        float3 S = FetchEyePos(uv);
        float tanS = Tangent(P, S);
        float d2 = Length2(S - P);

        // Use a merged dynamic branch
        [branch]
        if ((d2 < g_R2) && (tanS > tanH))
        {
            // Accumulate AO between the horizon and the sample
            float sinS = tanS / sqrt(1.0f + tanS*tanS);
            ao += Falloff(d2) * (sinS - sinH);

            // Update the current horizon angle
            tanH = tanS;
            sinH = sinS;
        }
    }

    return ao;
}

#endif //USE_NORMAL_FREE_HBAO

//----------------------------------------------------------------------------------
float2 RotateDirections(float2 Dir, float2 CosSin)
{
    return float2(Dir.x*CosSin.x - Dir.y*CosSin.y,
                  Dir.x*CosSin.y + Dir.y*CosSin.x);
}

//----------------------------------------------------------------------------------
void ComputeSteps(inout float2 step_size_uv, inout float numSteps, float ray_radius_pix, float rand)
{
    // Avoid oversampling if NUM_STEPS is greater than the kernel radius in pixels
    numSteps = min(NUM_STEPS, ray_radius_pix);

    // Divide by Ns+1 so that the farthest samples are not fully attenuated
    float step_size_pix = ray_radius_pix / (numSteps + 1);

    // Clamp numSteps if it is greater than the max kernel footprint
    float maxNumSteps = g_MaxRadiusPixels / step_size_pix;
    if (maxNumSteps < numSteps)
    {
        // Use dithering to avoid AO discontinuities
        numSteps = floor(maxNumSteps + rand);
        numSteps = max(numSteps, 1);
        step_size_pix = g_MaxRadiusPixels / numSteps;
    }

    // Step size in uv space
    step_size_uv = step_size_pix * g_InvAOResolution;
}

//----------------------------------------------------------------------------------
float2 HBAO_PS11(PostProc_VSOut IN ) : SV_TARGET
{
    float3 P = FetchEyePos(IN.texUV);

    // (cos(alpha),sin(alpha),jitter)
    float3 rand = tRandom.Sample(PointWrapSampler, IN.pos.xy / RANDOM_TEXTURE_WIDTH);

    // Compute projection of disk of radius g_R into uv space
    // Multiply by 0.5 to scale from [-1,1]^2 to [0,1]^2
    float2 ray_radius_uv = 0.5 * g_R * g_FocalLen / P.z;
    float ray_radius_pix = ray_radius_uv.x * g_AOResolution.x;
    if (ray_radius_pix < 1) return 1.0;

    float numSteps;
    float2 step_size;
    ComputeSteps(step_size, numSteps, ray_radius_pix, rand.z);

    // Nearest neighbor pixels on the tangent plane
    float3 Pr, Pl, Pt, Pb;
    Pr = FetchEyePos(IN.texUV + float2(g_InvAOResolution.x, 0));
    Pl = FetchEyePos(IN.texUV + float2(-g_InvAOResolution.x, 0));
    Pt = FetchEyePos(IN.texUV + float2(0, g_InvAOResolution.y));
    Pb = FetchEyePos(IN.texUV + float2(0, -g_InvAOResolution.y));

    // Screen-aligned basis for the tangent plane
    float3 dPdu = MinDiff(P, Pr, Pl);
    float3 dPdv = MinDiff(P, Pt, Pb) * (g_AOResolution.y * g_InvAOResolution.x);

    float ao = 0;
    float d;
    float alpha = 2.0f * M_PI / NUM_DIRECTIONS;

    // this switch gets unrolled by the HLSL compiler
#if USE_NORMAL_FREE_HBAO
    for (d = 0; d < NUM_DIRECTIONS*0.5; ++d)
    {
        float angle = alpha * d;
        float2 dir = RotateDirections(float2(cos(angle), sin(angle)), rand.xy);
        float2 deltaUV = dir * step_size.xy;
        float2 texelDeltaUV = dir * g_InvAOResolution;
        ao += NormalFreeHorizonOcclusion(deltaUV, texelDeltaUV, IN.texUV, P, numSteps, rand.z);
    }
    ao *= 2.0;
#else
    for (d = 0; d < NUM_DIRECTIONS; ++d)
    {
         float angle = alpha * d;
         float2 dir = RotateDirections(float2(cos(angle), sin(angle)), rand.xy);
         float2 deltaUV = dir * step_size.xy;
         float2 texelDeltaUV = dir * g_InvAOResolution;
         ao += horizon_occlusion(deltaUV, texelDeltaUV, IN.texUV, P, numSteps, rand.z, dPdu, dPdv);
    }
#endif

    ao = 1.0 - ao / NUM_DIRECTIONS * g_Strength;
    return float2(ao, P.z);
}
