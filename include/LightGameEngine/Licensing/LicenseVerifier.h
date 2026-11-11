#pragma once

#include <LightGameEngine/Licensing/StudioLicense.h>

#include <map>
#include <string>

namespace lge::licensing
{
	class Ed25519Verifier
	{
	public:
		explicit Ed25519Verifier(std::map<std::string, std::string> publicKeys);
		bool operator()(std::string const& keyId, std::string const& payload, std::string const& signature) const;
	private:
		std::map<std::string, std::string> publicKeys_;
	};
}
