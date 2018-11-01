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
#include "SDKmisc.h"
#include "DX9AnimationLoader.h"
#include <strsafe.h>

CharacterAnimation::CharacterAnimation(float _duration, int _bonesNum, float _timeStep)
{
    duration = _duration;
    timeStep = _timeStep;
    bonesNum = _bonesNum;
}

CharacterAnimation::~CharacterAnimation()
{
    while (frames.empty() == false)
    {
        delete [] frames.back();
        frames.pop_back();
    }
}

D3DXMATRIX* CharacterAnimation::GetFrame(int frame)
{
    int framesNum = (int)frames.size();

    if (framesNum)
        return frames[frame%framesNum];
    else
        return NULL;
}

D3DXMATRIX* CharacterAnimation::GetFrame(float time)
{
    int framesNum = (int)frames.size();

    if (framesNum)
    {
        int frame = int(time / timeStep);
        return frames[frame%framesNum];
    }
    else
        return NULL;
}

DX9AnimationLoader::DX9AnimationLoader()
{
    multiAnim = NULL;
    AH = NULL;
    pD3D9 = NULL;
    pDev9 = NULL;
    pBoneMatrixPtrs = NULL;
    pBoneOffsetMatrices = NULL;
}

HRESULT DX9AnimationLoader::Load(LPWSTR filename, bool bLoadAnimations, float timeStep)
{
    HRESULT hr = S_OK;

    CreateDevice();
    
    hr = LoadDX9Data(filename);
    if(hr != S_OK) return hr;

    FillMeshList((D3DXFRAME*)multiAnim->m_pFrameRoot, NULL, NULL);    

    return hr;
}

void DX9AnimationLoader::CreateDevice()
{
    HRESULT hr;

    pD3D9 = Direct3DCreate9( D3D_SDK_VERSION );
    assert( pD3D9 );

    D3DPRESENT_PARAMETERS pp;
    pp.BackBufferWidth = 320;
    pp.BackBufferHeight = 240;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    pp.MultiSampleQuality = 0;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = GetShellWindow();
    pp.Windowed = true;
    pp.Flags = 0;
    pp.FullScreen_RefreshRateInHz = 0;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    pp.EnableAutoDepthStencil = false;

    hr = pD3D9->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &pDev9 );
    if( FAILED( hr ) )
    {
        V(pD3D9->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, NULL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &pDev9 ));
    }

    AH = new CMultiAnimAllocateHierarchy();
    multiAnim = new CMultiAnim();
}

HRESULT DX9AnimationLoader::LoadDX9Data(LPWSTR filename)
{
    HRESULT hr;

    V( DXUTFindDXSDKMediaFileCch( fullFilename, MAX_PATH, filename ) );

    AH->SetMA( multiAnim );

    // This is taken from the MultiAnim DX9 sample
    V( multiAnim->Setup( pDev9, fullFilename, AH ) );

    return hr;
}

