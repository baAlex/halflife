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
#include "cvardef.h"

#include "Exports.h"

#include "view.hpp"
#include "ic/base.hpp"
#include "ic/vector.hpp"
#include "ic/game_constants.hpp"
#include "ic/weapons.hpp"
#include "messages.hpp"


template <typename T> static inline constexpr void s3op(T callback)
{
	for (int i = 0; i < 3; i += 1)
		callback(i);
}


class Stabilizer
{
	static constexpr float AMOUNT = 20.0f;     // Tested against stairs in Crossfire :)
	static constexpr float AMOUNT_AIR = 40.0f; // If more, it feels like there is no gravity

	float m_smooth;

  public:
	void Initialise(float rough)
	{
		m_smooth = rough;
	}

	float Step(float rough, int on_ground, float dt)
	{
		m_smooth = Ic::HolmerMix(m_smooth, rough, (on_ground != 0) ? AMOUNT : AMOUNT_AIR, dt);

		// 30 units is enough to jump and crouch without trigger the clamp
		if (0)
		{
			if (m_smooth < rough - 30.0f || m_smooth > rough + 30.0f)
				gEngfuncs.Con_Printf("Smooth is lagging behind!, %.2f vs %.2f\n", m_smooth, rough);
		}

		m_smooth = Ic::Clamp(m_smooth, rough - 30.0f, rough + 30.0f);

		return m_smooth;
	}
};


class SwayAnimator
{
	static constexpr Ic::Vector2 AMOUNT = {1.5f * 7.0f, 1.5f * 5.0f}; // Slow to make it obvious that is on purpose
	static constexpr Ic::Vector2 RANGE = {30.0f, 40.0f};

	Ic::Vector2 m_sway;

  public:
	void Initialise(const float* view_angles)
	{
		m_sway[0] = -view_angles[0];
		m_sway[1] = view_angles[1];
	}

	void Step(const float* view_angles, float dt, float* out_model_angles)
	{
		m_sway[0] = Ic::AnglesHolmerMix(m_sway[0], -view_angles[0], AMOUNT[0], dt);
		m_sway[1] = Ic::AnglesHolmerMix(m_sway[1], view_angles[1], AMOUNT[1], dt);

		m_sway[0] = Ic::ClampAroundCentre(m_sway[0], -view_angles[0], RANGE[0]);
		m_sway[1] = Ic::ClampAroundCentre(m_sway[1], view_angles[1], RANGE[1]);

		// Smooth again, 1 pole filter this time
		out_model_angles[0] = Ic::AnglesMix(-view_angles[0], m_sway[0], 0.5f);
		out_model_angles[1] = Ic::AnglesMix(view_angles[1], m_sway[1], 0.5f);
		out_model_angles[2] = 0.0f;
	}
};


class LeanAnimator
{
	static constexpr float SWITCH_SMOOTH = 10.0f;
	static constexpr Ic::Vector3 AMOUNT = {8.0f * 0.75f, 6.0f * 1.5f, 6.0f * 1.0f};
	static constexpr float SMOOTH = 5.0f;
	static constexpr Ic::Vector2 CLAMP = {-2.0f, 6.0f};

	Ic::Vector3 m_lean;
	float m_switch1;
	float m_switch2;

  public:
	void Initialise()
	{
		m_lean[0] = 0.0f;
		m_lean[1] = 0.0f;
		m_lean[2] = 0.0f;
		m_switch1 = 0.0f;
		m_switch2 = 0.0f;
	}

	void Fire(float angle_min = 16.0f, float angle_max = 32.0f, float position = 12.0f)
	{
		m_switch1 = 0.0f;
	}

