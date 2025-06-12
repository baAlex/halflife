/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#include "weapons.hpp"
#include "base.hpp"


#ifndef CLIENT_DLL
#include "extdll.h"
#include "enginecallback.h"

#define PRINTF(...) g_engfuncs.pfnAlertMessage(at_console, __VA_ARGS__)
#endif


constexpr float EPSILON = 0.001f;
static double s_time_empty = 0.0;


void Ic::ClosedBoltBehaviour::Initialise(const Properties* p)
{
	m_time = 0.0;

	m_magazine = p->magazine_size;
	m_chamber = 0;

	m_cock_done = 0.0;
	m_bolt_ready = 0.0;

	m_pressed = 0;
	m_first_fire = 0.0;
	m_fired_so_far = 0;
}


Ic::ClosedBoltBehaviour::FrameOutput Ic::ClosedBoltBehaviour::Frame(const Properties* p, float dt)
{
	FrameOutput ret = {};

	m_time += static_cast<Timer>(dt);

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

		// Return to outside world
		ret.magazine = m_magazine;
		ret.rounds_fired = to_fire;

		// Leave it automatically chambered for next fire
		// (that is what a moving bolt do in automatic and semi weapons)
		if ((p->mode == Mode::Automatic || p->mode == Mode::Semi) && m_magazine > 0)
		{
			m_magazine -= 1;
			m_chamber = 1;
		}

		// Release trigger if semi
		// (in those weapons the trigger generally disengage or block some movable piece)
		if (p->mode == Mode::Semi)
			m_pressed = 0;

		// Release trigger and require cock, if manual
		else if (p->mode == Mode::Manual)
		{
			// Notice how 'm_chamber' remains empty
			m_pressed = 0;
			m_cock_done = m_time + p->cock_duration;
		}

		// Developers, developers, developers
		if (1)
		{
			//	PRINTF("%i\n", ret.rounds_fired);
			if (m_magazine == 0)
				s_time_empty = m_time;
		}
	}

	return ret;
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


// Pistol, Glock like
// ==================

void Ic::PistolWeapon::Initialise()
{
	m_p.mode = Ic::ClosedBoltBehaviour::Mode::Semi;
	m_p.bolt_travel_duration = 60.0 / 2000.0;
	m_p.magazine_size = 17;
	m_p.cock_duration = 0.2;

	m_behaviour.Initialise(&m_p);
}

Ic::ClosedBoltBehaviour::FrameOutput Ic::PistolWeapon::Frame(float dt)
{
	return m_behaviour.Frame(&m_p, dt);
}

void Ic::PistolWeapon::Trigger(int gesture)
{
	return m_behaviour.Trigger(&m_p, gesture);
}

void Ic::PistolWeapon::Reload()
{
	return m_behaviour.Reload(&m_p);
}

Ic::ClosedBoltBehaviour::Mode Ic::PistolWeapon::CycleMode()
{
	return m_p.mode;
}


// Shotgun, SPAS 12 like
// =====================

void Ic::ShotgunWeapon::Initialise()
{
	m_p.mode = Ic::ClosedBoltBehaviour::Mode::Semi;
	m_p.bolt_travel_duration = 60.0 / 350.0;
	m_p.magazine_size = 7;
	m_p.cock_duration = .5;

	m_behaviour.Initialise(&m_p);
}

Ic::ClosedBoltBehaviour::FrameOutput Ic::ShotgunWeapon::Frame(float dt)
{
	return m_behaviour.Frame(&m_p, dt);
}

void Ic::ShotgunWeapon::Trigger(int gesture)
{
	return m_behaviour.Trigger(&m_p, gesture);
}

void Ic::ShotgunWeapon::Reload()
{
	return m_behaviour.Reload(&m_p);
}

