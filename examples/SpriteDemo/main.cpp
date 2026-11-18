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

// SpriteDemo: textured sprites. A procedurally generated 4x2 sprite sheet (no asset files)
// is animated on flat "ground" sprites, billboards stand on a floor with bottom-centre pivots,
// and translucent, overlapping and flipped sprites show the alpha sorting. Sprites come from
// two routes: ECS entities with a SpriteRendererComponent, and RenderSprites pushed straight
// into the scene.
//
// Controls: A/D orbit, W/S zoom, Escape quits.

#include "../Common/ExampleOptions.h"

#include "LightGameEngine/Backends/LightVulkanBackend.h"
#include "LightGameEngine/Engine/GameEngine.h"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <exception>
#include <iostream>
#include <string>

namespace
{

// 4x2 frames of 32x32 pixels: a soft disc with a bright wedge that sweeps round, so frame
// playback is obvious, and a 1-pixel transparent margin so neighbouring cells never bleed.
lge::TextureDescription makeSpinnerSheet()
{
	constexpr int cell = 32;
	constexpr int columns = 4;
	constexpr int rows = 2;
	lge::TextureDescription description;
	description.name = "spinner sheet";
	// Cells are never shrunk, and averaging them would smear neighbouring frames together.
	description.mipmaps = false;
	description.width = cell * columns;
	description.height = cell * rows;
	description.rgba8.assign(static_cast<std::size_t>(description.width) * description.height * 4, 0);
	for (int frame = 0; frame < columns * rows; ++frame)
	{
		float const sweep = glm::two_pi<float>() * static_cast<float>(frame) / static_cast<float>(columns * rows);
		for (int y = 0; y < cell; ++y)
		{
			for (int x = 0; x < cell; ++x)
			{
				float const px = (static_cast<float>(x) + 0.5f - cell * 0.5f) / (cell * 0.5f - 1.0f);
				float const py = (static_cast<float>(y) + 0.5f - cell * 0.5f) / (cell * 0.5f - 1.0f);
				float const radius = std::sqrt(px * px + py * py);
				float const coverage = glm::clamp((1.0f - radius) * 8.0f, 0.0f, 1.0f);
				float const angle = std::atan2(py, px);
				float const wedge = std::max(0.0f, std::cos(angle - sweep));
				float const bright = std::pow(wedge, 6.0f);
				std::size_t const index = (static_cast<std::size_t>((frame / columns) * cell + y) * description.width
					+ static_cast<std::size_t>((frame % columns) * cell + x)) * 4;
				description.rgba8[index + 0] = static_cast<std::uint8_t>(255.0f * glm::mix(0.25f, 1.0f, bright));
				description.rgba8[index + 1] = static_cast<std::uint8_t>(255.0f * glm::mix(0.55f, 1.0f, bright));
				description.rgba8[index + 2] = static_cast<std::uint8_t>(255.0f * glm::mix(0.9f, 1.0f, bright));
				description.rgba8[index + 3] = static_cast<std::uint8_t>(255.0f * coverage);
			}
		}
	}
	return description;
}

// 8x8 pixel-art face, drawn big, so the difference between nearest and linear sampling is obvious.
lge::TextureDescription makePixelArt(lge::TextureFilter const filter)
{
	char const* const rows[8] = {"..yyyy..", ".yyyyyy.", "yyKyyKyy", "yyyyyyyy", "yKyyyyKy", "yyKKKKyy", ".yyyyyy.", "..yyyy.."};
	lge::TextureDescription description;
	description.name = "pixel art";
	description.width = 8;
	description.height = 8;
	description.filter = filter;
	description.mipmaps = false;
	for (char const* row : rows)
	{
		for (int x = 0; x < 8; ++x)
		{
			switch (row[x])
			{
			case 'y': description.rgba8.insert(description.rgba8.end(), {255, 215, 40, 255}); break;
			case 'K': description.rgba8.insert(description.rgba8.end(), {30, 20, 10, 255}); break;
			default: description.rgba8.insert(description.rgba8.end(), {0, 0, 0, 0}); break;
			}
		}
	}
	return description;
}

// A tall pillar with a darker lower half and a pointer on one side, so flipping is visible.
lge::TextureDescription makePillar()
{
	constexpr int width = 32;
	constexpr int height = 64;
	lge::TextureDescription description;
	description.name = "pillar";
	description.width = width;
	description.height = height;
	description.rgba8.assign(static_cast<std::size_t>(width) * height * 4, 0);
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			bool const body = x >= 6 && x < 26 && y >= 4;
			bool const pointer = x >= 26 && y >= 12 && y < 20;
			if (!body && !pointer)
			{
				continue;
			}
			float const shade = y < height / 2 ? 1.0f : 0.6f;
			std::size_t const index = (static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4;
			description.rgba8[index + 0] = static_cast<std::uint8_t>((pointer ? 255.0f : 230.0f) * shade);
			description.rgba8[index + 1] = static_cast<std::uint8_t>((pointer ? 200.0f : 160.0f) * shade);
			description.rgba8[index + 2] = static_cast<std::uint8_t>((pointer ? 60.0f : 90.0f) * shade);
			description.rgba8[index + 3] = 255;
		}
	}
	return description;
}

