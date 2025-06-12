/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#ifndef IC_WEAPONS_HPP
#define IC_WEAPONS_HPP

#include "base.hpp"
#include <stdint.h>

namespace Ic
{

enum class WeaponMode
{
	Manual = 0,
	Semi = 1,
	Automatic = 2
};

static const char* ToString(WeaponMode mode)
{
	switch (mode)
	{
	case WeaponMode::Manual: return "MANUAL";
	case WeaponMode::Semi: return "SEMI";
	case WeaponMode::Automatic: return "AUTO";
	}

	return "";
}


struct WeaponState
{
	bool updated; // In comparision with previous state

	// Properties:
	WeaponMode mode; // Net-coded

	// Behaviour:
	int rounds_fired;
	int chamber;  // Net-coded
	int magazine; // Net-coded

	//

	static bool Compare(const WeaponState* a, const WeaponState* b)
	{
		// Mathematical memcmp():
		return (((static_cast<int>(a->mode) - static_cast<int>(b->mode)) | //
		         (a->rounds_fired - b->rounds_fired) |                     //
		         (a->chamber - b->chamber) |                               //
		         (a->magazine - b->magazine)) == 0)
		           ? false
		           : true;
	}

	static uint32_t EncodeNetWord(WeaponState s)
	{
		return (Ic::Clamp(s.magazine, 0, 127) << 0) | //
		       (Ic::Clamp(s.chamber, 0, 1) << 7) |    //
		       (static_cast<int>(s.mode) << 8);
	}

	static WeaponState DecodeNetWord(uint32_t w)
	{
		WeaponState ret = {};
		ret.magazine = (w >> 0) & 127;
		ret.chamber = (w >> 7) & 1;
		ret.mode = static_cast<WeaponMode>((w >> 8) & 3);
		return ret;
	}
};


class ClosedBoltBehaviour
{
  public:
	struct Properties
	{
		WeaponMode mode;
		double bolt_travel_duration;
		double magazine_size;
		double cock_duration;
	};

	void Initialise(const Properties*, WeaponState* out_state);
	void Frame(const Properties*, float dt, WeaponState* out_state);

	void Trigger(const Properties*, int gesture); // Gesture: 0 = Release, !0 = Press
	void Reload(const Properties*);

  private:
	using Timer = double; // Seconds (done to not mix them with other doubles)

	Timer m_time;

	int m_magazine;
	int m_chamber; // Zero or one

	Timer m_cock_done;
	Timer m_bolt_ready;

	int m_pressed;
	Timer m_first_fire;
	int m_fired_so_far; // Counter from first fire
};


class PistolWeapon
{
	Ic::ClosedBoltBehaviour::Properties m_p;
	ClosedBoltBehaviour m_behaviour;

	WeaponState m_prev_state;

  public:
	void Initialise();
	WeaponState Frame(float dt);
	void Trigger(int gesture);
	void Reload();
	WeaponMode CycleMode();
};

class ShotgunWeapon
{
	Ic::ClosedBoltBehaviour::Properties m_p;
	ClosedBoltBehaviour m_behaviour;

	WeaponState m_prev_state;

  public:
	void Initialise();
	WeaponState Frame(float dt);
	void Trigger(int gesture);
	void Reload();
	WeaponMode CycleMode();
};

class SmgWeapon
{
	Ic::ClosedBoltBehaviour::Properties m_p;
	ClosedBoltBehaviour m_behaviour;

	WeaponState m_prev_state;

  public:
	void Initialise();
	WeaponState Frame(float dt);
	void Trigger(int gesture);
	void Reload();
	WeaponMode CycleMode();
};

class ArWeapon
{
	Ic::ClosedBoltBehaviour::Properties m_p;
	ClosedBoltBehaviour m_behaviour;

	WeaponState m_prev_state;

  public:
	void Initialise();
	WeaponState Frame(float dt);
	void Trigger(int gesture);
	void Reload();
	WeaponMode CycleMode();
};

class RifleWeapon final
{
	Ic::ClosedBoltBehaviour::Properties m_p;
	ClosedBoltBehaviour m_behaviour;

	WeaponState m_prev_state;

  public:
	void Initialise();
	WeaponState Frame(float dt);
	void Trigger(int gesture);
	void Reload();
	WeaponMode CycleMode();
};

} // namespace Ic

#endif
