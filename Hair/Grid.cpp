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

#pragma warning(disable:4201) //avoid warning from mmsystem.h "warning C4201: nonstandard extension used : nameless struct/union"

#include "Grid.h"
#include <math.h>
#include "d3dx9math.h"

struct VS_INPUT_FLUIDSIM_STRUCT
{   
    D3DXVECTOR3 Pos; // Clip space position for slice vertices
    D3DXVECTOR3 Tex; // Cell coordinates in 0-"texture dimension" range
};

Grid::Grid( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext) : m_pD3DDevice(NULL),m_pD3DContext(NULL), m_layout(NULL), m_renderQuadBuffer(NULL),
    m_slicesBuffer(NULL), m_boundarySlicesBuffer(NULL), m_boundaryLinesBuffer(NULL), 
    m_numVerticesRenderQuad(0), m_numVerticesSlices(0), m_numVerticesBoundarySlices(0),
    m_numVerticesBoundaryLines(0), m_cols(0), m_rows(0)
{
    SAFE_ACQUIRE(m_pD3DDevice, pd3dDevice);
	SAFE_ACQUIRE(m_pD3DContext, pd3dContext);
}

HRESULT Grid::Initialize( int gridWidth, int gridHeight, int gridDepth,ID3DX11EffectTechnique* technique )
{
    HRESULT hr(S_OK);

    m_dim[0] = gridWidth;
    m_dim[1] = gridHeight;
    m_dim[2] = gridDepth;

    ComputeRowColsForFlat3DTexture(m_dim[2], &m_cols, &m_rows);

    V_RETURN(CreateVertexBuffers(technique));

    return S_OK;
}

Grid::~Grid()
{
    SAFE_RELEASE(m_layout);
    SAFE_RELEASE(m_renderQuadBuffer);
    SAFE_RELEASE(m_slicesBuffer);
    SAFE_RELEASE(m_boundarySlicesBuffer);
    SAFE_RELEASE(m_boundaryLinesBuffer);

    SAFE_RELEASE(m_pD3DDevice);    
	SAFE_RELEASE(m_pD3DContext);
}


