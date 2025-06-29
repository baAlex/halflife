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

#include "pm_defs.h"
#include "pmtrace.h"

#include "events.hpp"
#include "ic/weapons.hpp"
#include "ic/base.hpp"
#include "ic/messages.hpp"
#include "ic/material.hpp"
#include "ic/particles.hpp"

#include <string.h>
#include <stdint.h>


static float sEasing(float x)
{
	return Ic::Mix(x, x * x * x, 0.5f);
	// return x * x * x; // Too much
}


template <typename W>
static void sImpact(float* normal, int impacted_entity_index, int impacted_model, const float* start,
                    const float* trace_end, const char* texture_name, uint8_t light_at_impact)
{
	float* start_pos = (float*)start; // Disgusting const* casts, Valve please fix
	float* end_pos = (float*)trace_end;

	const physent_s* entity = gEngfuncs.pEventAPI->EV_GetPhysent(impacted_entity_index);

	// Filter where not, do an impact
	if (texture_name == nullptr || strcmp(texture_name, "sky") == 0)
		return;

	if (entity == nullptr || (entity->solid != SOLID_BSP && entity->movetype != MOVETYPE_PUSHSTEP))
		return;

	// Particles
	const Ic::Material mat = Ic::GetMaterial(texture_name);

	{
		// gEngfuncs.pEfxAPI->R_BulletImpactParticles(end_pos);
		Ic::Vector3 normal2;
		normal2.x = normal[0] * 256.0f;
		normal2.y = normal[1] * 256.0f;
		normal2.z = normal[2] * 256.0f;

		uint8_t colour[4];

		switch (mat.type)
		{
		case Ic::Material::Type::Metal:
			colour[0] = 48;
			colour[1] = 48;
			colour[2] = 32;
			Ic::DustParticles(3 / Ic::Min(W::PROPS.pellets_no, 3), {end_pos[0], end_pos[1], end_pos[2]}, normal2, -0.5f,
			                  (colour[0] * light_at_impact) >> 8, (colour[1] * light_at_impact) >> 8,
			                  (colour[2] * light_at_impact) >> 8, 32);
			break;
		case Ic::Material::Type::Wood:
			colour[0] = 152;
			colour[1] = 109;
			colour[2] = 75;
			Ic::DustParticles(3 / Ic::Min(W::PROPS.pellets_no, 3), {end_pos[0], end_pos[1], end_pos[2]}, normal2, -4.0f,
			                  (colour[0] * light_at_impact) >> 8, (colour[1] * light_at_impact) >> 8,
			                  (colour[2] * light_at_impact) >> 8, 96);
			break;
		case Ic::Material::Type::Snow:
			colour[0] = 255;
			colour[1] = 255;
			colour[2] = 255;
			Ic::DustParticles(8 / Ic::Min(W::PROPS.pellets_no, 8), {end_pos[0], end_pos[1], end_pos[2]}, normal2, -4.0f,
			                  (colour[0] * light_at_impact) >> 8, (colour[1] * light_at_impact) >> 8,
			                  (colour[2] * light_at_impact) >> 8, 150);
			break;
		case Ic::Material::Type::Dirt:
			colour[0] = 158;
			colour[1] = 150;
			colour[2] = 122;
			Ic::DustParticles(8 / Ic::Min(W::PROPS.pellets_no, 8), {end_pos[0], end_pos[1], end_pos[2]}, normal2, -1.0f,
			                  (colour[0] * light_at_impact) >> 8, (colour[1] * light_at_impact) >> 8,
			                  (colour[2] * light_at_impact) >> 8, 128);
			break;

		// case Ic::Material::Type::Unknown:
		// case Ic::Material::Type::Concrete:
		default:
			colour[0] = 140;
			colour[1] = 140;
			colour[2] = 140;
			Ic::DustParticles(6 / Ic::Min(W::PROPS.pellets_no, 6), {end_pos[0], end_pos[1], end_pos[2]}, normal2, -0.5f,
			                  (colour[0] * light_at_impact) >> 8, (colour[1] * light_at_impact) >> 8,
			                  (colour[2] * light_at_impact) >> 8, 95);
			break;
		}
	}

	// Sound
	const int rand1 = gEngfuncs.pfnRandomLong(0, Ic::Material::VARIATIONS_NO - 1);
	const int rand2 = gEngfuncs.pfnRandomLong(0, Ic::Material::VARIATIONS_NO - 1);

	gEngfuncs.pEventAPI->EV_PlaySound(
	    0, end_pos, CHAN_STATIC, (char*)(mat.impact_sounds[rand1]),
	    gEngfuncs.pfnRandomFloat(0.8f, 0.9f) /
	        sqrtf(static_cast<float>(W::PROPS.pellets_no)), // Something to compensate being linear
	    ATTN_NORM, 0, 98 + rand2);

	// Decal
	if (mat.decals != nullptr && gEngfuncs.pfnGetCvarFloat("r_decals") > 0.0f)
	{
		gEngfuncs.pEfxAPI->R_DecalShoot(
		    gEngfuncs.pEfxAPI->Draw_DecalIndex(gEngfuncs.pEfxAPI->Draw_DecalIndexFromName((char*)(mat.decals[rand1]))),
		    impacted_model, 0, end_pos, 0);
	}
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
		// Nope, do what server does
		out[2] += static_cast<float>((server_crouch == false) ? DEFAULT_VIEWHEIGHT : VEC_DUCK_VIEW);
	}

	out[0] += server_origin[0];
	out[1] += server_origin[1];
	out[2] += server_origin[2];
}


