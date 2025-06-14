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


// ================


const char* Ic::ToString(WeaponMode mode)
{
	switch (mode)
	{
	case WeaponMode::Manual: return "MANUAL";
	case WeaponMode::Semi: return "SEMI";
	case WeaponMode::Automatic: return "AUTO";
	}

	return "";
}


bool Ic::WeaponState::Compare(const WeaponState* a, const WeaponState* b)
{
	// Mathematical memcmp():
	// Not 'id', that shouldn't change
	return (((static_cast<int>(a->mode) - static_cast<int>(b->mode)) | //
	         (a->rounds_fired - b->rounds_fired) |                     //
	         (a->chamber - b->chamber) |                               //
	         (a->magazine - b->magazine)) == 0)
	           ? false
	           : true;
}

uint32_t Ic::WeaponState::EncodeNetWord(WeaponState s)
{
	return (Ic::Clamp(s.id, 0, 7) << 0) |         //
	       (Ic::Clamp(s.magazine, 0, 127) << 4) | //
	       (Ic::Clamp(s.chamber, 0, 1) << 11) |   //
	       (static_cast<int>(s.mode) << 12);
}

Ic::WeaponState Ic::WeaponState::DecodeNetWord(uint32_t w)
{
	WeaponState ret = {};
	ret.id = (w >> 0) & 7;
	ret.magazine = (w >> 4) & 127;
	ret.chamber = (w >> 11) & 1;
	ret.mode = static_cast<WeaponMode>((w >> 12) & 3);
	return ret;
}


// ================


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


void Ic::ClosedBoltBehaviour::Frame(const Properties* p, WeaponMode mode, float dt, WeaponState* out_state)
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
		if ((mode == WeaponMode::Automatic || mode == WeaponMode::Semi) && m_magazine > 0)
		{
			m_magazine -= 1;
			m_chamber = 1;
		}

		// Release trigger if semi
		// (in those weapons the trigger generally disengage or block some movable piece)
		if (mode == WeaponMode::Semi)
			m_pressed = 0;

		// Release trigger and require cock, if manual
		else if (mode == WeaponMode::Manual)
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


// ================


void Ic::GeneralizedWeapon::CommonInitialisation(int id, const ClosedBoltBehaviour::Properties* props, WeaponMode mode)
{
	m_mode = mode;
	m_prev_state = {};

	m_behaviour.Initialise(props, &m_prev_state);

	// Keep this ones
	m_prev_state.id = id;
	m_prev_state.mode = m_mode;
}

Ic::WeaponState Ic::GeneralizedWeapon::CommonFrameWithoutModeSwitch(const ClosedBoltBehaviour::Properties* props,
                                                                    float dt)
{
	Ic::WeaponState ret = m_prev_state;

	m_behaviour.Frame(props, m_mode, dt, &ret);

	ret.updated = WeaponState::Compare(&ret, &m_prev_state);
	m_prev_state = ret;
	return ret;
}

Ic::WeaponState Ic::GeneralizedWeapon::CommonFrameWithModeSwitch(const ClosedBoltBehaviour::Properties* props, float dt)
{
	WeaponState ret = m_prev_state;
	ret.mode = m_mode; // May have changed

	m_behaviour.Frame(props, m_mode, dt, &ret);

	ret.updated = WeaponState::Compare(&ret, &m_prev_state);
	m_prev_state = ret;
	return ret;
}

Ic::WeaponMode Ic::GeneralizedWeapon::CommonSwitchMode(WeaponMode a, WeaponMode b)
{
	m_mode = (m_mode == a) ? b : a;
	return m_mode;
}


// ================


