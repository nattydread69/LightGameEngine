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

// BasicWindow: the smallest graphical engine program. It opens a window through the
// LightVulkanGraphics backend, animates the clear colour, shows the window size and
// frame rate as overlay text, reacts to resizing, and exits on Escape.

#include "../Common/ExampleOptions.h"

#include "LightGameEngine/Backends/LightVulkanBackend.h"
#include "LightGameEngine/Engine/GameEngine.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <string>

namespace
{

class BasicWindow final : public lge::Application
{
public:
	explicit BasicWindow(lge::examples::FrameBudget budget)
		: budget_(std::move(budget))
	{
	}

	void onStart(lge::GameEngine& engine) override
	{
		quit_ = engine.actions().bind("Quit", lge::Key::Escape);
	}

	void onFixedUpdate(lge::GameEngine& engine, double) override
	{
		if (engine.actions().wasTriggered(quit_, engine.input()))
		{
			engine.requestQuit();
		}
	}

	void onFrame(lge::GameEngine& engine, double) override
	{
		// A slow colour cycle makes it obvious the window is being redrawn.
		double const t = engine.timing().totalSeconds;
		engine.world().environment().clearColor = glm::vec3 {
			static_cast<float>(0.10 + 0.08 * std::sin(t * 0.7)),
			static_cast<float>(0.12 + 0.08 * std::sin(t * 0.9 + 2.0)),
			static_cast<float>(0.22 + 0.10 * std::sin(t * 0.5 + 4.0))};
		budget_.update(engine);
	}

	void onBuildRenderScene(lge::GameEngine& engine, lge::RenderScene& scene) override
	{
		lge::WindowExtent const extent = engine.extent();
		scene.texts.push_back(lge::ScreenText {1,
			"LightGameEngine - BasicWindow   " + std::to_string(extent.width) + " x " + std::to_string(extent.height) +
				"   " + std::to_string(static_cast<int>(engine.timing().framesPerSecond)) + " FPS",
			{24.0f, 24.0f}, 2.5f, {1.0f, 1.0f, 1.0f, 1.0f}});
		scene.texts.push_back(lge::ScreenText {2, "Resize the window, press Escape to quit", {24.0f, 64.0f}, 2.0f,
			{0.8f, 0.85f, 1.0f, 1.0f}});
	}

private:
	lge::examples::FrameBudget budget_;
	lge::ActionId quit_;
};

}

int main(int argc, char** argv)
{
	try
	{
		lge::examples::ExampleOptions const options = lge::examples::parseOptions(argc, argv);
		lge::LightVulkanBackend backend {lge::LightVulkanBackendConfig {"LightGameEngine - BasicWindow", 1280, 720}};
		lge::GameEngine engine {lge::EngineConfig {"BasicWindow"}, backend, &backend};
		BasicWindow app {lge::examples::FrameBudget {options,
			[&backend](std::filesystem::path const& path)
				{
					backend.requestScreenshot(path);
				}}};
		engine.run(app);
		std::cout << "BasicWindow: " << engine.timing().frameCount << " frames, "
			<< backend.statistics().framesRendered << " rendered, clean shutdown\n";
		return 0;
	}
	catch (std::exception const& error)
	{
		std::cerr << "BasicWindow failed: " << error.what() << "\n";
		return 1;
	}
}
