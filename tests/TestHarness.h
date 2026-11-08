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

#pragma once

// Minimal dependency-free test harness. Test cases register themselves through
// static TestCase objects; checks record failures with their source location and
// keep running, so one run reports every failing check.

#include <cmath>
#include <functional>
#include <iostream>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace lge::test
{

struct Registered
{
	std::string name;
	std::function<void()> body;
};

inline std::vector<Registered>& registry()
{
	static std::vector<Registered> tests;
	return tests;
}

inline int& failureCount()
{
	static int failures = 0;
	return failures;
}

struct TestCase
{
	TestCase(std::string name, std::function<void()> body)
	{
		registry().push_back(Registered {std::move(name), std::move(body)});
	}
};

inline void fail(std::string_view const message, std::source_location const location)
{
	++failureCount();
	std::cerr << "  FAILED " << location.file_name() << ":" << location.line() << ": " << message << "\n";
}

inline bool check(bool const condition, std::string_view const description,
	std::source_location const location = std::source_location::current())
{
	if (!condition)
	{
		fail(description, location);
	}
	return condition;
}

template <typename A, typename B>
bool checkEqual(A const& actual, B const& expected, std::string_view const description,
	std::source_location const location = std::source_location::current())
{
	if (!(actual == expected))
	{
		std::ostringstream message;
		message << description << " (got " << actual << ", expected " << expected << ")";
		fail(message.str(), location);
		return false;
	}
	return true;
}

inline bool checkNear(double const actual, double const expected, double const tolerance,
	std::string_view const description, std::source_location const location = std::source_location::current())
{
	if (!(std::abs(actual - expected) <= tolerance))
	{
		std::ostringstream message;
		message << description << " (got " << actual << ", expected " << expected << " +/- " << tolerance << ")";
		fail(message.str(), location);
		return false;
	}
	return true;
}

template <typename Exception, typename Function>
bool checkThrows(Function&& function, std::string_view const description,
	std::source_location const location = std::source_location::current())
{
	try
	{
		function();
	}
	catch (Exception const&)
	{
		return true;
	}
	catch (...)
	{
		fail(std::string {description} + " (threw a different exception type)", location);
		return false;
	}
	fail(std::string {description} + " (did not throw)", location);
	return false;
}

// Runs every registered test, or only those whose names contain argv[1].
inline int runAll(int const argc, char** const argv)
{
	std::string_view const filter = argc > 1 ? std::string_view {argv[1]} : std::string_view {};
	int ran = 0;
	int failedTests = 0;
	for (Registered const& test : registry())
	{
		if (!filter.empty() && test.name.find(filter) == std::string::npos)
		{
			continue;
		}
		int const before = failureCount();
		std::cout << "[ RUN  ] " << test.name << "\n";
		try
		{
			test.body();
		}
		catch (std::exception const& error)
		{
			fail(std::string {"unexpected exception: "} + error.what(), std::source_location::current());
		}
		catch (...)
		{
			fail("unexpected non-standard exception", std::source_location::current());
		}
		bool const passed = failureCount() == before;
		std::cout << (passed ? "[  OK  ] " : "[ FAIL ] ") << test.name << "\n";
		++ran;
		failedTests += passed ? 0 : 1;
	}
	std::cout << ran - failedTests << "/" << ran << " test cases passed\n";
	if (ran == 0)
	{
		std::cerr << "No tests matched the filter\n";
		return 1;
	}
	return failedTests == 0 ? 0 : 1;
}

}
