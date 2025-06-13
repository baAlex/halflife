/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#include <math.h>

#include "weapons.hpp"


#if 0
#ifndef CLIENT_DLL
#include "extdll.h"
#include "enginecallback.h"

#define PRINTF(...) g_engfuncs.pfnAlertMessage(at_console, __VA_ARGS__)
#endif
#else
#define PRINTF(...) // Nothing
#endif


constexpr float EPSILON = 0.001f;
static double s_time_empty = 0.0;


void Ic::ClosedBoltBehaviour::Initialise(const Properties* p, WeaponState* out_state)
{
	m_time = 0.0;

	m_magazine = p->magazine_size - 1;
	m_chamber = 1;

	m_cock_done = 0.0;
	m_bolt_ready = 0.0;

	m_pressed = 0;
	m_first_fire = 0.0;
	m_fired_so_far = 0;

	// Returns to outside world
	out_state->rounds_fired = 0;
	out_state->chamber = m_chamber;
	out_state->magazine = m_magazine;
}


void Ic::ClosedBoltBehaviour::Frame(const Properties* p, float dt, WeaponState* out_state)
{
	m_time += static_cast<Timer>(dt);

	// Set some assumptions to return
	{
		out_state->rounds_fired = 0;
		out_state->chamber = m_chamber;
		out_state->magazine = m_magazine;
	}

	// Should we chamber it? (cock it)
	if (m_chamber == 0)
	{
		if (m_time < m_cock_done)
			PRINTF("Clank... (cock animation), %.2f\n", m_time - m_first_fire); // TODO
		else
		{
			if (m_magazine > 0)
			{
				m_magazine -= 1;
				m_chamber = 1;
			}
		}
	}

	// Should we fire?
	if (m_pressed == 1 && m_time > m_bolt_ready && m_chamber > 0)
	{
		// How many rounds to fire?
		// (maybe fps are really low and we need to fire multiple rounds)
		int to_fire;

		if (m_fired_so_far == 0)
		{
			m_first_fire = m_time; // We need to track it...
			to_fire = 1;
		}
		else
		{
			// ...to extrapolate remainder shoots
			to_fire = static_cast<int>(ceil((m_time - m_first_fire) / p->bolt_travel_duration));
		}

		// Update counters
		to_fire = Ic::Min(to_fire - m_fired_so_far, m_magazine + m_chamber); // Fire what mag and chamber hold, no more
		m_fired_so_far += to_fire;

		m_magazine = m_magazine - to_fire + m_chamber; // Subtract from mag what fired, EXCEPT what was in chamber
		m_chamber = 0;
		m_bolt_ready = m_time + p->bolt_travel_duration; // Aka: when weapon will be ready for fire

		// Leave it automatically chambered for next fire
		// (that is what a moving bolt do in automatic and semi weapons)
		if ((p->mode == WeaponMode::Automatic || p->mode == WeaponMode::Semi) && m_magazine > 0)
		{
			m_magazine -= 1;
			m_chamber = 1;
		}

		// Release trigger if semi
		// (in those weapons the trigger generally disengage or block some movable piece)
		if (p->mode == WeaponMode::Semi)
			m_pressed = 0;

		// Release trigger and require cock, if manual
		else if (p->mode == WeaponMode::Manual)
		{
			// Notice how 'm_chamber' remains empty
			m_pressed = 0;
			m_cock_done = m_time + p->cock_duration;
		}

		// Returns to outside world
		out_state->rounds_fired = to_fire;
		out_state->chamber = m_chamber;
		out_state->magazine = m_magazine;

		// Developers, developers, developers
		if (1)
		{
			//	PRINTF("%i\n", ret.rounds_fired);
			if (m_magazine == 0)
				s_time_empty = m_time;
		}
	}
}


void Ic::ClosedBoltBehaviour::Trigger(const Properties* p, int gesture)
{
	// Trigger press
	if (gesture != 0)
	{
		if (m_pressed == 1)
			return;

		m_pressed = 1;
		m_fired_so_far = 0;

		if (1)
		{
			PRINTF("ClosedBoltWeapon::Trigger(), %.2f, PRESS\n", m_time);
		}
	}

	// Trigger release
	else
	{
		if (m_pressed == 0)
			return;

		m_pressed = 0;

		if (1)
		{
			double rpm = 0.0;

			if (m_fired_so_far > 0)
			{
				if (s_time_empty > EPSILON)
					rpm = (static_cast<double>(m_fired_so_far) / (s_time_empty - m_first_fire)) * 60.0;
				else
					rpm = (static_cast<double>(m_fired_so_far) / (m_time - m_first_fire)) * 60.0;
			}

			PRINTF("ClosedBoltWeapon::Trigger(), %.2f, RELEASE, Rpm: %.2f (%i rounds)\n", m_time, rpm, m_fired_so_far);
		}
	}
}


void Ic::ClosedBoltBehaviour::Reload(const Properties* p)
{
	if (m_pressed == 1)
		return;

	m_magazine = p->magazine_size;

	if (m_chamber == 0) // Require cock
	{
		m_cock_done = m_time + p->cock_duration;
		PRINTF("ClosedBoltWeapon::Reload(), WITH COCK, %.2f\n", m_time);
	}
	else
	{
		PRINTF("ClosedBoltWeapon::Reload(), NO COCK, %.2f\n", m_time);
	}

	if (1)
		s_time_empty = 0.0;
}


// Typical C++ lasagna code
// ========================
void Ic::GeneralizedWeapon::Trigger(int gesture)
{
	return m_behaviour.Trigger(&m_p, gesture);
}

void Ic::GeneralizedWeapon::Reload()
{
	return m_behaviour.Reload(&m_p);
}

Ic::WeaponMode Ic::GeneralizedWeapon::CycleMode()
{
	return m_p.mode;
}


