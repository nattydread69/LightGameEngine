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
