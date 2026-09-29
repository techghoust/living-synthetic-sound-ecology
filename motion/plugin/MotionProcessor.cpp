#include "MotionProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include "shared/ui/LsseProductEditor.h"
#include "shared/state/StateMigration.h"

namespace
{
constexpr int stateVersion = 3;

class MotionTrajectoryEditor final : public juce::Component
{
public:
    explicit MotionTrajectoryEditor (MotionAudioProcessor& owner) : processor (owner)
    {
        path = processor.getUserPath();
        setTitle ("USER TRAJECTORY");
        setDescription ("Draw lateral position from past to future");
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff171e26));
        g.fillRoundedRectangle (area, 8.0f);
        g.setColour (juce::Colour (0xff8d83ff).withAlpha (0.35f));
        g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);
        auto plot = area.reduced (18.0f, 25.0f);
        g.setColour (juce::Colour (0xff596675));
        g.drawHorizontalLine (juce::roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());
        for (int column = 1; column < 4; ++column)
            g.drawVerticalLine (juce::roundToInt (plot.getX() + plot.getWidth() * column / 4.0f),
                                plot.getY(), plot.getBottom());
        juce::Path curve;
        for (std::size_t index = 0; index < path.size(); ++index)
        {
            const auto x = plot.getX() + plot.getWidth() * static_cast<float> (index)
                                           / static_cast<float> (path.size() - 1);
            const auto y = plot.getCentreY() - path[index] * plot.getHeight() * 0.5f;
            if (index == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        g.setColour (juce::Colour (0xff8d83ff));
        g.strokePath (curve, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffaab5c2));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("DRAW USER TRAJECTORY — LEFT / RIGHT", area.removeFromTop (21.0f),
                    juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& event) override { update (event.position); }
    void mouseDrag (const juce::MouseEvent& event) override { update (event.position); }

private:
    void update (juce::Point<float> point)
    {
        const auto plot = getLocalBounds().toFloat().reduced (18.0f, 25.0f);
        if (! plot.contains (point)) return;
        const auto index = juce::jlimit (0, static_cast<int> (path.size() - 1),
            juce::roundToInt ((point.x - plot.getX()) / plot.getWidth()
                              * static_cast<float> (path.size() - 1)));
        path[static_cast<std::size_t> (index)] = juce::jlimit (
            -1.0f, 1.0f, (plot.getCentreY() - point.y) / (plot.getHeight() * 0.5f));
        processor.setUserPath (path);
        processor.selectUserPath();
        repaint();
    }

    MotionAudioProcessor& processor;
    MotionAudioProcessor::UserPath path;
};
}

MotionAudioProcessor::MotionAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "MOTION_STATE", createParameterLayout())
{
    for (std::size_t index = 0; index < userPath.size(); ++index)
        userPath[index] = static_cast<float> (index) / static_cast<float> (userPath.size() - 1) * 2.0f - 1.0f;
}
void MotionAudioProcessor::prepareToPlay (double newSampleRate, int) { sampleRate = newSampleRate; timeline = 0; delayWrite = 0; filterState = {}; for(auto& line:delayLines)line.assign(static_cast<std::size_t>(sampleRate*0.25)+2,0.0f); }

bool MotionAudioProcessor::isBusesLayoutSupported (const BusesLayout& b) const
{
    const auto input = b.getMainInputChannelSet(), output = b.getMainOutputChannelSet();
    return input == output && (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo());
}

void MotionAudioProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals; const auto mix=parameters.getRawParameterValue("mix")->load()/100.0f; const auto depth=parameters.getRawParameterValue("depth")->load()/100.0f; const auto phaseOffset=parameters.getRawParameterValue("phase")->load()/100.0; const auto rateIndex=(int)parameters.getRawParameterValue("rate")->load(); const auto hz=std::pow(2.0,rateIndex-4)*0.25; const auto gain=std::pow(10.0f,parameters.getRawParameterValue("output")->load()/20.0f);const auto path=(int)parameters.getRawParameterValue("path")->load();const auto direction=(int)parameters.getRawParameterValue("direction")->load();const auto distance=parameters.getRawParameterValue("distance")->load()/100.0f;const auto doppler=parameters.getRawParameterValue("doppler")->load()/100.0f;const auto staticPan=parameters.getRawParameterValue("pan")->load()/100.0f;const auto width=parameters.getRawParameterValue("width")->load()/100.0f;const auto filter=parameters.getRawParameterValue("filter")->load()/100.0f;const auto delayMs=parameters.getRawParameterValue("delay")->load();const auto randomness=parameters.getRawParameterValue("randomness")->load()/100.0f;const auto seed=(std::uint32_t)parameters.getRawParameterValue("seed")->load();const auto drawn=getUserPath();
    for(int s=0;s<b.getNumSamples();++s,++timeline){auto p=phaseOffset+timeline*hz/sampleRate;if(direction==1)p=-p;else if(direction==2){const auto w=p-std::floor(p);p=w<0.5?w*2.0:2.0-w*2.0;}std::pair<float,float>xy;if(path==5){auto w=p-std::floor(p);const auto scaled=w*(drawn.size()-1);const auto first=(std::size_t)scaled;const auto next=juce::jmin(first+1,drawn.size()-1);const auto fraction=(float)(scaled-first);xy={drawn[first]+(drawn[next]-drawn[first])*fraction,0.0f};}else xy=lsse::motion::PathEngine::position(path,p,seed,randomness);const auto pan=lsse::motion::PathEngine::constantPowerPan(juce::jlimit(-1.0f,1.0f,staticPan+xy.first*depth));const auto movingDistance=juce::jlimit(0.0f,1.0f,distance+(xy.second+1.0f)*0.25f*depth);const auto delaySeconds=juce::jlimit(0.0,0.24,delayMs*0.001+movingDistance*0.018+doppler*xy.second*0.003);const auto delaySamples=static_cast<std::size_t>(delaySeconds*sampleRate);const auto read=(delayWrite+delayLines[0].size()-delaySamples)%delayLines[0].size();if(b.getNumChannels()==2){const auto rawL=b.getSample(0,s),rawR=b.getSample(1,s);const auto l=std::isfinite(rawL)?rawL:0.0f,r=std::isfinite(rawR)?rawR:0.0f;delayLines[0][delayWrite]=l;delayLines[1][delayWrite]=r;const auto mono=(delayLines[0][read]+delayLines[1][read])*0.70710678f;const auto cutoff=0.02f+(1.0f-filter*movingDistance)*0.45f;filterState[0]+=(mono-filterState[0])*cutoff;filterState[1]+=(mono-filterState[1])*cutoff;const auto attenuation=1.0f/(1.0f+movingDistance*2.5f);const auto wetL=filterState[0]*pan.first*attenuation*(2.0f-width+width*(1.0f+xy.second*0.1f));const auto wetR=filterState[1]*pan.second*attenuation*(2.0f-width+width*(1.0f-xy.second*0.1f));b.setSample(0,s,(l+(wetL-l)*mix)*gain);b.setSample(1,s,(r+(wetR-r)*mix)*gain);}else{const auto raw=b.getSample(0,s);const auto dry=std::isfinite(raw)?raw:0.0f;delayLines[0][delayWrite]=dry;const auto wet=delayLines[0][read]*(0.5f+0.5f*(1.0f-movingDistance));b.setSample(0,s,(dry+(wet-dry)*mix)*gain);}delayWrite=(delayWrite+1)%delayLines[0].size();}
}

