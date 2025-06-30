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

#ifndef IC_FOG_HPP
#define IC_FOG_HPP

#include "ic/vector.hpp"

namespace Ic
{

void FogInitialise();
void FogDraw();

void SoftwareFog(Ic::Vector3 camera, Ic::Vector3 point, Ic::Vector4* out_colour, float* out_mix);

} // namespace Ic

#endif
