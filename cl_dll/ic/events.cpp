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

#include "events.hpp"
#include "ic/weapons.hpp"
#include "ic/base.hpp"
#include "ic/messages.hpp"
#include "ic/material.hpp"
#include "ic/particles.hpp"
#include "ic/fog.hpp"

#include <string.h>
#include <stdint.h>


extern engine_studio_api_t IEngineStudio; // Global a la' Valve


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


static float sClientSideOrigin(int entity, const float* server_origin, bool server_crouch, float* out)
{
	// Events work in first and third-person, in first person tho, we need more precision
	// regarding height. And sadly that is only achievable from client side point of view

	out[0] = 0.0f;
	out[1] = 0.0f;
	out[2] = 0.0f;

	// Are we client's player?
	if (gEngfuncs.pEventAPI->EV_IsLocal(entity - 1) != 0) // Valve uses that mysterious '-1'
	{
		gEngfuncs.pEventAPI->EV_LocalPlayerViewheight(out);
	}
	else
	{
		// Nope, calculate height as server does
		out[2] += static_cast<float>((server_crouch == false) ? DEFAULT_VIEWHEIGHT : VEC_DUCK_VIEW);
	}

	out[0] += server_origin[0];
	out[1] += server_origin[1];
	out[2] += server_origin[2];
}


static float sEasing(float x)
{
	return Ic::Mix(x, x * x * x, 0.5f);
	// return x * x * x; // Too much
}


