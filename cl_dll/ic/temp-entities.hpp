/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#ifndef IC_TEMP_ENTITIES_HPP
#define IC_TEMP_ENTITIES_HPP

namespace Ic
{

using CreateCallback = void (*)(float dt, void* user_data, cl_entity_t* entity);
using UpdateCallback = int (*)(float dt, void* user_data, cl_entity_t* entity);

enum class TempEntityType
{
	Normal,
	Particle
};

void InitialiseTempEntities();
void UpdateTempEntities();

int CreateTempEntity(TempEntityType, CreateCallback create_callback, UpdateCallback update_callback,
                     size_t user_data_size, void* initial_user_data = nullptr);

} // namespace Ic

#endif
