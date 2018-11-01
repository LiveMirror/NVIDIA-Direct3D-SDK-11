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


// Global Variables -------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
float4x4 gMatWorld;
float4x4 gMatWorldViewProjection;
float4x4 gMatViewProjectionInv;

float3 gVecLightDir;

float4 gAmbientLightColor;
Texture2D gSpotlightTex;
Texture2D gShadowTex;

float2 gViewportSize;

float4x4 gMatSpotlightWorldViewProjection;

Texture2D gStereoParamTex;

Texture2D gHudTex;
float4x4 gMatHudViewProjection;

int gPosTechnique;

// State Definitions ------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
DepthStencilState EnableDepth
{ 
    DepthEnable = TRUE;
    DepthWriteMask = ALL;
    DepthFunc = LESS_EQUAL;
};

DepthStencilState DisableDepth
{ 
    DepthEnable = TRUE;
    DepthFunc = ALWAYS;
    DepthWriteMask = ZERO;
};

// Blending
BlendState NormalRenderNoBlending
{
	AlphaToCoverageEnable = FALSE;
	BlendEnable[0] = FALSE;
	RenderTargetWriteMask[0] = 15;
};

BlendState NormalRenderEnableBlending
{
	BlendEnable[0] = TRUE;
    SrcBlend = SRC_ALPHA;
    DestBlend = INV_SRC_ALPHA;
    BlendOp = ADD;
	
	RenderTargetWriteMask[0] = 15;
};

BlendState ColorWriteDisable
{
	AlphaToCoverageEnable = FALSE;
	BlendEnable[0] = FALSE;
	RenderTargetWriteMask[0] = 0;
};

// Texture
SamplerState TrilinearClamp
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState PointClamp
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerComparisonState BilinearComparisonClamp
{
    Filter = COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
    ComparisonFunc = LESS_EQUAL;
};

// Shader Output Structures -----------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
struct VsOutput
{
    float4 Position   : SV_POSITION; // vertex position (clip space)
    float4 Normal	  : TEXCOORD0;   // vertex normal (world space)
    float4 WorldPosition : TEXCOORD1; // vertex position (world space)
};

struct HudVsOutput
{
    float4 Position   : SV_POSITION; // vertex position (clip space)
    float2 UV	      : TEXCOORD0;   // texture coordinates
};

struct SingleColorOutput
{
    float4 RGBColor : SV_Target;  // Pixel color
};

// Vertex Shaders ---------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
VsOutput RenderSceneMeshVS(float4 pos : POSITION,
                           float3 normal : NORMAL,
                           float2 texCoord0 : TEXCOORD)
{
    VsOutput Out;
        
    // Transform the position from object space to homogeneous projection space
    Out.Position = mul(pos, gMatWorldViewProjection);
    Out.Normal = mul(normal, gMatWorld); 
    Out.WorldPosition = mul(pos, gMatWorld);
    
    return Out;
}

HudVsOutput RenderHudVS(float4 pos : POSITION,
						float3 normal : NORMAL,
						float2 texCoord0 : TEXCOORD)
{
	HudVsOutput Out;
	
    Out.Position = mul(pos, gMatHudViewProjection);
    Out.UV = texCoord0;

	return Out;
}

float4 RenderDebugVS(float4 pos : POSITION) : SV_POSITION
{
	return mul(pos, gMatWorldViewProjection);
}


// Function helpers -------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
float4 StereoToMonoClipSpace(float4 stereoClipPos)
{
	float4 monoClipPos = stereoClipPos;
	// Take a sample directly in the middle of the pixel at 0, 0, which is 0 / Width + 1 / (Width * 2)
    float2 stereoParms = gStereoParamTex.Sample(PointClamp, float2(0.0625, 0.5));
    float eyeSep = stereoParms.x;
    float convergence = stereoParms.y;
    
    // Stereo transform is: stereoClipPos.x = monoClipPos.x + eyeSep * (monoClipPos.w - convergence);
    monoClipPos.x -= eyeSep * (monoClipPos.w - convergence);
	
	return monoClipPos;
}

float4 ComputeWorldPosition(float4 screenPosition, bool undoMonoToStereoXform)
{	
	// First, undo the viewport transform.
	float4 homogenousDeviceSpacePos = float4(2 * (screenPosition.xy - 0.5) / gViewportSize - 1.0, screenPosition.zw);
	// Viewport space and homogenous device space are inverted with respect to y, so fix that here.
	homogenousDeviceSpacePos.y = -homogenousDeviceSpacePos.y;
	
	// Then, undo the w-divide. 
	float4 clipspacePos = float4(homogenousDeviceSpacePos.xyz * homogenousDeviceSpacePos.w, homogenousDeviceSpacePos.w);
	
	if (undoMonoToStereoXform) {
		clipspacePos = StereoToMonoClipSpace(clipspacePos);
	}
	
	// Finally, undo the world space projection
	return mul(clipspacePos, gMatViewProjectionInv);
}