// simple recursive to extract all mesh containers.
void DX9AnimationLoader::FillMeshList(D3DXFRAME *pFrame, D3DXFRAME *pFrameParent, const CHAR* pLastNameFound)
{
    CHAR *pToChildLastName = NULL;
    
    if(pFrame->Name && pFrame->Name[0])
    {
        pToChildLastName = new CHAR[MAX_PATH];
        StringCchCopyA(pToChildLastName,strlen(pFrame->Name)+1,pFrame->Name);
    }
    else if(pLastNameFound)
    {
        pToChildLastName = new CHAR[MAX_PATH];
        StringCchCopyA(pToChildLastName,strlen(pLastNameFound)+1,pLastNameFound);
    }

    // If we have a mesh container, add it to the list
    if(pFrame->pMeshContainer && pFrame->pMeshContainer->MeshData.pMesh) 
    {
        MultiAnimMC* pMCFrame = (MultiAnimMC*)pFrame->pMeshContainer;

        // Copy over parent's name if we have none of our own... push up till we find something...
        if((!pMCFrame->Name || pMCFrame->Name[0] == 0))
        {
            delete [] pMCFrame->Name;

            if (pToChildLastName)
            {
                DWORD dwLen = (DWORD) strlen( pToChildLastName ) + 1;
                pMCFrame->Name = new CHAR[ dwLen ];
                if( pMCFrame->Name )
                    StringCchCopyA( pMCFrame->Name, dwLen, pToChildLastName );
            }
            else
            {
                pMCFrame->Name = new CHAR[ strlen("<no_name_mesh>") ];
                StringCchCopyA( pMCFrame->Name, strlen("<no_name_mesh>"), "<no_name_mesh>" );
            }
        }
        
        meshList.insert(meshList.begin(), pMCFrame);
    }
    else
    {
        if(!pFrame->Name || !pFrame->Name[0])
        {
            CHAR *name = new CHAR[MAX_PATH];
#pragma warning (disable:4311)
            sprintf_s(name,MAX_PATH,"<no_name_%d>",reinterpret_cast<unsigned int>(pFrame->Name));
#pragma warning (default:4311)
            delete [] pFrame->Name;
            DWORD dwLen = (DWORD) strlen( name ) + 1;
            pFrame->Name = new CHAR[ dwLen ];
            if ( pFrame->Name )
                StringCchCopyA( pFrame->Name, dwLen, name );
            delete [] name;
        }

        frameNames.push_back(pFrame->Name);
    }

    // then process siblings and children
    if (pFrame->pFrameSibling) FillMeshList(pFrame->pFrameSibling, pFrameParent, pLastNameFound);
    
    // override last name found for our children only
    if (pFrame->pFrameFirstChild) FillMeshList(pFrame->pFrameFirstChild, pFrame, pToChildLastName);

    delete [] pToChildLastName;
}

HRESULT DX9AnimationLoader::CloneMesh(UINT meshIndex, D3DVERTEXELEMENT9* pDecl, LPD3DXMESH* ppMesh, D3DXMATRIX& staticMatrix)
{
    HRESULT hr = S_OK;

    if (meshIndex < (UINT)meshList.size())
    {
        MultiAnimMC *pMeshContainer = (MultiAnimMC *)meshList[meshIndex];
        LPD3DXMESH pDX9Mesh = pMeshContainer->MeshData.pMesh;

        V_RETURN(pDX9Mesh->CloneMesh(D3DXMESH_32BIT | D3DXMESH_DYNAMIC, pDecl, pDev9, ppMesh));

        staticMatrix = pMeshContainer->m_pStaticCombinedMatrix;
    }
    else
    {
        *ppMesh = NULL;
    }

    return hr;
}

// Walks tree and updates the transformations matrices to be combined
void DX9AnimationLoader::UpdateFrames(MultiAnimFrame* pFrame, D3DXMATRIX* pmxBase)
{
    assert( pFrame != NULL );
    assert( pmxBase != NULL );

    // transform the bone, aggregating in our parent matrix
    D3DXMatrixMultiply( &pFrame->CombinedTransformationMatrix,
        &pFrame->TransformationMatrix,
        pmxBase );

    // This should only be used when initializing the mesh pieces
    if(pFrame->pMeshContainer)
    {
        MultiAnimMC* pMCFrame = (MultiAnimMC*)pFrame->pMeshContainer;
        pMCFrame->m_pStaticCombinedMatrix = pFrame->CombinedTransformationMatrix;
    }

    // Transform siblings by the same matrix
    if( pFrame->pFrameSibling )
        UpdateFrames( (MultiAnimFrame *) pFrame->pFrameSibling, pmxBase );

    // Transform children by the transformed matrix - hierarchical transformation
    if( pFrame->pFrameFirstChild )
        UpdateFrames( (MultiAnimFrame *) pFrame->pFrameFirstChild,
        &pFrame->CombinedTransformationMatrix );
}

