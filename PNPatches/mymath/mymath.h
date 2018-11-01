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

#define _USE_MATH_DEFINES
#include <math.h>

template <class T>
struct myvect2
{
    T &operator[](int i)
    { return vector[i]; }
    const T &operator[](int i) const
    { return vector[i]; }
    inline myvect2()
    { }
    inline myvect2(T _x, T _y)
    { vector[0] = _x; vector[1] = _y; }
    inline bool operator == (const myvect2<T> &v) const
    { return vector[0] == v[0] && vector[1] == v[1]; }
    inline bool operator != (const myvect2<T> &v) const
    { return vector[0] != v[0] && vector[1] != v[1]; }
    inline myvect2 operator /(const double s) const
    { return myvect2((T)(vector[0] / s), (T)(vector[1] / s)); }
    inline myvect2 operator *(const double s) const
    { return myvect2((T)(vector[0] * s), (T)(vector[1] * s)); }
    inline myvect2 operator -(const myvect2 &v) const
    { return myvect2((T)(vector[0] - v[0]), (T)(vector[1] - v[1])); }
    inline myvect2 operator +(const myvect2 &v) const
    { return myvect2((T)(vector[0] + v[0]), (T)(vector[1] + v[1])); }
    inline myvect2 operator *(const myvect2 &v) const
    { return myvect2((T)(vector[0] * v[0]), (T)(vector[1] * v[1])); }
    inline myvect2<T> operator ++()
    { ++vector[0]; ++vector[1]; return *this; }
    inline myvect2<T> &operator +=(const double s)
    { 
        vector[0] += (T)s; vector[1] += (T)s;
        return *this;
    }
    inline myvect2<T> &operator *=(const double s)
    { 
        vector[0] *= (T)s; vector[1] *= (T)s;
        return *this;
    }
    inline myvect2<T> &operator /=(const double s)
    { 
        vector[0] /= (T)s; vector[1] /= (T)s;
        return *this;
    }
    inline myvect2<T> &operator +=(const myvect2<T> &v)
    { 
        vector[0] += (T)v[0]; vector[1] += (T)v[1];
        return *this;
    }
    inline myvect2<T> &operator -=(const myvect2<T> &v)
    { 
        vector[0] -= (T)v[0]; vector[1] -= (T)v[1];
        return *this;
    }
    inline myvect2<T> &operator *=(const myvect2<T> &v)
    { 
        vector[0] *= (T)v[0]; vector[1] *= (T)v[1];
        return *this;
    }
    inline bool AboutEqual(const myvect2<T> &v, double fEps = 0.001)
    {
        return abs(v[0] - vector[0]) < fEps && abs(v[1] - vector[1]) < fEps;
    }
    T vector[2];
};

