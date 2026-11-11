#pragma once

#include <LightGameEngine/Licensing/ILicenseProvider.h>

#include <functional>

namespace lge::licensing
{
	struct GumroadVerification
	{
		bool transportSucceeded {false};
		bool success {false};
		bool productMatches {false};
		bool refunded {false};
		bool disputed {false};
		bool subscriptionEnded {false};
		std::string purchaseId;
		std::string error;
	};

	class GumroadLicense final : public ILicenseProvider
	{
	public:
		using Verify = std::function<GumroadVerification(std::string_view key, bool incrementUses)>;
		explicit GumroadLicense(Verify verify);
		LicenseProviderKind provider() const noexcept override;
		bool available() const noexcept override;
		LicenseResult check() override;
		LicenseResult activate(std::string_view licenseKey) override;
		void deactivate() override;
		void refresh() override;
	private:
		Verify verify_;
		std::string key_;
	};
}