	void Step(const float* view_angles, const float* velocity, float dt, float* out_model_angles, float* out_model_z)
	{
		static constexpr Ic::Vector3 UP = {0.0f, 0.0f, 1.0f};

		Ic::Vector3 forward;
		Ic::Vector3 right;
		Ic::Vector3 temp;

		forward[0] = cosf(Ic::DegToRad(view_angles[1])); // AngleVectors() is terrible at 2d
		forward[1] = sinf(Ic::DegToRad(view_angles[1])); // (and I don't remember how to use it)
		forward[2] = 0.0f;

		right[0] = cosf(Ic::DegToRad(view_angles[1] + 90.0f));
		right[1] = sinf(Ic::DegToRad(view_angles[1] + 90.0f));
		right[2] = 0.0f;

		s3op([&](int i) { temp[i] = velocity[i] / Ic::PLAYER_MAX_SPEED; });

		m_switch1 = Ic::HolmerMix(m_switch1, 1.0f, SWITCH_SMOOTH, dt);
		m_switch2 = Ic::HolmerMix(m_switch2, m_switch1, SWITCH_SMOOTH, dt);

		m_lean[0] = Ic::HolmerMix(m_lean[0], AMOUNT[0] * Ic::Dot(forward, temp), SMOOTH, dt) * m_switch2;
		m_lean[1] = Ic::HolmerMix(m_lean[1], AMOUNT[1] * Ic::Dot(right, temp), SMOOTH, dt);
		m_lean[2] = Ic::HolmerMix(m_lean[2], AMOUNT[2] * Ic::Dot(UP, temp), SMOOTH, dt);

		// Apply
		out_model_angles[0] -= m_lean[0];
		out_model_angles[2] -= m_lean[1];

		*out_model_z -= Ic::Clamp(m_lean[2], CLAMP[0], CLAMP[1]);
	}
};


class CrouchAnimator
{
	static constexpr float CROUCH_AMOUNT = 1.5f;
	static constexpr float CROUCH_SMOOTH = 2.5f;

	float m_crouch;

  public:
	void Initialise()
	{
		m_crouch = 0.0f;
	}

	void Step(int crouch, const float* forward, const float* up, float dt, float* out_position)
	{
		m_crouch = Ic::HolmerMix(m_crouch, (crouch != 0) ? CROUCH_AMOUNT : 0.0f, CROUCH_SMOOTH, dt);

		out_position[0] -= forward[0] * m_crouch - up[0] * m_crouch;
		out_position[1] -= forward[1] * m_crouch - up[1] * m_crouch;
		out_position[2] -= forward[2] * m_crouch - up[2] * m_crouch;
	}
};


class WalkAnimator
{
	static constexpr float SWITCH_SMOOTH = 3.0f;
	static constexpr float AMOUNT[2] = {1.25f * 0.3f * 0.75f, 1.25f * 0.4f * 0.75f};
	static constexpr float SPEED[2] = {(M_PI * 2.0) / 0.45f, (M_PI * 1.0) / 0.45f};

	float m_switch;
	float m_phase[2];

  public:
	void Initialise()
	{
		m_switch = 0.0f;
		m_phase[0] = 0.0f;
		m_phase[1] = 0.0f;
	}

	void Step(int on_ground, const float* up, const float* right, const float* velocity, float dt,
	          float* out_model_origin)
	{
		// Arbitrary easings, just for the look
		auto easing_out = [](float x) { return 1.0f - powf(1.0f - x, 5.0f); };
		auto easing_in = [](float x) { return 2.0f - powf((x + 1.0f) * 0.5f, 1.30f) * 2.0f; };

		// Calculate speed
		float speed;
		{
			// 2d speed
			speed = sqrtf(velocity[0] * velocity[0] + velocity[1] * velocity[1]);

			// Under certain circumstances
			m_switch = Ic::HolmerMix(m_switch, (on_ground == 0 || speed < 40.0f) ? 0.0f : 1.0f, SWITCH_SMOOTH, dt);

			// Normalised and trough an easing
			speed = easing_out(speed / Ic::PLAYER_MAX_SPEED) * m_switch * dt;
		}

		// Update
		m_phase[0] = fmodf(m_phase[0] + SPEED[0] * speed, M_PI * 2.0f);
		m_phase[1] = fmodf(m_phase[1] + SPEED[1] * speed, M_PI * 2.0f);

		// Apply
		s3op([&](int i) { out_model_origin[i] += m_switch * AMOUNT[0] * easing_in(sinf(m_phase[0])) * up[i]; });
		s3op([&](int i) { out_model_origin[i] += m_switch * AMOUNT[1] * cosf(m_phase[1]) * right[i]; });
	}
};


