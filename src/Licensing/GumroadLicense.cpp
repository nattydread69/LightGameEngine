#include <LightGameEngine/Licensing/GumroadLicense.h>
#include <LightGameEngine/Licensing/LicenseFeatures.h>

namespace lge::licensing
{
	GumroadLicense::GumroadLicense(Verify verify) : verify_ {std::move(verify)} {}
	LicenseProviderKind GumroadLicense::provider() const noexcept { return LicenseProviderKind::Gumroad; }
	bool GumroadLicense::available() const noexcept { return static_cast<bool>(verify_); }
	LicenseResult GumroadLicense::check()
	{
		if (key_.empty()) return {LicenseProviderKind::Gumroad, ProductEdition::Free, LicenseStatus::NotActivated};
		return activate(key_);
	}
	LicenseResult GumroadLicense::activate(std::string_view key)
	{
		LicenseResult result {LicenseProviderKind::Gumroad, ProductEdition::Free, LicenseStatus::Unavailable};
		if (!available()) return result;
		GumroadVerification response;
		try { response = verify_(key, false); }
		catch (...) { response.error = "Gumroad verification transport failed"; }
		if (!response.transportSucceeded) { result.status = LicenseStatus::VerificationError; result.message = response.error; return result; }
		if (!response.success || !response.productMatches || response.refunded || response.disputed || response.subscriptionEnded)
		{ result.status = response.refunded || response.disputed || response.subscriptionEnded ? LicenseStatus::Revoked : LicenseStatus::Invalid; result.message = "Gumroad purchase is not eligible"; return result; }
		key_ = key;
		result.edition = ProductEdition::Pro;
		result.status = LicenseStatus::Valid;
		result.licenseId = response.purchaseId;
		result.capabilities = featuresFor(ProductEdition::Pro);
		result.lastVerifiedAt = std::chrono::system_clock::now();
		return result;
	}
	void GumroadLicense::deactivate() { key_.clear(); }
	void GumroadLicense::refresh() { if (!key_.empty()) (void)check(); }
}
