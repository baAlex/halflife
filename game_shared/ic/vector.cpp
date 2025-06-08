/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#include <math.h> // expf(), sqrtf()

#include "base.hpp"
#include "vector.hpp"


using namespace Ic;


template <typename T, int L> static T sOneTwo(float add)
{
	T ret;
	add += 1.0f;
	for (int i = 0; i < L; i += 1)
		ret[i] = static_cast<float>(i) + add;
	return ret;
}

template <typename T, int L> static T sAdd(T a, T b)
{
	for (int i = 0; i < L; i += 1)
		a[i] = a[i] + b[i];
	return a;
}

template <typename T, int L> static T sSubtract(T a, T b)
{
	for (int i = 0; i < L; i += 1)
		a[i] = a[i] - b[i];
	return a;
}

template <typename T, int L> static T sMultiply(T a, T b)
{
	for (int i = 0; i < L; i += 1)
		a[i] = a[i] * b[i];
	return a;
}

template <typename T, int L> static T sDivide(T a, T b)
{
	for (int i = 0; i < L; i += 1)
		a[i] = a[i] / b[i];
	return a;
}

template <typename T, int L> static T sMultiplyFactor(T v, float f)
{
	for (int i = 0; i < L; i += 1)
		v[i] = v[i] * f;
	return v;
}

template <typename T, int L> static T sDivideFactor(T v, float f)
{
	for (int i = 0; i < L; i += 1)
		v[i] = v[i] / f;
	return v;
}

template <typename T, int L> static float sSummation(T v)
{
	float a = 0.0f;
	for (int i = 0; i < L; i += 1)
		a = a + v[i];
	return a;
}

template <typename T> static T sMix(T a, T b, float f)
{
	return Add(a, Scale(Subtract(b, a), f));
}

template <typename T> static T sHolmerMix(T a, T b, float d, float dt)
{
	return Mix(a, b, static_cast<float>(expf(-d * dt)));
}

template <typename T> static float sLength(T v)
{
	return static_cast<float>(sqrtf(Summation(Multiply(v, v))));
}

template <typename T> static float sDot(T a, T b)
{
	return Summation(Multiply(a, b));
}

template <typename T, int L> static T sNormalize(T v)
{
	const float length = Length(v);
	// assert(length != 0.0f && length != -0.0f);
	return sDivideFactor<T, L>(v, length);
}

template <typename T, int L> static bool sEqual(T a, T b)
{
	// https://embeddeduse.com/2019/08/26/qt-compare-two-floats/

#if 0
	for (int i = 0; i < L; i += 1)
	{
		if (Abs(a[i] - b[i]) > 1.0e-5f * Max(Abs(a[i]), Abs(b[i])))
			return false;
	}
#else
	// Could be better in Vec2, but is fantastic in Vec4
	T ep;

	for (int i = 0; i < L; i += 1)
	{
		ep[i] = 1.0e-5f * Max(Abs(a[i]), Abs(b[i]));
		a[i] = Abs(a[i] - b[i]);
	}

	for (int i = 0; i < L; i += 1)
	{
		if (a[i] > ep[i])
			return false;
	}
#endif

	return true;
}


// clang-format off
Vector2 Ic::Vector2::OneTwo(float add) { return sOneTwo<Vector2, 2>(add); }

Vector2 Ic::Add(Vector2 a, Vector2 b)      { return sAdd<Vector2, 2>(a, b); }
Vector2 Ic::Subtract(Vector2 a, Vector2 b) { return sSubtract<Vector2, 2>(a, b); }
Vector2 Ic::Multiply(Vector2 a, Vector2 b) { return sMultiply<Vector2, 2>(a, b); }
Vector2 Ic::Divide(Vector2 a, Vector2 b)   { return sDivide<Vector2, 2>(a, b); }
Vector2 Ic::Scale(Vector2 v, float f)      { return sMultiplyFactor<Vector2, 2>(v, f); }
float   Ic::Summation(Vector2 v)           { return sSummation<Vector2, 2>(v); }

