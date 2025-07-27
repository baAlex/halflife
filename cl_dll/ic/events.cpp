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

#include "event_api.h"
#include "event_args.h"
#include "eventscripts.h"
#include "r_studioint.h"

#include "pm_defs.h"
#include "pmtrace.h"
#include "dlight.h"

#include "events.hpp"
#include "ic/weapons.hpp"
#include "ic/base.hpp"
#include "ic/messages.hpp"
#include "ic/material.hpp"
#include "ic/particles.hpp"
#include "ic/fog.hpp"
#include "ic/view.hpp"

#include <string.h>
#include <stdint.h>


extern engine_studio_api_t IEngineStudio; // Global a la' Valve
extern float g_muzzle_flash;              // Ditto
extern float g_muzzle_angle;              // Ditto


static void sImpactParticles(const Ic::Material* mat, Ic::Vector3 view_position, Ic::Vector3 position,
                             Ic::Vector3 force, float light_at_impact, int pellets_no)
{
	int number = mat->impact_particles_number / Ic::Min(pellets_no, mat->impact_particles_number);

	const Ic::Vector4 colour =
	    Ic::Multiply(mat->impact_colour, {light_at_impact, light_at_impact, light_at_impact, 1.0f});

	force = Ic::Scale(force, mat->impact_particles_force);

	if (IEngineStudio.IsHardware() != 0)
	{
		Ic::Vector4 fog_colour;
		float fog_mix;

		Ic::SoftwareFog(view_position, position, &fog_colour, &fog_mix);
		fog_colour[3] = colour[3];

		fog_colour = Ic::Mix(fog_colour, colour, fog_mix);
		Ic::DustParticles(number, position, force, mat->impact_particles_gravity, fog_colour);
	}
	else
	{
		if (number > 1)
			number >>= 1;
		Ic::DustParticles(number, position, force, mat->impact_particles_gravity, colour);
	}
}


static void sImpactSound(const Ic::Material* mat, float* position, int pellets_no)
{
	const int rand1 = gEngfuncs.pfnRandomLong(0, Ic::Material::VARIATIONS_NO - 1);
	const int rand2 = gEngfuncs.pfnRandomLong(0, Ic::Material::VARIATIONS_NO - 1);

	gEngfuncs.pEventAPI->EV_PlaySound(0, position, CHAN_STATIC, (char*)(mat->impact_sounds[rand1]),
	                                  gEngfuncs.pfnRandomFloat(0.8f, 0.9f) /
	                                      sqrtf(static_cast<float>(pellets_no)), // Something to compensate being linear
	                                  ATTN_NORM, 0, 98 + rand2);
}


static void sImpactDecal(const Ic::Material* mat, float* position, int impacted_model)
{
	const int rand = gEngfuncs.pfnRandomLong(0, Ic::Material::VARIATIONS_NO - 1);

	if (mat->decals != nullptr && gEngfuncs.pfnGetCvarFloat("r_decals") > 0.0f)
	{
		gEngfuncs.pEfxAPI->R_DecalShoot(
		    gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)(mat->decals[rand]))),
		    impacted_model, 0, position, 0);
	}
}


static void sEntityImpact(float* impact_normal, const float* start, const float* trace_end, float light_at_impact,
                          int pellets_no)
{
	float* end_pos = (float*)trace_end; // Disgusting const* casts, Valve please fix

	const Ic::Material* mat = Ic::GetMaterial(Ic::Material::Type::Flesh);

	sImpactParticles(mat, {start[0], start[1], start[2]}, {end_pos[0], end_pos[1], end_pos[2]},
	                 {impact_normal[0], impact_normal[1], impact_normal[2]}, light_at_impact, pellets_no);

	sImpactSound(mat, end_pos, pellets_no);
}


static void sWorldImpact(float* impact_normal, int impacted_model, const float* start, const float* trace_end,
                         const char* texture_name, float light_at_impact, int pellets_no)
{
	float* end_pos = (float*)trace_end; // Disgusting const* casts, Valve please fix

	if (texture_name == nullptr || strcmp(texture_name, "sky") == 0)
		return;

	bool do_decals;
	const Ic::Material* mat = Ic::GetMaterial(texture_name, &do_decals);

	sImpactParticles(mat, {start[0], start[1], start[2]}, {end_pos[0], end_pos[1], end_pos[2]},
	                 {impact_normal[0], impact_normal[1], impact_normal[2]}, light_at_impact, pellets_no);

	sImpactSound(mat, end_pos, pellets_no);
	if (do_decals == true)
		sImpactDecal(mat, end_pos, impacted_model);
}


