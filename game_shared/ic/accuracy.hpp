/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#ifndef IC_ACCURACY_HPP
#define IC_ACCURACY_HPP

#include "vector.hpp"

namespace Ic
{

class Accuracy
{
  public:
	void Initialise();
	float Sample(Ic::Vector2 xy_origin, Ic::Vector2 xz_angles, int crouch, int on_air, float max_speed, float dt);
	float Fire(float force, float decay);
	float Get() const;

  private:
	bool m_set;

	Ic::Vector2 m_prev_origin;
	Ic::Vector2 m_prev_angles;

	float m_walk_speed;
	float m_look_speed;
	float m_prev_look_speed;

	float m_air;
	float m_crouch;
	float m_fire;

	float m_fire_decay;
};

} // namespace Ic

#endif
