#include <LightGameEngine/Licensing/StudioLicense.h>
#include <LightGameEngine/Licensing/LicenseFeatures.h>

#include <fstream>
#include <iterator>

namespace lge::licensing
{
	StudioLicense::StudioLicense(std::filesystem::path path, SignatureVerifier verifier) : licenseFile_ {std::move(path)}, verifier_ {std::move(verifier)} {}
	LicenseProviderKind StudioLicense::provider() const noexcept { return LicenseProviderKind::Studio; }
	bool StudioLicense::available() const noexcept { return static_cast<bool>(verifier_); }
	LicenseResult StudioLicense::check()
	{
		LicenseResult result {LicenseProviderKind::Studio, ProductEdition::Free, LicenseStatus::NotActivated};
		if (!available()) { result.status = LicenseStatus::Unavailable; return result; }
		std::ifstream input {licenseFile_, std::ios::binary};
		if (!input) return result;
		std::string content {std::istreambuf_iterator<char> {input}, {}};
		if (content.size() > 65536) { result.status = LicenseStatus::Invalid; result.message = "Licence file exceeds 64 KiB"; return result; }
		auto field = [&content](std::string const& key) -> std::string
		{
			auto const marker = "\"" + key + "\":\"";
			auto const start = content.find(marker);
			if (start == std::string::npos) return {};
			auto const valueStart = start + marker.size();
			auto const end = content.find('"', valueStart);
			return end == std::string::npos ? std::string {} : content.substr(valueStart, end - valueStart);
		};
		std::string const payload = field("payload");
		std::string const keyId = field("key_id");
		std::string const signature = field("signature");
		if (payload.empty() || keyId.empty() || signature.empty() || !verifier_(keyId, payload, signature))
		{ result.status = LicenseStatus::Invalid; result.message = "Studio licence signature is invalid"; return result; }
		if (payload.find("\"product\":\"lightGameEngine\"") == std::string::npos || payload.find("\"edition\":\"Studio\"") == std::string::npos)
		{ result.status = LicenseStatus::Invalid; result.message = "Studio licence identity is invalid"; return result; }
		result.status = LicenseStatus::Valid;
		result.edition = ProductEdition::Studio;
		result.licenseId = "signed-studio-license";
		result.capabilities = featuresFor(ProductEdition::Studio);
		result.lastVerifiedAt = std::chrono::system_clock::now();
		return result;
	}
	LicenseResult StudioLicense::activate(std::string_view path) { licenseFile_ = path; return check(); }
	void StudioLicense::deactivate() { std::error_code error; std::filesystem::remove(licenseFile_, error); }
	void StudioLicense::refresh() {}
}