// clang-format off
void Ic::PistolWeapon::Initialise() { CommonInitialisation(PROPS.id, &BEHAVIOUR_PROPS, PROPS.mode[0]); }
int Ic::PistolWeapon::Id() const                                                            { return PROPS.id; }
const Ic::WeaponProperties* Ic::PistolWeapon::GetWeaponProperties() const                   { return &PROPS; };
const Ic::ClosedBoltBehaviour::Properties* Ic::PistolWeapon::GetBehaviourProperties() const { return &BEHAVIOUR_PROPS; };
Ic::WeaponState Ic::PistolWeapon::Frame(float dt) { return CommonFrameWithoutModeSwitch(&BEHAVIOUR_PROPS, dt); }
void Ic::PistolWeapon::Trigger(int gesture)       { return m_behaviour.Trigger(&BEHAVIOUR_PROPS, gesture); }
void Ic::PistolWeapon::Reload()                   { return m_behaviour.Reload(&BEHAVIOUR_PROPS); }
Ic::WeaponMode Ic::PistolWeapon::SwitchMode()     { return m_mode; }


void Ic::ShotgunWeapon::Initialise() { CommonInitialisation(PROPS.id, &BEHAVIOUR_PROPS, PROPS.mode[0]); }
 int Ic::ShotgunWeapon::Id() const                                                            { return PROPS.id; }
const Ic::WeaponProperties* Ic::ShotgunWeapon::GetWeaponProperties() const                   { return &PROPS; };
const Ic::ClosedBoltBehaviour::Properties* Ic::ShotgunWeapon::GetBehaviourProperties() const { return &BEHAVIOUR_PROPS; };
Ic::WeaponState Ic::ShotgunWeapon::Frame(float dt) { return CommonFrameWithModeSwitch(&BEHAVIOUR_PROPS, dt); }
void Ic::ShotgunWeapon::Trigger(int gesture)       { return m_behaviour.Trigger(&BEHAVIOUR_PROPS, gesture); }
void Ic::ShotgunWeapon::Reload()                   { return m_behaviour.Reload(&BEHAVIOUR_PROPS); }
Ic::WeaponMode Ic::ShotgunWeapon::SwitchMode()     { CommonSwitchMode(PROPS.mode[0], PROPS.mode[1]); }


void Ic::SmgWeapon::Initialise() { CommonInitialisation(PROPS.id, &BEHAVIOUR_PROPS, PROPS.mode[0]); }
int Ic::SmgWeapon::Id() const                                                            { return PROPS.id; }
const Ic::WeaponProperties* Ic::SmgWeapon::GetWeaponProperties() const                   { return &PROPS; };
const Ic::ClosedBoltBehaviour::Properties* Ic::SmgWeapon::GetBehaviourProperties() const { return &BEHAVIOUR_PROPS; };
Ic::WeaponState Ic::SmgWeapon::Frame(float dt) { return CommonFrameWithModeSwitch(&BEHAVIOUR_PROPS, dt); }
void Ic::SmgWeapon::Trigger(int gesture)       { return m_behaviour.Trigger(&BEHAVIOUR_PROPS, gesture); }
void Ic::SmgWeapon::Reload()                   { return m_behaviour.Reload(&BEHAVIOUR_PROPS); }
Ic::WeaponMode Ic::SmgWeapon::SwitchMode()     { CommonSwitchMode(PROPS.mode[0], PROPS.mode[1]); }


void Ic::ArWeapon::Initialise() { CommonInitialisation(PROPS.id, &BEHAVIOUR_PROPS, PROPS.mode[0]); }
int Ic::ArWeapon::Id() const                                                            { return PROPS.id; }
const Ic::WeaponProperties* Ic::ArWeapon::GetWeaponProperties() const                   { return &PROPS; };
const Ic::ClosedBoltBehaviour::Properties* Ic::ArWeapon::GetBehaviourProperties() const { return &BEHAVIOUR_PROPS; };
Ic::WeaponState Ic::ArWeapon::Frame(float dt) { return CommonFrameWithModeSwitch(&BEHAVIOUR_PROPS, dt); }
void Ic::ArWeapon::Trigger(int gesture)       { return m_behaviour.Trigger(&BEHAVIOUR_PROPS, gesture); }
void Ic::ArWeapon::Reload()                   { return m_behaviour.Reload(&BEHAVIOUR_PROPS); }
Ic::WeaponMode Ic::ArWeapon::SwitchMode()     { CommonSwitchMode(PROPS.mode[0], PROPS.mode[1]); }


