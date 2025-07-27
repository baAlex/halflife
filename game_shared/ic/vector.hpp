/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#ifndef IC_VECTOR_HPP
#define IC_VECTOR_HPP

#include "base.hpp"
#include <math.h> // expf(), sqrtf(), sinf(), cosf()

namespace Ic
{

struct Vector2
{
	float x;
	float y;

	static Vector2 OneTwo(float add = 0.0f);
	static Vector2 FromPtr(const float* ptr);

	inline float& operator[](int index)
	{
		return (index == 0) ? x : y;
	}

	inline const float& operator[](int index) const
	{
		return (index == 0) ? x : y;
	}
};

struct Vector3
{
	float x;
	float y;
	float z;

	static Vector3 OneTwo(float add = 0.0f);
	static Vector3 FromPtr(const float* ptr);

	// clang-format off
	inline float& operator[](int index)
	{
		if (index == 0) return x;
		if (index == 1) return y;
		return z;
	}

	inline const float& operator[](int index) const
	{
		if (index == 0) return x;
		if (index == 1) return y;
		return z;
	}
	// clang-format on
};

struct Vector4
{
	float x;
	float y;
	float z;
	float w;

	static Vector4 OneTwo(float add = 0.0f);
	static Vector4 FromPtr(const float* ptr);

	// clang-format off
	inline float& operator[](int index)
	{
		if (index == 0) return x;
		if (index == 1) return y;
		if (index == 2) return z;
		return w;
	}

	inline const float& operator[](int index) const
	{
		if (index == 0) return x;
		if (index == 1) return y;
		if (index == 2) return z;
		return w;
	}
	// clang-format on
};

Vector2 Add(Vector2 a, Vector2 b);
Vector2 Subtract(Vector2 a, Vector2 b);
Vector2 Multiply(Vector2 a, Vector2 b);
Vector2 Divide(Vector2 a, Vector2 b);
Vector2 Scale(Vector2 v, float f);
Vector2 Normalize(Vector2 v);

Vector2 Mix(Vector2 a, Vector2 b, float f);
Vector2 HolmerMix(Vector2 a, Vector2 b, float d, float dt);

float Summation(Vector2 v);
float Length(Vector2 v);
float Dot(Vector2 a, Vector2 b);
bool Equal(Vector2 a, Vector2 b);

Vector3 Add(Vector3 a, Vector3 b);
Vector3 Subtract(Vector3 a, Vector3 b);
Vector3 Multiply(Vector3 a, Vector3 b);
Vector3 Divide(Vector3 a, Vector3 b);
Vector3 Scale(Vector3 v, float f);
Vector3 Normalize(Vector3 v);

Vector3 Mix(Vector3 a, Vector3 b, float f);
Vector3 HolmerMix(Vector3 a, Vector3 b, float d, float dt);

float Summation(Vector3 v);
float Length(Vector3 v);
float Dot(Vector3 a, Vector3 b);
bool Equal(Vector3 a, Vector3 b);

Vector2 Xy(Vector3 v);

Vector4 Add(Vector4 a, Vector4 b);
Vector4 Subtract(Vector4 a, Vector4 b);
Vector4 Multiply(Vector4 a, Vector4 b);
Vector4 Divide(Vector4 a, Vector4 b);
Vector4 Scale(Vector4 v, float f);
Vector4 Normalize(Vector4 v);

Vector4 Mix(Vector4 a, Vector4 b, float f);
Vector4 HolmerMix(Vector4 a, Vector4 b, float d, float dt);

float Summation(Vector4 v);
float Length(Vector4 v);
float Dot(Vector4 a, Vector4 b);
bool Equal(Vector4 a, Vector4 b);

Vector2 Xy(Vector4 v);
Vector3 Xyz(Vector4 v);

inline void ProperAngleVectors(Vector3 a, Vector3* forward, Vector3* right, Vector3* up)
{
	// Mathematically identical to AngleVectors() in 'pm_math.c'

#if 1
	// Roll-less version of below Wikipedia copy

	const float sp = sinf(Ic::DegToRad(a.x));
	const float cp = cosf(Ic::DegToRad(a.x));
	const float sy = sinf(Ic::DegToRad(a.y));
	const float cy = cosf(Ic::DegToRad(a.y));

	if (forward != nullptr)
	{
		forward->x = cy * cp;
		forward->y = sy * cp;
		forward->z = -sp;
	}

	if (right != nullptr)
	{
		right->x = sy;
		right->y = -cy;
		right->z = 0.0f;
	}

	if (up != nullptr)
	{
		up->x = cy * sp;
		up->y = sy * sp;
		up->z = cp;
	}
#else
	// Mostly a copy-paste from Wikipedia:
	// Rotation formalisms, Euler angles (z-y′-x″ intrinsic) → rotation matrix
	// https://en.wikipedia.org/wiki/Rotation_formalisms_in_three_dimensions#Euler_angles_(z-y%E2%80%B2-x%E2%80%B3_intrinsic)_%E2%86%92_rotation_matrix

	const float sp = sinf(Ic::DegToRad(a.x));
	const float cp = cosf(Ic::DegToRad(a.x));
	const float sy = sinf(Ic::DegToRad(a.y));
	const float cy = cosf(Ic::DegToRad(a.y));
	const float sr = sinf(Ic::DegToRad(a.z));
	const float cr = cosf(Ic::DegToRad(a.z));

	if (forward != nullptr)
	{
		forward->x = cy * cp;
		forward->y = sy * cp;
		forward->z = -sp;
	}

	if (right != nullptr)
	{
		// Different from Wikipedia:
		right->x = -(-sy * cr + cy * sp * sr);
		right->y = -(cy * cr + sy * sp * sr);
		right->z = -(cp * sr);
	}

	if (up != nullptr)
	{
		up->x = sy * sr + cy * sp * cr;
		up->y = -cy * sr + sy * sp * cr;
		up->z = cp * cr;
	}
#endif
}

inline void BrokenAngleVectors(Vector3 a, Vector3* forward, Vector3* right, Vector3* up)
{
	// This bug is inherited from Quake's vectoangles(), which returns a negated pitch
	// (https://quakewiki.org/wiki/vectoangles)

	// Meaning that buggy angles feed to AngleVectors() will not work. The bug doesn´t
	// stop there tho, is easy to live with funky coordinates, the real problem is its
	// consistency. Since is a relatively easy to fix bug, it's fixed in some parts of
	// the codebase but not in others. And oh poor me, is painful to follow.

	a.x = -a.x; // Fix. A negated value is all what it takes, and code here and
	            // there was written with or without this prevision.
	return ProperAngleVectors(a, forward, right, up);
}

} // namespace Ic

#endif
