#include "ConvolutionLabProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include "shared/ui/LsseProductEditor.h"
#include "shared/state/StateMigration.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr int stateVersion = 4;
constexpr double maximumImpulseSeconds = 30.0;

juce::AudioBuffer<float> makeBuiltInImpulse (std::initializer_list<float> values)
{
    juce::AudioBuffer<float> result (1, static_cast<int> (values.size()));
    int index = 0;
    for (const auto value : values)
        result.setSample (0, index++, value);
    return result;
}
}

ConvolutionLabAudioProcessor::ConvolutionLabAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "CONVOLUTION_LAB_STATE", createParameterLayout())
{
}

ConvolutionLabAudioProcessor::~ConvolutionLabAudioProcessor()
{
    cancelPendingUpdate();
    fileChooser.reset();
}

void ConvolutionLabAudioProcessor::prepareToPlay (const double newSampleRate,
                                                   const int newMaximumBlockSize)
{
    sampleRate = juce::jlimit (8000.0, 384000.0, newSampleRate);
    maximumBlockSize = juce::jmax (1, newMaximumBlockSize);
    predelayWrite = 0;
    modulationPhase = 0.0;
    lowpassState = {};
    highpassState = {};
    for (auto& line : predelayLines)
        line.assign (static_cast<std::size_t> (sampleRate * 0.55) + 2, 0.0f);

    convolutionScratchA.setSize (2, maximumBlockSize, false, true, false);
    convolutionScratchB.setSize (2, maximumBlockSize, false, true, false);
    dryScratch.setSize (2, maximumBlockSize, false, true, false);
    const juce::dsp::ProcessSpec spec { sampleRate,
                                        static_cast<juce::uint32> (maximumBlockSize),
                                        static_cast<juce::uint32> (juce::jmax (1, getTotalNumOutputChannels())) };
    convolutionA.prepare (spec);
    convolutionB.prepare (spec);
    loadBuiltInImpulse (0);
    loadBuiltInImpulse (1);
    prepared.store (true, std::memory_order_release);
    triggerAsyncUpdate();
}

void ConvolutionLabAudioProcessor::releaseResources()
{
    prepared.store (false, std::memory_order_release);
    convolutionA.reset();
    convolutionB.reset();
}

bool ConvolutionLabAudioProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    const auto input = layout.getMainInputChannelSet();
    const auto output = layout.getMainOutputChannelSet();
    return input == output && (output == juce::AudioChannelSet::mono()
                               || output == juce::AudioChannelSet::stereo());
}

void ConvolutionLabAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                  juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (predelayLines[0].empty())
        return;

    const auto mix = parameters.getRawParameterValue ("mix")->load() / 100.0f;
    const auto gain = std::pow (10.0f, parameters.getRawParameterValue ("output")->load() / 20.0f);
    const auto baseMorph = parameters.getRawParameterValue ("morph")->load() / 100.0f;
    const auto modulation = parameters.getRawParameterValue ("modulation")->load() / 100.0f;
    const auto predelay = parameters.getRawParameterValue ("predelay")->load() * 0.001f;
    const auto lowCut = parameters.getRawParameterValue ("low_cut")->load();
    const auto highCut = parameters.getRawParameterValue ("high_cut")->load();
    const auto width = parameters.getRawParameterValue ("width")->load() / 100.0f;
    const auto slot = static_cast<int> (parameters.getRawParameterValue ("slot")->load());
    const auto channels = juce::jmin (buffer.getNumChannels(), 2);
    const auto delaySamples = juce::jlimit<std::size_t> (
        0, predelayLines[0].size() - 1, static_cast<std::size_t> (predelay * sampleRate));

    for (int offset = 0; offset < buffer.getNumSamples(); offset += maximumBlockSize)
    {
        const auto count = juce::jmin (maximumBlockSize, buffer.getNumSamples() - offset);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto read = (predelayWrite + predelayLines[0].size() - delaySamples)
                              % predelayLines[0].size();
            for (int channel = 0; channel < channels; ++channel)
            {
                const auto raw = buffer.getSample (channel, offset + sample);
                const auto dry = std::isfinite (raw) ? raw : 0.0f;
                dryScratch.setSample (channel, sample, dry);
                predelayLines[static_cast<std::size_t> (channel)][predelayWrite] = dry;
                const auto delayed = predelayLines[static_cast<std::size_t> (channel)][read];
                convolutionScratchA.setSample (channel, sample, delayed);
                convolutionScratchB.setSample (channel, sample, delayed);
            }
            predelayWrite = (predelayWrite + 1) % predelayLines[0].size();
        }

        auto blockA = juce::dsp::AudioBlock<float> (convolutionScratchA)
                          .getSubBlock (0, static_cast<std::size_t> (count));
        auto blockB = juce::dsp::AudioBlock<float> (convolutionScratchB)
                          .getSubBlock (0, static_cast<std::size_t> (count));
        juce::dsp::ProcessContextReplacing<float> contextA (blockA);
        juce::dsp::ProcessContextReplacing<float> contextB (blockB);
        convolutionA.process (contextA);
        convolutionB.process (contextB);

        for (int sample = 0; sample < count; ++sample)
        {
            const auto wobble = static_cast<float> (std::sin (modulationPhase)) * modulation * 0.12f;
            modulationPhase += 2.0 * juce::MathConstants<double>::pi * 0.17 / sampleRate;
            if (modulationPhase >= 2.0 * juce::MathConstants<double>::pi)
                modulationPhase -= 2.0 * juce::MathConstants<double>::pi;
            const auto morph = juce::jlimit (0.0f, 1.0f,
                                              (slot == 0 ? baseMorph : 1.0f - baseMorph) + wobble);
            for (int channel = 0; channel < channels; ++channel)
            {
                const auto a = convolutionScratchA.getSample (channel, sample);
                const auto b = convolutionScratchB.getSample (channel, sample);
                auto wet = a + (b - a) * morph;
                const auto lpCoefficient = juce::jlimit (
                    0.0001f, 0.99f,
                    static_cast<float> (2.0 * juce::MathConstants<double>::pi * highCut / sampleRate));
                lowpassState[static_cast<std::size_t> (channel)]
                    += (wet - lowpassState[static_cast<std::size_t> (channel)]) * lpCoefficient;
                const auto hpCoefficient = juce::jlimit (
                    0.0001f, 0.99f,
                    static_cast<float> (2.0 * juce::MathConstants<double>::pi * lowCut / sampleRate));
                highpassState[static_cast<std::size_t> (channel)]
                    += (lowpassState[static_cast<std::size_t> (channel)]
                        - highpassState[static_cast<std::size_t> (channel)]) * hpCoefficient;
                wet = lowpassState[static_cast<std::size_t> (channel)]
                      - highpassState[static_cast<std::size_t> (channel)];
                const auto dry = dryScratch.getSample (channel, sample);
                const auto output = (dry + (wet - dry) * mix) * gain;
                buffer.setSample (channel, offset + sample,
                                  std::isfinite (output) ? output : 0.0f);
            }
        }
    }

    if (channels == 2 && width != 1.0f)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto left = buffer.getSample (0, sample);
            const auto right = buffer.getSample (1, sample);
            const auto mid = (left + right) * 0.5f;
            const auto side = (left - right) * 0.5f * width;
            buffer.setSample (0, sample, mid + side);
            buffer.setSample (1, sample, mid - side);
        }
}

