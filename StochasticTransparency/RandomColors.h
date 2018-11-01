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

#include <DXUT.h>

inline float mix(float x, float y, float a)
{
    return x * (1.0f - a) + y * a;
}

void ComputeRandomColor(UINT subsetId, D3DXVECTOR3 &OutputColor)
{
    float h,s,v, r,g,b, h1,h3;
    h = fmodf((subsetId + 101) * 0.7182863f, 1.0f);
    s = 0.5;
    v = 1.0;

    h3 = 3.f * h;
    h1 = fmodf(h3, 1.0);
    r = g = b = 0.f;
    if (h3 < 1)
    {
        r = 1-h1;
        g = h1;
    }
    else if (h3 < 2)
    {
        g = 1-h1;
        b = h1;
    }
    else
    {
        b = 1-h1;
        r = h1;
    }
    OutputColor.x = mix(1, r, s) * v;
    OutputColor.y = mix(1, g, s) * v;
    OutputColor.z = mix(1, b, s) * v;
}
