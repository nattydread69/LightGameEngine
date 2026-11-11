#pragma once

#include <LightGameEngine/Licensing/ILicenseProvider.h>

#include <functional>

namespace lge::licensing
{
	class SteamLicense final : public ILicenseProvider
	{
	public:
		using OwnershipCheck = std::function<bool()>;
		explicit SteamLicense(OwnershipCheck ownershipCheck);
		LicenseProviderKind provider() const noexcept override;
		bool available() const noexcept override;
		LicenseResult check() override;
		LicenseResult activate(std::string_view) override;
		void deactivate() override;
		void refresh() override;
	private:
		OwnershipCheck ownershipCheck_;
	};
}
