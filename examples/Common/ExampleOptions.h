#pragma once

// Command-line options shared by the examples so they can also run unattended
// (automated smoke tests and documentation screenshots).

#include "LightGameEngine/Engine/GameEngine.h"

#include <charconv>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>
#include <string>

namespace lge::examples
{

struct ExampleOptions
{
	// Quit after this many frames; 0 runs until the window is closed.
	std::uint64_t frames = 0;
	// Save a PNG of a late frame (requires frames > 0).
	std::optional<std::filesystem::path> screenshot;
	// Let the game play itself (RobotArena).
	bool autopilot = false;
};

inline ExampleOptions parseOptions(int const argc, char** const argv)
{
	ExampleOptions options;
	for (int i = 1; i < argc; ++i)
	{
		std::string const arg = argv[i];
		if (arg == "--frames" && i + 1 < argc)
		{
			char const* const text = argv[++i];
			std::from_chars(text, text + std::strlen(text), options.frames);
		}
		else if (arg == "--screenshot" && i + 1 < argc)
		{
			options.screenshot = std::filesystem::path {argv[++i]};
		}
		else if (arg == "--autopilot")
		{
			options.autopilot = true;
		}
		else
		{
			std::cerr << "Unknown argument: " << arg << "\n"
				<< "Usage: " << argv[0] << " [--frames N] [--screenshot file.png] [--autopilot]\n";
		}
	}
	return options;
}

// Ends the run after the configured frame count and requests the screenshot a few
// frames before the end so the capture has time to complete. Call from onFrame.
class FrameBudget
{
public:
	FrameBudget(ExampleOptions const& options, std::function<void(std::filesystem::path const&)> requestScreenshot)
		: options_(options)
		, requestScreenshot_(std::move(requestScreenshot))
	{
	}

	void update(GameEngine& engine)
	{
		if (options_.frames == 0)
		{
			return;
		}
		std::uint64_t const frame = engine.timing().frameCount;
		std::uint64_t const captureFrame = options_.frames > 10 ? options_.frames - 10 : 1;
		if (options_.screenshot && frame == captureFrame && requestScreenshot_)
		{
			requestScreenshot_(*options_.screenshot);
		}
		if (frame >= options_.frames)
		{
			engine.requestQuit();
		}
	}

private:
	ExampleOptions options_;
	std::function<void(std::filesystem::path const&)> requestScreenshot_;
};

}
