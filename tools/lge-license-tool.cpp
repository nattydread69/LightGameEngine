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

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
	using namespace lge::licensing;
	std::map<std::string, std::string> arguments(int argc, char** argv, int start)
	{
		std::map<std::string, std::string> result;
		for (int i = start; i + 1 < argc; i += 2) result[argv[i]] = argv[i + 1];
		return result;
	}
	std::string required(std::map<std::string, std::string> const& args, std::string const& name)
	{
		auto const found = args.find(name);
		if (found == args.end() || found->second.empty()) throw std::runtime_error("Missing required argument " + name);
		return found->second;
	}
	std::vector<std::string> split(std::string const& text)
	{
		std::vector<std::string> values;
		std::istringstream stream {text};
		std::string item;
		while (std::getline(stream, item, ',')) if (!item.empty()) values.push_back(item);
		return values;
	}
	std::string issue(std::string const& privatePath, SignedLicenseData const& data)
	{
		BIO* bio = BIO_new_file(privatePath.c_str(), "rb");
		if (!bio) throw std::runtime_error("Could not open private signing key");
		std::unique_ptr<BIO, decltype(&BIO_free)> input {bio, BIO_free};
		std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key {PEM_read_bio_PrivateKey(input.get(), nullptr, nullptr, nullptr), EVP_PKEY_free};
		if (!key || EVP_PKEY_id(key.get()) != EVP_PKEY_ED25519) throw std::runtime_error("Private key must be an Ed25519 PEM key");
		auto const payload = SignedLicense::encodePayload(data);
		std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context {EVP_MD_CTX_new(), EVP_MD_CTX_free};
		if (!context || EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1) throw std::runtime_error("Could not initialise Ed25519 signing");
		size_t size = 0;
		if (EVP_DigestSign(context.get(), nullptr, &size, reinterpret_cast<unsigned char const*>(payload.data()), payload.size()) != 1) throw std::runtime_error("Could not size signature");
		std::string signature(size, '\0');
		if (EVP_DigestSign(context.get(), reinterpret_cast<unsigned char*>(signature.data()), &size,
			reinterpret_cast<unsigned char const*>(payload.data()), payload.size()) != 1) throw std::runtime_error("Could not sign licence");
		signature.resize(size);
		auto token = SignedLicense::encodeToken(data, signature);
		BIO* publicBio = BIO_new(BIO_s_mem());
		if (!publicBio || PEM_write_bio_PUBKEY(publicBio, key.get()) != 1)
		{
			if (publicBio) BIO_free(publicBio);
			throw std::runtime_error("Could not derive public key for issuance self-check");
		}
		BUF_MEM* publicBuffer = nullptr;
		BIO_get_mem_ptr(publicBio, &publicBuffer);
		std::string publicKey {publicBuffer->data, publicBuffer->length};
		BIO_free(publicBio);
		SignedLicense verifier {data.product, {{data.keyId, std::move(publicKey)}}};
		if (verifier.verify(token).status != LicenseStatus::Valid) throw std::runtime_error("Issued licence failed self-verification");
		return token;
	}
	std::string randomId()
	{
		std::array<unsigned char, 16> random {};
		if (RAND_bytes(random.data(), static_cast<int>(random.size())) != 1) throw std::runtime_error("Secure random generation failed");
		static constexpr char hex[] = "0123456789abcdef";
		std::string result;
		for (auto byte : random) { result.push_back(hex[byte >> 4U]); result.push_back(hex[byte & 15U]); }
		return result;
	}
	void generateKeys(std::map<std::string, std::string> const& args)
	{
		auto const privatePath = required(args, "--private");
		auto const publicPath = required(args, "--public");
		if (std::filesystem::exists(privatePath) || std::filesystem::exists(publicPath)) throw std::runtime_error("Refusing to overwrite an existing key file");
		std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> context {EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr), EVP_PKEY_CTX_free};
		EVP_PKEY* rawKey = nullptr;
		if (!context || EVP_PKEY_keygen_init(context.get()) != 1 || EVP_PKEY_keygen(context.get(), &rawKey) != 1) throw std::runtime_error("Ed25519 key generation failed");
		std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key {rawKey, EVP_PKEY_free};
		int privateDescriptor = -1;