Vector2 Ic::Mix(Vector2 a, Vector2 b, float f)                 { return sMix<Vector2>(a, b, f); }
Vector2 Ic::HolmerMix(Vector2 a, Vector2 b, float d, float dt) { return sHolmerMix<Vector2>(a, b, d, dt); }

float   Ic::Length(Vector2 v)          { return sLength<Vector2>(v); }
float   Ic::Dot(Vector2 a, Vector2 b)  { return sDot<Vector2>(a, b); }
Vector2 Ic::Normalize(Vector2 v)     { return sNormalize<Vector2, 2>(v); }
bool    Ic::Equal(Vector2 a, Vector2 b) { return sEqual<Vector2, 2>(a, b); }

Vector3 Ic::Vector3::OneTwo(float add) { return sOneTwo<Vector3, 3>(add); }

Vector3 Ic::Add(Vector3 a, Vector3 b)      { return sAdd<Vector3, 3>(a, b); }
Vector3 Ic::Subtract(Vector3 a, Vector3 b) { return sSubtract<Vector3, 3>(a, b); }
Vector3 Ic::Multiply(Vector3 a, Vector3 b) { return sMultiply<Vector3, 3>(a, b); }
Vector3 Ic::Divide(Vector3 a, Vector3 b)   { return sDivide<Vector3, 3>(a, b); }
Vector3 Ic::Scale(Vector3 v, float f)      { return sMultiplyFactor<Vector3, 3>(v, f); }
float   Ic::Summation(Vector3 v)           { return sSummation<Vector3, 3>(v); }

Vector3 Ic::Mix(Vector3 a, Vector3 b, float f)                 { return sMix<Vector3>(a, b, f); }
Vector3 Ic::HolmerMix(Vector3 a, Vector3 b, float d, float dt) { return sHolmerMix<Vector3>(a, b, d, dt); }

float   Ic::Length(Vector3 v)          { return sLength<Vector3>(v); }
float   Ic::Dot(Vector3 a, Vector3 b)  { return sDot<Vector3>(a, b); }
Vector3 Ic::Normalize(Vector3 v)     { return sNormalize<Vector3, 3>(v); }
bool    Ic::Equal(Vector3 a, Vector3 b) { return sEqual<Vector3, 3>(a, b); }

Vector4 Ic::Vector4::OneTwo(float add) { return sOneTwo<Vector4, 4>(add); }

Vector4 Ic::Add(Vector4 a, Vector4 b)      { return sAdd<Vector4, 4>(a, b); }
Vector4 Ic::Subtract(Vector4 a, Vector4 b) { return sSubtract<Vector4, 4>(a, b); }
Vector4 Ic::Multiply(Vector4 a, Vector4 b) { return sMultiply<Vector4, 4>(a, b); }
Vector4 Ic::Divide(Vector4 a, Vector4 b)   { return sDivide<Vector4, 4>(a, b); }
Vector4 Ic::Scale(Vector4 v, float f)      { return sMultiplyFactor<Vector4, 4>(v, f); }
float   Ic::Summation(Vector4 v)           { return sSummation<Vector4, 4>(v); }

Vector4 Ic::Mix(Vector4 a, Vector4 b, float f)                 { return sMix<Vector4>(a, b, f); }
Vector4 Ic::HolmerMix(Vector4 a, Vector4 b, float d, float dt) { return sHolmerMix<Vector4>(a, b, d, dt); }

float   Ic::Length(Vector4 v)          { return sLength<Vector4>(v); }
float   Ic::Dot(Vector4 a, Vector4 b)  { return sDot<Vector4>(a, b); }
Vector4 Ic::Normalize(Vector4 v)     { return sNormalize<Vector4, 4>(v); }
bool    Ic::Equal(Vector4 a, Vector4 b) { return sEqual<Vector4, 4>(a, b); }
// clang-format on


Vector2 Ic::Xy(Vector3 v)
{
	return {v.x, v.y};
}

Vector2 Ic::Xy(Vector4 v)
{
	return {v.x, v.y};
}

Vector3 Ic::Xyz(Vector4 v)
{
	return {v.x, v.y, v.z};
}
