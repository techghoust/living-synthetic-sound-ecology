#include "MachineProcessor.h"
#include "MachineEditor.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace machineParameterIds
{
constexpr auto mode = "mode";
constexpr auto sync = "sync";
constexpr auto rate = "rate";
constexpr auto steps = "steps";
constexpr auto swing = "swing";
constexpr auto probability = "probability";
constexpr auto ratchet = "ratchet";
constexpr auto irregularity = "irregularity";
constexpr auto seed = "seed";
constexpr auto force = "force";
constexpr auto motor = "motor";
constexpr auto relay = "relay";
constexpr auto friction = "friction";
constexpr auto body = "body";
constexpr auto feedback = "feedback";
constexpr auto drive = "drive";
constexpr auto tone = "tone";
constexpr auto stereo = "stereo";
constexpr auto mix = "mix";
constexpr auto output = "output";
constexpr auto sampleLayer = "sample_layer";
constexpr auto sampleBlend = "sample_blend";
constexpr auto samplePitch = "sample_pitch";
constexpr auto sampleLoop = "sample_loop";
} 
namespace
{
constexpr int stateVersion = 2;

struct FactoryPreset
{
    const char* name;
    std::array<float, 20> values;
};

constexpr std::array<const char*, 20> presetParameterIds {
    machineParameterIds::mode, machineParameterIds::sync, machineParameterIds::rate,
    machineParameterIds::steps, machineParameterIds::swing, machineParameterIds::probability,
    machineParameterIds::ratchet, machineParameterIds::irregularity, machineParameterIds::seed,
    machineParameterIds::force, machineParameterIds::motor, machineParameterIds::relay,
    machineParameterIds::friction, machineParameterIds::body, machineParameterIds::feedback,
    machineParameterIds::drive, machineParameterIds::tone, machineParameterIds::stereo,
    machineParameterIds::mix, machineParameterIds::output
};

constexpr std::array<FactoryPreset, 8> factoryPresets {{
    { "ASSEMBLY LINE", { 0, 1, 2, 16, 8, 100, 1, 5, 1977, 70, 60, 65, 35, 55, 15, 20, 5, 100, 75, -2 } },
    { "SERVO SWARM", { 1, 1, 3, 12, 15, 90, 2, 35, 4181, 55, 95, 10, 45, 45, 20, 35, 30, 150, 80, -3 } },
    { "RELAY GRID", { 2, 1, 1, 16, 0, 100, 2, 8, 9031, 85, 0, 100, 20, 60, 25, 45, 45, 120, 85, -4 } },
    { "HEAVY PRESS", { 0, 1, 6, 8, 12, 100, 1, 10, 3301, 100, 75, 70, 60, 95, 45, 70, -35, 80, 80, -6 } },
    { "BROKEN CLOCK", { 0, 1, 4, 7, 42, 72, 3, 85, 6661, 65, 50, 90, 55, 75, 58, 45, -10, 170, 90, -5 } },
    { "MICRO GEARS", { 0, 1, 0, 16, 20, 88, 4, 30, 1171, 45, 80, 65, 35, 40, 12, 30, 70, 180, 70, -3 } },
    { "INDUSTRIAL PULSE", { 1, 1, 4, 8, 4, 100, 1, 18, 7703, 90, 100, 15, 70, 85, 50, 80, -5, 130, 88, -7 } },
    { "EMPTY FACTORY", { 2, 1, 5, 11, 30, 60, 1, 55, 2029, 40, 15, 75, 80, 90, 72, 25, -60, 190, 65, -8 } }
}};

machine::PatternSnapshot makePresetPattern (const int presetIndex) noexcept
{
    machine::PatternSnapshot pattern;
    const auto index = static_cast<std::uint32_t> (std::max (0, presetIndex));
    pattern.revision = index + 2U;
    pattern.activeSteps = static_cast<std::uint8_t> (
        std::clamp (juce::roundToInt (factoryPresets[static_cast<std::size_t> (index)].values[3]),
                    1, 16));
    pattern.seed = static_cast<std::uint32_t> (
        factoryPresets[static_cast<std::size_t> (index)].values[8]);
    pattern.swing = factoryPresets[static_cast<std::size_t> (index)].values[4] * 0.01f;
    for (std::size_t stepIndex = 0; stepIndex < pattern.steps.size(); ++stepIndex)
    {
        auto& step = pattern.steps[stepIndex];
        const auto key = machine::TransportClock::makeRandomKey (
            pattern.seed, pattern.revision, 0, static_cast<std::uint8_t> (stepIndex), 0, 17);
        const auto random = static_cast<float> (machine::TransportClock::randomUnit (key));
        step.value = std::clamp (0.3f + 0.65f * random, 0.0f, 1.0f);
        step.accent = stepIndex % (presetIndex == 5 ? 3U : 4U) == 0U ? 1.0f : 0.18f;
        step.probability = presetIndex == 4 || presetIndex == 7
                               ? 0.48f + 0.5f * random : 1.0f;
        step.ratchet = static_cast<std::uint8_t> (
            presetIndex == 2 ? (stepIndex % 4U == 3U ? 3 : 1)
                             : presetIndex == 5 ? 2 : 1);
    }
    return pattern;
}

float clampedFloatProperty (const juce::ValueTree& tree, const juce::Identifier& name,
                            const float fallback, const float minimum,
                            const float maximum) noexcept
{
    const auto value = static_cast<float> (tree.getProperty (name, fallback));
    return std::isfinite (value) ? std::clamp (value, minimum, maximum) : fallback;
}
} 
MachineAudioProcessor::MachineAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "MACHINE_STATE", createParameterLayout())
{
    uiPattern = activePattern;
    syncParameter = parameters.getRawParameterValue (machineParameterIds::sync);
    rateParameter = parameters.getRawParameterValue (machineParameterIds::rate);
    stepsParameter = parameters.getRawParameterValue (machineParameterIds::steps);
    swingParameter = parameters.getRawParameterValue (machineParameterIds::swing);
    probabilityParameter = parameters.getRawParameterValue (machineParameterIds::probability);
    ratchetParameter = parameters.getRawParameterValue (machineParameterIds::ratchet);
    seedParameter = parameters.getRawParameterValue (machineParameterIds::seed);
    modeParameter = parameters.getRawParameterValue (machineParameterIds::mode);
    forceParameter = parameters.getRawParameterValue (machineParameterIds::force);
    motorParameter = parameters.getRawParameterValue (machineParameterIds::motor);
    relayParameter = parameters.getRawParameterValue (machineParameterIds::relay);
    frictionParameter = parameters.getRawParameterValue (machineParameterIds::friction);
    bodyParameter = parameters.getRawParameterValue (machineParameterIds::body);
    feedbackParameter = parameters.getRawParameterValue (machineParameterIds::feedback);
    driveParameter = parameters.getRawParameterValue (machineParameterIds::drive);
    toneParameter = parameters.getRawParameterValue (machineParameterIds::tone);
    irregularityParameter = parameters.getRawParameterValue (machineParameterIds::irregularity);
    stereoParameter = parameters.getRawParameterValue (machineParameterIds::stereo);
    mixParameter = parameters.getRawParameterValue (machineParameterIds::mix);
    outputParameter = parameters.getRawParameterValue (machineParameterIds::output);
    jassert (syncParameter != nullptr && rateParameter != nullptr && stepsParameter != nullptr
             && swingParameter != nullptr && probabilityParameter != nullptr
             && ratchetParameter != nullptr && seedParameter != nullptr
             && modeParameter != nullptr && forceParameter != nullptr
             && motorParameter != nullptr && relayParameter != nullptr
             && frictionParameter != nullptr && bodyParameter != nullptr
             && feedbackParameter != nullptr && driveParameter != nullptr
             && toneParameter != nullptr && irregularityParameter != nullptr
             && stereoParameter != nullptr && mixParameter != nullptr
             && outputParameter != nullptr);
}

