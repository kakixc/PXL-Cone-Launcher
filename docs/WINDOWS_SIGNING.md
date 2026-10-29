# Windows package signing

Windows CI packages are unsigned by default. The workflow does not use the upstream project's signing account or certificate profile.

To enable signing with your own Azure service, configure these repository variables:

- `PXL_WINDOWS_SIGNING_ENABLED`: `true`.
- `PXL_WINDOWS_SIGNING_ENDPOINT`: your Artifact Signing endpoint.
- `PXL_WINDOWS_SIGNING_ACCOUNT`: your signing account.
- `PXL_WINDOWS_SIGNING_PROFILE`: your certificate profile.

Configure `AZURE_CLIENT_ID`, `AZURE_TENANT_ID` and `AZURE_SUBSCRIPTION_ID` as repository secrets, with the appropriate owner-controlled Azure OIDC trust. Do not commit credentials. The workflow retains the existing Azure login and signing actions; it does not provision a certificate or Azure account.

With signing disabled, the build summary explicitly reports unsigned packages. If signing is enabled but configuration is incomplete, validation fails before packaging or Azure login rather than silently producing a supposedly signed package. Executables and the installer use the same validated configuration.

This configuration does not publish a release. A successful real CI signing run and signature verification are separate release checks. For an unsigned alpha, provide checksums and clearly identify its unsigned status.
