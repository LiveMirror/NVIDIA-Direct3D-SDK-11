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

struct PickVertex
{
    float4 vPosition  : SV_Position;
    float4 pickParams : TEXCOORD0;
};

//------------------------------------------------------------------------------
// Picking pass
PickVertex PickVS( AppVertex input )
{
    float3 displacedPos;
	int2 intUV;
	ReconstructPosition(input, displacedPos, intUV);
    
	const int mipLevel   = 0;
    float z = g_fDisplacementHeight * g_CoarseHeightMap.SampleLevel(SamplerClampLinear, displacedPos.xz, mipLevel, int2(1,1)).r;
    displacedPos.y += z;

	// TBD: fix to work with non-texture array version??!?!
    //float tileY = floor(input.InstanceId * RECIP_N_TILES);
    //float tileX = input.InstanceId - tileY * N_TILES;

	PickVertex output = (PickVertex) 0;
    //output.pickParams = float4(tileX, tileY, displacedPos.xz);
    
    output.vPosition = mul( float4( displacedPos, 1.0f ), g_WorldViewProj );
    
    return output;
}

float4 PickPS( PickVertex input ) : SV_Target
{
	return input.pickParams;
}
