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
#include "APIProxy.h"

#include "const.h"
#include "pm_movevars.h"
#include "usercmd.h"
#include "ref_params.h"

#include "Exports.h"

#include "view.hpp"
#include "ic/base.hpp"
#include "ic/vector.hpp"
#include "ic/game_constants.hpp"


template <typename T> static inline constexpr void sVecOp(T callback)
{
	for (int i = 0; i < 3; i += 1)
		callback(i);
}


static unsigned s_frame; // Unsigned to let it wrap


#define SMOOTH_Z
#define SWAY
#define LEAN
#define CROUCH
#define BOB
#define BREATH


#ifdef SMOOTH_Z
static float s_smooth_z;
static constexpr float SMOOTH_Z_AMOUNT = 20.0f;     // Tested against stairs in Crossfire :)
static constexpr float SMOOTH_Z_AMOUNT_AIR = 40.0f; // If more, it feels like there is no gravity

static float sSmoothZ(float rough_z, int on_ground, float dt)
{
	if (IC_UNLIKELY(s_frame == 0))
	{
		// This accumulator is the only one that requires this initialisation, and
		// is just for aesthetic reasons. The others are barely visible when left
		// uninitialised.
		s_smooth_z = rough_z;
		return rough_z;
	}

	s_smooth_z = Ic::HolmerMix(rough_z, s_smooth_z, (on_ground != 0) ? SMOOTH_Z_AMOUNT : SMOOTH_Z_AMOUNT_AIR, dt);

	// 30 units is enough to jump and crouch without trigger the clamp
	if (0)
	{
		if (s_smooth_z < rough_z - 30.0f || s_smooth_z > rough_z + 30.0f)
			gEngfuncs.Con_Printf("Smooth is lagging behind!, %.2f vs %.2f\n", s_smooth_z, rough_z);
	}

	s_smooth_z = Ic::Clamp(s_smooth_z, rough_z - 30.0f, rough_z + 30.0f);

	return s_smooth_z;
}
#endif


#ifdef SWAY
static Ic::Vector2 s_sway;
static constexpr Ic::Vector2 SWAY_AMOUNT = {1.5f * 7.0f, 1.5f * 5.0f}; // Slow to make it obvious that is on purpose
static constexpr Ic::Vector2 SWAY_OUTSIDE_RANGE = {30.0f, 40.0f};
static constexpr Ic::Vector2 SWAY_INSIDE_RANGE = {30.0f / 2.0f, 40.0f / 2.0f};

static void sSway(const float* view_angles, float dt, float* out_model_angles)
{
	s_sway[0] = Ic::AnglesHolmerMix(-view_angles[0], s_sway[0], SWAY_AMOUNT[0], dt);
	s_sway[1] = Ic::AnglesHolmerMix(view_angles[1], s_sway[1], SWAY_AMOUNT[1], dt);

	s_sway[0] = Ic::ClampAroundCentre(s_sway[0], -view_angles[0], SWAY_OUTSIDE_RANGE[0]);
	s_sway[1] = Ic::ClampAroundCentre(s_sway[1], view_angles[1], SWAY_OUTSIDE_RANGE[1]);

	// Smooth again, 1 pole filter this time
	out_model_angles[0] = Ic::AnglesMix(-view_angles[0], s_sway[0], 0.5f);
	out_model_angles[1] = Ic::AnglesMix(view_angles[1], s_sway[1], 0.5f);
	out_model_angles[2] = 0.0f;
}
#endif


#ifdef LEAN
static Ic::Vector3 s_lean;
static constexpr Ic::Vector3 LEAN_AMOUNT = {8.0f * 0.8f, 6.0f * 0.8f, 6.0f * 0.8f};
static constexpr float LEAN_SMOOTH = 5.0f;
static constexpr Ic::Vector2 LEAN_Z_CLAMP = {-2.0f, 6.0f};

