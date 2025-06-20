/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#include <math.h>

#include "base.hpp"


float Ic::DegToRad(float deg)
{
	return deg * (static_cast<float>(M_PI) / 180.0f);
}

float Ic::RadToDeg(float rad)
{
	return rad * (180.0f / static_cast<float>(M_PI));
}


float Ic::FmodFloored(float x, float y)
{
	return x - static_cast<float>(floorf(x / y)) * y;
}

float Ic::AnglesDifference(float a, float b)
{
	return FmodFloored(b - a + 180.0f, 360.0f) - 180.0f;
}


float Ic::Mix(float a, float b, float f)
{
	return a + (b - a) * f;
}

float Ic::AnglesMix(float a, float b, float f)
{
	return a + AnglesDifference(a, b) * f;
}

float Ic::HolmerMix(float a, float b, float d, float dt)
{
	// Freya Holmér. Lerp smoothing is broken. 2024.
	// https://www.youtube.com/watch?v=LSNQuFEDOyQ
	return Mix(a, b, static_cast<float>(expf(-d * dt)));
}

float Ic::AnglesHolmerMix(float a, float b, float d, float dt)
{
	return AnglesMix(a, b, static_cast<float>(expf(-d * dt)));
}


float Ic::ClampAroundCentre(float x, float centre, float range)
{
	return centre + Max(-range, Min(x - centre, range));
}


uint16_t Ic::Xorshift16(uint16_t x)
{
	// http://www.retroprogramming.com/2017/07/xorshift-pseudorandom-numbers-in-z80.html
	// assert(x != 0);
	x ^= static_cast<uint16_t>(x << 7);
	x ^= static_cast<uint16_t>(x >> 9);
	x ^= static_cast<uint16_t>(x << 8);
	return x;
}

uint16_t Ic::Xorshift16(uint16_t* state)
{    uint16_t x = *state;
    x ^= x << 7;
    x ^= x >> 9;
    x ^= x << 8;
    *state = x;
    return x;
}

static constexpr float SCALE = 1.0f / 65536.0f;

float Ic::RandomFloat(uint16_t* state)
{
	const uint16_t x = Xorshift16(state);
	return (static_cast<float>(x) * SCALE);
}
