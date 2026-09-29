#include "TextureProcessor.h"
#include "TextureEditor.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace textureParameterIds
{
constexpr auto input = "input";
constexpr auto capture = "capture";
constexpr auto grainSize = "grain_size";
constexpr auto density = "density";
constexpr auto spray = "spray";
constexpr auto pitch = "pitch";
constexpr auto reverse = "reverse";
constexpr auto blur = "blur";
constexpr auto freeze = "freeze";
constexpr auto detail = "detail";
constexpr auto wear = "wear";
constexpr auto dropout = "dropout";
constexpr auto motionRate = "motion_rate";
constexpr auto motionDepth = "motion_depth";
constexpr auto tone = "tone";
constexpr auto width = "width";
constexpr auto seed = "seed";
constexpr auto mix = "mix";
constexpr auto output = "output";
} 
namespace
{
struct FactoryPreset
{
    const char* name;
    std::array<float, 19> values;
};

constexpr std::array<const char*, 19> presetParameterIds {
    textureParameterIds::input, textureParameterIds::capture,
    textureParameterIds::grainSize, textureParameterIds::density,
    textureParameterIds::spray, textureParameterIds::pitch,
    textureParameterIds::reverse, textureParameterIds::blur,
    textureParameterIds::freeze, textureParameterIds::detail,
    textureParameterIds::wear, textureParameterIds::dropout,
    textureParameterIds::motionRate, textureParameterIds::motionDepth,
    textureParameterIds::tone, textureParameterIds::width,
    textureParameterIds::seed, textureParameterIds::mix,
    textureParameterIds::output
};

constexpr std::array<FactoryPreset, 8> factoryPresets {{
    { "INIT",           { 0, 60, 45, 8, 180, 0, 5, 0, 0, 20, 0, 0, 0.2f, 20, 0, 100, 1977, 0, 0 } },
    { "SOFT DUST",      { 0, 72, 28, 24, 240, 0, 8, 22, 0, 58, 18, 5, 0.35f, 35, -12, 125, 3101, 62, -1 } },
    { "VELVET SMEAR",   { 0, 85, 110, 38, 520, -3, 16, 78, 0, 26, 8, 0, 0.12f, 28, -35, 145, 8901, 76, -2 } },
    { "FROZEN GLASS",   { 0, 95, 75, 30, 380, 12, 12, 64, 1, 38, 5, 0, 0.08f, 18, 42, 165, 4421, 88, -3 } },
    { "BROKEN TAPE",    { 1, 82, 95, 18, 760, -5, 28, 35, 0, 18, 72, 58, 0.45f, 52, -28, 92, 7717, 72, -4 } },
    { "GRANULAR STORM", { -2, 100, 35, 72, 1450, 7, 48, 42, 0, 46, 31, 22, 2.4f, 92, 18, 190, 1207, 94, -6 } },
    { "AIR SHIMMER",    { 0, 68, 55, 34, 640, 12, 10, 48, 0, 82, 7, 2, 0.28f, 44, 55, 175, 6203, 70, -3 } },
    { "NARROW MACHINE", { 0, 78, 18, 55, 120, -12, 2, 12, 0, 12, 48, 18, 3.7f, 65, -18, 28, 991, 78, -5 } }
}};

constexpr int stateVersion = 3;
}

TextureAudioProcessor::TextureAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "TEXTURE_PARAMETERS", createParameterLayout())
{
    inputParameter = parameters.getRawParameterValue (textureParameterIds::input);
    captureParameter = parameters.getRawParameterValue (textureParameterIds::capture);
    grainSizeParameter = parameters.getRawParameterValue (textureParameterIds::grainSize);
    densityParameter = parameters.getRawParameterValue (textureParameterIds::density);
    sprayParameter = parameters.getRawParameterValue (textureParameterIds::spray);
    pitchParameter = parameters.getRawParameterValue (textureParameterIds::pitch);
    reverseParameter = parameters.getRawParameterValue (textureParameterIds::reverse);
    blurParameter = parameters.getRawParameterValue (textureParameterIds::blur);
    freezeParameter = parameters.getRawParameterValue (textureParameterIds::freeze);
    detailParameter = parameters.getRawParameterValue (textureParameterIds::detail);
    wearParameter = parameters.getRawParameterValue (textureParameterIds::wear);
    dropoutParameter = parameters.getRawParameterValue (textureParameterIds::dropout);
    motionRateParameter = parameters.getRawParameterValue (textureParameterIds::motionRate);
    motionDepthParameter = parameters.getRawParameterValue (textureParameterIds::motionDepth);
    toneParameter = parameters.getRawParameterValue (textureParameterIds::tone);
    widthParameter = parameters.getRawParameterValue (textureParameterIds::width);
    seedParameter = parameters.getRawParameterValue (textureParameterIds::seed);
    mixParameter = parameters.getRawParameterValue (textureParameterIds::mix);
    outputParameter = parameters.getRawParameterValue (textureParameterIds::output);

    jassert (inputParameter != nullptr && captureParameter != nullptr
             && grainSizeParameter != nullptr && densityParameter != nullptr
             && sprayParameter != nullptr && pitchParameter != nullptr
             && reverseParameter != nullptr && blurParameter != nullptr
             && freezeParameter != nullptr && detailParameter != nullptr
             && wearParameter != nullptr && dropoutParameter != nullptr
             && motionRateParameter != nullptr && motionDepthParameter != nullptr
             && toneParameter != nullptr && widthParameter != nullptr
             && seedParameter != nullptr
             && mixParameter != nullptr && outputParameter != nullptr);
}

