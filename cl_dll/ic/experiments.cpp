/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

// ORDER OF INCLUDES IS THIS AND NO OTHER
#include "wrect.h"
#include "cl_dll.h"
#include "parsemsg.h"
#include "event_api.h"
#include "pm_defs.h"
#include "pmtrace.h"

#include "experiments.hpp"
#include "ic/vector.hpp"
#include "ic/messages.hpp"


// Vertices of an icosphere hemisphere. Unlike other sphere algorithms its
// vertices are well equally distributed... at least that is what I known
// from Blender; regular sphere there have more vertices near the poles,
// which isn't desirable to use as a kernel
static constexpr int KERNEL_SAMPLES = 17;
static constexpr Ic::Vector3 KERNEL[KERNEL_SAMPLES] = {
    {0, -0.525731086730957, 0.8506507873535156},
    {0, 0.525731086730957, 0.8506507873535156},
    {0.8506507873535156, 0, 0.525731086730957},
    {-0.8506507873535156, 0, 0.525731086730957},
    {-0.80901700258255, 0.5, 0.30901700258255005},
    {-0.5, 0.30901700258255005, 0.80901700258255},
    {-0.30901700258255005, 0.80901700258255, 0.5},
    {0.30901700258255005, 0.80901700258255, 0.5},
    {-0.80901700258255, -0.5, 0.30901700258255005},
    {-0.5, -0.30901700258255005, 0.80901700258255},
    {0, 0, 1},
    {0.5, 0.30901700258255005, 0.80901700258255},
    {0.80901700258255, 0.5, 0.30901700258255005},
    {0.80901700258255, -0.5, 0.30901700258255005},
    {0.5, -0.30901700258255005, 0.80901700258255},
    {0.30901700258255005, -0.80901700258255, 0.5},
    {-0.30901700258255005, -0.80901700258255, 0.5},
};


static Ic::Vector3 sCross(Ic::Vector3 a, Ic::Vector3 b)
{
	return {a[1] * b[2] - a[2] * b[1],    //
	        -(a[0] * b[2] - a[2] * b[0]), //
	        a[0] * b[1] - a[1] * b[0]};
}


static Ic::Vector3 sTBNRotation(Ic::Vector3 vec, Ic::Vector3 normal)
{
	// Arbitrary vector that's not parallel to normal [a][d],
	// weird hack from a random fella on internet
	Ic::Vector3 arbitrary = {0.0f, 0.0f, 1.0f};

	if ((fabsf(normal.y) < 0.999f))
	{
		arbitrary.y = 1.0f;
		arbitrary.z = 0.0f;
	}

	// Calculate TBN [b][c], normal stuff
	const Ic::Vector3 tangent = Normalize(sCross(arbitrary, normal));
	const Ic::Vector3 bi_tangent = sCross(normal, tangent);

	return {
	    vec[0] * tangent[0] + vec[1] * bi_tangent[0] + vec[2] * normal[0],
	    vec[0] * tangent[1] + vec[1] * bi_tangent[1] + vec[2] * normal[1],
	    vec[0] * tangent[2] + vec[1] * bi_tangent[2] + vec[2] * normal[2],
	};

	// Thanks random fella in a random forum:
	// [a] Erik Rufelt (2009): «There are infinitely many vectors that you can use as tangents, as it can be any vector
	// perpendicular to the normal. You can find one by calculating the cross-product of the normal and an arbitrary
	// vector that isn't parallel to the normal.»
	// https://www.gamedev.net/forums/topic/552411-calculate-tangent-from-normal/

	// Here's something else with further Google-ing:
	// [d] Geeks3D (20013) Normal Mapping without Precomputed Tangent Space Vectors
	// https://geeks3d.com/20130122/normal-mapping-without-precomputed-tangent-space-vectors

	// In any case is a weird hack because I'm not a graphical programmer

	// UPDATE, MORE SOURCES, ***IS INDEED A WEIRD HACK***:
	// Self Shadow, Perpendicular Possibilities (2011)
	// https://blog.selfshadow.com/2011/10/17/perp-vectors/
	// « A quick hack involves taking the cross product of the original unit vector – let’s call it u(x,y,z) – with a
	// fixed ‘up’ axis, e.g. (0,1,0), and then normalising. A problem here is that if the two vectors are very close –
	// or equally, pointing directly away from each other – then the result will be a degenerate vector. However, it’s
	// still a reasonable approach in the context of a camera, if the view direction can be restricted to guard against
	// this. A general solution in this situation is to fall back to an alternative axis»

	// Is a rabbit hole going back to Hughes-Möller in the 90s, including Pixar as well:
	// Pixar, Building an Orthonormal Basis, Revisited (2017). Journal of Computer Graphics Techniques Vol. 6, No. 1.

	// [b] https://shaderlabs.org/wiki/Shader_Tricks
	// [c] https://learnopengl.com/Advanced-Lighting/Normal-Mapping
}


static void sAmbientOcclusionKernel(const struct ref_params_s* ref)
{
	Ic::Vector3 view_start;

	view_start[0] = ref->simorg[0] + ref->viewheight[0];
	view_start[1] = ref->simorg[1] + ref->viewheight[1];
	view_start[2] = ref->simorg[2] + ref->viewheight[2];

	Ic::Vector3 view_end;
	ProperAngleVectors(Ic::Vector3::FromPtr(ref->cl_viewangles), &view_end, nullptr, nullptr);

	view_end[0] = view_start[0] + view_end[0] * 8192.0f;
	view_end[1] = view_start[1] + view_end[1] * 8192.0f;
	view_end[2] = view_start[2] + view_end[2] * 8192.0f;

	pmtrace_t tr;
	float kernel_start[3];
	float kernel_normal[3];

	gEngfuncs.pEventAPI->EV_PushPMStates();
	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	gEngfuncs.pEventAPI->EV_PlayerTrace((float*)(&view_start.x), (float*)(&view_end.x), PM_NORMAL, -1, &tr);
	kernel_start[0] = tr.endpos[0];
	kernel_start[1] = tr.endpos[1];
	kernel_start[2] = tr.endpos[2];
	kernel_normal[0] = tr.plane.normal[0];
	kernel_normal[1] = tr.plane.normal[1];
	kernel_normal[2] = tr.plane.normal[2];

	//

	for (int i = 0; i < KERNEL_SAMPLES; i += 1)
	{
		auto new_end = sTBNRotation(KERNEL[i], Ic::Vector3::FromPtr(kernel_normal));
		new_end[0] = kernel_start[0] + new_end[0] * 64.0f;
		new_end[1] = kernel_start[1] + new_end[1] * 64.0f;
		new_end[2] = kernel_start[2] + new_end[2] * 64.0f;

		gEngfuncs.pEventAPI->EV_PlayerTrace(kernel_start, (float*)(&new_end.x), PM_NORMAL, -1, &tr);

		unsigned char colour = static_cast<unsigned char>(tr.fraction * 128.0f);
		gEngfuncs.pEfxAPI->R_ParticleLine(kernel_start, tr.endpos, colour, colour, colour, 0.1f);
	}

	//

	gEngfuncs.pEventAPI->EV_PopPMStates();
}


void Ic::Experiments(const struct ref_params_s* ref)
{
	if (GetDeveloperLevel() > 1)
	{
		sAmbientOcclusionKernel(ref);
	}
}
