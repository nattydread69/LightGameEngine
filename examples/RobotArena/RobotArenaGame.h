#pragma once

// RobotArena: a small original top-down robot shooter used as the engine's end-to-end
// demonstration. Everything here is game code built on the public engine API; none
// of it lives in the engine.

#include "LightGameEngine/AI/GridPathfinder.h"
#include "LightGameEngine/Engine/GameEngine.h"
#include "LightGameEngine/Rendering/RenderResourceCache.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace robotarena
{

struct RobotArenaOptions
{
	// The player robot steers and shoots by itself (demos, automated runs, tests).
	bool autopilot = false;
	std::uint32_t seed = 1234;
	// Resolved path of the floor texture; untextured floor when empty.
	std::filesystem::path floorTexture;
};

enum class ArenaState
{
	Playing,
	PlayerDestroyed,
	ArenaCleared
};

struct ArenaStats
{
	ArenaState state = ArenaState::Playing;
	int score = 0;
	int playerHealth = 0;
	int enemiesRemaining = 0;
	int enemiesDestroyed = 0;
	int shotsFired = 0;
	int projectilesAlive = 0;
};

class RobotArenaGame final : public lge::Application
{
public:
	explicit RobotArenaGame(RobotArenaOptions options);
	~RobotArenaGame() override;

	void onStart(lge::GameEngine& engine) override;
	void onFixedUpdate(lge::GameEngine& engine, double stepSeconds) override;
	void onFrame(lge::GameEngine& engine, double frameSeconds) override;
	void onBuildRenderScene(lge::GameEngine& engine, lge::RenderScene& scene) override;
	void onShutdown(lge::GameEngine& engine) override;

	[[nodiscard]] ArenaStats stats() const;
	[[nodiscard]] lge::Entity player() const noexcept
	{
		return player_;
	}
	// Half extents of the playable area in world units (x, z).
	[[nodiscard]] glm::dvec2 arenaHalfExtents() const noexcept;

	// Called at the end of every onFrame, e.g. to limit the run length.
	std::function<void(lge::GameEngine&)> frameHook;

private:
	struct Resources;
	struct SpawnRequest;

	void buildArena(lge::GameEngine& engine);
	lge::Entity spawnRobot(lge::GameEngine& engine, glm::dvec2 position, bool isPlayer, int variant);
	void spawnProjectile(lge::GameEngine& engine, SpawnRequest const& request);
	void spawnEffect(lge::GameEngine& engine, glm::dvec3 position, bool large);

	void updatePlayer(lge::GameEngine& engine, double step);
	void updateAutopilot(lge::GameEngine& engine, double step, glm::dvec2& move, bool& fire);
	void updateEnemies(lge::GameEngine& engine, double step);
	void updateProjectiles(lge::GameEngine& engine, double step);
	void updateEffects(lge::GameEngine& engine, double step);
	void handleCollisions(lge::GameEngine& engine);
	void damageRobot(lge::GameEngine& engine, lge::Entity robot, int amount);
	bool followPath(lge::GameEngine& engine, lge::Entity robot, glm::dvec2 goal, double speed, glm::dvec2& velocity);

	[[nodiscard]] glm::dvec2 cellCenter(lge::GridCell cell) const;
	[[nodiscard]] lge::GridCell cellAt(glm::dvec2 position) const;
	[[nodiscard]] lge::GridCell randomOpenCell();

	RobotArenaOptions options_;
	std::mt19937 random_;
	std::unique_ptr<Resources> resources_;
	lge::GridMap grid_;
	lge::GridPathfinder pathfinder_;
	std::vector<lge::GridCell> enemySpawns_;
	lge::GridCell playerSpawn_ {1, 1};
	std::vector<SpawnRequest> pendingSpawns_;

	// The engine's world while running; read by stats().
	lge::GameWorld const* world_ = nullptr;
	lge::Entity player_;
	lge::Entity camera_;
	ArenaState state_ = ArenaState::Playing;
	int score_ = 0;
	int enemiesDestroyed_ = 0;
	int shotsFired_ = 0;
	double stateTimer_ = 0.0;

	lge::ActionId moveUp_;
	lge::ActionId moveDown_;
	lge::ActionId moveLeft_;
	lge::ActionId moveRight_;
	lge::ActionId fire_;
	lge::ActionId restart_;
	lge::ActionId quit_;
	lge::ActionId toggleAutopilot_;
};

}
