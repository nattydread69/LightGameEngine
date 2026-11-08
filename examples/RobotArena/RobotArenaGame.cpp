#include "RobotArenaGame.h"

#include "LightGameEngine/AI/Steering.h"
#include "LightGameEngine/Physics/Plane.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace robotarena
{

namespace
{

// '#' wall, 'P' player start, 'E' enemy start, '.' floor. Row 0 is the north edge (-Z).
constexpr std::array<std::string_view, 18> arenaLayout {
	"############################",
	"#P.........#...............#",
	"#..........#...............#",
	"#...####...#....E.....##...#",
	"#...#..............E..##...#",
	"#...#......................#",
	"#.......E.......####.......#",
	"#...............#..........#",
	"######....#.....#.....######",
	"#.........#................#",
	"#.........#.......E........#",
	"#..###....######......##...#",
	"#..#..................##...#",
	"#..#.....E.................#",
	"#.............#............#",
	"#....##.......#.....E...#..#",
	"#.............#.........#..#",
	"############################",
};

constexpr int arenaWidth = 28;
constexpr int arenaHeight = 18;
constexpr double cellSize = 1.0;

// Collision layers.
constexpr lge::CollisionLayers wallLayer = 1u << 0;
constexpr lge::CollisionLayers playerLayer = 1u << 1;
constexpr lge::CollisionLayers enemyLayer = 1u << 2;
constexpr lge::CollisionLayers playerShotLayer = 1u << 3;
constexpr lge::CollisionLayers enemyShotLayer = 1u << 4;

constexpr int playerMaxHealth = 12;
constexpr double playerSpeed = 5.0;
constexpr double playerFireInterval = 0.18;
constexpr double playerShotSpeed = 16.0;
constexpr double enemyShotSpeed = 9.0;
constexpr double enemyFireInterval = 1.1;
constexpr double enemySightRange = 11.0;
constexpr double enemyPreferredRange = 4.5;
constexpr double robotRadius = 0.4;
constexpr double turnRate = 10.0;
constexpr double projectileLifetime = 1.6;

enum class Faction : std::uint8_t
{
	Player,
	Enemy
};

enum class EnemyMode : std::uint8_t
{
	Patrol,
	Chase,
	Search
};

struct RobotComponent
{
	Faction faction = Faction::Enemy;
	int health = 3;
	int maxHealth = 3;
	int variant = 0;
	double speed = 2.5;
	double cooldown = 0.0;
	double yaw = 0.0;
	// The coloured dome shows the robot's type and damage state.
	lge::Entity dome;
};

// Shared by enemies and the autopilot: a cached A* route towards a goal cell.
struct NavigationComponent
{
	std::vector<lge::GridCell> path;
	std::size_t next = 0;
	lge::GridCell goal {-1, -1};
	double repathTimer = 0.0;
};

struct EnemyBrainComponent
{
	EnemyMode mode = EnemyMode::Patrol;
	glm::dvec2 lastKnownTarget {0.0};
	double modeTimer = 0.0;
};

struct ProjectileComponent
{
	Faction owner = Faction::Player;
	double remaining = projectileLifetime;
};

struct WallTag
{
};

struct EffectComponent
{
	double remaining = 0.3;
	double duration = 0.3;
	double startScale = 0.4;
	double endScale = 1.6;
};

// Turns `yaw` towards `target` by at most maxStep radians along the shorter arc.
double turnTowards(double const yaw, double const target, double const maxStep)
{
	double delta = std::remainder(target - yaw, glm::two_pi<double>());
	delta = std::clamp(delta, -maxStep, maxStep);
	return yaw + delta;
}

glm::dvec2 facingDirection(double const yaw)
{
	glm::dvec3 const forward = lge::yawRotation(yaw) * lge::worldForward;
	return lge::toPlane(forward);
}

}

struct RobotArenaGame::Resources
{
	explicit Resources(lge::IRenderer& renderer)
		: cache(renderer)
	{
	}

	lge::RenderResourceCache cache;
	lge::MeshHandle cube;
	lge::MeshHandle sphere;
	lge::MeshHandle cylinder;
	lge::MaterialHandle floor;
	lge::MaterialHandle wall;
	lge::MaterialHandle playerBody;
	lge::MaterialHandle playerDome;
	std::array<lge::MaterialHandle, 2> enemyBody;
	std::array<lge::MaterialHandle, 2> enemyDamaged;
	lge::MaterialHandle enemyDome;
	lge::MaterialHandle visor;
	lge::MaterialHandle playerShot;
	lge::MaterialHandle enemyShot;
	lge::MaterialHandle explosion;
	lge::MaterialHandle spark;
};

struct RobotArenaGame::SpawnRequest
{
	Faction owner;
	glm::dvec2 position;
	glm::dvec2 direction;
};

RobotArenaGame::RobotArenaGame(RobotArenaOptions options)
	: options_(std::move(options))
	, random_(options_.seed)
	, grid_(arenaWidth, arenaHeight)
{
	for (int row = 0; row < arenaHeight; ++row)
	{
		if (arenaLayout[static_cast<std::size_t>(row)].size() != static_cast<std::size_t>(arenaWidth))
		{
			throw std::logic_error("RobotArena layout rows must all have the same width");
		}
		for (int column = 0; column < arenaWidth; ++column)
		{
			char const cell = arenaLayout[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)];
			lge::GridCell const position {column, row};
			grid_.setBlocked(position, cell == '#');
			if (cell == 'P')
			{
				playerSpawn_ = position;
			}
			else if (cell == 'E')
			{
				enemySpawns_.push_back(position);
			}
		}
	}
}

