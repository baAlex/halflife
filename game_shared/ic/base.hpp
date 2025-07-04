/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#ifndef IC_BASE_HPP
#define IC_BASE_HPP

#include <stdint.h>

namespace Ic
{

#if defined(__GNUC__) || defined(__clang__)
// Is more about the intent that the actual effect
#define IC_LIKELY(expr) (__builtin_expect(!!(expr), 1))
#define IC_UNLIKELY(expr) (__builtin_expect(!!(expr), 0))
#else
#define IC_LIKELY(x) (x)
#define IC_UNLIKELY(x) (x)
#endif

// clang-format off
template <typename T> T Min(T a, T b) { return (a < b) ? a : b; }
template <typename T> T Max(T a, T b) { return (a > b) ? a : b; }
template <typename T> T Clamp(T x, T min, T max) { return Min(Max(x, min), max); }
template <typename T> T Abs(T v) { return Max(v, -v); }
// clang-format on

float DegToRad(float deg);
float RadToDeg(float rad);

float FmodFloored(float x, float y);
float AnglesDifference(float a, float b);

float Mix(float a, float b, float f);
float AnglesMix(float a, float b, float f);
float HolmerMix(float a, float b, float d, float dt);
float AnglesHolmerMix(float a, float b, float d, float dt);

float ClampAroundCentre(float x, float centre, float range);

uint16_t Xorshift16(uint16_t x);      // Don't pass a zero, also, function will never return zero
uint16_t Xorshift16(uint16_t* state); // Don't pass a zero, also, function will never return zero
float RandomFloat(uint16_t* state);   // Don't pass a zero

} // namespace Ic

#endif