Ic::Vector3 sClientSideOrigin(int entity, const float* server_origin, bool server_crouch)
{
	// Events work in first and third-person, in first person tho, we need more precision
	// regarding height. And sadly that is only achievable from client side point of view
	Ic::Vector3 ret = {};

	// Are we client's player?
	if (gEngfuncs.pEventAPI->EV_IsLocal(entity - 1) != 0) // Valve uses that mysterious '-1'
	{
		gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(reinterpret_cast<float*>(&ret.x));
	}
	else
	{
		// Nope, calculate height as server does
		ret.z += static_cast<float>((server_crouch == false) ? DEFAULT_VIEWHEIGHT : VEC_DUCK_VIEW);
	}

	ret.x += server_origin[0];
	ret.y += server_origin[1];
	ret.z += server_origin[2];
	return ret;
}


static void sPellet(Ic::Vector3 client_side_start, Ic::Vector3 start, Ic::Vector3 end, Ic::Vector3 up,
                    Ic::Vector3 right, float light_at_impact, int pellets_no, float* muzzle_origin)
{
	pmtrace_t tr; // Omg, so Quake-ish!
	float temp1[3];
	float temp2[3];
	float dev_colour[3];

	float* s = reinterpret_cast<float*>(&start.x);
	float* e = reinterpret_cast<float*>(&end.x);

	// Trace ray
	gEngfuncs.pEventAPI->EV_PlayerTrace(s, e, PM_NORMAL, -1, &tr);

	// Impact effects
	if (gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent)->studiomodel == nullptr)
	{
		// We impacted the world

		const physent_s* entity = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
		if (entity != nullptr && (entity->solid == SOLID_BSP || entity->movetype == MOVETYPE_PUSHSTEP))
		{
			// EV_TraceTexture() is quite imprecise, so rather than let it cast from eyes all
			// the way to target, we trace from points near to the already traced surface
			temp1[0] = tr.endpos[0] + tr.plane.normal[0] * 2.0f;
			temp1[1] = tr.endpos[1] + tr.plane.normal[1] * 2.0f;
			temp1[2] = tr.endpos[2] + tr.plane.normal[2] * 2.0f;
			end[0] = tr.endpos[0] - tr.plane.normal[0] * 2.0f;
			end[1] = tr.endpos[1] - tr.plane.normal[1] * 2.0f;
			end[2] = tr.endpos[2] - tr.plane.normal[2] * 2.0f;

			const char* texture_name = gEngfuncs.pEventAPI->EV_TraceTexture(tr.ent, temp1, e);
			// gEngfuncs.Con_Printf("#### '%s'\n", texture_name);

			sWorldImpact(tr.plane.normal, gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), s, tr.endpos, texture_name,
			             light_at_impact, pellets_no);

			dev_colour[0] = 0.0f;
			dev_colour[1] = 0.0f;
			dev_colour[2] = 0.5f;
		}
	}
	else
	{
		// We impacted an entity

		sEntityImpact(tr.plane.normal, s, tr.endpos, light_at_impact, pellets_no);

		dev_colour[0] = 0.5f;
		dev_colour[1] = 0.0f;
		dev_colour[2] = 0.0f;
	}

	// Render tracer
	// Using 'client_side_start', as is purely for aesthetics reasons
	if (1)
	{
		temp1[0] = client_side_start.x - up.x * 2.5f + right.x * 2.5f;
		temp1[1] = client_side_start.y - up.y * 2.5f + right.y * 2.5f;
		temp1[2] = client_side_start.z - up.z * 2.5f + right.z * 2.5f;
	}
	else
	{
		// With the gun swaying around, it looks goofy; also it doesn´t work
		// in third person, nor in any other client aside local player
		temp1[0] = muzzle_origin[0];
		temp1[1] = muzzle_origin[1];
		temp1[2] = muzzle_origin[2];
	}

	temp2[0] = temp1[0]; // R_TracerEffect() modifies input, ewww...
	temp2[1] = temp1[1]; // (TODO, confirm it better)
	temp2[2] = temp1[2];

	gEngfuncs.pEfxAPI->R_TracerEffect(temp1, tr.endpos);

	// Render line (for debuging purposes)
	if (Ic::GetDeveloperLevel() > 1)
	{
		const int dev_sprite = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/smoke.spr");

		gEngfuncs.pEfxAPI->R_BeamPoints(temp2, tr.endpos, dev_sprite, //
		                                4.0f,                         // Life
		                                0.5f,                         // Width
		                                0.0f,                         // Amplitude
		                                1.0f,                         // Brightness
		                                0,                            // Speed
		                                0,                            // Start frame
		                                0,                            // Frame rate
		                                dev_colour[0], dev_colour[1], dev_colour[2]);
	}
}


