// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Light Game Engine
// Copyright (C) 2026 and onwards Dr. Nathanael John Inkson

#include <LightGameEngine/Desktop/FeedbackForm.h>

#include <cstdlib>
#include <iostream>

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

	if (failures == 0) std::cout << "FeedbackConfigTests passed\n";
	return failures == 0 ? 0 : 1;
}
