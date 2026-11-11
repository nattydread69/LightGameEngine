#include <LightGameEngine/Licensing/LicenseVerifier.h>

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <memory>

namespace lge::licensing
{
	Ed25519Verifier::Ed25519Verifier(std::map<std::string, std::string> publicKeys) : publicKeys_ {std::move(publicKeys)} {}
	bool Ed25519Verifier::operator()(std::string const& keyId, std::string const& payload, std::string const& signature) const
	{
		auto const keyText = publicKeys_.find(keyId);
		if (keyText == publicKeys_.end()) return false;
		std::unique_ptr<BIO, decltype(&BIO_free)> bio {BIO_new_mem_buf(keyText->second.data(), static_cast<int>(keyText->second.size())), BIO_free};
		if (!bio) return false;
		std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key {PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr), EVP_PKEY_free};
		if (!key || EVP_PKEY_id(key.get()) != EVP_PKEY_ED25519) return false;
		std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context {EVP_MD_CTX_new(), EVP_MD_CTX_free};
		if (!context || EVP_DigestVerifyInit(context.get(), nullptr, nullptr, nullptr, key.get()) != 1) return false;
		return EVP_DigestVerify(context.get(), reinterpret_cast<unsigned char const*>(signature.data()), signature.size(),
			reinterpret_cast<unsigned char const*>(payload.data()), payload.size()) == 1;
	}
}