static void sLean(const float* view_angles, const float* velocity, float dt, float* out_model_angles,
                  float* out_model_z)
{
	const Ic::Vector3 up{0.0f, 0.0f, 1.0f};

	Ic::Vector3 forward;
	Ic::Vector3 right;
	Ic::Vector3 temp;

	forward[0] = cosf(Ic::DegToRad(view_angles[1])); // AngleVectors() is terrible at 2d
	forward[1] = sinf(Ic::DegToRad(view_angles[1])); // (and I don't remember how to use it)
	forward[2] = 0.0f;

	right[0] = cosf(Ic::DegToRad(view_angles[1] + 90.0f));
	right[1] = sinf(Ic::DegToRad(view_angles[1] + 90.0f));
	right[2] = 0.0f;

	sVecOp([&](int i) { temp[i] = velocity[i] / Ic::PLAYER_MAX_SPEED; });

	s_lean[0] = Ic::HolmerMix(LEAN_AMOUNT[0] * Ic::Dot(forward, temp), s_lean[0], LEAN_SMOOTH, dt);
	s_lean[1] = Ic::HolmerMix(LEAN_AMOUNT[1] * Ic::Dot(right, temp), s_lean[1], LEAN_SMOOTH, dt);
	s_lean[2] = Ic::HolmerMix(LEAN_AMOUNT[2] * Ic::Dot(up, temp), s_lean[2], LEAN_SMOOTH, dt);

	out_model_angles[0] -= (s_lean[0] > 0.0f) ? s_lean[0] : s_lean[0] / 2.0f; // Less when backwards
	out_model_angles[2] -= s_lean[1];
	*out_model_z -= Ic::Clamp(s_lean[2], LEAN_Z_CLAMP[0], LEAN_Z_CLAMP[1]);
}
#endif


#ifdef CROUCH
static float s_crouch;
static constexpr float CROUCH_AMOUNT = 2.0f;
static constexpr float CROUCH_SMOOTH = 3.0f;

static void sCrouch(int crouch, float dt, float* out_model_z)
{
	s_crouch = Ic::HolmerMix((crouch != 0) ? CROUCH_AMOUNT : 0.0f, s_crouch, CROUCH_SMOOTH, dt);
	*out_model_z += s_crouch;
}
#endif


#ifdef BOB
static float bob_switch;
static constexpr float WALK_SMOOTH = 3.0f;

static float s_bob[2];
static constexpr float BOB_AMOUNT[2] = {1.25f * 0.3f, 1.25f * 0.4f};
static constexpr float BOB_SPEED[2] = {(M_PI * 2.0) / 0.45f, (M_PI * 1.0) / 0.45f};

float sEasingOut(float x)
{
	return 1.0f - powf(1.0f - x, 5.0f);
}

float sEasingIn(float x)
{
	const float f = 1.30f;
	return 2.0f - powf((x + 1.0f) * 0.5f, f) * 2.0f;
}

static void sBob(int on_ground, const float* up, const float* right, const float* velocity, float dt,
                 float* out_model_origin)
{
	float speed = sqrtf(velocity[0] * velocity[0] + velocity[1] * velocity[1]);

	// Smooth switch
	bob_switch = Ic::HolmerMix((on_ground == 0 || speed < 40.0f) ? 0.0f : 1.0f, bob_switch, WALK_SMOOTH, dt);

	speed = sEasingOut(speed / Ic::PLAYER_MAX_SPEED) * bob_switch * dt; // From this line we need to apply 'dt'

	sVecOp([&](int i) { out_model_origin[i] += bob_switch * BOB_AMOUNT[0] * sEasingIn(sinf(s_bob[0])) * up[i]; });
	sVecOp([&](int i) { out_model_origin[i] += bob_switch * BOB_AMOUNT[1] * cosf(s_bob[1]) * right[i]; });

	s_bob[0] = fmodf(s_bob[0] + BOB_SPEED[0] * speed, M_PI * 2.0f);
	s_bob[1] = fmodf(s_bob[1] + BOB_SPEED[1] * speed, M_PI * 2.0f);
}
#endif