RobotArenaGame::~RobotArenaGame() = default;

glm::dvec2 RobotArenaGame::arenaHalfExtents() const noexcept
{
	return glm::dvec2 {arenaWidth * cellSize * 0.5, arenaHeight * cellSize * 0.5};
}

glm::dvec2 RobotArenaGame::cellCenter(lge::GridCell const cell) const
{
	return glm::dvec2 {(cell.x - arenaWidth * 0.5 + 0.5) * cellSize, (cell.y - arenaHeight * 0.5 + 0.5) * cellSize};
}

lge::GridCell RobotArenaGame::cellAt(glm::dvec2 const position) const
{
	return lge::GridCell {static_cast<int>(std::floor(position.x / cellSize + arenaWidth * 0.5)),
		static_cast<int>(std::floor(position.y / cellSize + arenaHeight * 0.5))};
}

lge::GridCell RobotArenaGame::randomOpenCell()
{
	std::uniform_int_distribution<int> columns {1, arenaWidth - 2};
	std::uniform_int_distribution<int> rows {1, arenaHeight - 2};
	for (int attempt = 0; attempt < 200; ++attempt)
	{
		lge::GridCell const cell {columns(random_), rows(random_)};
		if (!grid_.isBlocked(cell))
		{
			return cell;
		}
	}
	return playerSpawn_;
}

void RobotArenaGame::onStart(lge::GameEngine& engine)
{
	world_ = &engine.world();
	lge::ActionMap& actions = engine.actions();
	moveUp_ = actions.bind("MoveUp", lge::Key::W);
	actions.bind("MoveUp", lge::Key::Up);
	moveDown_ = actions.bind("MoveDown", lge::Key::S);
	actions.bind("MoveDown", lge::Key::Down);
	moveLeft_ = actions.bind("MoveLeft", lge::Key::A);
	actions.bind("MoveLeft", lge::Key::Left);
	moveRight_ = actions.bind("MoveRight", lge::Key::D);
	actions.bind("MoveRight", lge::Key::Right);
	fire_ = actions.bind("Fire", lge::Key::Space);
	actions.bind("Fire", lge::MouseButton::Left);
	restart_ = actions.bind("Restart", lge::Key::R);
	quit_ = actions.bind("Quit", lge::Key::Escape);
	toggleAutopilot_ = actions.bind("ToggleAutopilot", lge::Key::Tab);

	if (lge::IRenderer* const renderer = engine.renderer())
	{
		resources_ = std::make_unique<Resources>(*renderer);
		Resources& r = *resources_;
		r.cube = r.cache.primitive(lge::PrimitiveShape::Cube);
		r.sphere = r.cache.primitive(lge::PrimitiveShape::Sphere);
		r.cylinder = r.cache.primitive(lge::PrimitiveShape::Cylinder);
		lge::MaterialDescription floor {{0.85f, 0.87f, 0.92f, 1.0f}};
		if (!options_.floorTexture.empty())
		{
			floor.texture = options_.floorTexture;
			floor.textureTiling = glm::vec2 {arenaWidth, arenaHeight};
		}
		else
		{
			floor.baseColor = {0.20f, 0.22f, 0.26f, 1.0f};
		}
		r.floor = r.cache.material("arena/floor", floor);
		r.wall = r.cache.material("arena/wall", {{0.34f, 0.42f, 0.55f, 1.0f}});
		r.playerBody = r.cache.material("robot/player-body", {{0.92f, 0.94f, 0.97f, 1.0f}});
		r.playerDome = r.cache.material("robot/player-dome", {{0.25f, 0.75f, 1.0f, 1.0f}});
		r.enemyBody[0] = r.cache.material("robot/sentry-body", {{0.86f, 0.24f, 0.20f, 1.0f}});
		r.enemyBody[1] = r.cache.material("robot/hunter-body", {{0.95f, 0.62f, 0.16f, 1.0f}});
		r.enemyDamaged[0] = r.cache.material("robot/sentry-damaged", {{0.45f, 0.12f, 0.10f, 1.0f}});
		r.enemyDamaged[1] = r.cache.material("robot/hunter-damaged", {{0.50f, 0.32f, 0.08f, 1.0f}});
		r.enemyDome = r.cache.material("robot/enemy-dome", {{0.30f, 0.30f, 0.34f, 1.0f}});
		r.visor = r.cache.material("robot/visor", {{1.0f, 1.0f, 0.75f, 1.0f}, 1.0f});
		r.playerShot = r.cache.material("fx/player-shot", {{0.45f, 0.95f, 1.0f, 1.0f}, 1.0f});
		r.enemyShot = r.cache.material("fx/enemy-shot", {{1.0f, 0.35f, 0.25f, 1.0f}, 1.0f});
		r.explosion = r.cache.material("fx/explosion", {{1.0f, 0.65f, 0.2f, 1.0f}, 1.0f});
		r.spark = r.cache.material("fx/spark", {{1.0f, 0.95f, 0.7f, 1.0f}, 1.0f});
	}
	buildArena(engine);
}