void Ic::RifleWeapon::Initialise() { CommonInitialisation(PROPS.id, &BEHAVIOUR_PROPS, PROPS.mode[0]); }
int Ic::RifleWeapon::Id() const                                                            { return PROPS.id; }
const Ic::WeaponProperties* Ic::RifleWeapon::GetWeaponProperties() const                   { return &PROPS; };
const Ic::ClosedBoltBehaviour::Properties* Ic::RifleWeapon::GetBehaviourProperties() const { return &BEHAVIOUR_PROPS; };
Ic::WeaponState Ic::RifleWeapon::Frame(float dt) { return CommonFrameWithoutModeSwitch(&BEHAVIOUR_PROPS, dt); }
void Ic::RifleWeapon::Trigger(int gesture)       { return m_behaviour.Trigger(&BEHAVIOUR_PROPS, gesture); }
void Ic::RifleWeapon::Reload()                   { return m_behaviour.Reload(&BEHAVIOUR_PROPS); }
Ic::WeaponMode Ic::RifleWeapon::SwitchMode()     { return m_mode; }
// clang-format on


// ================


extern constexpr Ic::WeaponProperties Ic::PistolWeapon::PROPS;  // C++11 quirk, is fixed in C++17
extern constexpr Ic::WeaponProperties Ic::ShotgunWeapon::PROPS; // https://stackoverflow.com/a/53350948
extern constexpr Ic::WeaponProperties Ic::SmgWeapon::PROPS;
extern constexpr Ic::WeaponProperties Ic::ArWeapon::PROPS;
extern constexpr Ic::WeaponProperties Ic::RifleWeapon::PROPS;

extern constexpr Ic::ClosedBoltBehaviour::Properties Ic::PistolWeapon::BEHAVIOUR_PROPS;
extern constexpr Ic::ClosedBoltBehaviour::Properties Ic::ShotgunWeapon::BEHAVIOUR_PROPS;
extern constexpr Ic::ClosedBoltBehaviour::Properties Ic::SmgWeapon::BEHAVIOUR_PROPS;
extern constexpr Ic::ClosedBoltBehaviour::Properties Ic::ArWeapon::BEHAVIOUR_PROPS;
extern constexpr Ic::ClosedBoltBehaviour::Properties Ic::RifleWeapon::BEHAVIOUR_PROPS;

void Ic::RetrieveWeaponProps(int id, const WeaponProperties** props,
                             const ClosedBoltBehaviour::Properties** behaviour_props)
{
	// I basically re-invented a dynamic type thingie
	switch (id)
	{
	case Ic::PistolWeapon::PROPS.id:
		*props = &Ic::PistolWeapon::PROPS;
		*behaviour_props = &Ic::PistolWeapon::BEHAVIOUR_PROPS;
		return;
	case Ic::ShotgunWeapon::PROPS.id:
		*props = &Ic::ShotgunWeapon::PROPS;
		*behaviour_props = &Ic::ShotgunWeapon::BEHAVIOUR_PROPS;
		return;
	case Ic::SmgWeapon::PROPS.id:
		*props = &Ic::SmgWeapon::PROPS;
		*behaviour_props = &Ic::SmgWeapon::BEHAVIOUR_PROPS;
		return;
	case Ic::ArWeapon::PROPS.id:
		*props = &Ic::ArWeapon::PROPS;
		*behaviour_props = &Ic::ArWeapon::BEHAVIOUR_PROPS;
		return;
	case Ic::RifleWeapon::PROPS.id:
		*props = &Ic::RifleWeapon::PROPS;
		*behaviour_props = &Ic::RifleWeapon::BEHAVIOUR_PROPS;
		return;
	}

	*props = nullptr;
	*behaviour_props = nullptr;
}