template <typename W>
static void sGenericEvent(int entity, float* origin, float* angles, float* muzzle_origin, float* eject_origin,
                          bool crouch, float accuracy, int rounds_no, int seed, float light_at_impact,
                          float* player_velocity)
{
	// Developers, developers, developers
	// To test behaviour noted in [a]
	if (0)
	{
		cl_entity_t* view_model = gEngfuncs.GetViewModel();
		gEngfuncs.Con_Printf("[%.2f, %.2f, %.2f] / [%.2f, %.2f, %.2f]\n", angles[0], angles[1], angles[2],
		                     view_model->angles[0], view_model->angles[1], view_model->angles[2]);
	}

	// Calculate origin
	Ic::Vector3 client_side_origin = sClientSideOrigin(entity, origin, crouch);
	origin[2] += static_cast<float>((crouch == false) ? DEFAULT_VIEWHEIGHT
	                                                  : VEC_DUCK_VIEW); // Notice that is done after sClientSideOrigin()

	// Fire sound
	gEngfuncs.pEventAPI->EV_PlaySound(entity, origin, CHAN_WEAPON, W::PROPS.fire_sound,
	                                  gEngfuncs.pfnRandomFloat(0.92f, 1.0f), ATTN_NORM, 0,
	                                  98 + gEngfuncs.pfnRandomLong(0, 3));

	// First person event?
	if (gEngfuncs.pEventAPI->EV_IsLocal(entity - 1) != 0) // Valve uses that mysterious '-1'
	{
		uint16_t s = static_cast<uint16_t>(seed); // Copy it!, we don't want to alter the seed
		                                          // received from server. As it's intended for
		                                          // Ic::WeaponFire() consumption

		g_muzzle_angle = Ic::RandomFloat(&s) * 255.0f;
		g_muzzle_flash = 0.2f + Ic::RandomFloat(&s) * 0.8f;

		// Fire light
		dlight_t* dl = gEngfuncs.pEfxAPI->CL_AllocDlight(0); // Elights are too bright

		dl->origin[0] = muzzle_origin[0];
		dl->origin[1] = muzzle_origin[1];
		dl->origin[2] = muzzle_origin[2];

		dl->color.r = W::PROPS.fire_colour[0];
		dl->color.g = W::PROPS.fire_colour[1];
		dl->color.b = W::PROPS.fire_colour[2];
		dl->radius = W::PROPS.fire_colour[3];

		dl->die = gEngfuncs.GetClientTime() + 0.01f;

		// Eject shell
		if (W::PROPS.fire_eject != nullptr)
		{
			cl_entity_t* player = gEngfuncs.GetLocalPlayer();
			cl_entity_t* view_model = gEngfuncs.GetViewModel();

			Ic::Vector3 forward;
			Ic::Vector3 right;
			Ic::Vector3 up;
			Ic::ProperAngleVectors(Ic::Vector3::FromPtr(view_model->angles), &forward, &right, &up);

			auto f = -Ic::RandomFloat(&s) * 32.0f;
			auto r = Ic::RandomFloat(&s) * 64.0f + 64.0f;
			auto u = Ic::RandomFloat(&s) * 32.0f;

			right[0] = forward[0] * f + right[0] * r + up[0] * u + player_velocity[0];
			right[1] = forward[1] * f + right[1] * r + up[1] * u + player_velocity[1];
			right[2] = forward[2] * f + right[2] * r + up[2] * u + player_velocity[2];

			auto shell = gEngfuncs.pEventAPI->EV_FindModelIndex(W::PROPS.fire_eject);
			gEngfuncs.pEfxAPI->R_TempModel(eject_origin, &right.x, view_model->angles, 2.5f, shell, TE_BOUNCE_SHELL);
		}
	}

	// Set global state thingies
	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetSolidPlayers(entity - 1);
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	// Per pellets things
	Ic::WeaponFire(
	    &W::PROPS, {origin[0], origin[1], origin[2]}, {angles[0], angles[1], angles[2]}, static_cast<uint16_t>(seed),
	    rounds_no, accuracy, [=](Ic::Vector3 start, Ic::Vector3 end, Ic::Vector3 up, Ic::Vector3 right) //
	    {                                                                                               //
		    sPellet(client_side_origin, start, end, up, right, light_at_impact, W::PROPS.pellets_no, muzzle_origin);
	    });

	// Restore global state thingies
	gEngfuncs.pEventAPI->EV_PopPMStates();
}