void RobotArenaGame::buildArena(lge::GameEngine& engine)
{
	lge::GameWorld& world = engine.world();
	world.clear();
	pendingSpawns_.clear();
	state_ = ArenaState::Playing;
	score_ = 0;
	enemiesDestroyed_ = 0;
	shotsFired_ = 0;
	stateTimer_ = 0.0;
	random_.seed(options_.seed);
	lge::MeshHandle const cube = resources_ ? resources_->cube : lge::MeshHandle {};

	world.environment().clearColor = {0.03f, 0.035f, 0.05f};
	world.environment().ambientLight = {0.30f, 0.31f, 0.36f};

	// Floor slab: rendering only, no collider.
	lge::Transform floorTransform;
	floorTransform.position = {0.0, -0.05, 0.0};
	floorTransform.scale = {arenaWidth * cellSize, 0.1, arenaHeight * cellSize};
	lge::Entity const floor = world.createEntity("Floor", floorTransform);
	world.add<lge::MeshRendererComponent>(floor, lge::MeshRendererComponent {cube, resources_ ? resources_->floor : lge::MaterialHandle {}});

	// Walls: merge horizontal runs of '#' into single boxes, fewer objects and no
	// seams for robots to snag on.
	for (int row = 0; row < arenaHeight; ++row)
	{
		int column = 0;
		while (column < arenaWidth)
		{
			if (!grid_.isBlocked({column, row}))
			{
				++column;
				continue;
			}
			int const start = column;
			while (column < arenaWidth && grid_.isBlocked({column, row}))
			{
				++column;
			}
			int const length = column - start;
			glm::dvec2 const first = cellCenter({start, row});
			glm::dvec2 const centre {first.x + (length - 1) * cellSize * 0.5, first.y};
			double const wallHeight = 1.1;
			lge::Transform transform;
			transform.position = lge::fromPlane(centre, wallHeight * 0.5);
			transform.scale = {length * cellSize, wallHeight, cellSize};
			lge::Entity const wall = world.createEntity({}, transform);
			world.add<WallTag>(wall);
			world.add<lge::MeshRendererComponent>(wall, lge::MeshRendererComponent {cube, resources_ ? resources_->wall : lge::MaterialHandle {}});
			lge::Collider2DComponent collider;
			collider.shape = lge::ColliderShape2D::box({length * cellSize * 0.5, cellSize * 0.5});
			collider.layer = wallLayer;
			collider.isStatic = true;
			world.add<lge::Collider2DComponent>(wall, collider);
		}
	}

	player_ = spawnRobot(engine, cellCenter(playerSpawn_), true, 0);
	for (std::size_t i = 0; i < enemySpawns_.size(); ++i)
	{
		spawnRobot(engine, cellCenter(enemySpawns_[i]), false, static_cast<int>(i % 2));
	}

	// Key light from the north-west casting shadows, plus the ambient term above.
	lge::Transform sunTransform;
	sunTransform.rotation = lge::lookRotation(glm::normalize(glm::dvec3 {0.45, -1.0, 0.35}), lge::worldUp + glm::dvec3 {0.0, 0.0, -0.2});
	lge::Entity const sun = world.createEntity("Sun", sunTransform);
	world.add<lge::LightComponent>(sun, lge::LightComponent {lge::LightType::Directional, {1.0f, 0.96f, 0.9f}, 0.85f, 60.0f,
		0.35f, 0.5f, true});

	// Orthographic camera looking straight down, with north (-Z) at the top of the screen.
	lge::Transform cameraTransform;
	cameraTransform.position = {0.0, 30.0, 0.0};
	cameraTransform.rotation = lge::lookRotation({0.0, -1.0, 0.0}, {0.0, 0.0, -1.0});
	camera_ = world.createEntity("Camera", cameraTransform);
	world.add<lge::CameraComponent>(camera_, lge::CameraComponent {lge::ProjectionType::Orthographic, 60.0,
		arenaHeight * cellSize + 1.5, 1.0, 80.0});
	world.setActiveCamera(camera_);
}