juce::AudioProcessorEditor* ConvolutionLabAudioProcessor::createEditor()
{
    using namespace lsse::ui;
    return new ProductEditor (
        *this, parameters, "CONVOLUTION",
        "Impulse spaces, dual references and spectral morph", juce::Colour (0xff55bde8),
        { { "Impulse", { "ir_kind", "slot", "predelay", "start", "end", "reverse" } },
          { "Time", { "stretch", "envelope", "decay", "quality" } },
          { "Spectrum", { "low_cut", "high_cut", "width" } },
          { "Morph", { "morph", "modulation", "mix", "output" } } },
        { { "Compact Room", { {"mix",1}, {"slot",0}, {"morph",.12f}, {"predelay",.04f}, {"width",.5f} } },
          { "Metal Tunnel", { {"mix",1}, {"slot",1}, {"morph",.72f}, {"predelay",.2f}, {"modulation",.28f}, {"width",.76f} } },
          { "Spectral Ghost", { {"mix",1}, {"morph",.5f}, {"modulation",.85f}, {"low_cut",.35f}, {"high_cut",.7f}, {"width",1} } } },
        { { "LOAD / RELINK A", [this] { chooseImpulseForSlot (0); } },
          { "LOAD / RELINK B", [this] { chooseImpulseForSlot (1); } } });
}

double ConvolutionLabAudioProcessor::getTailLengthSeconds() const
{
    const auto samples = juce::jmax (convolutionA.getCurrentIRSize(), convolutionB.getCurrentIRSize());
    return sampleRate > 0.0 ? juce::jmin (30.5, samples / sampleRate + 0.5) : 0.0;
}

void ConvolutionLabAudioProcessor::chooseImpulseForSlot (const int slotIndex)
{
    if (! juce::isPositiveAndBelow (slotIndex, 2) || fileChooser != nullptr)
        return;
    const auto previous = getImpulseAsset (slotIndex);
    const auto initial = juce::File (previous).existsAsFile()
                             ? juce::File (previous).getParentDirectory()
                             : juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load impulse response for slot " + juce::String (slotIndex == 0 ? "A" : "B"),
        initial, "*.wav;*.aif;*.aiff;*.flac", true);
    constexpr auto flags = juce::FileBrowserComponent::openMode
                           | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync (flags, [this, slotIndex] (const juce::FileChooser& chooser)
    {
        const auto selected = chooser.getResult();
        if (selected.existsAsFile())
            loadImpulseResponseForSlot (slotIndex, selected);
        fileChooser.reset();
    });
}