class SpriteDemo final : public lge::Application
{
public:
	explicit SpriteDemo(lge::examples::FrameBudget budget)
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
		orbitRight_ = actions.bind("OrbitRight", lge::Key::D);
		zoomIn_ = actions.bind("ZoomIn", lge::Key::W);
		zoomOut_ = actions.bind("ZoomOut", lge::Key::S);

		sheet_ = renderer.createTexture(makeSpinnerSheet());
		pillar_ = renderer.createTexture(makePillar());
		crisp_ = renderer.createTexture(makePixelArt(lge::TextureFilter::Nearest));
		soft_ = renderer.createTexture(makePixelArt(lge::TextureFilter::Linear));
		cube_ = renderer.createMesh(lge::MeshDescription::primitive(lge::PrimitiveShape::Cube));
		floorMaterial_ = renderer.createMaterial(lge::MaterialDescription {{0.16f, 0.2f, 0.26f, 1.0f}});

		lge::Transform floorTransform;
		floorTransform.position = {0.0, -0.1, 0.0};
		floorTransform.scale = {18.0, 0.2, 12.0};
		lge::Entity const floor = world.createEntity("Floor", floorTransform);
		world.add<lge::MeshRendererComponent>(floor, lge::MeshRendererComponent {cube_, floorMaterial_});

		// A row of flat spinners lying on the floor, each at its own phase and tint.
		for (int i = 0; i < 7; ++i)
		{
			lge::Transform transform;
			transform.position = {-6.0 + 2.0 * i, 0.02, 2.5};
			lge::Entity const spinner = world.createEntity("Spinner" + std::to_string(i), transform);
			lge::SpriteRendererComponent sprite;
			sprite.texture = sheet_;
			sprite.size = {1.8f, 1.8f};
			sprite.orientation = lge::SpriteOrientation::Ground;
			sprite.rotationRadians = 0.35f * static_cast<float>(i);
			float const hue = static_cast<float>(i) / 7.0f;
			sprite.tint = {0.5f + 0.5f * std::cos(glm::two_pi<float>() * hue), 0.5f + 0.5f * std::cos(glm::two_pi<float>() * (hue + 0.33f)),
				0.5f + 0.5f * std::cos(glm::two_pi<float>() * (hue + 0.66f)), 1.0f};
			world.add<lge::SpriteRendererComponent>(spinner, sprite);
			spinners_.push_back(spinner);
		}

		lge::Transform keyTransform;
		keyTransform.rotation = lge::lookRotation(glm::normalize(glm::dvec3 {-0.4, -1.0, -0.3}), lge::worldUp + glm::dvec3 {0.0, 0.0, 0.1});
		lge::Entity const key = world.createEntity("KeyLight", keyTransform);
		world.add<lge::LightComponent>(key, lge::LightComponent {lge::LightType::Directional, {1.0f, 0.97f, 0.9f}, 0.9f, 50.0f, 0.35f, 0.5f, false});

		camera_ = world.createEntity("Camera");
		world.add<lge::CameraComponent>(camera_, lge::CameraComponent {lge::ProjectionType::Perspective, 50.0, 22.0, 0.1, 200.0});
		world.setActiveCamera(camera_);
		world.environment().clearColor = {0.05f, 0.06f, 0.09f};
		world.environment().ambientLight = {0.3f, 0.3f, 0.34f};
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
		orbitAngle_ += step * (0.1 + 1.0 * actions.axis(orbitLeft_, orbitRight_, input));
		orbitDistance_ = glm::clamp(orbitDistance_ + step * 8.0 * actions.axis(zoomIn_, zoomOut_, input), 6.0, 30.0);
		time_ += step;

