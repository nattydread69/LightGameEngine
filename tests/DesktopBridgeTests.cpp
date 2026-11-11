// SPDX-License-Identifier: LGPL-3.0-or-later
#include <LightGameEngine/Desktop/DesktopApplication.h>
#include <LightGameEngine/Licensing/SteamLicense.h>

#include <memory>

int main()
{
	using namespace lge::licensing;
	lge::DesktopApplication freeApp;
	if (freeApp.licensing().edition() != ProductEdition::Free ||
		!freeApp.licensing().providerStatuses().empty()) return 1;
	bool owned = true;
	auto provider = std::make_shared<SteamLicense>([&owned] { return owned; });
	lge::DesktopApplication app {{provider}};
	if (app.licensing().edition() != ProductEdition::Pro) return 2;
	owned = false;
	app.licensing().refresh();
	if (app.licensing().edition() != ProductEdition::Free) return 3;
	app.setClearColor({0.1f, 0.2f, 0.3f, 1.0f});
	return 0;
}
