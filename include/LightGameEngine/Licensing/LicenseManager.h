#pragma once

#include <LightGameEngine/Licensing/ILicenseProvider.h>

#include <memory>
#include <string>
#include <vector>

namespace lge::licensing
{
	class LicenseManager
	{
	public:
		explicit LicenseManager(std::vector<std::shared_ptr<ILicenseProvider>> providers);
		LicenseManager(ProductIdentity product, std::vector<std::shared_ptr<ILicenseProvider>> providers,
			std::string freeEdition, std::vector<FeatureId> freeFeatures, std::vector<std::string> editionPrecedence = {},
			bool combineProviderFeatures = false);
		ProductEdition edition() const noexcept;
		bool hasFeature(LicensedFeature feature) const noexcept;
		bool hasFeature(std::string const& featureId) const;
		std::string editionId() const;
		LicenseResult currentLicense() const;
		LicenseStatus status() const noexcept;
		std::vector<LicenseResult> providerStatuses() const;
		LicenseResult activate(LicenseProviderKind provider, std::string_view credential);
		void refresh();
		void deactivate(LicenseProviderKind provider);
	private:
		std::vector<std::shared_ptr<ILicenseProvider>> providers_;
		std::vector<LicenseResult> results_;
		ProductIdentity product_;
		std::string freeEdition_;
		std::vector<FeatureId> freeFeatures_;
		std::vector<std::string> editionPrecedence_;
		bool genericPolicy_ {false};
		bool combineProviderFeatures_ {false};
		void reload();
	};
}
