#include "EnvironmentProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include "shared/ui/LsseProductEditor.h"
#include "shared/state/StateMigration.h"

namespace { constexpr int stateVersion = 3; }
EnvironmentAudioProcessor::EnvironmentAudioProcessor() : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)), parameters (*this, nullptr, "ENVIRONMENT_STATE", createParameterLayout()) {}
void EnvironmentAudioProcessor::prepareToPlay(double sampleRate,int){scheduler.prepare(sampleRate,(std::uint32_t)parameters.getRawParameterValue("seed")->load());timeline=0;spaceState={};toneState={};}
bool EnvironmentAudioProcessor::isBusesLayoutSupported (const BusesLayout& b) const { const auto i = b.getMainInputChannelSet(), o = b.getMainOutputChannelSet(); return i == o && (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()); }
void EnvironmentAudioProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals n;const auto pct=[&](const char* id){return parameters.getRawParameterValue(id)->load()/100.0f;};const auto generator=parameters.getRawParameterValue("generator")->load()>0.5f;const auto density=pct("density"),variation=pct("variation"),scale=pct("scale"),bed=pct("bed"),weather=pct("weather"),events=pct("events"),follow=pct("input_follow"),perspective=pct("perspective"),space=pct("space"),decay=pct("decay"),motion=pct("motion"),width=parameters.getRawParameterValue("width")->load()/100.0f;const auto distance=pct("distance"),room=pct("room"),air=pct("air"),obstruction=pct("obstruction"),reflection=pct("reflection"),spaceSize=pct("space_size");const auto tone=parameters.getRawParameterValue("tone")->load()/100.0f,low=parameters.getRawParameterValue("low")->load()/100.0f,high=parameters.getRawParameterValue("high")->load()/100.0f;const auto mix=pct("mix");const auto gain=std::pow(10.0f,parameters.getRawParameterValue("output")->load()/20.0f);
    for(int s=0;s<b.getNumSamples();++s,++timeline){const auto layers=scheduler.layersAt(timeline,generator,density,variation,scale);const auto movement=static_cast<float>(std::sin(timeline*0.000071))*motion;for(int c=0;c<b.getNumChannels();++c){const auto raw=b.getSample(c,s);const auto dry=std::isfinite(raw)?raw:0.0f;const auto side=c==0?-1.0f:1.0f;const auto absorption=juce::jlimit(0.002f,0.75f,0.62f-air*0.42f-obstruction*0.34f-distance*0.16f);toneState[(std::size_t)c]+=(dry-toneState[(std::size_t)c])*absorption;const auto attenuated=toneState[(std::size_t)c]/(1.0f+distance*3.5f+obstruction*1.6f);auto generated=generator?(layers.bed*bed+layers.weather*weather*(1.0f+side*width*0.18f)+layers.events*events*(1.0f+side*movement*0.25f)):0.0f;generated*=0.18f+0.32f*follow;const auto feedback=juce::jlimit(0.0f,0.985f,0.42f+decay*0.38f+spaceSize*0.12f);spaceState[(std::size_t)c]=attenuated*(0.12f+room*0.3f)+generated+spaceState[(std::size_t)c]*feedback;const auto reflected=spaceState[(std::size_t)c]*(reflection*0.38f+space*0.16f);auto environmental=(attenuated*(0.45f+follow*0.55f)+reflected+generated)*(1.0f-perspective*0.38f);environmental=environmental*(1.0f+low*0.22f)+toneState[(std::size_t)c]*high*0.12f;b.setSample(c,s,(dry+(environmental-dry)*mix)*gain);}}
}
juce::AudioProcessorEditor* EnvironmentAudioProcessor::createEditor()
{
    using namespace lsse::ui;
    return new ProductEditor (*this, parameters, "ENVIRONMENT", "Place a recognizable source inside a changing environment", juce::Colour (0xff55c9bc),
        { { "Source", { "input_follow", "distance", "air", "obstruction", "generator" } },
          { "Room", { "room", "reflection", "space_size", "space", "decay" } },
          { "Optional ecology", { "bed", "weather", "events", "density", "variation" } },
          { "Perspective", { "scale", "perspective", "motion", "tone", "width" } },
          { "Spectrum", { "low", "high", "seed", "mix", "output" } } },
        { { "Quiet Habitat", { {"generator",1},{"mix",1},{"bed",.7f},{"weather",.18f},{"events",.08f},{"density",.22f},{"space",.62f} } },
          { "Electric Weather", { {"generator",1},{"mix",1},{"bed",.35f},{"weather",.92f},{"events",.42f},{"density",.78f},{"variation",.82f} } },
          { "Distant Signals", { {"generator",1},{"mix",1},{"bed",.28f},{"events",.9f},{"perspective",.8f},{"space",.86f},{"decay",.82f} } } });
}
void EnvironmentAudioProcessor::getStateInformation (juce::MemoryBlock& d) { auto s = parameters.copyState(); s.setProperty ("stateVersion", stateVersion, nullptr); if (auto x = s.createXml()) copyXmlToBinary (*x, d); }
void EnvironmentAudioProcessor::setStateInformation (const void* d, int z) { if (auto x = getXmlFromBinary (d, z); x && x->hasTagName (parameters.state.getType())) parameters.replaceState (lsse::state::mergeParameterState(parameters.copyState(),juce::ValueTree::fromXml (*x))); }

juce::AudioProcessorValueTreeState::ParameterLayout EnvironmentAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l; const auto pct = juce::AudioParameterFloatAttributes{}.withLabel ("%");
    auto p = [&] (const char* id, const char* name, float value) { l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> { 0, 100, 0.1f }, value, pct)); };
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "generator", 1 }, "GENERATOR", false));
    p ("input_follow", "INPUT FOLLOW", 100);p("distance","DISTANCE",0);p("room","ROOM",35);p("air","AIR",0);p("obstruction","OBSTRUCTION",0);p("reflection","REFLECTION",30);p("space_size","SPACE SIZE",50); p ("bed", "BED", 45); p ("weather", "WEATHER", 15); p ("events", "EVENTS", 10); p ("density", "DENSITY", 35); p ("variation", "VARIATION", 25); p ("scale", "SCALE", 50); p ("perspective", "PERSPECTIVE", 50); p ("space", "SPACE", 35); p ("decay", "DECAY", 50); p ("motion", "MOTION", 20);
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tone", 1 }, "TONE", juce::NormalisableRange<float> { -100, 100, 0.1f }, 0, pct));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "low", 1 }, "LOW", juce::NormalisableRange<float> { -100, 100, 0.1f }, 0, pct));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "high", 1 }, "HIGH", juce::NormalisableRange<float> { -100, 100, 0.1f }, 0, pct));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "width", 1 }, "WIDTH", juce::NormalisableRange<float> { 0, 200, 0.1f }, 100, pct));
    l.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "seed", 1 }, "SEED", 0, 9999, 1));
    p ("mix", "MIX", 0); l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "output", 1 }, "OUTPUT", juce::NormalisableRange<float> { -24, 6, 0.1f }, 0, juce::AudioParameterFloatAttributes{}.withLabel ("dB")));
    return l;
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EnvironmentAudioProcessor(); }
