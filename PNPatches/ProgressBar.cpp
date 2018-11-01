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
#include "ProgressBar.h"

static float g_fProgress = -1;
static HWND g_hProgressWnd = NULL;
const int g_BufferSize = 1024;
static char g_sProgressString[g_BufferSize];
static HANDLE g_hProgressThread = 0;
static LARGE_INTEGER startCounter, endCounter, frequency;

LRESULT CALLBACK ProgressBarProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;

	switch(msg)
	{
	    case WM_CLOSE:
		    DestroyWindow(hWnd);
		    break;
	    case WM_DESTROY:
		    PostQuitMessage(0);
		    break;
	    case WM_PAINT:
		    {
                BeginPaint(hWnd, &ps);
			    char buffer[g_BufferSize];
                QueryPerformanceCounter(&endCounter);
                double time = (double)(endCounter.QuadPart - startCounter.QuadPart) / (double)frequency.QuadPart;
			    int ilen;
                if (g_fProgress)
                    ilen = sprintf_s(buffer, g_BufferSize, "%s  %.1fs (%.2f %%)", g_sProgressString, time, g_fProgress * 100);
                else
                    ilen = sprintf_s(buffer, g_BufferSize, "%s  %.1fs", g_sProgressString, time);
                TextOutA(ps.hdc, 0, 0, buffer, ilen); 
                EndPaint(hWnd, &ps);
		    }
		    break;
	    default:
		    return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

DWORD WINAPI ProgressBarThread(void *lpParameter)
{
	WNDCLASSEX wc;

	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = 0;
	wc.lpfnWndProc   = ProgressBarProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = DXUTGetHINSTANCE();
	wc.hIcon         = NULL;
	wc.hCursor       = NULL;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = L"myWindowClass";
	wc.hIconSm       = NULL;

	if(!RegisterClassEx(&wc))
	{
		assert(false);
		return 0;
	}

	HWND hDesktop = GetDesktopWindow();
	RECT desktopRect;
	GetClientRect(hDesktop, &desktopRect);

	g_hProgressWnd = CreateWindowEx(
		WS_EX_CLIENTEDGE | WS_EX_TOPMOST,
		L"myWindowClass",
		L"PN-Patches progress",
		0,
		desktopRect.right / 2 - 400, desktopRect.bottom / 2 - 30, 800, 60,
		hDesktop, NULL, DXUTGetHINSTANCE(), NULL);
	
    assert(g_hProgressWnd != NULL);

    QueryPerformanceFrequency(&frequency);

	// wait when for we actually need progress bar
	SuspendThread(g_hProgressThread);

	for ( ; ; )
	{
		// process messages
		bool bGotMsg;
		MSG msg;
        bGotMsg = ( PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE ) != 0 );
        if( bGotMsg )
        {
			TranslateMessage( &msg );
			DispatchMessage( &msg );
        }
		// hide window and suspend thread if needed
		if (g_fProgress < 0 || g_fProgress > 1)
		{
			// hide progress bar
			ShowWindow(g_hProgressWnd, SW_HIDE);
			SuspendThread(g_hProgressThread);
			continue;
		}
		// show progress bar
		ShowWindow(g_hProgressWnd, SW_SHOW);
		InvalidateRect(g_hProgressWnd, NULL, true);
		UpdateWindow(g_hProgressWnd);
		Sleep(100); // progress bar is updated 10 times per second
	}
}

void InitializeProgressBar()
{
	DWORD thread_id;
	g_hProgressThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ProgressBarThread, 0, 0, &thread_id);
	assert(g_hProgressThread != 0);
}

void DestroyProgressBar()
{
}

void SetProgress(float fProgress)
{
	g_fProgress = fProgress;
	if (g_fProgress >= 0 && g_fProgress <= 1)
	{
		ResumeThread(g_hProgressThread);
	}
}

void SetProgressString(char *sProgressString)
{
	strcpy_s(g_sProgressString, g_BufferSize, sProgressString);
    QueryPerformanceCounter(&startCounter);
}