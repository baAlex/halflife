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


extern int g_iUser1;       // Defined in "cl_dll/vgui_TeamFortressViewport.cpp"
extern int g_iUser2;       // Ditto
extern Vector v_origin;    // Defined in "cl_dll/view.cpp"
extern Vector v_angles;    // Ditto
extern Vector v_cl_angles; // Ditto
extern Vector v_sim_org;   // Ditto


void Ic::ViewInitialise() {}


template <typename T> static void sVecOp(T callback)
{
	for (int i = 0; i < 3; i += 1)
		callback(i);
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
}
