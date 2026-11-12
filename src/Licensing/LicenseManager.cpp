#include <LightGameEngine/Licensing/LicenseManager.h>
#include <LightGameEngine/Licensing/LicenseFeatures.h>

#include <algorithm>

namespace lge::licensing
{
	LicenseManager::LicenseManager(std::vector<std::shared_ptr<ILicenseProvider>> providers) : providers_ {std::move(providers)} { reload(); }
	LicenseManager::LicenseManager(ProductIdentity product, std::vector<std::shared_ptr<ILicenseProvider>> providers,
		std::string freeEdition, std::vector<FeatureId> freeFeatures, std::vector<std::string> precedence, bool combineProviderFeatures)
		: providers_ {std::move(providers)}, product_ {std::move(product)}, freeEdition_ {std::move(freeEdition)},
		  freeFeatures_ {std::move(freeFeatures)}, editionPrecedence_ {std::move(precedence)}, genericPolicy_ {true},
		  combineProviderFeatures_ {combineProviderFeatures}
	{
		if (editionPrecedence_.empty()) editionPrecedence_.push_back(freeEdition_);
		reload();
	}
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
	bool LicenseManager::hasFeature(std::string const& featureId) const
	{
		for (auto const& feature : freeFeatures_) if (feature == featureId) return true;
		if (combineProviderFeatures_)
		{
			for (auto const& result : results_)
				if (result.status == LicenseStatus::Valid && std::find(result.featureIds.begin(), result.featureIds.end(), featureId) != result.featureIds.end()) return true;
		}
		else
		{
			auto const active = currentLicense();
			if (active.status == LicenseStatus::Valid && std::find(active.featureIds.begin(), active.featureIds.end(), featureId) != active.featureIds.end()) return true;
		}
		return false;
	}
	std::string LicenseManager::editionId() const
	{
		if (!genericPolicy_) return toString(edition());
		std::size_t best = 0;
		std::string selected = freeEdition_;
		for (auto const& result : results_)
		{
			if (result.status != LicenseStatus::Valid) continue;
			auto const found = std::find(editionPrecedence_.begin(), editionPrecedence_.end(), result.editionId);
			if (found != editionPrecedence_.end() && static_cast<std::size_t>(found - editionPrecedence_.begin()) >= best)
			{ best = static_cast<std::size_t>(found - editionPrecedence_.begin()); selected = result.editionId; }
		}
		return selected;
	}
	LicenseResult LicenseManager::currentLicense() const
	{
		if (!genericPolicy_)
			for (auto const& result : results_) if (result.status == LicenseStatus::Valid) return result;
		LicenseResult const* selected = nullptr;
		std::size_t best = 0;
		for (auto const& result : results_)
		{
			if (result.status != LicenseStatus::Valid) continue;
			auto const found = std::find(editionPrecedence_.begin(), editionPrecedence_.end(), result.editionId);
			if (found == editionPrecedence_.end()) { if (!selected) selected = &result; continue; }
			auto const rank = static_cast<std::size_t>(found - editionPrecedence_.begin());
			if (!selected || rank > best) { selected = &result; best = rank; }
		}
		if (selected) return *selected;
		LicenseResult result;
		result.status = status();
		result.productId = product_.productId;
		result.publisherId = product_.publisherId;
		result.editionId = genericPolicy_ ? freeEdition_ : toString(edition());
		return result;
	}
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
