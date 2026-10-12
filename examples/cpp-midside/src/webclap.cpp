// SPDX-License-Identifier: MIT
// Copyright 2026 SynthCity
#include "MidSide.hpp"
#include <clap/clap.h>
#include "modular-draft.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <atomic>
static const char* features[]={CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,CLAP_PLUGIN_FEATURE_STEREO,nullptr};
static const clap_plugin_descriptor_t descriptor={CLAP_VERSION,"org.synthcity.examples.hetrick-midside","HetrickCV MidSide — stereo example","SynthCity migration examples","https://github.com/mhetrick/hetrickcv","","","0.1.0","DSP by Michael Hetrick (CC0); stereo width mapping",features};
struct Instance {clap_plugin_t plugin{};MidSide dsp;const clap_host_t* host=nullptr;const clap_host_params_t* hostParams=nullptr;double rate=48000;bool active=false;bool modular=false;uint32_t maxFrames=0;std::atomic<float> published{0.f};std::atomic<bool> statePending{false};};
static Instance& self(const clap_plugin_t* p){return *static_cast<Instance*>(p->plugin_data);}
static void event(Instance& s,const clap_event_header_t* h){
    if(!h||h->space_id!=CLAP_CORE_EVENT_SPACE_ID||h->type!=CLAP_EVENT_PARAM_VALUE||h->size<sizeof(clap_event_param_value_t))return;
    auto* e=reinterpret_cast<const clap_event_param_value_t*>(h);
    if(e->param_id==0&&e->note_id==-1&&e->channel==-1&&e->key==-1&&std::isfinite(e->value)){s.dsp.params[0].value=std::clamp(float(e->value),-5.f,5.f);s.published.store(s.dsp.params[0].value);s.statePending.store(false);}
}
static const char* inputNames[]={"Left","Right","Mid override","Side override","Width CV"};
static const char* outputNames[]={"Mid","Side","Left","Right"};
static const clap_plugin_audio_ports_t ports={
    [](const clap_plugin_t* p,bool input)->uint32_t{return self(p).modular?(input?5:4):1;},
    [](const clap_plugin_t* p,uint32_t index,bool input,clap_audio_port_info_t* info)->bool{
        bool modular=self(p).modular;
        if(index>=(modular?(input?5u:4u):1u))return false;
        *info={};info->id=index;
        std::snprintf(info->name,sizeof(info->name),"%s",modular?(input?inputNames[index]:outputNames[index]):(input?"Stereo input":"Stereo output"));
        info->flags=modular?0:CLAP_AUDIO_PORT_IS_MAIN;
        info->channel_count=modular?16:2;
        info->port_type=modular?SC_PORT_VOLTAGE:CLAP_PORT_STEREO;
        info->in_place_pair=modular?CLAP_INVALID_ID:0;return true;
    }
};
static const clap_plugin_audio_ports_config_t configs={
    [](const clap_plugin_t*)->uint32_t{return 2;},
    [](const clap_plugin_t*,uint32_t i,clap_audio_ports_config_t* info)->bool{
        if(i>1)return false;*info={};info->id=i;
        std::snprintf(info->name,sizeof(info->name),"%s",i?"Modular voltage ports":"Stereo effect");
        info->input_port_count=i?5:1;info->output_port_count=i?4:1;
        info->has_main_input=info->has_main_output=!i;
        info->main_input_channel_count=info->main_output_channel_count=i?0:2;
        info->main_input_port_type=info->main_output_port_type=i?nullptr:CLAP_PORT_STEREO;return true;
    },
    [](const clap_plugin_t* p,clap_id id)->bool{
        auto& s=self(p);if(s.active||id>1)return false;s.modular=id==1;
        for(auto& port:s.dsp.inputs){port.channels=0;port.volts.fill(0);}
        for(auto& port:s.dsp.outputs){port.channels=0;port.volts.fill(0);}return true;
    }
};
static const sc_plugin_modular modularExtension={
    [](const clap_plugin_t*)->clap_id{return 1;},
    [](const clap_plugin_t*,bool input,clap_id id,sc_socket_info* info)->bool{
        if(id>=(input?5u:4u))return false;
        *info={id,input&&id==4?"cv":"audio",input?inputNames[id]:outputNames[id],
            input?(id==2?"Internal (L+R)/2":id==3?"Internal (R-L)*width":"0 V"):"",-5,5,16};return true;
    },
    [](const clap_plugin_t* p,clap_id id,uint32_t channels)->bool{
        auto& s=self(p);if(!s.modular||id>=5||channels>16)return false;
        s.dsp.inputs[id].channels=channels;return true;
    },
    [](const clap_plugin_t* p,clap_id id)->uint32_t{return self(p).modular&&id<4?self(p).dsp.outputs[id].channels:0;}
};
static const clap_plugin_params_t params={
    [](const clap_plugin_t*)->uint32_t{return 1;},
    [](const clap_plugin_t*,uint32_t index,clap_param_info_t* info)->bool{if(index)return false;*info={};info->id=0;info->flags=CLAP_PARAM_IS_AUTOMATABLE;std::snprintf(info->name,sizeof(info->name),"Width");info->min_value=-5;info->max_value=5;info->default_value=0;return true;},
    [](const clap_plugin_t* p,clap_id id,double* value)->bool{if(id)return false;*value=self(p).published.load();return true;},
    [](const clap_plugin_t*,clap_id id,double value,char* text,uint32_t size)->bool{if(id||!size)return false;std::snprintf(text,size,"%.3f",value);return true;},
    [](const clap_plugin_t*,clap_id id,const char* text,double* value)->bool{if(id)return false;char* end;double v=std::strtod(text,&end);if(end==text||*end||!std::isfinite(v)||v< -5||v>5)return false;*value=v;return true;},
    [](const clap_plugin_t* p,const clap_input_events_t* in,const clap_output_events_t*){if(in)for(uint32_t i=0;i<in->size(in);i++)event(self(p),in->get(in,i));}
};
// Portable, versioned parameter state, shared by native and WebCLAP targets.
// Streams may return short reads/writes. Validate the whole record before commit.
static const clap_plugin_state_t state={
    [](const clap_plugin_t* p,const clap_ostream_t* stream)->bool{
        unsigned char bytes[8]={'M','S','W',1,0,0,0,0};
        uint32_t bits;float value=self(p).published.load();std::memcpy(&bits,&value,4);
        for(int i=0;i<4;i++)bytes[4+i]=static_cast<unsigned char>(bits>>(8*i));
        for(uint64_t at=0;at<sizeof(bytes);){auto n=stream->write(stream,bytes+at,sizeof(bytes)-at);if(n<=0||uint64_t(n)>sizeof(bytes)-at)return false;at+=n;}
        return true;
    },
    [](const clap_plugin_t* p,const clap_istream_t* stream)->bool{
        unsigned char bytes[8];
        for(uint64_t at=0;at<sizeof(bytes);){auto n=stream->read(stream,bytes+at,sizeof(bytes)-at);if(n<=0||uint64_t(n)>sizeof(bytes)-at)return false;at+=n;}
        if(bytes[0]!='M'||bytes[1]!='S'||bytes[2]!='W'||bytes[3]!=1)return false;
        uint32_t bits=0;for(int i=0;i<4;i++)bits|=uint32_t(bytes[4+i])<<(8*i);
        float value;std::memcpy(&value,&bits,4);if(!std::isfinite(value)||value< -5||value>5)return false;
        auto& s=self(p);s.published.store(value);s.statePending.store(true);
        // State loads happen on the main thread. Tell the host its cached values
        // changed even though the processing side applies them at the next block.
        if(s.hostParams)s.hostParams->rescan(s.host,CLAP_PARAM_RESCAN_VALUES);
        return true;
    }
};
static clap_process_status process(const clap_plugin_t* p,const clap_process_t* block){
    auto& instance=self(p);
    if(instance.statePending.exchange(false))instance.dsp.params[0].value=instance.published.load();
    if(!instance.active||block->frames_count>instance.maxFrames)return CLAP_PROCESS_ERROR;
    if(instance.modular){
        if(block->audio_inputs_count!=5||block->audio_outputs_count!=4)return CLAP_PROCESS_ERROR;
        for(int i=0;i<5;i++){
            const auto& bus=block->audio_inputs[i];
            if(bus.channel_count!=16||!bus.data32)return CLAP_PROCESS_ERROR;
            for(int c=0;c<16;c++)if(!bus.data32[c])return CLAP_PROCESS_ERROR;
        }
        for(int i=0;i<4;i++){
            const auto& bus=block->audio_outputs[i];
            if(bus.channel_count!=16||!bus.data32)return CLAP_PROCESS_ERROR;
            for(int c=0;c<16;c++)if(!bus.data32[c])return CLAP_PROCESS_ERROR;
        }
        uint32_t e=0,n=block->in_events?block->in_events->size(block->in_events):0;
        auto& m=instance.dsp;
        for(uint32_t f=0;f<block->frames_count;f++){
            while(e<n){auto* h=block->in_events->get(block->in_events,e);if(h->time>f)break;event(instance,h);++e;}
            for(int i=0;i<5;i++)for(int c=0;c<16;c++)
                m.inputs[i].volts[c]=c<m.inputs[i].channels?block->audio_inputs[i].data32[c][f]:0.f;
            m.process({float(instance.rate),float(1/instance.rate)});
            for(int i=0;i<4;i++)for(int c=0;c<16;c++)
                block->audio_outputs[i].data32[c][f]=c<m.outputs[i].channels?m.outputs[i].volts[c]:0.f;
        }
        for(int i=0;i<4;i++)block->audio_outputs[i].constant_mask=0;
        return CLAP_PROCESS_CONTINUE;
    }
    if(block->audio_inputs_count!=1||block->audio_outputs_count!=1)return CLAP_PROCESS_ERROR;
    auto& in=block->audio_inputs[0];auto& out=block->audio_outputs[0];
    if(in.channel_count!=2||out.channel_count!=2||!in.data32||!out.data32)return CLAP_PROCESS_ERROR;
    for(int c=0;c<2;c++)if(!in.data32[c]||!out.data32[c])return CLAP_PROCESS_ERROR;
    auto& s=self(p);auto& m=s.dsp;uint32_t e=0,n=block->in_events?block->in_events->size(block->in_events):0;
    for(uint32_t f=0;f<block->frames_count;f++){
        while(e<n){auto* h=block->in_events->get(block->in_events,e);if(h->time>f)break;event(s,h);++e;}
        // Web audio +/-1 corresponds to rack +/-5 V. Internal M/S and width CV stay unpatched.
        m.inputs[0].channels=m.inputs[1].channels=1;
        m.inputs[0].volts[0]=in.data32[0][f]*5;m.inputs[1].volts[0]=in.data32[1][f]*5;
        m.process({float(s.rate),float(1/s.rate)});
        out.data32[0][f]=m.outputs[2].volts[0]*.2f;out.data32[1][f]=m.outputs[3].volts[0]*.2f;
    }
    out.constant_mask=0;return CLAP_PROCESS_CONTINUE;
}
static const clap_plugin_factory_t factory={
    [](const clap_plugin_factory_t*)->uint32_t{return 1;},
    [](const clap_plugin_factory_t*,uint32_t i)->const clap_plugin_descriptor_t*{return i?nullptr:&descriptor;},
    [](const clap_plugin_factory_t*,const clap_host_t* host,const char* id)->const clap_plugin_t*{
        if(!host||!clap_version_is_compatible(host->clap_version)||std::strcmp(id,descriptor.id))return nullptr;
        auto* s=new(std::nothrow) Instance;if(!s)return nullptr;s->host=host;
        s->plugin={&descriptor,s,
            [](const clap_plugin_t* p){auto& s=self(p);if(s.host->get_extension)s.hostParams=static_cast<const clap_host_params_t*>(s.host->get_extension(s.host,CLAP_EXT_PARAMS));return true;},
            [](const clap_plugin_t* p){delete &self(p);},
            [](const clap_plugin_t* p,double rate,uint32_t min,uint32_t max){if(!std::isfinite(rate)||rate<=0||min<1||max<min)return false;auto& s=self(p);if(s.active)return false;s.rate=rate;s.maxFrames=max;s.active=true;return true;},
            [](const clap_plugin_t* p){self(p).active=false;},[](const clap_plugin_t* p){return self(p).active;},[](const clap_plugin_t*){},
            [](const clap_plugin_t*){},process,
            [](const clap_plugin_t*,const char* id)->const void*{if(!std::strcmp(id,CLAP_EXT_AUDIO_PORTS))return &ports;if(!std::strcmp(id,CLAP_EXT_PARAMS))return &params;if(!std::strcmp(id,CLAP_EXT_STATE))return &state;if(!std::strcmp(id,CLAP_EXT_AUDIO_PORTS_CONFIG))return &configs;if(!std::strcmp(id,SC_EXT_MODULAR))return &modularExtension;return nullptr;},
            [](const clap_plugin_t*){}};
        return &s->plugin;
    }
};
extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry={CLAP_VERSION,
    [](const char*){return true;},[](){},
    [](const char* id)->const void*{return !std::strcmp(id,CLAP_PLUGIN_FACTORY_ID)?&factory:nullptr;}};