HRESULT Grid::CreateVertexBuffers( ID3DX11EffectTechnique* technique )
{
    HRESULT hr(S_OK);

    // Create layout
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = 
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,       0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT,       0,12, D3D11_INPUT_PER_VERTEX_DATA, 0 }, 
    };
    UINT numElements = sizeof(layoutDesc)/sizeof(layoutDesc[0]);
    CreateLayout( layoutDesc, numElements, technique, &m_layout);

    int index(0);
    VS_INPUT_FLUIDSIM_STRUCT *renderQuad(NULL);
    VS_INPUT_FLUIDSIM_STRUCT *slices(NULL);
    VS_INPUT_FLUIDSIM_STRUCT *boundarySlices(NULL);
    VS_INPUT_FLUIDSIM_STRUCT *boundaryLines(NULL);

    try
    {
        #define VERTICES_PER_SLICE 6
        #define VERTICES_PER_LINE 2
        #define LINES_PER_SLICE 4

        m_numVerticesRenderQuad = VERTICES_PER_SLICE * m_dim[2];
        renderQuad = new VS_INPUT_FLUIDSIM_STRUCT[ m_numVerticesRenderQuad ];

        m_numVerticesSlices = VERTICES_PER_SLICE * (m_dim[2] - 2);
        slices = new VS_INPUT_FLUIDSIM_STRUCT[ m_numVerticesSlices ];

        m_numVerticesBoundarySlices = VERTICES_PER_SLICE * 2;
        boundarySlices = new VS_INPUT_FLUIDSIM_STRUCT[ m_numVerticesBoundarySlices ];

        m_numVerticesBoundaryLines = VERTICES_PER_LINE * LINES_PER_SLICE * (m_dim[2]);
        boundaryLines = new VS_INPUT_FLUIDSIM_STRUCT[ m_numVerticesBoundaryLines ];
    }
    catch(...)
    {
        hr = E_OUTOFMEMORY;
        goto cleanup;
    }
    
    assert(renderQuad && m_numVerticesSlices && m_numVerticesBoundarySlices && m_numVerticesBoundaryLines);

    // Vertex buffer for "m_dim[2]" quads to draw all the slices of the 3D-texture as a flat 3D-texture
    // (used to draw all the individual slices at once to the screen buffer)
    index = 0;
    for(int z=0; z<m_dim[2]; z++)
        InitScreenSlice(&renderQuad,z,index);
    V_RETURN(CreateVertexBuffer(sizeof(VS_INPUT_FLUIDSIM_STRUCT)*m_numVerticesRenderQuad,
        D3D11_BIND_VERTEX_BUFFER, &m_renderQuadBuffer, renderQuad, m_numVerticesRenderQuad));

    // Vertex buffer for "m_dim[2]" quads to draw all the slices to a 3D texture
    // (a Geometry Shader is used to send each quad to the appropriate slice)
    index = 0;
    for( int z = 1; z < m_dim[2]-1; z++ )
        InitSlice( z, &slices, index );
    assert(index==m_numVerticesSlices);
    V_RETURN(CreateVertexBuffer(sizeof(VS_INPUT_FLUIDSIM_STRUCT)*m_numVerticesSlices,
        D3D11_BIND_VERTEX_BUFFER, &m_slicesBuffer, slices , m_numVerticesSlices));

    // Vertex buffers for boundary geometry
    //   2 boundary slices
    index = 0;
    InitBoundaryQuads(&boundarySlices,index);
    assert(index==m_numVerticesBoundarySlices);
    V_RETURN(CreateVertexBuffer(sizeof(VS_INPUT_FLUIDSIM_STRUCT)*m_numVerticesBoundarySlices,
        D3D11_BIND_VERTEX_BUFFER, &m_boundarySlicesBuffer, boundarySlices, m_numVerticesBoundarySlices));
    //   ( 4 * "m_dim[2]" ) boundary lines
    index = 0;
    InitBoundaryLines(&boundaryLines,index);
    assert(index==m_numVerticesBoundaryLines);
    V_RETURN(CreateVertexBuffer(sizeof(VS_INPUT_FLUIDSIM_STRUCT)*m_numVerticesBoundaryLines,
        D3D11_BIND_VERTEX_BUFFER, &m_boundaryLinesBuffer, boundaryLines, m_numVerticesBoundaryLines));

cleanup:
    delete [] renderQuad;
    renderQuad = NULL;

    delete [] slices;
    slices = NULL;

    delete [] boundarySlices;
    boundarySlices = NULL;

    delete [] boundaryLines;
    boundaryLines = NULL;

    return hr;
}

void Grid::InitScreenSlice(VS_INPUT_FLUIDSIM_STRUCT** vertices, int z, int& index )
{
    VS_INPUT_FLUIDSIM_STRUCT tempVertex1;
    VS_INPUT_FLUIDSIM_STRUCT tempVertex2;
    VS_INPUT_FLUIDSIM_STRUCT tempVertex3;
    VS_INPUT_FLUIDSIM_STRUCT tempVertex4;

    // compute the offset (px, py) in the "flat 3D-texture" space for the slice with given 'z' coordinate
    int column      = z % m_cols;
    int row         = (int)floorf((float)(z/m_cols));
    int px = column * m_dim[0];
    int py = row    * m_dim[1];

    float w = float(m_dim[0]);
    float h = float(m_dim[1]);

    float Width  = float(m_cols * m_dim[0]);
    float Height = float(m_rows * m_dim[1]);

    float left   = -1.0f + (px*2.0f/Width);
    float right  = -1.0f + ((px+w)*2.0f/Width);
    float top    =  1.0f - py*2.0f/Height;
    float bottom =  1.0f - ((py+h)*2.0f/Height);

    tempVertex1.Pos   = D3DXVECTOR3( left   , top    , 0.0f      );
    tempVertex1.Tex   = D3DXVECTOR3( 0      , 0      , float(z)  );

    tempVertex2.Pos   = D3DXVECTOR3( right  , top    , 0.0f      );
    tempVertex2.Tex   = D3DXVECTOR3( w      , 0      , float(z)  );

    tempVertex3.Pos   = D3DXVECTOR3( right  , bottom , 0.0f      );
    tempVertex3.Tex   = D3DXVECTOR3( w      , h      , float(z)  );

    tempVertex4.Pos   = D3DXVECTOR3( left   , bottom , 0.0f      );
    tempVertex4.Tex   = D3DXVECTOR3( 0      , h      , float(z)  );

    (*vertices)[index++] = tempVertex1;
    (*vertices)[index++] = tempVertex2;
    (*vertices)[index++] = tempVertex3;
    (*vertices)[index++] = tempVertex1;
    (*vertices)[index++] = tempVertex3;
    (*vertices)[index++] = tempVertex4;
}

