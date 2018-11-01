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

#include "mymath.h"

template <class T>
struct BBox3
{
	inline BBox3()
	{ }
	inline BBox3(const myvect3<T> &v)
	{ vmin = v; vmax = v; }
	inline BBox3(const myvect3<T> &_vmin, const myvect3<T> &_vmax)
	{ vmin = _vmin; vmax = _vmax; }
	inline void Include(const myvect3<T> &v)
	{
		vmin = mymin(vmin, v);
		vmax = mymax(vmax, v);
	}
	inline void Include(const BBox3<T> &b)
	{
		Include(b.vmin);
		Include(b.vmax);
	}
	inline myvect3<T> EvalDiag()
	{
		return vmax - vmin;
	}
	myvect3<T> vmin, vmax;
};

typedef BBox3<float> BBox3f;
typedef BBox3<double> BBox3d;

template <class T>
struct BBox2
{
	inline BBox2()
	{ }
	inline BBox2(const myvect2<T> &v)
	{ vmin = v; vmax = v; }
	inline void Include(const myvect2<T> &v)
	{
		vmin = mymin(vmin, v);
		vmax = mymax(vmax, v);
	}
	myvect2<T> vmin, vmax;
};

typedef BBox2<float> BBox2f;
typedef BBox2<double> BBox2d;

template <class T>
struct Plane
{
	inline double Deviation(const myvect3<T> &v)
	{ return dot(v, normal) + d; }
	myvect3<T> normal;
	T d;
};

typedef Plane<float> Plane3f;
typedef Plane<double> Plane3d;

template <class T>
struct Matrix32
{
	inline Matrix32()
	{ }
	inline Matrix32(const myvect2<T> &r0, const myvect2<T> &r1, const myvect2<T> &r2)
	{
		rows[0] = r0; rows[1] = r1; rows[2] = r2;
	}
	inline const myvect2<T> &operator[](int i) const
	{ return rows[i]; }
	inline myvect2<T> &operator[](int i)
	{ return rows[i]; }
	inline void operator *=(const double s)
	{ rows[0] *= s; rows[1] *= s; rows[2] *= s; }
	myvect2<T> rows[3];
};

typedef Matrix32<float> Matrix32f;
typedef Matrix32<double> Matrix32d;

template <class T>
inline Matrix32f ConvF(const Matrix32<T> &m)
{
	return Matrix32f(ConvF(m[0]), ConvF(m[1]), ConvF(m[2]));
}

float truncate_double(double val);

float prev_float(float number);

float next_float(float number);

