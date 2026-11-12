#include "TestHarness.h"

#include <LightGameEngine/Licensing/GumroadLicense.h>
#include <LightGameEngine/Licensing/LicenseFeatures.h>
#include <LightGameEngine/Licensing/LicenseManager.h>
#include <LightGameEngine/Licensing/SteamLicense.h>
#include <LightGameEngine/Licensing/SignedLicense.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

using namespace lge::licensing;
using namespace lge::test;

namespace
{
	struct TestSigningKey
	{
		std::string publicKey;
	};

	std::pair<TestSigningKey, std::string> makeSignedToken(lge::licensing::SignedLicenseData const& data)
	{
		using namespace lge::licensing;
		EVP_PKEY_CTX* context = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
		EVP_PKEY* key = nullptr;
		EVP_PKEY_keygen_init(context);
		EVP_PKEY_keygen(context, &key);
		EVP_PKEY_CTX_free(context);
		BIO* bio = BIO_new(BIO_s_mem());
		PEM_write_bio_PUBKEY(bio, key);
		BUF_MEM* buffer = nullptr;
		BIO_get_mem_ptr(bio, &buffer);
		TestSigningKey signing;
		signing.publicKey.assign(buffer->data, buffer->length);
		BIO_free(bio);
		auto const payload = SignedLicense::encodePayload(data);
		EVP_MD_CTX* md = EVP_MD_CTX_new();
		EVP_DigestSignInit(md, nullptr, nullptr, nullptr, key);
		size_t size = 64;
		std::string signature(size, '\0');
		EVP_DigestSign(md, reinterpret_cast<unsigned char*>(signature.data()), &size,
			reinterpret_cast<unsigned char const*>(payload.data()), payload.size());
		signature.resize(size);
		EVP_MD_CTX_free(md);
		EVP_PKEY_free(key);
		return {std::move(signing), SignedLicense::encodeToken(data, signature)};
	}
}

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

TestCase const genericSignedLicenses {"Licensing: signed offline credentials are product scoped", []
{
	using namespace std::chrono;
	auto const now = system_clock::time_point {seconds {2000000000}};
	SignedLicenseData data;
	data.product = {"example.applicationA", "example.publisher"};
	data.editionId = "professional";
	data.licenseId = "test-license-1";
	data.issuedAt = now - hours {1};
	data.features = {"feature.export", "feature.batch"};
	data.keyId = "test-key";
	auto [signer, token] = makeSignedToken(data);
	std::map<std::string, std::string> keys {{"test-key", signer.publicKey}};
	SignedLicense appA {data.product, keys, {}, [now] { return now; }};
	SignedLicense appB {{"example.applicationB", "example.publisher"}, keys, {}, [now] { return now; }};
	check(appA.verify(token).status == LicenseStatus::Valid, "correct product accepts its signed token");
	check(appB.verify(token).status == LicenseStatus::Invalid, "different product rejects the signed token");
	check(appA.verify(token + "x").status == LicenseStatus::Invalid, "tampered token is rejected");
	LicenseManager manager {data.product, {std::make_shared<SignedLicense>(data.product, keys, std::filesystem::path {}, [now] { return now; })},
		"community", {"feature.read"}, {"community", "professional", "enterprise"}};
	check(manager.activate(LicenseProviderKind::Signed, token).status == LicenseStatus::Valid, "token activates offline");
	check(manager.hasFeature("feature.export"), "signed feature is available");
	check(manager.hasFeature("feature.read"), "application free feature is available");
	check(manager.editionId() == "professional", "edition hierarchy is application configured");
	auto const storage = std::filesystem::temp_directory_path() / "lge-generic-license-test";
	std::filesystem::remove_all(storage);
	{
		SignedLicense stored {data.product, keys, storage, [now] { return now; }};
		check(stored.activate(token).status == LicenseStatus::Valid, "verified token activates into scoped storage");
		std::ifstream beforeFile {stored.storagePath(), std::ios::binary};
		std::string const before {std::istreambuf_iterator<char> {beforeFile}, {}};
		check(stored.activate("lge1.invalid.invalid").status == LicenseStatus::Invalid, "invalid replacement activation fails");
		std::ifstream afterFile {stored.storagePath(), std::ios::binary};
		std::string const after {std::istreambuf_iterator<char> {afterFile}, {}};
		check(before == after, "failed activation preserves the existing credential");
	}
	{
		SignedLicense restarted {data.product, keys, storage, [now] { return now; }};
		check(restarted.check().status == LicenseStatus::Valid, "stored credential verifies after provider restart");
		check(restarted.storagePath() != SignedLicense {{"example.applicationB", "example.publisher"}, keys, storage, [now] { return now; }}.storagePath(), "storage path is product scoped");
		restarted.deactivate();
		check(!std::filesystem::exists(restarted.storagePath()), "local deactivation removes stored credential");
	}
	std::filesystem::remove_all(storage);
	data.expiresAt = now - seconds {1};
	auto [expiredSigner, expiredToken] = makeSignedToken(data);
	SignedLicense expired {data.product, {{"test-key", expiredSigner.publicKey}}, {}, [now] { return now; }};
	check(expired.verify(expiredToken).status == LicenseStatus::Expired, "expired signed entitlement is rejected");
}};