float4 UnprojectSpotlightTexture(float4 worldPosition)
{
	float4 lightSpacePos = mul(worldPosition, gMatSpotlightWorldViewProjection);

	if (0 > lightSpacePos.w || abs(lightSpacePos.x) > lightSpacePos.w || abs(lightSpacePos.y) > lightSpacePos.w || lightSpacePos.z > lightSpacePos.w) {
		return float4(0, 0, 0, 0);
	}
	
	lightSpacePos.y = -lightSpacePos.y;
	lightSpacePos.xy /= lightSpacePos.w;
	lightSpacePos.xy = (lightSpacePos.xy + 1) / 2;
	
	return gSpotlightTex.Sample(TrilinearClamp, lightSpacePos.xy);
}

float GetShadowedPcg(float4 worldPosition)
{
	float4 lightSpacePos = mul(worldPosition, gMatSpotlightWorldViewProjection);

	if (0 > lightSpacePos.w || abs(lightSpacePos.x) > lightSpacePos.w || abs(lightSpacePos.y) > lightSpacePos.w || lightSpacePos.z > lightSpacePos.w) {
		return 0;
	}
	
	// Move back a little to avoid (some) shadow acne.
	lightSpacePos.z -= 0.01;
	lightSpacePos.y = -lightSpacePos.y;
	lightSpacePos.xyz /= lightSpacePos.w;
	lightSpacePos.xy = (lightSpacePos.xy + 1) / 2;
	
	return gShadowTex.SampleCmp(BilinearComparisonClamp, lightSpacePos.xy, lightSpacePos.z);
}

float4 GetSpotlightContribution(VsOutput In, int posTechnique)
{
	/* 
	   A common operation in modern rendering engines is to take a given UV position and
	   a depth value and to compute the world or view position of the pixel at that location.
	   This gives accurate results when rendering in mono, but produces faults when rendering
	   in stereo.
	
	   In 3D Vision Automatic or 3D Vision Explicit modes, this operation will yield the wrong 
	   position because of a hidden mono-to-stereo transform that has been performed for you.
	   
	   posTechnique 0 performs canonical unprojection, and shows faults in stereo.
	   posTechnique 1 uses an additional 'worldPosition' attribute in the vertex, 
	     which will be the same in the left and right eye render. This produces 
	     expected results in mono and stereo.
	   posTechnique 2 performs canonical unprojection, but uses information in a 
	     special stereo texture to correctly invert the mono-to-stereo clip 
	     projection. This produces expected results in mono and stereo.
	*/
	
	float4 worldPosition;
	if (posTechnique == 0) {
		// Perform canonical unprojection. During post-processing effects, 
		// the position passed in will often be UV coordinates instead of 
		// screen space coordinates, and the actual unproject would look slightly
		// different as a result.
	    worldPosition = ComputeWorldPosition(In.Position, false);
	
    } else if (posTechnique == 1) {
		// Grab world position from what we computed during the vertex shader.
		worldPosition = In.WorldPosition;
    
	} else if (posTechnique == 2) {
		// Another method of fixing this involves using a stereoized texture, fetching parameters
		// from that texture and using them to duplicate the mono-to-stereo transform the vertex
		// shader has used. This method works on Direct3D 9 and above, and will not disable Early-Z
		// optimizations. However, it requires that you understand the mono-to-stereo transform that
		// the vertex shader has applied, and additionally introduces a dependent texture read. These
		// types of reads (where reading one texture requires the results of first reading another)
		// can reduce your shader performance.

		worldPosition = ComputeWorldPosition(In.Position, true);
	}
	
	return GetShadowedPcg(worldPosition) 
	     * max(0, dot(normalize(In.Normal), -gVecLightDir)) 
	     * UnprojectSpotlightTexture(worldPosition);
}

// Pixel Shaders ----------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
SingleColorOutput RenderSceneMeshPS(VsOutput In) 
{ 
    SingleColorOutput Out;
    Out.RGBColor = GetSpotlightContribution(In, gPosTechnique) + gAmbientLightColor;
    return Out;
}

SingleColorOutput RenderHudPS(HudVsOutput In)
{
	SingleColorOutput Out;
	Out.RGBColor = gHudTex.Sample(TrilinearClamp, In.UV);
	return Out;
}

float4 RenderDebugPS(float4 unused : SV_POSITION) : SV_Target
{
	return float4(1, 1, 1, 1);
}

// Techniques -------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
technique10 ShadowBufferRender
{
	pass P0
	{
		// Caller will ensure that the right matrices are set for shadow rendering.
        SetVertexShader(CompileShader(vs_4_0, RenderSceneMeshVS()));
        SetGeometryShader(NULL);
        SetPixelShader(NULL);
        
        SetDepthStencilState(EnableDepth, 0);
        SetBlendState(ColorWriteDisable, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}
}

technique10 ForwardRenderScene
{	
    pass P0
    {
        SetVertexShader(CompileShader(vs_4_0, RenderSceneMeshVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, RenderSceneMeshPS( )));
        
        SetDepthStencilState(EnableDepth, 0);
        SetBlendState(NormalRenderNoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
    }
}


technique10 CursorRender
{
	pass P0
	{
        SetVertexShader(CompileShader(vs_4_0, RenderHudVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, RenderHudPS( )));
	
        SetDepthStencilState(DisableDepth, 0);
		SetBlendState(NormalRenderEnableBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);
	}

}

technique10 DebugRender
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_4_0, RenderDebugVS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_4_0, RenderDebugPS()));
		
		SetDepthStencilState(EnableDepth, 0);
        SetBlendState(NormalRenderNoBlending, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF);	
	}
}