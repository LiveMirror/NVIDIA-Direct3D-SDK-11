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

// This dample demonstrates the use of approximating Catmull-clark subdivision 
// surfaces with gregory patches. 

//=================================================================================================================================
// Constant buffer
//=================================================================================================================================

cbuffer cbTriangles : register( b0 )
{
    float4x4    g_f4x4WorldViewProjection;  // World * View * Projection matrix
    float4      g_f4TessFactors;            
	float4      g_f2Modes;
}


//=================================================================================================================================
// Shader structures
//=================================================================================================================================

struct Position_Input
{
    float3 f3Position   : POSITION0;  
};

struct HS_ConstantOutput
{
    float fTessFactor[3]    : SV_TessFactor;
    float fInsideTessFactor : SV_InsideTessFactor;
};

struct HS_ConstantOutput_Quad
{
    float fTessFactor[4]       : SV_TessFactor;
    float fInsideTessFactor[2] : SV_InsideTessFactor;
};

struct DS_Output
{
    float4 f4Position   : SV_Position;

};

struct PS_RenderSceneInput
{
    float4 f4Position   : SV_Position;

};

struct PS_RenderOutput
{
    float4 f4Color      : SV_Target0;
};
//=================================================================================================================================
// Functions
//=================================================================================================================================
float3 bilerpUV(float3 v[4], float2 uv)
{
    // bilerp the texture coordinates    
    float3 bottom = lerp( v[0], v[1], uv.x );
    float3 top = lerp( v[3], v[2], uv.x );
    float3 result = lerp( bottom, top, uv.y );
	
    return result;    
}

//=================================================================================================================================
// This vertex shader is a pass through stage, with HS, tessellation, and DS stages following
//=================================================================================================================================
Position_Input VS_RenderSceneWithTessellation( Position_Input I )
{
    Position_Input O;
    
    // Pass through world space position
    O.f3Position = I.f3Position;
    
    return O;    
}

