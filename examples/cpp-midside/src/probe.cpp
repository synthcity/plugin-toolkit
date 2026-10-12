#include "MidSide.hpp"
#include "MinMax.hpp"
#include "Exponent.hpp"
#include <cstdio>
#include <cstdlib>
static HCVModule* create(int kind){switch(kind){case 0:return new MidSide;case 1:return new MinMax;case 2:return new Exponent;default:return nullptr;}}
extern "C" {
HCVModule* module_create(int kind){return create(kind);}
void module_destroy(HCVModule* m){delete m;}
int module_input(HCVModule* m,int port,int channels,const float* v){
    if(!m||port<0||port>=(int)m->inputs.size()||channels<0||channels>16||(!v&&channels))return 0;
    auto& p=m->inputs[port];p.volts.fill(0);p.channels=channels;if(channels)std::copy_n(v,channels,p.volts.data());return 1;
}
int module_param(HCVModule* m,int id,float v){if(!m||id<0||id>=(int)m->params.size()||!std::isfinite(v))return 0;auto& p=m->paramInfo[id];m->params[id].value=std::clamp(p.stepped?std::round(v):v,p.min,p.max);return 1;}
int module_process(HCVModule* m,float rate){if(!m||!std::isfinite(rate)||rate<8000)return 0;m->process({rate,1/rate});return 1;}
float* module_output(HCVModule* m,int port){return m&&port>=0&&port<(int)m->outputs.size()?m->outputs[port].volts.data():nullptr;}
int module_channels(HCVModule* m,int port){return m&&port>=0&&port<(int)m->outputs.size()?m->outputs[port].channels:-1;}
}
#ifdef NATIVE_TEST
#include <cassert>
int main(){
    for(int k=0;k<3;k++){auto* m=create(k);float v[16];for(int i=0;i<16;i++)v[i]=i*.25f-2;assert(module_input(m,0,16,v));assert(module_process(m,48000));assert(module_channels(m,0)==16);for(int i=0;i<16;i++){float expected=k==0?v[i]*.5f:v[i];assert(std::abs(module_output(m,0)[i]-expected)<1e-5f);}module_destroy(m);}
    puts("Native: three original DSP implementations, 16 channels passed");
}
#endif
