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

#include <LightGameEngine/Desktop/FeedbackForm.h>

#include <lightVulkanGraphics/ui/Ui.h>
#include <curl/curl.h>

#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace lge {
namespace {

bool validAddress(std::string_view address) {
	return !address.empty() && address.find('@') != std::string_view::npos &&
		address.find_first_of("\r\n<>\0", 0, 5) == std::string_view::npos;
}

bool validHeader(std::string_view value) {
	return !value.empty() && value.find_first_of("\r\n\0", 0, 3) == std::string_view::npos;
}

void overlayFromEnvironment(std::string const& prefix, char const* suffix, std::string& field) {
	if (char const* value = std::getenv((prefix + "_" + suffix).c_str()); value && *value)
		field = value;
}

struct Upload {
	std::string payload;
	size_t offset = 0;
};

size_t readPayload(char* output, size_t size, size_t count, void* user) {
	auto& upload = *static_cast<Upload*>(user);
	size_t const available = upload.payload.size() - upload.offset;
	size_t const bytes = std::min(size * count, available);
	if (bytes) {
		std::memcpy(output, upload.payload.data() + upload.offset, bytes);
		upload.offset += bytes;
	}
	return bytes;
}

} // namespace

FeedbackConfig FeedbackConfig::fromEnvironment(std::string_view prefix, FeedbackConfig defaults) {
	std::string const base(prefix);
	overlayFromEnvironment(base, "APPLICATION_NAME", defaults.applicationName);
	overlayFromEnvironment(base, "DEVELOPER_EMAIL", defaults.developerEmail);
	overlayFromEnvironment(base, "SENDER_EMAIL", defaults.senderEmail);
	overlayFromEnvironment(base, "SMTP_URL", defaults.smtpUrl);
	overlayFromEnvironment(base, "SMTP_USERNAME", defaults.smtpUsername);
	overlayFromEnvironment(base, "SMTP_PASSWORD", defaults.smtpPassword);
	return defaults;
}

FeedbackConfig FeedbackConfig::fromEnvironment(std::string_view prefix) {
	return fromEnvironment(prefix, FeedbackConfig{});
}

bool FeedbackConfig::isConfigured() const {
	return validAddress(developerEmail) && validAddress(senderEmail) &&
		(smtpUrl.rfind("smtp://", 0) == 0 || smtpUrl.rfind("smtps://", 0) == 0) &&
		!categories.empty();
}