#ifdef BREATH
static Ic::Vector3 s_breath = {0.589, 1.123, 2.333f}; // Some random phases
static constexpr Ic::Vector3 BREATH_AMOUNT = {0.33f * 1.0f, 0.33f * 1.0f, 0.33f * 1.0f};
static constexpr Ic::Vector3 BREATH_SPEED = {1.0f / 4.4f, 1.0f / 3.5f, 1.0f / 2.6f};

static void sBreath(const float* forward, const float* right, const float* up, float dt, float* out_model_origin)
{
	sVecOp([&](int i) { out_model_origin[i] += sinf(s_breath[0]) * BREATH_AMOUNT[0] * forward[i]; });
	sVecOp([&](int i) { out_model_origin[i] += sinf(s_breath[1]) * BREATH_AMOUNT[1] * right[i]; });
	sVecOp([&](int i) { out_model_origin[i] += sinf(s_breath[2]) * BREATH_AMOUNT[2] * up[i]; });

	s_breath[0] = fmodf(s_breath[0] + dt * BREATH_SPEED[0], M_PI * 2.0f);
	s_breath[1] = fmodf(s_breath[1] + dt * BREATH_SPEED[1], M_PI * 2.0f);
	s_breath[2] = fmodf(s_breath[2] + dt * BREATH_SPEED[2], M_PI * 2.0f);
}
#endif


void Ic::ViewInitialise()
{
	gEngfuncs.Con_Printf("### Ic::ViewInitialise()\n");
	s_frame = 0;
}


extern int g_iUser1;       // Defined in "cl_dll/vgui_TeamFortressViewport.cpp"
extern int g_iUser2;       // Ditto
extern Vector v_origin;    // Defined in "cl_dll/view.cpp"
extern Vector v_angles;    // Ditto
extern Vector v_cl_angles; // Ditto
extern Vector v_sim_org;   // Ditto


static void sIntermissionView(struct ref_params_s* in_out)
{
	// Disable weapon model
	cl_entity_t* view_model = gEngfuncs.GetViewModel();
	if (view_model != nullptr)
	{
		view_model->model = nullptr; // It seems that the engine does it by default
	}

	// View origin and angles, copy from predicted values
	if (gEngfuncs.IsSpectateOnly() == 0)
	{
		sVecOp([&](int i) { in_out->vieworg[i] = in_out->simorg[i] + in_out->viewheight[i]; });
		sVecOp([&](int i) { in_out->viewangles[i] = in_out->cl_viewangles[i]; });
	}
	else // Spectators use others values
	{
		// VectorCopy(gHUD.m_Spectator.m_cameraOrigin, in_out->vieworg);
		// VectorCopy(gHUD.m_Spectator.m_cameraAngles, in_out->viewangles);
	}

	// Used outside this file
	sVecOp([&](int i) { v_origin[i] = in_out->vieworg[i]; });
	sVecOp([&](int i) { v_angles[i] = in_out->viewangles[i]; });
	sVecOp([&](int i) { v_cl_angles[i] = in_out->viewangles[i]; });
}


