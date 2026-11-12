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

#include <LightGameEngine/Licensing/SignedLicense.h>

#include <openssl/sha.h>

#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <system_error>

namespace lge::licensing
{
	namespace
	{
		constexpr std::string_view alphabet {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"};
		constexpr std::size_t maxCredentialSize {16384};
		bool readCredentialFile(std::filesystem::path const& path, std::string& value)
		{
			std::ifstream input {path, std::ios::binary};
			if (!input) return false;
			std::array<char, maxCredentialSize + 1> buffer {};
			input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
			auto const count = static_cast<std::size_t>(input.gcount());
			if (count > maxCredentialSize) return false;
			value.assign(buffer.data(), count);
			return true;
		}

		void appendU32(std::string& out, std::uint32_t value)
		{
			for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<char>((value >> shift) & 0xffU));
		}
		void appendI64(std::string& out, std::int64_t value)
		{
			auto const bits = static_cast<std::uint64_t>(value);
			for (int shift = 56; shift >= 0; shift -= 8) out.push_back(static_cast<char>((bits >> shift) & 0xffU));
		}
		void appendString(std::string& out, std::string_view value)
		{
			appendU32(out, static_cast<std::uint32_t>(value.size()));
			out.append(value);
		}
		bool readU32(std::string_view data, std::size_t& at, std::uint32_t& value)
		{
			if (data.size() - at < 4) return false;
			value = 0;
			for (int i = 0; i < 4; ++i) value = (value << 8U) | static_cast<unsigned char>(data[at++]);
			return true;
		}
		bool readI64(std::string_view data, std::size_t& at, std::int64_t& value)
		{
			if (data.size() - at < 8) return false;
			std::uint64_t bits = 0;
			for (int i = 0; i < 8; ++i) bits = (bits << 8U) | static_cast<unsigned char>(data[at++]);
			value = static_cast<std::int64_t>(bits);
			return true;
		}
		bool readString(std::string_view data, std::size_t& at, std::string& value)
		{
			std::uint32_t size = 0;
			if (!readU32(data, at, size) || size > 4096 || data.size() - at < size) return false;
			value.assign(data.substr(at, size));
			at += size;
			return true;
		}
		std::string base64(std::string_view input)
		{
			std::string out;
			std::uint32_t accumulator = 0;
			int bits = 0;
		for (char rawByte : input)
			{
				auto const byte = static_cast<unsigned char>(rawByte);
				accumulator = (accumulator << 8U) | byte;
				bits += 8;
				while (bits >= 6) { bits -= 6; out.push_back(alphabet[(accumulator >> bits) & 63U]); }
			}
			if (bits > 0) out.push_back(alphabet[(accumulator << (6 - bits)) & 63U]);
			return out;
		}
		bool unbase64(std::string_view input, std::string& out)
		{
			std::uint32_t accumulator = 0;
			int bits = 0;
		for (char rawByte : input)
			{
				auto const byte = static_cast<unsigned char>(rawByte);
				auto const found = alphabet.find(static_cast<char>(byte));
				if (found == std::string_view::npos) return false;
				accumulator = (accumulator << 6U) | static_cast<std::uint32_t>(found);
				bits += 6;
				if (bits >= 8) { bits -= 8; out.push_back(static_cast<char>((accumulator >> bits) & 0xffU)); }
			}
			return bits < 6 && (bits == 0 || (accumulator & ((1U << bits) - 1U)) == 0);
		}
		LicenseResult failed(LicenseStatus status, std::string message)
		{
			LicenseResult result;
			result.provider = LicenseProviderKind::Signed;
			result.status = status;
			result.message = std::move(message);
			return result;
		}
	}

