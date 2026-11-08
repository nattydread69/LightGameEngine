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

// End-to-end integration test: the complete RobotArena game simulated headlessly
// (NullRenderer, virtual time, no window or GPU) with the autopilot playing.

#include "TestHarness.h"

#include "RobotArenaGame.h"

#include "LightGameEngine/Backends/NullRenderer.h"
#include "LightGameEngine/Platform/HeadlessPlatform.h"

using namespace lge;
using namespace lge::test;

namespace
{

struct RunResult
{
	robotarena::ArenaStats stats;
	glm::dvec3 playerPosition {0.0};
	bool allRobotsInside = true;
	std::uint64_t ticks = 0;
	std::size_t maxSubmittedObjects = 0;
	std::size_t rejectedObjects = 0;
	std::size_t meshesAfterShutdown = 0;
	// Statistics reset when a round restarts, so track the best values seen.
	int mostEnemiesDestroyed = 0;
	int mostShotsFired = 0;
	int mostProjectilesAlive = 0;
	bool sawArenaCleared = false;
	bool sawPlayerDestroyed = false;
};

RunResult simulate(std::uint64_t const frames, double const frameSeconds, bool const autopilot)
{
	HeadlessPlatform platform {HeadlessPlatform::Config {frameSeconds, frames, {1440, 900}}};
	NullRenderer renderer;
	robotarena::RobotArenaOptions options;
	options.autopilot = autopilot;
	robotarena::RobotArenaGame game {options};
	RunResult result;
	game.frameHook = [&](GameEngine& engine)
	{
		glm::dvec2 const half = game.arenaHalfExtents();
		engine.world().each<TransformComponent, Collider2DComponent>(
			[&](Entity, TransformComponent const& transform, Collider2DComponent const& collider)
			{
				if (!collider.isStatic && !collider.isTrigger &&
					(std::abs(transform.position.x) > half.x || std::abs(transform.position.z) > half.y))
				{
					result.allRobotsInside = false;
				}
			});
		RenderStatistics const stats = renderer.statistics();
		result.maxSubmittedObjects = std::max(result.maxSubmittedObjects, stats.submittedObjects);
		result.rejectedObjects += stats.rejectedObjects;
		result.stats = game.stats();
		result.mostEnemiesDestroyed = std::max(result.mostEnemiesDestroyed, result.stats.enemiesDestroyed);
		result.mostShotsFired = std::max(result.mostShotsFired, result.stats.shotsFired);
		result.mostProjectilesAlive = std::max(result.mostProjectilesAlive, result.stats.projectilesAlive);
		result.sawArenaCleared = result.sawArenaCleared || result.stats.state == robotarena::ArenaState::ArenaCleared;
		result.sawPlayerDestroyed = result.sawPlayerDestroyed || result.stats.state == robotarena::ArenaState::PlayerDestroyed;
		if (engine.world().isAlive(game.player()))
		{
			result.playerPosition = engine.world().get<TransformComponent>(game.player()).position;
		}
		result.ticks = engine.timing().tickCount;
	};
	{
		GameEngine engine {EngineConfig {"RobotArenaHeadless"}, platform, &renderer};
		engine.run(game);
	}
	result.meshesAfterShutdown = renderer.statistics().meshes + renderer.statistics().materials;
	return result;
}

TestCase const arenaStarts {"RobotArena: arena builds with player, enemies and walls", []
	{
		RunResult const result = simulate(2, 1.0 / 60.0, false);
		checkEqual(result.stats.enemiesRemaining, 6, "six enemies");
		checkEqual(result.stats.playerHealth, 12, "player at full health");
		check(result.maxSubmittedObjects > 30, "floor, walls and robot parts submitted for rendering");
		checkEqual(result.rejectedObjects, std::size_t {0}, "every submitted object has valid resources");
		checkEqual(result.meshesAfterShutdown, std::size_t {0}, "all renderer resources released at shutdown");
	}};

TestCase const autopilotPlays {"RobotArena: autopilot plays a full minute headlessly", []
	{
		RunResult const result = simulate(3600, 1.0 / 60.0, true);
		checkEqual(result.ticks, std::uint64_t {3599}, "fixed 60 Hz ticks");
		check(result.mostShotsFired > 20, "shots were fired");
		check(result.mostEnemiesDestroyed > 0, "projectiles damaged and destroyed enemies");
		check(result.sawArenaCleared || result.sawPlayerDestroyed, "at least one round reached an outcome");
		check(result.allRobotsInside, "no robot ever left the arena (collision against walls works)");
		check(result.mostProjectilesAlive < 40, "projectiles expire");
		checkEqual(result.rejectedObjects, std::size_t {0}, "no invalid render submissions");
	}};

TestCase const deterministic {"RobotArena: simulation is deterministic and independent of frame rate", []
	{
		RunResult const at60 = simulate(1201, 1.0 / 60.0, true);
		RunResult const again = simulate(1201, 1.0 / 60.0, true);
		RunResult const at30 = simulate(601, 1.0 / 30.0, true);
		checkEqual(at60.stats.shotsFired, again.stats.shotsFired, "same shots on a repeat run");
		checkEqual(at60.stats.score, again.stats.score, "same score on a repeat run");
		checkNear(at60.playerPosition.x, again.playerPosition.x, 0.0, "identical player position");
		checkEqual(at30.ticks, at60.ticks, "same number of ticks at 30 FPS");
		checkEqual(at30.stats.shotsFired, at60.stats.shotsFired, "same shots at 30 FPS");
		checkNear(at30.playerPosition.x, at60.playerPosition.x, 1e-9, "same player position at 30 FPS");
		checkNear(at30.playerPosition.z, at60.playerPosition.z, 1e-9, "same player position at 30 FPS (z)");
	}};

}
