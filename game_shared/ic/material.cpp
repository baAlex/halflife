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

static const char* s_flesh_decal[Ic::Material::VARIATIONS_NO] = {"{bshot1", "{bshot2", "{bshot3", "{bshot4"};
static const char* s_flesh_sound[Ic::Material::VARIATIONS_NO] = {"impacts/flesh-1.wav", "impacts/flesh-2.wav",
                                                                 "impacts/flesh-3.wav", "impacts/flesh-4.wav"};


static constexpr Ic::Vector4 GENERIC_IMPACT_COLOUR = {0.54f, 0.54f, 0.54f, 0.37f};


static constexpr Ic::Material GENERIC_MATERIAL = {
    Ic::Material::Type::Unknown, // Type
    s_generic_decal,             // Decals
    s_generic_sound,             // Impact sounds
    GENERIC_IMPACT_COLOUR,       // Impact colour
    6,                           // Impact particles number
    256.0f,                      // Impact particles force
    -0.5f,                       // Impact particles gravity
};

static constexpr Ic::Material CONCRETE_MATERIAL = {
    Ic::Material::Type::Concrete, // Type
    s_generic_decal,              // Decals
    s_generic_sound,              // Impact sounds
    GENERIC_IMPACT_COLOUR,        // Impact colour
    6,                            // Impact particles number
    256.0f,                       // Impact particles force
    -0.5f,                        // Impact particles gravity
};

static constexpr Ic::Material METAL_MATERIAL = {
    Ic::Material::Type::Metal,    // Type
    s_generic_decal,              // Decals
    s_metal_sound,                // Impact sounds
    {0.18f, 0.18f, 0.12f, 0.12f}, // Impact colour
    3,                            // Impact particles number
    256.0f,                       // Impact particles force
    -0.5f,                        // Impact particles gravity
};

static constexpr Ic::Material WOOD_MATERIAL = {
    Ic::Material::Type::Wood,     // Type
    s_generic_decal,              // Decals
    s_wood_sound,                 // Impact sounds
    {0.59f, 0.45f, 0.29f, 0.37f}, // Impact colour
    3,                            // Impact particles number
    256.0f,                       // Impact particles force
    -4.0f,                        // Impact particles gravity
};

static constexpr Ic::Material DIRT_MATERIAL = {
    Ic::Material::Type::Dirt,    // Type
    s_generic_decal,             // Decals
    s_generic_sound,             // Impact sounds
    {0.61f, 0.58f, 0.47f, 0.5f}, // Impact colour
    8,                           // Impact particles number
    256.0f,                      // Impact particles force
    -1.0f,                       // Impact particles gravity
};

static constexpr Ic::Material SNOW_MATERIAL = {
    Ic::Material::Type::Snow,  // Type
    s_generic_decal,           // Decals
    s_generic_sound,           // Impact sounds
    {1.0f, 1.0f, 1.0f, 0.58f}, // Impact colour
    8,                         // Impact particles number
    256.0f,                    // Impact particles force
    -4.0f,                     // Impact particles gravity
};

static constexpr Ic::Material FLESH_MATERIAL = {
    Ic::Material::Type::Flesh,  // Type
    s_flesh_decal,              // Decals
    s_flesh_sound,              // Impact sounds
    {0.6f, 0.05f, 0.05f, 0.4f}, // Impact colour
    4,                          // Impact particles number
    64.0f,                      // Impact particles force
    -4.0f,                      // Impact particles gravity
};


const Ic::Material* Ic::GetMaterial(const char* texture_name, bool* do_decals)
{
	const Material* ret = &GENERIC_MATERIAL; // Assume it for now
	*do_decals = true;                       // Ditto

	if (texture_name == nullptr)
		return ret;

	bool lad_found = false;
	for (const char* c = texture_name; c != 0x00 && c < texture_name + 3; c += 1) // Yes, up to 3 characters
	{
		if (*c == '#') // This lad
		{
			lad_found = true;
			break;
		}
		else if (*c == 'c')
			ret = &CONCRETE_MATERIAL;
		else if (*c == 'm')
			ret = &METAL_MATERIAL;
		else if (*c == 'w')
			ret = &WOOD_MATERIAL;
		else if (*c == 'd')
			ret = &DIRT_MATERIAL;
		else if (*c == 's')
			ret = &SNOW_MATERIAL;
		else if (*c == 'f')
			ret = &FLESH_MATERIAL;
		else if (*c == 'x')
			*do_decals = false;
	}

	// Return generic if texture did't had our nomenclature
	if (lad_found == false)
	{
		*do_decals = true;
		return &GENERIC_MATERIAL;
	}

	// Bye!
	return ret;
}


const Ic::Material* Ic::GetMaterial(Ic::Material::Type type)
{
	const Ic::Material* array[] = {
	    &GENERIC_MATERIAL, &CONCRETE_MATERIAL, &METAL_MATERIAL, &WOOD_MATERIAL,
	    &DIRT_MATERIAL,    &SNOW_MATERIAL,     &FLESH_MATERIAL,
	};

	return array[static_cast<int>(type)];
}
