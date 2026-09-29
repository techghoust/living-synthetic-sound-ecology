#include "convolution_lab/dsp/ReferenceConvolver.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
int main(){try{lsse::convolution::ReferenceConvolver c;c.setImpulse({1});if(c.process(0.25f)!=0.25f)throw std::runtime_error("identity IR failed");c.setImpulse({1,0.5f});if(std::abs(c.process(1)-1)>1e-6||std::abs(c.process(0)-0.5f)>1e-6)throw std::runtime_error("reference convolution failed");const auto r=lsse::convolution::ReferenceConvolver::resampleLinear({0,1,0},48000,96000);if(r.size()!=6||!std::isfinite(r[3]))throw std::runtime_error("IR resampling failed");const auto edited=lsse::convolution::ReferenceConvolver::transformImpulse({1,.5f,.25f,0},0,1,true,2,0.5f);if(edited.size()!=8||!std::isfinite(edited.back())||std::abs(edited.front())>0.001f)throw std::runtime_error("IR edit pipeline failed");std::cout<<"CONVOLUTION reference tests passed\n";return 0;}catch(const std::exception&x){std::cerr<<x.what()<<'\n';return 1;}}