juce::AudioBuffer<float> ConvolutionLabAudioProcessor::prepareImpulse (
    juce::AudioFormatReader& reader) const
{
    if (reader.numChannels < 1 || reader.numChannels > 2
        || reader.sampleRate < 8000.0 || reader.sampleRate > 192000.0)
        return {};
    const auto maximumSourceSamples = static_cast<juce::int64> (
        std::llround (reader.sampleRate * maximumImpulseSeconds));
    const auto sourceSamples = static_cast<int> (
        juce::jlimit<juce::int64> (1, maximumSourceSamples, reader.lengthInSamples));
    juce::AudioBuffer<float> source (static_cast<int> (reader.numChannels), sourceSamples);
    if (! reader.read (&source, 0, sourceSamples, 0, true, true))
        return {};
    for (int channel = 0; channel < source.getNumChannels(); ++channel)
        for (int sample = 0; sample < source.getNumSamples(); ++sample)
            if (! std::isfinite (source.getSample (channel, sample)))
                source.setSample (channel, sample, 0.0f);

    const auto start = parameters.getRawParameterValue ("start")->load() * 0.01f;
    const auto end = parameters.getRawParameterValue ("end")->load() * 0.01f;
    const auto first = juce::jlimit (0, sourceSamples - 1,
                                     static_cast<int> (std::floor (start * sourceSamples)));
    const auto last = juce::jlimit (first + 1, sourceSamples,
                                    static_cast<int> (std::ceil (juce::jmax (start, end) * sourceSamples)));
    const auto stretch = parameters.getRawParameterValue ("stretch")->load() * 0.01f;
    const auto maximumProcessedSamples = static_cast<int> (
        std::llround (reader.sampleRate * maximumImpulseSeconds));
    const auto outputSamples = juce::jlimit (
        1, maximumProcessedSamples, static_cast<int> (std::llround ((last - first) * stretch)));
    juce::AudioBuffer<float> result (source.getNumChannels(), outputSamples);
    const auto reverse = parameters.getRawParameterValue ("reverse")->load() > 0.5f;
    const auto decay = parameters.getRawParameterValue ("decay")->load();
    const auto envelope = static_cast<int> (parameters.getRawParameterValue ("envelope")->load());
    const auto decayShape = (100.0f - decay) * 0.055f;
    for (int channel = 0; channel < result.getNumChannels(); ++channel)
        for (int sample = 0; sample < outputSamples; ++sample)
        {
            const auto normalised = outputSamples > 1
                                        ? static_cast<double> (sample) / (outputSamples - 1)
                                        : 0.0;
            const auto sourcePosition = normalised * (last - first - 1);
            const auto base = static_cast<int> (sourcePosition);
            const auto next = juce::jmin (base + 1, last - first - 1);
            const auto fraction = static_cast<float> (sourcePosition - base);
            const auto firstIndex = reverse ? last - 1 - base : first + base;
            const auto nextIndex = reverse ? last - 1 - next : first + next;
            auto value = source.getSample (channel, firstIndex)
                         + fraction * (source.getSample (channel, nextIndex)
                                       - source.getSample (channel, firstIndex));
            const auto t = static_cast<float> (normalised);
            if (envelope == 0)
                value *= juce::jmax (0.0f, 1.0f - t * juce::jmax (0.0f, decayShape * 0.25f));
            else if (envelope == 1)
                value *= std::exp (-t * decayShape);
            else if (t > juce::jlimit (0.05f, 1.0f, decay * 0.01f))
                value = 0.0f;
            result.setSample (channel, sample, value);
        }
    return result;
}

bool ConvolutionLabAudioProcessor::loadImpulseResponseForSlot (const int slotIndex,
                                                                const juce::File& file)
{
    if (! juce::isPositiveAndBelow (slotIndex, 2) || ! file.existsAsFile())
        return false;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto reader = formats.createReaderFor (file);
    if (reader == nullptr)
        return false;
    auto impulse = prepareImpulse (*reader);
    if (impulse.getNumSamples() == 0)
        return false;
    const auto stereo = impulse.getNumChannels() == 2
                            ? juce::dsp::Convolution::Stereo::yes
                            : juce::dsp::Convolution::Stereo::no;
    convolutionForSlot (slotIndex).loadImpulseResponse (
        std::move (impulse), reader->sampleRate, stereo,
        juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
    {
        const juce::ScopedLock lock (assetLock);
        assetPaths[static_cast<std::size_t> (slotIndex)] = file.getFullPathName();
        assetStatus[static_cast<std::size_t> (slotIndex)] = "Loaded: " + file.getFileName();
    }
    return true;
}

void ConvolutionLabAudioProcessor::loadBuiltInImpulse (const int slotIndex)
{
    auto impulse = slotIndex == 0
                       ? makeBuiltInImpulse ({ 0.68f, 0.24f, 0.13f, 0.07f, 0.035f })
                       : makeBuiltInImpulse ({ 0.52f, -0.31f, 0.23f, -0.16f,
                                               0.11f, -0.075f, 0.048f, -0.028f });
    convolutionForSlot (slotIndex).loadImpulseResponse (
        std::move (impulse), sampleRate, juce::dsp::Convolution::Stereo::no,
        juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
}

juce::dsp::Convolution& ConvolutionLabAudioProcessor::convolutionForSlot (const int slotIndex)
{
    return slotIndex == 0 ? convolutionA : convolutionB;
}

juce::String ConvolutionLabAudioProcessor::getImpulseAsset (const int slotIndex) const
{
    if (! juce::isPositiveAndBelow (slotIndex, 2))
        return {};
    const juce::ScopedLock lock (assetLock);
    return assetPaths[static_cast<std::size_t> (slotIndex)];
}

void ConvolutionLabAudioProcessor::handleAsyncUpdate()
{
    if (! prepared.load (std::memory_order_acquire))
        return;
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex)
    {
        const auto path = getImpulseAsset (slotIndex);
        if (path.startsWith ("built-in:"))
            continue;
        const juce::File file (path);
        if (! loadImpulseResponseForSlot (slotIndex, file))
        {
            loadBuiltInImpulse (slotIndex);
            const juce::ScopedLock lock (assetLock);
            assetStatus[static_cast<std::size_t> (slotIndex)] = "Missing — relink: " + file.getFileName();
        }
    }
}

void ConvolutionLabAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    state.setProperty ("stateVersion", stateVersion, nullptr);
    state.setProperty ("slotAAsset", getImpulseAsset (0), nullptr);
    state.setProperty ("slotBAsset", getImpulseAsset (1), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destination);
}

