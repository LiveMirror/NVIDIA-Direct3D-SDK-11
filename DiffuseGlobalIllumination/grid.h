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

#ifndef GRID_H
#define GRID_H

#include <d3dx11effect.h>
#include "math.h"
#include "assert.h"
#include "dxut.h" 

#ifndef SAFE_ACQUIRE
#define SAFE_ACQUIRE(dst, p)      { if(dst) { SAFE_RELEASE(dst); } if (p) { (p)->AddRef(); } dst = (p); }
#endif

inline void ComputeRowsColsForFlat3DTexture( int depth, int *outCols, int *outRows )
{
    // Compute # of rows and cols for a "flat 3D-texture" configuration
    // (in this configuration all the slices in the volume are spread in a single 2D texture)
    int rows =(int)floorf(sqrtf((float)depth));
    int cols = rows;
    while( rows * cols < depth ) {
        cols++;
    }
    assert( rows*cols >= depth );
    
    *outCols = cols;
    *outRows = rows;
}


struct VS_INPUT_GRID_STRUCT
{   
    D3DXVECTOR3 Pos; // Clip space position for slice vertices
    D3DXVECTOR3 Tex; // Cell coordinates in 0-"texture dimension" range
};

class Grid
{
public:
    Grid( ID3D11Device* pd3dDevice,ID3D11DeviceContext* pd3dContext );
    virtual ~Grid( void );

    HRESULT Initialize( int gridWidth, int gridHeight, int gridDepth,void* shaderBytecode, SIZE_T shaderBytecodeLength );

    void DrawSlices        ( void );
    void DrawSlice         ( int slice );
    void DrawSlicesToScreen( void );
    void DrawBoundaryQuads ( void );
    void DrawBoundaryLines ( void );
    
    int  GetCols(){return m_cols;};
    int  GetRows(){return m_rows;};

    int  GetDimX() {return m_dim[0]; }
    int  GetDimY() {return m_dim[1]; }
    int  GetDimZ() {return m_dim[2]; }

protected:

    
    HRESULT CreateVertexBuffers  (void* shaderBytecode, SIZE_T shaderBytecodeLength);
    void InitScreenSlice        (VS_INPUT_GRID_STRUCT** vertices, int z, int& index);
    void InitSlice             (int z, VS_INPUT_GRID_STRUCT** vertices, int& index);
    void InitLine              (float x1, float y1, float x2, float y2, int z,
                                    VS_INPUT_GRID_STRUCT** vertices, int& index);
    void InitBoundaryQuads     (VS_INPUT_GRID_STRUCT** vertices, int& index);
    void InitBoundaryLines     (VS_INPUT_GRID_STRUCT** vertices, int& index);

    HRESULT CreateLayout( D3D11_INPUT_ELEMENT_DESC* layoutDesc, UINT numElements, void* shaderBytecode, SIZE_T shaderBytecodeLength, ID3D11InputLayout** layout);
    HRESULT CreateVertexBuffer( int ByteWidth, UINT bindFlags, ID3D11Buffer** vertexBuffer,VS_INPUT_GRID_STRUCT* vertices,int numVertices);

    // D3D11 helper functions
    void DrawPrimitive( D3D11_PRIMITIVE_TOPOLOGY PrimitiveType, ID3D11InputLayout* layout, ID3D11Buffer** vertexBuffer,UINT* stride, UINT* offset, UINT StartVertex, UINT VertexCount );

    // Internal State
    //===============
    ID3D11Device*              m_pD3DDevice;
    ID3D11DeviceContext*       m_pD3DContext;

    ID3D11InputLayout*         m_layout;
    ID3D11Buffer*              m_renderQuadBuffer;
    ID3D11Buffer*              m_slicesBuffer;
    ID3D11Buffer*              m_boundarySlicesBuffer;
    ID3D11Buffer*              m_boundaryLinesBuffer;

    int m_numVerticesRenderQuad;
    int m_numVerticesSlices;
    int m_numVerticesBoundarySlices;
    int m_numVerticesBoundaryLines;
    int m_cols;
    int m_rows;

    int m_dim[3];
};

#endif

