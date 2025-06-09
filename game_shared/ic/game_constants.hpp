/*

Copyright (c) 2025 Alexander Brandt

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This Source Code Form is "Incompatible With Secondary Licenses", as
defined by the Mozilla Public License, v. 2.0.
*/

#ifndef IC_GAME_CONSTANTS_HPP
#define IC_GAME_CONSTANTS_HPP

namespace Ic
{

constexpr float PLAYER_MAX_SPEED = 400.0f; // Is quite a challenge to
// retrieve the maximum speed on all circumstances. Worse, the engine
// changes it game to game[1] (is an engine cvar). And for most purposes
// I only need it to normalize things, so is not even desirable to use
// something that changes; here the answer, an hardcoded value and
// pray that normalizations yield something close to 1

// [1] Multiplayer/singleplayer games set different values

} // namespace Ic

#endif
