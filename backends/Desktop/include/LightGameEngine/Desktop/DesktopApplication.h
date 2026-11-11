// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2026 onwards Dr. Nathanael John Inkson
#pragma once

#include <LightGameEngine/Licensing/LicenseManager.h>
#include <VkApp.h>

#include <memory>
#include <utility>
#include <vector>

namespace lge
{

// Compatibility bridge for desktop tools that use LVG's immediate scene and UI
// APIs. This owns exactly one LVG application and one independent licence manager.
// It does not use GameEngine's world, renderer or application loop.
// As with VkApp, create, use and destroy this object on the same thread.
class DesktopApplication : public lightGraphics::VkApp
{
public:
	using Providers = std::vector<std::shared_ptr<licensing::ILicenseProvider>>;
	explicit DesktopApplication(Providers providers = {}) : licensing_ {std::move(providers)} {}
	DesktopApplication(DesktopApplication const&) = delete;
	DesktopApplication& operator=(DesktopApplication const&) = delete;

	[[nodiscard]] licensing::LicenseManager& licensing() noexcept { return licensing_; }
	[[nodiscard]] licensing::LicenseManager const& licensing() const noexcept { return licensing_; }

private:
	licensing::LicenseManager licensing_;
};

}
