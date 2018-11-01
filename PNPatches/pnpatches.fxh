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

//Work-around for an optimization rule problem in the June 2010 HLSL Compiler
// (9.29.952.3111)
// see http://support.microsoft.com/kb/2448404
#if D3DX_VERSION == 0xa2b
#pragma ruledisable 0x0802405f
#endif 

// Common subroutines

float4 BernsteinBasis(float t)
{
    float invT = 1.0f - t;

    return float4( invT * invT * invT,
                   3.0f * t * invT * invT,
                   3.0f * t * t * invT,
                   t * t * t );
}

float3 EvaluateBezier( float3  p0, float3  p1, float3  p2, float3  p3,
                       float3  p4, float3  p5, float3  p6, float3  p7,
                       float3  p8, float3  p9, float3 p10, float3 p11,
                       float3 p12, float3 p13, float3 p14, float3 p15,
                       float4 BasisU,
                       float4 BasisV )
{
    float3 Value;
    Value  = BasisV.x * (  p0 * BasisU.x +  p1 * BasisU.y +  p2 * BasisU.z +  p3 * BasisU.w );
    Value += BasisV.y * (  p4 * BasisU.x +  p5 * BasisU.y +  p6 * BasisU.z +  p7 * BasisU.w );
    Value += BasisV.z * (  p8 * BasisU.x +  p9 * BasisU.y + p10 * BasisU.z + p11 * BasisU.w );
    Value += BasisV.w * ( p12 * BasisU.x + p13 * BasisU.y + p14 * BasisU.z + p15 * BasisU.w );
    return Value;
}