template <class T>
struct myvect3
{
    T &operator[](int i)
    { return vector[i]; }
    const T &operator[](int i) const
    { return vector[i]; }
    inline myvect3()
    { }
    inline myvect3(T _x, T _y, T _z)
    { vector[0] = _x; vector[1] = _y; vector[2] = _z; }
    inline myvect3 operator *(const double s) const
    { return myvect3((T)(vector[0] * s), (T)(vector[1] * s), (T)(vector[2] * s)); }
    inline myvect3 operator /(const double s) const
    { return myvect3((T)(vector[0] / s), (T)(vector[1] / s), (T)(vector[2] / s)); }
    inline myvect3 operator +(const myvect3 &v) const
    { return myvect3((T)(vector[0] + v[0]), (T)(vector[1] + v[1]), (T)(vector[2] + v[2])); }
    inline myvect3 operator -(const myvect3 &v) const
    { return myvect3((T)(vector[0] - v[0]), (T)(vector[1] - v[1]), (T)(vector[2] - v[2])); }
    inline myvect3<T> operator +=(const myvect3<double> &v)
    { 
        vector[0] += (T)v[0]; vector[1] += (T)v[1]; vector[2] += (T)v[2];
		return *this;
    }
    inline myvect3<T> operator +=(const myvect3<float> &v)
    { 
        vector[0] += (T)v[0]; vector[1] += (T)v[1]; vector[2] += (T)v[2];
		return *this;
    }
    inline myvect3<T> operator +=(const myvect3<int> &v)
    { 
        vector[0] += (T)v[0]; vector[1] += (T)v[1]; vector[2] += (T)v[2];
        return *this;
    }
    inline void operator /=(const double s)
    {
        vector[0] /= (T)s; vector[1] /= (T)s; vector[2] /= (T)s;
        return;
    }
    inline void operator *=(const double s)
    {
        vector[0] *= (T)s; vector[1] *= (T)s; vector[2] *= (T)s;
        return;
    }
    inline myvect3<T> operator -() const
    {
        return myvect3<T>(-vector[0], -vector[1], -vector[2]);
    }
	inline double Sum()
	{ return vector[0] + vector[1] + vector[2]; }
	inline bool operator !=(const myvect3<T> &v) const
	{ return vector[0] != v.vector[0] || vector[1] != v.vector[1] || vector[2] != v.vector[2]; }
	inline bool operator ==(const myvect3<T> &v) const
	{ return vector[0] == v.vector[0] && vector[1] == v.vector[1] && vector[2] == v.vector[2]; }
	inline myvect2<T> &xy()
	{
		return (myvect2<T> &)vector[0];
	}
	inline const myvect2<T> &xy() const
	{
		return (const myvect2<T> &)vector[0];
	}
    T vector[3];
};

template <class T>
struct myvect4
{
    T &operator[](int i)
    { return vector[i]; }
    const T &operator[](int i) const
    { return vector[i]; }
    inline myvect4()
    { }
    inline myvect4(T _x, T _y, T _z, T _w)
    { vector[0] = _x; vector[1] = _y; vector[2] = _z; vector[3] = _w; }
    inline bool operator == (const myvect4<T> &v) const
    {
        return vector[0] == v[0] && vector[1] == v[1] && vector[2] == v[2] && vector[3] == v[3];
    }
	inline myvect4<T> operator +(const myvect4<T> &v) const
	{
		return myvect4<T>(vector[0] + v.vector[0], vector[1] + v.vector[1], vector[2] + v.vector[2], vector[3] + v.vector[3]);
	}
	inline myvect4<T> operator *(const double s) const
	{
		return myvect4<T>((T)(vector[0] * s), (T)(vector[1] * s), (T)(vector[2] * s), (T)(vector[3] * s));
	}
	inline bool AllZero()
	{ return vector[0] == 0 && vector[1] == 0 && vector[2] == 0 && vector[3] == 0; }
	inline const myvect3<T> &xyz() const
	{
		return (const myvect3<T> &)vector[0];
	}
	inline myvect3<T> &xyz()
	{
		return (myvect3<T> &)vector[0];
	}
    T vector[4];
};

typedef myvect2<int> int2;
typedef myvect2<unsigned> uint2;
typedef myvect2<float> float2;
typedef myvect2<double> double2;
typedef myvect2<double> Vect2d;

typedef myvect3<int> int3;
typedef myvect3<float> float3;
typedef myvect3<double> double3;
typedef myvect3<double> Vect3d;
typedef myvect3<float> Vect3f;

typedef myvect4<int> int4;
typedef myvect4<unsigned> uint4;
typedef myvect4<float> float4;
typedef myvect4<unsigned short> word4;
typedef myvect4<double> double4;
typedef myvect4<unsigned char> byte4;

