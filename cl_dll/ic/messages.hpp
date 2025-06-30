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

#ifndef IC_MESSAGES_HPP
#define IC_MESSAGES_HPP

#include "ic/vector.hpp"

namespace Ic
{

enum class Side
{
	Client = 0,
	Server = 1
};

void MessagesInitialise();
void MessagesSoftInitialise();
void MessagesSetAccuracy(Side, float a);
void MessagesSetSpeed(float s, float max_speed);
void MessagesSetForward(Vector3 f);

bool GetIfDead();
int GetHealth();
float GetAccuracy(Side);
float GetSpeed();
Vector3 GetForward();
const char* GetWeaponMode();
int GetChamberAmmo();
int GetMagazineAmmo();
const char* GetWeaponName();
int GetDeveloperLevel();

struct WorldProperties
{
	Vector3 fog_colour1;
	Vector3 fog_colour2;
	float fog_density;
	float fog_angle;
};

const WorldProperties* GetWorldProperties();
const void ParseWorldProperties();

} // namespace Ic

#endif
