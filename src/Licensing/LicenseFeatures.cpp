#include <LightGameEngine/Licensing/LicenseFeatures.h>

#include <algorithm>

namespace lge::licensing
{
	std::vector<LicensedFeature> featuresFor(ProductEdition edition)
	{
		std::vector<LicensedFeature> result {LicensedFeature::AssetValidation, LicensedFeature::BasicReporting};
		if (edition >= ProductEdition::Pro)
		{
			result.insert(result.end(), {LicensedFeature::AutomaticRepair, LicensedFeature::CorrectedAssetExport,
				LicensedFeature::BatchProcessing, LicensedFeature::CustomProfiles, LicensedFeature::AdvancedReporting,
				LicensedFeature::JsonReporting});
		}
		if (edition == ProductEdition::Studio)
		{
			result.insert(result.end(), {LicensedFeature::CommandLineProcessing,
				LicensedFeature::HeadlessProcessing, LicensedFeature::CIIntegration});
		}
		return result;
	}

	bool editionIncludes(ProductEdition edition, LicensedFeature feature) noexcept
	{
		auto const features = featuresFor(edition);
		return std::find(features.begin(), features.end(), feature) != features.end();
	}

	char const* toString(ProductEdition value) noexcept
	{
		switch (value) { case ProductEdition::Pro: return "Pro"; case ProductEdition::Studio: return "Studio"; default: return "Free"; }
	}
	char const* toString(LicenseStatus value) noexcept
	{
		switch (value)
		{
			case LicenseStatus::Valid: return "Valid"; case LicenseStatus::Expired: return "Expired";
			case LicenseStatus::Revoked: return "Revoked"; case LicenseStatus::Invalid: return "Invalid";
			case LicenseStatus::Unavailable: return "Unavailable"; case LicenseStatus::VerificationError: return "VerificationError";
			default: return "NotActivated";
		}
	}
	char const* toString(LicensedFeature value) noexcept
	{
		switch (value)
		{
			case LicensedFeature::AssetValidation: return "AssetValidation"; case LicensedFeature::BasicReporting: return "BasicReporting";
			case LicensedFeature::AutomaticRepair: return "AutomaticRepair"; case LicensedFeature::CorrectedAssetExport: return "CorrectedAssetExport";
			case LicensedFeature::BatchProcessing: return "BatchProcessing"; case LicensedFeature::CustomProfiles: return "CustomProfiles";
			case LicensedFeature::AdvancedReporting: return "AdvancedReporting"; case LicensedFeature::JsonReporting: return "JsonReporting";
			case LicensedFeature::CommandLineProcessing: return "CommandLineProcessing"; case LicensedFeature::HeadlessProcessing: return "HeadlessProcessing";
			case LicensedFeature::CIIntegration: return "CIIntegration";
		}
		return "Unknown";
	}
}