template <class T>
inline double dot(const myvect3<T> &v1, const myvect3<T> &v2)
{
    return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

template <class T>
inline double dot(const myvect2<T> &v1, const myvect2<T> &v2)
{
    return v1[0] * v2[0] + v1[1] * v2[1];
}

template <class T>
inline myvect2<T> normalize(const myvect2<T> &_v)
{
    myvect2<T> v = _v;
    double lensqr = dot(v, v);
    if (lensqr > 0)
    {
        v /= sqrt(lensqr);
    }
    return v;
}

template <class T>
inline myvect3<T> normalize(const myvect3<T> &_v)
{
    myvect3<T> v = _v;
    double lensqr = dot(v, v);
    if (lensqr > 0)
    {
        v /= sqrt(lensqr);
    }
    return v;
}

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

template <class T>
inline T mymin(const T s1, const T s2)
{
	return s1 < s2 ? s1 : s2;
}

template <class T>
inline T mymax(const T s1, const T s2)
{
	return s1 > s2 ? s1 : s2;
}

template <class T>
inline myvect2<T> mymin(const myvect2<T> &v1, const myvect2<T> &v2)
{
    return myvect2<T>(min(v1[0], v2[0]), min(v1[1], v2[1]));
}

template <class T>
inline myvect3<T> mymin(const myvect3<T> &v1, const myvect3<T> &v2)
{
    return myvect3<T>(min(v1[0], v2[0]), min(v1[1], v2[1]), min(v1[2], v2[2]));
}

template <class T>
inline myvect2<T> mymax(const myvect2<T> &v1, const myvect2<T> &v2)
{
    return myvect2<T>(max(v1[0], v2[0]), max(v1[1], v2[1]));
}

template <class T>
inline myvect3<T> mymax(const myvect3<T> &v1, const myvect3<T> &v2)
{
    return myvect3<T>(max(v1[0], v2[0]), max(v1[1], v2[1]), max(v1[2], v2[2]));
}

template <class T>
inline myvect2<double> ConvD(const myvect2<T> &v)
{
    return double2((double)v[0], (double)v[1]);
}

template <class T>
inline myvect3<double> ConvD(const myvect3<T> &v)
{
    return double3((double)v[0], (double)v[1], (double)v[2]);
}

template <class T>
inline myvect2<float> ConvF(const myvect2<T> &v)
{
    return float2((float)v[0], (float)v[1]);
}

template <class T>
inline myvect3<float> ConvF(const myvect3<T> &v)
{
    return float3((float)v[0], (float)v[1], (float)v[2]);
}

template <class T>
inline double cross(const myvect2<T> &v1, const myvect2<T> &v2)
{ return v1[0] * v2[1] - v1[1] * v2[0]; }

template <class T>
inline myvect3<T> cross(const myvect3<T> &a, const myvect3<T> &b)
{ return myvect3<T>(a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]); }

template<class T> class Math
{
public:
  /// multiplier to get value-dependent tolerance
  static const T TOL_MULT;
  /// maximum values for float and double types (rounded down)
  static const T MIN_VALUE;
  /// minimal positive values for float and double types (rounded up)
  static const T MAX_VALUE;
};

typedef Math<float> MathF;
typedef Math<double> MathD;

typedef void* VOID_PTR;

template <class T>
inline void Swap(T &a, T &b)
{
	T tmp = a;
	a = b;
	b = tmp;
}

#ifdef _M_X64
typedef unsigned __int64 PTR_VAL;
#else
typedef unsigned PTR_VAL;
#endif

extern void SetProgress(float fProgress);
extern void SetProgressString(char *sProgressString);

template <class T>
struct mymatr3x3
{
	inline mymatr3x3()
	{ }
	inline mymatr3x3(const myvect3<T> &x, const myvect3<T> &y, const myvect3<T> &z)
	{
		rot[0] = x;
		rot[1] = y;
		rot[2] = z;
	}
	inline myvect3<T> Project(const myvect3<T> &v) const
	{
		return myvect3<T>(dot(rot[0], v), dot(rot[1], v), dot(rot[2], v));
	}
	inline mymatr3x3<T> Transpond() const
	{
		mymatr3x3<T> r;
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				r.rot[i][j] = rot[j][i];
			}
		}
		return r;
	}
	myvect3<T> rot[3];
};

typedef mymatr3x3<double> matr3x3d;
