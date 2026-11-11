#include <LightGameEngine/Licensing/SteamLicense.h>
#include <LightGameEngine/Licensing/LicenseFeatures.h>

namespace lge::licensing
{
	SteamLicense::SteamLicense(OwnershipCheck check) : ownershipCheck_ {std::move(check)} {}
	LicenseProviderKind SteamLicense::provider() const noexcept { return LicenseProviderKind::Steam; }
	bool SteamLicense::available() const noexcept { return static_cast<bool>(ownershipCheck_); }
	LicenseResult SteamLicense::check()
	{
		LicenseResult result {LicenseProviderKind::Steam, ProductEdition::Free, LicenseStatus::Unavailable};
		if (!available()) return result;
		try
		{
			if (ownershipCheck_()) { result.edition = ProductEdition::Pro; result.status = LicenseStatus::Valid; result.capabilities = featuresFor(ProductEdition::Pro); }
			else { result.status = LicenseStatus::NotActivated; result.message = "Steam entitlement not detected"; }
		}
		catch (...) { result.status = LicenseStatus::VerificationError; result.message = "Steam ownership check failed"; }
		return result;
	}
	LicenseResult SteamLicense::activate(std::string_view) { return check(); }
	void SteamLicense::deactivate() {}
	void SteamLicense::refresh() {}
}
