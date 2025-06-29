/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#ifndef IC_PARTICLES_HPP
#define IC_PARTICLES_HPP

#include <stdint.h>
#include "vector.hpp"

namespace Ic
{

static constexpr int MAX_PARTICLES = 32;
static constexpr float DUST_PARTICLES_LIFE = 0.5f; // Seconds

struct Particle
{
	float life;
	float gravity;
	Ic::Vector3 position;
	Ic::Vector3 force;

	uint8_t colour[4];
};

void ParticlesInitialise(float time);

const Particle* DustParticlesUpdate(float time);
void DustParticles(int number, Vector3 position, Vector3 force, float gravity, uint8_t r, uint8_t g, uint8_t b,
                   uint8_t a);

} // namespace Ic

#endif