class IdleAnimator
{
	static constexpr Ic::Vector3 AMOUNT = {0.33f * 0.75f, 0.33f * 0.75f, 0.33f * 0.75f};
	static constexpr Ic::Vector3 SPEED = {(1.0f / 4.4f) * 2.0f, (1.0f / 3.5f) * 2.0f, (1.0f / 2.6f) * 2.0f};

	Ic::Vector3 m_phase;

  public:
	void Initialise()
	{
		m_phase[0] = 0.589f; // Some random phases
		m_phase[1] = 1.123f;
		m_phase[2] = 2.333f;
	}

	void Step(const float* forward, const float* right, const float* up, float dt, float* out_model_origin)
	{
		// Update state
		s3op([&](int i) { out_model_origin[i] += sinf(m_phase[0]) * AMOUNT[0] * forward[i]; });
		s3op([&](int i) { out_model_origin[i] += sinf(m_phase[1]) * AMOUNT[1] * right[i]; });
		s3op([&](int i) { out_model_origin[i] += sinf(m_phase[2]) * AMOUNT[2] * up[i]; });

		// Apply
		s3op([&](int i) { m_phase[i] = fmodf(m_phase[i] + dt * SPEED[i], M_PI * 2.0f); });
	}
};


class FireAnimator
{
	static constexpr float ANGLE_TENSION[2] = {12.0f, 2.0f};
	static constexpr float POSITION_TENSION[2] = {12.0f, 4.0f};
	static constexpr float FEEDBACK[2] = {2.0f, 5.0f};

	uint16_t m_rng_state;
	float m_feedback;
	float m_angle_flip_flop;
	float m_position_flip_flop;

	float m_angle_lp1;
	float m_angle_lp2;
	float m_pos_lp1;
	float m_pos_lp2;

  public:
	void Initialise()
	{
		m_rng_state = 0xBEEF;
		m_feedback = 0.0f;
		m_angle_flip_flop = 0.0f;
		m_position_flip_flop = 0.0f;

		m_angle_lp1 = 0.0f;
		m_angle_lp2 = 0.0f;
		m_pos_lp1 = 0.0f;
		m_pos_lp2 = 0.0f;
	}

	void Fire(float angle_min, float angle_max, float position)
	{
		m_feedback = FEEDBACK[0] + Ic::RandomFloat(&m_rng_state) * FEEDBACK[1];
		m_angle_flip_flop = angle_min + Ic::RandomFloat(&m_rng_state) * (angle_max - angle_min);
		m_position_flip_flop = -position;
	}

