# SynthCity Plugin Toolkit

Portable audio plugin starters and build adapters for **CLAP, WebCLAP, VST3 and Audio Unit**.
Reuse DSP across browser and native targets, using upstream interfaces. SynthCity is one consumer; plugins do not require SynthCity services.

## Start here

| Example | DSP / framework | Targets |
| --- | --- | --- |
| [Rust gain](examples/rust-gain/) | Clack, stereo gain | Native CLAP, WebCLAP; macOS AUv2 and VST3 through clap-wrapper |
| [C++ MidSide](examples/cpp-midside/) | Original CC0 HetrickCV DSP, narrow Rack API adapter | Native CLAP, WebCLAP; macOS AUv2 and VST3 through clap-wrapper |

Requires Node.js 22+, Rust (edition 2024), CMake 3.25+, a C++17 compiler and Python 3. Emscripten is required for C++ WebCLAP. Xcode command-line tools are required for macOS wrappers. Install toolchains from their official distributions.

```sh
rustup target add wasm32-unknown-unknown
cargo test --locked --manifest-path examples/rust-gain/Cargo.toml
node examples/rust-gain/build.mjs
cmake -S examples/cpp-midside -B examples/cpp-midside/build/native -DCMAKE_BUILD_TYPE=Release
cmake --build examples/cpp-midside/build/native
ctest --test-dir examples/cpp-midside/build/native --output-on-failure
emcmake cmake -S examples/cpp-midside -B examples/cpp-midside/build/wasm -DCMAKE_BUILD_TYPE=Release
cmake --build examples/cpp-midside/build/wasm
```

Build outputs stay local. Nothing installs into your DAW or publishes packages automatically.
Before releasing your own plugin, replace example IDs, bundle identifiers, AU manufacturer/subtype codes and state format identifiers consistently. Preserve IDs once users save projects with your plugin.

## Verify in an independent WebCLAP host

```sh
npm ci
npx playwright install chromium
mkdir -p .cache
git clone https://github.com/WebCLAP/browser-test-host.git .cache/webclap-host
git -C .cache/webclap-host checkout b42ade615bef2c989f96400a7b0b2ef15cabd396
git -C .cache/webclap-host submodule update --init --recursive
node tests/upstream-webclap.mjs .cache/webclap-host examples/rust-gain/dist/portable-clack-gain.wclap/module.wasm gain
node tests/upstream-webclap.mjs .cache/webclap-host examples/cpp-midside/build/wasm/midside.wasm midside
```

The test loads the unchanged upstream AudioWorklet host and checks discovery, audio, parameter changes and state restoration at 44.1, 48 and 96 kHz. Processing runs in WASM. The test's JavaScript orchestrates the host and measures output; it is not a DSP implementation.

## Standards and scope

- [CLAP](https://github.com/free-audio/clap) is the plugin interface.
- [WebCLAP](https://github.com/WebCLAP) defines the browser/WASM contract. Bundles use `name.wclap/module.wasm`.
- [clap-wrapper](https://github.com/free-audio/clap-wrapper) provides the native format adapters.
- [Clack](https://github.com/prokopyl/clack) provides the Rust CLAP implementation.
- The C++ example includes an **experimental vendor extension** for modular ports. It is not an official CLAP extension or a stable standard. Ordinary hosts can use the stereo configuration without it.

This is an early developer toolkit. The examples have been exercised on macOS and in Chromium. Linux CLAP build coverage is provided by CI; Windows packaging, AUv3, instruments and custom editors are not qualified here. An existing plugin with OS, GUI, threading or platform-SIMD dependencies may need portability work. This is not an automatic converter for arbitrary VCV plugins.

Product engines, sound libraries and application source are not included. This repository contains source examples, not SynthCity product plugin exports.

## Licence and contributions

Toolkit code is MIT except where a subdirectory or file specifies otherwise. The Rust starter is MIT OR Apache-2.0, derived from Clack's gain example; HetrickCV sources are CC0; CLAP headers retain their MIT licence. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

[Contributing](CONTRIBUTING.md) · [Security](SECURITY.md) · [SynthCity](https://synthcity.org)
