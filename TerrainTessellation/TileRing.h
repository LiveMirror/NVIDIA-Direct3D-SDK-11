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

//----------------------------------------------------------------------------------
// Defines and draws one ring of tiles in a concentric, nested set of rings.  Each ring
// is a different LOD and uses a different tile/patch size.  These are actually square
// rings.  (Is there a term for that?)  But they could conceivably change to circular.
// The inner-most LOD is a square, represented by a degenerate ring.
//
#pragma once

#include <d3d11.h>
#include <d3dx11.h>
#include "d3dx11effect.h"

struct InstanceData;
struct Adjacency;

// Int dimensions specified to the ctor are in numbers of tiles.  It's symmetrical in
// each direction.  (Don't read much into the exact numbers of #s in this diagram.)
//
//    <-   outerWidth  ->
//    ###################
//    ###################
//    ###             ###
//    ###<-holeWidth->###
//    ###             ###
//    ###    (0,0)    ###
//    ###             ###
//    ###             ###
//    ###             ###
//    ###################
//    ###################
//
class TileRing
{
public:
	// holeWidth & outerWidth are nos. of tiles
	// tileSize is a world-space length
	TileRing(ID3D11Device*, int holeWidth, int outerWidth, float tileSize);
	~TileRing();

	static void CreateInputLayout(ID3D11Device*, ID3DX11EffectTechnique*);
	static void ReleaseInputLayout();

	void SetRenderingState(ID3D11DeviceContext*) const;

	int   outerWidth() const { return m_outerWidth; }
	int   nTiles()     const { return m_nTiles; }
	float tileSize()   const { return m_tileSize; }

private:
	void CreateInstanceDataVB(ID3D11Device*);
	bool InRing(int x, int y) const;
	void AssignNeighbourSizes(int x, int y, Adjacency*) const;

	static ID3D11InputLayout* m_pInputLayout;
	ID3D11Buffer*             m_pPositionsVB;

	const int m_holeWidth, m_outerWidth, m_ringWidth;
	const int m_nTiles;
	const float m_tileSize;
	InstanceData* m_pVBData;

	// Revoked:
	TileRing(const TileRing&);
	TileRing& operator=(const TileRing&);
};
