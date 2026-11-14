// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Light Game Engine
// Copyright (C) 2026 and onwards Dr. Nathanael John Inkson

#include <LightGameEngine/Desktop/FeedbackForm.h>

#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <stdexcept>

namespace {
int failures = 0;
void check(bool ok, char const* what) {
	if (!ok) { std::cerr << "FAILED: " << what << "\n"; ++failures; }
}
}

int main() {
	using lge::FeedbackConfig;

	check(!FeedbackConfig{}.isConfigured(), "empty config is not configured");

	FeedbackConfig defaults;
	check(std::find(defaults.categories.begin(), defaults.categories.end(), "Crash report") != defaults.categories.end(),
		"crash report category is available");
	defaults.developerEmail = "dev@example.com";
	defaults.senderEmail = "app@example.com";
	defaults.smtpUrl = "smtps://mail.example.com:465";
	check(defaults.isConfigured(), "complete config is configured");

	FeedbackConfig badUrl = defaults;
	badUrl.smtpUrl = "http://mail.example.com";
	check(!badUrl.isConfigured(), "non-smtp URL is not configured");
	FeedbackConfig noCategories = defaults;
	noCategories.categories.clear();
	check(!noCategories.isConfigured(), "no categories is not configured");

	// An unset prefix leaves the defaults alone.
	auto same = FeedbackConfig::fromEnvironment("LGE_TEST_UNSET_PREFIX", defaults);
	check(same.developerEmail == defaults.developerEmail && same.smtpPassword.empty(), "unset keeps defaults");

	setenv("LGE_TEST_FB_SMTP_PASSWORD", "secret", 1);
	setenv("LGE_TEST_FB_DEVELOPER_EMAIL", "other@example.com", 1);
	setenv("LGE_TEST_FB_SMTP_USERNAME", "", 1); // empty must not blank a default
	defaults.smtpUsername = "user";
	auto merged = FeedbackConfig::fromEnvironment("LGE_TEST_FB", defaults);
	check(merged.smtpPassword == "secret", "password from environment");
	check(merged.developerEmail == "other@example.com", "environment overrides default");
	check(merged.smtpUsername == "user", "empty variable keeps default");
	check(merged.senderEmail == "app@example.com", "unrelated default kept");
	check(merged.categories == defaults.categories, "categories kept");

	FeedbackConfig disabled;
	disabled.crashReportingEnabled = false;
	check(lge::sendCrashReport(disabled, {}).empty(), "disabled reporting skips delivery");
	bool caught = false;
	try {
		lge::runWithCrashReporting(disabled, [] { throw std::runtime_error("original error"); });
	} catch (std::runtime_error const& error) {
		caught = std::string(error.what()) == "original error";
	}
	check(caught, "wrapper preserves the original exception");
	check(lge::runWithCrashReporting(disabled, [] { return 42; }) == 42, "wrapper returns the application result");
	FeedbackConfig invalid;
	check(!lge::sendCrashReport(invalid, std::make_exception_ptr(std::runtime_error("failure"))).empty(),
		"enabled reporting attempts delivery and returns configuration errors");

	if (failures == 0) std::cout << "FeedbackConfigTests passed\n";
	return failures == 0 ? 0 : 1;
}
