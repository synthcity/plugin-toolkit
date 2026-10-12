// SPDX-License-Identifier: MIT
// Copyright 2026 SynthCity
#include <clap/clap.h>
extern "C" const clap_plugin_entry_t portable_midside_entry;
extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry={CLAP_VERSION,
    [](const char* path){return portable_midside_entry.init(path);},
    [](){portable_midside_entry.deinit();},
    [](const char* id){return portable_midside_entry.get_factory(id);}};