	void Step(const float* forward, float dt, float* out_angles, float* out_model_origin)
	{
		m_angle_flip_flop /= dt * 50.0f;    // Filters cannot filter that much at low frame rates,
		m_position_flip_flop /= dt * 50.0f; // so I'm reducing the shake. Btw, 50 is a magic number

		// Update state,
		// an arrangement of two poles lowpass filters with resonance
		const float angle_tension = (m_angle_flip_flop > 0.0f) ? ANGLE_TENSION[0] : ANGLE_TENSION[1];
		const float pos_tension = (m_angle_flip_flop > 0.0f) ? POSITION_TENSION[0] : POSITION_TENSION[1];

		m_angle_lp1 = Ic::AnglesHolmerMix(m_angle_lp1, m_angle_flip_flop - m_angle_lp2 * m_feedback, angle_tension, dt);
		m_angle_lp1 = Ic::Clamp(m_angle_lp1, 0.0f, 50.0f); // Don't let it go unstable
		m_angle_lp2 = Ic::AnglesHolmerMix(m_angle_lp2, m_angle_lp1, angle_tension, dt);

		m_pos_lp1 = Ic::AnglesHolmerMix(m_pos_lp1, m_position_flip_flop - m_pos_lp2 * m_feedback, pos_tension, dt);
		m_pos_lp1 = Ic::Clamp(m_pos_lp1, -10.0f, 0.0f);
		m_pos_lp2 = Ic::AnglesHolmerMix(m_pos_lp2, m_pos_lp1, pos_tension, dt);

		// Apply
		s3op([&](int i) { out_model_origin[i] += forward[i] * m_pos_lp2; });
		out_angles[0] += m_angle_lp2;

		// Restore flip flops
		m_angle_flip_flop = 0.0f;
		m_position_flip_flop = 0.0f;
	}
};


extern int g_iUser1;       // Defined in "cl_dll/vgui_TeamFortressViewport.cpp"
extern Vector v_origin;    // Defined in "cl_dll/view.cpp"
extern Vector v_cl_angles; // Ditto

static unsigned s_frame = 0; // Unsigned to let it wrap

static cvar_t* s_gun_x;
static cvar_t* s_gun_y;
static cvar_t* s_gun_z;

static Stabilizer s_stabilizer;
static SwayAnimator s_sway_animator;
static LeanAnimator s_lean_animator;
static CrouchAnimator s_crouch_animator;
static WalkAnimator s_walk_animator;
static IdleAnimator s_idle_animator;
static FireAnimator s_fire_animator;


void Ic::ViewFire(float angle_min, float angle_max, float position)
{
	s_lean_animator.Fire();
	s_fire_animator.Fire(angle_min, angle_max, position);
}


void Ic::ViewInitialise()
{
	gEngfuncs.Con_Printf("### Ic::ViewInitialise()\n");
	s_frame = 0;

	gEngfuncs.pfnRegisterVariable("gun_x", "5", 0);
	gEngfuncs.pfnRegisterVariable("gun_y", "-1", 0);
	gEngfuncs.pfnRegisterVariable("gun_z", "-9", 0);

	gEngfuncs.pfnRegisterVariable("gun_fov", "0.666", 0); // StudioModelRendered() seems not being
	                                                      // able to register cvars, only read them

	s_gun_x = gEngfuncs.pfnGetCvarPointer("gun_x");
	s_gun_y = gEngfuncs.pfnGetCvarPointer("gun_y");
	s_gun_z = gEngfuncs.pfnGetCvarPointer("gun_z");

	// s_stabilizer.Initialise(0.0f); // ViewInitialise() happens before level starts, which means that we don't known z
	// position yet
	// s_sway_animator.Initialise(0.0f); // Same problem here

	s_lean_animator.Initialise();
	s_crouch_animator.Initialise();
	s_walk_animator.Initialise();
	s_idle_animator.Initialise();
	s_fire_animator.Initialise();
}


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
		s3op([&](int i) { in_out->vieworg[i] = in_out->simorg[i] + in_out->viewheight[i]; });
		s3op([&](int i) { in_out->viewangles[i] = in_out->cl_viewangles[i]; });
	}
	else // Spectators use others values
	{
		// VectorCopy(gHUD.m_Spectator.m_cameraOrigin, in_out->vieworg);
		// VectorCopy(gHUD.m_Spectator.m_cameraAngles, in_out->viewangles);
	}

	// Used outside this file
	s3op([&](int i) { v_origin[i] = in_out->vieworg[i]; });
	s3op([&](int i) { v_cl_angles[i] = in_out->viewangles[i]; });
}


