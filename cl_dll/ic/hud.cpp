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
#include <string.h>

#include "hud.hpp"
#include "ic/base.hpp"
#include "ic/accuracy.hpp"
#include "ic/messages.hpp"
#include "ic/game_constants.hpp"


static SCREENINFO_s s_screen;
static int s_margin = 15; // TODO, should change according resolution

static float s_prev_time;


static constexpr int ANTI_BLEEDING = 1; // To substract from dimensions as fonts have an extra pixel to combat bleeding
static constexpr int WHITE[3] = {255, 255, 255};
static constexpr int ACCENT[3] = {234, 0, 39};


static int sFontHeight(HSPRITE font)
{
	return gEngfuncs.pfnSPR_Height(font, static_cast<int>('\n')) - ANTI_BLEEDING;
}

static void sTextDimensions(const char* text, HSPRITE font, int* out_w, int* out_h)
{
	int w_acc = 0;
	int w_current_line_acc = 0;
	int h_acc = 0;

	for (const char* c = text; *c != 0x00; c += 1)
	{
		const int frame = static_cast<int>(*c);

		if (*c == '\n')
		{
			w_acc = Ic::Max(w_acc, w_current_line_acc);
			w_current_line_acc = 0;

			h_acc += gEngfuncs.pfnSPR_Height(font, frame) - ANTI_BLEEDING;
			continue;
		}

		w_current_line_acc += gEngfuncs.pfnSPR_Width(font, frame) - ANTI_BLEEDING;
	}

	*out_w = w_acc;
	*out_h = h_acc + sFontHeight(font);
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
			y += gEngfuncs.pfnSPR_Height(font, frame) - ANTI_BLEEDING;
			continue;
		}

		gEngfuncs.pfnSPR_Draw(frame, x2, y, &rect);
		x2 += gEngfuncs.pfnSPR_Width(font, frame) - ANTI_BLEEDING;
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

			snprintf(m_text_buffer, TEXT_BUFFER_LENGTH, "Speed: %03.0f/%03.0f\nAngle: %.2f deg", Ic::GetSpeed(),
			         Ic::PLAYER_MAX_SPEED, Ic::RadToDeg(atan2f(Ic::GetForward().y, Ic::GetForward().x)));
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
	float m_hide;

  public:
	void Initialise()
	{
		m_horizontal = gEngfuncs.pfnSPR_Load("sprites/480-crosshair-h.spr");
		m_vertical = gEngfuncs.pfnSPR_Load("sprites/480-crosshair-v.spr");

		m_h_w = gEngfuncs.pfnSPR_Width(m_horizontal, 0);
		m_v_h = gEngfuncs.pfnSPR_Height(m_vertical, 0);

		m_hide = 0.0f;
	}

	void Draw(float time, float dt)
	{
		(void)time;
		struct rect_s rect;

		if (m_hide > 0.0f)
		{
			m_hide -= dt;
			return;
		}

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

	void Hide(float duration)
	{
		m_hide = duration;
	}
};


class DevText
{
	HSPRITE m_font;
	float m_life;

	static constexpr size_t TEXT_BUFFER_LENGTH = 256;
	static constexpr int LEFT_MARGIN = 40;

	char m_text_buffer[TEXT_BUFFER_LENGTH];

  public:
	void Initialise()
	{
		m_font = gEngfuncs.pfnSPR_Load("sprites/720-font-dev-node.spr");
		m_life = 0.0f;
	}

	void Draw(float time, float dt)
	{
		m_life -= dt;
		if (m_life <= 0.0f)
			return;

		int width;
		int height;
		sTextDimensions(m_text_buffer, m_font, &width, &height);

		sDrawText(LEFT_MARGIN, s_screen.iHeight / 2 - height / 2, m_font, WHITE[0], WHITE[1], WHITE[2], m_text_buffer);
	}

	void SetText(const char* text, float duration = 4.0f)
	{
		m_life = duration;

		// Wrap text
		const int RIGHT_MARGIN = s_screen.iWidth / 2 /*- LEFT_MARGIN*/;

		int word_w = 0;
		int line_w = LEFT_MARGIN;

		char* out_cursor = m_text_buffer;
		const char* in_cursor_start = text;
		const char* in_end = text + strlen(text);
		for (const char* in_cursor_end = text; in_cursor_end < (in_end + 1); in_cursor_end += 1)
		{
			const int character_w = gEngfuncs.pfnSPR_Width(m_font, static_cast<int>(*in_cursor_end)) - ANTI_BLEEDING;
			word_w += character_w;
			line_w += character_w;

			if (*in_cursor_end == ' ' || *in_cursor_end == 0x00)
			{
				if (in_cursor_start > text) // Ignore first word
				{
					if (line_w <= RIGHT_MARGIN)
					{
						*out_cursor++ = ' ';
					}
					else
					{
						*out_cursor++ = '\n';
						line_w = word_w + LEFT_MARGIN;
					}
				}

				for (; (in_cursor_start < in_cursor_end) && (out_cursor < m_text_buffer + TEXT_BUFFER_LENGTH);
				     in_cursor_start += 1, out_cursor += 1)
					*out_cursor = *in_cursor_start;

				if (*in_cursor_end == 0x00)
					break;

				in_cursor_start += 1;
				word_w = 0;
			}
		}

		*out_cursor = 0x00;
	}
};


static DevDashboard s_dev_dashboard;
static Crosshair s_crosshair;
static DevText s_dev_text;


void Ic::HudInitialise()
{
	gEngfuncs.Con_Printf("### Ic::HudInitialise()\n");

	s_screen.iSize = sizeof(SCREENINFO_s); // Silly versioning thing
	gEngfuncs.pfnGetScreenInfo(&s_screen);

	s_dev_dashboard.Initialise();
	s_crosshair.Initialise();
	s_dev_text.Initialise();

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
	s_dev_text.Draw(time, dt);
}


void Ic::HudDevText(const char* text, float duration)
{
	s_dev_text.SetText(text, duration);
	// s_crosshair.Hide(duration);
}
