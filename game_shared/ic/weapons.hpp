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
#include "vector.hpp"

#include <stdint.h>
#include <functional>

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

	float accuracy_force; // [1]
	float accuracy_decay; // [2]
	float spread;

	int pellets_no;
	float pellets_dispersion;

	const char* event_fire;
	const char* fire_sound;

	float fire_kick; // TODO, temporary

	// [1] Formula '1.0f / static_cast<float>(BEHAVIOUR_PROPS.magazine_size / 2)' means:
	// 'accuracy reaches 1 half way the magazine'. One being bad accuracy, two terrible.

	// [2] Decay communicating an optimal rate/cadence of fire to adopt
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


void WeaponFire(const WeaponProperties* props, Vector3 origin, Vector3 view_angle, uint16_t rng_state, int rounds_no,
                float accuracy, std::function<void(Vector3 start, Vector3 end, Vector3 up, Vector3 right)>);


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


#define COMMON_SPREAD 3100.0f // A thing less to configure


// Pistol, Glock like
// ==================

class PistolWeapon final : public GeneralizedWeapon
{
  public:
	static constexpr ClosedBoltBehaviour::Properties BEHAVIOUR_PROPS = {
	    60.0 / 2000.0, // Bolt travel duration
	    17,            // Magazine size
	    0.2,           // Cock duration
	};

	static constexpr WeaponProperties PROPS = {
	    1,                                                            // Id
	    "Pistol",                                                     // Short name
	    1,                                                            // Modes number
	    {WeaponMode::Semi},                                           // Modes
	    1.0f / static_cast<float>(BEHAVIOUR_PROPS.magazine_size / 4), // Accuracy force
	    2.0f / 0.8f,                                                  // Accuracy decay
	    COMMON_SPREAD,                                                // Spread
	    1,                                                            // Pellets
	    0.0f,                                                         // Pellets dispersion
	    "events/pistol-fire.sc",                                      // Fire event
	    "weapons/pistol-fire.wav",                                    // Fire sound
	    2.0f,                                                         // Fire kick
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
	static constexpr ClosedBoltBehaviour::Properties BEHAVIOUR_PROPS = {
	    60.0 / 350.0, // Bolt travel duration
	    7,            // Magazine size
	    0.75,         // Cock duration
	};

	static constexpr WeaponProperties PROPS = {
	    2,                                                            // Id
	    "Shotgun",                                                    // Short name
	    2,                                                            // Modes number
	    {WeaponMode::Semi, WeaponMode::Manual},                       // Modes
	    1.0f / static_cast<float>(BEHAVIOUR_PROPS.magazine_size - 1), // Accuracy force
	    2.0f / 1.75f,                                                 // Accuracy decay
	    COMMON_SPREAD,                                                // Spread
	    12,                                                           // Pellets
	    350.0f,                                                       // Pellets dispersion
	    "events/shotgun-fire.sc",                                     // Fire event
	    "weapons/shotgun-fire.wav",                                   // Fire sound
	    3.0f,                                                         // Fire kick
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
	static constexpr ClosedBoltBehaviour::Properties BEHAVIOUR_PROPS = {
	    60.0 / 1100.0, // Bolt travel duration (this is different, in comparison an MP5 is 850 just like an AR)
	    20,            // Magazine size
	    0.25,          // Cock duration
	};

	static constexpr WeaponProperties PROPS = {
	    3,                                         // Id
	    "SMG",                                     // Short name
	    2,                                         // Modes number
	    {WeaponMode::Automatic, WeaponMode::Semi}, // Modes

	    1.0f /
	        static_cast<float>(BEHAVIOUR_PROPS.magazine_size / 2 + BEHAVIOUR_PROPS.magazine_size / 4), // Accuracy force

	    (2.0f / 0.5f),          // Accuracy decay
	    COMMON_SPREAD,          // Spread
	    1,                      // Pellets
	    0.0f,                   // Pellets dispersion
	    "events/smg-fire.sc",   // Fire event
	    "weapons/smg-fire.wav", // Fire sound
	    1.0f,                   // Fire kick
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
	static constexpr ClosedBoltBehaviour::Properties BEHAVIOUR_PROPS = {
	    60.0 / 850.0, // Bolt travel duration
	    30,           // Magazine size, NATO be like this
	    0.25,         // Cock duration
	};

	static constexpr WeaponProperties PROPS = {
	    4,                                         // Id
	    "AR",                                      // Short name
	    2,                                         // Modes number
	    {WeaponMode::Automatic, WeaponMode::Semi}, // Modes
	    SmgWeapon::PROPS.accuracy_force,           // Accuracy force

	    // Accuracy decay, first I'm compensating being a slower weapon so both
	    // perform the same, then I'm making it 1.75 "better"
	    SmgWeapon::PROPS.accuracy_decay*(SmgWeapon::BEHAVIOUR_PROPS.bolt_travel_duration /
	                                     BEHAVIOUR_PROPS.bolt_travel_duration) *
	        1.75f,

	    COMMON_SPREAD,         // Spread
	    1,                     // Pellets
	    0.0f,                  // Pellets dispersion
	    "events/ar-fire.sc",   // Fire event
	    "weapons/ar-fire.wav", // Fire sound
	    1.5f,                  // Fire kick
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
	static constexpr ClosedBoltBehaviour::Properties BEHAVIOUR_PROPS = {
	    60.0 / 4000.0, // Bolt travel duration (barely moves in a bolt action)
	    5,             // Magazine size
	    1.5,           // Cock duration (manual cock is what determines rate of fire)
	};

	static constexpr WeaponProperties PROPS = {
	    5,                                                        // Id
	    "Rifle",                                                  // Short name
	    1,                                                        // Modes number
	    {WeaponMode::Manual},                                     // Modes
	    1.0f / static_cast<float>(BEHAVIOUR_PROPS.magazine_size), // Accuracy force
	    2.0f / 1.25f,                                             // Accuracy decay
	    COMMON_SPREAD,                                            // Spread
	    1,                                                        // Pellets
	    0.0f,                                                     // Pellets dispersion
	    "events/rifle-fire.sc",                                   // Fire event
	    "weapons/rifle-fire.wav",                                 // Fire sound
	    4.0f,                                                     // Fire kick
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