lge::Entity RobotArenaGame::spawnRobot(lge::GameEngine& engine, glm::dvec2 const position, bool const isPlayer, int const variant)
{
	lge::GameWorld& world = engine.world();
	Resources const* const r = resources_.get();
	double const bodyHeight = 0.55;
	lge::Transform transform;
	transform.position = lge::fromPlane(position, bodyHeight * 0.5);
	transform.scale = {robotRadius * 2.0, bodyHeight, robotRadius * 2.0};
	lge::Entity const body = world.createEntity(isPlayer ? "Player" : "Enemy", transform);

	RobotComponent robot;
	robot.faction = isPlayer ? Faction::Player : Faction::Enemy;
	robot.variant = variant;
	robot.maxHealth = isPlayer ? playerMaxHealth : (variant == 0 ? 3 : 2);
	robot.health = robot.maxHealth;
	robot.speed = isPlayer ? playerSpeed : (variant == 0 ? 2.2 : 3.4);
	robot.yaw = isPlayer ? glm::pi<double>() : 0.0;
	transform.rotation = lge::yawRotation(robot.yaw);
	world.get<lge::TransformComponent>(body).rotation = transform.rotation;
	world.add<RobotComponent>(body, robot);
	world.add<lge::VelocityComponent>(body);
	world.add<NavigationComponent>(body);
	if (!isPlayer)
	{
		EnemyBrainComponent brain;
		// Stagger decisions so enemies do not all re-plan on the same tick.
		brain.modeTimer = std::uniform_real_distribution<double> {0.0, 1.0}(random_);
		world.add<EnemyBrainComponent>(body, brain);
	}

	lge::Collider2DComponent collider;
	collider.shape = lge::ColliderShape2D::circle(robotRadius);
	collider.layer = isPlayer ? playerLayer : enemyLayer;
	collider.mask = isPlayer ? (wallLayer | enemyLayer | enemyShotLayer) : (wallLayer | playerLayer | enemyLayer | playerShotLayer);
	world.add<lge::Collider2DComponent>(body, collider);

	lge::MaterialHandle const bodyMaterial = r ? (isPlayer ? r->playerBody : r->enemyDome) : lge::MaterialHandle {};
	world.add<lge::MeshRendererComponent>(body, lge::MeshRendererComponent {r ? r->cylinder : lge::MeshHandle {}, bodyMaterial});

	// Visual parts follow the body through attachments. Offsets are in the body's
	// scaled local space, so divide by the body scale to get world-unit offsets.
	glm::dvec3 const inverseScale = 1.0 / transform.scale;
	lge::Entity const dome = world.createEntity();
	lge::Transform domeLocal;
	domeLocal.position = glm::dvec3 {0.0, 0.32, 0.0} * inverseScale;
	domeLocal.scale = glm::dvec3 {0.62, 0.5, 0.62} * inverseScale;
	world.add<lge::MeshRendererComponent>(dome, lge::MeshRendererComponent {r ? r->sphere : lge::MeshHandle {},
		r ? (isPlayer ? r->playerDome : r->enemyBody[static_cast<std::size_t>(variant)]) : lge::MaterialHandle {}});
	world.attach(dome, body, domeLocal);
	world.get<RobotComponent>(body).dome = dome;

	lge::Entity const visor = world.createEntity();
	lge::Transform visorLocal;
	visorLocal.position = glm::dvec3 {0.0, 0.38, -0.3} * inverseScale;
	visorLocal.scale = glm::dvec3 {0.36, 0.1, 0.12} * inverseScale;
	world.add<lge::MeshRendererComponent>(visor, lge::MeshRendererComponent {r ? r->cube : lge::MeshHandle {}, r ? r->visor : lge::MaterialHandle {}});
	world.attach(visor, body, visorLocal);
	return body;
}

void RobotArenaGame::spawnProjectile(lge::GameEngine& engine, SpawnRequest const& request)
{
	lge::GameWorld& world = engine.world();
	bool const fromPlayer = request.owner == Faction::Player;
	lge::Transform transform;
	transform.position = lge::fromPlane(request.position + request.direction * (robotRadius + 0.2), 0.45);
	transform.scale = glm::dvec3 {fromPlayer ? 0.2 : 0.26};
	lge::Entity const shot = world.createEntity({}, transform);
	world.add<ProjectileComponent>(shot, ProjectileComponent {request.owner, projectileLifetime});
	world.add<lge::VelocityComponent>(shot, lge::VelocityComponent {
		lge::fromPlane(request.direction * (fromPlayer ? playerShotSpeed : enemyShotSpeed), 0.0), {}});
	lge::Collider2DComponent collider;
	collider.shape = lge::ColliderShape2D::circle(0.12);
	collider.layer = fromPlayer ? playerShotLayer : enemyShotLayer;
	collider.mask = fromPlayer ? (wallLayer | enemyLayer) : (wallLayer | playerLayer);
	collider.isTrigger = true;
	world.add<lge::Collider2DComponent>(shot, collider);
	if (resources_)
	{
		world.add<lge::MeshRendererComponent>(shot, lge::MeshRendererComponent {resources_->sphere,
			fromPlayer ? resources_->playerShot : resources_->enemyShot});
	}
	++shotsFired_;
}

