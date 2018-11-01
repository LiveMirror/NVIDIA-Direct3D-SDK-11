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

#ifndef CUSTOM_CAMERA_H
#define CUSTOM_CAMERA_H

#include "DXUT.h"

#include "DXUTCamera.h"

class CCustomCamera : public CModelViewerCamera
{
public:

    CCustomCamera();

    const D3DXMATRIX* GetViewMatrix() const
    {
        return &m_mWorldToView;
    }

    void FrameMove(double fTime, float fElapsedTime, const D3DXVECTOR3& LookAt);

    virtual LRESULT HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

    HRESULT SaveState(double fTime);

    void ToggleDoAdjust() { m_bDoAdjust = !m_bDoAdjust; }
    BOOL GetDoAdjust() const { return m_bDoAdjust; }

	void ToggleCameraOrbit();
	void DisableCameraOrbit() { m_OrbitRate = 0.f; }

	// NB: This will hide the function with the same name in the base class, which is intentional
	// (so that we can preserve our custom min-radius setting)
	void SetViewParams( D3DXVECTOR3* pvEyePt, D3DXVECTOR3* pvLookatPt );

private:

    HRESULT LoadState(double fTime);
    D3DXMATRIX GetOrbitMatrix();

    BOOL m_bMoveIn;
    BOOL m_bMoveOut;

    // Automatic adjust state
    BOOL m_bDoAdjust;
    BOOL m_bIsMoving;
    D3DXVECTOR3 m_CurrOffset;
    D3DXVECTOR3 m_TargetOffset;
    FLOAT m_Speed;
    FLOAT m_MaxError;
    BOOL m_FirstUpdate;
    double m_LastUpdateTime;

    FLOAT m_OrbitRate;
    FLOAT m_OrbitAngle;

    D3DXMATRIX m_mWorldToView;
};

#endif // CUSTOM_CAMERA_H
