# Generic offline licensing

`LightGame::Licensing` verifies entitlements. Each application supplies a stable `ProductIdentity`, its own edition names and precedence, free features, and the public signing keys it trusts. Marketplace IDs and payment details are outside this API. Steam and Gumroad remain optional, separate providers.

## Application integration

```cpp
#include <LightGameEngine/Licensing/LicenseManager.h>
#include <LightGameEngine/Licensing/SignedLicense.h>

using namespace lge::licensing;
ProductIdentity product {"example.application", "example.publisher"};
std::map<std::string, std::string> trustedKeys {{"2026-primary", readText("license-public.pem")}};
auto signedProvider = std::make_shared<SignedLicense>(product, trustedKeys, userDataDirectory);
LicenseManager licenses {product, {signedProvider}, "community", {"feature.read"},
	{"community", "commercial", "enterprise"}};
auto activation = licenses.activate(LicenseProviderKind::Signed, userSuppliedToken);
if (activation.status == LicenseStatus::Valid && licenses.hasFeature("feature.export"))
{
	// Run the application's export operation.
}
```

`hasFeature` only reports application supplied free features or features in a valid signed entitlement. Edition precedence is ordered from lowest to highest by the application. Unlisted editions do not affect the selected edition; the signed feature list remains authoritative. Use `currentLicense()` and `providerStatuses()` for validity, expiry, provider, and licence diagnostics.

## Credential format

The `lge1.` token contains URL safe Base64 payload and Ed25519 signature components. The version 1 payload uses a fixed field order and big endian length-prefixed UTF-8 strings/integers. It includes product and publisher IDs, edition, unique licence ID, optional organisation ID, issue and optional expiry timestamps, key ID, and feature identifiers. The signature covers the complete payload. The exact token string can also be stored in a `.lic` text file; both activation paths use the same verifier. Whitespace around a pasted token is ignored. The runtime rejects unknown keys, unexpected products, malformed fields, duplicate features, invalid time intervals, and oversized credentials.

The format is binary inside the textual token rather than JSON. This provides one unambiguous signed-byte representation without relying on JSON field ordering or permissive parsing. Do not change this representation without introducing a new token version.

## Issuer utility

Build the separate internal tool with `-DLGE_BUILD_LICENSE_ISSUER=ON`. It is not part of the runtime library or an application dependency.

```sh
lge-license-tool generate-keypair --private signing-private.pem --public signing-public.pem
lge-license-tool issue --private-key signing-private.pem --product example.application \
  --publisher example.publisher --edition commercial --organisation example.customer \
  --features feature.export,feature.batch \
  --key-id 2026-primary --output one.lic
lge-license-tool issue-batch --private-key signing-private.pem --product example.application \
  --edition commercial --features feature.export --key-id 2026-primary --count 100 --output tokens.txt
lge-license-tool verify --input one.lic --public-key signing-public.pem \
  --product example.application --publisher example.publisher --key-id 2026-primary
```

Each output line is an independent signed token with a secure random unique ID. The tool reports the token character count; a typical token is several hundred characters, so check the chosen distributor's current external-key limits before importing. No marketplace-specific limit is assumed. Expiry can be set with `--expires-epoch` (UTC Unix seconds); omit it for a perpetual licence. The CLI refuses to overwrite an existing output. Keep the private key in an access-controlled secret store or offline signing environment. Only distribute public keys, and configure multiple key IDs during rotation so old and new credentials remain verifiable.

The internal issuer currently creates PEM key files and attempts to restrict the private file to its owner. Secure the destination directory and signing host as well. Automated tests generate ephemeral keys and do not use production material.

## Storage and activation

Pass an application data directory to `SignedLicense` to persist the verified token in a product-scoped file. Pass an empty path for headless applications that manage credential input and persistence themselves. The signature and expected product are rechecked when stored credentials load. Activation only replaces storage after verification succeeds. `deactivate()` removes the local copy; it cannot invalidate copies held elsewhere.

To distribute custom external keys, issue a newline separated batch, import each complete token into the distributor's external-key mechanism, and have the application accept the supplied token in its normal activation flow. Verification has no network or marketplace dependency.

## Security and compatibility

An Ed25519 signature proves that a configured signing authority issued the payload. It does not independently prove that a marketplace purchase happened, identify the current user as the purchaser, detect refunds, or prevent copying. Offline expiry relies on the machine's clock and can be bypassed by changing it. Use an optional online provider only if purchase reconciliation, revocation, or device activation is required.

The legacy `ProductEdition`, `LicensedFeature`, Steam, Gumroad, and `StudioLicense` interfaces remain available for existing integrations. Legacy edition-to-feature mapping is retained only for those older providers. New generic applications should use string edition and feature IDs plus `SignedLicense`; migrate legacy signed files by issuing a new version 1 credential for the application's configured product and explicit capabilities. Never copy a legacy application's private key into a distributed runtime.