	SignedLicense::SignedLicense(ProductIdentity product, std::map<std::string, std::string> keys,
		std::filesystem::path directory, Clock clock)
		: expectedProduct_ {std::move(product)}, trustedPublicKeys_ {std::move(keys)}, storageDirectory_ {std::move(directory)},
		  clock_ {std::move(clock)}, verifier_ {trustedPublicKeys_}
	{}
	LicenseProviderKind SignedLicense::provider() const noexcept { return LicenseProviderKind::Signed; }
	bool SignedLicense::available() const noexcept { return !expectedProduct_.productId.empty() && !trustedPublicKeys_.empty(); }
	std::filesystem::path SignedLicense::storagePath() const
	{
		std::string identity = expectedProduct_.productId;
		identity.push_back('\0');
		identity.append(expectedProduct_.publisherId);
		std::array<unsigned char, SHA256_DIGEST_LENGTH> digest {};
		SHA256(reinterpret_cast<unsigned char const*>(identity.data()), identity.size(), digest.data());
		static constexpr char digits[] = "0123456789abcdef";
		std::string fileName;
		fileName.reserve(digest.size() * 2 + 12);
		for (unsigned char byte : digest) { fileName.push_back(digits[byte >> 4U]); fileName.push_back(digits[byte & 15U]); }
		return storageDirectory_ / (fileName + ".lge-license");
	}
	std::string SignedLicense::encodePayload(SignedLicenseData const& data)
	{
		std::string out {"LGE-SIGNED-LICENSE\1"};
		appendString(out, data.product.productId);
		appendString(out, data.product.publisherId);
		appendString(out, data.editionId);
		appendString(out, data.licenseId);
		appendString(out, data.organizationId);
		appendI64(out, std::chrono::duration_cast<std::chrono::seconds>(data.issuedAt.time_since_epoch()).count());
		appendI64(out, data.expiresAt ? std::chrono::duration_cast<std::chrono::seconds>(data.expiresAt->time_since_epoch()).count() : std::numeric_limits<std::int64_t>::max());
		appendString(out, data.keyId);
		appendU32(out, static_cast<std::uint32_t>(data.features.size()));
		for (auto const& feature : data.features) appendString(out, feature);
		return out;
	}
	std::string SignedLicense::encodeToken(SignedLicenseData const& data, std::string_view signature)
	{
		return "lge1." + base64(encodePayload(data)) + "." + base64(signature);
	}
	LicenseResult SignedLicense::verify(std::string_view token) const { return verifyCredential(token); }
	LicenseResult SignedLicense::verifyCredential(std::string_view token) const
	{
		if (!available()) return failed(LicenseStatus::Unavailable, "No product identity or trusted public keys configured");
		if (token.size() > maxCredentialSize) return failed(LicenseStatus::Invalid, "Licence credential is too large");
		while (!token.empty() && (token.front() == ' ' || token.front() == '\t' || token.front() == '\r' || token.front() == '\n')) token.remove_prefix(1);
		while (!token.empty() && (token.back() == ' ' || token.back() == '\t' || token.back() == '\r' || token.back() == '\n')) token.remove_suffix(1);
		if (!token.starts_with("lge1.")) return failed(LicenseStatus::Invalid, "Unsupported licence token format");
		auto const separator = token.find('.', 5);
		if (separator == std::string_view::npos || token.find('.', separator + 1) != std::string_view::npos) return failed(LicenseStatus::Invalid, "Malformed licence token");
		std::string payload, signature;
		if (!unbase64(token.substr(5, separator - 5), payload) || !unbase64(token.substr(separator + 1), signature) || signature.size() != 64)
			return failed(LicenseStatus::Invalid, "Malformed licence encoding or signature");
		if (!payload.starts_with(std::string_view {"LGE-SIGNED-LICENSE\1", 19})) return failed(LicenseStatus::Invalid, "Unsupported signed licence schema");
		std::size_t at = 19;
		SignedLicenseData data;
		std::int64_t issued = 0, expires = 0;
		std::uint32_t count = 0;
		if (!readString(payload, at, data.product.productId) || !readString(payload, at, data.product.publisherId) ||
			!readString(payload, at, data.editionId) || !readString(payload, at, data.licenseId) || !readString(payload, at, data.organizationId) || !readI64(payload, at, issued) ||
			!readI64(payload, at, expires) || !readString(payload, at, data.keyId) || !readU32(payload, at, count) || count > 256)
			return failed(LicenseStatus::Invalid, "Malformed signed licence payload");
		std::set<std::string> unique;
		for (std::uint32_t i = 0; i < count; ++i)
		{
			std::string feature;
			if (!readString(payload, at, feature) || feature.empty() || !unique.insert(feature).second) return failed(LicenseStatus::Invalid, "Malformed or duplicate feature identifier");
			data.features.push_back(std::move(feature));
		}
		if (at != payload.size() || data.product.productId.empty() || data.editionId.empty() || data.licenseId.empty() || data.keyId.empty())
			return failed(LicenseStatus::Invalid, "Signed licence contains invalid fields");
		if (data.product.productId != expectedProduct_.productId || (!expectedProduct_.publisherId.empty() && data.product.publisherId != expectedProduct_.publisherId))
			return failed(LicenseStatus::Invalid, "Licence product identity does not match");
		if (!verifier_(data.keyId, payload, signature)) return failed(LicenseStatus::Invalid, "Licence signature is invalid or its key is untrusted");
		auto const now = clock_();
		data.issuedAt = std::chrono::system_clock::time_point {std::chrono::seconds {issued}};
		if (data.issuedAt > now) return failed(LicenseStatus::Invalid, "Licence issue date is in the future");
		if (expires != std::numeric_limits<std::int64_t>::max())
		{
			data.expiresAt = std::chrono::system_clock::time_point {std::chrono::seconds {expires}};
			if (*data.expiresAt <= data.issuedAt) return failed(LicenseStatus::Invalid, "Licence validity interval is invalid");
			if (now >= *data.expiresAt) return failed(LicenseStatus::Expired, "Licence has expired");
		}
		LicenseResult result;
		result.provider = LicenseProviderKind::Signed;
		result.status = LicenseStatus::Valid;
		result.productId = data.product.productId;
		result.publisherId = data.product.publisherId;
		result.editionId = data.editionId;
		result.licenseId = data.licenseId;
		result.organization = data.organizationId;
		result.expiresAt = data.expiresAt.value_or(std::chrono::system_clock::time_point {});
		result.lastVerifiedAt = now;
		result.featureIds = std::move(data.features);
		return result;
	}
	LicenseResult SignedLicense::check()
	{
		if (!token_.empty()) return verifyCredential(token_);
		if (storageDirectory_.empty()) return failed(LicenseStatus::NotActivated, "No signed licence is stored");
		std::string stored;
		if (!readCredentialFile(storagePath(), stored)) return failed(LicenseStatus::NotActivated, "No readable signed licence is stored, or it exceeds the size limit");
		auto result = verifyCredential(stored);
		if (result.status == LicenseStatus::Valid) token_ = std::move(stored);
		return result;
	}
	LicenseResult SignedLicense::activate(std::string_view credential)
	{
		std::string input {credential};
		if (!input.starts_with("lge1."))
		{
			std::string fileContents;
			if (!readCredentialFile(std::filesystem::path {input}, fileContents)) return failed(LicenseStatus::Invalid, "Credential is not a token, or the licence file could not be read within the size limit");
			input = std::move(fileContents);
		}
		auto result = verifyCredential(input);
		if (result.status != LicenseStatus::Valid) return result;
		if (!storageDirectory_.empty())
		{
			std::error_code error;
			std::filesystem::create_directories(storageDirectory_, error);
			if (error) return failed(LicenseStatus::VerificationError, "Could not create licence storage directory");
			std::filesystem::permissions(storageDirectory_, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, error);
			if (error) return failed(LicenseStatus::VerificationError, "Could not secure licence storage directory");
			auto const path = storagePath();
			auto const temporary = path.string() + ".tmp";
			{ std::ofstream output {temporary, std::ios::binary | std::ios::trunc}; if (!output) return failed(LicenseStatus::VerificationError, "Could not persist licence");
				std::filesystem::permissions(temporary, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace, error);
				if (error) return failed(LicenseStatus::VerificationError, "Could not secure stored licence");
				output << input; if (!output) return failed(LicenseStatus::VerificationError, "Could not persist licence"); }
			std::filesystem::rename(temporary, path, error);
			if (error) { std::filesystem::remove(temporary); return failed(LicenseStatus::VerificationError, "Could not replace stored licence"); }
		}
		token_ = std::move(input);
		return result;
	}
	void SignedLicense::deactivate()
	{
		token_.clear();
		if (!storageDirectory_.empty()) { std::error_code error; std::filesystem::remove(storagePath(), error); }
	}
	void SignedLicense::refresh() { (void)check(); }
}
