// SPDX-License-Identifier: MIT
// Copyright 2026 SynthCity
// Process-local AU registration: no installation or system registry changes.
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <cstring>
static void checkAt(OSStatus status,int line){if(status){std::fprintf(stderr,"AudioUnit error %d at line %d\n",int(status),line);std::abort();}}
#define check(status) checkAt(status,__LINE__)
static OSStatus input(void*,AudioUnitRenderActionFlags*,const AudioTimeStamp*,UInt32,UInt32 frames,AudioBufferList* buffers){
    assert(buffers->mNumberBuffers==2);
    for(unsigned c=0;c<2;c++)for(unsigned f=0;f<frames;f++)static_cast<float*>(buffers->mBuffers[c].mData)[f]=c?.1f:.2f;
    return noErr;
}
int main(int argc,char** argv){
    assert(argc==2 || argc==3);
    const bool gain=argc==3 && std::strcmp(argv[2],"gain")==0;
    assert(argc==2 || gain);
    void* library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!library){std::fprintf(stderr,"%s\n",dlerror());return 1;}
    auto factory=reinterpret_cast<AudioComponentFactoryFunction>(dlsym(library,"wrapAsAUV2_inst0Factory"));assert(factory);
    AudioComponentDescription desc{kAudioUnitType_Effect,gain?UInt32('ClGn'):UInt32('HtMS'),gain?UInt32('Exmp'):UInt32('SyCi'),0,0};
    auto component=AudioComponentRegister(&desc,CFSTR("Portable plugin qualification"),0x100,factory);assert(component);
    int cases=0;
    for(double rate:{44100.,48000.,96000.}){
        AudioUnit unit=nullptr;check(AudioComponentInstanceNew(component,&unit));
        AudioStreamBasicDescription format{rate,kAudioFormatLinearPCM,kAudioFormatFlagsNativeFloatPacked|kAudioFormatFlagIsNonInterleaved,4,1,4,2,32,0};
        check(AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input,0,&format,sizeof(format)));
        check(AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output,0,&format,sizeof(format)));
        UInt32 frames=128;check(AudioUnitSetProperty(unit,kAudioUnitProperty_MaximumFramesPerSlice,kAudioUnitScope_Global,0,&frames,sizeof(frames)));
        AURenderCallbackStruct callback{input,nullptr};
        check(AudioUnitSetProperty(unit,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,0,&callback,sizeof(callback)));
        UInt32 bytes=0;check(AudioUnitGetPropertyInfo(unit,kAudioUnitProperty_ParameterList,kAudioUnitScope_Global,0,&bytes,nullptr));assert(bytes==sizeof(AudioUnitParameterID));
        AudioUnitParameterID parameter;check(AudioUnitGetProperty(unit,kAudioUnitProperty_ParameterList,kAudioUnitScope_Global,0,&parameter,&bytes));
        check(AudioUnitInitialize(unit));
        float data[2][128]{};struct {UInt32 count;AudioBuffer buffers[2];} output{2,{{1,sizeof(data[0]),data[0]},{1,sizeof(data[1]),data[1]}}};
        AudioTimeStamp timestamp{};timestamp.mFlags=kAudioTimeStampSampleTimeValid;
        auto render=[&](float left,float right){
            for(unsigned n:{1u,7u,128u}){
                for(auto& buffer:output.buffers)buffer.mDataByteSize=n*sizeof(float);
                AudioUnitRenderActionFlags flags=0;
                check(AudioUnitRender(unit,&flags,&timestamp,0,n,reinterpret_cast<AudioBufferList*>(&output)));
                for(unsigned f=0;f<n;f++)assert(std::isfinite(data[0][f])&&std::isfinite(data[1][f])&&std::abs(data[0][f]-left)<1e-5&&std::abs(data[1][f]-right)<1e-5);
                timestamp.mSampleTime+=n;++cases;
            }
        };
        check(AudioUnitSetParameter(unit,parameter,kAudioUnitScope_Global,0,gain?1:0,0));render(.2f,.1f);
        CFPropertyListRef saved=nullptr;bytes=sizeof(saved);check(AudioUnitGetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&saved,&bytes));assert(saved);
        check(AudioUnitSetParameter(unit,parameter,kAudioUnitScope_Global,0,gain?.5f:-5.f,0));render(gain?.1f:.15f,gain?.05f:.15f);
        check(AudioUnitSetProperty(unit,kAudioUnitProperty_ClassInfo,kAudioUnitScope_Global,0,&saved,sizeof(saved)));CFRelease(saved);render(.2f,.1f);
        check(AudioUnitUninitialize(unit));check(AudioComponentInstanceDispose(unit));
    }
    // Keep the library loaded for the lifetime of the registered component.
    std::printf("AUv2: %d numerical renders plus parameter/state checks passed\n",cases);
}
