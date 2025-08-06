/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#include "wrect.h"
#include "cl_dll.h"
#include "com_model.h"

#include "entity_types.h"
#include "event_api.h"

#include <string.h>
#include "ic/vector.hpp"
#include "temp-entities.hpp"


static constexpr size_t USER_DATA_SIZE = 1024;

struct TempEntity
{
	unsigned active;
	Ic::TempEntityType type;
	unsigned life;

	Ic::UpdateCallback update_callback;
	uint8_t user_data[USER_DATA_SIZE];

	cl_entity_t engine_entity;
};

static constexpr int MAX_ENTITIES = 64;
static TempEntity s_entities[MAX_ENTITIES];
static float s_prev_time;


void Ic::InitialiseTempEntities()
{
	s_prev_time = gEngfuncs.GetClientTime();

	for (TempEntity* ent = s_entities; ent < s_entities + MAX_ENTITIES; ent += 1)
		ent->active = 0;
}


static float sUpdateAndGetDelta()
{
	const float time = gEngfuncs.GetClientTime();
	const float dt = time - s_prev_time;
	s_prev_time = time;
	return dt;
}

void Ic::UpdateTempEntities()
{
	const float dt = sUpdateAndGetDelta();

	for (TempEntity* ent = s_entities; ent < s_entities + MAX_ENTITIES; ent += 1)
	{
		if (ent->active != 1)
			continue;

		ent->life += 1;

		if (ent->update_callback(dt, ent->user_data, &ent->engine_entity) != 0)
		{
			ent->active = 0;
			continue;
		}

		ent->engine_entity.index = static_cast<int>(ent - s_entities) + 512; // TODO, document this 512
		gEngfuncs.CL_CreateVisibleEntity(ET_TEMPENTITY, &ent->engine_entity);
	}
}


int Ic::CreateTempEntity(TempEntityType type, CreateCallback create_callback, UpdateCallback update_callback,
                         size_t user_data_size, void* initial_user_data)
{
	const float dt = sUpdateAndGetDelta();

	if (user_data_size > USER_DATA_SIZE)
	{
		gEngfuncs.Con_Printf("Ic::CreateTempEntity() error, data size too large\n");
		return 1;
	}

	// Find an inactive spot, or the oldest one
	TempEntity* ent = s_entities;
	TempEntity* oldest = nullptr;
	for (; ent < s_entities + MAX_ENTITIES; ent += 1)
	{
		if (ent->active != 1)
			break;

		if (ent->type == TempEntityType::Particle && (oldest == nullptr || ent->life > oldest->life))
			oldest = ent;
	}

	if (ent == s_entities + MAX_ENTITIES)
	{
		switch (type)
		{
		case TempEntityType::Normal: return 1;
		case TempEntityType::Particle: ent = oldest; break;
		}

		if (ent == nullptr)
			return 1;
	}

	// Set
	ent->active = 1;
	ent->type = type;
	ent->life = 0;

	ent->update_callback = update_callback;

	if (initial_user_data != nullptr)
		memcpy(ent->user_data, initial_user_data, user_data_size);
	else
		memset(ent->user_data, 0, user_data_size);

	memset(&ent->engine_entity, 0, sizeof(cl_entity_t));

	// Bye!
	create_callback(dt, ent->user_data, &ent->engine_entity);
	return 0;
}