void ConvolutionLabAudioProcessor::setStateInformation (const void* data, const int size)
{
    if (auto xml = getXmlFromBinary (data, size);
        xml != nullptr && xml->hasTagName (parameters.state.getType()))
    {
        auto restored = juce::ValueTree::fromXml (*xml);
        {
            const juce::ScopedLock lock (assetLock);
            assetPaths[0] = restored.getProperty ("slotAAsset", "built-in:reference-a").toString();
            assetPaths[1] = restored.getProperty ("slotBAsset", "built-in:reference-b").toString();
        }
        parameters.replaceState (lsse::state::mergeParameterState (parameters.copyState(), restored));
        triggerAsyncUpdate();
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
ConvolutionLabAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto percent = juce::AudioParameterFloatAttributes {}.withLabel ("%");
    auto addPercent = [&] (const char* id, const char* name, const float value)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> { 0, 100, 0.1f }, value, percent));
    };
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "ir_kind", 1 }, "IR MATERIAL",
        juce::StringArray { "REAL IR", "CREATIVE IR" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "slot", 1 }, "SLOT", juce::StringArray { "A", "B" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "predelay", 1 }, "PREDELAY",
        juce::NormalisableRange<float> { 0, 500, 0.1f }, 0,
        juce::AudioParameterFloatAttributes {}.withLabel ("ms")));
    addPercent ("start", "START", 0);
    addPercent ("end", "END", 100);
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "reverse", 1 }, "REVERSE", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "stretch", 1 }, "STRETCH",
        juce::NormalisableRange<float> { 25, 400, 0.1f, 0.5f }, 100, percent));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "envelope", 1 }, "ENVELOPE",
        juce::StringArray { "LINEAR", "EXPONENTIAL", "GATED" }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "low_cut", 1 }, "LOW CUT",
        juce::NormalisableRange<float> { 20, 20000, 1, 0.3f }, 20,
        juce::AudioParameterFloatAttributes {}.withLabel ("Hz")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "high_cut", 1 }, "HIGH CUT",
        juce::NormalisableRange<float> { 20, 20000, 1, 0.3f }, 20000,
        juce::AudioParameterFloatAttributes {}.withLabel ("Hz")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "decay", 1 }, "DECAY",
        juce::NormalisableRange<float> { 0, 200, 0.1f }, 100, percent));
    addPercent ("morph", "MORPH", 0);
    addPercent ("modulation", "MODULATION", 0);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "width", 1 }, "WIDTH",
        juce::NormalisableRange<float> { 0, 200, 0.1f }, 100, percent));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "quality", 1 }, "QUALITY",
        juce::StringArray { "LOW LATENCY", "BALANCED", "EFFICIENT" }, 1));
    addPercent ("mix", "MIX", 0);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, "OUTPUT",
        juce::NormalisableRange<float> { -24, 6, 0.1f }, 0,
        juce::AudioParameterFloatAttributes {}.withLabel ("dB")));
    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ConvolutionLabAudioProcessor();
}
