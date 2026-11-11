#include "TestHarness.h"

#include <LightGameEngine/Licensing/GumroadLicense.h>
#include <LightGameEngine/Licensing/LicenseFeatures.h>
#include <LightGameEngine/Licensing/LicenseManager.h>
#include <LightGameEngine/Licensing/SteamLicense.h>

using namespace lge::licensing;
using namespace lge::test;

TestCase const licensingEntitlements {"Licensing: provider precedence and failures", []
{
	auto steam = std::make_shared<SteamLicense>([] { return true; });
	auto gumroad = std::make_shared<GumroadLicense>([](std::string_view, bool increment)
	{
		return GumroadVerification {!increment, !increment, !increment, false, false, false, "sale-1", ""};
	});
	LicenseManager manager {{steam, gumroad}};
	check(manager.edition() == ProductEdition::Pro, "verified Steam entitlement grants Pro");
	check(manager.hasFeature(LicensedFeature::AutomaticRepair), "Pro repair capability");
	check(!manager.hasFeature(LicensedFeature::CIIntegration), "Pro excludes Studio capability");
	steam = std::make_shared<SteamLicense>([] { return false; });
	LicenseManager freeManager {{steam}};
	check(freeManager.edition() == ProductEdition::Free, "missing entitlement remains Free");
	check(freeManager.hasFeature(LicensedFeature::AssetValidation), "Free validation remains available");
	check(!freeManager.hasFeature(LicensedFeature::BatchProcessing), "Free batch processing denied");
}};

TestCase const gumroadErrors {"Licensing: transient Gumroad failure is not invalid", []
{
	GumroadLicense provider {[](std::string_view, bool)
	{
		return GumroadVerification {false, false, false, false, false, false, "", "network unavailable"};
	}};
	auto result = provider.activate("secret-key");
	check(result.status == LicenseStatus::VerificationError, "network failure is distinct");
	check(result.edition == ProductEdition::Free, "failure does not grant Pro");
}};