void RobotArenaGame::spawnEffect(lge::GameEngine& engine, glm::dvec3 const position, bool const large)
{
	lge::GameWorld& world = engine.world();
	lge::Transform transform;
	transform.position = position;
	lge::Entity const effect = world.createEntity({}, transform);
	EffectComponent component;
	component.duration = large ? 0.45 : 0.15;
	component.remaining = component.duration;
	component.startScale = large ? 0.5 : 0.35;
	component.endScale = large ? 2.2 : 0.05;
	world.get<lge::TransformComponent>(effect).scale = glm::dvec3 {component.startScale};
	world.add<EffectComponent>(effect, component);
	if (resources_)
	{
		world.add<lge::MeshRendererComponent>(effect, lge::MeshRendererComponent {resources_->sphere,
			large ? resources_->explosion : resources_->spark});
	}
}

void RobotArenaGame::onFixedUpdate(lge::GameEngine& engine, double const step)
{
	lge::ActionMap const& actions = engine.actions();
	lge::InputState const& input = engine.input();
	if (actions.wasTriggered(quit_, input))
	{
		engine.requestQuit();
		return;
	}
	if (actions.wasTriggered(toggleAutopilot_, input))
	{
		options_.autopilot = !options_.autopilot;
	}
	if (actions.wasTriggered(restart_, input) && state_ != ArenaState::Playing)
	{
		buildArena(engine);
		return;
	}

	// Collision events come from the previous tick's physics step.
	handleCollisions(engine);
	updatePlayer(engine, step);
	updateEnemies(engine, step);
	updateProjectiles(engine, step);
	updateEffects(engine, step);

	// Entities created during iteration would invalidate the iterators, so spawning
	// is deferred to here.
	for (SpawnRequest const& request : pendingSpawns_)
	{
		spawnProjectile(engine, request);
	}
	pendingSpawns_.clear();

	if (state_ == ArenaState::Playing)
	{
		if (!engine.world().isAlive(player_))
		{
			state_ = ArenaState::PlayerDestroyed;
		}
		else if (engine.world().registry().count<EnemyBrainComponent>() == 0)
		{
			state_ = ArenaState::ArenaCleared;
		}
	}
	else
	{
		stateTimer_ += step;
		// Unattended runs restart on their own so a demo keeps going.
		if (options_.autopilot && stateTimer_ > 3.0)
		{
			buildArena(engine);
		}
	}
}

void RobotArenaGame::handleCollisions(lge::GameEngine& engine)
{
	lge::GameWorld& world = engine.world();
	std::vector<lge::Entity> spent;
	for (lge::CollisionEvent const& event : engine.physics().events())
	{
		for (int side = 0; side < 2; ++side)
		{
			lge::Entity const shot = side == 0 ? event.a : event.b;
			lge::Entity const other = side == 0 ? event.b : event.a;
			ProjectileComponent const* const projectile = world.isAlive(shot) ? world.tryGet<ProjectileComponent>(shot) : nullptr;
			if (projectile == nullptr || !world.isAlive(other) || std::find(spent.begin(), spent.end(), shot) != spent.end())
			{
				continue;
			}
			glm::dvec3 const impact = world.get<lge::TransformComponent>(shot).position;
			if (world.has<WallTag>(other))
			{
				spent.push_back(shot);
				spawnEffect(engine, impact, false);
			}
			else if (RobotComponent const* const robot = world.tryGet<RobotComponent>(other); robot != nullptr && robot->faction != projectile->owner)
			{
				spent.push_back(shot);
				spawnEffect(engine, impact, false);
				damageRobot(engine, other, 1);
			}
		}
	}
	for (lge::Entity const shot : spent)
	{
		world.queueDestroy(shot);
	}
}

void RobotArenaGame::damageRobot(lge::GameEngine& engine, lge::Entity const entity, int const amount)
{
	lge::GameWorld& world = engine.world();
	RobotComponent& robot = world.get<RobotComponent>(entity);
	if (robot.health <= 0)
	{
		return;
	}
	robot.health -= amount;
	if (robot.faction == Faction::Enemy && resources_ && robot.health * 2 <= robot.maxHealth && world.isAlive(robot.dome))
	{
		world.get<lge::MeshRendererComponent>(robot.dome).material = resources_->enemyDamaged[static_cast<std::size_t>(robot.variant)];
	}
	if (robot.health <= 0)
	{
		spawnEffect(engine, world.get<lge::TransformComponent>(entity).position + glm::dvec3 {0.0, 0.3, 0.0}, true);
		if (robot.faction == Faction::Enemy)
		{
			score_ += robot.variant == 0 ? 100 : 150;
			++enemiesDestroyed_;
			// Remove the brain now so the "arena cleared" check this tick sees it.
			world.registry().remove<EnemyBrainComponent>(entity);
		}
		// Attached dome and visor are destroyed with the body.
		world.queueDestroy(entity);
	}
}

