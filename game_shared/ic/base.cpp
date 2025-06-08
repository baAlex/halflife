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
