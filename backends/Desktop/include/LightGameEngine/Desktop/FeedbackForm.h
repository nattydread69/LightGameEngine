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

#include <future>
#include <string>
#include <string_view>
#include <vector>

namespace lightGraphics::ui { class GuiContext; class Panel; class DropDown; class TextBox; class Label; }

namespace lge {

struct FeedbackConfig {
	std::string applicationName; // name of the concrete program, included in the email
	std::string developerEmail;
	std::string smtpUrl;       // smtp://host:587 (STARTTLS) or smtps://host:465
	std::string smtpUsername;
	std::string smtpPassword;
	std::string senderEmail;   // mailbox authorised by the SMTP server
	std::vector<std::string> categories {"Bug report", "Feature request", "Feedback"};

	// Overlays values from the process environment onto `defaults` (typically the
	// application's compile-time, non-secret values). For a prefix such as "MYAPP_FEEDBACK"
	// it reads MYAPP_FEEDBACK_DEVELOPER_EMAIL, _SENDER_EMAIL, _SMTP_URL, _SMTP_USERNAME and
	// _SMTP_PASSWORD. A variable that is unset or empty leaves the default untouched, so
	// the password can come from the runtime environment and never from the binary.
	[[nodiscard]] static FeedbackConfig fromEnvironment(std::string_view prefix, FeedbackConfig defaults);
	[[nodiscard]] static FeedbackConfig fromEnvironment(std::string_view prefix);

	// True when the fields needed to address and deliver a message are all present:
	// developer address, sender address and an smtp:// or smtps:// URL, plus at least one
	// category. Credentials are not required here (they may be supplied later); a send
	// without them reports "Configure SMTP credentials." Use it to decide whether to
	// offer feedback in the UI at all.
	[[nodiscard]] bool isConfigured() const;
};

// Keep this object alive while its GuiContext exists. Call update() once per frame.
// Sending runs off the UI thread; the message stays in the form if delivery fails.
class FeedbackForm {
public:
	FeedbackForm(lightGraphics::ui::GuiContext& gui, FeedbackConfig config);
	~FeedbackForm();
	FeedbackForm(FeedbackForm const&) = delete;
	FeedbackForm& operator=(FeedbackForm const&) = delete;

	void open();
	void update();
	[[nodiscard]] bool visible() const;

private:
	void send();

	lightGraphics::ui::GuiContext& gui_;
	FeedbackConfig config_;
	lightGraphics::ui::Panel* panel_ = nullptr;
	lightGraphics::ui::DropDown* category_ = nullptr;
	lightGraphics::ui::TextBox* replyTo_ = nullptr;
	lightGraphics::ui::TextBox* message_ = nullptr;
	lightGraphics::ui::Label* status_ = nullptr;
	std::future<std::string> sendResult_;
};

} // namespace lge