template <typename W>
static void sGenericEvent(int entity, float* origin, float* angles, bool crouch, float accuracy, int rounds_no,
                          int seed, float light_at_impact)
{
	float dev_colour[3];

	pmtrace_t tr; // Omg, so Quake-ish!
	float forward[3];
	float right[3];
	float up[3];
	float end[3];
	float client_side_origin[3];

	uint16_t rng_state = static_cast<uint16_t>(seed); // Using our rng, not Valve one, in order to being in sync
	                                                  // with server (which uses this rng)

	// gEngfuncs.Con_Printf("#### IcEventX(), %f, %i, %u\n", accuracy, rounds_no, seed);

	// Fire sound
	gEngfuncs.pEventAPI->EV_PlaySound(entity, origin, CHAN_WEAPON, W::PROPS.fire_sound,
	                                  gEngfuncs.pfnRandomFloat(0.92f, 1.0f), ATTN_NORM, 0,
	                                  98 + gEngfuncs.pfnRandomLong(0, 3));

	// Set some things before loop
	gEngfuncs.pfnAngleVectors(angles, forward, right, up);

	sClientSideOrigin(entity, origin, crouch, client_side_origin);
	origin[2] += static_cast<float>((crouch == false) ? DEFAULT_VIEWHEIGHT : VEC_DUCK_VIEW);

	gEngfuncs.pEventAPI->EV_PushPMStates();

	gEngfuncs.pEventAPI->EV_SetSolidPlayers(entity - 1); // A global state, configuration thing
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);             // for the tracer logic

	// Iterate rounds
	for (int r = 0; r < rounds_no; r += 1)
	{
		// Calculate round spread
		float round_spread_x;
		float round_spread_y;
		{
			const float angle = Ic::RandomFloat(&rng_state) * static_cast<float>(M_PI) * 2.0f;
			const float length = sEasing(Ic::RandomFloat(&rng_state) * 2.0f - 1.0f) * accuracy * W::PROPS.spread;
			round_spread_x = cosf(angle) * length;
			round_spread_y = sinf(angle) * length;
		}

		// Iterate pellets
		for (int p = 0; p < W::PROPS.pellets_no; p += 1)
		{
			float pellet_dispersion_x = round_spread_x;
			float pellet_dispersion_y = round_spread_y;

			// Calculate pellet dispersion
			if (p > 0)
			{
				const float angle = Ic::RandomFloat(&rng_state) * static_cast<float>(M_PI) * 2.0f;
				const float length = sEasing(Ic::RandomFloat(&rng_state) * 2.0f - 1.0f) * W::PROPS.pellets_dispersion;
				pellet_dispersion_x += cosf(angle) * length;
				pellet_dispersion_y += sinf(angle) * length;
			}

			// Trace ray
			// Using 'origin', not 'client_side_origin', we want to render the results that server expects
			end[0] = origin[0] + forward[0] * 8192.0f + up[0] * pellet_dispersion_x + right[0] * pellet_dispersion_y;
			end[1] = origin[1] + forward[1] * 8192.0f + up[1] * pellet_dispersion_x + right[1] * pellet_dispersion_y;
			end[2] = origin[2] + forward[2] * 8192.0f + up[2] * pellet_dispersion_x + right[2] * pellet_dispersion_y;

			gEngfuncs.pEventAPI->EV_PlayerTrace(origin, end, PM_NORMAL, -1, &tr);

			// Impact effects
			float temp[3];

			if (gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent)->studiomodel == nullptr)
			{
				// We impacted the world

				const physent_s* entity = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);
				if (entity != nullptr && (entity->solid == SOLID_BSP || entity->movetype == MOVETYPE_PUSHSTEP))
				{
					// EV_TraceTexture() is quite imprecise, so rather than let it cast from eyes all
					// the way to target, we trace from points near to the already traced surface
					temp[0] = tr.endpos[0] + tr.plane.normal[0] * 2.0f;
					temp[1] = tr.endpos[1] + tr.plane.normal[1] * 2.0f;
					temp[2] = tr.endpos[2] + tr.plane.normal[2] * 2.0f;
					end[0] = tr.endpos[0] - tr.plane.normal[0] * 2.0f;
					end[1] = tr.endpos[1] - tr.plane.normal[1] * 2.0f;
					end[2] = tr.endpos[2] - tr.plane.normal[2] * 2.0f;

					const char* texture_name = gEngfuncs.pEventAPI->EV_TraceTexture(tr.ent, temp, end);
					// gEngfuncs.Con_Printf("#### IcEventX(), '%s'\n", texture_name);

					sWorldImpact(tr.plane.normal, gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), origin, tr.endpos,
					             texture_name, light_at_impact, W::PROPS.pellets_no);

					dev_colour[0] = 0.0f;
					dev_colour[1] = 0.0f;
					dev_colour[2] = 0.5f;
				}
			}
			else
			{
				// We impacted an entity
				sEntityImpact(tr.plane.normal, origin, tr.endpos, light_at_impact, W::PROPS.pellets_no);

				dev_colour[0] = 0.5f;
				dev_colour[1] = 0.0f;
				dev_colour[2] = 0.0f;
			}

			// Render tracer
			// Here using 'client_side_origin', as is purely for aesthetics reasons
			float temp_temp[3];

			temp[0] = client_side_origin[0] - up[0] * 3.0f + right[0] * 1.0f; // TODO, there has to be a better
			temp[1] = client_side_origin[1] - up[1] * 3.0f + right[1] * 1.0f; // way to determine gun origin
			temp[2] = client_side_origin[2] - up[2] * 3.0f + right[2] * 1.0f;

			temp_temp[0] = temp[0]; // R_TracerEffect() modifies input, ewww...
			temp_temp[1] = temp[1]; // (TODO, confirm it better)
			temp_temp[2] = temp[2];

			gEngfuncs.pEfxAPI->R_TracerEffect(temp, tr.endpos);

			// Render line (for debuging purposes)
			if (Ic::GetDeveloperLevel() > 1)
			{
				const int dev_sprite = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/smoke.spr");

				gEngfuncs.pEfxAPI->R_BeamPoints(temp_temp, tr.endpos, dev_sprite, //
				                                4.0f,                             // Life
				                                0.5f,                             // Width
				                                0.0f,                             // Amplitude
				                                1.0f,                             // Brightness
				                                0,                                // Speed
				                                0,                                // Start frame
				                                0,                                // Frame rate
				                                dev_colour[0], dev_colour[1], dev_colour[2]);
			}
		}
	}

	gEngfuncs.pEventAPI->EV_PopPMStates();
}


void IcEventWeapon1(struct event_args_s* args)
{
	sGenericEvent<Ic::PistolWeapon>(args->entindex, args->origin, args->angles, args->ducking, args->fparam1,
	                                args->iparam1, args->iparam2, args->fparam2 / 255.0f);
}

void IcEventWeapon2(struct event_args_s* args)
{
	sGenericEvent<Ic::ShotgunWeapon>(args->entindex, args->origin, args->angles, args->ducking, args->fparam1,
	                                 args->iparam1, args->iparam2, args->fparam2 / 255.0f);
}

void IcEventWeapon3(struct event_args_s* args)
{
	sGenericEvent<Ic::SmgWeapon>(args->entindex, args->origin, args->angles, args->ducking, args->fparam1,
	                             args->iparam1, args->iparam2, args->fparam2 / 255.0f);
}

void IcEventWeapon4(struct event_args_s* args)
{
	sGenericEvent<Ic::ArWeapon>(args->entindex, args->origin, args->angles, args->ducking, args->fparam1, args->iparam1,
	                            args->iparam2, args->fparam2 / 255.0f);
}

void IcEventWeapon5(struct event_args_s* args)
{
	sGenericEvent<Ic::RifleWeapon>(args->entindex, args->origin, args->angles, args->ducking, args->fparam1,
	                               args->iparam1, args->iparam2, args->fparam2 / 255.0f);
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