LPCSTR DX9AnimationLoader::FindLastBoneParentOf(MultiAnimFrame * pFrameRoot,MultiAnimMC *pMeshContainer, LPCSTR pLastBoneName)
{
    if(!pFrameRoot) return NULL;

    LPCSTR pFound = NULL;

    // find the container in our siblings, but pass in parents bone name.  if match, pass that result
    if((pFound = FindLastBoneParentOf((MultiAnimFrame *)pFrameRoot->pFrameSibling,pMeshContainer,pLastBoneName)) != NULL)
        return pFound;

    // Save our name to pass to children if we match
    if(pFrameRoot->Name && strncmp(pFrameRoot->Name,"Bip",3)==0) 
        pLastBoneName = pFrameRoot->Name;

    // We match, so just return what we think is the last bone name.
    if(pFrameRoot->pMeshContainer == pMeshContainer)
        return pLastBoneName;    // gotta return our name...

    // our child found it, pass that result.
    if((pFound = FindLastBoneParentOf((MultiAnimFrame *)pFrameRoot->pFrameFirstChild,pMeshContainer,pLastBoneName))!=NULL)
        return pFound;

    return NULL; // nothing matched in children or siblings... 
}

void DX9AnimationLoader::ExtractSkeleton()
{
    UINT bonesNum = (UINT)boneNames.size();

    MultiAnimFrame *pFrame;
    pBoneMatrixPtrs = new D3DXMATRIX*[bonesNum];
    pBoneOffsetMatrices = new D3DXMATRIX [bonesNum];

    for (UINT i=0; i<bonesNum; ++i)
    {
        D3DXMatrixIdentity(&pBoneOffsetMatrices[i]);
        pBoneMatrixPtrs[i] = NULL;
    }

    for (int iCurrentSI=0; iCurrentSI<(int)pSkinInfos.size(); ++iCurrentSI)
    {
        LPD3DXSKININFO pSkinInfo = pSkinInfos[iCurrentSI];
        DWORD dwNumBones = pSkinInfo->GetNumBones();

        for (DWORD iBone = 0; iBone < dwNumBones; iBone++)
        {
            LPCSTR boneName = pSkinInfo->GetBoneName(iBone);
            int boneIndex = -1;
            for (int iLocalBone = 0;iLocalBone<(int)boneNames.size();iLocalBone++)
            {
                if (strcmp(boneNames[iLocalBone].c_str(),boneName) == 0)
                {
                    boneIndex = iLocalBone;
                    break;
                }
            }

            if(boneIndex != -1)
            {
                pFrame = (MultiAnimFrame*)D3DXFrameFind( multiAnim->m_pFrameRoot, boneName );
                assert(pFrame);
                pBoneMatrixPtrs[boneIndex] = &pFrame->CombinedTransformationMatrix;
                pBoneOffsetMatrices[boneIndex] = *pSkinInfo->GetBoneOffsetMatrix(iBone);
            }
        }
    }

    for (UINT i=0; i<bonesNum; ++i)
    {
        if (!pBoneMatrixPtrs[i])
        {
            pBoneMatrixPtrs[i] = new D3DXMATRIX();
            D3DXMatrixIdentity(pBoneMatrixPtrs[i]);
        }
    }
}

void DX9AnimationLoader::ExtractFrame(CharacterAnimation** ppAnimation)
{
    D3DXMATRIX mIdentity;
    D3DXMatrixIdentity(&mIdentity);
    UpdateFrames(multiAnim->m_pFrameRoot, &mIdentity);
    
    int bonesNum = (*ppAnimation)->GetBonesNum();
    D3DXMATRIX *pFrame = new D3DXMATRIX[bonesNum];
    
    for (int i=0; i<bonesNum; ++i)
        D3DXMatrixMultiply(&(pFrame[i]), &(pBoneOffsetMatrices[i]), pBoneMatrixPtrs[i]);

    (*ppAnimation)->AddFrame(pFrame);
}

void DX9AnimationLoader::Cleanup()
{
    meshList.clear();
    multiAnim->Cleanup(AH);
    delete multiAnim; multiAnim = NULL;
    delete AH; AH = NULL;
    SAFE_DELETE_ARRAY(pBoneOffsetMatrices);
    SAFE_DELETE_ARRAY(pBoneMatrixPtrs);
    SAFE_RELEASE(pDev9);
    SAFE_RELEASE(pD3D9);

    playout = NULL;

    frameNames.clear();
    boneNames.clear();
    pSkinInfos.clear();
}

