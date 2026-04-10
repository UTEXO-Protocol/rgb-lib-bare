# RGB Lib Bare bindings

[Bare]-runtime native addon for the [rgb-lib] Rust library — built to plug
RGB into the [Tether WDK] (Wallet Development Kit) alongside BTC, EVM, TON,
Solana, and the other chains WDK supports.

This project includes `rgb-lib` as a git submodule, wraps its [C FFI] via
`binding.cc`, and builds a `.bare` native addon using [cmake-bare]. The
static library is statically linked into the addon so a single `.bare` file
is self-contained — required for iOS and optimal for Android.

## Why Bare?

Node.js bindings ([rgb-lib-nodejs]) and Swift bindings ([rgb-lib-swift]) cover
desktop and native iOS. This package exists for a specific consumer: the
**[Tether WDK]** (Wallet Development Kit). WDK runs its chain-specific wallet
logic inside a [Bare] worklet — a sandboxed JS runtime that executes inside
the host app via [react-native-bare-kit] on mobile or as a subprocess on
desktop. Each supported chain (BTC, EVM, TON, Solana, etc.) ships as a
`@wdk/wallet-*` package that runs inside the worklet.

For RGB to join WDK, `rgb-lib` needs to be callable from inside that worklet.
That means:

- The native code must **link statically** into a `.bare` addon (worklets
  can't dlopen shared libraries the way Node can — and iOS App Store policy
  forbids dynamic linking anyway).
- The JS API must be the **same shape** as `@utexo/rgb-lib` (the Node.js
  package) so [`@utexo/wdk-wallet-rgb`][wdk-wallet-rgb] and [rgb-sdk] can
  write one code path that works in both runtimes via the `@utexo/rgb-lib`
  universal loader.

This package is the layer that makes both possible. Consumers don't import it
directly in React Native code — they import `@utexo/rgb-lib` (or use WDK),
and the universal loader picks `rgb-lib-bare` when it detects the bare
runtime.

## Platform support

Bindings are pre-built and distributed as a **single package** containing
static libraries (`.a`) for all supported targets. The cmake-bare build system
picks the correct one at install time.

| Platform        | Target                       | Linking |
|-----------------|------------------------------|---------|
| iOS arm64       | `aarch64-apple-ios`          | Static  |
| iOS arm64 sim   | `aarch64-apple-ios-sim`      | Static  |
| iOS x64 sim     | `x86_64-apple-ios`           | Static  |
| macOS arm64     | `aarch64-apple-darwin`       | Static  |
| Android arm64   | `aarch64-linux-android`      | Static  |
| Android arm     | `armv7-linux-androideabi`    | Static  |
| Android x64     | `x86_64-linux-android`       | Static  |
| Android x86     | `i686-linux-android`         | Static  |

Static linking is required for iOS (App Store policy) and gives a single
self-contained `.bare` addon on Android.

## Requirements

- Node.js 18+ (for `cmake-bare` and install scripts)
- [bare] runtime (for actually running the addon)

## Installation

You don't normally install this package directly. It's a transitive
dependency of [`@utexo/wdk-wallet-rgb`][wdk-wallet-rgb], which gets pulled in
automatically when you use WDK with the RGB chain enabled.

If you are building something that runs inside a bare worklet and need
`rgb-lib` directly, you can install it explicitly:

```sh
npm install @utexo/rgb-lib-bare
```

A postinstall script fetches the pre-built static libraries and `.bare`
prebuilds from this repo's GitHub Releases. No Rust toolchain or cross-compiler
is needed on the consumer machine.

## Usage

```javascript
const rgblib = require('@utexo/rgb-lib-bare')

// Generate keys for a new wallet
const keys = rgblib.generateKeys(rgblib.BitcoinNetwork.Regtest)
console.log(keys)

// Create a wallet, use it, then drop it to free native memory
const wallet = new rgblib.Wallet(walletData)
try {
  const address = wallet.getAddress()
  const balance = wallet.getBtcBalance(online, /*skipSync*/ false)
  // ... more operations
} finally {
  wallet.drop()
}
```

The JavaScript API is class-based and mirrors the shape of `@utexo/rgb-lib`
(the Node.js package) so code can be written once and run in either runtime
through the [`@utexo/rgb-lib` universal loader][rgb-lib-nodejs].

