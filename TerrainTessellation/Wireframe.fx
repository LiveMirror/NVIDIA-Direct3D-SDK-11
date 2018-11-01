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

struct WireVertex
{
    float4 clipPos : SV_POSITION;
    float2 vWorldXZ        : TEXCOORD1;
    noperspective float4 edgeA: TEXCOORD2;
    float3 vNormal : NORMAL;
	float3 debugColour : COLOR;
};

float4 Viewport;

// From window pixel pos to projection frame at the specified z (view frame). 
float2 projToWindow(in float4 pos)
{
    return float2(  Viewport.x*0.5*((pos.x/pos.w) + 1) + Viewport.z,
                    Viewport.y*0.5*(1-(pos.y/pos.w)) + Viewport.w );
}

// Solid wire courtesy of Sam Cake's SDK sample.  This is a straight copy-and-paste, except
// that I've cut out his complex, general case for vertices behind the eye because: 1) our
// triangles here are relatively small, and 2) it's a debug mode so we don't care.
[maxvertexcount(3)]
void GSSolidWire(triangle MeshVertex input[3], inout TriangleStream<WireVertex> outStream )
{
    WireVertex output;

   // Transform position to window space
    float2 points[3];
    points[0] = projToWindow(input[0].vPosition);
    points[1] = projToWindow(input[1].vPosition);
    points[2] = projToWindow(input[2].vPosition);

    output.edgeA = float4(0,0,0,0);

    // Compute the edges vectors of the transformed triangle
    float2 edges[3];
    edges[0] = points[1] - points[0];
    edges[1] = points[2] - points[1];
    edges[2] = points[0] - points[2];

    // Store the length of the edges
    float lengths[3];
    lengths[0] = length(edges[0]);
    lengths[1] = length(edges[1]);
    lengths[2] = length(edges[2]);

    // Compute the cos angle of each vertices
    float cosAngles[3];
    cosAngles[0] = dot( -edges[2], edges[0]) / ( lengths[2] * lengths[0] );
    cosAngles[1] = dot( -edges[0], edges[1]) / ( lengths[0] * lengths[1] );
    cosAngles[2] = dot( -edges[1], edges[2]) / ( lengths[1] * lengths[2] );

    // The height for each vertices of the triangle
    float heights[3];
    heights[1] = lengths[0]*sqrt(1 - cosAngles[0]*cosAngles[0]);
    heights[2] = lengths[1]*sqrt(1 - cosAngles[1]*cosAngles[1]);
    heights[0] = lengths[2]*sqrt(1 - cosAngles[2]*cosAngles[2]);

	/* Experimental: try to show over and under tessellation by computing triangle area.
	   (The HS/DS try to make it constant size in screen-space.)
	float area = 0.5 * heights[0] * lengths[0];
	float3 targetSizeColour;
	if (area > 36)
		targetSizeColour = float3(0,0,1);	// blue = under tessellated
	else if (area < 28)
		targetSizeColour = float3(1,0,0);	// red = over tessellated
	else
		targetSizeColour = float3(0,1,0);	// green = just right
	*/
	
	output.clipPos  =( input[0].vPosition );
    output.vNormal  =( input[0].vNormal );
    output.vWorldXZ        = input[0].vWorldXZ;
	output.debugColour = input[0].debugColour;
    output.edgeA[0] = 0;
    output.edgeA[1] = heights[0];
    output.edgeA[2] = 0;
    outStream.Append( output );

    output.clipPos  = ( input[1].vPosition );
    output.vNormal  =( input[1].vNormal );
    output.vWorldXZ        = input[1].vWorldXZ;
	output.debugColour = input[1].debugColour;
    output.edgeA[0] = 0;
    output.edgeA[1] = 0;
    output.edgeA[2] = heights[1];
    outStream.Append( output );

    output.clipPos  = ( input[2].vPosition );
    output.vNormal  =( input[2].vNormal );
    output.vWorldXZ        = input[2].vWorldXZ;
	output.debugColour = input[1].debugColour;
    output.edgeA[0] = heights[2];
    output.edgeA[1] = 0;
    output.edgeA[2] = 0;
    outStream.Append( output );

    outStream.RestartStrip();
}

float g_WireWidth = 1;
float g_WireAlpha = 0.7;

float4 PSSolidWire(WireVertex input) : SV_Target
{
	MeshVertex mv;
	mv.vPosition = 0;
	mv.vNormal  = input.vNormal;
	mv.vWorldXZ = input.vWorldXZ;
	
	// Invoke the non-wire PS to reuse its shading calculation, whatever that is.
	float4 color = SmoothShadePS(mv);
	
    // Compute the shortest distance between the fragment and the edges.
    float dist = min ( min (input.edgeA.x, input.edgeA.y), input.edgeA.z);

    // Cull fragments too far from the edge.
    if (dist < 0.5*g_WireWidth+1)
    {
		// Map the computed distance to the [0,2] range on the border of the line.
		dist = clamp((dist - (0.5*g_WireWidth - 1)), 0, 2);

		// Alpha is computed from the function exp2(-2(x)^2).
		dist *= dist;
		float fallOffAlpha = exp2(-2*dist);

		// Standard wire color
		const float4 wireColour = float4(input.debugColour, 1);
		color = lerp(color, wireColour, g_WireAlpha * fallOffAlpha);
	}
	
    return color;
}
