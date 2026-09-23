# Host-only reference dependencies

These files are excluded from every firmware source manifest.

- mbedTLS 3.2.1: minimal AES-CBC implementation and matching headers copied from the retained old SDK's application/ssl/mbedtls-3.2.1. Original copyright and Apache-2.0 notices are retained in each file. This is a test reference, not the target TLS stack.
- cJSON: C implementation and matching header copied from the retained old SDK's application/cJSON. Original MIT license notices are retained in the files.

Firmware uses the new SDK's mbedTLS 3.6.4 headers and matched base-image AES exports, plus the new base cJSON exports. Never add tests/vendor to firmware include paths or source manifests.
