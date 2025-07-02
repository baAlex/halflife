/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#ifndef IC_MATERIAL_HPP
#define IC_MATERIAL_HPP

#include "vector.hpp"

namespace Ic
{

struct Material
{
	enum class Type
	{
		Unknown = 0,
		Concrete,
		Metal,
		Wood,
		Dirt,
		Snow,
		Flesh,
	};

	static constexpr int VARIATIONS_NO = 4;

	Type type;
	const char** decals;
	const char** impact_sounds;

	Vector4 impact_colour;
	int impact_particles_number;
	float impact_particles_force;
	float impact_particles_gravity;
};

const Material* GetMaterial(const char* texture_name, bool* do_decals);
const Material* GetMaterial(Material::Type type);

} // namespace Ic

#endif