void MachineAudioProcessor::prepareToPlay (const double sampleRate, int)
{
    processingSampleRate = sampleRate;
    layerSamplePosition = 0.0;
    layerSampleActive = false;
    transportClock.prepare (sampleRate);
    mechanismEngine.prepare (sampleRate);
    lastEvents = {};
    excitationEnvelope = 0.0f;
    excitationAttack = static_cast<float> (std::exp (-1.0 / (sampleRate * 0.002)));
    excitationRelease = static_cast<float> (std::exp (-1.0 / (sampleRate * 0.120)));

    for (auto* smoothed : { &forceAmount, &motorAmount, &relayAmount,
                            &frictionAmount, &bodyAmount, &feedbackAmount,
                            &driveAmount, &toneAmount, &irregularityAmount,
                            &stereoAmount, &mixAmount, &outputGain })
        smoothed->reset (sampleRate, 0.02);
    forceAmount.setCurrentAndTargetValue (forceParameter->load() * 0.01f);
    motorAmount.setCurrentAndTargetValue (motorParameter->load() * 0.01f);
    relayAmount.setCurrentAndTargetValue (relayParameter->load() * 0.01f);
    frictionAmount.setCurrentAndTargetValue (frictionParameter->load() * 0.01f);
    bodyAmount.setCurrentAndTargetValue (bodyParameter->load() * 0.01f);
    feedbackAmount.setCurrentAndTargetValue (feedbackParameter->load() * 0.01f);
    driveAmount.setCurrentAndTargetValue (driveParameter->load() * 0.01f);
    toneAmount.setCurrentAndTargetValue (toneParameter->load() * 0.01f);
    irregularityAmount.setCurrentAndTargetValue (irregularityParameter->load() * 0.01f);
    stereoAmount.setCurrentAndTargetValue (stereoParameter->load() * 0.01f);
    mixAmount.setCurrentAndTargetValue (mixParameter->load() * 0.01f);
    outputGain.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (outputParameter->load()));
}

bool MachineAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output && (output == juce::AudioChannelSet::mono()
                               || output == juce::AudioChannelSet::stereo());
}

void MachineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (layerSampleResetRequested.exchange (false, std::memory_order_acq_rel))
    {
        layerSamplePosition = 0.0;
        layerSampleActive = false;
    }
    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    machine::PatternSnapshot incoming;
    while (pendingPatterns.pop (incoming))
        activePattern = incoming;

    auto runtimePattern = activePattern;
    runtimePattern.activeSteps = static_cast<std::uint8_t> (
        std::clamp (juce::roundToInt (stepsParameter->load()), 1, 16));
    runtimePattern.swing = std::clamp (swingParameter->load() * 0.01f, 0.0f, 1.0f);
    runtimePattern.seed = static_cast<std::uint32_t> (
        std::clamp (juce::roundToInt (seedParameter->load()), 1, 65535));
    const auto globalProbability = std::clamp (probabilityParameter->load() * 0.01f,
                                                0.0f, 1.0f);
    const auto globalRatchet = static_cast<std::uint8_t> (
        std::clamp (juce::roundToInt (ratchetParameter->load()), 1, 4));
    for (auto& step : runtimePattern.steps)
    {
        step.probability = std::clamp (step.probability * globalProbability, 0.0f, 1.0f);
        step.ratchet = std::max (step.ratchet, globalRatchet);
    }

    machine::BlockContext context;
    context.sampleRate = getSampleRate();
    context.numSamples = buffer.getNumSamples();
    context.sync = syncParameter->load() >= 0.5f;
    context.rateIndex = static_cast<std::uint8_t> (
        std::clamp (juce::roundToInt (rateParameter->load()), 0, 6));
    context.host = readHostPosition();
    lastEvents = transportClock.processBlock (context, runtimePattern);

    if (lastEvents.discontinuity == machine::Discontinuity::seek
        || lastEvents.discontinuity == machine::Discontinuity::loopWrap
        || lastEvents.discontinuity == machine::Discontinuity::sourceChange)
        mechanismEngine.reset();
    if (lastEvents.discontinuity == machine::Discontinuity::stop)
        currentStepForUi.store (-1, std::memory_order_relaxed);

    forceAmount.setTargetValue (std::clamp (forceParameter->load() * 0.01f, 0.0f, 1.0f));
    motorAmount.setTargetValue (std::clamp (motorParameter->load() * 0.01f, 0.0f, 1.0f));
    relayAmount.setTargetValue (std::clamp (relayParameter->load() * 0.01f, 0.0f, 1.0f));
    frictionAmount.setTargetValue (std::clamp (frictionParameter->load() * 0.01f, 0.0f, 1.0f));
    bodyAmount.setTargetValue (std::clamp (bodyParameter->load() * 0.01f, 0.0f, 1.0f));
    feedbackAmount.setTargetValue (std::clamp (feedbackParameter->load() * 0.01f, 0.0f, 0.95f));
    driveAmount.setTargetValue (std::clamp (driveParameter->load() * 0.01f, 0.0f, 1.0f));
    toneAmount.setTargetValue (std::clamp (toneParameter->load() * 0.01f, -1.0f, 1.0f));
    irregularityAmount.setTargetValue (
        std::clamp (irregularityParameter->load() * 0.01f, 0.0f, 1.0f));
    stereoAmount.setTargetValue (std::clamp (stereoParameter->load() * 0.01f, 0.0f, 2.0f));
    mixAmount.setTargetValue (std::clamp (mixParameter->load() * 0.01f, 0.0f, 1.0f));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputParameter->load()));

    const auto mode = static_cast<machine::MechanismEngine::Mode> (
        std::clamp (juce::roundToInt (modeParameter->load()), 0, 2));
    const auto sampleData = layerSample.get();
    const auto sampleLayer = std::clamp (juce::roundToInt (
        parameters.getRawParameterValue (machineParameterIds::sampleLayer)->load()), 0, 6);
    const auto sampleBlend = parameters.getRawParameterValue (
        machineParameterIds::sampleBlend)->load() * 0.01f;
    const auto samplePitch = parameters.getRawParameterValue (
        machineParameterIds::samplePitch)->load();
    const auto sampleLoop = parameters.getRawParameterValue (
        machineParameterIds::sampleLoop)->load() >= 0.5f;
    std::size_t eventIndex = 0;
    constexpr auto halfPi = juce::MathConstants<float>::halfPi;
    const auto channels = std::min (buffer.getNumChannels(), getTotalNumOutputChannels());
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        std::array<float, 2> dry {};
        for (int channel = 0; channel < channels; ++channel)
            dry[static_cast<std::size_t> (channel)] = buffer.getSample (channel, sample);
        const auto inputMagnitude = channels == 2
                                        ? 0.5f * (std::abs (dry[0]) + std::abs (dry[1]))
                                        : std::abs (dry[0]);
        const auto safeMagnitude = std::isfinite (inputMagnitude)
                                       ? std::clamp (inputMagnitude, 0.0f, 1.0f) : 0.0f;
        const auto coefficient = safeMagnitude > excitationEnvelope
                                     ? excitationAttack : excitationRelease;
        excitationEnvelope = safeMagnitude
                             + coefficient * (excitationEnvelope - safeMagnitude);

        machine::MechanismEngine::Settings settings;
        settings.mode = mode;
        settings.force = forceAmount.getNextValue();
        settings.motor = motorAmount.getNextValue();
        settings.relay = relayAmount.getNextValue();
        settings.friction = frictionAmount.getNextValue();
        settings.body = bodyAmount.getNextValue();
        settings.feedback = feedbackAmount.getNextValue();
        settings.drive = driveAmount.getNextValue();
        settings.tone = toneAmount.getNextValue();
        settings.irregularity = irregularityAmount.getNextValue();
        settings.stereo = stereoAmount.getNextValue();
        while (eventIndex < lastEvents.size
               && lastEvents.events[eventIndex].sampleOffset == sample)
        {
            mechanismEngine.trigger (lastEvents.events[eventIndex], excitationEnvelope, settings);
            if (sampleData != nullptr && sampleLayer != 1 && sampleLayer != 2)
            {
                layerSamplePosition = 0.0;
                layerSampleActive = true;
            }
            currentStepForUi.store (lastEvents.events[eventIndex].stepIndex,
                                    std::memory_order_relaxed);
            ++eventIndex;
        }
        auto wet = mechanismEngine.processFrame (safeMagnitude, settings);
        if (sampleData != nullptr)
        {
            const auto continuous = sampleLayer == 1 || sampleLayer == 2;
            if (continuous) layerSampleActive = true;
            const auto inRange = layerSamplePosition < sampleData->audio.getNumSamples();
            if (layerSampleActive && (sampleLoop || inRange))
            {
                const auto layerAmount = sampleLayer == 1 ? settings.motor
                    : sampleLayer == 2 ? settings.friction * (0.2f + excitationEnvelope)
                    : sampleLayer == 3 ? settings.body
                    : sampleLayer == 4 ? settings.force
                    : sampleLayer == 5 ? settings.relay
                    : sampleLayer == 6 ? settings.feedback + settings.body * 0.35f
                                       : settings.force * 0.8f;
                for (int channel = 0; channel < 2; ++channel)
                    wet[static_cast<std::size_t> (channel)] +=
                        lsse::audio::UserSampleSlot::readLinear (*sampleData, channel,
                                                                  layerSamplePosition)
                        * sampleBlend * juce::jlimit (0.0f, 1.2f, layerAmount);
                const auto speedLink = 0.72 + settings.force * 0.5
                    + (continuous ? static_cast<double> (context.rateIndex) * 0.045 : 0.0);
                layerSamplePosition += sampleData->sampleRate / processingSampleRate
                    * std::pow (2.0, samplePitch / 12.0) * speedLink;
                if (layerSamplePosition >= sampleData->audio.getNumSamples())
                {
                    if (sampleLoop)
                        layerSamplePosition = std::fmod (
                            layerSamplePosition,
                            static_cast<double> (sampleData->audio.getNumSamples()));
                    else
                        layerSampleActive = false;
                }
            }
        }
        const auto mix = mixAmount.getNextValue();
        const auto dryGain = std::cos (mix * halfPi);
        const auto wetGain = std::sin (mix * halfPi);
        const auto gain = outputGain.getNextValue();
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto wetSample = channels == 1 ? 0.5f * (wet[0] + wet[1])
                                                 : wet[static_cast<std::size_t> (channel)];
            auto value = (dry[static_cast<std::size_t> (channel)] * dryGain
                          + wetSample * wetGain) * gain;
            if (! std::isfinite (value))
                value = 0.0f;
            else if (mix > 1.0e-5f && std::abs (value) > 0.98f)
                value = std::copysign (0.98f + 0.02f
                    * std::tanh ((std::abs (value) - 0.98f) / 0.02f), value);
            buffer.setSample (channel, sample, value);
        }
    }
}

void MachineAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&)
{
    buffer.clear();
}

juce::AudioProcessorEditor* MachineAudioProcessor::createEditor()
{
    return new MachineAudioProcessorEditor (*this);
}

machine::HostPosition MachineAudioProcessor::readHostPosition() const noexcept
{
    machine::HostPosition result;
    result.isPlaying = true;
    if (const auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
        {
            result.available = true;
            result.isPlaying = position->getIsPlaying();
            result.isLooping = position->getIsLooping();
            if (const auto bpm = position->getBpm())
                result.bpm = *bpm;
            if (const auto ppq = position->getPpqPosition())
                result.ppq = *ppq;
            if (const auto samples = position->getTimeInSamples())
                result.timeInSamples = *samples;
            if (const auto loop = position->getLoopPoints())
            {
                result.loopStartPpq = loop->ppqStart;
                result.loopEndPpq = loop->ppqEnd;
            }
        }
    }
    return result;
}

void MachineAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    state.setProperty ("stateVersion", stateVersion, nullptr);
    state.setProperty ("currentPreset", getCurrentFactoryPreset(), nullptr);
    {
        const juce::ScopedLock lock (layerSamplePathLock);
        state.setProperty ("layerSamplePath", layerSamplePath, nullptr);
    }
    const auto savedPattern = getPatternForUi();
    juce::ValueTree pattern ("PATTERN");
    pattern.setProperty ("version", 1, nullptr);
    pattern.setProperty ("revision", static_cast<juce::int64> (savedPattern.revision), nullptr);
    for (std::size_t index = 0; index < savedPattern.steps.size(); ++index)
    {
        const auto& source = savedPattern.steps[index];
        juce::ValueTree step ("STEP");
        step.setProperty ("index", static_cast<int> (index), nullptr);
        step.setProperty ("value", source.value, nullptr);
        step.setProperty ("accent", source.accent, nullptr);
        step.setProperty ("probability", source.probability, nullptr);
        step.setProperty ("ratchet", static_cast<int> (source.ratchet), nullptr);
        pattern.addChild (step, -1, nullptr);
    }
    state.addChild (pattern, -1, nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destination);
}

