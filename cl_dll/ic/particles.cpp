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

#include "temp-entities.hpp"
#include "particles.hpp"


static uint16_t s_rng = 123;
static HSPRITE s_dust_sprite;


void Ic::ParticlesInitialise()
{
	s_dust_sprite = gEngfuncs.pfnSPR_Load("sprites/dust.spr");
}


struct Dust
{
	static constexpr float LIFE = 0.5f; // Seconds

	Ic::Vector3 position;
	Ic::Vector3 force;
	Ic::Vector4 colour;
	float life;
	float gravity;

	static void CreateCallback(float dt, void* user_data, cl_entity_t* entity)
	{
		Dust* data = reinterpret_cast<Dust*>(user_data);

		entity->origin[0] = data->position.x;
		entity->origin[1] = data->position.y;
		entity->origin[2] = data->position.z;

		data->life = Dust::LIFE;

		entity->model = (model_s*)(gEngfuncs.GetSpritePointer(s_dust_sprite)); // Nasty 'const' conversion

		entity->curstate.rendermode = kRenderTransAlpha; // Only mode that more or less works in Software and OpenGl
		entity->curstate.rendercolor.r = static_cast<byte>(data->colour[0] * 255.0f);
		entity->curstate.rendercolor.g = static_cast<byte>(data->colour[1] * 255.0f);
		entity->curstate.rendercolor.b = static_cast<byte>(data->colour[2] * 255.0f);
	}

	static int UpdateCallback(float dt, void* user_data, cl_entity_t* entity)
	{
		Dust* data = reinterpret_cast<Dust*>(user_data);

		data->life -= dt;
		if (data->life <= 0.0f)
			return 1;

		const float friction = expf(-2.0f * dt);

		data->position = Add(data->position, Scale(data->force, dt));
		data->force = Scale(data->force, friction);
		data->force.z += data->gravity * dt;

		entity->origin[0] = data->position.x;
		entity->origin[1] = data->position.y;
		entity->origin[2] = data->position.z;

		entity->curstate.renderamt =
		    static_cast<int>(data->life * 255.0f * (1.0f / LIFE) * data->colour[3]); // Only in OpenGL

		return 0;
	}
};


void Ic::DustParticles(int number, Vector3 position, Vector3 force, float gravity, Vector4 colour, float randomness)
{
	Dust p;

	p.position = position;
	p.gravity = gravity;
	p.colour = colour;

	for (int n = 0; n < number; n += 1)
	{
		const float r1 = (RandomFloat(&s_rng) * 2.0f - 1.0f) * 64.0f * randomness;
		const float r2 = (RandomFloat(&s_rng) * 2.0f - 1.0f) * 64.0f * randomness;
		const float r3 = (RandomFloat(&s_rng) * 2.0f - 1.0f) * 64.0f * randomness;

		p.force = {force.x + r1, force.y + r2, force.z + r3};

		CreateTempEntity(TempEntityType::Particle, Dust::CreateCallback, Dust::UpdateCallback, sizeof(Dust), &p);
	}
}


struct Shell
{
	static constexpr float LIFE = 3.0f; // Seconds

	Ic::Vector3 position;
	Ic::Vector3 angle;
	Ic::Vector3 force;
	Ic::Vector3 a_force;
	int type;
	float life;

	static void CreateCallback(float dt, void* user_data, cl_entity_t* entity)
	{
		Shell* data = reinterpret_cast<Shell*>(user_data);

		entity->origin[0] = data->position.x;
		entity->origin[1] = data->position.y;
		entity->origin[2] = data->position.z;

		entity->curstate.angles[0] = data->angle[0];
		entity->curstate.angles[1] = data->angle[1];
		entity->curstate.angles[2] = data->angle[2];

		data->life = Shell::LIFE;

		int temp;
		if (data->type == 1)
			entity->model = gEngfuncs.CL_LoadModel("models/shotgunshell.mdl", &temp);
		else
			entity->model = gEngfuncs.CL_LoadModel("models/shell.mdl", &temp);
	}

	static int UpdateCallback(float dt, void* user_data, cl_entity_t* entity)
	{
		Shell* data = reinterpret_cast<Shell*>(user_data);

		data->life -= dt;
		if (data->life <= 0.0f)
			return 1;

		const float friction = expf(-2.0f * dt);

		data->position = Add(data->position, Scale(data->force, dt));
		data->force = Scale(data->force, friction);
		data->force.z -= 512.0f * dt;

		data->angle = Add(data->angle, Scale(data->a_force, dt));

		entity->origin[0] = data->position.x;
		entity->origin[1] = data->position.y;
		entity->origin[2] = data->position.z;

		entity->curstate.angles[0] = data->angle[0];
		entity->curstate.angles[1] = data->angle[1];
		entity->curstate.angles[2] = data->angle[2];

		return 0;
	}
};


void Ic::ShellParticle(int type, Vector3 position, Vector3 force, Vector3 angle)
{
	Shell p;

	p.position = position;
	p.angle = angle;
	p.force = force;

	const float r1 = (RandomFloat(&s_rng) * 2.0f - 1.0f) * 128.0f;
	const float r2 = (RandomFloat(&s_rng) * 2.0f - 1.0f) * 128.0f;
	const float r3 = (RandomFloat(&s_rng) * 2.0f - 1.0f) * 128.0f;

	p.a_force = {r1, r2, r3};
	p.type = type;

	CreateTempEntity(TempEntityType::Particle, Shell::CreateCallback, Shell::UpdateCallback, sizeof(Shell), &p);
}
