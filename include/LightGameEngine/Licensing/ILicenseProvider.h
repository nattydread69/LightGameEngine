#pragma once

#include <LightGameEngine/Licensing/LicenseTypes.h>

#include <string_view>

namespace lge::licensing
{
	class ILicenseProvider
	{
	public:
		virtual ~ILicenseProvider() = default;
		virtual LicenseProviderKind provider() const noexcept = 0;
		virtual bool available() const noexcept = 0;
		virtual LicenseResult check() = 0;
		virtual LicenseResult activate(std::string_view credential) = 0;
		virtual void deactivate() = 0;
		virtual void refresh() = 0;
	};
}