void MachineAudioProcessor::setStateInformation (const void* data, const int size)
{
    if (const auto xml = getXmlFromBinary (data, size))
    {
        if (! xml->hasTagName (parameters.state.getType()))
            return;
        const auto restored = juce::ValueTree::fromXml (*xml);
        auto migrated = parameters.copyState();
        for (int index = 0; index < restored.getNumChildren(); ++index)
        {
            const auto source = restored.getChild (index);
            if (source.hasType ("PATTERN"))
                continue;
            auto destination = migrated.getChildWithProperty ("id", source.getProperty ("id"));
            if (destination.isValid() && source.hasProperty ("value"))
                destination.setProperty ("value", source.getProperty ("value"), nullptr);
        }
        migrated.setProperty ("stateVersion", stateVersion, nullptr);
        parameters.replaceState (migrated);
        currentFactoryPreset.store (std::clamp (
            static_cast<int> (restored.getProperty ("currentPreset", 0)),
            0, getNumFactoryPresets() - 1), std::memory_order_relaxed);
        restorePattern (restored.getChildWithName ("PATTERN"));
        const auto samplePath = restored.getProperty ("layerSamplePath").toString();
        if (samplePath.isNotEmpty())
            loadLayerSample (juce::File (samplePath));
        else
            clearLayerSample();
    }
}

