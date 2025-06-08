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

#include <math.h>

#include "hud.hpp"
#include "ic/base.hpp"
#include "ic/accuracy.hpp"

#include "wrect.h"
#include "cl_dll.h"
#include "APIProxy.h"

#include "triangleapi.h"


HSPRITE s_test_sprite;


void Ic::HudInitialise()
{
	gEngfuncs.Con_Printf("### Ic::HudInitialise()\n");
	s_test_sprite = gEngfuncs.pfnSPR_Load("sprites/dot.spr");
}

void Ic::HudSoftInitialise()
{
	gEngfuncs.Con_Printf("### Ic::HudSoftInitialise()\n");
}


void Ic::HudDraw(float time)
{
	(void)time;

#if 0
	gEngfuncs.Con_Printf("Ic::HudDraw()\n");

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
	gEngfuncs.pTriAPI->CullFace(TRI_FRONT);
	gEngfuncs.pTriAPI->Color4f(1.0f, 0.0f, 0.0f, 1.0f);
	gEngfuncs.pTriAPI->Brightness(1.0f);

	if (gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)(gEngfuncs.GetSpritePointer(s_test_sprite)), 0) == 0)
		return;

	gEngfuncs.pTriAPI->Begin(TRI_QUADS);
	{
		gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f);
		gEngfuncs.pTriAPI->Vertex3f(10.0f, 10.0f, 0.0f);

		gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f);
		gEngfuncs.pTriAPI->Vertex3f(50.0f, 10.0f, 0.0f);

		gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f);
		gEngfuncs.pTriAPI->Vertex3f(50.0f, 50.0f, 0.0f);

		gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f);
		gEngfuncs.pTriAPI->Vertex3f(10.0f, 50.0f, 0.0f);
	}
	gEngfuncs.pTriAPI->End();
#endif
}
