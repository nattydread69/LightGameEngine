#pragma once

#include <LightGameEngine/Licensing/ILicenseProvider.h>

#include <filesystem>
#include <functional>
#include <string>

namespace lge::licensing
{
	using SignatureVerifier = std::function<bool(std::string const& keyId, std::string const& payload, std::string const& signature)>;

	class StudioLicense final : public ILicenseProvider
	{
	public:
		StudioLicense(std::filesystem::path licenseFile, SignatureVerifier verifier, ProductIdentity expectedProduct = {});
		LicenseProviderKind provider() const noexcept override;
		bool available() const noexcept override;
		LicenseResult check() override;
		LicenseResult activate(std::string_view path) override;
		void deactivate() override;
		void refresh() override;
	private:
		std::filesystem::path licenseFile_;
		SignatureVerifier verifier_;
		ProductIdentity expectedProduct_;
	};
}