void MachineAudioProcessor::chooseLayerSample()
{
    if (fileChooser != nullptr) return;
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load MACHINE layer sample",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac", true);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (file.existsAsFile()) loadLayerSample (file);
            fileChooser.reset();
        });
}

bool MachineAudioProcessor::loadLayerSample (const juce::File& file)
{
    if (! layerSample.load (file)) return false;
    layerSampleResetRequested.store (true, std::memory_order_release);
    const juce::ScopedLock lock (layerSamplePathLock);
    layerSamplePath = file.getFullPathName();
    return true;
}

void MachineAudioProcessor::clearLayerSample()
{
    layerSample.clear();
    layerSampleResetRequested.store (true, std::memory_order_release);
    const juce::ScopedLock lock (layerSamplePathLock);
    layerSamplePath.clear();
}

void MachineAudioProcessor::restorePattern (const juce::ValueTree& tree) noexcept
{
    machine::PatternSnapshot restored;
    if (tree.isValid())
    {
        restored.revision = static_cast<std::uint32_t> (std::clamp<juce::int64> (
            static_cast<juce::int64> (tree.getProperty ("revision", 1)), 1, 0x7fffffff));
        std::array<bool, machine::maxSteps> seen {};
        for (int child = 0; child < tree.getNumChildren(); ++child)
        {
            const auto source = tree.getChild (child);
            if (! source.hasType ("STEP"))
                continue;
            const auto index = std::clamp (static_cast<int> (source.getProperty ("index", -1)),
                                           -1, static_cast<int> (machine::maxSteps - 1));
            if (index < 0 || seen[static_cast<std::size_t> (index)])
                continue;
            seen[static_cast<std::size_t> (index)] = true;
            auto& destination = restored.steps[static_cast<std::size_t> (index)];
            destination.value = clampedFloatProperty (source, "value", 1.0f, 0.0f, 1.0f);
            destination.accent = clampedFloatProperty (source, "accent", 0.0f, 0.0f, 1.0f);
            destination.probability = clampedFloatProperty (source, "probability", 1.0f,
                                                            0.0f, 1.0f);
            destination.ratchet = static_cast<std::uint8_t> (std::clamp (
                static_cast<int> (source.getProperty ("ratchet", 1)), 1, 4));
        }
    }
    activePattern = restored;
    {
        const juce::ScopedLock lock (uiPatternLock);
        uiPattern = restored;
    }
}

int MachineAudioProcessor::getNumFactoryPresets() const noexcept
{
    return static_cast<int> (factoryPresets.size());
}

int MachineAudioProcessor::getCurrentFactoryPreset() const noexcept
{
    return currentFactoryPreset.load (std::memory_order_relaxed);
}

void MachineAudioProcessor::applyFactoryPreset (const int index)
{
    if (index < 0 || index >= getNumFactoryPresets())
        return;
    const auto& preset = factoryPresets[static_cast<std::size_t> (index)];
    for (std::size_t parameterIndex = 0; parameterIndex < presetParameterIds.size(); ++parameterIndex)
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (
                parameters.getParameter (presetParameterIds[parameterIndex])))
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 (preset.values[parameterIndex]));
    submitPatternFromUi (makePresetPattern (index));
    currentFactoryPreset.store (index, std::memory_order_relaxed);
}

const juce::String MachineAudioProcessor::getFactoryPresetName (const int index) const
{
    if (index < 0 || index >= getNumFactoryPresets())
        return {};
    return factoryPresets[static_cast<std::size_t> (index)].name;
}