> :warning: **Memory must be freed manually.**
>
> `Wallet`, `Online`, `Invoice`, and `VssBackupClient` instances hold native
> resources. Always call `.drop()` when you're done, or wrap usage in a
> `try…finally` block. Failing to drop will leak Rust-owned memory.

## C FFI coverage

All 50 functions from rgb-lib's C FFI (v0.3.0-beta.15) are wrapped:

- **Wallet lifecycle** — `newWallet`, `generateKeys`, `restoreKeys`, `goOnline`
- **Queries** — `getAddress`, `getBtcBalance`, `getAssetBalance`,
  `getAssetMetadata`, `getFeeEstimation`, `listAssets`, `listTransfers`,
  `listTransactions`, `listUnspents`
- **UTXO management** — `createUtxos`, `createUtxosBegin`, `createUtxosEnd`
- **Send / receive** — `send`, `sendBegin`, `sendEnd`, `sendBtc`,
  `sendBtcBegin`, `sendBtcEnd`, `blindReceive`, `witnessReceive`
- **Asset issuance** — `issueAssetNia`, `issueAssetCfa`, `issueAssetIfa`,
  `issueAssetUda`, `inflate`
- **PSBT** — `signPsbt`, `finalizePsbt`
- **Invoice** — `invoiceNew`, `invoiceData`, `invoiceString`
- **Transfer ops** — `refresh`, `sync`, `deleteTransfers`, `failTransfers`
- **Backup** — `backup`, `backupInfo`, `restoreBackup`, `validateConsignment`
- **VSS backup** — `newVssBackupClient`, `configureVssBackup`, `vssBackup`,
  `vssBackupInfo`, `vssBackupClientEncryptionEnabled`, `vssDeleteBackup`,
  `disableVssAutoBackup`, `restoreFromVss`

## Build and publish

If you are a maintainer and want to build and release this project from
source:

```shell
# Update the submodule to the desired rgb-lib tag
git submodule update --init --recursive
cd rgb-lib && git checkout v0.3.0-beta.15 && cd ..

# Install Node dependencies (cmake-bare, cmake-npm)
npm install

# Cross-compile the Rust static library for all targets
bash scripts/build-ios.sh all

# Build the .bare prebuilds via cmake-bare for each target
bash scripts/build-prebuilds.sh

# Upload assets to a GitHub Release
gh release create v0.3.0-beta.15 \
  lib/*/librgblibcffi.a prebuilds/*/utexo__rgb-lib-bare.bare

# Publish to npm (maintainers only)
npm publish
```

CI/CD is configured in `.github/workflows/release.yml`: every tag of
[rgb-lib] fires a `repository_dispatch` that builds all 8 targets, uploads
assets, and publishes to npm automatically.

## Architecture

```
rgb-lib (Rust)                  ← source of truth, as a submodule
  └── bindings/c-ffi/           ← cbindgen → rgblib.h
        └── cargo rustc          → librgblibcffi.a (static, per target)
              ↑
rgb-lib-bare (this repo)         ← cmake-bare + binding.cc
  └── binding.cc                 ← wraps the C FFI with bare's <js.h> API
  └── CMakeLists.txt             ← links librgblibcffi.a statically
        ↓
      utexo__rgb-lib-bare.bare   ← the loadable bare addon
```

The key difference from [rgb-lib-nodejs]: node-gyp links dynamically at
runtime, while cmake-bare links statically at build time. Everything else
(the underlying `rgb-lib` Rust crate and the C FFI layer) is identical.

[Bare]: https://github.com/holepunchto/bare
[bare]: https://github.com/holepunchto/bare
[cmake-bare]: https://github.com/holepunchto/cmake-bare
[react-native-bare-kit]: https://github.com/holepunchto/react-native-bare-kit
[Tether WDK]: https://github.com/tetherto/wdk
[wdk-wallet-rgb]: https://github.com/UTEXO-Protocol/wdk-wallet-rgb
[rgb-sdk]: https://github.com/UTEXO-Protocol/rgb-sdk
[rgb-lib]: https://github.com/UTEXO-Protocol/rgb-lib
[rgb-lib-nodejs]: https://github.com/UTEXO-Protocol/rgb-lib-nodejs
[rgb-lib-swift]: https://github.com/UTEXO-Protocol/rgb-lib-swift
[rgb-lib C FFI]: https://github.com/UTEXO-Protocol/rgb-lib/tree/master/bindings/c-ffi
[C FFI]: https://github.com/UTEXO-Protocol/rgb-lib/tree/master/bindings/c-ffi
