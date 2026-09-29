#include "impact/dsp/TransientBodyEngine.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
int main(){try{lsse::impact::TransientBodyEngine e;e.prepare(48000);double energy=0;for(int i=0;i<48000;++i){const auto y=e.process(i==127?1.0f:0.0f,0.8f,0.0f,0.4f);if(!std::isfinite(y)||std::abs(y)>1.001f)throw std::runtime_error("unsafe body output");energy+=y*y;}if(e.getTriggerCount()!=1)throw std::runtime_error("transient duplicated or missed");if(energy<=0)throw std::runtime_error("body was silent");std::cout<<"IMPACT transient/body tests passed\n";return 0;}catch(const std::exception&x){std::cerr<<x.what()<<'\n';return 1;}}
