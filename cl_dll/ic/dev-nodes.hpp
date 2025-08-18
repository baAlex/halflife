/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#ifndef IC_DEV_NODES_HPP
#define IC_DEV_NODES_HPP

#include "ic/vector.hpp"

namespace Ic
{

static constexpr size_t DEV_NODE_MAX_TEXT_LENGTH = 256;

bool DevNodesInterceptUse();
void CreateDevNode(Vector3 pos, const char* text);

} // namespace Ic

#endif
