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

// GettingStarted: the minimal game from docs/getting-started.md. It runs headlessly
// (no window or GPU) so it works everywhere; the documentation shows the three-line
// change that puts it on screen with the LightVulkanGraphics backend.

#include "LightGameEngine/Backends/NullRenderer.h"
#include "LightGameEngine/Engine/GameEngine.h"
#include "LightGameEngine/Platform/HeadlessPlatform.h"

#include <iostream>

class BouncingBall final : public lge::Application
{
public:
	void onStart(lge::GameEngine& engine) override
	{
		lge::IRenderer& renderer = *engine.renderer();
		mesh_ = renderer.createMesh(lge::MeshDescription::primitive(lge::PrimitiveShape::Sphere));
		material_ = renderer.createMaterial(lge::MaterialDescription {{1.0f, 0.4f, 0.2f, 1.0f}});

		lge::GameWorld& world = engine.world();
		ball_ = world.createEntity("Ball", lge::Transform {{0.0, 5.0, 0.0}});
		world.add<lge::MeshRendererComponent>(ball_, lge::MeshRendererComponent {mesh_, material_});
		world.add<lge::VelocityComponent>(ball_);

		lge::Entity const camera = world.createEntity("Camera", lge::Transform {{0.0, 3.0, 12.0}});
		world.add<lge::CameraComponent>(camera);
		world.setActiveCamera(camera);

		jump_ = engine.actions().bind("Jump", lge::Key::Space);
	}

	void onFixedUpdate(lge::GameEngine& engine, double const step) override
	{
		lge::VelocityComponent& velocity = engine.world().get<lge::VelocityComponent>(ball_);
		lge::TransformComponent& transform = engine.world().get<lge::TransformComponent>(ball_);
		velocity.linear.y -= 9.81 * step;
		if (transform.position.y < 0.5 && velocity.linear.y < 0.0)
		{
			// Bounce, losing a little energy.
			transform.position.y = 0.5;
			velocity.linear.y = -velocity.linear.y * 0.8;
			++bounces_;
		}
		if (engine.actions().wasTriggered(jump_, engine.input()))
		{
			velocity.linear.y = 8.0;
		}
	}

	void onShutdown(lge::GameEngine& engine) override
	{
		engine.renderer()->destroyMesh(mesh_);
		engine.renderer()->destroyMaterial(material_);
		std::cout << "The ball bounced " << bounces_ << " times\n";
	}

private:
	lge::MeshHandle mesh_;
	lge::MaterialHandle material_;
	lge::Entity ball_;
	lge::ActionId jump_;
	int bounces_ = 0;
};

int main()
{
	// Ten seconds of virtual time at 60 frames per second.
	lge::HeadlessPlatform platform {lge::HeadlessPlatform::Config {1.0 / 60.0, 600}};
	platform.scheduleKey(300, lge::Key::Space, true);
	lge::NullRenderer renderer;

	lge::GameEngine engine {lge::EngineConfig {"GettingStarted"}, platform, &renderer};
	BouncingBall game;
	engine.run(game);

	std::cout << "Simulated " << engine.timing().simulationSeconds << " s in " << engine.timing().tickCount
		<< " ticks; rendered " << renderer.statistics().framesRendered << " frames\n";
	return 0;
}
