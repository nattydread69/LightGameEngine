# User feedback (desktop applications)

`LightGame::Desktop` provides `lge::FeedbackForm`. It displays a category drop down,
an optional reply address, a message field, and Send/Close buttons. The default
categories are Bug report, Feature request, and Feedback. A concrete application
may replace the whole category list in `FeedbackConfig`.

Build with `-DLGE_BUILD_DESKTOP_BRIDGE=ON` and link `LightGame::Desktop`. Configure
the form after `DesktopApplication::init()` has created the GUI. Keep it alive until
before the application and GUI are destroyed.

```cpp
#include <LightGameEngine/Desktop/DesktopApplication.h>
#include <LightGameEngine/Desktop/FeedbackForm.h>

lge::DesktopApplication app;
// Configure and initialise app as usual, then:
lge::FeedbackConfig feedbackConfig;
feedbackConfig.applicationName = "My Game";
feedbackConfig.developerEmail = "feedback@example.com";
feedbackConfig.senderEmail = "app@example.com";
feedbackConfig.smtpUrl = "smtps://mail.example.com:465";
feedbackConfig.smtpUsername = smtpUserFromSettings();
feedbackConfig.smtpPassword = smtpPasswordFromSecureStore();
// Optional: feedbackConfig.categories = {"Problem", "Idea", "Other"};
lge::FeedbackForm feedback(app.gui(), std::move(feedbackConfig));
app.gui().menuBar().addMenu("Help").addItem("Send feedback", [&] { feedback.open(); });
app.setUpdateCallback([&](float) { feedback.update(); });
app.run();
```

## Filling the configuration

`FeedbackConfig::fromEnvironment(prefix, defaults)` overlays runtime environment
variables onto the application's own defaults, and `isConfigured()` says whether
there is enough to deliver a message, so an application can offer feedback only when
it works:

```cpp
lge::FeedbackConfig defaults;            // non-secret values compiled into the app
defaults.developerEmail = "feedback@example.com";
defaults.senderEmail = "app@example.com";
defaults.smtpUrl = "smtps://mail.example.com:465";
auto config = lge::FeedbackConfig::fromEnvironment("MYAPP_FEEDBACK", defaults);
if (config.isConfigured()) {
    // create the FeedbackForm and add the menu item
}
```

For the prefix `MYAPP_FEEDBACK` the variables include `MYAPP_FEEDBACK_APPLICATION_NAME`, `MYAPP_FEEDBACK_DEVELOPER_EMAIL`,
`_SENDER_EMAIL`, `_SMTP_URL`, `_SMTP_USERNAME` and `_SMTP_PASSWORD`. Unset or empty
variables keep the default, so the password can come from the environment and never
from the binary.

The application supplies a developer email address and an SMTP account permitted
to send from `senderEmail`. `smtp://...:587` with STARTTLS is also supported. The
connection requires TLS and has a 30 second timeout. Delivery runs on a worker
thread; call `update()` once per frame to show success or failure. On failure the
message stays in the form for a retry. Do not embed an SMTP password in a shipped
binary; read it from a secure runtime source or provide a server-side relay under
the application's control.

The underlying UI library currently has a single-line `TextBox`. The message
field accepts up to 4000 characters and scrolls horizontally during editing.
