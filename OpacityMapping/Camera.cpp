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

#include "Camera.h"

#include "SDKmisc.h"

#define _USE_MATH_DEFINES
#include <math.h>

namespace
{
    const LPCWSTR CameraStateFileName = L"CameraState.bin";
}

CCustomCamera::CCustomCamera() :
    m_bMoveIn(FALSE),
    m_bMoveOut(FALSE),
    m_bDoAdjust(TRUE),
    m_bIsMoving(FALSE),
    m_CurrOffset(D3DXVECTOR3(0.f,0.f,0.f)),
    m_TargetOffset(D3DXVECTOR3(0.f,0.f,0.f)),
    m_Speed(0.2f),
    m_MaxError(0.2f),
    m_FirstUpdate(TRUE),
    m_LastUpdateTime(0.0),
    m_OrbitRate(FLOAT(M_PI)/6.f),
    m_OrbitAngle(0.f)
{
	// Allow a tighter min radius, so we can get nice and close to details
	m_fMinRadius *= 0.25f;
}

HRESULT CCustomCamera::LoadState(double fTime)
{
	HRESULT hr;

	WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, CameraStateFileName) );

    FILE* pFile = NULL;
    if(_wfopen_s(&pFile, str, L"rb"))
        return E_FAIL;

    // Read in the data
    BOOL bIsMoving;
    D3DXVECTOR3 TargetOffset;
    D3DXVECTOR3 CurrOffset;

    if(fread(&bIsMoving, sizeof(bIsMoving), 1, pFile) < 1)
        return E_FAIL;
    if(fread(&TargetOffset, sizeof(TargetOffset), 1, pFile) < 1)
        return E_FAIL;
    if(fread(&CurrOffset, sizeof(CurrOffset), 1, pFile) < 1)
        return E_FAIL;

    fclose(pFile);

    // All is well, instate the new data
    m_bIsMoving = bIsMoving;
    m_CurrOffset = TargetOffset;
    m_TargetOffset = CurrOffset;

    return S_OK;
}

HRESULT CCustomCamera::SaveState(double fTime)
{
    FILE* pFile = NULL;
    if(_wfopen_s(&pFile, CameraStateFileName, L"wb+"))
        return E_FAIL;

    // Write out the data
    fwrite(&m_bIsMoving, sizeof(m_bIsMoving), 1, pFile);
    fwrite(&m_CurrOffset, sizeof(m_CurrOffset), 1, pFile);
    fwrite(&m_TargetOffset, sizeof(m_TargetOffset), 1, pFile);

    fclose(pFile);

    return S_OK;
}

void CCustomCamera::FrameMove(double fTime, float fElapsedTime, const D3DXVECTOR3& LookAt)
{
    // We do our own camera radius updates
    FLOAT radiusSpeed = 1.f;
    if(m_bMoveIn)
        radiusSpeed -= 0.5f * fElapsedTime;
    if(m_bMoveOut)
        radiusSpeed += 0.5f * fElapsedTime;
    m_fRadius = m_fRadius * radiusSpeed;

    // Now do the base-class update
    CModelViewerCamera::FrameMove( fElapsedTime );

    if(m_FirstUpdate)
    {
        // Try loading the state
        LoadState(fTime);

        m_LastUpdateTime = fTime;
        m_FirstUpdate = FALSE;
        return;
    }

    if(!m_bIsMoving)
    {
        // Maybe we should be moving?
        const D3DXVECTOR3 errorVector = m_CurrOffset - LookAt;
        if(D3DXVec3Length(&errorVector) > m_MaxError)
        {
            // Move!
            m_bIsMoving = TRUE;
            m_TargetOffset = LookAt;
        }
    }

    if(m_bIsMoving)
    {
        const FLOAT maxDistanceToMove = m_Speed * FLOAT(fTime - m_LastUpdateTime);
        const D3DXVECTOR3 shortfallVector = m_TargetOffset - m_CurrOffset;
        const FLOAT shortfallLength = D3DXVec3Length(&shortfallVector);
        if(maxDistanceToMove > shortfallLength)
        {
            // We're done moving
            m_bIsMoving = FALSE;
            m_CurrOffset = m_TargetOffset;
        }
        else
        {
            // Move a little more
            m_CurrOffset += (maxDistanceToMove/shortfallLength) * shortfallVector;
        }
    }

    // Orbiting
    m_OrbitAngle += m_OrbitRate * fElapsedTime;

    // Finally, update the world-view matrix
    D3DXMATRIX mAdjust;
    D3DXMatrixIdentity(&mAdjust);
    if(m_bDoAdjust)
    {
        mAdjust._41 -= m_CurrOffset.x;
        mAdjust._42 -= m_CurrOffset.y;
        mAdjust._43 -= m_CurrOffset.z;
    }

    m_mWorldToView = GetOrbitMatrix() * mAdjust * *CModelViewerCamera::GetViewMatrix();

    m_LastUpdateTime = fTime;
}

D3DXMATRIX CCustomCamera::GetOrbitMatrix()
{
    D3DXMATRIX result;
    D3DXMatrixRotationY(&result, m_OrbitAngle);
    return result;
}

LRESULT CCustomCamera::HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    switch( uMsg )
    {
        case WM_KEYDOWN:
        {
            if(VK_UP == wParam)
                m_bMoveIn = TRUE;
            else if(VK_DOWN == wParam)
                m_bMoveOut = TRUE;
            break;
        }

        case WM_KEYUP:
        {
            if(VK_UP == wParam)
                m_bMoveIn = FALSE;
            else if(VK_DOWN == wParam)
                m_bMoveOut = FALSE;
            break;
        }
    }

    return CModelViewerCamera::HandleMessages(hWnd, uMsg, wParam, lParam);
}

void CCustomCamera::SetViewParams( D3DXVECTOR3* pvEyePt, D3DXVECTOR3* pvLookatPt )
{
	const FLOAT fMinRadius = m_fMinRadius;
	CModelViewerCamera::SetViewParams(pvEyePt, pvLookatPt);
	m_fMinRadius = fMinRadius;
}

void CCustomCamera::ToggleCameraOrbit()
{
	if(m_OrbitRate > 0.f) m_OrbitRate = 0.f;
	else m_OrbitRate = FLOAT(M_PI)/6.f;
}
