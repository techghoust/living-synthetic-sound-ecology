#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>
#include <cmath>

namespace parameterIds
{
constexpr auto memoryLength = "memoryLength";
constexpr auto recall = "recall";
constexpr auto age = "age";
constexpr auto fragment = "fragment";
constexpr auto decay = "decay";
constexpr auto corruption = "corruption";
constexpr auto drift = "drift";
constexpr auto repeat = "repeat";
constexpr auto feedback = "feedback";
constexpr auto pastPresent = "pastPresent";
constexpr auto output = "output";
} 
namespace
{
struct FactoryPreset
{
    const char* name;
    std::array<float, 11> values;
};

constexpr std::array<const char*, 11> presetParameterIds {
    parameterIds::memoryLength, parameterIds::recall, parameterIds::age,
    parameterIds::fragment, parameterIds::decay, parameterIds::corruption,
    parameterIds::drift, parameterIds::repeat, parameterIds::feedback,
    parameterIds::pastPresent, parameterIds::output
};

constexpr std::array<FactoryPreset, 6> factoryPresets {{
    { "DEFAULT",       { 30.0f, 35.0f, 40.0f, 450.0f, 20.0f,  8.0f, 12.0f, 15.0f,  0.0f, 60.0f,  0.0f } },
    { "SHORT MEMORY",  { 12.0f, 30.0f, 25.0f, 280.0f, 10.0f,  5.0f,  6.0f, 10.0f,  5.0f, 45.0f,  0.0f } },
    { "OLD MEMORY",    { 90.0f, 35.0f, 85.0f, 900.0f, 65.0f, 18.0f, 28.0f, 18.0f,  8.0f, 35.0f, -1.0f } },
    { "BROKEN RECALL", { 25.0f, 80.0f, 45.0f, 110.0f, 35.0f, 85.0f, 70.0f, 75.0f, 30.0f,  0.0f, -3.0f } },
    { "GHOST SIGNAL",  { 60.0f, 18.0f, 65.0f,1200.0f, 55.0f, 12.0f, 22.0f,  8.0f,  4.0f, 82.0f, -2.0f } },
    { "LONG MEMORY",   {120.0f, 12.0f, 90.0f,1800.0f, 60.0f, 10.0f, 18.0f,  5.0f,  2.0f, 70.0f,  0.0f } }
}};

constexpr int currentStateVersion = 4;
} 
MemoryAudioProcessor::MemoryAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "MEMORY_PARAMETERS", createParameterLayout())
{
    memoryLengthParameter = parameters.getRawParameterValue (parameterIds::memoryLength);
    recallParameter = parameters.getRawParameterValue (parameterIds::recall);
    ageParameter = parameters.getRawParameterValue (parameterIds::age);
    fragmentParameter = parameters.getRawParameterValue (parameterIds::fragment);
    decayParameter = parameters.getRawParameterValue (parameterIds::decay);
    corruptionParameter = parameters.getRawParameterValue (parameterIds::corruption);
    driftParameter = parameters.getRawParameterValue (parameterIds::drift);
    repeatParameter = parameters.getRawParameterValue (parameterIds::repeat);
    feedbackParameter = parameters.getRawParameterValue (parameterIds::feedback);
    pastPresentParameter = parameters.getRawParameterValue (parameterIds::pastPresent);
    outputParameter = parameters.getRawParameterValue (parameterIds::output);

    jassert (memoryLengthParameter != nullptr && recallParameter != nullptr
             && ageParameter != nullptr && fragmentParameter != nullptr
             && decayParameter != nullptr && corruptionParameter != nullptr
             && driftParameter != nullptr
             && repeatParameter != nullptr && feedbackParameter != nullptr
             && pastPresentParameter != nullptr && outputParameter != nullptr);
}