void Grid::InitSlice( int z, VS_INPUT_FLUIDSIM_STRUCT** vertices, int& index )
{
    VS_INPUT_FLUIDSIM_STRUCT tempVertex1;
    VS_INPUT_FLUIDSIM_STRUCT tempVertex2;
    VS_INPUT_FLUIDSIM_STRUCT tempVertex3;
    VS_INPUT_FLUIDSIM_STRUCT tempVertex4;

    int w = m_dim[0];
    int h = m_dim[1];

    float left   = -1.0f + 2.0f/w;
    float right  =  1.0f - 2.0f/w;
    float top    =  1.0f - 2.0f/h;
    float bottom = -1.0f + 2.0f/h;

    tempVertex1.Pos = D3DXVECTOR3( left     , top       , 0.0f      );
    tempVertex1.Tex = D3DXVECTOR3( 1.0f     , 1.0f      , float(z)  );

    tempVertex2.Pos = D3DXVECTOR3( right    , top       , 0.0f      );
    tempVertex2.Tex = D3DXVECTOR3( (w-1.0f) , 1.0f      , float(z)  );

    tempVertex3.Pos = D3DXVECTOR3( right    , bottom    , 0.0f      );
    tempVertex3.Tex = D3DXVECTOR3( (w-1.0f) , (h-1.0f)  , float(z)  );

    tempVertex4.Pos = D3DXVECTOR3( left     , bottom    , 0.0f      );
    tempVertex4.Tex = D3DXVECTOR3( 1.0f     , (h-1.0f)  , float(z)  );


    (*vertices)[index++] = tempVertex1;
    (*vertices)[index++] = tempVertex2;
    (*vertices)[index++] = tempVertex3;
    (*vertices)[index++] = tempVertex1;
    (*vertices)[index++] = tempVertex3;
    (*vertices)[index++] = tempVertex4;

}

void Grid::InitLine( float x1, float y1, float x2, float y2, int z,
                    VS_INPUT_FLUIDSIM_STRUCT** vertices, int& index )
{
    VS_INPUT_FLUIDSIM_STRUCT tempVertex;
    int w = m_dim[0];
    int h = m_dim[1];

    tempVertex.Pos  = D3DXVECTOR3( x1*2.0f/w - 1.0f , -y1*2.0f/h + 1.0f , 0.5f      );
    tempVertex.Tex  = D3DXVECTOR3( 0.0f             , 0.0f              , float(z)  );
    (*vertices)[index++] = tempVertex;

    tempVertex.Pos  = D3DXVECTOR3( x2*2.0f/w - 1.0f , -y2*2.0f/h + 1.0f , 0.5f      );
    tempVertex.Tex  = D3DXVECTOR3( 0.0f             , 0.0f              , float(z)  );
    (*vertices)[index++] = tempVertex;
}


void Grid::InitBoundaryQuads( VS_INPUT_FLUIDSIM_STRUCT** vertices, int& index )
{
    InitSlice( 0, vertices, index );
    InitSlice( m_dim[2]-1, vertices, index );
}

