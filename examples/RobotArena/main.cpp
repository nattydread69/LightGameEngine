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

// RobotArena: playable top-down demonstration on the LightVulkanGraphics backend.
//
// Usage: RobotArena [--autopilot] [--frames N] [--screenshot file.png]

#include "RobotArenaGame.h"

#include "../Common/ExampleOptions.h"

#include "LightGameEngine/Assets/AssetManager.h"
#include "LightGameEngine/Backends/LightVulkanBackend.h"

#include <exception>
#include <iostream>

int main(int argc, char** argv)
{
	try
	{
		lge::examples::ExampleOptions const options = lge::examples::parseOptions(argc, argv);

		robotarena::RobotArenaOptions gameOptions;
		gameOptions.autopilot = options.autopilot;
		try
		{
			lge::AssetManager const assets = lge::AssetManager::locate("textures/arena_floor.png");
			gameOptions.floorTexture = assets.resolve("textures/arena_floor.png");
		}
		catch (lge::AssetError const& error)
		{
			// The game is fully playable without the texture.
			std::cerr << "RobotArena: " << error.what() << " - using an untextured floor\n";
		}

		lge::LightVulkanBackend backend {lge::LightVulkanBackendConfig {"LightGameEngine - RobotArena", 1440, 900}};
		lge::GameEngine engine {lge::EngineConfig {"RobotArena"}, backend, &backend};
		robotarena::RobotArenaGame game {gameOptions};
		lge::examples::FrameBudget budget {options, [&backend](std::filesystem::path const& path)
			{
				backend.requestScreenshot(path);
			}};
		game.frameHook = [&budget](lge::GameEngine& running)
		{
			budget.update(running);
		};

		engine.run(game);

		robotarena::ArenaStats const stats = game.stats();
		std::cout << "RobotArena: " << engine.timing().tickCount << " ticks, " << engine.timing().frameCount << " frames, score "
			<< stats.score << ", enemies destroyed " << stats.enemiesDestroyed << ", shots " << stats.shotsFired << "\n";
		return 0;
	}
	catch (std::exception const& error)
	{
		std::cerr << "RobotArena failed: " << error.what() << "\n";
		return 1;
	}
}