void MemoryAudioProcessor::prepareToPlay (const double sampleRate,
                                          const int maximumExpectedSamplesPerBlock)
{
    const auto capacity = memory::AudioHistory::capacityForDuration (
        sampleRate, maximumHistorySeconds);
    audioHistory.prepare (static_cast<std::size_t> (getTotalNumInputChannels()), capacity);
    recallEngine.prepare (sampleRate);
    recallScratch.setSize (getTotalNumOutputChannels(),
                           std::max (1, maximumExpectedSamplesPerBlock),
                           false, true, false);

    visualizationFramesPerBin = std::max<std::size_t> (
        1, static_cast<std::size_t> (std::round (sampleRate / 30.0)));
    visualizationFramesAccumulated = 0;
    visualizationPeak = 0.0f;
    visualizationWriteIndex.store (0, std::memory_order_relaxed);
    visualizationActiveEvents.store (0, std::memory_order_relaxed);
    visualizationRecalledAge.store (0.0f, std::memory_order_relaxed);
    for (auto& bin : visualizationActivity)
        bin.store (0.0f, std::memory_order_relaxed);

    presentGain.reset (sampleRate, 0.02);
    pastGain.reset (sampleRate, 0.02);
    feedbackGain.reset (sampleRate, 0.05);
    outputGain.reset (sampleRate, 0.02);
    updateMixTargets();
    presentGain.setCurrentAndTargetValue (presentGain.getTargetValue());
    pastGain.setCurrentAndTargetValue (pastGain.getTargetValue());
    feedbackGain.setCurrentAndTargetValue (
        std::clamp (feedbackParameter->load() * 0.01f, 0.0f, 0.95f) * 0.85f);
    outputGain.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (outputParameter->load()));
}

void MemoryAudioProcessor::releaseResources()
{
    recallEngine.reset();
}

bool MemoryAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output && (output == juce::AudioChannelSet::mono()
                               || output == juce::AudioChannelSet::stereo());
}

void MemoryAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    audioHistory.write (buffer.getArrayOfReadPointers(),
                        static_cast<std::size_t> (getTotalNumInputChannels()),
                        static_cast<std::size_t> (buffer.getNumSamples()));
    publishInputActivity (buffer);

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    updateRecallSettings();
    updateMixTargets();
    feedbackGain.setTargetValue (
        std::clamp (feedbackParameter->load() * 0.01f, 0.0f, 0.95f) * 0.85f);
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputParameter->load()));

    const auto frameCount = buffer.getNumSamples();
    const auto blockHistoryStart = audioHistory.getTotalFramesWritten()
                                   - static_cast<memory::AudioHistory::FramePosition> (frameCount);
    const auto scratchCapacity = recallScratch.getNumSamples();

    for (int offset = 0; offset < frameCount; offset += scratchCapacity)
    {
        const auto chunk = std::min (scratchCapacity, frameCount - offset);
        recallScratch.clear (0, chunk);
        recallEngine.processBlock (audioHistory,
                                   recallScratch.getArrayOfWritePointers(),
                                   static_cast<std::size_t> (getTotalNumOutputChannels()),
                                   static_cast<std::size_t> (chunk));

        const auto feedbackStart = feedbackGain.getCurrentValue();
        const auto feedbackEnd = feedbackGain.skip (chunk);
        (void) audioHistory.mixIntoFrames (
            blockHistoryStart + static_cast<memory::AudioHistory::FramePosition> (offset),
            recallScratch.getArrayOfReadPointers(),
            static_cast<std::size_t> (getTotalNumOutputChannels()),
            static_cast<std::size_t> (chunk), feedbackStart, feedbackEnd);

        const auto presentStart = presentGain.getCurrentValue();
        const auto presentEnd = presentGain.skip (chunk);
        const auto pastStart = pastGain.getCurrentValue();
        const auto pastEnd = pastGain.skip (chunk);
        const auto outputStart = outputGain.getCurrentValue();
        const auto outputEnd = outputGain.skip (chunk);

        for (int channel = 0; channel < getTotalNumOutputChannels(); ++channel)
        {
            buffer.applyGainRamp (channel, offset, chunk, presentStart, presentEnd);
            buffer.addFromWithRamp (channel, offset,
                                    recallScratch.getReadPointer (channel), chunk,
                                    pastStart, pastEnd);
            buffer.applyGainRamp (channel, offset, chunk, outputStart, outputEnd);
        }
    }

    const auto activeEvents = recallEngine.getActiveEventCount();
    visualizationActiveEvents.store (activeEvents, std::memory_order_relaxed);
    if (activeEvents > 0)
        visualizationRecalledAge.store (
            std::clamp (recallEngine.getLastStartedEvent().normalizedAge, 0.0f, 1.0f),
            std::memory_order_relaxed);
}

void MemoryAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* MemoryAudioProcessor::createEditor()
{
    return new MemoryAudioProcessorEditor (*this);
}

MemoryAudioProcessor::VisualizationSnapshot
MemoryAudioProcessor::getVisualizationSnapshot() const noexcept
{
    VisualizationSnapshot snapshot;
    for (std::size_t index = 0; index < snapshot.activity.size(); ++index)
        snapshot.activity[index] = visualizationActivity[index].load (std::memory_order_relaxed);

    snapshot.writeIndex = visualizationWriteIndex.load (std::memory_order_relaxed);
    snapshot.activeEvents = visualizationActiveEvents.load (std::memory_order_relaxed);
    snapshot.recalledAge = visualizationRecalledAge.load (std::memory_order_relaxed);
    return snapshot;
}

void MemoryAudioProcessor::publishInputActivity (const juce::AudioBuffer<float>& buffer) noexcept
{
    auto blockPeak = 0.0f;
    const auto channels = std::min (buffer.getNumChannels(), getTotalNumInputChannels());
    for (int channel = 0; channel < channels; ++channel)
        blockPeak = std::max (blockPeak,
                              buffer.getMagnitude (channel, 0, buffer.getNumSamples()));

    if (! std::isfinite (blockPeak))
        blockPeak = 0.0f;

    visualizationPeak = std::max (visualizationPeak, std::min (blockPeak, 1.0f));
    visualizationFramesAccumulated += static_cast<std::size_t> (buffer.getNumSamples());

    while (visualizationFramesAccumulated >= visualizationFramesPerBin)
    {
        const auto index = visualizationWriteIndex.load (std::memory_order_relaxed);
        visualizationActivity[index].store (visualizationPeak, std::memory_order_relaxed);
        visualizationWriteIndex.store ((index + 1) % visualizationBinCount,
                                       std::memory_order_relaxed);
        visualizationFramesAccumulated -= visualizationFramesPerBin;
        visualizationPeak = 0.0f;
    }
}

void MemoryAudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    if (auto state = parameters.copyState(); state.isValid())
    {
        state.setProperty ("stateVersion", currentStateVersion, nullptr);
        state.setProperty ("currentPreset", getCurrentFactoryPreset(), nullptr);
        if (const auto xml = state.createXml())
            copyXmlToBinary (*xml, destinationData);
    }
}

void MemoryAudioProcessor::setStateInformation (const void* data, const int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (parameters.state.getType()))
        {
            const auto restored = juce::ValueTree::fromXml (*xml);
            auto migrated = parameters.copyState();

            for (int childIndex = 0; childIndex < restored.getNumChildren(); ++childIndex)
            {
                const auto source = restored.getChild (childIndex);
                const auto id = source.getProperty ("id");
                auto destination = migrated.getChildWithProperty ("id", id);
                if (destination.isValid() && source.hasProperty ("value"))
                    destination.setProperty ("value", source.getProperty ("value"), nullptr);
            }

            migrated.setProperty ("stateVersion", currentStateVersion, nullptr);
            parameters.replaceState (migrated);
            const auto restoredPreset = restored.hasProperty ("currentPreset")
                                            ? restored.getProperty ("currentPreset")
                                            : restored.getProperty ("currentProgram", 0);
            currentFactoryPreset.store (std::clamp (
                static_cast<int> (restoredPreset), 0, getNumFactoryPresets() - 1),
                std::memory_order_relaxed);
        }
    }
}

int MemoryAudioProcessor::getNumPrograms()
{
            return 1;
}

int MemoryAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MemoryAudioProcessor::setCurrentProgram (const int)
{
}

const juce::String MemoryAudioProcessor::getProgramName (const int index)
{
    return index == 0 ? "Default" : juce::String {};
}

int MemoryAudioProcessor::getNumFactoryPresets() const noexcept
{
    return static_cast<int> (factoryPresets.size());
}

int MemoryAudioProcessor::getCurrentFactoryPreset() const noexcept
{
    return currentFactoryPreset.load (std::memory_order_relaxed);
}

