#include "ImpactProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include "shared/ui/LsseProductEditor.h"
#include "shared/state/StateMigration.h"

namespace { constexpr int stateVersion = 3; }
ImpactAudioProcessor::ImpactAudioProcessor() : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)), parameters(*this,nullptr,"IMPACT_STATE",createParameterLayout()) {}
void ImpactAudioProcessor::prepareToPlay(double newSampleRate,int){sampleRate=newSampleRate;contactPosition={};debrisPosition={};detectorEnvelope={};for(auto& e:engines)e.prepare(sampleRate);}
bool ImpactAudioProcessor::isBusesLayoutSupported(const BusesLayout& b) const { const auto i=b.getMainInputChannelSet(),o=b.getMainOutputChannelSet(); return i==o&&(o==juce::AudioChannelSet::mono()||o==juce::AudioChannelSet::stereo()); }
void ImpactAudioProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&)
{
    juce::ScopedNoDenormals n;if(layerResetRequested.exchange(false,std::memory_order_acq_rel)){contactPosition={};debrisPosition={};}
    const auto sensitivity=parameters.getRawParameterValue("sensitivity")->load()/100.0f;
    const auto pitch=parameters.getRawParameterValue("body_pitch")->load()/24.0f;
    const auto decay=juce::jlimit(0.0f,1.0f,parameters.getRawParameterValue("decay")->load()/1500.0f);
    const auto amount=parameters.getRawParameterValue("body")->load()/100.0f;
    const auto pitchDrop=parameters.getRawParameterValue("pitch_drop")->load()/100.0f;
    const auto attack=parameters.getRawParameterValue("attack")->load()/100.0f;
    const auto attackTone=parameters.getRawParameterValue("attack_tone")->load()/100.0f;
    const auto sub=parameters.getRawParameterValue("sub")->load()/100.0f;
    const auto debris=parameters.getRawParameterValue("debris")->load()/100.0f;
    const auto tail=parameters.getRawParameterValue("tail")->load()/100.0f;
    const auto tailSpace=parameters.getRawParameterValue("tail_space")->load()/100.0f;
    const auto drive=parameters.getRawParameterValue("drive")->load()/100.0f;
    const auto width=parameters.getRawParameterValue("width")->load()/100.0f;
    const auto clipMode=static_cast<int>(parameters.getRawParameterValue("clip_mode")->load());
    const auto mix=parameters.getRawParameterValue("mix")->load()/100.0f;
    const auto gain=std::pow(10.0f,parameters.getRawParameterValue("output")->load()/20.0f);
    const auto ceiling=std::pow(10.0f,parameters.getRawParameterValue("ceiling")->load()/20.0f);
    const auto size=parameters.getRawParameterValue("size")->load()/100.0f;const auto mass=parameters.getRawParameterValue("mass")->load()/100.0f;const auto hardness=parameters.getRawParameterValue("hardness")->load()/100.0f;const auto force=parameters.getRawParameterValue("force")->load()/100.0f;const auto contactLevel=parameters.getRawParameterValue("contact_sample")->load()/100.0f;const auto debrisLevel=parameters.getRawParameterValue("debris_sample")->load()/100.0f;const auto contactData=layerSamples[0].get();const auto debrisData=layerSamples[1].get();
    const auto channels=juce::jmin(b.getNumChannels(),static_cast<int>(engines.size()));
    for(int c=0;c<channels;++c)for(int s=0;s<b.getNumSamples();++s){const auto raw=b.getSample(c,s);const auto dry=std::isfinite(raw)?raw:0.0f;const auto magnitude=std::abs(dry);const auto threshold=0.015f+(1.0f-sensitivity)*0.2f;const auto triggered=magnitude>threshold&&detectorEnvelope[(std::size_t)c]<=threshold;if(triggered){contactPosition[(std::size_t)c]=0.0;debrisPosition[(std::size_t)c]=0.0;}detectorEnvelope[(std::size_t)c]=magnitude+0.995f*(detectorEnvelope[(std::size_t)c]-magnitude);const auto physicalPitch=pitch+(0.5f-size)*0.7f+(0.5f-mass)*0.45f;const auto physicalDecay=juce::jlimit(0.0f,1.0f,decay*(0.55f+mass*0.75f)+size*0.15f);auto wet=engines[(std::size_t)c].process(dry,sensitivity,physicalPitch,physicalDecay,juce::jlimit(0.0f,1.0f,pitchDrop*(0.5f+force)),juce::jlimit(0.0f,1.0f,attack*(0.45f+force)),juce::jlimit(0.0f,1.0f,attackTone*(0.35f+hardness)),sub*(0.4f+mass*0.8f),debris*(0.35f+hardness),tail*(0.4f+size*0.8f),tailSpace)*amount*(0.45f+force*0.9f);if(contactData!=nullptr&&contactPosition[(std::size_t)c]<contactData->audio.getNumSamples()){wet+=lsse::audio::UserSampleSlot::readLinear(*contactData,c,contactPosition[(std::size_t)c])*contactLevel*(0.25f+force*1.25f);contactPosition[(std::size_t)c]+=contactData->sampleRate/sampleRate*std::pow(2.0,(hardness-0.5)*1.5);}if(debrisData!=nullptr&&debrisPosition[(std::size_t)c]<debrisData->audio.getNumSamples()){const auto envelope=std::exp(-debrisPosition[(std::size_t)c]/juce::jmax(1.0,debrisData->sampleRate*(0.08+size*1.2)));wet+=lsse::audio::UserSampleSlot::readLinear(*debrisData,c,debrisPosition[(std::size_t)c])*debrisLevel*(0.2f+hardness)*static_cast<float>(envelope);debrisPosition[(std::size_t)c]+=debrisData->sampleRate/sampleRate*(1.35-mass*0.55);}const auto driven=wet*(1.0f+drive*11.0f);if(clipMode==0)wet=std::tanh(driven);else if(clipMode==1)wet=juce::jlimit(-1.0f,1.0f,driven);else wet=std::sin(driven);b.setSample(c,s,(dry+(wet-dry)*mix)*gain);}
    if(channels==2)for(int s=0;s<b.getNumSamples();++s){const auto l=b.getSample(0,s),r=b.getSample(1,s),mid=(l+r)*0.5f,side=(l-r)*0.5f*width;b.setSample(0,s,juce::jlimit(-ceiling,ceiling,mid+side));b.setSample(1,s,juce::jlimit(-ceiling,ceiling,mid-side));}
    else if(channels==1)for(int s=0;s<b.getNumSamples();++s)b.setSample(0,s,juce::jlimit(-ceiling,ceiling,b.getSample(0,s)));
}
juce::AudioProcessorEditor* ImpactAudioProcessor::createEditor()
{
    using namespace lsse::ui;
    return new ProductEditor (*this, parameters, "IMPACT", "Transient anatomy and cinematic mass", juce::Colour (0xffff5a5f),
        { { "Physical", { "size", "mass", "hardness", "force" } },
          { "Detection", { "sensitivity", "lookahead", "attack", "attack_tone" } },
          { "Body", { "body", "body_pitch", "pitch_drop", "decay", "sub" } },
          { "Debris and tail", { "debris", "debris_size", "tail", "tail_space", "width" } },
          { "Samples", { "contact_sample", "debris_sample" } },
          { "Safety", { "drive", "clip_mode", "oversampling", "mix", "ceiling", "output" } } },
        { { "Deep Hit", { {"mix",1},{"body",.92f},{"sub",.78f},{"pitch_drop",.72f},{"decay",.55f},{"drive",.22f} } },
          { "Broken Metal", { {"mix",1},{"attack",.82f},{"attack_tone",.9f},{"debris",.88f},{"tail",.35f},{"drive",.48f} } },
          { "Long Collapse", { {"mix",1},{"body",.7f},{"debris",.62f},{"tail",.9f},{"tail_space",.86f},{"decay",.82f} } } },
        { { "LOAD CONTACT", [this]{chooseLayerSample(0);} }, { "LOAD DEBRIS", [this]{chooseLayerSample(1);} } });
}
void ImpactAudioProcessor::chooseLayerSample(int slot){if(!juce::isPositiveAndBelow(slot,2)||fileChooser!=nullptr)return;fileChooser=std::make_unique<juce::FileChooser>(slot==0?"Load IMPACT contact":"Load IMPACT debris",juce::File::getSpecialLocation(juce::File::userMusicDirectory),"*.wav;*.aif;*.aiff;*.flac",true);fileChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this,slot](const juce::FileChooser&c){const auto f=c.getResult();if(f.existsAsFile())loadLayerSample(slot,f);fileChooser.reset();});}
bool ImpactAudioProcessor::loadLayerSample(int slot,const juce::File&f){if(!juce::isPositiveAndBelow(slot,2)||!layerSamples[(std::size_t)slot].load(f))return false;layerResetRequested.store(true,std::memory_order_release);const juce::ScopedLock lock(layerPathLock);layerPaths[(std::size_t)slot]=f.getFullPathName();return true;}
void ImpactAudioProcessor::clearLayerSample(int slot){if(!juce::isPositiveAndBelow(slot,2))return;layerSamples[(std::size_t)slot].clear();layerResetRequested.store(true,std::memory_order_release);const juce::ScopedLock lock(layerPathLock);layerPaths[(std::size_t)slot].clear();}
void ImpactAudioProcessor::getStateInformation(juce::MemoryBlock& d){auto s=parameters.copyState();s.setProperty("stateVersion",stateVersion,nullptr);{const juce::ScopedLock lock(layerPathLock);s.setProperty("contactSamplePath",layerPaths[0],nullptr);s.setProperty("debrisSamplePath",layerPaths[1],nullptr);}if(auto x=s.createXml())copyXmlToBinary(*x,d);}
void ImpactAudioProcessor::setStateInformation(const void* d,int z){if(auto x=getXmlFromBinary(d,z);x&&x->hasTagName(parameters.state.getType())){auto s=juce::ValueTree::fromXml(*x);parameters.replaceState(lsse::state::mergeParameterState(parameters.copyState(),s));for(int slot=0;slot<2;++slot){const auto path=s.getProperty(slot==0?"contactSamplePath":"debrisSamplePath").toString();if(path.isNotEmpty())loadLayerSample(slot,juce::File(path));else clearLayerSample(slot);}}}
juce::AudioProcessorValueTreeState::ParameterLayout ImpactAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;const auto pct=juce::AudioParameterFloatAttributes{}.withLabel("%");auto p=[&](const char*id,const char*n,float v){l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},n,juce::NormalisableRange<float>{0,100,0.1f},v,pct));};
    p("size","SIZE",50);p("mass","MASS",60);p("hardness","HARDNESS",55);p("force","FORCE",65);p("sensitivity","SENSITIVITY",50);l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"lookahead",1},"LOOKAHEAD",juce::NormalisableRange<float>{0,20,0.1f},0,juce::AudioParameterFloatAttributes{}.withLabel("ms")));
    p("attack","ATTACK",55);p("attack_tone","ATTACK TONE",60);p("body","BODY",65);l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"body_pitch",1},"BODY PITCH",juce::NormalisableRange<float>{-24,24,0.1f},-5,juce::AudioParameterFloatAttributes{}.withLabel("st")));p("pitch_drop","PITCH DROP",55);
    l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"decay",1},"DECAY",juce::NormalisableRange<float>{20,4000,1,0.35f},500,juce::AudioParameterFloatAttributes{}.withLabel("ms")));
    p("sub","SUB",40);p("debris","DEBRIS",30);l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"debris_size",1},"DEBRIS SIZE",juce::NormalisableRange<float>{5,500,1,0.4f},80,juce::AudioParameterFloatAttributes{}.withLabel("ms")));p("tail","TAIL",30);p("tail_space","TAIL SPACE",50);p("contact_sample","CONTACT SAMPLE",55);p("debris_sample","DEBRIS SAMPLE",45);p("drive","DRIVE",20);
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"clip_mode",1},"CLIP MODE",juce::StringArray{"SOFT","HARD","FOLD"},0));l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"oversampling",1},"OVERSAMPLING",juce::StringArray{"OFF","2X","4X"},0));p("width","WIDTH",100);p("mix","MIX",0);
    l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"ceiling",1},"CEILING",juce::NormalisableRange<float>{-12,0,0.1f},-0.5f,juce::AudioParameterFloatAttributes{}.withLabel("dB")));l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"output",1},"OUTPUT",juce::NormalisableRange<float>{-24,6,0.1f},0,juce::AudioParameterFloatAttributes{}.withLabel("dB")));return l;
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new ImpactAudioProcessor();}
