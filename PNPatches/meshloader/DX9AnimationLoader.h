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

#pragma once

#include "MultiAnimation.h"
#include "../mymath/mymath.h"
#include "bone_weights.h"

class CharacterAnimation
{
    float   duration;
    float   timeStep;
    int     bonesNum;
    
    std::vector<D3DXMATRIX*> frames;

public:
    CharacterAnimation(float _duration, int _bonesNum, float _timeStep = 1.0f/30);
    ~CharacterAnimation();

    int GetBonesNum(void) { return bonesNum; }
    int GetFramesNum(void) { return (int)frames.size(); }
    void AddFrame(D3DXMATRIX* pFrame) { frames.push_back(pFrame); }
    D3DXMATRIX* GetFrame(int frame);
    D3DXMATRIX* GetFrame(float time); 
};

class DX9AnimationLoader
{
    friend class CAnimInstance;
    friend class CMultiAnim;
    friend struct MultiAnimFrame;
    friend struct MultiAnimMC;
    friend class CMultiAnimAllocateHierarchy;
    friend class AnimatedCharacter;
    friend class AnimatedCharacterInstance;

public:

    // Input/intermediate structures
    WCHAR fullFilename[MAX_PATH];
    CMultiAnim *multiAnim;
    CMultiAnimAllocateHierarchy *AH;
    IDirect3D9 *pD3D9;
    IDirect3DDevice9*pDev9;
    CONST D3D10_INPUT_ELEMENT_DESC*playout;
    int cElements;

    std::vector<std::string> frameNames;
    std::vector<std::string> boneNames;
    std::vector<std::string> attachmentNames;
    std::vector<LPD3DXSKININFO> pSkinInfos;
    std::vector<MultiAnimMC*> meshList;
    D3DXMATRIX *pBoneOffsetMatrices;    // this is the bind pose matrices
    D3DXMATRIX **pBoneMatrixPtrs;       // pointers into the skeleton for each bone, this gets updates as animation continues
  
    DX9AnimationLoader();
    HRESULT Load(LPWSTR filename, bool bLoadAnimations=true, float timeStep = 1/30.f);

    void CreateDevice();
    HRESULT LoadDX9Data(LPWSTR filename);
    void FillMeshList(D3DXFRAME *pFrame, D3DXFRAME *pFrameParent, const CHAR*pLastNameFound);
    void InitBuffers(bool bAddSkeleton=true);
    void ExtractSkeleton();
    void Cleanup();

    void GetAnimationData(CharacterAnimation** ppAnimation, BoneWeights& boneWeights, float timeStep = 1.0f/30);

    HRESULT CloneMesh(UINT meshIndex, D3DVERTEXELEMENT9* pDecl, LPD3DXMESH* ppMesh, D3DXMATRIX& staticMatrix);
    D3DXMATRIX GetStaticCombinedMatrix(UINT meshIndex);

    void ExtractFrame(CharacterAnimation** ppAnimation);
    void UpdateFrames( MultiAnimFrame * pFrame, D3DXMATRIX * pmxBase);
    LPCSTR FindLastBoneParentOf(MultiAnimFrame * pFrameRoot,MultiAnimMC *pMeshContainer, LPCSTR pLastBoneName);
};