void MemoryAudioProcessor::applyFactoryPreset (const int index)
{
    if (index < 0 || index >= getNumFactoryPresets())
        return;

    const auto& preset = factoryPresets[static_cast<std::size_t> (index)];
    for (std::size_t parameterIndex = 0; parameterIndex < presetParameterIds.size(); ++parameterIndex)
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (
                parameters.getParameter (presetParameterIds[parameterIndex])))
        {
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 (preset.values[parameterIndex]));
        }
    }

    currentFactoryPreset.store (index, std::memory_order_relaxed);
}

const juce::String MemoryAudioProcessor::getFactoryPresetName (const int index) const
{
    if (index < 0 || index >= getNumFactoryPresets())
        return {};

    return factoryPresets[static_cast<std::size_t> (index)].name;
}

juce::AudioProcessorValueTreeState::ParameterLayout MemoryAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { parameterIds::memoryLength, 1 }, "MEMORY LENGTH",
        juce::NormalisableRange<float> { 5.0f, 120.0f, 0.1f, 0.4f }, 30.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("s")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { parameterIds::recall, 1 }, "RECALL",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 35.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("%")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { parameterIds::age, 1 }, "AGE",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 40.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("%")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { parameterIds::fragment, 1 }, "FRAGMENT",
        juce::NormalisableRange<float> { 20.0f, 4000.0f, 1.0f, 0.35f }, 450.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("ms")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { parameterIds::decay, 1 }, "DECAY",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 20.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("%")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { parameterIds::corruption, 1 }, "CORRUPTION",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 8.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("%")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { parameterIds::drift, 1 }, "DRIFT",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 12.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("%")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { parameterIds::repeat, 1 }, "REPEAT",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 15.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("%")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { parameterIds::feedback, 1 }, "FEEDBACK",
        juce::NormalisableRange<float> { 0.0f, 95.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("%")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { parameterIds::pastPresent, 1 }, "PAST / PRESENT",
        juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f }, 60.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { parameterIds::output, 1 }, "OUTPUT",
        juce::NormalisableRange<float> { -24.0f, 6.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("dB")));

    return layout;
}

void MemoryAudioProcessor::updateRecallSettings() noexcept
{
    const auto recallAmount = std::clamp (recallParameter->load() * 0.01f, 0.0f, 1.0f);
    const auto fragmentSeconds = static_cast<double> (fragmentParameter->load()) * 0.001;
    const auto memoryLengthSeconds = static_cast<double> (memoryLengthParameter->load());

    memory::RecallEngine::Settings settings;
    settings.enabled = recallAmount > 0.0001f;
    settings.intervalSeconds = 12.0 * std::pow (0.04, static_cast<double> (recallAmount));
    settings.fragmentSeconds = fragmentSeconds;
    settings.minimumAgeSeconds = 0.05;
    settings.maximumAgeSeconds = std::max (0.05, memoryLengthSeconds - fragmentSeconds);
    settings.memoryLengthSeconds = memoryLengthSeconds;
    settings.ageBias = std::clamp (ageParameter->load() * 0.01f, 0.0f, 1.0f);
    settings.decayAmount = std::clamp (decayParameter->load() * 0.01f, 0.0f, 1.0f);
    settings.corruptionAmount = std::clamp (corruptionParameter->load() * 0.01f, 0.0f, 1.0f);
    settings.driftAmount = std::clamp (driftParameter->load() * 0.01f, 0.0f, 1.0f);
    settings.repeatAmount = std::clamp (repeatParameter->load() * 0.01f, 0.0f, 1.0f);
    settings.fadeSeconds = std::min (0.01, fragmentSeconds * 0.25);
    settings.gain = 0.2f + 0.8f * recallAmount;
    recallEngine.updateSettings (settings);
}

void MemoryAudioProcessor::updateMixTargets() noexcept
{
    const auto position = std::clamp ((pastPresentParameter->load() + 100.0f) / 200.0f,
                                      0.0f, 1.0f);
    constexpr auto halfPi = juce::MathConstants<float>::halfPi;
    presentGain.setTargetValue (std::sin (position * halfPi));
    pastGain.setTargetValue (std::cos (position * halfPi));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MemoryAudioProcessor();
}