Ic::ClosedBoltBehaviour::Mode Ic::ShotgunWeapon::CycleMode()
{
	m_p.mode = (m_p.mode == ClosedBoltBehaviour::Mode::Semi) ? ClosedBoltBehaviour::Mode::Manual
	                                                         : ClosedBoltBehaviour::Mode::Semi;

	PRINTF("ShotgunWeapon::CycleMode, %i\n", static_cast<int>(m_p.mode));
	return m_p.mode;
}


// Submachine gun, FAMAE SAF like
// obscure because I need to make it feel different to an AR
// =========================================================

void Ic::SmgWeapon::Initialise()
{
	m_p.mode = Ic::ClosedBoltBehaviour::Mode::Automatic;
	m_p.bolt_travel_duration = 60.0 / 1100.0; // This is different, in comparison the MP5 is 850 just like an AR
	m_p.magazine_size = 20;
	m_p.cock_duration = 0.25;

	m_behaviour.Initialise(&m_p);
}

Ic::ClosedBoltBehaviour::FrameOutput Ic::SmgWeapon::Frame(float dt)
{
	return m_behaviour.Frame(&m_p, dt);
}

void Ic::SmgWeapon::Trigger(int gesture)
{
	return m_behaviour.Trigger(&m_p, gesture);
}

void Ic::SmgWeapon::Reload()
{
	return m_behaviour.Reload(&m_p);
}

Ic::ClosedBoltBehaviour::Mode Ic::SmgWeapon::CycleMode()
{
	m_p.mode = (m_p.mode == ClosedBoltBehaviour::Mode::Automatic) ? ClosedBoltBehaviour::Mode::Semi
	                                                              : ClosedBoltBehaviour::Mode::Automatic;

	PRINTF("SmgWeapon::CycleMode, %i\n", static_cast<int>(m_p.mode));
	return m_p.mode;
}


// Assault rifle, HK416 like
// =========================

void Ic::ArWeapon::Initialise()
{
	m_p.mode = Ic::ClosedBoltBehaviour::Mode::Automatic;
	m_p.bolt_travel_duration = 60.0 / 850.0;
	m_p.magazine_size = 30; // NATO be like this
	m_p.cock_duration = 0.25;

	m_behaviour.Initialise(&m_p);
}

Ic::ClosedBoltBehaviour::FrameOutput Ic::ArWeapon::Frame(float dt)
{
	return m_behaviour.Frame(&m_p, dt);
}

void Ic::ArWeapon::Trigger(int gesture)
{
	return m_behaviour.Trigger(&m_p, gesture);
}

void Ic::ArWeapon::Reload()
{
	return m_behaviour.Reload(&m_p);
}

Ic::ClosedBoltBehaviour::Mode Ic::ArWeapon::CycleMode()
{
	m_p.mode = (m_p.mode == ClosedBoltBehaviour::Mode::Automatic) ? ClosedBoltBehaviour::Mode::Semi
	                                                              : ClosedBoltBehaviour::Mode::Automatic;

	PRINTF("ArWeapon::CycleMode, %i\n", static_cast<int>(m_p.mode));
	return m_p.mode;
}


// Rifle, Karabiner 98k like
// =========================

void Ic::RifleWeapon::Initialise()
{
	m_p.mode = Ic::ClosedBoltBehaviour::Mode::Manual;
	m_p.bolt_travel_duration = 60.0 / 4000.0; // Bolt barely moves...
	m_p.magazine_size = 5;
	m_p.cock_duration = 1.5; // ...manual cock is what determines rate of fire

	m_behaviour.Initialise(&m_p);
}

Ic::ClosedBoltBehaviour::FrameOutput Ic::RifleWeapon::Frame(float dt)
{
	return m_behaviour.Frame(&m_p, dt);
}

void Ic::RifleWeapon::Trigger(int gesture)
{
	return m_behaviour.Trigger(&m_p, gesture);
}

void Ic::RifleWeapon::Reload()
{
	return m_behaviour.Reload(&m_p);
}

Ic::ClosedBoltBehaviour::Mode Ic::RifleWeapon::CycleMode()
{
	return m_p.mode;
}
