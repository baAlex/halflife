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
	int id;          // Net-coded
	WeaponMode mode; // Net-coded

	// Behaviour:
	int rounds_fired;
	int chamber;  // Net-coded
	int magazine; // Net-coded

	//

	static bool Compare(const WeaponState* a, const WeaponState* b)
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

	static uint32_t EncodeNetWord(WeaponState s)
	{
		return (Ic::Clamp(s.id, 0, 7) << 0) |         //
		       (Ic::Clamp(s.magazine, 0, 127) << 4) | //
		       (Ic::Clamp(s.chamber, 0, 1) << 11) |   //
		       (static_cast<int>(s.mode) << 12);
	}

	static WeaponState DecodeNetWord(uint32_t w)
	{
		WeaponState ret = {};
		ret.id = (w >> 0) & 7;
		ret.magazine = (w >> 4) & 127;
		ret.chamber = (w >> 11) & 1;
		ret.mode = static_cast<WeaponMode>((w >> 12) & 3);
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


class PistolWeapon;
class ShotgunWeapon;
class SmgWeapon;
class ArWeapon;
class RifleWeapon;

class GeneralizedWeapon
{
	ClosedBoltBehaviour::Properties m_p;
	ClosedBoltBehaviour m_behaviour;

	WeaponState m_prev_state;

	// Friendship is magic :)
	friend PistolWeapon;
	friend ShotgunWeapon;
	friend SmgWeapon;
	friend ArWeapon;
	friend RifleWeapon;

  public:
	virtual int Id() const = 0;
	virtual WeaponState Frame(float dt) = 0;
	virtual void Trigger(int gesture);
	virtual void Reload();
	virtual WeaponMode CycleMode();
};


class PistolWeapon final : public GeneralizedWeapon
{
  public:
	static constexpr int ID = 1;
	static constexpr const char* SHORT_NAME = "Pistol";

	void Initialise();
	int Id() const override;
	WeaponState Frame(float dt) override;
};

class ShotgunWeapon final : public GeneralizedWeapon
{
  public:
	static constexpr int ID = 2;
	static constexpr const char* SHORT_NAME = "Shotgun";

	void Initialise();
	int Id() const override;
	WeaponState Frame(float dt) override;
	WeaponMode CycleMode() override;
};

class SmgWeapon final : public GeneralizedWeapon
{
  public:
	static constexpr int ID = 3;
	static constexpr const char* SHORT_NAME = "SMG";

	void Initialise();
	int Id() const override;
	WeaponState Frame(float dt) override;
	WeaponMode CycleMode() override;
};

class ArWeapon final : public GeneralizedWeapon
{
  public:
	static constexpr int ID = 4;
	static constexpr const char* SHORT_NAME = "AR";

	void Initialise();
	int Id() const override;
	WeaponState Frame(float dt) override;
	WeaponMode CycleMode() override;
};

class RifleWeapon final : public GeneralizedWeapon
{
  public:
	static constexpr int ID = 5;
	static constexpr const char* SHORT_NAME = "Rifle";

	void Initialise();
	int Id() const override;
	WeaponState Frame(float dt) override;
};

} // namespace Ic

#endif
