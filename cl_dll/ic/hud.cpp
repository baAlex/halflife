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
#include "cvardef.h"
#include "wrect.h"
#include "cl_dll.h"
#include "APIProxy.h"

#include <math.h>

#include "hud.hpp"
#include "ic/base.hpp"
#include "ic/accuracy.hpp"
#include "ic/messages.hpp"
#include "ic/game_constants.hpp"


static SCREENINFO_s s_screen;
static int s_margin = 15; // TODO, should change according resolution

static float s_prev_time;


static constexpr int WHITE[3] = {255, 255, 255};
static constexpr int ACCENT[3] = {234, 0, 39};


static int sFontHeight(HSPRITE font)
{
	// Less one as fonts have am extra pixel to combat bleeding
	return gEngfuncs.pfnSPR_Height(font, static_cast<int>('\n')) - 1;
}

static void sDrawText(int x, int y, HSPRITE font, int r, int g, int b, const char* text)
{
	struct rect_s rect;
	int x2 = x;

	gEngfuncs.pfnSPR_Set(font, r, g, b);

	for (const char* c = text; *c != 0x00; c += 1)
	{
		const int frame = static_cast<int>(*c);

		if (*c == '\n')
		{
			x2 = x;
			y += gEngfuncs.pfnSPR_Height(font, frame) - 1; // Less one as fonts have am extra pixel to combat bleeding
			continue;
		}

		gEngfuncs.pfnSPR_DrawHoles(frame, x2, y, &rect);
		x2 += gEngfuncs.pfnSPR_Width(font, frame) - 1; // Less one as fonts have am extra pixel to combat bleeding
	}
}


class DevDashboard
{
	HSPRITE m_dev_font;

	static constexpr size_t TEXT_BUFFER_LENGTH = 256;
	char m_text_buffer[TEXT_BUFFER_LENGTH];

  public:
	void Initialise()
	{
		m_dev_font = gEngfuncs.pfnSPR_Load("sprites/480-font-dev.spr");
	}

	void Draw(float time, float dt)
	{
		(void)time;
		struct rect_s rect;

		if (Ic::GetDeveloperLevel() > 0)
		{
			const int height = sFontHeight(m_dev_font);

			snprintf(m_text_buffer, TEXT_BUFFER_LENGTH, "Dt: %f", dt);
			sDrawText(s_margin, 100 + height * 0, m_dev_font, WHITE[0], WHITE[1], WHITE[2], m_text_buffer);

			snprintf(m_text_buffer, TEXT_BUFFER_LENGTH, "Health: %i", Ic::GetHealth());
			sDrawText(s_margin, 100 + height * 2, m_dev_font, WHITE[0], WHITE[1], WHITE[2], m_text_buffer);

			snprintf(m_text_buffer, TEXT_BUFFER_LENGTH, "Weapon:\n- (S) %s, %s, %i + %i", Ic::GetWeaponName(),
			         Ic::GetWeaponMode(), Ic::GetChamberAmmo(), Ic::GetMagazineAmmo());
			sDrawText(s_margin, 100 + height * 4, m_dev_font, WHITE[0], WHITE[1], WHITE[2], m_text_buffer);

			snprintf(m_text_buffer, TEXT_BUFFER_LENGTH, "Client accuracy: %.2f\nServer accuracy: %.2f",
			         Ic::GetAccuracy(Ic::Side::Client), Ic::GetAccuracy(Ic::Side::Server));
			sDrawText(s_margin, 100 + height * 7, m_dev_font, WHITE[0], WHITE[1], WHITE[2], m_text_buffer);

			snprintf(m_text_buffer, TEXT_BUFFER_LENGTH, "Speed: %03.0f/%03.0f", Ic::GetSpeed(), Ic::PLAYER_MAX_SPEED);
			sDrawText(s_margin, 100 + height * 10, m_dev_font, WHITE[0], WHITE[1], WHITE[2], m_text_buffer);
		}
	}
};


class Crosshair
{
	// Seems like I'm in a good direction, crosshair here looks identical to the one
	// from Day of Defeat (including bleeding, blur). The one in Counter Strike tho,
	// seems to be using TriangleApi as is quite sharp and thin.

