/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

// ORDER OF INCLUDES IS THIS AND NO OTHER
#include "wrect.h"
#include "cl_dll.h"
#include "APIProxy.h"
#include "entity_types.h"

#include <math.h>
#include "ic/base.hpp"

#include "particles.hpp"


static constexpr int MAX_PARTICLES = 32;
static constexpr float DUST_PARTICLES_LIFE = 0.5f; // Seconds

struct Particle
{
	float life;
	float gravity;
	Ic::Vector3 position;
	Ic::Vector3 force;

	Ic::Vector4 colour;
};


static float s_prev_time;
static HSPRITE s_dust_sprite;
static uint16_t s_rng = 123;

static Particle s_dust_particles[MAX_PARTICLES]; // Nothing fancy
static cl_entity_t s_dust_particles_entities[MAX_PARTICLES] = {};


void Ic::ParticlesInitialise()
{
	s_prev_time = gEngfuncs.GetClientTime();
	s_dust_sprite = gEngfuncs.pfnSPR_Load("sprites/dust.spr");

	for (Particle* p = s_dust_particles; p < s_dust_particles + MAX_PARTICLES; p += 1)
	{
		p->life = 0.0f;
	}
}


static void sUpdateParticles()
{
	const float time = gEngfuncs.GetClientTime();
	const float dt = time - s_prev_time;
	s_prev_time = time;

	const float friction = expf(-2.0f * dt);

	for (Particle* p = s_dust_particles; p < s_dust_particles + MAX_PARTICLES; p += 1)
	{
		p->life -= dt;
		if (p->life <= 0.0f)
			continue;

		p->position = Add(p->position, Scale(p->force, dt));
		p->force = Scale(p->force, friction);
		p->force.z += p->gravity;
	}
}


void Ic::ParticlesEntities()
{
	model_s* sprite_as_model = (model_s*)(gEngfuncs.GetSpritePointer(s_dust_sprite)); // Nasty 'const' conversion

	sUpdateParticles();

	for (const Particle* p = s_dust_particles; p < s_dust_particles + MAX_PARTICLES; p += 1)
	{
		if (p->life <= 0.0f)
			continue;

		const int index = static_cast<int>(p - s_dust_particles);
		cl_entity_s* ent = s_dust_particles_entities + index;

		ent->index = index + 123;     // It cannot be zero, and that 123 is an offset for... just
		ent->model = sprite_as_model; // in case, not override Bench_AddObjects(), Game_AddObjects()
		                              // and GetClientVoiceMgr(), objects

		ent->curstate.rendermode = kRenderTransAlpha; // Only mode that more or less works in Software and OpenGL
		ent->curstate.renderamt =
		    static_cast<int>(p->life * 255.0f * (1.0f / DUST_PARTICLES_LIFE) * p->colour[3]); // Only in OpenGL
		ent->curstate.rendercolor.r = static_cast<byte>(p->colour[0] * 255.0f);
		ent->curstate.rendercolor.g = static_cast<byte>(p->colour[1] * 255.0f);
		ent->curstate.rendercolor.b = static_cast<byte>(p->colour[2] * 255.0f);

		ent->origin[0] = p->position.x;
		ent->origin[1] = p->position.y;
		ent->origin[2] = p->position.z;

		gEngfuncs.CL_CreateVisibleEntity(ET_TEMPENTITY, ent);
	}
}


void Ic::DustParticles(int number, Ic::Vector3 position, Ic::Vector3 force, float gravity, Ic::Vector4 colour)
{
	for (int n = 0; n < number; n += 1)
	{
		Particle* p = nullptr;
		Particle* oldest = s_dust_particles;

		for (Particle* i = s_dust_particles; i < s_dust_particles + MAX_PARTICLES; i += 1)
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

		p->colour = colour;
	}
}
