/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#ifndef IC_PARTICLES_HPP
#define IC_PARTICLES_HPP

#include "ic/vector.hpp"

namespace Ic
{

void ParticlesInitialise();
void ParticlesEntities();

void DustParticles(int number, Vector3 position, Vector3 force, float gravity, Vector4 colour);

} // namespace Ic

#endif
