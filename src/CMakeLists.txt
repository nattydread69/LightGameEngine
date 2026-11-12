# Engine modules, lowest layer first. None of them depends on Vulkan or a window.

# Math, clock, fixed timestep, ECS (header-only), input, asset path resolution.
lge_add_library(LightGameCore Core
	SOURCES
		Math/Transform.cpp
		Core/FixedTimestep.cpp
		Core/GameClock.cpp
		Input/InputState.cpp
		Input/ActionMap.cpp
		Assets/AssetManager.cpp
	PUBLIC_DEPS glm::glm)

# 2D collision geometry and queries; independent of the ECS and of rendering.
lge_add_library(LightGamePhysics Physics
	SOURCES
		Physics/Collision2D.cpp
		Physics/CollisionWorld2D.cpp
	PUBLIC_DEPS glm::glm)

# Grid pathfinding and steering helpers.
lge_add_library(LightGameAI AI
	SOURCES
		AI/GridPathfinder.cpp
		AI/Steering.cpp
	PUBLIC_DEPS glm::glm)

# Backend-independent renderer interface, render snapshot types and resource cache.
lge_add_library(LightGameRendering Rendering
	SOURCES
		Rendering/RenderScene.cpp
		Rendering/RenderResourceCache.cpp
	PUBLIC_DEPS glm::glm)

# Game world: components, render extraction, ECS-bound 2D physics.
lge_add_library(LightGameScene Scene
	SOURCES
		Scene/GameWorld.cpp
		Scene/PhysicsSystem2D.cpp
	PUBLIC_DEPS LightGame::Core LightGame::Physics LightGame::Rendering)

# Application loop and platform abstraction (including the headless platform).
lge_add_library(LightGameEngine Engine
	SOURCES
		Engine/GameEngine.cpp
		Platform/HeadlessPlatform.cpp
	PUBLIC_DEPS LightGame::Scene LightGame::AI)

# Licensing is an independent subsystem and does not pull GUI or graphics dependencies.
find_package(OpenSSL REQUIRED COMPONENTS Crypto)
lge_add_library(LightGameLicensing Licensing
	SOURCES
		Licensing/LicenseFeatures.cpp
		Licensing/LicenseManager.cpp
		Licensing/LicenseVerifier.cpp
		Licensing/SignedLicense.cpp
		Licensing/SteamLicense.cpp
		Licensing/GumroadLicense.cpp
		Licensing/StudioLicense.cpp
	PUBLIC_DEPS OpenSSL::Crypto)