	static constexpr int OFFSET = 1;

	static constexpr int MINIMUM_GAP = 2;
	static constexpr float AMPLITUDE = 70.0f;

	HSPRITE m_horizontal;
	HSPRITE m_vertical;

	int m_h_w;
	int m_v_h;

  public:
	void Initialise()
	{
		m_horizontal = gEngfuncs.pfnSPR_Load("sprites/480-crosshair-h.spr");
		m_vertical = gEngfuncs.pfnSPR_Load("sprites/480-crosshair-v.spr");

		m_h_w = gEngfuncs.pfnSPR_Width(m_horizontal, 0);
		m_v_h = gEngfuncs.pfnSPR_Height(m_vertical, 0);
	}

	void Draw(float time, float dt)
	{
		(void)time;
		(void)dt;
		struct rect_s rect;

		{
			// Accuracy should be around 0,1
			const int gap = MINIMUM_GAP + static_cast<int>(roundf(Ic::GetAccuracy(Ic::Side::Client) * AMPLITUDE));

			gEngfuncs.pfnSPR_Set(m_horizontal, ACCENT[0], ACCENT[1], ACCENT[2]);
			gEngfuncs.pfnSPR_DrawHoles(0, s_screen.iWidth / 2 + gap, s_screen.iHeight / 2 - OFFSET, &rect);
			gEngfuncs.pfnSPR_DrawHoles(1, s_screen.iWidth / 2 - gap - m_h_w, s_screen.iHeight / 2 - OFFSET, &rect);

			gEngfuncs.pfnSPR_Set(m_vertical, ACCENT[0], ACCENT[1], ACCENT[2]);
			gEngfuncs.pfnSPR_DrawHoles(0, s_screen.iWidth / 2 - OFFSET, s_screen.iHeight / 2 + gap, &rect);
			gEngfuncs.pfnSPR_DrawHoles(1, s_screen.iWidth / 2 - OFFSET, s_screen.iHeight / 2 - gap - m_v_h, &rect);
		}

		if (Ic::GetDeveloperLevel() > 1)
		{
			const int gap = MINIMUM_GAP + static_cast<int>(roundf(Ic::GetAccuracy(Ic::Side::Server) * AMPLITUDE));

			gEngfuncs.pfnSPR_Set(m_horizontal, WHITE[0], WHITE[1], WHITE[2]);
			gEngfuncs.pfnSPR_DrawHoles(0, s_screen.iWidth / 2 + gap, 4 + s_screen.iHeight / 2 - OFFSET, &rect);
			gEngfuncs.pfnSPR_DrawHoles(1, s_screen.iWidth / 2 - gap - m_h_w, -4 + s_screen.iHeight / 2 - OFFSET, &rect);

			gEngfuncs.pfnSPR_Set(m_vertical, WHITE[0], WHITE[1], WHITE[2]);
			gEngfuncs.pfnSPR_DrawHoles(0, -4 + s_screen.iWidth / 2 - OFFSET, s_screen.iHeight / 2 + gap, &rect);
			gEngfuncs.pfnSPR_DrawHoles(1, 4 + s_screen.iWidth / 2 - OFFSET, s_screen.iHeight / 2 - gap - m_v_h, &rect);
		}
	}
};


static DevDashboard s_dev_dashboard;
static Crosshair s_crosshair;


void Ic::HudInitialise()
{
	gEngfuncs.Con_Printf("### Ic::HudInitialise()\n");

	s_screen.iSize = sizeof(SCREENINFO_s); // Silly versioning thing
	gEngfuncs.pfnGetScreenInfo(&s_screen);

	s_dev_dashboard.Initialise();
	s_crosshair.Initialise();

	HudSoftInitialise();
}

void Ic::HudSoftInitialise()
{
	gEngfuncs.Con_Printf("### Ic::HudSoftInitialise()\n");
	s_prev_time = 0.0f;
}


void Ic::HudDraw(float time)
{
	const float dt = time - s_prev_time;
	s_prev_time = time;

	s_dev_dashboard.Draw(time, dt);

	if (Ic::GetIfDead() == true)
		return;

	s_crosshair.Draw(time, dt);
}
