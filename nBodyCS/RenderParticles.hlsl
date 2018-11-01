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

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

Texture2D <float4>g_ParticleTex : TEXTURE0;
StructuredBuffer<float4> g_particles;

//--------------------------------------------------------------------------------------
// Global constants
//--------------------------------------------------------------------------------------

cbuffer cbDraw
{
    float4x4 g_mWorldViewProjection;
    float	 g_fPointSize;
    uint     g_readOffset;
};

cbuffer cbImmutable
{
    float4 g_positions[4] : packoffset(c0) =
    {
        float4(  0.5, -0.5, 0, 0),
        float4(  0.5,  0.5, 0, 0),
        float4( -0.5, -0.5, 0, 0),
        float4( -0.5,  0.5, 0, 0),
    };
    float4 g_texcoords[4] : packoffset(c4) = 
    { 
        float4(1,0, 0, 0), 
        float4(1,1, 0, 0),
        float4(0,0, 0, 0),
        float4(0,1, 0, 0),
    };
};


//--------------------------------------------------------------------------------------
// Samplers
//--------------------------------------------------------------------------------------

SamplerState PointSampler
{
    Filter   = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

SamplerState LinearSampler
{
    Filter   = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

//--------------------------------------------------------------------------------------
// Vertex shader and pixel shader input/output structures
//--------------------------------------------------------------------------------------


struct VS_Input_Indices
{
    uint   pId : SV_VertexID ;   
};

struct DisplayVS_OUTPUT
{
    float4 Position   : SV_POSITION;   // vertex position 
    float4 Diffuse    : COLOR0;        // vertex diffuse color (note that COLOR0 is clamped from 0..1)
    float2 uv         : TEXCOORD0;
    float  PointSize  : PSIZE;		   // point size;
};

struct DisplayPS_OUTPUT
{
    float4 RGBColor : SV_TARGET0;  
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
DisplayVS_OUTPUT DisplayVS_StructBuffer( VS_Input_Indices In)
{
    DisplayVS_OUTPUT Output = (DisplayVS_OUTPUT) 0;
    
    float4 pos = g_particles[g_readOffset + In.pId];
    
     // Transform the position from object space to homogeneous projection space
    Output.Position  = mul(pos, g_mWorldViewProjection);
    Output.PointSize = g_fPointSize;

    // Compute simple directional lighting equation
    Output.Diffuse = abs(pos);
    Output.uv = 0;
         
    return Output;    
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
DisplayPS_OUTPUT DisplayPS( DisplayVS_OUTPUT In) 
{ 
    DisplayPS_OUTPUT Output;
	
	Output.RGBColor = In.Diffuse;
		
    return Output;
}

DisplayPS_OUTPUT DisplayPSTex( DisplayVS_OUTPUT In) 
{ 
    DisplayPS_OUTPUT Output;
	
	float tex = g_ParticleTex.Sample(LinearSampler, In.uv).x;
	
	if(tex.x < .05) 
		discard;
	
	Output.RGBColor = tex.xxxx * float4(0.3, 1.0, 0.2, 1.0);
		
    return Output;
}

//--------------------------------------------------------------------------------------
//
// Geometry shader for creating point sprites
//
//--------------------------------------------------------------------------------------
[maxvertexcount(4)]
void DisplayGS(point DisplayVS_OUTPUT input[1], inout TriangleStream<DisplayVS_OUTPUT> SpriteStream)
{
    DisplayVS_OUTPUT output;
    
	//
	// Emit two new triangles
	//
	[unroll]for(int i=0; i<4; i++)
	{
        output.Position = input[0].Position + float4(g_positions[i].xy * input[0].PointSize,0,0); 
             
        //output.Position.y *= g_fAspectRatio; // Correct for the screen aspect ratio, since the sprite is calculated in eye space
             
        // pass along the texcoords
        output.uv        = g_texcoords[i];
        output.Diffuse   = input[0].Diffuse;
        output.PointSize = input[0].PointSize;                      
		
		SpriteStream.Append(output);
    }
    SpriteStream.RestartStrip();
}


