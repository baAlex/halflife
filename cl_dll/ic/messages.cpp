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

#include "messages.hpp"
#include "ic/base.hpp"
#include "ic/weapons.hpp"

#include <string.h>


static int s_health;

static Ic::WeaponState s_weapon_state;
static const Ic::WeaponProperties* s_weapon_props;
static const Ic::ClosedBoltBehaviour::Properties* s_weapon_behaviour_props;

static float s_accuracy[2];
static float s_speed;
static float s_max_speed;
static float s_angle;

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

void Ic::MessagesSetAngle(float a)
{
	s_angle = a;
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

float Ic::GetAngle()
{
	return s_angle;
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


static char s_map_name[256];
static Ic::WorldProperties s_world_p = {};

const Ic::WorldProperties* Ic::GetWorldProperties()
{
	WorldProperties new_p = {};

	if (strcmp(s_map_name, gEngfuncs.pfnGetLevelName()) == 0)
		return &s_world_p;

	strcpy(s_map_name, gEngfuncs.pfnGetLevelName());
	memset(&s_world_p, 0, sizeof(WorldProperties));

	// ----
	// Mostly a copy of UTIL_FindEntityInMap(), 'cl_dll/hud_spectator.cpp'

	// Btw, makes more sense to parse world at any of the Initialise() functions,
	// however those are called before the level is actually loaded.

	bool found = false;
	char keyname[256];
	char token[1024];

	cl_entity_t* world_entity = gEngfuncs.GetEntityByIndex(0);
	if (world_entity == nullptr)
		return &s_world_p;

	for (char* data = world_entity->model->entities; data != nullptr;)
	{
		data = gEngfuncs.COM_ParseFile(data, token);

		if ((token[0] == '}') || (token[0] == 0))
			break;

		if (data == nullptr)
		{
			gEngfuncs.Con_Printf("Ic::GetWorldProperties(), EOF without closing brace\n");
			return &s_world_p;
		}

		if (token[0] != '{')
		{
			gEngfuncs.Con_Printf("Ic::GetWorldProperties(), expected {\n");
			return &s_world_p;
		}

		// Now parse entities properties
		while (1)
		{
			// Key
			data = gEngfuncs.COM_ParseFile(data, token);

			if (token[0] == '}')
				break; // Finish parsing this entity

			if (data == nullptr)
			{
				gEngfuncs.Con_Printf("Ic::GetWorldProperties(), EOF without closing brace\n");
				return &s_world_p;
			}

			strcpy(keyname, token);

			// Hack to fix keynames with trailing spaces
			size_t n = strlen(keyname);
			while (n != 0 && keyname[n - 1] == ' ')
			{
				keyname[n - 1] = 0;
				n--;
			}

			// Parse values
			data = gEngfuncs.COM_ParseFile(data, token);

			if (data == nullptr)
			{
				gEngfuncs.Con_Printf("Ic::GetWorldProperties(), EOF without closing brace\n");
				return &s_world_p;
			}

			if (token[0] == '}')
			{
				gEngfuncs.Con_Printf("Ic::GetWorldProperties(), closing brace without data");
				return &s_world_p;
			}

			if (strcmp(keyname, "classname") == 0)
			{
				if (strcmp(token, "worldspawn") == 0)
				{
					found = true; // That's our entity
				}
			}

			if (strcmp(keyname, "fog_colour1") == 0)
			{
				int temp[3] = {};
				sscanf(token, "%i %i %i", &temp[0], &temp[1], &temp[2]);

				new_p.fog_colour1[0] = static_cast<float>(temp[0]) / 255.0f;
				new_p.fog_colour1[1] = static_cast<float>(temp[1]) / 255.0f;
				new_p.fog_colour1[2] = static_cast<float>(temp[2]) / 255.0f;

				gEngfuncs.Con_Printf("### fog_colour1 = '%s'\n", token);
			}

			if (strcmp(keyname, "fog_colour2") == 0)
			{
				int temp[3] = {};
				sscanf(token, "%i %i %i", &temp[0], &temp[1], &temp[2]);

				new_p.fog_colour2[0] = static_cast<float>(temp[0]) / 255.0f;
				new_p.fog_colour2[1] = static_cast<float>(temp[1]) / 255.0f;
				new_p.fog_colour2[2] = static_cast<float>(temp[2]) / 255.0f;

				gEngfuncs.Con_Printf("### fog_colour2 = '%s'\n", token);
			}

			if (strcmp(keyname, "fog_density") == 0)
			{
				new_p.fog_density = static_cast<float>(atof(token));
				gEngfuncs.Con_Printf("### fog_density = '%s'\n", token);
			}

			if (strcmp(keyname, "fog_angle") == 0)
			{
				new_p.fog_angle = static_cast<float>(atof(token));
				gEngfuncs.Con_Printf("### fog_angle = '%s'\n", token);
			}
		}

		if (found == true)
		{
			s_world_p = new_p;
			return &s_world_p;
		}
	}
}
