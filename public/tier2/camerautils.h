//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Compatibility shim. CS:GO consolidated the camera utilities into
//          mathlib/camera.{h,cpp} (Camera_t / CFrustum / ComputeViewMatrix ...),
//          so this legacy tier2 header now just forwards to that one.
//
//===========================================================================//

#ifndef CAMERAUTILS_H
#define CAMERAUTILS_H

#ifdef _WIN32
#pragma once
#endif

#include "tier2/tier2.h"
#include "Color.h"
#include "mathlib/camera.h"

#endif // CAMERAUTILS_H
