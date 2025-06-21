/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#include "material.hpp"


static const char* s_generic_decal[Ic::Material::VARIATIONS_NO] = {"{shot1", "{shot2", "{shot3", "{shot4"};
static const char* s_generic_sound[Ic::Material::VARIATIONS_NO] = {"impacts/generic-1.wav", "impacts/generic-2.wav",
                                                                   "impacts/generic-3.wav", "impacts/generic-4.wav"};

static const char* s_metal_sound[Ic::Material::VARIATIONS_NO] = {"impacts/metal-1.wav", "impacts/metal-2.wav",
                                                                 "impacts/metal-3.wav", "impacts/metal-4.wav"};

static const char* s_wood_sound[Ic::Material::VARIATIONS_NO] = {"impacts/wood-1.wav", "impacts/wood-2.wav",
                                                                "impacts/wood-3.wav", "impacts/wood-4.wav"};


Ic::Material Ic::GetMaterial(const char* texture_name)
{
	Material ret;
	bool lad_found = false;
	bool do_decals = true;

	// Safe assumption
	ret.type = Material::Type::Unknown;
	ret.decals = s_generic_decal;
	ret.impact_sounds = s_generic_sound;

	// Parse
	if (texture_name == nullptr)
		return ret;

	for (const char* c = texture_name; c != 0x00 && c < texture_name + 3; c += 1) // Yes, up to 3 characters
	{
		if (*c == '#') // This lad
		{
			lad_found = true;
			break;
		}
		else if (*c == 'c')
		{
			ret.type = Material::Type::Concrete;
			// ret.decals = s_generic_decal;
			// ret.impact_sounds = s_generic_sound;
		}
		else if (*c == 'm')
		{
			ret.type = Material::Type::Metal;
			// ret.decals = s_generic_decal;
			ret.impact_sounds = s_metal_sound;
		}
		else if (*c == 'w')
		{
			ret.type = Material::Type::Wood;
			// ret.decals = s_generic_decal;
			ret.impact_sounds = s_wood_sound;
		}
		else if (*c == 'd')
		{
			ret.type = Material::Type::Dirt;
			// ret.decals = s_generic_decal;
			// ret.impact_sounds = s_generic_sound;
		}
		else if (*c == 's')
		{
			ret.type = Material::Type::Snow;
			// ret.decals = s_generic_decal;
			// ret.impact_sounds = s_generic_sound;
		}
		else if (*c == 'x')
			do_decals = false;
	}

	if (do_decals == false)
		ret.decals = nullptr;

	// Texture doesn't have our nomenclature, go
	// back to safe assumption
	if (lad_found == false)
	{
		ret.type = Material::Type::Unknown;
		ret.decals = s_generic_decal;
		ret.impact_sounds = s_generic_sound;
	}

	// Bye!
	return ret;
}
