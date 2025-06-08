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

namespace Ic
{

struct Vector2
{
	float x;
	float y;

	static Vector2 OneTwo(float add = 0.0f);

	inline float& operator[](int index)
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

	// clang-format off
	inline float& operator[](int index)
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

	// clang-format off
	inline float& operator[](int index)
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

} // namespace Ic

#endif
