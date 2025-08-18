/***
 *
 *	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/

// ORDER OF INCLUDES IS THIS AND NO OTHER
#include "cvardef.h"
#include "wrect.h"
#include "cl_dll.h"
#include "parsemsg.h"
#include "com_model.h"

#include "event_api.h"
#include "pm_defs.h"
#include "pmtrace.h"

#include "messages.hpp"
#include "ic/base.hpp"
#include "ic/weapons.hpp"
#include "ic/temp-entities.hpp"
#include "ic/dev-nodes.hpp"

#include <string.h>


static int s_world_updated = 0;

static int s_health;

static Ic::WeaponState s_weapon_state;
static const Ic::WeaponProperties* s_weapon_props;
static const Ic::ClosedBoltBehaviour::Properties* s_weapon_behaviour_props;

static float s_accuracy[2];
static float s_speed;
static float s_max_speed;
static Ic::Vector3 s_forward;
static Ic::Vector3 s_position;
static Ic::Vector3 s_look_end;

static int s_developer_level;


static int sHealthChanges(const char* name, int size, void* pbuf)
{
	BEGIN_READ(pbuf, size);
	s_health = READ_BYTE();

	// gEngfuncs.Con_Printf("Health changes: %i\n", s_health);
	return 1;
}

static int sDamageReceive(const char* name, int size, void* pbuf)
{
	BEGIN_READ(pbuf, size);

	int armor = READ_BYTE();
	int damage = READ_BYTE();
	long damage_bits = READ_LONG();

	Vector vecFrom;
	for (int i = 0; i < 3; i++)
		vecFrom[i] = READ_COORD();

	// gEngfuncs.Con_Printf("Damage receive, armor: %i, damage: %i\n", armor, damage);
	return 1;
}


static int sWeaponState(const char* name, int size, void* pbuf)
{
	BEGIN_READ(pbuf, size);
	s_weapon_state = Ic::WeaponState::DecodeNetWord(READ_LONG());

	Ic::RetrieveWeaponProps(s_weapon_state.id, &s_weapon_props, &s_weapon_behaviour_props);

	// gEngfuncs.Con_Printf("Weapon state changes: %i, %i, %i\n", s_weapon_state.mode, s_weapon_state.chamber,
	//                      s_weapon_state.magazine);
	return 1;
}


void Ic::MessagesInitialise()
{
	gEngfuncs.pfnHookUserMsg("Health", sHealthChanges);
	gEngfuncs.pfnHookUserMsg("Damage", sDamageReceive);
	gEngfuncs.pfnHookUserMsg("WeaponState", sWeaponState);

	{
		const auto dev_cvar = gEngfuncs.pfnGetCvarPointer("developer");
		gEngfuncs.pfnAddCommand("dev_dashboard", []() { s_developer_level = (s_developer_level + 1) % 3; });
		s_developer_level = (dev_cvar != nullptr) ? static_cast<int>(dev_cvar->value) : 0;
	}

	// TODO: there are a lot of messages currently sent by player
	// that are not received anywhere, they remain in 'ammo.cpp'
	// which is now disabled. Of course I need replacement for
	// them, or remove them entirely, but doing that require
	// knowledge on what they do and when. And yeah...

	MessagesSoftInitialise();
}

void Ic::MessagesSoftInitialise()
{
	s_world_updated = 0; // TODO, FIXME maybe, does this work at levels transition?

	s_health = 0.0f;
	s_accuracy[0] = 0.0f;
	s_accuracy[1] = 0.0f;
}

void Ic::MessagesSetAccuracy(Side side, float a)
{
	s_accuracy[static_cast<int>(side)] = a;
}

void Ic::MessagesSetSpeed(float s, float max_speed)
{
	s_speed = s;
	s_max_speed = max_speed;
}

void Ic::MessagesSetForward(Vector3 v)
{
	s_forward = v; // TODO, maybe, some day, this needs to be normalized
}

void Ic::MessagesSetPosition(Vector3 v)
{
	s_position = v;

	{
		pmtrace_t tr;

		Ic::Vector3 start = s_position;
		Ic::Vector3 end = Add(s_position, Scale(s_forward, 8192.0f));

		gEngfuncs.pEventAPI->EV_PushPMStates();
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace((float*)(&start.x), (float*)(&end.x), PM_NORMAL, -1, &tr);
		gEngfuncs.pEventAPI->EV_PopPMStates();

		// gEngfuncs.pEfxAPI->R_ParticleLine((float*)(&start.x), tr.endpos, 0, 0, 255, 0.1f);

		s_look_end = Ic::Vector3::FromPtr(tr.endpos);
	}
}


bool Ic::GetIfDead()
{
	return (s_health <= 0) ? true : false;
}

int Ic::GetHealth()
{
	return s_health;
}

float Ic::GetAccuracy(Side side)
{
	return s_accuracy[static_cast<int>(side)];
}

float Ic::GetSpeed()
{
	return s_speed;
}

Ic::Vector3 Ic::GetForward()
{
	return s_forward;
}

Ic::Vector3 Ic::GetPosition()
{
	return s_position;
}

Ic::Vector3 Ic::GetLookEnd()
{
	return s_look_end;
}

const char* Ic::GetWeaponMode()
{
	return Ic::ToString(s_weapon_state.mode);
}

int Ic::GetChamberAmmo()
{
	return s_weapon_state.chamber;
}

int Ic::GetMagazineAmmo()
{
	return s_weapon_state.magazine;
}

const char* Ic::GetWeaponName()
{
	if (s_weapon_props == nullptr)
		return "Unknown";

	return s_weapon_props->short_name;
}

int Ic::GetDeveloperLevel()
{
	return s_developer_level;
}


static Ic::WorldProperties s_world_p = {};

const Ic::WorldProperties* Ic::GetWorldProperties()
{
	return &s_world_p;
}


template <typename C1, typename C2>
static void sParsieMcParserFace(char* data, C1 entity_field_callback, C2 entity_end_callback)
{
	// Mostly a copy of UTIL_FindEntityInMap(), 'cl_dll/hud_spectator.cpp'

	char token[1024];  // Length number courtesy of Valve
	char keyname[256]; // Ditto

	for (; data != nullptr;)
	{
		data = gEngfuncs.COM_ParseFile(data, token);

		if ((token[0] == '}') || (token[0] == 0))
			break;

		if (data == nullptr)
		{
			gEngfuncs.Con_Printf("Ic::sParsieMcParserFace(), EOF without closing brace\n");
			return;
		}

		if (token[0] != '{')
		{
			gEngfuncs.Con_Printf("Ic::sParsieMcParserFace(), expected {\n");
			return;
		}

		// Now parse entities properties
		while (1)
		{
			// Key
			data = gEngfuncs.COM_ParseFile(data, token);

			if (data == nullptr)
			{
				gEngfuncs.Con_Printf("Ic::sParsieMcParserFace(), EOF without closing brace\n");
				return;
			}

			if (token[0] == '}')
			{
				entity_end_callback();
				break; // Finish parsing this entity
			}

			strcpy(keyname, token);

			// Hack to fix keynames with trailing spaces
			size_t n = strlen(keyname);
			while (n != 0 && keyname[n - 1] == ' ')
			{
				keyname[n - 1] = 0;
				n--;
			}

			// Parse value
			data = gEngfuncs.COM_ParseFile(data, token);

			if (data == nullptr)
			{
				gEngfuncs.Con_Printf("Ic::sParsieMcParserFace(), EOF without closing brace\n");
				return;
			}

			if (token[0] == '}')
			{
				gEngfuncs.Con_Printf("Ic::sParsieMcParserFace(), closing brace without data");
				return;
			}

			// Tell outside code about
			entity_field_callback(keyname, token);
		}
	}
}


static Ic::Vector3 sStringToVector3(const char* string)
{
	int temp[3];
	sscanf(string, "%i %i %i", &temp[0], &temp[1], &temp[2]);
	return {static_cast<float>(temp[0]), static_cast<float>(temp[1]), static_cast<float>(temp[2])};
}

const void Ic::ParseWorldProperties()
{
	if (s_world_updated == 1)
		return;
	s_world_updated = 1;

	gEngfuncs.Con_Printf("### Ic::ParseWorldProperties()\n");

	// ----
	// Btw, makes more sense to parse world at any of the Initialise() functions,
	// however those are called before the level is actually loaded.

	bool found = false;
	char keyname[256];
	char token[1024];

	WorldProperties new_p = {};

	cl_entity_t* world_entity = gEngfuncs.GetEntityByIndex(0);
	if (world_entity == nullptr)
		return;

	// Create dev nodes
	{
		bool found = false;
		Ic::Vector3 position = {};
		char text[Ic::DEV_NODE_MAX_TEXT_LENGTH] = {};

		sParsieMcParserFace(
		    world_entity->model->entities,
		    [&](const char* key, const char* value)
		    {
			    // gEngfuncs.Con_Printf("### Ic::EntityField: '%s' : '%s'\n", key, value);

			    if (strcmp(key, "classname") == 0 && strcmp(value, "info_dev_node") == 0)
				    found = true;
			    if (strcmp(key, "text") == 0)
				    strncpy(text, value, Ic::DEV_NODE_MAX_TEXT_LENGTH);
			    if (strcmp(key, "origin") == 0)
				    position = sStringToVector3(value);
		    },
		    [&]()
		    {
			    // gEngfuncs.Con_Printf("### Ic::EntityEnds\n");

			    if (found == true)
			    {
				    Ic::CreateDevNode(position, text);

				    position = {0};
				    text[0] = 0;
			    }

			    found = false;
		    });
	}

	// Retrieve fog values
	{
		bool found = false;
		Ic::Vector3 colour1 = {};
		Ic::Vector3 colour2 = {};
		float density = {};
		float angle = {};

		sParsieMcParserFace(
		    world_entity->model->entities,
		    [&](const char* key, const char* value)
		    {
			    if (strcmp(key, "classname") == 0 && strcmp(value, "worldspawn") == 0)
				    found = true;
			    if (strcmp(key, "fog_colour1") == 0)
				    colour1 = sStringToVector3(value);
			    if (strcmp(key, "fog_colour2") == 0)
				    colour2 = sStringToVector3(value);
			    if (strcmp(key, "fog_density") == 0)
				    density = static_cast<float>(atof(value));
			    if (strcmp(key, "fog_angle") == 0)
				    angle = static_cast<float>(atof(value));
		    },
		    [&]()
		    {
			    if (found == true)
			    {
				    gEngfuncs.Con_Printf("### fog_colour1 = %f, %f, %f\n", colour1[0], colour1[1], colour1[2]);
				    gEngfuncs.Con_Printf("### fog_colour2 = %f, %f, %f\n", colour2[0], colour2[1], colour2[2]);
				    gEngfuncs.Con_Printf("### fog_density = %f\n", density);
				    gEngfuncs.Con_Printf("### fog_angle = %f\n", angle);

				    new_p.fog_colour1[0] = static_cast<float>(colour1[0]) / 255.0f;
				    new_p.fog_colour1[1] = static_cast<float>(colour1[1]) / 255.0f;
				    new_p.fog_colour1[2] = static_cast<float>(colour1[2]) / 255.0f;

				    new_p.fog_colour2[0] = static_cast<float>(colour2[0]) / 255.0f;
				    new_p.fog_colour2[1] = static_cast<float>(colour2[1]) / 255.0f;
				    new_p.fog_colour2[2] = static_cast<float>(colour2[2]) / 255.0f;

				    new_p.fog_density = density;
				    new_p.fog_angle = angle;

				    s_world_p = new_p;
			    }

			    found = false;
		    });
	}
}
