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
#include "wrect.h"
#include "cl_dll.h"
#include "parsemsg.h"

#include "messages.hpp"
#include "ic/base.hpp"


static int s_health;
static float s_accuracy[2];
static float s_speed;
static float s_max_speed;


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


void Ic::MessagesInitialise()
{
	gEngfuncs.pfnHookUserMsg("Health", sHealthChanges);
	gEngfuncs.pfnHookUserMsg("Damage", sDamageReceive);

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