HS_ConstantOutput HS_TrianglesConstant( InputPatch<Position_Input, 3> I )
{
    HS_ConstantOutput O = (HS_ConstantOutput)0;

	float3 rtf; float ritf, itf;
	uint mode = (uint)g_f2Modes.x;
	switch (mode)
	{
		case 0: ProcessTriTessFactorsMax(g_f4TessFactors.xyz, g_f2Modes.y, rtf, ritf, itf);
			break;
		case 1: ProcessTriTessFactorsMin(g_f4TessFactors.xyz, g_f2Modes.y, rtf, ritf, itf);
			break;
		case 2: ProcessTriTessFactorsAvg(g_f4TessFactors.xyz, g_f2Modes.y, rtf, ritf, itf);
			break;
		default: ProcessTriTessFactorsMax(g_f4TessFactors.xyz, g_f2Modes.y, rtf, ritf, itf);
			break;
	}
    O.fTessFactor[0] = rtf.x;
	O.fTessFactor[1] = rtf.y;
	O.fTessFactor[2] = rtf.z;
    O.fInsideTessFactor = ritf;

    return O;
}
[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[patchconstantfunc("HS_TrianglesConstant")]
[outputcontrolpoints(3)]
Position_Input HS_Triangles_integer( InputPatch<Position_Input, 3> I, uint uCPID : SV_OutputControlPointID )
{
    Position_Input O = (Position_Input)0;

    O.f3Position = I[uCPID].f3Position;
    
    return O;
}
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[patchconstantfunc("HS_TrianglesConstant")]
[outputcontrolpoints(3)]
Position_Input HS_Triangles_fractionalodd( InputPatch<Position_Input, 3> I, uint uCPID : SV_OutputControlPointID )
{
    Position_Input O = (Position_Input)0;

    O.f3Position = I[uCPID].f3Position;
    
    return O;
}
[domain("tri")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[patchconstantfunc("HS_TrianglesConstant")]
[outputcontrolpoints(3)]
Position_Input HS_Triangles_fractionaleven( InputPatch<Position_Input, 3> I, uint uCPID : SV_OutputControlPointID )
{
    Position_Input O = (Position_Input)0;

    O.f3Position = I[uCPID].f3Position;
    
    return O;
}
[domain("tri")]
[partitioning("pow2")]
[outputtopology("triangle_cw")]
[patchconstantfunc("HS_TrianglesConstant")]
[outputcontrolpoints(3)]
Position_Input HS_Triangles_pow2( InputPatch<Position_Input, 3> I, uint uCPID : SV_OutputControlPointID )
{
    Position_Input O = (Position_Input)0;

    O.f3Position = I[uCPID].f3Position;
    
    return O;
}
//=================================================================================================================================
// This domain shader applies contol point weighting to the barycentric coords produced by the FF tessellator 
//=================================================================================================================================
[domain("tri")]
PS_RenderSceneInput DS_Triangles( HS_ConstantOutput HSConstantData, const OutputPatch<Position_Input, 3> I, float3 f3BarycentricCoords : SV_DomainLocation )
{
    PS_RenderSceneInput O = (PS_RenderSceneInput)0;

    // The barycentric coordinates
    float fU = f3BarycentricCoords.x;
    float fV = f3BarycentricCoords.y;
    float fW = f3BarycentricCoords.z;


	float3 f3Position = I[0].f3Position * fW +
                        I[1].f3Position * fU +
                        I[2].f3Position * fV;


    // Transform model position with view-projection matrix
    O.f4Position = mul( float4( f3Position.xyz, 1.0 ), g_f4x4WorldViewProjection );

        
    return O;
}



HS_ConstantOutput_Quad HS_QuadsConstant( InputPatch<Position_Input, 4> I )
{
    HS_ConstantOutput_Quad O = (HS_ConstantOutput_Quad)0;
    
    float2 ritf,itf; float4 rtf;
	uint mode = (uint)g_f2Modes.x;
	switch (mode)
	{
		case 0: Process2DQuadTessFactorsMax(g_f4TessFactors, g_f2Modes.y, rtf, ritf, itf);
			break;
		case 1: Process2DQuadTessFactorsMin(g_f4TessFactors, g_f2Modes.y, rtf, ritf, itf);
			break;
		case 2: Process2DQuadTessFactorsAvg(g_f4TessFactors, g_f2Modes.y, rtf, ritf, itf);
			break;
		default: Process2DQuadTessFactorsMax(g_f4TessFactors, g_f2Modes.y, rtf, ritf, itf);
			break;
	}

    O.fTessFactor[0] = rtf.x;
	O.fTessFactor[1] = rtf.y;
	O.fTessFactor[2] = rtf.z;
	O.fTessFactor[3] = rtf.w;
    O.fInsideTessFactor[0] = ritf.x;
	O.fInsideTessFactor[1] = ritf.y;

    return O;
}
[domain("quad")]
[partitioning("pow2")]
[outputtopology("triangle_cw")]
[patchconstantfunc("HS_QuadsConstant")]
[outputcontrolpoints(4)]
Position_Input HS_Quads_pow2( InputPatch<Position_Input, 4> I, uint uCPID : SV_OutputControlPointID )
{
    Position_Input O = (Position_Input)0;

    O.f3Position = I[uCPID].f3Position; 
    
    return O;
}
[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[patchconstantfunc("HS_QuadsConstant")]
[outputcontrolpoints(4)]
Position_Input HS_Quads_integer( InputPatch<Position_Input, 4> I, uint uCPID : SV_OutputControlPointID )
{
    Position_Input O = (Position_Input)0;

    O.f3Position = I[uCPID].f3Position; 
    
    return O;
}
[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[patchconstantfunc("HS_QuadsConstant")]
[outputcontrolpoints(4)]
Position_Input HS_Quads_fractionalodd( InputPatch<Position_Input, 4> I, uint uCPID : SV_OutputControlPointID )
{
    Position_Input O = (Position_Input)0;

    O.f3Position = I[uCPID].f3Position; 
    
    return O;
}
[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[patchconstantfunc("HS_QuadsConstant")]
[outputcontrolpoints(4)]
Position_Input HS_Quads_fractionaleven( InputPatch<Position_Input, 4> I, uint uCPID : SV_OutputControlPointID )
{
    Position_Input O = (Position_Input)0;

    O.f3Position = I[uCPID].f3Position; 
    
    return O;
}

[domain("quad")]
PS_RenderSceneInput DS_Quads( HS_ConstantOutput_Quad HSConstantData, const OutputPatch<Position_Input, 4> I, float2 uv : SV_DomainLocation )
{
    PS_RenderSceneInput O = (PS_RenderSceneInput)0;
	float3 f3Position; 

	float3 p[4]; 
	[unroll]
	for (uint i=0; i<4; i++)
	{
	    p[i]=I[i].f3Position;
	}
    f3Position = bilerpUV(p, uv);

    O.f4Position = mul( float4( f3Position.xyz, 1.0 ), g_f4x4WorldViewProjection );
        
    return O;
}

PS_RenderOutput PS_SolidColor( PS_RenderSceneInput I )
{
    PS_RenderOutput O;


	O.f4Color=float4(0.9,0.9,0.9,1);


    return O;
}
//=================================================================================================================================
// EOF
//=================================================================================================================================
