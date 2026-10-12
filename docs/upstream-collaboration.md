# Working with WebCLAP

This toolkit follows [WebCLAP](https://github.com/WebCLAP) and [CLAP](https://github.com/free-audio/clap). It is a consumer and collection of portable examples, not a competing format or certification authority.

## Compatibility checks

`Portable examples` CI has independent `pinned` and `latest` WebCLAP lanes. Both run on pushes, pull requests and manual dispatch. There is no scheduled job. Pinned tests remain the release baseline. Latest tests check the current browser-test-host HEAD with the current wclap-host-js HEAD; they do not change release dependencies automatically.

The report records the actual host revision, recursive loader dependency revisions, plugin hash and per-rate results. A latest run uses the browser host's committed host.wasm; it tests the latest JavaScript loader with that host binary, not a fresh build of every upstream C++ component. Changes to the native host ABI require a separate rebuild and qualification.

Reproduce the canary in a separate checkout:

```sh
git clone https://github.com/WebCLAP/browser-test-host.git .cache/webclap-latest
git -C .cache/webclap-latest submodule update --init --recursive
git -C .cache/webclap-latest/clap-audionode/wclap-js fetch origin HEAD
git -C .cache/webclap-latest/clap-audionode/wclap-js checkout --detach FETCH_HEAD
git -C .cache/webclap-latest/clap-audionode/wclap-js submodule update --init --recursive
WEBCLAP_CANARY=1 node tests/upstream-webclap.mjs .cache/webclap-latest examples/rust-gain/dist/portable-clack-gain.wclap/module.wasm gain
```

The explicit canary flag allows different upstream commit pins, not modified upstream source. The ordinary invocation still requires the release pin and matching submodules.

On 2026-10-12, both Rust gain and C++ MidSide passed discovery, audio, parameter changes and state restoration at 44.1, 48 and 96 kHz using browser-test-host b42ade615bef2c989f96400a7b0b2ef15cabd396 and loader 75792ade475b88ce85694a3725d8da86d2765394. CI currently builds the Rust WASM fixture; both C++ and Rust native fixtures are checked on Linux and macOS. The C++ WASM result was run locally.

## Small contributions to discuss with maintainers

- Link or adapt the Rust/Clack starter into upstream examples, preserving its licences and source attribution.
- Offer the independent-host test harness: numerical audio assertions, state restoration, parameters and explicit AudioContext startup sequencing. Extend coverage to sample-offset events, sleep/wake, multiple instances and memory ceilings before claiming those behaviours are qualified.
- Clarify integration documentation for host event handlers such as `node.events.params_rescan`; that notification is expected on state restore and is not itself a host failure.
- Discuss the modular use cases below before promoting any new extension.

Start with a short conversation about what upstream wants. Submit focused changes with minimal standalone reproducers and exact revision pins. Do not publish product engines, private DSP, graph code or user patches as fixtures.

## Modular questions for discussion

Existing CLAP facilities remain authoritative for parameters, audio ports/configurations, note events, state, latency and tails. The experiment adds information alongside them:

| Requirement | Existing facility / open question |
| --- | --- |
| Stable socket identity and fixed buffer capacity | Use CLAP audio port IDs and channel_count; do not add another port registry. |
| Socket meaning | Can port_type conventions cover audio, CV, pitch, gate and trigger, or is optional metadata preferable? |
| Voltages and pitch | Describe nominal voltage ranges and pitch convention; distinguish metadata from hard clipping limits. |
| Cable polyphony | Distinguish active lanes from fixed channel capacity and MIDI voices. Can active counts change per block without reactivation? |
| Unconnected inputs | Distinguish no cable from a connected zero-valued signal; processor-owned normal connections need this information. |
| Compatibility | Retain an ordinary audio configuration, unknown-extension fallback, version negotiation and stable saved-state behaviour. |

The C++ starter contains `org.synthcity.modular/draft-0`. It is an experimental vendor interface, not an agreed standard. Independent host/plugin implementations, Rust bindings and migration rules are needed before proposing a stable shared ABI. The initial discussion should focus on these requirements, not adoption of our draft verbatim.
