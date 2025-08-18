/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

// ORDER OF INCLUDES IS THIS AND NO OTHER
#include "cvardef.h"
#include "wrect.h"
#include "cl_dll.h"

#include "parsemsg.h"
#include "com_model.h"

#include "event_api.h"
#include "pm_defs.h"
#include "pmtrace.h"

#include "messages.hpp"
#include "ic/base.hpp"
#include "ic/weapons.hpp"
#include "ic/temp-entities.hpp"
#include "ic/hud.hpp"

#include <string.h>

#include "ic/messages.hpp"
#include "ic/dev-nodes.hpp"


bool sLineSphereIntersect(Ic::Vector3 line_start, Ic::Vector3 line_end, Ic::Vector3 sphere_centre, float sphere_radius)
{
	// Kyle Halladay. Ray-Sphere Intersection with Simple Math. 24 Dec 2013
	// https://kylehalladay.com/blog/tutorial/math/2013/12/24/Ray-Sphere-Intersection.html

	// A Minimal Ray-Tracer
	// https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection.html

	const Ic::Vector3 dir = Normalize(Subtract(line_end, line_start));

	const Ic::Vector3 l = Subtract(sphere_centre, line_start);
	const float tc = Dot(l, dir);
	if (tc < 0.0f)
	{
		// gEngfuncs.Con_Printf("### Reject, wrong direction\n");
		return false;
	}

	const float d = Dot(l, l) - tc * tc;
	if (d > sphere_radius * sphere_radius)
	{
		// gEngfuncs.Con_Printf("### Reject, outside radius\n");
		return false;
	}

	const Ic::Vector3 l2 = Subtract(sphere_centre, line_end);
	if (Dot(l2, dir) > 0.0f)
	{
		// gEngfuncs.Con_Printf("### Reject, line ends early\n");
		return false;
	}

	// gEngfuncs.Con_Printf("### Hai!\n");
	return true;
}


struct DevNode
{
	float active;
	Ic::Vector3 position;
	float duration;
	char text[Ic::DEV_NODE_MAX_TEXT_LENGTH];

	static void CreateCallback(float dt, void* user_data, cl_entity_t* entity)
	{
		DevNode* self = reinterpret_cast<DevNode*>(user_data);

		// Put it 32 units from floor, just to keep uniformity
		{
			pmtrace_t tr;

			Ic::Vector3 end = self->position;
			end[2] -= 8192.0f;

			gEngfuncs.pEventAPI->EV_PushPMStates();
			gEngfuncs.pEventAPI->EV_SetTraceHull(2);
			gEngfuncs.pEventAPI->EV_PlayerTrace((float*)(&self->position.x), (float*)(&end.x), PM_NORMAL, -1, &tr);
			gEngfuncs.pEventAPI->EV_PopPMStates();

			self->position[2] = tr.endpos[2] + 32.0f;
		}

		entity->origin[0] = self->position.x;
		entity->origin[1] = self->position.y;
		entity->origin[2] = self->position.z;

		int temp;
		entity->model = gEngfuncs.CL_LoadModel("models/dev_node.mdl", &temp);

		gEngfuncs.Con_Printf("### Created dev node \"%s\" (%.2f seconds)\n", self->text, self->duration);
	}

	static int UpdateCallback(float dt, void* user_data, cl_entity_t* entity)
	{
		DevNode* self = reinterpret_cast<DevNode*>(user_data);
		float rotation_speed = 64.0f;

		if (self->active > 0.0f)
		{
			self->active -= dt;
			rotation_speed = 300.0f;
		}
		else if (sLineSphereIntersect(Ic::GetPosition(), Ic::GetLookEnd(), self->position, 24.0f) == true)
			rotation_speed = 170.0f;

		entity->curstate.angles[1] = fmodf(entity->curstate.angles[1] + rotation_speed * dt, 360.0f);
		return 0;
	}

	static bool UseCallback(DevNode* self)
	{
		if (sLineSphereIntersect(Ic::GetPosition(), Ic::GetLookEnd(), self->position, 24.0f) == true)
		{
			Ic::HudDevText(self->text, self->duration);
			self->active = self->duration;
			return true;
		}

		return false;
	}
};


static constexpr int MAX_NODES = 32;
DevNode* s_nodes[MAX_NODES];

void Ic::CreateDevNode(Ic::Vector3 position, const char* text)
{
	DevNode node = {};
	{
		// TODO, this can be part of CreateCallback()
		node.active = 0.0;
		node.position = position;
		node.duration = 0.0;

		int i = 0;
		for (const char* c = text; *c != 0x00; c += 1, i += 1)
			node.duration += (*c == ' ') ? 0.4f : 0.0f; // Seconds

		node.duration = Ic::Max(node.duration, 4.0f);
		strncpy(node.text, text, DEV_NODE_MAX_TEXT_LENGTH);
	}

	for (int i = 0; i < MAX_NODES; i += 1)
	{
		if (s_nodes[i] != nullptr)
			continue;

		s_nodes[i] = reinterpret_cast<DevNode*>(Ic::CreateTempEntity(
		    Ic::TempEntityType::Normal, DevNode::CreateCallback, DevNode::UpdateCallback, sizeof(DevNode), &node));
		break;
	}
}

bool Ic::DevNodesInterceptUse()
{
	for (int i = 0; i < MAX_NODES; i += 1)
	{
		if (s_nodes[i] == nullptr)
			continue;

		if (DevNode::UseCallback(s_nodes[i]) == true)
			return true;
	}

	return false;
}
