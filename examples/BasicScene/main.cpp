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

// BasicScene: entities with transforms, velocities and mesh renderers produce render
// submissions through the abstract IRenderer. Shows built-in primitives, a
// procedurally generated custom mesh (a small height-field terrain), lights, and a
// camera that switches between perspective and orthographic projection.
//
// Controls: A/D or Left/Right orbit, W/S or Up/Down zoom, P toggles projection,
// Escape quits.

#include "../Common/ExampleOptions.h"

#include "LightGameEngine/Backends/LightVulkanBackend.h"
#include "LightGameEngine/Engine/GameEngine.h"

#include <glm/gtc/constants.hpp>

#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <string>

namespace
{

// Gentle rolling hills coloured by height. Pure function of position, so the terrain
// is identical on every run.
lge::MeshData makeTerrain(int const cells, float const size)
{
	auto const height = [](float const x, float const z)
	{
		return 0.6f * std::sin(x * 0.45f) * std::cos(z * 0.35f) + 0.25f * std::sin((x + z) * 0.9f);
	};
	lge::MeshData data;
	float const step = size / static_cast<float>(cells);
	float const half = size * 0.5f;
	for (int j = 0; j <= cells; ++j)
	{
		for (int i = 0; i <= cells; ++i)
		{
			float const x = -half + static_cast<float>(i) * step;
			float const z = -half + static_cast<float>(j) * step;
			float const y = height(x, z);
			// Central differences give a smooth normal.
			float const dx = height(x + 0.01f, z) - height(x - 0.01f, z);
			float const dz = height(x, z + 0.01f) - height(x, z - 0.01f);
			glm::vec3 const normal = glm::normalize(glm::vec3 {-dx / 0.02f, 1.0f, -dz / 0.02f});
			float const t = glm::clamp((y + 0.85f) / 1.7f, 0.0f, 1.0f);
			glm::vec4 const low {0.16f, 0.36f, 0.20f, 1.0f};
			glm::vec4 const high {0.62f, 0.58f, 0.42f, 1.0f};
			data.vertices.push_back(lge::MeshVertex {{x, y, z}, normal, glm::mix(low, high, t)});
		}
	}
	auto const index = [cells](int const i, int const j)
	{
		return static_cast<std::uint32_t>(j * (cells + 1) + i);
	};
	for (int j = 0; j < cells; ++j)
	{
		for (int i = 0; i < cells; ++i)
		{
			// Counter-clockwise when viewed from above (+Y).
			data.indices.insert(data.indices.end(), {index(i, j), index(i, j + 1), index(i + 1, j)});
			data.indices.insert(data.indices.end(), {index(i + 1, j), index(i, j + 1), index(i + 1, j + 1)});
		}
	}
	return data;
}

class BasicScene final : public lge::Application
{
public:
	explicit BasicScene(lge::examples::FrameBudget budget)
		: budget_(std::move(budget))
	{
	}