template <typename W>
static void sGenericEvent(int entity, float* origin, float* angles, bool crouch, float accuracy, int rounds_no,
                          int seed, uint8_t light_at_impact)
{
	const int dev_sprite = gEngfuncs.pEventAPI->EV_FindModelIndex("sprites/smoke.spr");
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

	// Sound
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

		for (int p = 0; p < W::PROPS.pellets_no; p += 1)
		{
			float pellet_dispersion_x = round_spread_x;
			float pellet_dispersion_y = round_spread_y;

			{
				dev_colour[0] = (p == 0) ? 0.25f : 0.1f;
				dev_colour[1] = (p == 0) ? 0.25f : 0.0f;
				dev_colour[2] = (p == 0) ? 0.25f : 0.0f;
			}

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

			// Place a decal and a impact effect
			float temp[3];
			{
				temp[0] = tr.endpos[0] + tr.plane.normal[0] * 2.0f;
				temp[1] = tr.endpos[1] + tr.plane.normal[1] * 2.0f;
				temp[2] = tr.endpos[2] + tr.plane.normal[2] * 2.0f;
				end[0] = tr.endpos[0] - tr.plane.normal[0] * 2.0f;
				end[1] = tr.endpos[1] - tr.plane.normal[1] * 2.0f;
				end[2] = tr.endpos[2] - tr.plane.normal[2] * 2.0f;

				// EV_TraceTexture() is quite imprecise, so rather than let it cast from eyes all
				// the way to target, we only let it trace from points near the already traced surface
				const char* texture_name = gEngfuncs.pEventAPI->EV_TraceTexture(tr.ent, temp, end);
				// gEngfuncs.Con_Printf("#### IcEventX(), '%s'\n", texture_name);

				sImpact<W>(tr.plane.normal, tr.ent, gEngfuncs.pEventAPI->EV_IndexFromTrace(&tr), origin, tr.endpos,
				           texture_name, light_at_impact);

				// Render a line (for debuging purposes)
				if (Ic::GetDeveloperLevel() > 1)
				{
					gEngfuncs.pEfxAPI->R_BeamPoints(temp, tr.endpos, dev_sprite, //
					                                8.0f,                        // Life
					                                1.0f,                        // Width
					                                0.0f,                        // Amplitude
					                                1.0f,                        // Brightness
					                                0,                           // Speed
					                                0,                           // Start frame
					                                0,                           // Frame rate
					                                dev_colour[0] * 4.0f, dev_colour[1] * 4.0f, dev_colour[2] * 4.0f);
				}
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

			// Render more lines (for more debuging purposes)
			if (Ic::GetDeveloperLevel() > 1)
			{
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
	                                args->iparam1, args->iparam2, static_cast<uint8_t>(args->fparam2));
}

void IcEventWeapon2(struct event_args_s* args)
{
	sGenericEvent<Ic::ShotgunWeapon>(args->entindex, args->origin, args->angles, args->ducking, args->fparam1,
	                                 args->iparam1, args->iparam2, static_cast<uint8_t>(args->fparam2));
}

void IcEventWeapon3(struct event_args_s* args)
{
	sGenericEvent<Ic::SmgWeapon>(args->entindex, args->origin, args->angles, args->ducking, args->fparam1,
	                             args->iparam1, args->iparam2, static_cast<uint8_t>(args->fparam2));
}

void IcEventWeapon4(struct event_args_s* args)
{
	sGenericEvent<Ic::ArWeapon>(args->entindex, args->origin, args->angles, args->ducking, args->fparam1, args->iparam1,
	                            args->iparam2, static_cast<uint8_t>(args->fparam2));
}

void IcEventWeapon5(struct event_args_s* args)
{
	sGenericEvent<Ic::RifleWeapon>(args->entindex, args->origin, args->angles, args->ducking, args->fparam1,
	                               args->iparam1, args->iparam2, static_cast<uint8_t>(args->fparam2));
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
