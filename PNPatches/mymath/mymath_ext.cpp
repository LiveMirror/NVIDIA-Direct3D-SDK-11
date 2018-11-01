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

#include "mymath_ext.h"

const float Math<float>::TOL_MULT = 1./0x2000;
const double Math<double>::TOL_MULT = 1./0x1000000;
const float Math<float>::MIN_VALUE = 1e-35f;
const double Math<double>::MIN_VALUE = 1e-300;
const float Math<float>::MAX_VALUE = 1e+35f;
const double Math<double>::MAX_VALUE = 1e+300;

float next_float(float number)
{
  if (number != 0)
  {
    if (number > 0) ++(int &)number;
    else
    {
      if ((int &)number != (0x00800000 | 0x80000000))
        --(int &)number;
      else number = 0;
    }
  }
  else (int &)number = 0x00800000;
  return number;
}

float prev_float(float number)
{
  if (number != 0)
  {
    if (number < 0) ++(int &)number;
    else
    {
      if ((int &)number != 0x00800000)
        --(int &)number;
      else number = 0;
    }
  }
  else (int &)number = (0x00800000 | 0x80000000);
  return number;
}

float truncate_double(double val)
{
  if (val != 0)
  {
    unsigned *_val = (unsigned *)&val;
    _val[1] -= 0x38000000;
    unsigned result = (_val[1] & 0x80000000) | ((_val[1] << 3) & 0x7ffffff8) | (_val[0] >> 29);
    return (float &)result;
  }
  return 0;
}
