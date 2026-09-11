# Local toolchain

Host: Apple Silicon, macOS 27.0 (26A5388g), 512 GB RAM. Installation date: 2026-09-11.

| Component | Version | Location relative to Studio Contents |
|---|---|---|
| DevEco Studio Mac ARM Release | 26.0.0.821 | `/Applications/DevEco-Studio.app` |
| SDK / Native SDK | 26.0.0.105, API 26 | `sdk/default` |
| Node.js | 24.14.1 | `tools/node/bin/node` |
| OHPM | 26.0.0.630 | `tools/ohpm` |
| Hvigor | 6.26.4 | `tools/hvigor` |
| OHOS Clang | 15.0.4, commit 329916b990b43824d4b7e67de911fee7a966b1c8 | `sdk/default/openharmony/native/llvm/bin/clang` |
| CMake | 3.28.2 | `sdk/default/openharmony/native/build-tools/cmake/bin/cmake` |
| HDC | 3.2.0f | `sdk/default/openharmony/toolchains/hdc` |
| DevEco CLI | 1.3.0-stable | project `.tools/deveco/node_modules/.bin/devecocli` |
| Command Line Tools | 26.0.0.821 | project `.tools/command-line-tools` |
| splat-transform | 2.1.1 (bebac61) | existing `/Users/szmg/Documents/splat-transform` |
| PlayCanvas reference engine | 2.18.1 | existing splat-transform dependency |

Studio and CLT were downloaded by the user through Safari from the
[official download center](https://developer.huawei.com/consumer/cn/download/deveco-studio).
The Studio DMG passed `hdiutil verify`; the application passed strict deep
`codesign` verification (Huawei TeamIdentifier TZEA3TN37Q). Local DMG SHA-256 is
recorded in `artifacts/harmonyos/studio-dmg.sha256`; it is not claimed to have
been compared to the website's checksum.

Use `source scripts/harmonyos/env.sh` from bash for the scoped environment.
No global shell profile, Node, Java or Xcode selection was changed.
CodeGenie 26.0.0.821 is already bundled with Studio.

IDE network settings were set to manual HTTP proxy `127.0.0.1:1082`, matching
the existing macOS proxy. The website connection test passed. Initial image
downloads failed, then a subsequent IDE retry downloaded normally. Keep TLS
verification enabled. See verification.md for installation/run acceptance.
