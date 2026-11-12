// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Light Game Engine
// Copyright (C) 2026 and onwards Dr. Nathanael John Inkson
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <LightGameEngine/Licensing/ILicenseProvider.h>
#include <LightGameEngine/Licensing/LicenseVerifier.h>

#include <filesystem>
#include <functional>
#include <map>
#include <optional>

namespace lge::licensing
{
	struct SignedLicenseData
	{
		ProductIdentity product;
		std::string editionId;
		std::string licenseId;
		std::string organizationId;
		std::chrono::system_clock::time_point issuedAt {};
		std::optional<std::chrono::system_clock::time_point> expiresAt;
		std::vector<FeatureId> features;
		std::string keyId;
	};

	class SignedLicense final : public ILicenseProvider
	{
	public:
		using Clock = std::function<std::chrono::system_clock::time_point()>;
		SignedLicense(ProductIdentity expectedProduct, std::map<std::string, std::string> trustedPublicKeys,
			std::filesystem::path storageDirectory = {}, Clock clock = [] { return std::chrono::system_clock::now(); });
		LicenseProviderKind provider() const noexcept override;
		bool available() const noexcept override;
		LicenseResult check() override;
		LicenseResult activate(std::string_view credential) override;
		void deactivate() override;
		void refresh() override;
		LicenseResult verify(std::string_view token) const;
		std::filesystem::path storagePath() const;
		static std::string encodeToken(SignedLicenseData const& data, std::string_view signature);
		static std::string encodePayload(SignedLicenseData const& data);
	private:
		ProductIdentity expectedProduct_;
		std::map<std::string, std::string> trustedPublicKeys_;
		std::filesystem::path storageDirectory_;
		Clock clock_;
		Ed25519Verifier verifier_;
		std::string token_;
		LicenseResult verifyCredential(std::string_view token) const;
	};
}