void TextureAudioProcessor::prepareToPlay (const double sampleRate,
                                           const int /*maximumExpectedSamplesPerBlock*/)
{
    const auto capacity = static_cast<std::size_t> (std::ceil (sampleRate * captureSeconds));
    captureBuffer.prepare (static_cast<std::size_t> (getTotalNumInputChannels()), capacity);
    activeSeed = static_cast<std::uint32_t> (std::max (1.0f, seedParameter->load()));
    texture::GrainEngine::Settings settings;
    settings.seed = activeSeed;
    grainEngine.prepare (sampleRate, settings);
    updateGrainSettings();
    spectralSurface.prepare (static_cast<std::size_t> (getTotalNumInputChannels()));
    detailWear.prepare (sampleRate, activeSeed);
    dryDelay = {};
    dryDelayPosition = 0;

    inputGain.reset (sampleRate, 0.02);
    mixAmount.reset (sampleRate, 0.02);
    outputGain.reset (sampleRate, 0.02);
    inputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (inputParameter->load()));
    mixAmount.setCurrentAndTargetValue (std::clamp (mixParameter->load() * 0.01f, 0.0f, 1.0f));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParameter->load()));
    setLatencySamples (texture::SpectralSurface::getLatencySamples());
}

void TextureAudioProcessor::releaseResources()
{
    grainEngine.reset();
    spectralSurface.reset();
    detailWear.reset (activeSeed);
}

bool TextureAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output && (output == juce::AudioChannelSet::mono()
                               || output == juce::AudioChannelSet::stereo());
}

void TextureAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto channels = std::min (buffer.getNumChannels(), getTotalNumInputChannels());
    for (auto channel = channels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    const auto requestedSeed = static_cast<std::uint32_t> (std::max (1.0f, seedParameter->load()));
    if (requestedSeed != activeSeed)
    {
        activeSeed = requestedSeed;
        texture::GrainEngine::Settings settings;
        settings.seed = activeSeed;
        grainEngine.prepare (getSampleRate(), settings);
        detailWear.reset (activeSeed);
    }
    updateGrainSettings();
    spectralSurface.setParameters (blurParameter->load() * 0.01f,
                                   freezeParameter->load() >= 0.5f,
                                   toneParameter->load() * 0.01f);
    detailWear.setParameters (detailParameter->load() * 0.01f,
                              wearParameter->load() * 0.01f,
                              dropoutParameter->load() * 0.01f);
    inputGain.setTargetValue (juce::Decibels::decibelsToGain (inputParameter->load()));
    mixAmount.setTargetValue (std::clamp (mixParameter->load() * 0.01f, 0.0f, 1.0f));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputParameter->load()));
    const auto stereoWidth = std::clamp (widthParameter->load() * 0.01f, 0.0f, 2.0f);

    constexpr auto halfPi = juce::MathConstants<float>::halfPi;
    std::array<float, 2> dry {};
    std::array<float, 2> delayedDry {};
    std::array<float, 2> wet {};
    std::array<float, 2> spectral {};
    auto inputPeak = 0.0f;
    auto outputPeak = 0.0f;
    auto surfaceTotal = 0.0;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto inputLevel = inputGain.getNextValue();
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto value = buffer.getSample (channel, sample);
            dry[static_cast<std::size_t> (channel)] = std::isfinite (value)
                                                           ? value * inputLevel : 0.0f;
            inputPeak = std::max (inputPeak, std::abs (dry[static_cast<std::size_t> (channel)]));
        }

        captureBuffer.writeFrame (dry.data(), static_cast<std::size_t> (channels));
        grainEngine.processFrame (captureBuffer, wet.data(), static_cast<std::size_t> (channels));
        spectralSurface.processFrame (wet.data(), spectral.data(), static_cast<std::size_t> (channels));
        detailWear.processFrame (spectral.data(), static_cast<std::size_t> (channels));
        for (int channel = 0; channel < channels; ++channel)
            surfaceTotal += std::abs (spectral[static_cast<std::size_t> (channel)]);

        if (channels == 2)
        {
            const auto mid = 0.5f * (spectral[0] + spectral[1]);
            const auto side = 0.5f * (spectral[0] - spectral[1])
                              * stereoWidth;
            spectral[0] = mid + side;
            spectral[1] = mid - side;
        }

        const auto readPosition = (dryDelayPosition + 1) % dryDelay[0].size();
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto index = static_cast<std::size_t> (channel);
            dryDelay[index][dryDelayPosition] = dry[index];
            delayedDry[index] = dryDelay[index][readPosition];
        }
        dryDelayPosition = readPosition;

        const auto mix = mixAmount.getNextValue();
        const auto dryGain = std::cos (mix * halfPi);
        const auto wetGain = std::sin (mix * halfPi);
        const auto finalGain = outputGain.getNextValue();
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto index = static_cast<std::size_t> (channel);
            const auto combined = (delayedDry[index] * dryGain + spectral[index] * wetGain)
                                  * finalGain;
            const auto magnitude = std::abs (combined);
            const auto safe = magnitude <= 0.98f
                                  ? combined
                                  : std::copysign (0.98f + 0.02f
                                      * std::tanh ((magnitude - 0.98f) / 0.02f), combined);
            buffer.setSample (channel, sample, safe);
            outputPeak = std::max (outputPeak, std::abs (safe));
        }
    }

    const auto frameCount = std::max (1, buffer.getNumSamples() * std::max (1, channels));
    visualizationInput.store (std::clamp (inputPeak, 0.0f, 1.0f), std::memory_order_relaxed);
    visualizationOutput.store (std::clamp (outputPeak, 0.0f, 1.0f), std::memory_order_relaxed);
    visualizationSurface.store (std::clamp (static_cast<float> (surfaceTotal / frameCount)
                                                * 3.0f, 0.0f, 1.0f),
                                std::memory_order_relaxed);
    visualizationGrains.store (grainEngine.getActiveVoiceCount(), std::memory_order_relaxed);
    visualizationFrozen.store (freezeParameter->load() >= 0.5f, std::memory_order_relaxed);
}

void TextureAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer&)
{
    buffer.clear();
}

juce::AudioProcessorEditor* TextureAudioProcessor::createEditor()
{
    return new TextureAudioProcessorEditor (*this);
}

void TextureAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    if (auto state = parameters.copyState(); state.isValid())
    {
        state.setProperty ("stateVersion", stateVersion, nullptr);
        state.setProperty ("currentPreset", getCurrentFactoryPreset(), nullptr);
        if (const auto xml = state.createXml())
            copyXmlToBinary (*xml, destination);
    }
}