juce::AudioProcessorEditor* MotionAudioProcessor::createEditor()
{
    using namespace lsse::ui;
    return new ProductEditor (*this, parameters, "MOTION", "Spatial trajectories, distance and kinetic illusion", juce::Colour (0xff8d83ff),
        { { "Trajectory", { "path", "sync", "rate", "phase", "direction" } },
          { "Geometry", { "depth", "distance", "doppler", "pan", "width" } },
          { "Routing", { "filter", "delay", "curve", "randomness" } },
          { "Transport", { "seed", "retrigger", "mix", "output" } } },
        { { "Slow Orbit", { {"mix",1},{"path",.2f},{"rate",.58f},{"depth",.82f},{"distance",.28f},{"doppler",.18f} } },
          { "Near Flyby", { {"mix",1},{"path",0},{"rate",.72f},{"depth",1},{"distance",.12f},{"doppler",.86f},{"delay",.3f} } },
          { "Unstable Field", { {"mix",1},{"path",.8f},{"randomness",.92f},{"depth",.7f},{"filter",.62f},{"width",.82f} } } },
        {}, std::make_unique<MotionTrajectoryEditor> (*this));
}
MotionAudioProcessor::UserPath MotionAudioProcessor::getUserPath() const { const juce::ScopedLock lock(userPathLock); return userPath; }
void MotionAudioProcessor::setUserPath(const UserPath& path){const juce::ScopedLock lock(userPathLock);userPath=path;}
void MotionAudioProcessor::selectUserPath(){if(auto* p=parameters.getParameter("path")){p->beginChangeGesture();p->setValueNotifyingHost(1.0f);p->endChangeGesture();}}
void MotionAudioProcessor::getStateInformation (juce::MemoryBlock& d) { auto s = parameters.copyState(); s.setProperty ("stateVersion", stateVersion, nullptr);const auto path=getUserPath();juce::ValueTree child("USER_PATH");for(std::size_t i=0;i<path.size();++i)child.setProperty("p"+juce::String(i),path[i],nullptr);s.addChild(child,-1,nullptr); if (auto x = s.createXml()) copyXmlToBinary (*x, d); }
void MotionAudioProcessor::setStateInformation (const void* d, int z) { if (auto x = getXmlFromBinary (d, z); x && x->hasTagName (parameters.state.getType())){auto s=juce::ValueTree::fromXml(*x);parameters.replaceState(lsse::state::mergeParameterState(parameters.copyState(),s));if(auto child=s.getChildWithName("USER_PATH");child.isValid()){auto path=getUserPath();for(std::size_t i=0;i<path.size();++i)path[i]=juce::jlimit(-1.0f,1.0f,(float)child.getProperty("p"+juce::String(i),path[i]));setUserPath(path);}} }

juce::AudioProcessorValueTreeState::ParameterLayout MotionAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    const auto pct = juce::AudioParameterFloatAttributes{}.withLabel ("%");
    auto p = [&] (const char* id, const char* name, float value) { l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> { 0, 100, 0.1f }, value, pct)); };
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "path", 1 }, "PATH", juce::StringArray { "LINE", "ORBIT", "PENDULUM", "FIGURE 8", "RANDOM WALK", "USER" }, 1));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "sync", 1 }, "SYNC", false));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "rate", 1 }, "RATE", juce::StringArray { "1/16", "1/8", "1/4", "1/2", "1 BAR", "2 BARS", "4 BARS", "FREE" }, 4));
    p ("phase", "PHASE", 0); l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "direction", 1 }, "DIRECTION", juce::StringArray { "FORWARD", "REVERSE", "PING PONG" }, 0));
    p ("depth", "DEPTH", 65); p ("distance", "DISTANCE", 35); p ("doppler", "DOPPLER", 25);
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "pan", 1 }, "PAN", juce::NormalisableRange<float> { -100, 100, 0.1f }, 0, pct));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "width", 1 }, "WIDTH", juce::NormalisableRange<float> { 0, 200, 0.1f }, 100, pct));
    p ("filter", "FILTER", 30); l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "delay", 1 }, "DELAY", juce::NormalisableRange<float> { 0, 100, 0.1f }, 20, juce::AudioParameterFloatAttributes{}.withLabel ("ms")));
    p ("curve", "CURVE", 50); p ("randomness", "RANDOMNESS", 0);
    l.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "seed", 1 }, "SEED", 0, 9999, 1));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "retrigger", 1 }, "RETRIGGER", true));
    p ("mix", "MIX", 0); l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "output", 1 }, "OUTPUT", juce::NormalisableRange<float> { -24, 6, 0.1f }, 0, juce::AudioParameterFloatAttributes{}.withLabel ("dB")));
    return l;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MotionAudioProcessor(); }