// ============================


struct Queue
{
	static constexpr int MAX_DELAYED_ITEMS = 16;

	event_args_s events[MAX_DELAYED_ITEMS];
	int cursor;
};

static Queue s_pistol_queue;
static Queue s_shotgun_queue;
static Queue s_smg_queue;
static Queue s_ar_queue;
static Queue s_rifle_queue;

/*
    Protip: I'm delaying events because they need updated variables
    like player and weapon positions, and, events seems to be received
    early in the frame while player and weapon thingies are done later.
    So, the delay is in order to process events after all previous enchilada.

    Oh!, and doing it later also means that I get some goodies like player
    velocity.
*/

template <typename W> static void sDoThingie(Queue* q, event_args_s* args)
{
	if (q->cursor == Queue::MAX_DELAYED_ITEMS)
		return;
	q->events[q->cursor] = *args;
	q->cursor += 1;

	// First person event?,
	// we need to tell the view module to animate this weapon
	if (gEngfuncs.pEventAPI->EV_IsLocal(args->entindex - 1) != 0) // Valve uses that mysterious '-1'
	{
		// TODO, temporary constant values suitable for the Smg model
		float fire_angle_min = 16.0f * W::PROPS.fire_kick;
		float angle_max = 32.0f * W::PROPS.fire_kick;
		float position = 12.0f * W::PROPS.fire_kick;
		Ic::ViewFire(fire_angle_min, angle_max, position);
	}
}

void IcEventWeapon1(event_args_s* args)
{
	sDoThingie<Ic::PistolWeapon>(&s_pistol_queue, args);
}

void IcEventWeapon2(event_args_s* args)
{
	sDoThingie<Ic::ShotgunWeapon>(&s_shotgun_queue, args);
}

void IcEventWeapon3(event_args_s* args)
{
	sDoThingie<Ic::SmgWeapon>(&s_smg_queue, args);
}

void IcEventWeapon4(event_args_s* args)
{
	sDoThingie<Ic::ArWeapon>(&s_ar_queue, args);
}

void IcEventWeapon5(event_args_s* args)
{
	sDoThingie<Ic::RifleWeapon>(&s_rifle_queue, args);
}

void Ic::HookEvents()
{
	// (baAlex)
	// (disgusting char* casts)
	gEngfuncs.pfnHookEvent((char*)(Ic::PistolWeapon::PROPS.event_fire), IcEventWeapon1);
	gEngfuncs.pfnHookEvent((char*)(Ic::ShotgunWeapon::PROPS.event_fire), IcEventWeapon2);
	gEngfuncs.pfnHookEvent((char*)(Ic::SmgWeapon::PROPS.event_fire), IcEventWeapon3);
	gEngfuncs.pfnHookEvent((char*)(Ic::ArWeapon::PROPS.event_fire), IcEventWeapon4);
	gEngfuncs.pfnHookEvent((char*)(Ic::RifleWeapon::PROPS.event_fire), IcEventWeapon5);
}