		// Frame playback: 8 frames at 10 fps, each spinner a few frames apart.
		lge::GameWorld& world = engine.world();
		for (std::size_t i = 0; i < spinners_.size(); ++i)
		{
			auto const frame = static_cast<std::uint32_t>(time_ * 10.0) + static_cast<std::uint32_t>(i);
			world.get<lge::SpriteRendererComponent>(spinners_[i]).uvRect = lge::atlasFrame(4, 2, frame);
		}
		placeCamera(world);
	}

	void onFrame(lge::GameEngine& engine, double) override
	{
		budget_.update(engine);
	}

	void onBuildRenderScene(lge::GameEngine& engine, lge::RenderScene& scene) override
	{
		// Billboards standing on the floor: a bottom-centre pivot puts their base on the position.
		// The second is mirrored; the three overlapping translucent ones show the back-to-front
		// sorting and the fourth, forced on top with sortOrder, ignores distance.
		auto const pillar = [&](lge::RenderObjectId const id, glm::dvec3 const position, glm::vec4 const tint, bool const flip, int const order, bool const lit = false)
		{
			lge::RenderSprite sprite;
			sprite.id = id;
			sprite.texture = pillar_;
			sprite.position = position;
			sprite.size = {1.6f, 3.2f};
			sprite.pivot = {0.5f, 0.0f};
			sprite.tint = tint;
			sprite.flipX = flip;
			sprite.sortOrder = order;
			sprite.lit = lit;
			scene.sprites.push_back(sprite);
		};
		pillar(1001, {-6.0, 0.0, -2.5}, {1.0f, 1.0f, 1.0f, 1.0f}, false, 0);
		pillar(1002, {-3.5, 0.0, -2.5}, {1.0f, 1.0f, 1.0f, 1.0f}, true, 0);
		pillar(1003, {0.0, 0.0, -2.0}, {1.0f, 0.4f, 0.4f, 0.55f}, false, 0);
		pillar(1004, {0.7, 0.0, -2.8}, {0.4f, 1.0f, 0.5f, 0.55f}, false, 0);
		pillar(1005, {1.4, 0.0, -3.6}, {0.5f, 0.6f, 1.0f, 0.55f}, false, 0);
		pillar(1006, {4.0, 0.0, -2.5}, {1.0f, 1.0f, 1.0f, 0.5f + 0.5f * static_cast<float>(std::sin(time_ * 2.0))}, false, 0);
		pillar(1007, {6.2, 0.0, -2.5}, {1.0f, 1.0f, 0.4f, 1.0f}, false, 5);
		// The same artwork lit by the library's shading (left) beside the unlit default (right).
		pillar(1008, {-8.0, 0.0, -1.0}, {1.0f, 1.0f, 1.0f, 1.0f}, false, 0, true);
		pillar(1009, {-8.0, 0.0, 1.5}, {1.0f, 1.0f, 1.0f, 1.0f}, false, 0, false);

		// Pixel art on the floor: nearest sampling keeps hard texels, linear blurs them.
		for (int i = 0; i < 2; ++i)
		{
			lge::RenderSprite art;
			art.id = 1100 + static_cast<lge::RenderObjectId>(i);
			art.texture = i == 0 ? crisp_ : soft_;
			art.position = {-2.0 + 4.0 * i, 0.03, 5.0};
			art.size = {2.6f, 2.6f};
			art.orientation = lge::SpriteOrientation::Ground;
			scene.sprites.push_back(art);
		}

		lge::RenderStatistics const stats = engine.renderer()->statistics();
		scene.texts.push_back(lge::ScreenText {1,
			"LightGameEngine - SpriteDemo   " + std::to_string(stats.drawnSprites) + " sprites   " + std::to_string(stats.textures) + " textures   " +
				std::to_string(static_cast<int>(engine.timing().framesPerSecond)) + " FPS",
			{24.0f, 24.0f}, 2.5f, {1.0f, 1.0f, 1.0f, 1.0f}});
		scene.texts.push_back(lge::ScreenText {2, "A/D orbit   W/S zoom   Esc quit", {24.0f, 64.0f}, 2.0f, {0.9f, 0.95f, 1.0f, 1.0f}});
	}

	void onShutdown(lge::GameEngine& engine) override
	{
		lge::IRenderer& renderer = *engine.renderer();
		renderer.destroyTexture(sheet_);
		renderer.destroyTexture(pillar_);
		renderer.destroyTexture(crisp_);
		renderer.destroyTexture(soft_);
		renderer.destroyMesh(cube_);
		renderer.destroyMaterial(floorMaterial_);
	}

private:
	void placeCamera(lge::GameWorld& world)
	{
		glm::dvec3 const target {0.0, 0.8, 0.0};
		glm::dvec3 const eye = target + glm::dvec3 {std::sin(orbitAngle_) * 0.5, 0.75, std::cos(orbitAngle_) * 0.9} * orbitDistance_;
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
	lge::TextureHandle sheet_;
	lge::TextureHandle pillar_;
	lge::TextureHandle crisp_;
	lge::TextureHandle soft_;
	lge::MeshHandle cube_;
	lge::MaterialHandle floorMaterial_;
	std::vector<lge::Entity> spinners_;
	lge::Entity camera_;
	double orbitAngle_ = 0.0;
	double orbitDistance_ = 15.0;
	double time_ = 0.0;
};

}

int main(int argc, char** argv)
{
	try
	{
		lge::examples::ExampleOptions const options = lge::examples::parseOptions(argc, argv);
		lge::LightVulkanBackend backend {lge::LightVulkanBackendConfig {"LightGameEngine - SpriteDemo", 1280, 720}};
		lge::GameEngine engine {lge::EngineConfig {"SpriteDemo"}, backend, &backend};
		SpriteDemo app {lge::examples::FrameBudget {options,
			[&backend](std::filesystem::path const& path)
				{
					backend.requestScreenshot(path);
				}}};
		engine.run(app);
		std::cout << "SpriteDemo: " << engine.timing().tickCount << " ticks, " << engine.timing().frameCount
			<< " frames, clean shutdown\n";
		return 0;
	}
	catch (std::exception const& error)
	{
		std::cerr << "SpriteDemo failed: " << error.what() << "\n";
		return 1;
	}
}
