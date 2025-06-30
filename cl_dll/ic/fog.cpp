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
#include "triangleapi.h"
#include "cvardef.h"

#include "fog.hpp"
#include "messages.hpp"
#include "ic/base.hpp"

#include "SDL2/SDL_video.h"


// Mini Glad implementation

#ifndef GLAPI
#if defined(GLAD_GLAPI_EXPORT)
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(GLAD_GLAPI_EXPORT_BUILD)
#if defined(__GNUC__)
#define GLAPI __attribute__((dllexport)) extern
#else
#define GLAPI __declspec(dllexport) extern
#endif
#else
#if defined(__GNUC__)
#define GLAPI __attribute__((dllimport)) extern
#else
#define GLAPI __declspec(dllimport) extern
#endif
#endif
#elif defined(__GNUC__) && defined(GLAD_GLAPI_EXPORT_BUILD)
#define GLAPI __attribute__((visibility("default"))) extern
#else
#define GLAPI extern
#endif
#else
#define GLAPI extern
#endif

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY*
#endif
#endif

typedef int GLint;
typedef float GLfloat;
typedef unsigned int GLenum;

typedef void(APIENTRYP PFNGLENABLEPROC)(GLenum cap);
typedef void(APIENTRYP PFNGLFOGIPROC)(GLenum pname, GLint param);
typedef void(APIENTRYP PFNGLFOGFPROC)(GLenum pname, GLfloat param);
typedef void(APIENTRYP PFNGLFOGFVPROC)(GLenum pname, const GLfloat* params);

GLAPI PFNGLENABLEPROC glad_glEnable = NULL;
GLAPI PFNGLFOGIPROC glad_glFogi = NULL;
GLAPI PFNGLFOGFPROC glad_glFogf = NULL;
GLAPI PFNGLFOGFVPROC glad_glFogfv = NULL;

#define GL_FOG 0x0B60
#define GL_FOG_DENSITY 0x0B62
#define GL_FOG_START 0x0B63
#define GL_FOG_END 0x0B64
#define GL_FOG_MODE 0x0B65
#define GL_FOG_COLOR 0x0B66
#define GL_LINEAR 0x2601
#define GL_EXP 0x0800
#define GL_EXP2 0x0801

// ----


static int s_fog_available = 0;


void Ic::FogInitialise()
{
	// Dynamical load OpenGl functions,
	// using SDL to make it portable, the un-portable alternative is to include
	// some ancient Gl header ("opengl.h") and link against an equally ancient
	// library (which is doable in Windows, not in Linux). In our case is up to
	// SDL to dynamically link against a proper Gl implementation
	glad_glEnable = (PFNGLENABLEPROC)(SDL_GL_GetProcAddress("glEnable"));
	glad_glFogi = (PFNGLFOGIPROC)(SDL_GL_GetProcAddress("glFogi"));
	glad_glFogf = (PFNGLFOGFPROC)(SDL_GL_GetProcAddress("glFogf"));
	glad_glFogfv = (PFNGLFOGFVPROC)(SDL_GL_GetProcAddress("glFogfv"));

	if (glad_glEnable == nullptr || glad_glFogi == nullptr || //
	    glad_glFogf == nullptr || glad_glFogfv == nullptr)
	{
		gEngfuncs.Con_Printf("Error loading OpenGl functions.\n");
	}
	else
	{
		s_fog_available = 1;
	}
}


static Ic::Vector3 s_colour = {1.0f, 0.0f, 0.0f};
static float s_density = 1.0f / 500.0f;

void Ic::SoftwareFog(Ic::Vector3 camera, Ic::Vector3 point, Ic::Vector4* out_colour, float* out_mix)
{
	// It's 2025 and I'm here emulating OpenGl's fixed pipeline from the 90s...
	// credits to ChatGPT for this 'Vector scalar projection' trick, yeah really.

	// As Wikipedia, the trick comes from:
	// «The scalar projection is a scalar, equal to the *length* of the *orthogonal* projection of A on B,
	//  with a negative sign if the projection has an opposite direction with respect to B.»
	// (https://en.wikipedia.org/wiki/Scalar_projection) (emphasis mine)

	// Here my human interpretation:

	// Basically old OpenGl calculates fog density from Z component, *after* world-to-screen matrices
	// multiplications on vertices. As such, fog is a weird screen/planar/orthogonal distance/length
	// thing (what other terms should I add?, lol), between vertices and the near plane.

	// How Dot() trick works, I have no idea (...kinda, the diagram in Wikipedia is quite good).
	// Important tho is that a distance/length of the screen/planar/orthogonal kind, is exactly what is
	// needed to emulate OpenGl's old fog. And I'm glad the AI told me about yet another use of Dot().

	const float length = fabsf(Dot(Subtract(point, camera), GetForward()));
	const float mix = Clamp(expf(-powf(s_density * length, 2.0f)), 0.0f, 1.0f);

	out_colour->x = s_colour.x;
	out_colour->y = s_colour.y;
	out_colour->z = s_colour.z;
	out_colour->w = 1.0f;
	*out_mix = mix;
}


void Ic::FogDraw()
{
	const float s = 0.01f;  // Start and end doesn't affect EXP2 mode, which in
	const float e = 500.0f; // turn is hardcoded because is the best looking one
	// and what Valve, hardcoded-ly, applies to models and brush models (all
	// code involving the mini-glad implementation here only affects 'fs_world.frag'
	// shader file)

	if (s_fog_available == 0)
		return;

	{
		const WorldProperties* p = GetWorldProperties();

		const float view_angle = RadToDeg(atan2f(GetForward().y, GetForward().x));
		const float diff = (fabsf(AnglesDifference(view_angle, p->fog_angle)) / 180.0f);
		s_colour = Mix(p->fog_colour2, p->fog_colour1, diff);

		// gEngfuncs.Con_Printf("%.2f -> %.2f\n", AnglesDifference(view_angle, p->fog_angle), diff);

		if (p->fog_density != 0.0f)
			s_density = 1.0f / p->fog_density;
		else
			s_density = 0.0f;
	}

	glad_glEnable(GL_FOG);
	glad_glFogfv(GL_FOG_COLOR, &s_colour.x);
	glad_glFogi(GL_FOG_MODE, GL_EXP2);
	glad_glFogf(GL_FOG_DENSITY, s_density);
	glad_glFogf(GL_FOG_START, s);
	glad_glFogf(GL_FOG_END, e);

	gEngfuncs.pTriAPI->FogParams(s_density, 0);
	gEngfuncs.pTriAPI->Fog(&s_colour.x, s, e, 1);
}
