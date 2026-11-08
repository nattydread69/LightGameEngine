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

#include "TestHarness.h"

#include "LightGameEngine/Core/FixedTimestep.h"
#include "LightGameEngine/Core/GameClock.h"
#include "LightGameEngine/Math/Transform.h"

#include <glm/gtc/constants.hpp>

using namespace lge;
using namespace lge::test;

namespace
{

TestCase const fixedStepCountsExactFrames {"FixedTimestep: frames equal to the step produce one step each", []
	{
		FixedTimestep timestep;
		std::uint32_t total = 0;
		for (int i = 0; i < 60; ++i)
		{
			total += timestep.advance(1.0 / 60.0).steps;
		}
		checkEqual(total, 60u, "60 frames at 60 Hz run 60 steps");
		checkNear(timestep.accumulatedSeconds(), 0.0, 1e-9, "accumulator is empty");
	}};

TestCase const fixedStepAccumulatesShortFrames {"FixedTimestep: short frames accumulate", []
	{
		FixedTimestep timestep {FixedTimestepConfig {0.01, 0.25, 8}};
		checkEqual(timestep.advance(0.004).steps, 0u, "4 ms < 10 ms step");
		checkEqual(timestep.advance(0.004).steps, 0u, "8 ms accumulated");
		FixedStepResult const third = timestep.advance(0.004);
		checkEqual(third.steps, 1u, "12 ms accumulated yields one step");
		checkNear(third.alpha, 0.2, 1e-9, "2 ms remainder is alpha 0.2");
	}};

TestCase const fixedStepRunsSeveralStepsForLongFrame {"FixedTimestep: a long frame runs several steps", []
	{
		FixedTimestep timestep {FixedTimestepConfig {0.01, 0.25, 8}};
		FixedStepResult const result = timestep.advance(0.035);
		checkEqual(result.steps, 3u, "35 ms is three 10 ms steps");
		checkNear(result.alpha, 0.5, 1e-9, "half a step remains");
		checkNear(result.droppedSeconds, 0.0, 1e-12, "nothing dropped");
	}};

TestCase const fixedStepClampsHugeFrames {"FixedTimestep: huge frames are clamped (no spiral of death)", []
	{
		FixedTimestep timestep {FixedTimestepConfig {1.0 / 60.0, 0.25, 100}};
		FixedStepResult const result = timestep.advance(5.0);
		checkEqual(result.steps, 15u, "clamped to 0.25 s = 15 steps");
		checkNear(result.droppedSeconds, 4.75, 1e-9, "4.75 s dropped");
	}};

TestCase const fixedStepLimitsStepsPerFrame {"FixedTimestep: step limit discards the backlog", []
	{
		FixedTimestep timestep {FixedTimestepConfig {0.01, 1.0, 4}};
		FixedStepResult const result = timestep.advance(0.105);
		checkEqual(result.steps, 4u, "limited to 4 steps");
		check(timestep.accumulatedSeconds() < 0.01, "backlog does not carry over");
		checkNear(result.droppedSeconds, 0.06, 1e-9, "six steps' worth dropped");
		checkEqual(timestep.advance(0.0).steps, 0u, "no steps on the next empty frame");
	}};

TestCase const fixedStepRejectsBadConfig {"FixedTimestep: invalid configuration throws", []
	{
		checkThrows<std::invalid_argument>([]
			{
				FixedTimestep {FixedTimestepConfig {0.0, 0.25, 8}};
			}, "zero step");
		checkThrows<std::invalid_argument>([]
			{
				FixedTimestep {FixedTimestepConfig {0.01, 0.25, 0}};
			}, "zero step limit");
	}};

TestCase const fixedStepIgnoresNegativeTime {"FixedTimestep: negative and non-finite frame times are ignored", []
	{
		FixedTimestep timestep;
		checkEqual(timestep.advance(-1.0).steps, 0u, "negative");
		checkEqual(timestep.advance(std::nan("")).steps, 0u, "NaN");
		checkNear(timestep.accumulatedSeconds(), 0.0, 0.0, "accumulator untouched");
	}};

TestCase const clockTracksTime {"GameClock: frame, total and simulation time", []
	{
		GameClock clock;
		checkNear(clock.beginFrame(10.0), 0.0, 0.0, "first frame has zero delta");
		checkNear(clock.beginFrame(10.5), 0.5, 1e-12, "second frame delta");
		clock.recordSimulationStep(0.25);
		clock.recordSimulationStep(0.25);
		FrameTiming const& timing = clock.timing();
		checkNear(timing.totalSeconds, 0.5, 1e-12, "total since first frame");
		checkEqual(timing.frameCount, std::uint64_t {2}, "frame count");
		checkEqual(timing.tickCount, std::uint64_t {2}, "tick count");
		checkNear(timing.simulationSeconds, 0.5, 1e-12, "simulation seconds");
	}};

TestCase const clockMeasuresFps {"GameClock: FPS over a measurement window", []
	{
		GameClock clock {1.0};
		for (int i = 0; i <= 50; ++i)
		{
			clock.beginFrame(i * 0.02);
		}
		checkNear(clock.timing().framesPerSecond, 50.0, 1e-6, "50 FPS");
	}};

TestCase const clockHandlesBackwardsTime {"GameClock: a clock going backwards yields zero delta", []
	{
		GameClock clock;
		clock.beginFrame(5.0);
		checkNear(clock.beginFrame(4.0), 0.0, 0.0, "no negative delta");
		checkNear(clock.beginFrame(5.5), 0.5, 1e-12, "resumes from the latest time");
	}};

TestCase const transformAxes {"Transform: conventions (Y up, -Z forward, right-handed)", []
	{
		Transform transform;
		checkNear(transform.forward().z, -1.0, 1e-12, "identity faces -Z");
		transform.rotation = yawRotation(glm::half_pi<double>());
		checkNear(transform.forward().x, -1.0, 1e-12, "yaw +90 degrees faces -X");
		checkNear(yawFromDirection(glm::dvec3 {-1.0, 0.0, 0.0}), glm::half_pi<double>(), 1e-12, "yawFromDirection inverts yawRotation");
		glm::dvec3 const crossed = glm::cross(worldRight, worldUp);
		checkNear(crossed.z, 1.0, 1e-12, "right x up = +Z (backwards), so the basis is right-handed");
	}};

TestCase const transformMatrixOrder {"Transform: matrix applies scale, then rotation, then translation", []
	{
		Transform transform;
		transform.position = {10.0, 0.0, 0.0};
		transform.rotation = yawRotation(glm::half_pi<double>());
		transform.scale = {2.0, 2.0, 2.0};
		glm::dvec4 const p = transform.matrix() * glm::dvec4 {1.0, 0.0, 0.0, 1.0};
		checkNear(p.x, 10.0, 1e-12, "x");
		checkNear(p.z, -2.0, 1e-12, "scaled +X rotated to -Z then translated");
	}};

TestCase const lookRotationFacesTarget {"Transform: lookRotation produces the requested basis", []
	{
		Transform transform;
		transform.rotation = lookRotation(glm::dvec3 {0.0, -1.0, 0.0}, glm::dvec3 {0.0, 0.0, -1.0});
		checkNear(transform.forward().y, -1.0, 1e-9, "looks down");
		checkNear(transform.up().z, -1.0, 1e-9, "screen up is north (-Z)");
		checkThrows<std::invalid_argument>([]
			{
				(void)lookRotation(worldUp, worldUp);
			}, "parallel up throws");
	}};

TestCase const interpolationBlends {"Transform: interpolate blends position and rotation", []
	{
		Transform a;
		Transform b;
		b.position = {4.0, 0.0, 0.0};
		b.rotation = yawRotation(glm::half_pi<double>());
		Transform const mid = interpolate(a, b, 0.5);
		checkNear(mid.position.x, 2.0, 1e-12, "halfway position");
		checkNear(yawFromDirection(mid.forward()), glm::quarter_pi<double>(), 1e-9, "halfway yaw");
	}};

}