void Ic::ProcessEvents(float* player_velocity)
{
	// Regarding note [a], is to fix a Quake inherited bug where pitch is negated.
	// Is not fixed in other instances in this file since it seems that Valve
	// negated it before transmitting angles from server. Problem here is that
	// 'view_model->angles' is a non-transmitted, client-predicted thing.

	// Retrieve client muzzle and eject position
	float muzzle_position[3] = {};
	float eject_position[3] = {};

	cl_entity_t* view_model = gEngfuncs.GetViewModel();
	if (view_model != nullptr)
	{
		float end[3];

		// Delayed
		if (Ic::GetDeveloperLevel() > 1)
		{
			muzzle_position[0] = view_model->attachment[0][0];
			muzzle_position[1] = view_model->attachment[0][1];
			muzzle_position[2] = view_model->attachment[0][2];

			Ic::Vector3 forward;
			Ic::BrokenAngleVectors(Ic::Vector3::FromPtr(view_model->angles), &forward, nullptr, nullptr); // [a]

			end[0] = muzzle_position[0] + forward[0] * 10.0f;
			end[1] = muzzle_position[1] + forward[1] * 10.0f;
			end[2] = muzzle_position[2] + forward[2] * 10.0f;

			gEngfuncs.pEfxAPI->R_ParticleLine(muzzle_position, end, 0, 255, 0, 0.1f);
		}

		// Better
		// What happens is that 'view_model->attachment' is delayed by one frame, while 'view_model->origin' is not.
		// Luckily attachments between them are delayed by same amount, so if one of them is positioned in model base,
		// by subtracting them we get a local space origin, suitable to then add with 'view_model->origin'
		muzzle_position[0] = view_model->origin[0] + (view_model->attachment[0][0] - view_model->attachment[2][0]);
		muzzle_position[1] = view_model->origin[1] + (view_model->attachment[0][1] - view_model->attachment[2][1]);
		muzzle_position[2] = view_model->origin[2] + (view_model->attachment[0][2] - view_model->attachment[2][2]);

		if (Ic::GetDeveloperLevel() > 1)
		{
			Ic::Vector3 up;
			Ic::BrokenAngleVectors(Ic::Vector3::FromPtr(view_model->angles), nullptr, nullptr, &up); // [a]

			end[0] = muzzle_position[0] + up[0] * 10.0f;
			end[1] = muzzle_position[1] + up[1] * 10.0f;
			end[2] = muzzle_position[2] + up[2] * 10.0f;

			gEngfuncs.pEfxAPI->R_ParticleLine(muzzle_position, end, 255, 0, 0, 0.1f);
		}

		eject_position[0] = view_model->origin[0] + (view_model->attachment[1][0] - view_model->attachment[2][0]);
		eject_position[1] = view_model->origin[1] + (view_model->attachment[1][1] - view_model->attachment[2][1]);
		eject_position[2] = view_model->origin[2] + (view_model->attachment[1][2] - view_model->attachment[2][2]);

		if (Ic::GetDeveloperLevel() > 1)
		{
			Ic::Vector3 right;
			Ic::BrokenAngleVectors(Ic::Vector3::FromPtr(view_model->angles), nullptr, &right, nullptr); // [a]

			end[0] = eject_position[0] + right[0] * 8.0f;
			end[1] = eject_position[1] + right[1] * 8.0f;
			end[2] = eject_position[2] + right[2] * 8.0f;

			gEngfuncs.pEfxAPI->R_ParticleLine(eject_position, end, 0, 0, 255, 0.1f);
		}
	}

	// Process all events
	for (; s_pistol_queue.cursor != 0; s_pistol_queue.cursor -= 1)
	{
		event_args_s* args = s_pistol_queue.events + s_pistol_queue.cursor - 1;
		sGenericEvent<Ic::PistolWeapon>(args->entindex, args->origin, args->angles, muzzle_position, eject_position,
		                                args->ducking, args->fparam1, args->iparam1, args->iparam2,
		                                args->fparam2 / 255.0f, player_velocity);
	}

	for (; s_shotgun_queue.cursor != 0; s_shotgun_queue.cursor -= 1)
	{
		event_args_s* args = s_shotgun_queue.events + s_shotgun_queue.cursor - 1;
		sGenericEvent<Ic::ShotgunWeapon>(args->entindex, args->origin, args->angles, muzzle_position, eject_position,
		                                 args->ducking, args->fparam1, args->iparam1, args->iparam2,
		                                 args->fparam2 / 255.0f, player_velocity);
	}

	for (; s_smg_queue.cursor != 0; s_smg_queue.cursor -= 1)
	{
		event_args_s* args = s_smg_queue.events + s_smg_queue.cursor - 1;
		sGenericEvent<Ic::SmgWeapon>(args->entindex, args->origin, args->angles, muzzle_position, eject_position,
		                             args->ducking, args->fparam1, args->iparam1, args->iparam2, args->fparam2 / 255.0f,
		                             player_velocity);
	}

	for (; s_ar_queue.cursor != 0; s_ar_queue.cursor -= 1)
	{
		event_args_s* args = s_ar_queue.events + s_ar_queue.cursor - 1;
		sGenericEvent<Ic::ArWeapon>(args->entindex, args->origin, args->angles, muzzle_position, eject_position,
		                            args->ducking, args->fparam1, args->iparam1, args->iparam2, args->fparam2 / 255.0f,
		                            player_velocity);
	}

	for (; s_rifle_queue.cursor != 0; s_rifle_queue.cursor -= 1)
	{
		event_args_s* args = s_rifle_queue.events + s_rifle_queue.cursor - 1;
		sGenericEvent<Ic::RifleWeapon>(args->entindex, args->origin, args->angles, muzzle_position, eject_position,
		                               args->ducking, args->fparam1, args->iparam1, args->iparam2,
		                               args->fparam2 / 255.0f, player_velocity);
	}
}
