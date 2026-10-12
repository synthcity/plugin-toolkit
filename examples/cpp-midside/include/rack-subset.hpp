// SPDX-License-Identifier: MIT
// Copyright 2026 SynthCity. Independently implemented, deliberately narrow API subset.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>
namespace simd {
struct float_4 {
    float v[4];
    float_4(float x=0) : v{x,x,x,x} {}
    static float_4 load(const float* p) { float_4 x; std::copy_n(p,4,x.v); return x; }
    void store(float* p) const { std::copy_n(v,4,p); }
    float& operator[](int i) { return v[i]; }
    float operator[](int i) const { return v[i]; }
    float_4& operator*=(float_4 b);
    float_4& operator+=(float_4 b);
};
#define VECTOR_OP(op) inline float_4 operator op(float_4 a,float_4 b) { for(int i=0;i<4;i++)a.v[i]=a.v[i] op b.v[i];return a; }
VECTOR_OP(+) VECTOR_OP(-) VECTOR_OP(*)
#undef VECTOR_OP
inline float_4& float_4::operator*=(float_4 b) { return *this=*this*b; }
inline float_4& float_4::operator+=(float_4 b) { return *this=*this+b; }
#define VECTOR_FN(name,expr) inline float_4 name(float_4 a,float_4 b) { for(int i=0;i<4;i++)a.v[i]=(expr);return a; }
VECTOR_FN(fmin,std::fmin(a.v[i],b.v[i])) VECTOR_FN(fmax,std::fmax(a.v[i],b.v[i])) VECTOR_FN(pow,std::pow(a.v[i],b.v[i]))
#undef VECTOR_FN
inline float_4 abs(float_4 a) { for(auto& x:a.v)x=std::abs(x);return a; }
inline float_4 sgn(float_4 a) { for(auto& x:a.v)x=(x>0)-(x<0);return a; }
}
inline float clamp(float x,float lo,float hi) {return std::clamp(x,lo,hi);}
inline simd::float_4 clamp(simd::float_4 a,float lo,float hi) {for(auto& x:a.v)x=clamp(x,lo,hi);return a;}
struct Param { float value=0; float getValue()const{return value;} };
struct Port {
    std::array<float,16> volts{};
    int channels=0;
    bool isConnected()const{return channels>0;}
    int getChannels()const{return channels;}
    void setChannels(int n){channels=std::clamp(n,0,16);}
    float* getVoltages(int c=0){return volts.data()+c;}
};
struct Light {void setSmoothBrightness(float,float){}}; // DSP-only test target has no lights.
struct ParamInfo {float min=0,max=0,def=0;std::string name;bool stepped=false;};
struct HCVModule {
    struct ProcessArgs {float sampleRate=48000,sampleTime=1.f/48000;};
    std::vector<Param> params;
    std::vector<Port> inputs,outputs;
    std::vector<Light> lights;
    std::vector<ParamInfo> paramInfo;
    virtual ~HCVModule()=default;
    virtual void process(const ProcessArgs&)=0;
    void config(int p,int i,int o,int l=0){params.resize(p);paramInfo.resize(p);inputs.resize(i);outputs.resize(o);lights.resize(l);}
    void configParam(int i,float lo,float hi,float def,const char* name){paramInfo.at(i)={lo,hi,def,name,false};params.at(i).value=def;}
    void configSwitch(int i,float lo,float hi,float def,const char* name,std::initializer_list<const char*>){configParam(i,lo,hi,def,name);paramInfo.at(i).stepped=true;}
    void configInput(int,const char*){} void configOutput(int,const char*){}
    void configBypass(int,int){} // Mapping recorded in the guide, not an implemented bypass host.
    int getMaxInputPolyphony(){int n=1;for(auto& p:inputs)n=std::max(n,p.channels);return n;}
};
