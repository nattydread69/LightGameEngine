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

#include "LightGameEngine/Engine/GameEngine.h"

#include <exception>
#include <stdexcept>

namespace lge
{

void Application::onStart(GameEngine&)
{
}

void Application::onFrame(GameEngine&, double)
{
}

void Application::onBuildRenderScene(GameEngine&, RenderScene&)
{
}

void Application::onShutdown(GameEngine&)
{
}

GameEngine::GameEngine(EngineConfig config, IPlatform& platform, IRenderer* const renderer)
	: config_(std::move(config))
	, platform_(&platform)
	, renderer_(renderer)
	, physics_(config_.physics)
	, timestep_(config_.timestep)
{
}

GameEngine::~GameEngine()
{
	// Destructors must not throw; a failing onShutdown is already past recovery.
	try
	{
		shutdown();
	}
	catch (...)
	{
	}
}

void GameEngine::run(Application& application)
{
	start(application);
	try
	{
		platform_->run([this]()
			{
				return frame(platform_->timeSeconds());
			});
	}
	catch (...)
	{
		std::exception_ptr const error = std::current_exception();
		try
		{
			shutdown();
		}
		catch (...)
		{
			// The original error is the more useful one to report.
		}
		std::rethrow_exception(error);
	}
	shutdown();
}

void GameEngine::start(Application& application)
{
	if (application_ != nullptr)
	{
		throw std::logic_error("GameEngine::start: an application is already running");
	}
	application_ = &application;
	quitRequested_ = false;
	clock_.reset();
	timestep_.reset();
	extent_ = platform_->framebufferExtent();
	if (renderer_ != nullptr && extent_.width > 0 && extent_.height > 0)
	{
		renderer_->resize(extent_.width, extent_.height);
	}
	application.onStart(*this);
}

bool GameEngine::frame(double const nowSeconds)
{
	if (application_ == nullptr)
	{
		throw std::logic_error("GameEngine::frame: start() has not been called");
	}

	double const frameSeconds = clock_.beginFrame(nowSeconds);
	platform_->pollInput(input_);

	WindowExtent const extent = platform_->framebufferExtent();
	if (extent != extent_)
	{
		extent_ = extent;
		// A minimised window reports 0x0; keep the last valid size for the renderer.
		if (renderer_ != nullptr && extent.width > 0 && extent.height > 0)
		{
			renderer_->resize(extent.width, extent.height);
		}
	}

	lastSteps_ = timestep_.advance(frameSeconds);
	for (std::uint32_t i = 0; i < lastSteps_.steps && !quitRequested_; ++i)
	{
		tick();
	}

	application_->onFrame(*this, frameSeconds);

	if (renderer_ != nullptr)
	{
		world_.extractRenderScene(renderScene_, config_.interpolate ? lastSteps_.alpha : 1.0);
		application_->onBuildRenderScene(*this, renderScene_);
		renderer_->render(renderScene_);
	}
	return !quitRequested_;
}

void GameEngine::stepSimulation(std::uint32_t const ticks)
{
	if (application_ == nullptr)
	{
		throw std::logic_error("GameEngine::stepSimulation: start() has not been called");
	}
	for (std::uint32_t i = 0; i < ticks && !quitRequested_; ++i)
	{
		tick();
	}
}

void GameEngine::tick()
{
	double const step = timestep_.stepSeconds();
	input_.beginTick();
	world_.storeTransformHistory();
	application_->onFixedUpdate(*this, step);
	world_.integrateVelocities(step);
	physics_.step(world_);
	world_.updateAttachments();
	world_.flushDestroyQueue();
	clock_.recordSimulationStep(step);
}

void GameEngine::shutdown()
{
	if (application_ == nullptr)
	{
		return;
	}
	Application* const application = application_;
	application_ = nullptr;
	application->onShutdown(*this);
	world_.clear();
}

}
