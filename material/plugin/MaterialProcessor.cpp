#include "MaterialProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include "shared/ui/LsseProductEditor.h"
#include "shared/state/StateMigration.h"

namespace { constexpr int stateVersion = 3; }
MaterialAudioProcessor::MaterialAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "MATERIAL_STATE", createParameterLayout()) {}
void MaterialAudioProcessor::prepareToPlay (double newSampleRate, int) { sampleRate = newSampleRate; excitationPhase = 0.0; previousInput = {}; for (auto& engine : engines) engine.prepare (sampleRate); }
bool MaterialAudioProcessor::isBusesLayoutSupported (const BusesLayout& b) const { const auto i=b.getMainInputChannelSet(), o=b.getMainOutputChannelSet(); return i==o && (o==juce::AudioChannelSet::mono() || o==juce::AudioChannelSet::stereo()); }
void MaterialAudioProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals n;
    const auto size = parameters.getRawParameterValue ("size")->load() / 100.0f;
    const auto damping = parameters.getRawParameterValue ("damping")->load() / 100.0f;
    const auto resonance = parameters.getRawParameterValue ("resonance")->load() / 100.0f;
    const auto body = parameters.getRawParameterValue ("body")->load() / 100.0f;
    auto materialX = parameters.getRawParameterValue ("material_x")->load() / 100.0f;
    auto materialY = parameters.getRawParameterValue ("material_y")->load() / 100.0f;
    const auto family = static_cast<int> (parameters.getRawParameterValue ("material_family")->load());
    const auto familyAmount = parameters.getRawParameterValue ("family_amount")->load() / 100.0f;
    constexpr std::array<std::pair<float,float>,7> families {{{.9f,.32f},{.58f,.96f},{.28f,.35f},{.42f,.18f},{.72f,.55f},{.12f,.82f},{.2f,.22f}}};
    if (family > 0) { const auto target=families[(std::size_t)juce::jlimit(0,6,family-1)]; materialX += (target.first-materialX)*familyAmount; materialY += (target.second-materialY)*familyAmount; }
    const auto contact = parameters.getRawParameterValue ("contact")->load() / 100.0f;
    const auto hardness = parameters.getRawParameterValue ("hardness")->load() / 100.0f;
    const auto shape = parameters.getRawParameterValue ("shape")->load() / 100.0f;
    const auto inharmonicity = parameters.getRawParameterValue ("inharmonicity")->load() / 100.0f;
    const auto scrapeRate = parameters.getRawParameterValue ("scrape_rate")->load();
    const auto bowPressure = parameters.getRawParameterValue ("bow_pressure")->load() / 100.0f;
    const auto stereo = parameters.getRawParameterValue ("stereo")->load() / 100.0f;
    const auto excitationMode = static_cast<int> (parameters.getRawParameterValue ("excitation_mode")->load());
    const auto mix = parameters.getRawParameterValue ("mix")->load() / 100.0f;
    const auto nonlinear = parameters.getRawParameterValue ("nonlinear")->load() / 100.0f;
    const auto gain = std::pow (10.0f, parameters.getRawParameterValue ("output")->load() / 20.0f);
    const auto channels = juce::jmin (b.getNumChannels(), static_cast<int> (engines.size()));
    for (int s = 0; s < b.getNumSamples(); ++s)
    {
        const auto scrape = static_cast<float> (0.5 + 0.5 * std::sin (excitationPhase));
        excitationPhase += 2.0 * juce::MathConstants<double>::pi * scrapeRate / sampleRate;
        if (excitationPhase >= 2.0 * juce::MathConstants<double>::pi) excitationPhase -= 2.0 * juce::MathConstants<double>::pi;
        for (int c = 0; c < channels; ++c)
        {
            const auto raw = b.getSample (c, s);
            const auto dry = std::isfinite (raw) ? raw : 0.0f;
            float excitation = dry;
            if (excitationMode == 0) excitation = dry - previousInput[(std::size_t) c];
            else if (excitationMode == 2) excitation = std::tanh (dry * (1.0f + 7.0f * bowPressure)) * scrape;
            else if (excitationMode == 3) excitation = std::abs (dry - previousInput[(std::size_t) c]) > (0.02f + 0.18f * (1.0f - contact)) ? dry : 0.0f;
            previousInput[(std::size_t) c] = dry;
            const auto profileOffset = channels == 2 ? (c == 0 ? -0.08f : 0.08f) * stereo : 0.0f;
            auto wet = engines[(std::size_t)c].process (excitation, juce::jlimit (0.0f, 1.0f, size + (shape - 0.5f) * 0.25f), damping, resonance,
                                                                     juce::jlimit (0.0f, 1.0f, materialX + profileOffset), materialY,
                                                                     contact, hardness, inharmonicity) * body;
            wet = wet + (std::tanh (wet * (1.0f + nonlinear * 7.0f)) - wet) * nonlinear;
            b.setSample (c, s, (dry + (wet - dry) * mix) * gain);
        }
    }
}
juce::AudioProcessorEditor* MaterialAudioProcessor::createEditor()
{
    using namespace lsse::ui;
    return new ProductEditor (*this, parameters, "MATERIAL", "Resonant matter and physical surfaces", juce::Colour (0xffffa64d),
        { { "Material family", { "material_family", "family_amount", "material_x", "material_y" } },
          { "Material field", { "size", "shape", "inharmonicity", "nonlinear" } },
          { "Excitation", { "excitation_mode", "contact", "hardness", "scrape_rate", "bow_pressure" } },
          { "Body", { "damping", "resonance", "body", "stereo" } },
          { "Output", { "seed", "mix", "output" } } },
        { { "Soft Body", { {"mix",1},{"material_x",.15f},{"material_y",.2f},{"size",.7f},{"damping",.68f},{"body",.78f} } },
          { "Metal Skin", { {"mix",1},{"material_x",.92f},{"material_y",.35f},{"hardness",.9f},{"inharmonicity",.7f},{"resonance",.76f} } },
          { "Glass Cavity", { {"mix",1},{"material_x",.58f},{"material_y",.96f},{"size",.42f},{"damping",.28f},{"resonance",.86f} } } });
}
void MaterialAudioProcessor::getStateInformation (juce::MemoryBlock& d) { auto s=parameters.copyState(); s.setProperty("stateVersion",stateVersion,nullptr); if(auto x=s.createXml()) copyXmlToBinary(*x,d); }
void MaterialAudioProcessor::setStateInformation (const void* d,int z) { if(auto x=getXmlFromBinary(d,z); x && x->hasTagName(parameters.state.getType())) parameters.replaceState(lsse::state::mergeParameterState(parameters.copyState(),juce::ValueTree::fromXml(*x))); }
juce::AudioProcessorValueTreeState::ParameterLayout MaterialAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l; const auto pct=juce::AudioParameterFloatAttributes{}.withLabel("%");
    auto p=[&](const char* id,const char* name,float value){ l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,juce::NormalisableRange<float>{0,100,0.1f},value,pct)); };
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"material_family",1},"MATERIAL FAMILY",juce::StringArray{"MORPH","METAL","GLASS","WOOD","PLASTIC","STONE","LIQUID","ORGANIC"},0));p("family_amount","FAMILY AMOUNT",100);p("material_x","MATERIAL X",35); p("material_y","MATERIAL Y",45);p("nonlinear","NONLINEAR",15);
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"excitation_mode",1},"EXCITATION",juce::StringArray{"TRANSIENT","CONTINUOUS","BOW","IMPULSE"},0));
    p("contact","CONTACT",50); p("hardness","HARDNESS",55); p("size","SIZE",50); p("shape","SHAPE",50); p("damping","DAMPING",45); p("inharmonicity","INHARMONICITY",25); p("resonance","RESONANCE",50);
    l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"scrape_rate",1},"SCRAPE RATE",juce::NormalisableRange<float>{0.1f,30.0f,0.01f,0.4f},3.0f,juce::AudioParameterFloatAttributes{}.withLabel("Hz")));
    p("bow_pressure","BOW PRESSURE",35); p("body","BODY",60); p("stereo","STEREO",100);
    l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"seed",1},"SEED",1,65535,1977)); p("mix","MIX",0);
    l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"output",1},"OUTPUT",juce::NormalisableRange<float>{-24,6,0.1f},0,juce::AudioParameterFloatAttributes{}.withLabel("dB")));
    return l;
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MaterialAudioProcessor(); }
