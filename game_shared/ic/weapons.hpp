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

const char* ToString(WeaponMode mode);


struct WeaponProperties
{
	int id; // Unique per weapon
	const char* short_name;

	int modes_no; // No more than 2, there is no code for that
	WeaponMode mode[2];

	const char* sound_fire;
};


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

	static bool Compare(const WeaponState* a, const WeaponState* b);
	static uint32_t EncodeNetWord(WeaponState s);
	static WeaponState DecodeNetWord(uint32_t w);
};


class ClosedBoltBehaviour
{
  public:
	struct Properties
	{
		double bolt_travel_duration;
		double magazine_size;
		double cock_duration;
	};

	void Initialise(const Properties*, WeaponState* out_state);
	void Frame(const Properties*, WeaponMode mode, float dt, WeaponState* out_state);

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
	WeaponMode m_mode;
	WeaponState m_prev_state;
	ClosedBoltBehaviour m_behaviour;

	// Friendship is magic :)
	friend PistolWeapon;
	friend ShotgunWeapon;
	friend SmgWeapon;
	friend ArWeapon;
	friend RifleWeapon;

  public:
	void CommonInitialisation(int id, const ClosedBoltBehaviour::Properties*, WeaponMode);
	Ic::WeaponState CommonFrameWithoutModeSwitch(const ClosedBoltBehaviour::Properties* props, float dt);
	Ic::WeaponState CommonFrameWithModeSwitch(const ClosedBoltBehaviour::Properties* props, float dt);
	Ic::WeaponMode CommonSwitchMode(WeaponMode a, WeaponMode b);

	virtual int Id() const = 0;
	virtual const WeaponProperties* GetWeaponProperties() const = 0;
	virtual const ClosedBoltBehaviour::Properties* GetBehaviourProperties() const = 0;

	virtual WeaponState Frame(float dt) = 0;
	virtual void Trigger(int gesture) = 0;
	virtual void Reload() = 0;
	virtual WeaponMode SwitchMode() = 0;
};


// Pistol, Glock like
// ==================

class PistolWeapon final : public GeneralizedWeapon
{
  public:
	static constexpr WeaponProperties PROPS = {
	    1,                     // Id
	    "Pistol",              // Short name
	    1,                     // Modes number
	    {WeaponMode::Semi},    // Modes
	    "weapons/pl_gun3.wav", // Fire sound
	};

	static constexpr ClosedBoltBehaviour::Properties BEHAVIOUR_PROPS = {
	    60.0 / 2000.0, // Bolt travel duration
	    17,            // Magazine size
	    0.2,           // Cock duration
	};

	void Initialise();

	int Id() const override;
	const WeaponProperties* GetWeaponProperties() const override;
	const ClosedBoltBehaviour::Properties* GetBehaviourProperties() const override;

	WeaponState Frame(float dt) override;
	void Trigger(int gesture) override;
	void Reload() override;
	WeaponMode SwitchMode() override;
};


// Shotgun, SPAS 12 like
// =====================

class ShotgunWeapon final : public GeneralizedWeapon
{
  public:
	static constexpr WeaponProperties PROPS = {
	    2,                                      // Id
	    "Shotgun",                              // Short name
	    2,                                      // Modes number
	    {WeaponMode::Semi, WeaponMode::Manual}, // Modes
	    "weapons/sbarrel1.wav",                 // Fire sound
	};

	static constexpr ClosedBoltBehaviour::Properties BEHAVIOUR_PROPS = {
	    60.0 / 350.0, // Bolt travel duration
	    7,            // Magazine size
	    0.5,          // Cock duration
	};

	void Initialise();

	int Id() const override;
	const WeaponProperties* GetWeaponProperties() const override;
	const ClosedBoltBehaviour::Properties* GetBehaviourProperties() const override;

	WeaponState Frame(float dt) override;
	void Trigger(int gesture) override;
	void Reload() override;
	WeaponMode SwitchMode() override;
};


// Submachine gun, FAMAE SAF like
// obscure because I need to make it feel different to an AR
// =========================================================

class SmgWeapon final : public GeneralizedWeapon
{
  public:
	static constexpr WeaponProperties PROPS = {
	    3,                                         // Id
	    "SMG",                                     // Short name
	    2,                                         // Modes number
	    {WeaponMode::Automatic, WeaponMode::Semi}, // Modes
	    "weapons/pl_gun1.wav",                     // Fire sound
	};

	static constexpr ClosedBoltBehaviour::Properties BEHAVIOUR_PROPS = {
	    60.0 / 1100.0, // Bolt travel duration (this is different, in comparison an MP5 is 850 just like an AR)
	    20,            // Magazine size
	    0.25,          // Cock duration
	};

	void Initialise();

	int Id() const override;
	const WeaponProperties* GetWeaponProperties() const override;
	const ClosedBoltBehaviour::Properties* GetBehaviourProperties() const override;

	WeaponState Frame(float dt) override;
	void Trigger(int gesture) override;
	void Reload() override;
	WeaponMode SwitchMode() override;
};


// Assault rifle, HK416 like
// =========================

class ArWeapon final : public GeneralizedWeapon
{
  public:
	static constexpr WeaponProperties PROPS = {
	    4,                                         // Id
	    "AR",                                      // Short name
	    2,                                         // Modes number
	    {WeaponMode::Automatic, WeaponMode::Semi}, // Modes
	    "weapons/hks1.wav",                        // Fire sound
	};

	static constexpr ClosedBoltBehaviour::Properties BEHAVIOUR_PROPS = {
	    60.0 / 850.0, // Bolt travel duration
	    30,           // Magazine size, NATO be like this
	    0.25,         // Cock duration
	};

	void Initialise();

	int Id() const override;
	const WeaponProperties* GetWeaponProperties() const override;
	const ClosedBoltBehaviour::Properties* GetBehaviourProperties() const override;

	WeaponState Frame(float dt) override;
	void Trigger(int gesture) override;
	void Reload() override;
	WeaponMode SwitchMode() override;
};


// Rifle, Karabiner 98k like
// =========================

class RifleWeapon final : public GeneralizedWeapon
{
  public:
	static constexpr WeaponProperties PROPS = {
	    5,                       // Id
	    "Rifle",                 // Short name
	    1,                       // Modes number
	    {WeaponMode::Manual},    // Modes
	    "weapons/357_shot1.wav", // Fire sound
	};

	static constexpr ClosedBoltBehaviour::Properties BEHAVIOUR_PROPS = {
	    60.0 / 4000.0, // Bolt travel duration (barely moves in a bolt action)
	    5,             // Magazine size
	    1.5,           // Cock duration (manual cock is what determines rate of fire)
	};

	void Initialise();

	int Id() const override;
	const WeaponProperties* GetWeaponProperties() const override;
	const ClosedBoltBehaviour::Properties* GetBehaviourProperties() const override;

	WeaponState Frame(float dt) override;
	void Trigger(int gesture) override;
	void Reload() override;
	WeaponMode SwitchMode() override;
};


void RetrieveWeaponProps(int id, const WeaponProperties** props,
                         const ClosedBoltBehaviour::Properties** behaviour_props);


} // namespace Ic

#endif