bool RobotArenaGame::followPath(lge::GameEngine& engine, lge::Entity const entity, glm::dvec2 const goal,
	double const speed, glm::dvec2& velocity)
{
	lge::GameWorld& world = engine.world();
	NavigationComponent& navigation = world.get<NavigationComponent>(entity);
	glm::dvec2 const position = lge::toPlane(world.get<lge::TransformComponent>(entity).position);
	lge::GridCell const goalCell = cellAt(goal);
	if (navigation.goal != goalCell || navigation.next >= navigation.path.size() || navigation.repathTimer <= 0.0)
	{
		navigation.goal = goalCell;
		navigation.next = 1;
		navigation.repathTimer = 1.0;
		if (!pathfinder_.findPath(grid_, cellAt(position), goalCell, navigation.path))
		{
			navigation.path.clear();
			velocity = glm::dvec2 {0.0};
			return false;
		}
	}
	if (navigation.next >= navigation.path.size())
	{
		velocity = lge::arrive(position, goal, speed, 0.6);
		return glm::length(goal - position) < 0.3;
	}
	glm::dvec2 const waypoint = navigation.next + 1 == navigation.path.size() ? goal : cellCenter(navigation.path[navigation.next]);
	if (glm::length(waypoint - position) < 0.35)
	{
		++navigation.next;
	}
	velocity = lge::seek(position, waypoint, speed);
	return false;
}

void RobotArenaGame::updatePlayer(lge::GameEngine& engine, double const step)
{
	lge::GameWorld& world = engine.world();
	if (!world.isAlive(player_))
	{
		return;
	}
	lge::ActionMap const& actions = engine.actions();
	lge::InputState const& input = engine.input();
	RobotComponent& robot = world.get<RobotComponent>(player_);
	robot.cooldown = std::max(robot.cooldown - step, 0.0);
	world.get<NavigationComponent>(player_).repathTimer -= step;

	// Screen up is north (-Z), so "up" moves along -v in plane coordinates.
	glm::dvec2 move {actions.axis(moveLeft_, moveRight_, input), actions.axis(moveUp_, moveDown_, input)};
	bool fire = actions.isActive(fire_, input);
	if (options_.autopilot)
	{
		updateAutopilot(engine, step, move, fire);
	}
	if (glm::length(move) > 1.0)
	{
		move = glm::normalize(move);
	}
	world.get<lge::VelocityComponent>(player_).linear = lge::fromPlane(move * robot.speed, 0.0);
	if (glm::length(move) > 0.1 && !(options_.autopilot && fire))
	{
		robot.yaw = turnTowards(robot.yaw, lge::yawFromDirection(lge::fromPlane(move)), turnRate * step);
	}
	world.get<lge::TransformComponent>(player_).rotation = lge::yawRotation(robot.yaw);

	if (fire && robot.cooldown <= 0.0 && state_ == ArenaState::Playing)
	{
		glm::dvec2 const position = lge::toPlane(world.get<lge::TransformComponent>(player_).position);
		pendingSpawns_.push_back(SpawnRequest {Faction::Player, position, facingDirection(robot.yaw)});
		robot.cooldown = playerFireInterval;
	}
}

void RobotArenaGame::updateAutopilot(lge::GameEngine& engine, double const step, glm::dvec2& move, bool& fire)
{
	lge::GameWorld& world = engine.world();
	glm::dvec2 const position = lge::toPlane(world.get<lge::TransformComponent>(player_).position);
	lge::Entity target;
	double bestDistance = 1e9;
	world.each<RobotComponent, EnemyBrainComponent, lge::TransformComponent>(
		[&](lge::Entity const entity, RobotComponent const&, EnemyBrainComponent const&, lge::TransformComponent const& transform)
		{
			double const distance = glm::length(lge::toPlane(transform.position) - position);
			if (distance < bestDistance)
			{
				bestDistance = distance;
				target = entity;
			}
		});
	fire = false;
	if (!target.isValid())
	{
		move = glm::dvec2 {0.0};
		return;
	}
	glm::dvec2 const targetPosition = lge::toPlane(world.get<lge::TransformComponent>(target).position);
	RobotComponent& robot = world.get<RobotComponent>(player_);
	if (bestDistance < 9.0 && engine.physics().hasLineOfSight(position, targetPosition, wallLayer))
	{
		// Face the target and keep a moderate distance while shooting.
		double const desiredYaw = lge::yawFromDirection(lge::fromPlane(targetPosition - position));
		robot.yaw = turnTowards(robot.yaw, desiredYaw, turnRate * step);
		fire = std::abs(std::remainder(desiredYaw - robot.yaw, glm::two_pi<double>())) < 0.15;
		glm::dvec2 const away = glm::normalize(position - targetPosition);
		glm::dvec2 const strafe {-away.y, away.x};
		move = (bestDistance < 4.0 ? away : -away * 0.4) + strafe * 0.5;
		return;
	}
	glm::dvec2 velocity {0.0};
	followPath(engine, player_, targetPosition, 1.0, velocity);
	move = velocity;
}

