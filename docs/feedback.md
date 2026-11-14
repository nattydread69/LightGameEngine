# User feedback (desktop applications)

`LightGame::Desktop` provides `lge::FeedbackForm`. It displays a category drop down,
an optional reply address, a message field, and Send/Close buttons. The default
categories are Bug report, Feature request, Feedback, and Crash report. A concrete application
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

## Automatic crash reports

Wrap the concrete application's top-level work, including initialization, in
`runWithCrashReporting`. It sends exceptions that escape the callable using the same
SMTP settings, with the subject category `Crash report`, then rethrows the original
exception for the application's normal error handling. Keep the configuration alive
outside the callable. Set the one `crashReportingEnabled` variable to `false` to
disable automatic reports; it defaults to `true` when the wrapper is used.

```cpp
auto config = lge::FeedbackConfig::fromEnvironment("MYAPP_FEEDBACK", defaults);
config.crashReportingEnabled = true;
try {
    return lge::runWithCrashReporting(config, [&] {
        return runMyApplication();
    });
} catch (std::exception const& error) {
    // Log or display error.what() and return a failure code.
    return 1;
}
```

Reports contain the application name and exception message (or an indication that
the exception is not a standard exception). Delivery is best effort and synchronous,
with the same 30-second timeout as feedback. SMTP must be configured and credentials
available. This catches exceptions on the wrapped thread; fatal signals, process
termination, and exceptions on other threads are not captured. Exception messages
may contain sensitive information, so applications should consider what they expose.

The message field is a multi-line, word-wrapped `TextArea` (Enter inserts a newline)
limited to 4000 bytes; longer messages are refused with a status message.
