#include "creature/dsp/GestureEngine.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
int main(){try{lsse::creature::GestureEngine a,b;a.prepare(48000,77);b.prepare(48000,77);std::vector<float>x(4096);double energy=0;for(int i=0;i<4096;++i){const float in=i<512?0.4f*std::sin(i*0.07f):0.0f;x[i]=a.process(in,0.7f,0.8f,0.6f);const auto y=b.process(in,0.7f,0.8f,0.6f);if(x[i]!=y||!std::isfinite(y))throw std::runtime_error("seeded gesture was not deterministic");energy+=y*y;}if(energy<=0)throw std::runtime_error("gesture was silent");a.reset();for(int i=0;i<256;++i)if(a.process(0,1,1,1)!=0)throw std::runtime_error("silence policy failed");std::cout<<"CREATURE gesture tests passed\n";return 0;}catch(const std::exception&x){std::cerr<<x.what()<<'\n';return 1;}}