	void onStart(lge::GameEngine& engine) override
	{
		lge::IRenderer& renderer = *engine.renderer();
		lge::GameWorld& world = engine.world();
		lge::ActionMap& actions = engine.actions();
		quit_ = actions.bind("Quit", lge::Key::Escape);
		orbitLeft_ = actions.bind("OrbitLeft", lge::Key::A);
		actions.bind("OrbitLeft", lge::Key::Left);
		orbitRight_ = actions.bind("OrbitRight", lge::Key::D);
		actions.bind("OrbitRight", lge::Key::Right);
		zoomIn_ = actions.bind("ZoomIn", lge::Key::W);
		actions.bind("ZoomIn", lge::Key::Up);
		zoomOut_ = actions.bind("ZoomOut", lge::Key::S);
		actions.bind("ZoomOut", lge::Key::Down);
		toggleProjection_ = actions.bind("ToggleProjection", lge::Key::P);

		terrainMesh_ = renderer.createMesh(lge::MeshDescription::custom(makeTerrain(48, 24.0f), "terrain"));
		std::array<lge::PrimitiveShape, 5> const shapes {lge::PrimitiveShape::Cube, lge::PrimitiveShape::Sphere,
			lge::PrimitiveShape::Cylinder, lge::PrimitiveShape::Cone, lge::PrimitiveShape::Capsule};
		std::array<glm::vec4, 5> const colors {glm::vec4 {0.90f, 0.30f, 0.25f, 1.0f}, glm::vec4 {0.25f, 0.55f, 0.95f, 1.0f},
			glm::vec4 {0.95f, 0.80f, 0.25f, 1.0f}, glm::vec4 {0.40f, 0.85f, 0.45f, 1.0f}, glm::vec4 {0.80f, 0.45f, 0.90f, 1.0f}};
		for (std::size_t i = 0; i < shapes.size(); ++i)
		{
			meshes_[i] = renderer.createMesh(lge::MeshDescription::primitive(shapes[i]));
			materials_[i] = renderer.createMaterial(lge::MaterialDescription {colors[i]});
		}
		groundMaterial_ = renderer.createMaterial(lge::MaterialDescription {});
		glowMaterial_ = renderer.createMaterial(lge::MaterialDescription {{1.0f, 0.85f, 0.5f, 1.0f}, 1.0f});

		lge::Entity const terrain = world.createEntity("Terrain");
		world.add<lge::MeshRendererComponent>(terrain, lge::MeshRendererComponent {terrainMesh_, groundMaterial_});

		// A ring of independently positioned and animated primitives.
		for (int i = 0; i < 10; ++i)
		{
			double const angle = glm::two_pi<double>() * i / 10.0;
			lge::Transform transform;
			transform.position = {6.0 * std::cos(angle), 2.0, 6.0 * std::sin(angle)};
			transform.scale = glm::dvec3 {1.2};
			lge::Entity const shape = world.createEntity("Shape" + std::to_string(i), transform);
			std::size_t const kind = static_cast<std::size_t>(i) % shapes.size();
			world.add<lge::MeshRendererComponent>(shape, lge::MeshRendererComponent {meshes_[kind], materials_[kind]});
			// Spin about a tilted axis at a per-object rate.
			world.add<lge::VelocityComponent>(shape, lge::VelocityComponent {{0.0, 0.0, 0.0},
				glm::normalize(glm::dvec3 {0.3, 1.0, 0.2 * i}) * (0.6 + 0.15 * i)});
			bobbers_.push_back(shape);
		}

		// A glowing "sun" that orbits the centre, with a point light attached to it.
		lge::Transform sunTransform;
		sunTransform.scale = glm::dvec3 {0.6};
		sun_ = world.createEntity("Sun", sunTransform);
		world.add<lge::MeshRendererComponent>(sun_, lge::MeshRendererComponent {meshes_[1], glowMaterial_});
		lge::Entity const sunLight = world.createEntity("SunLight");
		world.add<lge::LightComponent>(sunLight, lge::LightComponent {lge::LightType::Point, {1.0f, 0.8f, 0.5f}, 3.0f, 12.0f});
		world.attach(sunLight, sun_);

		lge::Transform keyTransform;
		keyTransform.rotation = lge::lookRotation(glm::normalize(glm::dvec3 {-0.4, -1.0, -0.3}), lge::worldUp + glm::dvec3 {0.0, 0.0, 0.1});
		lge::Entity const key = world.createEntity("KeyLight", keyTransform);
		world.add<lge::LightComponent>(key, lge::LightComponent {lge::LightType::Directional, {1.0f, 0.97f, 0.9f}, 0.9f,
			50.0f, 0.35f, 0.5f, true});

		camera_ = world.createEntity("Camera");
		world.add<lge::CameraComponent>(camera_, lge::CameraComponent {lge::ProjectionType::Perspective, 55.0, 22.0, 0.1, 200.0});
		world.setActiveCamera(camera_);
		world.environment().clearColor = {0.55f, 0.68f, 0.82f};
		world.environment().ambientLight = {0.25f, 0.27f, 0.32f};
		placeCamera(world);
	}

	void onFixedUpdate(lge::GameEngine& engine, double const step) override
	{
		lge::ActionMap const& actions = engine.actions();
		lge::InputState const& input = engine.input();
		if (actions.wasTriggered(quit_, input))
		{
			engine.requestQuit();
		}
		if (actions.wasTriggered(toggleProjection_, input))
		{
			lge::CameraComponent& camera = engine.world().get<lge::CameraComponent>(camera_);
			camera.projection = camera.projection == lge::ProjectionType::Perspective ? lge::ProjectionType::Orthographic
				: lge::ProjectionType::Perspective;
		}
		// Slow automatic orbit plus manual control.
		orbitAngle_ += step * (0.15 + 1.2 * actions.axis(orbitLeft_, orbitRight_, input));
		orbitDistance_ = glm::clamp(orbitDistance_ + step * 10.0 * actions.axis(zoomIn_, zoomOut_, input), 8.0, 40.0);
		engine.world().get<lge::CameraComponent>(camera_).orthographicHeight = orbitDistance_ * 0.9;

		time_ += step;
		lge::GameWorld& world = engine.world();
		for (std::size_t i = 0; i < bobbers_.size(); ++i)
		{
			world.get<lge::TransformComponent>(bobbers_[i]).position.y = 2.0 + 0.6 * std::sin(time_ * 1.5 + static_cast<double>(i));
		}
		lge::TransformComponent& sun = world.get<lge::TransformComponent>(sun_);
		sun.position = {3.0 * std::cos(time_ * 0.8), 4.0, 3.0 * std::sin(time_ * 0.8)};
		placeCamera(world);
	}

