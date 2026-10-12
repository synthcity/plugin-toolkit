# C++ / Rack source migration example

This example retains Michael Hetrick's CC0 MidSide DSP and adds a CLAP wrapper. It can build as a stereo effect or expose voltage sockets through an optional vendor extension.

`prepare.py` checks the exact hashes in `sources.json`, extracts the DSP portion from three pinned HetrickCV sources (MidSide, MinMax and Exponent), and substitutes a deliberately narrow, independently implemented Rack API subset. It fails if those inputs change. The retained upstream sources include UI code for provenance; their original GUI is not compiled or supplied as a working plugin editor.

This is a reviewed recipe, not a universal VCV-to-CLAP converter. Unsupported Rack APIs, scheduling, UI and signal semantics need deliberate adaptation. Only the selected CC0 sources are included; this does not grant rights to other plugins.

See the root README for native CLAP and Emscripten WebCLAP commands. Package the WASM result with:

```sh
cmake --install examples/cpp-midside/build/wasm --prefix examples/cpp-midside/dist
```

On macOS, build CLAP, VST3 and AUv2 using upstream clap-wrapper:

```sh
cmake -S examples/cpp-midside/native -B examples/cpp-midside/build/native-formats -DCMAKE_BUILD_TYPE=Release
cmake --build examples/cpp-midside/build/native-formats --parallel 4
ctest --test-dir examples/cpp-midside/build/native-formats --output-on-failure
```

The default stereo configuration maps normalized audio to ±5 V internally. `include/modular-draft.h` describes the experimental `org.synthcity.modular/draft-0` interface: socket roles, nominal voltages, normal connections and active polyphonic lanes. It is not a stable ABI or official CLAP/WebCLAP standard. The product's graph engine and optimized host are not included.

Adapter licence: MIT. HetrickCV DSP: CC0. CLAP headers: MIT. Keep each notice when redistributing.
