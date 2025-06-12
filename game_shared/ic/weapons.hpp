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

namespace Ic
{

class ClosedBoltBehaviour
{
  public:
	enum class Mode
	{
		Semi,
		Automatic,
		Manual
	};

	struct Properties
	{
		Mode mode;
		double bolt_travel_duration;
		double magazine_size;
		double cock_duration;
	};

	struct FrameOutput // What happened this frame
	{
		int rounds_fired;
		int magazine;
	};

	void Initialise(const Properties*);
	FrameOutput Frame(const Properties*, float dt);

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

  public:
	void Initialise();
	ClosedBoltBehaviour::FrameOutput Frame(float dt);
	void Trigger(int gesture);
	void Reload();
	ClosedBoltBehaviour::Mode CycleMode();
};

class ShotgunWeapon
{
	Ic::ClosedBoltBehaviour::Properties m_p;
	ClosedBoltBehaviour m_behaviour;

  public:
	void Initialise();
	ClosedBoltBehaviour::FrameOutput Frame(float dt);
	void Trigger(int gesture);
	void Reload();
	ClosedBoltBehaviour::Mode CycleMode();
};

class SmgWeapon
{
	Ic::ClosedBoltBehaviour::Properties m_p;
	ClosedBoltBehaviour m_behaviour;

  public:
	void Initialise();
	ClosedBoltBehaviour::FrameOutput Frame(float dt);
	void Trigger(int gesture);
	void Reload();
	ClosedBoltBehaviour::Mode CycleMode();
};

class ArWeapon
{
	Ic::ClosedBoltBehaviour::Properties m_p;
	ClosedBoltBehaviour m_behaviour;

  public:
	void Initialise();
	ClosedBoltBehaviour::FrameOutput Frame(float dt);
	void Trigger(int gesture);
	void Reload();
	ClosedBoltBehaviour::Mode CycleMode();
};

class RifleWeapon final
{
	Ic::ClosedBoltBehaviour::Properties m_p;
	ClosedBoltBehaviour m_behaviour;

  public:
	void Initialise();
	ClosedBoltBehaviour::FrameOutput Frame(float dt);
	void Trigger(int gesture);
	void Reload();
	ClosedBoltBehaviour::Mode CycleMode();
};

} // namespace Ic

#endif