machine::PatternSnapshot MachineAudioProcessor::getPatternForUi() const
{
    const juce::ScopedLock lock (uiPatternLock);
    return uiPattern;
}

bool MachineAudioProcessor::submitPatternFromUi (machine::PatternSnapshot pattern)
{
    pattern.activeSteps = static_cast<std::uint8_t> (std::clamp<int> (pattern.activeSteps, 1, 16));
    pattern.swing = std::clamp (pattern.swing, 0.0f, 1.0f);
    pattern.seed = std::clamp<std::uint32_t> (pattern.seed, 1, 65535);
    pattern.revision = std::max<std::uint32_t> (pattern.revision, 1);
    for (auto& step : pattern.steps)
    {
        step.value = std::clamp (step.value, 0.0f, 1.0f);
        step.accent = std::clamp (step.accent, 0.0f, 1.0f);
        step.probability = std::clamp (step.probability, 0.0f, 1.0f);
        step.ratchet = static_cast<std::uint8_t> (std::clamp<int> (step.ratchet, 1, 4));
    }
    if (! pendingPatterns.push (pattern))
        return false;
    const juce::ScopedLock lock (uiPatternLock);
    uiPattern = pattern;
    return true;
}

juce::AudioProcessorValueTreeState::ParameterLayout
MachineAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto percent = juce::AudioParameterFloatAttributes {}.withLabel ("%");
    const auto choices = [] (std::initializer_list<const char*> values)
    {
        juce::StringArray result;
        for (const auto* value : values)
            result.add (value);
        return result;
    };

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { machineParameterIds::mode, 1 }, "MODE",
        choices ({ "HYBRID", "MOTOR", "RELAY" }), 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { machineParameterIds::sync, 1 }, "SYNC", true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { machineParameterIds::rate, 1 }, "RATE",
        choices ({ "1/32 / 16 Hz", "1/16T / 12 Hz", "1/16 / 8 Hz",
                   "1/8T / 6 Hz", "1/8 / 4 Hz", "1/4T / 3 Hz", "1/4 / 2 Hz" }), 2));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { machineParameterIds::steps, 1 }, "STEPS", 1, 16, 16));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { machineParameterIds::swing, 1 }, "SWING",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { machineParameterIds::probability, 1 }, "PROBABILITY",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 100.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { machineParameterIds::ratchet, 1 }, "RATCHET", 1, 4, 1));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { machineParameterIds::irregularity, 1 }, "IRREGULARITY",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { machineParameterIds::seed, 1 }, "SEED", 1, 65535, 1977));
    for (const auto parameter : {
             std::pair { machineParameterIds::force, "FORCE" },
             std::pair { machineParameterIds::motor, "MOTOR" },
             std::pair { machineParameterIds::relay, "RELAY" },
             std::pair { machineParameterIds::friction, "FRICTION" },
             std::pair { machineParameterIds::body, "BODY" } })
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { parameter.first, 1 }, parameter.second,
            juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f },
            juce::String (parameter.first) == machineParameterIds::friction ? 30.0f
                                                                             : 50.0f,
            percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { machineParameterIds::feedback, 1 }, "FEEDBACK",
        juce::NormalisableRange<float> { 0.0f, 95.0f, 0.1f }, 0.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { machineParameterIds::drive, 1 }, "DRIVE",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { machineParameterIds::tone, 1 }, "TONE",
        juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { machineParameterIds::stereo, 1 }, "STEREO",
        juce::NormalisableRange<float> { 0.0f, 200.0f, 0.1f }, 100.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { machineParameterIds::mix, 1 }, "MIX",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { machineParameterIds::output, 1 }, "OUTPUT",
        juce::NormalisableRange<float> { -24.0f, 6.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { machineParameterIds::sampleLayer, 1 }, "SAMPLE LAYER",
        choices ({ "ACTUATOR", "MOTOR", "FRICTION", "BODY", "IMPACT",
                   "ELECTRICAL", "RESONANCE" }), 4));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { machineParameterIds::sampleBlend, 1 }, "SAMPLE BLEND",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 50.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { machineParameterIds::samplePitch, 1 }, "SAMPLE PITCH",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("st")));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { machineParameterIds::sampleLoop, 1 }, "SAMPLE LOOP", false));
    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MachineAudioProcessor();
}
