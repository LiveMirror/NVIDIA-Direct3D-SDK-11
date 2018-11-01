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

#ifndef __MULTIANIMATION_H__
#define __MULTIANIMATION_H__


#pragma warning( push, 3 )
#pragma warning(disable:4786 4788)
#include <vector>
#pragma warning( pop )
#pragma warning(disable:4786 4788)

#include <d3d9.h>
#include <d3dx9.h>


//-----------------------------------------------------------------------------
// Forward declarations

class CMultiAnim;


//-----------------------------------------------------------------------------
// Name: class CMultiAnimAllocateHierarchy
// Desc: Inheriting from ID3DXAllocateHierarchy, this class handles the
//       allocation and Release of the memory used by animation frames and
//       meshes.  Applications derive their own version of this class so
//       that they can customize the behavior of allocation and Release.
//-----------------------------------------------------------------------------
class CMultiAnimAllocateHierarchy : public ID3DXAllocateHierarchy
{
    // callback to create a D3DXFRAME-derived object and Initialize it
    STDMETHOD( CreateFrame )( THIS_ LPCSTR Name, LPD3DXFRAME *ppNewFrame );
    // callback to create a D3DXMESHCONTAINER-derived object and Initialize it
    STDMETHOD( CreateMeshContainer )( THIS_ LPCSTR Name, CONST D3DXMESHDATA * pMeshData, 
                            CONST D3DXMATERIAL * pMaterials, CONST D3DXEFFECTINSTANCE * pEffectInstances,
                            DWORD NumMaterials, CONST DWORD * pAdjacency, LPD3DXSKININFO pSkinInfo, 
                            LPD3DXMESHCONTAINER * ppNewMeshContainer );
    // callback to Release a D3DXFRAME-derived object
    STDMETHOD( DestroyFrame )( THIS_ LPD3DXFRAME pFrameToFree );
    // callback to Release a D3DXMESHCONTAINER-derived object
    STDMETHOD( DestroyMeshContainer )( THIS_ LPD3DXMESHCONTAINER pMeshContainerToFree );

public:
    CMultiAnimAllocateHierarchy();

    // Setup method
    STDMETHOD( SetMA )( THIS_ CMultiAnim *pMA );

private:
    CMultiAnim *m_pMA;
};




//-----------------------------------------------------------------------------
// Name: struct MultiAnimFrame
// Desc: Inherits from D3DXFRAME.  This represents an animation frame, or
//       bone.
//-----------------------------------------------------------------------------
struct MultiAnimFrame : public D3DXFRAME
{
    D3DXMATRIXA16        CombinedTransformationMatrix;    // this is just a fully aggregated matrix for each frame
};




//-----------------------------------------------------------------------------
// Name: struct MultiAnimMC
// Desc: Inherits from D3DXMESHCONTAINER.  This represents a mesh object
//       that gets its vertices blended and rendered based on the frame
//       information in its hierarchy.
//-----------------------------------------------------------------------------
struct MultiAnimMC : public D3DXMESHCONTAINER
{
    LPD3DXMESH          m_pWorkingMesh;
    CHAR                m_pTextureFilename[MAX_PATH];
    
    D3DXMATRIX          m_pStaticCombinedMatrix;
    MultiAnimFrame      *m_pParentFrame;
    DWORD               m_dwNumPaletteEntries;
    DWORD               m_dwMaxNumFaceInfls;
    DWORD               m_dwNumAttrGroups;
    LPD3DXBUFFER        m_pBufBoneCombos;
};




//-----------------------------------------------------------------------------
// Name: class CMultiAnim
// Desc: This class encapsulates a mesh hierarchy (typically loaded from an
//       .X file). 
//-----------------------------------------------------------------------------
class CMultiAnim
{
    friend class CMultiAnimAllocateHierarchy;
    friend struct MultiAnimFrame;
    friend struct MultiAnimMC;

public:

    LPDIRECT3DDEVICE9         m_pDevice;

    MultiAnimFrame *          m_pFrameRoot;           // shared between all instances
    LPD3DXANIMATIONCONTROLLER m_pAC;                  // AC that all children clone from -- to clone clean, no keys

    // useful data an app can retrieve
    float                     m_fBoundingRadius;

private:

public:

                              CMultiAnim();
    virtual                   ~CMultiAnim();

    virtual HRESULT           Setup( LPDIRECT3DDEVICE9 pDevice, TCHAR sXFile[], CMultiAnimAllocateHierarchy *pAH, LPD3DXLOADUSERDATA pLUD = NULL );
    virtual HRESULT           Cleanup( CMultiAnimAllocateHierarchy * pAH );

            LPDIRECT3DDEVICE9 GetDevice();
            float             GetBoundingRadius();
};

#endif // #ifndef __MULTIANIMATION_H__
