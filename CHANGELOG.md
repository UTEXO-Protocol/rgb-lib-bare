# Changelog

All notable changes to `@utexo/rgb-lib-bare` are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
while pre-`1.0`.

## [0.3.0-beta.18] — 2026-04-20

### Added
- CI: `contents: write` permission on the release job so the GitHub
  Actions workflow can publish prebuild assets.

## [0.3.0-beta.17] — 2026-04-16

### Added
- `inflate_begin` / `inflate_end` and `drain_to_begin` / `drain_to_end`
  wrappers to support the two-phase PSBT flows added in `rgb-lib`.

## [0.3.0-beta.16] — 2026-04-15

### Fixed
- Android prebuild linking: pass `--unresolved-symbols=ignore-all` to
  the linker so platform-dependent NDK symbols don't fail the link.

## [0.3.0-beta.15] — 2026-03-25

### Added
- Full Android support: 4 static libs (arm64, arm, x64, x86), 4 prebuild
  variants, and updated build scripts.

## [0.3.0-beta.13] — 2026-03-21

### Added
- Initial public release: bare native addon wrapping `rgb-lib` C FFI.
  50 functions covering the full wallet lifecycle (BTC + RGB), VSS
  backup, asset issuance, send/receive, and PSBT signing.
