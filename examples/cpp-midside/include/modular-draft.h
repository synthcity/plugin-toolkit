// SPDX-License-Identifier: MIT
// Copyright 2026 SynthCity
#pragma once
#include <clap/clap.h>
#ifdef __cplusplus
extern "C" {
#endif
// Experimental vendor extension. Not an official CLAP extension or stable ABI.
#define SC_EXT_MODULAR "org.synthcity.modular/draft-0"
#define SC_PORT_VOLTAGE "org.synthcity.voltage/draft-0"
// Standard CLAP audio port buffers carry voltages directly in modular configuration.
// Each socket is a fixed-capacity bus. Active lanes are negotiated separately so
// changing cable polyphony never changes CLAP's activated audio-port layout.
typedef struct sc_socket_info {
    clap_id port_id;
    const char* role; // "audio", "cv", "pitch", "gate", "trigger"
    const char* description;
    const char* normal_description;
    double nominal_min_volts, nominal_max_volts; // metadata, not clipping limits
    uint32_t capacity;
} sc_socket_info;
typedef struct sc_plugin_modular {
    // [main thread, initialized] config id from clap.audio-ports-config.
    clap_id (*configuration)(const clap_plugin_t*);
    // [main thread, initialized] enumerate through standard audio-ports first.
    bool (*socket_info)(const clap_plugin_t*,bool input,clap_id port,sc_socket_info*);
    // [audio thread, between process calls] 0 = disconnected; >0 = connected.
    // Only in modular configuration. Fixed for this block. Must set every input
    // on initial activation. Host zeroes inactive lanes; processor owns normals.
    bool (*set_input_channels)(const clap_plugin_t*,clap_id port,uint32_t channels);
    // [audio thread, after process] output active lanes. Remaining lanes are zero.
    uint32_t (*output_channels)(const clap_plugin_t*,clap_id port);
} sc_plugin_modular;
#ifdef __cplusplus
}
#endif
