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

cbuffer GlobalConstantBuffer : register(b0)
{
  float2 g_FullResolution;
  float2 g_InvFullResolution;

  float2 g_AOResolution;
  float2 g_InvAOResolution;

  float2 g_FocalLen;
  float2 g_InvFocalLen;

  float2 g_UVToViewA;
  float2 g_UVToViewB;

  float g_R;
  float g_R2;
  float g_NegInvR2;
  float g_MaxRadiusPixels;

  float g_AngleBias;
  float g_TanAngleBias;
  float g_PowExponent;
  float g_Strength;

  float g_BlurDepthThreshold;
  float g_BlurFalloff;
  float g_LinA;
  float g_LinB;
};