void RobotArenaGame::updateEnemies(lge::GameEngine& engine, double const step)
{
	lge::GameWorld& world = engine.world();
	bool const playerAlive = world.isAlive(player_) && state_ == ArenaState::Playing;
	glm::dvec2 const playerPosition = playerAlive ? lge::toPlane(world.get<lge::TransformComponent>(player_).position) : glm::dvec2 {0.0};

	// Collect first: followPath and line-of-sight queries look components up by
	// entity, which is fine, but keeping the per-enemy logic outside each() keeps the
	// structural-change rules obvious.
	std::vector<lge::Entity> enemies;
	world.each<EnemyBrainComponent>([&](lge::Entity const entity, EnemyBrainComponent const&)
		{
			enemies.push_back(entity);
		});
	std::reverse(enemies.begin(), enemies.end());

	for (lge::Entity const entity : enemies)
	{
		RobotComponent& robot = world.get<RobotComponent>(entity);
		EnemyBrainComponent& brain = world.get<EnemyBrainComponent>(entity);
		NavigationComponent& navigation = world.get<NavigationComponent>(entity);
		glm::dvec2 const position = lge::toPlane(world.get<lge::TransformComponent>(entity).position);
		robot.cooldown = std::max(robot.cooldown - step, 0.0);
		navigation.repathTimer -= step;
		brain.modeTimer -= step;

		double const distance = glm::length(playerPosition - position);
		bool const seesPlayer = playerAlive && distance < enemySightRange &&
			engine.physics().hasLineOfSight(position, playerPosition, wallLayer);
		if (seesPlayer)
		{
			brain.mode = EnemyMode::Chase;
			brain.lastKnownTarget = playerPosition;
		}
		else if (brain.mode == EnemyMode::Chase)
		{
			// Lost sight: go to where the player was last seen.
			brain.mode = EnemyMode::Search;
			brain.modeTimer = 6.0;
		}

		glm::dvec2 velocity {0.0};
		double desiredYaw = robot.yaw;
		switch (brain.mode)
		{
		case EnemyMode::Chase:
		{
			glm::dvec2 const toPlayer = playerPosition - position;
			desiredYaw = lge::yawFromDirection(lge::fromPlane(toPlayer));
			if (distance > enemyPreferredRange)
			{
				velocity = lge::seek(position, playerPosition, robot.speed);
			}
			else if (distance < enemyPreferredRange - 1.5)
			{
				velocity = lge::flee(position, playerPosition, robot.speed * 0.6);
			}
			bool const aimed = std::abs(std::remainder(desiredYaw - robot.yaw, glm::two_pi<double>())) < 0.25;
			if (aimed && robot.cooldown <= 0.0 && distance < enemySightRange * 0.9)
			{
				// A little inaccuracy keeps the enemies fair.
				double const spread = std::uniform_real_distribution<double> {-0.08, 0.08}(random_);
				pendingSpawns_.push_back(SpawnRequest {Faction::Enemy, position, facingDirection(robot.yaw + spread)});
				robot.cooldown = enemyFireInterval * (robot.variant == 0 ? 1.0 : 0.8);
			}
			break;
		}
		case EnemyMode::Search:
		{
			bool const arrived = followPath(engine, entity, brain.lastKnownTarget, robot.speed, velocity);
			if (arrived || brain.modeTimer <= 0.0)
			{
				brain.mode = EnemyMode::Patrol;
				navigation.path.clear();
			}
			break;
		}
		case EnemyMode::Patrol:
		{
			if (navigation.path.empty() || navigation.next >= navigation.path.size() || brain.modeTimer <= 0.0)
			{
				navigation.goal = randomOpenCell();
				navigation.repathTimer = 0.0;
				brain.modeTimer = 8.0;
				navigation.path.clear();
			}
			bool const arrived = followPath(engine, entity, cellCenter(navigation.goal), robot.speed * 0.7, velocity);
			if (arrived)
			{
				navigation.path.clear();
			}
			break;
		}
		}

		if (brain.mode != EnemyMode::Chase && glm::length(velocity) > 0.1)
		{
			desiredYaw = lge::yawFromDirection(lge::fromPlane(velocity));
		}
		robot.yaw = turnTowards(robot.yaw, desiredYaw, turnRate * 0.6 * step);
		world.get<lge::TransformComponent>(entity).rotation = lge::yawRotation(robot.yaw);
		world.get<lge::VelocityComponent>(entity).linear = lge::fromPlane(velocity, 0.0);
	}
}

