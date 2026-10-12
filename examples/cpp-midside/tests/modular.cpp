// Same CLAP host contract exercised natively and inside WASM.
#include "modular-draft.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <cstring>
#include <algorithm>
extern "C" const clap_plugin_entry_t clap_entry;
static void noop(const clap_host_t*){}
static const clap_host_params_t hostParams={
    [](const clap_host_t* h,clap_param_rescan_flags flags){assert(flags==CLAP_PARAM_RESCAN_VALUES);++*static_cast<int*>(h->host_data);},
    [](const clap_host_t*,clap_id,clap_param_clear_flags){},noop};
static const void* ext(const clap_host_t*,const char* id){return std::strcmp(id,CLAP_EXT_PARAMS)?nullptr:&hostParams;}
extern "C" int test_modular(){
    int rescans=0;
    clap_host_t host={CLAP_VERSION,&rescans,"Migration test","SynthCity","","1",ext,noop,noop,noop};
    assert(clap_entry.init(""));
    auto* factory=static_cast<const clap_plugin_factory_t*>(clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
    auto* p=factory->create_plugin(factory,&host,factory->get_plugin_descriptor(factory,0)->id);assert(p&&p->init(p));
    auto* config=static_cast<const clap_plugin_audio_ports_config_t*>(p->get_extension(p,CLAP_EXT_AUDIO_PORTS_CONFIG));
    auto* ports=static_cast<const clap_plugin_audio_ports_t*>(p->get_extension(p,CLAP_EXT_AUDIO_PORTS));
    auto* rack=static_cast<const sc_plugin_modular*>(p->get_extension(p,SC_EXT_MODULAR));assert(config&&ports&&rack);
    assert(ports->count(p,true)==1);assert(!rack->set_input_channels(p,0,1));
    assert(config->select(p,rack->configuration(p)));assert(ports->count(p,true)==5&&ports->count(p,false)==4);
    clap_audio_port_info_t port{};assert(ports->get(p,4,true,&port)&&port.channel_count==16);
    sc_socket_info socket{};assert(rack->socket_info(p,true,4,&socket)&&socket.capacity==16);
    assert(!rack->socket_info(p,true,5,&socket));
    static float in[5][16][128],out[4][16][128];
    float* ip[5][16];float* op[4][16];clap_audio_buffer_t ib[5]{},ob[4]{};
    for(int i=0;i<5;i++){for(int c=0;c<16;c++)ip[i][c]=in[i][c];ib[i].data32=ip[i];ib[i].channel_count=16;}
    for(int i=0;i<4;i++){for(int c=0;c<16;c++)op[i][c]=out[i][c];ob[i].data32=op[i];ob[i].channel_count=16;}
    clap_process_t block{};block.audio_inputs=ib;block.audio_outputs=ob;block.audio_inputs_count=5;block.audio_outputs_count=4;
    int cases=0;
    for(double rate:{44100.,48000.,96000.}){
        assert(p->activate(p,rate,1,128));assert(p->start_processing(p));assert(!config->select(p,0));
        for(int voices:{1,3,5,16})for(int frames:{1,7,128}){
            block.frames_count=frames;
            for(int i=0;i<5;i++)assert(rack->set_input_channels(p,i,i<2?voices:0));
            for(int c=0;c<16;c++)for(int f=0;f<frames;f++){in[0][c][f]=1+c*.1f;in[1][c][f]=-.5f;}
            assert(p->process(p,&block)==CLAP_PROCESS_CONTINUE);
            assert(rack->output_channels(p,0)==unsigned(voices));
            for(int c=0;c<16;c++)for(int f=0;f<frames;f++){
                assert(std::abs(out[2][c][f]-(c<voices?in[0][c][f]:0))<1e-5);
                assert(std::abs(out[3][c][f]-(c<voices?-.5f:0))<1e-5);
            }
            // A patched zero Mid must override its internal normal.
            assert(rack->set_input_channels(p,2,voices));assert(p->process(p,&block)==CLAP_PROCESS_CONTINUE);
            for(int c=0;c<voices;c++)assert(std::abs(out[2][c][0]+out[1][c][0])<1e-5);
            cases++;
        }
        assert(!rack->set_input_channels(p,0,17));assert(!rack->set_input_channels(p,5,1));
        block.frames_count=129;assert(p->process(p,&block)==CLAP_PROCESS_ERROR);
        p->stop_processing(p);p->deactivate(p);
    }
    // Return the very same instance to its ordinary stereo layout.
    assert(config->select(p,0));assert(ports->count(p,true)==1);
    assert(p->activate(p,48000,1,128));assert(p->start_processing(p));
    ib[0].channel_count=ob[0].channel_count=2;block.audio_inputs_count=block.audio_outputs_count=1;block.frames_count=1;
    in[0][0][0]=.6f;in[0][1][0]=-.2f;
    assert(p->process(p,&block)==CLAP_PROCESS_CONTINUE);
    assert(std::abs(out[0][0][0]-.6f)<1e-6&&std::abs(out[0][1][0]+.2f)<1e-6);
    p->stop_processing(p);p->deactivate(p);
    // State must notify the host's cached parameter values, including before
    // activation. Invalid state must neither change values nor request a rescan.
    auto* state=static_cast<const clap_plugin_state_t*>(p->get_extension(p,CLAP_EXT_STATE));
    auto* params=static_cast<const clap_plugin_params_t*>(p->get_extension(p,CLAP_EXT_PARAMS));
    struct StateBytes {unsigned char data[8]={'M','S','W',1,0,0,128,63};size_t at=0;} saved;
    clap_istream_t stream{&saved,[](const clap_istream_t* in,void* dst,uint64_t size)->int64_t{
        auto& s=*static_cast<StateBytes*>(in->ctx);size_t n=std::min<size_t>(size,std::min<size_t>(3,8-s.at));
        std::memcpy(dst,s.data+s.at,n);s.at+=n;return n;}};
    assert(state->load(p,&stream));assert(rescans==1);
    double value=0;assert(params->get_value(p,0,&value)&&value==1);
    saved.at=0;saved.data[3]=99;assert(!state->load(p,&stream));assert(rescans==1);
    assert(params->get_value(p,0,&value)&&value==1);
    assert(p->activate(p,1234.567,1,128));p->deactivate(p);
    p->destroy(p);clap_entry.deinit();return cases;
}
#ifndef __EMSCRIPTEN__
int main(){std::printf("Modular CLAP: %d configurations passed\n",test_modular());}
#endif