static void sNormalView(struct ref_params_s* in_out)
{
	// View origin and angles, copy from predicted values
	sVecOp([&](int i) { in_out->vieworg[i] = in_out->simorg[i] + in_out->viewheight[i]; });

	if (in_out->health > 0) // Not angles if we are dead
	{
		sVecOp([&](int i) { in_out->viewangles[i] = in_out->cl_viewangles[i]; });
	}

#ifdef SMOOTH_Z
	in_out->vieworg[2] = sSmoothZ(in_out->vieworg[2], in_out->onground, in_out->frametime);
#endif

	// No idea what this does aside from an offset thingie banned
	// in multiplayer. Is marked as output in 'ref_params_s' tho,
	// so maybe the engine is using it for render purposes
	gEngfuncs.pfnAngleVectors(in_out->cl_viewangles, in_out->forward, in_out->right, in_out->up);

	// Weapon model
	cl_entity_t* view_model = gEngfuncs.GetViewModel();
	if (view_model != nullptr)
	{
		// Copy origin and angles, same as view
		sVecOp([&](int i) { view_model->origin[i] = in_out->vieworg[i]; });
		sVecOp([&](int i) { view_model->angles[i] = in_out->cl_viewangles[i]; });
		view_model->angles[0] = -view_model->angles[0]; // Pitch is inverted [a]

		// Procedural animations
#ifdef SWAY
		sSway(in_out->cl_viewangles, in_out->frametime, view_model->angles);
#endif

#ifdef LEAN
		sLean(in_out->cl_viewangles, in_out->simvel, in_out->frametime, view_model->angles, &view_model->origin[2]);
#endif

#ifdef CROUCH
		sCrouch(in_out->cmd->buttons & IN_DUCK, in_out->frametime, &view_model->origin[2]);
#endif

#ifdef BOB
		sBob(in_out->onground, in_out->up, in_out->right, in_out->simvel, in_out->frametime, view_model->origin);
#endif

#ifdef BREATH
		sBreath(in_out->forward, in_out->right, in_out->up, in_out->frametime, view_model->origin);
#endif

		// No idea, engine seems to use them
		sVecOp([&](int i) { view_model->curstate.origin[i] = view_model->origin[i]; });
		sVecOp([&](int i) { view_model->latched.prevorigin[i] = view_model->origin[i]; });

		sVecOp([&](int i) { view_model->curstate.angles[i] = view_model->angles[i]; });
		sVecOp([&](int i) { view_model->latched.prevangles[i] = view_model->angles[i]; });
	}

	// Used outside this file,
	// -before changes for third person camera-
	sVecOp([&](int i) { v_angles[i] = in_out->viewangles[i]; });

	// Third person model
	{
		cl_entity_t* ent;
		ent = gEngfuncs.GetLocalPlayer();

		// Map view-wider-pitch to a shorter one for the model
		float pitch = in_out->viewangles[0];

		if (pitch > 180.0f)
			pitch -= 360.0f;
		else if (pitch < -180.f)
			pitch += 360.0f;

		pitch /= -3.0; // Des-invert [a]

		ent->angles[0] = pitch;
		ent->curstate.angles[0] = pitch;
		ent->prevstate.angles[0] = pitch;
		ent->latched.prevangles[0] = pitch;
	}

	// Third person camera
	if (CL_IsThirdPerson() != 0)
	{
		float forward[3];
		float right[3];

		float temp[3];
		float dist;

		CL_CameraOffset(temp); // Returns an improper vector: [pitch, yaw, dist]
		dist = temp[2];        // Keep this
		temp[2] = 0.0f;        // Without 'dist' now is a vector

		gEngfuncs.pfnAngleVectors(temp, forward, right, nullptr);

		sVecOp([&](int i) { in_out->vieworg[i] += -forward[i] * dist + right[i] * (dist / 6.0f); });
		sVecOp([&](int i) { in_out->viewangles[i] = temp[i]; }); // Overwrites previous value
	}

	// Used outside this file,
	// -after third person camera changes-
	sVecOp([&](int i) { v_origin[i] = in_out->vieworg[i]; });
}


void Ic::ViewUpdate(struct ref_params_s* in_out)
{
	if (in_out->intermission != 0)
	{
		sIntermissionView(in_out);
	}
	else if (in_out->spectator != 0 || g_iUser1 != 0)
	{
		// sSpectatorView(in_out);
	}
	else if (in_out->paused == 0)
	{
		sNormalView(in_out);
	}

	s_frame += 1;
}
