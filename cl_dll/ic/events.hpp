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

#ifndef IC_EVENTS_HPP
#define IC_EVENTS_HPP

// Called by the engine, they need to be in C api
extern "C" void IcEventWeapon1(struct event_args_s* args);
extern "C" void IcEventWeapon2(struct event_args_s* args);
extern "C" void IcEventWeapon3(struct event_args_s* args);
extern "C" void IcEventWeapon4(struct event_args_s* args);
extern "C" void IcEventWeapon5(struct event_args_s* args);

namespace Ic
{
void HookEvents();
}

#endif
