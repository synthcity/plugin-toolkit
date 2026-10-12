# Portable Rust gain

A Clack-based stereo gain example with sample-offset automation and versioned parameter state. Adapted from Clack's gain example, with upstream MIT/Apache notices retained.

From the repository root:

```sh
cargo test --locked --manifest-path examples/rust-gain/Cargo.toml
node examples/rust-gain/build.mjs
```

The build writes native CLAP and WebCLAP bundles to this example's `dist/` directory. Rust dependencies are pinned in Cargo.toml and Cargo.lock. The WASM allocator adapter handles host scratch buffers outside the DSP implementation.

On macOS, build the same Rust implementation through upstream clap-wrapper:

```sh
cmake -S examples/rust-gain/native -B examples/rust-gain/build/native -DCMAKE_BUILD_TYPE=Release
cmake --build examples/rust-gain/build/native --parallel 4
ctest --test-dir examples/rust-gain/build/native --output-on-failure
```

This produces CLAP, VST3 and AUv2 locally. CTest exercises AU audio and state; upstream CLAP/VST3 validators can be supplied with `-DCLAP_VALIDATOR=/path/to/clap-validator` and `-DVST3_VALIDATOR=/path/to/validator`.

The example ID is `org.example.portable-clack-gain`. Choose your own ID and native format identifiers before distribution. There is no custom editor or instrument support in this starter.

Licence: MIT OR Apache-2.0. Original Clack copyright: Adrien Prokopowicz. Portable build adapters and modifications: Copyright 2026 SynthCity.
