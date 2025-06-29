/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#include <math.h>
#include "particles.hpp"
#include "base.hpp"


float s_prev_time;
static uint16_t s_rng = 123;

static Ic::Particle s_dust_particles[Ic::MAX_PARTICLES]; // Nothing fancy


void Ic::ParticlesInitialise(float time)
{
	s_prev_time = time;

	for (Particle* p = s_dust_particles; p < s_dust_particles + Ic::MAX_PARTICLES; p += 1)
	{
		p->life = 0.0f;
	}
}


const Ic::Particle* Ic::DustParticlesUpdate(float time)
{
	const float dt = time - s_prev_time;
	s_prev_time = time;

	const float friction = expf(-2.0f * dt);

	for (Particle* p = s_dust_particles; p < s_dust_particles + Ic::MAX_PARTICLES; p += 1)
	{
		p->life -= dt;
		if (p->life <= 0.0f)
			continue;

		p->position = Add(p->position, Scale(p->force, dt));
		p->force = Scale(p->force, friction);
		p->force.z += p->gravity;
	}

	return s_dust_particles;
}


void Ic::DustParticles(int number, Ic::Vector3 position, Ic::Vector3 force, float gravity, uint8_t r, uint8_t g,
                       uint8_t b, uint8_t a)
{
	for (int n = 0; n < number; n += 1)
	{
		Particle* p = nullptr;
		Particle* oldest = s_dust_particles;

		for (Particle* i = s_dust_particles; i < s_dust_particles + Ic::MAX_PARTICLES; i += 1)
		{
			if (i->life <= 0.0f)
			{
				p = i;
				goto found;
			}

			if (i->life < oldest->life)
				oldest = i;
		}

		if (p == nullptr)
			p = oldest;

	found:
		const float r1 = (Ic::RandomFloat(&s_rng) * 2.0f - 1.0f) * 64.0f;
		const float r2 = (Ic::RandomFloat(&s_rng) * 2.0f - 1.0f) * 64.0f;
		const float r3 = (Ic::RandomFloat(&s_rng) * 2.0f - 1.0f) * 64.0f;

		p->life = DUST_PARTICLES_LIFE;
		p->gravity = gravity;
		p->position = position;
		p->force = {force.x + r1, force.y + r2, force.z + r3};

		p->colour[0] = r;
		p->colour[1] = g;
		p->colour[2] = b;
		p->colour[3] = a;
	}
}