// Pistol, Glock like
// ==================

void Ic::PistolWeapon::Initialise()
{
	m_p.mode = Ic::WeaponMode::Semi;
	m_p.bolt_travel_duration = 60.0 / 2000.0;
	m_p.magazine_size = 17;
	m_p.cock_duration = 0.2;

	m_prev_state = {};
	m_behaviour.Initialise(&m_p, &m_prev_state);
	m_prev_state.mode = m_p.mode; // TODO?
	m_prev_state.id = ID;         // TODO?
}

int Ic::PistolWeapon::Id() const
{
	return ID;
}

Ic::WeaponState Ic::PistolWeapon::Frame(float dt)
{
	Ic::WeaponState ret = m_prev_state;

	m_behaviour.Frame(&m_p, dt, &ret);

	ret.updated = Ic::WeaponState::Compare(&ret, &m_prev_state);
	m_prev_state = ret;

	return ret;
}


// Shotgun, SPAS 12 like
// =====================

void Ic::ShotgunWeapon::Initialise()
{
	m_p.mode = Ic::WeaponMode::Semi;
	m_p.bolt_travel_duration = 60.0 / 350.0;
	m_p.magazine_size = 7;
	m_p.cock_duration = .5;

	m_prev_state = {};
	m_behaviour.Initialise(&m_p, &m_prev_state);
	m_prev_state.mode = m_p.mode; // TODO?
	m_prev_state.id = ID;         // TODO?
}

int Ic::ShotgunWeapon::Id() const
{
	return ID;
}

Ic::WeaponState Ic::ShotgunWeapon::Frame(float dt)
{
	Ic::WeaponState ret = m_prev_state;
	ret.mode = m_p.mode; // May have changed

	m_behaviour.Frame(&m_p, dt, &ret);

	ret.updated = Ic::WeaponState::Compare(&ret, &m_prev_state);
	m_prev_state = ret;

	return ret;
}

Ic::WeaponMode Ic::ShotgunWeapon::CycleMode()
{
	m_p.mode = (m_p.mode == WeaponMode::Semi) ? WeaponMode::Manual : WeaponMode::Semi;
	return m_p.mode;
}


// Submachine gun, FAMAE SAF like
// obscure because I need to make it feel different to an AR
// =========================================================

void Ic::SmgWeapon::Initialise()
{
	m_p.mode = Ic::WeaponMode::Automatic;
	m_p.bolt_travel_duration = 60.0 / 1100.0; // This is different, in comparison the MP5 is 850 just like an AR
	m_p.magazine_size = 20;
	m_p.cock_duration = 0.25;

	m_prev_state = {};
	m_behaviour.Initialise(&m_p, &m_prev_state);
	m_prev_state.mode = m_p.mode; // TODO?
	m_prev_state.id = ID;         // TODO?
}

int Ic::SmgWeapon::Id() const
{
	return ID;
}

Ic::WeaponState Ic::SmgWeapon::Frame(float dt)
{
	Ic::WeaponState ret = m_prev_state;
	ret.mode = m_p.mode; // May have changed

	m_behaviour.Frame(&m_p, dt, &ret);

	ret.updated = Ic::WeaponState::Compare(&ret, &m_prev_state);
	m_prev_state = ret;

	return ret;
}

Ic::WeaponMode Ic::SmgWeapon::CycleMode()
{
	m_p.mode = (m_p.mode == WeaponMode::Automatic) ? WeaponMode::Semi : WeaponMode::Automatic;
	return m_p.mode;
}


// Assault rifle, HK416 like
// =========================

void Ic::ArWeapon::Initialise()
{
	m_p.mode = Ic::WeaponMode::Automatic;
	m_p.bolt_travel_duration = 60.0 / 850.0;
	m_p.magazine_size = 30; // NATO be like this
	m_p.cock_duration = 0.25;

	m_prev_state = {};
	m_behaviour.Initialise(&m_p, &m_prev_state);
	m_prev_state.mode = m_p.mode; // TODO?
	m_prev_state.id = ID;         // TODO?
}

int Ic::ArWeapon::Id() const
{
	return ID;
}

Ic::WeaponState Ic::ArWeapon::Frame(float dt)
{
	Ic::WeaponState ret = m_prev_state;
	ret.mode = m_p.mode; // May have changed

	m_behaviour.Frame(&m_p, dt, &ret);

	ret.updated = Ic::WeaponState::Compare(&ret, &m_prev_state);
	m_prev_state = ret;

	return ret;
}

Ic::WeaponMode Ic::ArWeapon::CycleMode()
{
	m_p.mode = (m_p.mode == WeaponMode::Automatic) ? WeaponMode::Semi : WeaponMode::Automatic;
	return m_p.mode;
}


// Rifle, Karabiner 98k like
// =========================

void Ic::RifleWeapon::Initialise()
{
	m_p.mode = Ic::WeaponMode::Manual;
	m_p.bolt_travel_duration = 60.0 / 4000.0; // Bolt barely moves...
	m_p.magazine_size = 5;
	m_p.cock_duration = 1.5; // ...manual cock is what determines rate of fire

	m_prev_state = {};
	m_behaviour.Initialise(&m_p, &m_prev_state);
	m_prev_state.mode = m_p.mode; // TODO?
	m_prev_state.id = ID;         // TODO?
}

int Ic::RifleWeapon::Id() const
{
	return ID;
}

Ic::WeaponState Ic::RifleWeapon::Frame(float dt)
{
	Ic::WeaponState ret = m_prev_state;

	m_behaviour.Frame(&m_p, dt, &ret);

	ret.updated = Ic::WeaponState::Compare(&ret, &m_prev_state);
	m_prev_state = ret;

	return ret;
}
