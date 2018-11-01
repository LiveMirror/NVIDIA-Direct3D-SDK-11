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

#include "DXUT.h"
#include "TileRing.h"

ID3D11InputLayout* TileRing::m_pInputLayout = NULL;

struct Adjacency
{
	// These are the size of the neighbours along +/- x or y axes.  For interior tiles
	// this is 1.  For edge tiles it is 0.5 or 2.0.
	float neighbourMinusX;
	float neighbourMinusY;
	float neighbourPlusX;
	float neighbourPlusY;
};

struct InstanceData
{
	float x,y;
	Adjacency adjacency;
};

TileRing::TileRing(ID3D11Device* pd3dDevice, int holeWidth, int outerWidth, float tileSize):
	m_holeWidth(holeWidth),
	m_outerWidth(outerWidth), 
	m_ringWidth((outerWidth - holeWidth) / 2),				// No remainder - see assert below.
	m_nTiles(outerWidth*outerWidth - holeWidth*holeWidth),
	m_tileSize(tileSize),
	m_pPositionsVB(NULL),
	m_pVBData(NULL)
{
	assert((outerWidth - holeWidth) % 2 == 0);
	CreateInstanceDataVB(pd3dDevice);
}

TileRing::~TileRing()
{
    delete[] m_pVBData;
	SAFE_RELEASE(m_pPositionsVB);
}

//static 
void TileRing::CreateInputLayout(ID3D11Device* pd3dDevice, ID3DX11EffectTechnique* pTech)
{
	HRESULT hr;

	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION_2D",     0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "ADJACENCY_SIZES", 0, DXGI_FORMAT_R32_FLOAT,    0, 8,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "ADJACENCY_SIZES", 1, DXGI_FORMAT_R32_FLOAT,    0, 12, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "ADJACENCY_SIZES", 2, DXGI_FORMAT_R32_FLOAT,    0, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "ADJACENCY_SIZES", 3, DXGI_FORMAT_R32_FLOAT,    0, 20, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	};
	const size_t N_ELEMS = sizeof(layout) / sizeof(D3D11_INPUT_ELEMENT_DESC);

	assert(pTech != NULL);	// Load this first.
	D3DX11_PASS_DESC passDesc;
	V(pTech->GetPassByIndex(0)->GetDesc(&passDesc));

	V(pd3dDevice->CreateInputLayout(layout, N_ELEMS, passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &m_pInputLayout));
}

// static 
void TileRing::ReleaseInputLayout()
{
    SAFE_RELEASE(m_pInputLayout);
}

bool TileRing::InRing(int x, int y) const
{
	assert(x >= 0 && x < m_outerWidth);
	assert(y >= 0 && y < m_outerWidth);
	return (x < m_ringWidth || y < m_ringWidth || x >= m_outerWidth - m_ringWidth || y >= m_outerWidth - m_ringWidth);
}

void TileRing::AssignNeighbourSizes(int x, int y, Adjacency* pAdj) const
{
	pAdj->neighbourPlusX  = 1.0f;
	pAdj->neighbourPlusY  = 1.0f;
	pAdj->neighbourMinusX = 1.0f;
	pAdj->neighbourMinusY = 1.0f;

	// TBD: these aren't necessarily 2x different.  Depends on the relative tiles sizes supplied to ring ctors.
	const float innerNeighbourSize = 0.5f;
	const float outerNeighbourSize = 2.0f;

	// Inner edges abut tiles that are smaller.  (But not on the inner-most.)
	if (m_holeWidth > 0)
	{
		if (y >= m_ringWidth && y < m_outerWidth-m_ringWidth)
		{
			if (x == m_ringWidth-1)
				pAdj->neighbourPlusX  = innerNeighbourSize;
			if (x == m_outerWidth - m_ringWidth)
				pAdj->neighbourMinusX = innerNeighbourSize;
		}
		if (x >= m_ringWidth && x < m_outerWidth-m_ringWidth)
		{
			if (y == m_ringWidth-1)
				pAdj->neighbourPlusY  = innerNeighbourSize;
			if (y == m_outerWidth - m_ringWidth)
				pAdj->neighbourMinusY = innerNeighbourSize;
		}
	}

	// Outer edges abut tiles that are larger.  We could skip this on the outer-most ring.  But it will
	// make almost zero visual or perf difference.
	if (x == 0)
		pAdj->neighbourMinusX = outerNeighbourSize;
	if (y == 0)
		pAdj->neighbourMinusY = outerNeighbourSize;
	if (x == m_outerWidth - 1)
		pAdj->neighbourPlusX  = outerNeighbourSize;
	if (y == m_outerWidth - 1)
		pAdj->neighbourPlusY  = outerNeighbourSize;
}

void TileRing::CreateInstanceDataVB(ID3D11Device* pd3dDevice)
{
    D3D11_SUBRESOURCE_DATA initData;
    ZeroMemory(&initData, sizeof(D3D11_SUBRESOURCE_DATA));
    
    int index = 0;
    m_pVBData = new InstanceData[m_nTiles];

	const float halfWidth = 0.5f * (float) m_outerWidth;
    for (int y = 0; y < m_outerWidth; ++y)
    {
        for (int x = 0; x < m_outerWidth; ++x)
        {
			if (InRing(x,y))
			{
				m_pVBData[index].x = m_tileSize * ((float) x - halfWidth);
				m_pVBData[index].y = m_tileSize * ((float) y - halfWidth);
				AssignNeighbourSizes(x, y, &(m_pVBData[index].adjacency));
				index++;
			}
        }
    }
	assert(index == m_nTiles);
    
    initData.pSysMem = m_pVBData;
    D3D11_BUFFER_DESC vbDesc =
    {
        sizeof(InstanceData) * m_nTiles,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_VERTEX_BUFFER,
        0,
        0
    };

    HRESULT hr;
    V(pd3dDevice->CreateBuffer(&vbDesc, &initData, &m_pPositionsVB));
}

void TileRing::SetRenderingState(ID3D11DeviceContext* pContext) const
{
	UINT uiVertexStrides[1] = {sizeof(InstanceData)};
    UINT uOffsets[1] = {0};

	pContext->IASetInputLayout(m_pInputLayout);
	pContext->IASetVertexBuffers(0, 1, &m_pPositionsVB, uiVertexStrides, uOffsets);
}