void Grid::InitBoundaryLines( VS_INPUT_FLUIDSIM_STRUCT** vertices, int& index )
{
    int w = m_dim[0];
    int h = m_dim[1];

    for( int z = 0; z < m_dim[2]; z++ )
    {
        // bottom
        InitLine( 0.0f, 1.0f, float(w), 1.0f, z, vertices, index );
        // top
        InitLine( 0.0f, float(h), float(w), float(h), z, vertices, index );
        // left
        InitLine( 1.0f, 0.0f, 1.0f, float(h), z, vertices, index );
        //right
        InitLine( float(w), 0.0f, float(w), float(h), z, vertices, index );
    }
}

void Grid::DrawSlices( void )
{
    UINT stride[1] = { sizeof(VS_INPUT_FLUIDSIM_STRUCT) };
    UINT offset[1] = { 0 };
	DrawPrimitive( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, m_layout, &m_slicesBuffer,stride, offset, 0, m_numVerticesSlices );
    //DrawPrimitive( D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST, m_layout, &m_slicesBuffer,stride, offset, 0, m_numVerticesSlices );

}

void Grid::DrawSlice( int slice )
{
    UINT stride[1] = { sizeof(VS_INPUT_FLUIDSIM_STRUCT) };
    UINT offset[1] = { 0 };
    DrawPrimitive( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, m_layout, &m_slicesBuffer,
        stride, offset, VERTICES_PER_SLICE*slice, VERTICES_PER_SLICE );
}

void Grid::DrawSlicesToScreen( void )
{
    UINT stride[1] = { sizeof(VS_INPUT_FLUIDSIM_STRUCT) };
    UINT offset[1] = { 0 };
    DrawPrimitive( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, m_layout, &m_renderQuadBuffer,
        stride, offset, 0, m_numVerticesRenderQuad );

}

void Grid::DrawBoundaryQuads( void )
{
    UINT stride[1] = { sizeof(VS_INPUT_FLUIDSIM_STRUCT) };
    UINT offset[1] = { 0 };
    DrawPrimitive( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, m_layout, &m_boundarySlicesBuffer,
        stride, offset, 0, m_numVerticesBoundarySlices );

}

void Grid::DrawBoundaryLines( void )
{

    UINT stride[1] = { sizeof(VS_INPUT_FLUIDSIM_STRUCT) };
    UINT offset[1] = { 0 };
    DrawPrimitive( D3D11_PRIMITIVE_TOPOLOGY_LINELIST, m_layout, &m_boundaryLinesBuffer, 
        stride, offset, 0, m_numVerticesBoundaryLines  );

}

void Grid::DrawPrimitive( D3D11_PRIMITIVE_TOPOLOGY PrimitiveType, ID3D11InputLayout* layout,
                         ID3D11Buffer** vertexBuffer,UINT* stride, UINT* offset, UINT StartVertex,
                         UINT VertexCount )
{
    m_pD3DContext->IASetPrimitiveTopology( PrimitiveType );
    m_pD3DContext->IASetInputLayout( layout );
    m_pD3DContext->IASetVertexBuffers( 0, 1, vertexBuffer, stride, offset );
    m_pD3DContext->Draw( VertexCount, StartVertex ); 
}

HRESULT Grid::CreateLayout( D3D11_INPUT_ELEMENT_DESC* layoutDesc, UINT numElements,
                           ID3DX11EffectTechnique* technique, ID3D11InputLayout** layout)
{
    HRESULT hr(S_OK);
    D3DX11_PASS_DESC PassDesc;
    technique->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN(m_pD3DDevice->CreateInputLayout( layoutDesc, numElements, 
        PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, layout ));
    return S_OK;
}

HRESULT Grid::CreateVertexBuffer( int ByteWidth, UINT bindFlags, ID3D11Buffer** vertexBuffer,
                                 VS_INPUT_FLUIDSIM_STRUCT* vertices,int numVertices)
{
    HRESULT hr(S_OK);

    D3D11_BUFFER_DESC bd;
    bd.ByteWidth = ByteWidth;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D11_SUBRESOURCE_DATA) );
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = ByteWidth/numVertices;

    V_RETURN( m_pD3DDevice->CreateBuffer( &bd, &InitData,vertexBuffer  ) );

    return S_OK;
}