static void sNormalView(struct ref_params_s* in_out)
{
	// View origin and angles, copy from predicted values
	s3op([&](int i) { in_out->vieworg[i] = in_out->simorg[i] + in_out->viewheight[i]; });

	if (in_out->health > 0) // Not angles if we are dead
	{
		s3op([&](int i) { in_out->viewangles[i] = in_out->cl_viewangles[i]; });
	}

	if (s_frame == 0) // This only happens once
	{
		s_sway_animator.Initialise(in_out->cl_viewangles);
		s_stabilizer.Initialise(in_out->vieworg[2]);
	}

	in_out->vieworg[2] = s_stabilizer.Step(in_out->vieworg[2], in_out->onground, in_out->frametime);

	// No idea what this does aside from an offset thingie banned
	// in multiplayer. Is marked as output in 'ref_params_s' tho,
	// so maybe the engine is using it for render purposes
	gEngfuncs.pfnAngleVectors(in_out->viewangles, in_out->forward, in_out->right, in_out->up);

	// Weapon model
	cl_entity_t* view_model = gEngfuncs.GetViewModel();
	if (view_model != nullptr)
	{
		// Copy origin and angles, same as view
		s3op([&](int i) { view_model->origin[i] = in_out->vieworg[i]; });
		s3op([&](int i) { view_model->angles[i] = in_out->cl_viewangles[i]; });
		view_model->angles[0] = -view_model->angles[0]; // Pitch is inverted [a]

		s3op([&](int i) { view_model->origin[i] += in_out->right[i] * s_gun_x->value; });
		s3op([&](int i) { view_model->origin[i] += in_out->forward[i] * s_gun_y->value; });
		s3op([&](int i) { view_model->origin[i] += in_out->up[i] * s_gun_z->value; });

		// gEngfuncs.Con_Printf("### %.2f, %.2f, %.2f\n", s_gun_x->value, s_gun_y->value, s_gun_z->value);

		// Procedural animations
		if (1)
		{
			s_sway_animator.Step(in_out->cl_viewangles, in_out->frametime, view_model->angles);
		}

		if (1)
		{
			s_lean_animator.Step(in_out->cl_viewangles, in_out->simvel, in_out->frametime, view_model->angles,
			                     &view_model->origin[2]);
		}

		if (1)
		{
			s_crouch_animator.Step(in_out->cmd->buttons & IN_DUCK, in_out->forward, in_out->up, in_out->frametime,
			                       view_model->origin);
		}

		if (1)
		{
			s_walk_animator.Step(in_out->onground, in_out->up, in_out->right, in_out->simvel, in_out->frametime,
			                     view_model->origin);
		}

		if (1)
		{
			s_idle_animator.Step(in_out->forward, in_out->right, in_out->up, in_out->frametime, view_model->origin);
		}

		if (1)
		{
			s_fire_animator.Step(in_out->forward, in_out->frametime, view_model->angles, view_model->origin);
		}

		// No idea, engine seems to use them
		s3op([&](int i) { view_model->curstate.origin[i] = view_model->origin[i]; });
		s3op([&](int i) { view_model->latched.prevorigin[i] = view_model->origin[i]; });

		s3op([&](int i) { view_model->curstate.angles[i] = view_model->angles[i]; });
		s3op([&](int i) { view_model->latched.prevangles[i] = view_model->angles[i]; });
	}

	// Third person model
	{
		cl_entity_t* ent;
		ent = gEngfuncs.GetLocalPlayer();

		// Map view-wider-pitch to a shorter one for the model
		float pitch = in_out->cl_viewangles[0];

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

		s3op([&](int i) { in_out->vieworg[i] += -forward[i] * dist + right[i] * (dist / 6.0f); });
		s3op([&](int i) { in_out->viewangles[i] = temp[i]; }); // Overwrites previous value
	}

	// Used outside this file,
	// -after third person camera changes-
	s3op([&](int i) { v_origin[i] = in_out->vieworg[i]; });
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
