# Licensing module status

The engine repository is not the asset-validation desktop application described in the commercial licensing brief. It currently has no mesh validation, repair, GUI, or command-line product target. This change adds an independent licensing library and test seams; it does not pretend those absent product features are wired or protected.

## Core model

`LightGame::Licensing` provides `ILicenseProvider`, `LicenseManager`, edition/feature mapping, an injected Gumroad verifier, an injected Steam ownership check, Studio file loading, and an Ed25519 verification helper. Providers only grant Pro or Studio when their result is `Valid`; the manager picks the highest valid edition and preserves per-provider status. Free includes validation and basic reporting.

## Studio files and keys

The current Studio reader accepts a small envelope containing string fields `payload`, `key_id`, and `signature`. `payload` is the exact signed byte string and must contain the product and edition identity markers. Signature bytes are passed directly to the verifier. The verifier uses OpenSSL EVP Ed25519 and a caller supplied map of key IDs to PEM public keys. Keep private signing keys outside this repository and all distributed builds. The file reader limits input to 64 KiB.

This is a first integration boundary, not a finished commercial licence schema: expiry, issued-at, organization, capabilities, secure file permissions, and a canonical JSON parser are not yet implemented. Do not issue production licences with this interim reader.

## Marketplace integration

Steam ownership is delegated to an injected callback. The product application must implement that callback with the licensed Steamworks SDK and a configured App ID or DLC ID, initialize/shutdown Steamworks, and use `ISteamApps::BIsSubscribedApp` for the chosen entitlement. This repository does not contain the proprietary SDK, App ID, or a Steam-enabled executable. Steam client ownership checks are client-side checks, not tamper-proof DRM; family sharing and offline behavior follow Steamworks semantics.

Gumroad is delegated to an injected verifier. The future application adapter must POST over verified HTTPS to `https://api.gumroad.com/v2/licenses/verify`, using `product_id`, `license_key`, and `increment_uses_count=false` for routine checks, and validate product, refund, dispute, and subscription fields. No HTTP stack or product ID is configured here. The adapter must not log the key. A timeout is a verification error, not proof of revocation. Local deactivation only forgets the key; Gumroad's usage counter is not device deactivation. No trusted signing service is present, so secure offline Gumroad entitlement is not provided.

## Integrating the product

Construct one `LicenseManager` for the GUI or CLI and pass it to protected application services. Those services must check the relevant `LicensedFeature` before beginning work. Add the real repair, export, batch, profile, report, and CLI service checks when those product services exist. Keep licensing independent of GUI and renderer types. A Studio license path should be selected by application configuration or `lightGameEngine_LICENSE_FILE`, not by a privilege-granting override. The environment variable alone never grants an entitlement.

No activation server, secrets, GUI workflow, or CLI command is implemented by this module. Commercial release still requires product-specific service enforcement and UI, Gumroad HTTPS transport and product configuration, Steamworks SDK integration/App ID setup, finalized Studio JSON schema and expiration checks, and integration tests against the actual application.