namespace {

std::string sendMail(FeedbackConfig config, std::string category,
	std::string replyTo, std::string message) {
	if (!validAddress(config.developerEmail) || !validAddress(config.senderEmail) ||
		(!replyTo.empty() && !validAddress(replyTo)) || !validHeader(category))
		return "Invalid email address or category.";
	if (config.smtpUrl.rfind("smtp://", 0) != 0 && config.smtpUrl.rfind("smtps://", 0) != 0)
		return "Configure an SMTP or SMTPS server.";
	if (config.smtpUsername.empty() || config.smtpPassword.empty())
		return "Configure SMTP credentials.";
	if (message.empty()) return "Please enter a message.";

	static bool const curlReady = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
	if (!curlReady) return "Could not initialise email delivery.";
	auto curl = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>(curl_easy_init(), curl_easy_cleanup);
	if (!curl) return "Could not initialise email delivery.";

	// Header values are checked above; normalise line endings for SMTP DATA.
	std::string body;
	body.reserve(message.size() + 32);
	for (size_t i = 0; i < message.size(); ++i) {
		if (message[i] == '\r') {
			body += "\r\n";
			if (i + 1 < message.size() && message[i + 1] == '\n') ++i;
		} else if (message[i] == '\n') body += "\r\n";
		else if (message[i] != '\0') body += message[i];
	}
	if (!body.ends_with("\r\n")) body += "\r\n";
	std::string const appName = config.applicationName.empty() ? "Application" : config.applicationName;
	Upload upload {"To: <" + config.developerEmail + ">\r\n"
		"From: <" + config.senderEmail + ">\r\n"
		"Subject: [" + appName + "] " + category + "\r\n"
		+ (replyTo.empty() ? "" : "Reply-To: <" + replyTo + ">\r\n")
		+ "MIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\n"
		"Content-Transfer-Encoding: 8bit\r\n\r\n"
		"Application: " + appName + "\r\nCategory: " + category + "\r\n\r\n" + body};
	std::string const sender = "<" + config.senderEmail + ">";
	std::string const recipient = "<" + config.developerEmail + ">";
	curl_slist* recipients = curl_slist_append(nullptr, recipient.c_str());
	if (!recipients) return "Could not prepare email delivery.";

	curl_easy_setopt(curl.get(), CURLOPT_URL, config.smtpUrl.c_str());
	curl_easy_setopt(curl.get(), CURLOPT_USERNAME, config.smtpUsername.c_str());
	curl_easy_setopt(curl.get(), CURLOPT_PASSWORD, config.smtpPassword.c_str());
	curl_easy_setopt(curl.get(), CURLOPT_USE_SSL, CURLUSESSL_ALL);
	curl_easy_setopt(curl.get(), CURLOPT_MAIL_FROM, sender.c_str());
	curl_easy_setopt(curl.get(), CURLOPT_MAIL_RCPT, recipients);
	curl_easy_setopt(curl.get(), CURLOPT_READFUNCTION, readPayload);
	curl_easy_setopt(curl.get(), CURLOPT_READDATA, &upload);
	curl_easy_setopt(curl.get(), CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);
	curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "smtp,smtps");
	CURLcode const result = curl_easy_perform(curl.get());
	curl_slist_free_all(recipients);
	if (result != CURLE_OK) return std::string("Send failed: ") + curl_easy_strerror(result);
	return {};
}

} // namespace

FeedbackForm::FeedbackForm(lightGraphics::ui::GuiContext& gui, FeedbackConfig config)
	: gui_(gui), config_(std::move(config)) {
	if (config_.categories.empty())
		throw std::invalid_argument("FeedbackForm requires at least one category");
	for (auto const& category : config_.categories)
		if (!validHeader(category)) throw std::invalid_argument("FeedbackForm has an invalid category");

	namespace ui = lightGraphics::ui;
	panel_ = gui_.createPanel("Send feedback", {80, 80, 440, 280});
	panel_->setVisible(false);
	panel_->add<ui::Label>("Send your comments to the developer.")->setWordWrap(true);
	category_ = panel_->add<ui::DropDown>("Category", config_.categories);
	replyTo_ = panel_->add<ui::TextBox>("Your email (optional)");
	replyTo_->setMaxLength(254);
	message_ = panel_->add<ui::TextBox>("Message");
	message_->setMaxLength(4000);
	status_ = panel_->add<ui::Label>("");
	status_->setWordWrap(true);
	panel_->add<ui::Button>("Send")->setOnClick([this] { send(); });
	panel_->add<ui::Button>("Close")->setOnClick([this] { panel_->setVisible(false); });
}

FeedbackForm::~FeedbackForm() {
	if (sendResult_.valid()) sendResult_.wait();
	gui_.destroyPanel(panel_);
}

void FeedbackForm::open() {
	panel_->setVisible(true);
	panel_->bringToFront();
}

bool FeedbackForm::visible() const { return panel_->visible(); }

void FeedbackForm::send() {
	if (sendResult_.valid()) return;
	std::string const message(message_->text());
	if (message.empty()) {
		status_->setText("Please enter a message.");
		return;
	}
	status_->setText("Sending...");
	sendResult_ = std::async(std::launch::async, sendMail, config_,
		std::string(category_->selectedText()), std::string(replyTo_->text()), message);
}

void FeedbackForm::update() {
	if (!sendResult_.valid() ||
		sendResult_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
	try {
		std::string const error = sendResult_.get();
		status_->setText(error.empty() ? "Feedback sent. Thank you." : error);
		if (error.empty()) message_->setText("");
	} catch (std::exception const&) {
		status_->setText("Send failed. Please try again.");
	}
}

} // namespace lge
