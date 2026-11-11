#pragma once

#include <LightGameEngine/Licensing/ILicenseProvider.h>

#include <memory>
#include <vector>

namespace lge::licensing
{
	class LicenseManager
	{
	public:
		explicit LicenseManager(std::vector<std::shared_ptr<ILicenseProvider>> providers);
		ProductEdition edition() const noexcept;
		bool hasFeature(LicensedFeature feature) const noexcept;
		LicenseStatus status() const noexcept;
		std::vector<LicenseResult> providerStatuses() const;
		LicenseResult activate(LicenseProviderKind provider, std::string_view credential);
		void refresh();
		void deactivate(LicenseProviderKind provider);
	private:
		std::vector<std::shared_ptr<ILicenseProvider>> providers_;
		std::vector<LicenseResult> results_;
		void reload();
	};
}
