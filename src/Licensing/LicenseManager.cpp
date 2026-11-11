#include <LightGameEngine/Licensing/LicenseManager.h>
#include <LightGameEngine/Licensing/LicenseFeatures.h>

#include <algorithm>

namespace lge::licensing
{
	LicenseManager::LicenseManager(std::vector<std::shared_ptr<ILicenseProvider>> providers) : providers_ {std::move(providers)} { reload(); }
	void LicenseManager::reload()
	{
		results_.clear();
		for (auto const& provider : providers_)
		{
			try { results_.push_back(provider->check()); }
			catch (...) { results_.push_back({provider->provider(), ProductEdition::Free, LicenseStatus::VerificationError}); }
		}
	}
	ProductEdition LicenseManager::edition() const noexcept
	{
		ProductEdition best {ProductEdition::Free};
		for (auto const& result : results_)
			if (result.status == LicenseStatus::Valid && result.edition > best) best = result.edition;
		return best;
	}
	bool LicenseManager::hasFeature(LicensedFeature feature) const noexcept { return editionIncludes(edition(), feature); }
	LicenseStatus LicenseManager::status() const noexcept
	{
		for (auto const& result : results_) if (result.status == LicenseStatus::Valid) return LicenseStatus::Valid;
		return LicenseStatus::NotActivated;
	}
	std::vector<LicenseResult> LicenseManager::providerStatuses() const { return results_; }
	LicenseResult LicenseManager::activate(LicenseProviderKind kind, std::string_view credential)
	{
		for (auto const& provider : providers_) if (provider->provider() == kind) { auto result = provider->activate(credential); reload(); return result; }
		return {kind, ProductEdition::Free, LicenseStatus::Unavailable, {}, {}, {}, {}, {}, "Provider unavailable"};
	}
	void LicenseManager::refresh() { for (auto const& provider : providers_) provider->refresh(); reload(); }
	void LicenseManager::deactivate(LicenseProviderKind kind) { for (auto const& provider : providers_) if (provider->provider() == kind) provider->deactivate(); reload(); }
}
