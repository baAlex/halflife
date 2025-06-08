/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#include <math.h>
#include <string.h>

#include "accuracy.hpp"
#include "base.hpp"


void Ic::Accuracy::Initialise()
{
	m_set = false;

	m_prev_origin = {};
	m_prev_angles = {};

	m_walk_speed = 0.0f;
	m_look_speed = 0.0f;
	m_prev_look_speed = 0.0f;

	m_air = 0.0f;
	m_crouch = 0.0f;
}


static constexpr float WALK_SMOOTH = 15.0f;            // Origin has a quite low precision, we need to smooth it
static constexpr float LOOK_SMOOTH[2] = {12.0f, 5.0f}; // Not here
static constexpr float AIR_SMOOTH = 7.0f;
static constexpr float CROUCH_SMOOTH = 3.0f;

static constexpr float WALK_CONTRIBUTION = 0.4f;
static constexpr float LOOK_CONTRIBUTION = 0.55f;
static constexpr float AIR_CONTRIBUTION = 0.3f;
static constexpr float CROUCH_MAX = 0.3f;
static constexpr float CROUCH_MIN = 0.15f;

static constexpr float NORMALISE = 1.0f / (WALK_CONTRIBUTION + LOOK_CONTRIBUTION + AIR_CONTRIBUTION + CROUCH_MAX);


float Ic::Accuracy::Sample(Ic::Vector2 origin, Ic::Vector2 angles, int crouch, int on_air, float max_speed, float dt)
{
	const float air_float = Ic::Clamp(static_cast<float>(on_air), 0.0f, AIR_CONTRIBUTION);
	const float crouch_float = CROUCH_MAX - Ic::Clamp(static_cast<float>(crouch), 0.0f, CROUCH_MAX - CROUCH_MIN);

	if (m_set == false)
	{
		m_set = true;

		m_prev_origin = origin;
		m_prev_angles = angles;

		m_walk_speed = 0.0f;
		m_look_speed = 0.0f;
		m_prev_look_speed = 0.0f;

		m_air = air_float;
		m_crouch = crouch_float;

		return 0.0f;
	}

	if (dt <= 0.001f) // Things happen
		return 0.0f;

	// Walk
	{
		const Ic::Vector2 delta =
		    Scale(Subtract(origin, m_prev_origin), 1.0f / (dt / WALK_CONTRIBUTION)); // We want deltas in game units
		m_prev_origin = origin;

		m_walk_speed = Ic::HolmerMix(Length(delta) / max_speed, m_walk_speed, WALK_SMOOTH, dt);
	}

	// Look
	{
		const float dx = Ic::AnglesDifference(angles[0], m_prev_angles[0]) / dt;
		const float dz = Ic::AnglesDifference(angles[1], m_prev_angles[1]) / dt;
		m_prev_angles = angles;

		const float speed = sqrtf(dx * dx + dz * dz) / (360.0f / LOOK_CONTRIBUTION);
		m_look_speed =
		    Ic::AnglesHolmerMix(speed, m_look_speed, (speed > m_prev_look_speed) ? LOOK_SMOOTH[0] : LOOK_SMOOTH[1], dt);
		m_prev_look_speed = m_look_speed;
	}

	// The others
	{
		m_air = Ic::AnglesHolmerMix(air_float, m_air, AIR_SMOOTH, dt);
		m_crouch = Ic::AnglesHolmerMix(crouch_float, m_crouch, CROUCH_SMOOTH, dt);
	}


	return Get();
}


float Ic::Accuracy::Get() const
{
	const auto easing = [](float x)
	{
		return x;
		// return x * x; // Bad idea, makes it feel more "smooth" or "delayed"
	};

	return easing((m_crouch + m_air + m_look_speed + m_walk_speed) * NORMALISE);
}
