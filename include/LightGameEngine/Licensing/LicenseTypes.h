#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace lge::licensing
{
	enum class ProductEdition { Free, Pro, Studio };
	enum class LicensedFeature
	{
		AssetValidation, BasicReporting, AutomaticRepair, CorrectedAssetExport,
		BatchProcessing, CustomProfiles, AdvancedReporting, JsonReporting,
		CommandLineProcessing, HeadlessProcessing, CIIntegration
	};
	enum class LicenseStatus { Valid, NotActivated, Expired, Revoked, Invalid, Unavailable, VerificationError };
	enum class LicenseProviderKind { None, Steam, Gumroad, Studio };

	struct LicenseResult
	{
		LicenseProviderKind provider {LicenseProviderKind::None};
		ProductEdition edition {ProductEdition::Free};
		LicenseStatus status {LicenseStatus::NotActivated};
		std::string licenseId;
		std::string organization;
		std::chrono::system_clock::time_point expiresAt {};
		std::chrono::system_clock::time_point lastVerifiedAt {};
		std::vector<LicensedFeature> capabilities;
		std::string message;
	};

	char const* toString(ProductEdition value) noexcept;
	char const* toString(LicenseStatus value) noexcept;
	char const* toString(LicensedFeature value) noexcept;
}