#ifdef _WIN32
		privateDescriptor = _open(privatePath.c_str(), _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
#else
		privateDescriptor = open(privatePath.c_str(), O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR);
#endif
		BIO* privateBio = privateDescriptor < 0 ? nullptr : BIO_new_fd(privateDescriptor, BIO_CLOSE);
		BIO* publicBio = BIO_new_file(publicPath.c_str(), "wb");
		if (!privateBio || !publicBio) { if (privateBio) BIO_free(privateBio); else if (privateDescriptor >= 0) {
#ifdef _WIN32
			_close(privateDescriptor);
#else
			close(privateDescriptor);
#endif
		} if (publicBio) BIO_free(publicBio); throw std::runtime_error("Could not create key files"); }
		bool const success = PEM_write_bio_PrivateKey(privateBio, key.get(), nullptr, nullptr, 0, nullptr, nullptr) == 1 && PEM_write_bio_PUBKEY(publicBio, key.get()) == 1;
		BIO_free(privateBio); BIO_free(publicBio);
		if (!success) throw std::runtime_error("Could not securely write Ed25519 keypair");
		std::cout << "Created Ed25519 keypair. Keep " << privatePath << " secret; distribute only " << publicPath << ".\n";
	}
}

int main(int argc, char** argv)
{
	try
	{
		if (argc < 2) throw std::runtime_error("Usage: lge-license-tool generate-keypair|issue|issue-batch|verify [options]");
		std::string const command {argv[1]};
		auto const args = arguments(argc, argv, 2);
		if (command == "generate-keypair") { generateKeys(args); return 0; }
		if (command == "issue" || command == "issue-batch")
		{
			auto const privatePath = required(args, "--private-key");
			auto const product = required(args, "--product");
			auto const edition = required(args, "--edition");
			auto const outputPath = required(args, "--output");
			if (std::filesystem::exists(outputPath)) throw std::runtime_error("Refusing to overwrite an existing output file");
			std::size_t const count = command == "issue-batch" ? static_cast<std::size_t>(std::stoul(required(args, "--count"))) : 1;
			if (count == 0 || count > 100000) throw std::runtime_error("Count must be between 1 and 100000");
			std::vector<std::string> lines;
			std::set<std::string> licenseIds;
			for (std::size_t i = 0; i < count; ++i)
			{
				SignedLicenseData data;
				data.product.productId = product;
				if (args.contains("--publisher")) data.product.publisherId = args.at("--publisher");
				data.editionId = edition;
				do { data.licenseId = randomId(); } while (!licenseIds.insert(data.licenseId).second);
				if (args.contains("--organisation")) data.organizationId = args.at("--organisation");
				data.issuedAt = std::chrono::system_clock::now();
				data.keyId = args.contains("--key-id") ? args.at("--key-id") : "default";
				if (args.contains("--features")) data.features = split(args.at("--features"));
				if (args.contains("--expires-epoch")) data.expiresAt = std::chrono::system_clock::time_point {std::chrono::seconds {std::stoll(args.at("--expires-epoch"))}};
				lines.push_back(issue(privatePath, data));
			}
			std::ofstream output {outputPath, std::ios::binary};
			if (!output) throw std::runtime_error("Could not create output file");
			for (auto const& line : lines) output << line << '\n';
			if (!output) throw std::runtime_error("Could not write licence output");
			std::cout << "Wrote " << count << " offline token(s); token length is " << lines.front().size() << " characters.\n";
			return 0;
		}
		if (command == "verify")
		{
			auto const tokenPath = required(args, "--input");
			auto const publicPath = required(args, "--public-key");
			std::ifstream tokenFile {tokenPath, std::ios::binary};
			std::string token {std::istreambuf_iterator<char> {tokenFile}, {}};
			std::ifstream keyFile {publicPath, std::ios::binary};
			std::string publicKey {std::istreambuf_iterator<char> {keyFile}, {}};
			SignedLicense verifier {{required(args, "--product"), args.contains("--publisher") ? args.at("--publisher") : ""}, {{required(args, "--key-id"), publicKey}}};
			auto const result = verifier.verify(token);
			if (result.status != LicenseStatus::Valid) { std::cerr << result.message << '\n'; return 1; }
			std::cout << "Valid " << result.editionId << " licence " << result.licenseId << "\n";
			return 0;
		}
		throw std::runtime_error("Unknown command: " + command);
	}
	catch (std::exception const& error) { std::cerr << "lge-license-tool: " << error.what() << '\n'; return 2; }
}