void TextureAudioProcessor::setStateInformation (const void* data, const int size)
{
    if (const auto xml = getXmlFromBinary (data, size))
    {
        if (xml->hasTagName (parameters.state.getType()))
        {
            const auto restored = juce::ValueTree::fromXml (*xml);
            auto migrated = parameters.copyState();
            for (int index = 0; index < restored.getNumChildren(); ++index)
            {
                const auto source = restored.getChild (index);
                auto destination = migrated.getChildWithProperty ("id", source.getProperty ("id"));
                if (destination.isValid() && source.hasProperty ("value"))
                    destination.setProperty ("value", source.getProperty ("value"), nullptr);
            }
            migrated.setProperty ("stateVersion", stateVersion, nullptr);
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

int TextureAudioProcessor::getNumPrograms()
{
                return 1;
}

int TextureAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TextureAudioProcessor::setCurrentProgram (const int)
{
}

const juce::String TextureAudioProcessor::getProgramName (const int index)
{
    return index == 0 ? "Default" : juce::String {};
}

int TextureAudioProcessor::getNumFactoryPresets() const noexcept
{
    return static_cast<int> (factoryPresets.size());
}

int TextureAudioProcessor::getCurrentFactoryPreset() const noexcept
{
    return currentFactoryPreset.load (std::memory_order_relaxed);
}

void TextureAudioProcessor::applyFactoryPreset (const int index)
{
    if (index < 0 || index >= getNumFactoryPresets())
        return;

    const auto& preset = factoryPresets[static_cast<std::size_t> (index)];
    for (std::size_t parameterIndex = 0; parameterIndex < presetParameterIds.size(); ++parameterIndex)
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (
                parameters.getParameter (presetParameterIds[parameterIndex])))
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 (preset.values[parameterIndex]));

    currentFactoryPreset.store (index, std::memory_order_relaxed);
}

const juce::String TextureAudioProcessor::getFactoryPresetName (const int index) const
{
    if (index < 0 || index >= getNumFactoryPresets())
        return {};
    return factoryPresets[static_cast<std::size_t> (index)].name;
}

TextureAudioProcessor::VisualizationSnapshot
TextureAudioProcessor::getVisualizationSnapshot() const noexcept
{
    return {
        visualizationInput.load (std::memory_order_relaxed),
        visualizationOutput.load (std::memory_order_relaxed),
        visualizationSurface.load (std::memory_order_relaxed),
        visualizationGrains.load (std::memory_order_relaxed),
        visualizationFrozen.load (std::memory_order_relaxed)
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout TextureAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto percent = juce::AudioParameterFloatAttributes {}.withLabel ("%");
    const auto db = juce::AudioParameterFloatAttributes {}.withLabel ("dB");

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::input, 1 }, "INPUT",
        juce::NormalisableRange<float> { -24.0f, 12.0f, 0.1f }, 0.0f, db));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::capture, 1 }, "CAPTURE",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 60.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::grainSize, 1 }, "GRAIN SIZE",
        juce::NormalisableRange<float> { 5.0f, 250.0f, 0.1f, 0.4f }, 45.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("ms")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::density, 1 }, "DENSITY",
        juce::NormalisableRange<float> { 0.5f, 80.0f, 0.1f, 0.4f }, 8.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("grains/s")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::spray, 1 }, "SPRAY",
        juce::NormalisableRange<float> { 0.0f, 2000.0f, 1.0f, 0.4f }, 180.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("ms")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::pitch, 1 }, "PITCH",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes {}.withLabel ("st")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::reverse, 1 }, "REVERSE",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 5.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::blur, 1 }, "BLUR",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { textureParameterIds::freeze, 1 }, "FREEZE", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::detail, 1 }, "DETAIL",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 20.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::wear, 1 }, "WEAR",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::dropout, 1 }, "DROPOUT",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::motionRate, 1 }, "MOTION RATE",
        juce::NormalisableRange<float> { 0.02f, 8.0f, 0.001f, 0.35f }, 0.2f,
        juce::AudioParameterFloatAttributes {}.withLabel ("Hz")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::motionDepth, 1 }, "MOTION DEPTH",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 20.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::tone, 1 }, "TONE",
        juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::width, 1 }, "WIDTH",
        juce::NormalisableRange<float> { 0.0f, 200.0f, 0.1f }, 100.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { textureParameterIds::seed, 1 }, "SEED", 1, 65535, 1977));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::mix, 1 }, "MIX",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, percent));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { textureParameterIds::output, 1 }, "OUTPUT",
        juce::NormalisableRange<float> { -24.0f, 6.0f, 0.1f }, 0.0f, db));
    return layout;
}

void TextureAudioProcessor::updateGrainSettings() noexcept
{
    texture::GrainEngine::Settings settings;
    settings.grainSizeSeconds = static_cast<double> (grainSizeParameter->load()) * 0.001;
    settings.densityPerSecond = densityParameter->load();
    settings.spraySeconds = static_cast<double> (sprayParameter->load()) * 0.001;
    settings.pitchSemitones = pitchParameter->load();
    settings.reverseProbability = reverseParameter->load() * 0.01f;
    settings.captureAmount = captureParameter->load() * 0.01f;
    settings.motionRateHz = motionRateParameter->load();
    settings.motionDepth = motionDepthParameter->load() * 0.01f;
    settings.stereoWidth = widthParameter->load() * 0.01f;
    settings.seed = activeSeed;
    grainEngine.updateSettings (settings);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TextureAudioProcessor();
}
