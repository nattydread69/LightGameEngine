#pragma once

#include <LightGameEngine/Licensing/LicenseTypes.h>

#include <vector>

namespace lge::licensing
{
	std::vector<LicensedFeature> featuresFor(ProductEdition edition);
	bool editionIncludes(ProductEdition edition, LicensedFeature feature) noexcept;
}