	void onFrame(lge::GameEngine& engine, double) override
	{
		budget_.update(engine);
	}

	void onBuildRenderScene(lge::GameEngine& engine, lge::RenderScene& scene) override
	{
		lge::RenderStatistics const stats = engine.renderer()->statistics();
		bool const perspective = engine.world().get<lge::CameraComponent>(camera_).projection == lge::ProjectionType::Perspective;
		scene.texts.push_back(lge::ScreenText {1,
			std::string {"LightGameEngine - BasicScene   "} + (perspective ? "perspective" : "orthographic") + "   " +
				std::to_string(stats.drawnObjects) + " objects   " + std::to_string(stats.lights) + " lights   " +
				std::to_string(static_cast<int>(engine.timing().framesPerSecond)) + " FPS",
			{24.0f, 24.0f}, 2.5f, {1.0f, 1.0f, 1.0f, 1.0f}});
		scene.texts.push_back(lge::ScreenText {2, "A/D orbit   W/S zoom   P projection   Esc quit", {24.0f, 64.0f}, 2.0f,
			{0.9f, 0.95f, 1.0f, 1.0f}});
	}

	void onShutdown(lge::GameEngine& engine) override
	{
		lge::IRenderer& renderer = *engine.renderer();
		renderer.destroyMesh(terrainMesh_);
		for (std::size_t i = 0; i < meshes_.size(); ++i)
		{
			renderer.destroyMesh(meshes_[i]);
			renderer.destroyMaterial(materials_[i]);
		}
		renderer.destroyMaterial(groundMaterial_);
		renderer.destroyMaterial(glowMaterial_);
	}

private:
	void placeCamera(lge::GameWorld& world)
	{
		glm::dvec3 const target {0.0, 1.0, 0.0};
		glm::dvec3 const eye = target + glm::dvec3 {std::cos(orbitAngle_), 0.55, std::sin(orbitAngle_)} * orbitDistance_;
		lge::TransformComponent& transform = world.get<lge::TransformComponent>(camera_);
		transform.position = eye;
		transform.rotation = lge::lookRotation(target - eye, lge::worldUp);
	}

	lge::examples::FrameBudget budget_;
	lge::ActionId quit_;
	lge::ActionId orbitLeft_;
	lge::ActionId orbitRight_;
	lge::ActionId zoomIn_;
	lge::ActionId zoomOut_;
	lge::ActionId toggleProjection_;
	lge::MeshHandle terrainMesh_;
	std::array<lge::MeshHandle, 5> meshes_;
	std::array<lge::MaterialHandle, 5> materials_;
	lge::MaterialHandle groundMaterial_;
	lge::MaterialHandle glowMaterial_;
	std::vector<lge::Entity> bobbers_;
	lge::Entity sun_;
	lge::Entity camera_;
	double orbitAngle_ = 0.6;
	double orbitDistance_ = 18.0;
	double time_ = 0.0;
};

}

int main(int argc, char** argv)
{
	try
	{
		lge::examples::ExampleOptions const options = lge::examples::parseOptions(argc, argv);
		lge::LightVulkanBackend backend {lge::LightVulkanBackendConfig {"LightGameEngine - BasicScene", 1280, 720}};
		lge::GameEngine engine {lge::EngineConfig {"BasicScene"}, backend, &backend};
		BasicScene app {lge::examples::FrameBudget {options,
			[&backend](std::filesystem::path const& path)
				{
					backend.requestScreenshot(path);
				}}};
		engine.run(app);
		std::cout << "BasicScene: " << engine.timing().tickCount << " ticks, " << engine.timing().frameCount
			<< " frames, clean shutdown\n";
		return 0;
	}
	catch (std::exception const& error)
	{
		std::cerr << "BasicScene failed: " << error.what() << "\n";
		return 1;
	}
}