void RobotArenaGame::updateProjectiles(lge::GameEngine& engine, double const step)
{
	lge::GameWorld& world = engine.world();
	world.each<ProjectileComponent>([&](lge::Entity const entity, ProjectileComponent& projectile)
		{
			projectile.remaining -= step;
			if (projectile.remaining <= 0.0)
			{
				world.queueDestroy(entity);
			}
		});
}

void RobotArenaGame::updateEffects(lge::GameEngine& engine, double const step)
{
	lge::GameWorld& world = engine.world();
	world.each<EffectComponent, lge::TransformComponent>(
		[&](lge::Entity const entity, EffectComponent& effect, lge::TransformComponent& transform)
		{
			effect.remaining -= step;
			double const t = 1.0 - std::max(effect.remaining, 0.0) / effect.duration;
			transform.scale = glm::dvec3 {glm::mix(effect.startScale, effect.endScale, t)};
			if (effect.remaining <= 0.0)
			{
				world.queueDestroy(entity);
			}
		});
}

void RobotArenaGame::onFrame(lge::GameEngine& engine, double)
{
	// Fit the whole arena on screen for any window shape (presentation only).
	lge::WindowExtent const extent = engine.extent();
	if (extent.width > 0 && extent.height > 0 && engine.world().isAlive(camera_))
	{
		double const aspect = static_cast<double>(extent.width) / static_cast<double>(extent.height);
		double const margin = 1.5;
		double const height = std::max(arenaHeight * cellSize + margin, (arenaWidth * cellSize + margin) / aspect);
		// The extra 20% leaves room above and below the arena for the HUD.
		engine.world().get<lge::CameraComponent>(camera_).orthographicHeight = height * 1.2;
	}
	if (frameHook)
	{
		frameHook(engine);
	}
}

void RobotArenaGame::onBuildRenderScene(lge::GameEngine& engine, lge::RenderScene& scene)
{
	lge::WindowExtent const extent = engine.extent();
	// Screen text glyphs advance about glyphAdvance pixels per unit of scale.
	constexpr float glyphAdvance = 6.0f;
	float const scale = std::clamp(static_cast<float>(extent.height) / 360.0f, 1.0f, 4.0f);
	float const line = 10.0f * scale;
	ArenaStats const s = stats();

	std::string health = "HEALTH ";
	for (int i = 0; i < playerMaxHealth; ++i)
	{
		health += i < s.playerHealth ? '#' : '.';
	}
	scene.texts.push_back(lge::ScreenText {1,
		"SCORE " + std::to_string(s.score) + "    " + health + "    ENEMIES " + std::to_string(s.enemiesRemaining) +
			(options_.autopilot ? "    [AUTOPILOT]" : ""),
		{line, line}, scale, {0.9f, 0.97f, 1.0f, 1.0f}});

	std::string const help = "WASD/arrows move   Space fire   Tab autopilot   R restart   Esc quit   " +
		std::to_string(static_cast<int>(engine.timing().framesPerSecond)) + " FPS";
	scene.texts.push_back(lge::ScreenText {2, help, {line, static_cast<float>(extent.height) - line * 2.2f}, scale * 0.75f,
		{0.65f, 0.72f, 0.8f, 1.0f}});

	if (s.state != ArenaState::Playing)
	{
		std::string const message = s.state == ArenaState::ArenaCleared ? "ARENA CLEARED - press R to play again"
			: "ROBOT DESTROYED - press R to restart";
		float const bigScale = scale * 1.6f;
		float const width = static_cast<float>(message.size()) * glyphAdvance * bigScale;
		scene.texts.push_back(lge::ScreenText {3, message,
			{(static_cast<float>(extent.width) - width) * 0.5f, static_cast<float>(extent.height) * 0.45f}, bigScale,
			s.state == ArenaState::ArenaCleared ? glm::vec4 {0.5f, 1.0f, 0.6f, 1.0f} : glm::vec4 {1.0f, 0.45f, 0.35f, 1.0f}});
	}
}

void RobotArenaGame::onShutdown(lge::GameEngine&)
{
	// Releases every mesh and material through the renderer before it goes away.
	resources_.reset();
	world_ = nullptr;
}

ArenaStats RobotArenaGame::stats() const
{
	ArenaStats result;
	result.state = state_;
	result.score = score_;
	result.enemiesDestroyed = enemiesDestroyed_;
	result.shotsFired = shotsFired_;
	if (world_ != nullptr)
	{
		if (RobotComponent const* const robot = world_->isAlive(player_) ? world_->registry().tryGet<RobotComponent>(player_) : nullptr)
		{
			result.playerHealth = std::max(robot->health, 0);
		}
		result.enemiesRemaining = static_cast<int>(world_->registry().count<EnemyBrainComponent>());
		result.projectilesAlive = static_cast<int>(world_->registry().count<ProjectileComponent>());
	}
	return result;
}

}