void DX9AnimationLoader::GetAnimationData(CharacterAnimation** ppAnimation, BoneWeights& boneWeights, float timeStep)
{
    for (int iCurrentMeshContainer=0; iCurrentMeshContainer<(int)meshList.size(); iCurrentMeshContainer++)
    {
        MultiAnimMC *pMeshContainer = (MultiAnimMC *)meshList[iCurrentMeshContainer];
        
        if (pMeshContainer->pSkinInfo)
        {
            pSkinInfos.push_back(pMeshContainer->pSkinInfo);
            DWORD dwNumBones = pMeshContainer->pSkinInfo->GetNumBones();

            // Now go through the skin and set the bone indices and weights, max 4 per vert
            for (int iCurrentBone=0; iCurrentBone<(int)dwNumBones; ++iCurrentBone)
            {
                LPCSTR boneName = pMeshContainer->pSkinInfo->GetBoneName(iCurrentBone);
                int numInfl = pMeshContainer->pSkinInfo->GetNumBoneInfluences(iCurrentBone);
                DWORD *verts = new DWORD[numInfl];
                FLOAT *weights = new FLOAT[numInfl];
                pMeshContainer->pSkinInfo->GetBoneInfluence(iCurrentBone,verts,weights);

                int boneIndex = -1;
                for (int i=0; i<(int)boneNames.size(); i++)
                {
                    if (strcmp(boneNames[i].c_str(),boneName) == 0)
                    {
                        boneIndex = i;
                        break;
                    }
                }
                if (boneIndex == -1)
                {
                    boneIndex = (int)boneNames.size();
                    boneNames.push_back(boneName);
                }

                for (int iInfluence = 0; iInfluence<numInfl; iInfluence++)
                {
                    int minimalIndex = 0;
                    float minimalWeight = boneWeights.weights[verts[iInfluence]].vector[0];

                    // Search bone with minimal weight
                    for (int i=1; i<4; ++i)
                    {
                        if (boneWeights.weights[verts[iInfluence]].vector[i] < minimalWeight)
                        {
                            minimalIndex = i;
                            minimalWeight = boneWeights.weights[verts[iInfluence]].vector[i];
                        }
                    }
                    
                    // Replace bone if needed
                    if (weights[iInfluence] > minimalWeight)
                    {
                        boneWeights.bones[verts[iInfluence]].vector[minimalIndex] = (float)boneIndex;
                        boneWeights.weights[verts[iInfluence]].vector[minimalIndex] = weights[iInfluence];
                    }
                }
                
                delete [] verts;
                delete [] weights;
            }
        }
    }
    
    if (pSkinInfos.empty() == false)
    {
        ExtractSkeleton();

        LPD3DXANIMATIONCONTROLLER pAnimationController = multiAnim->m_pAC;
    
        if(pAnimationController == NULL) return;

        // Start with all tracks disabled
        UINT dwTracks = pAnimationController->GetMaxNumTracks();
        for (int i=0; i<(int)dwTracks; ++i)
            pAnimationController->SetTrackEnable(i, FALSE);

        pAnimationController->SetTrackEnable(0, TRUE);

        int numAnimationSets = 1;//pAnimationController->GetNumAnimationSets();
        for (int iCurrentAnimationSet=0; iCurrentAnimationSet<numAnimationSets; ++iCurrentAnimationSet)
        {
            LPD3DXKEYFRAMEDANIMATIONSET pAnimationSet = NULL;
            
            pAnimationController->GetAnimationSet(iCurrentAnimationSet,(LPD3DXANIMATIONSET *)&pAnimationSet);
            pAnimationController->SetTrackAnimationSet(0,pAnimationSet);
            pAnimationSet->Release();

            pAnimationController->ResetTime();
            float duration = (float)pAnimationSet->GetPeriod();
            *ppAnimation = new CharacterAnimation(duration, (int)boneNames.size(), timeStep);

            for (double dTime=0; dTime < (double)duration; dTime+=(double)timeStep)
            {
                pAnimationController->AdvanceTime((double)timeStep, NULL);
                ExtractFrame(ppAnimation);
            }
        }
    }
}