// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 onwards Dr. Nathanael John Inkson
#pragma once

#include <VkApp.h>

namespace lge
{

// Compatibility bridge for desktop tools that use LVG's immediate scene and UI
// APIs. This owns exactly one LVG application. It does not use GameEngine's world,
// renderer or application loop, and it carries no licence checking: an application
// that sells editions links its own licensing (this library is LGPL and may be replaced).
// As with VkApp, create, use and destroy this object on the same thread.
class DesktopApplication : public lightGraphics::VkApp
{
public:
	DesktopApplication() = default;
	DesktopApplication(DesktopApplication const&) = delete;
	DesktopApplication& operator=(DesktopApplication const&) = delete;
};

}
