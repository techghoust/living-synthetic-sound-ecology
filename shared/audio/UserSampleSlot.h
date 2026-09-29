#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <cmath>
#include <memory>

namespace lsse::audio
{
struct LoadedSample
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 48000.0;
    juce::String path;
};

class UserSampleSlot
{
public:
    bool load (const juce::File& file, const double maximumSeconds = 120.0)
    {
        if (! file.existsAsFile())
            return false;

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        auto reader = formats.createReaderFor (file);
        if (reader == nullptr || reader->numChannels == 0 || reader->sampleRate < 8000.0)
            return false;

        const auto maximumSamples = static_cast<juce::int64> (
            std::llround (reader->sampleRate * juce::jlimit (1.0, 600.0, maximumSeconds)));
        const auto samples = static_cast<int> (
            juce::jlimit<juce::int64> (1, maximumSamples, reader->lengthInSamples));
        auto loaded = std::make_shared<LoadedSample>();
        loaded->audio.setSize (juce::jlimit (1, 2, static_cast<int> (reader->numChannels)),
                               samples, false, true, false);
        if (! reader->read (&loaded->audio, 0, samples, 0, true, true))
            return false;

        for (int channel = 0; channel < loaded->audio.getNumChannels(); ++channel)
            for (int sample = 0; sample < loaded->audio.getNumSamples(); ++sample)
                if (! std::isfinite (loaded->audio.getSample (channel, sample)))
                    loaded->audio.setSample (channel, sample, 0.0f);

        loaded->sampleRate = reader->sampleRate;
        loaded->path = file.getFullPathName();
        std::shared_ptr<const LoadedSample> published = std::move (loaded);
        std::atomic_store_explicit (&current, std::move (published), std::memory_order_release);
        return true;
    }

    void clear() noexcept
    {
        std::atomic_store_explicit (&current, std::shared_ptr<const LoadedSample> {},
                                    std::memory_order_release);
    }

    std::shared_ptr<const LoadedSample> get() const noexcept
    {
        return std::atomic_load_explicit (&current, std::memory_order_acquire);
    }

    juce::String getPath() const
    {
        if (const auto loaded = get())
            return loaded->path;
        return {};
    }

    static float readLinear (const LoadedSample& loaded, const int channel,
                             const double position) noexcept
    {
        const auto samples = loaded.audio.getNumSamples();
        if (samples <= 0)
            return 0.0f;
        const auto wrapped = position - std::floor (position / samples) * samples;
        const auto first = juce::jlimit (0, samples - 1, static_cast<int> (wrapped));
        const auto second = (first + 1) % samples;
        const auto fraction = static_cast<float> (wrapped - first);
        const auto sourceChannel = juce::jlimit (0, loaded.audio.getNumChannels() - 1, channel);
        const auto a = loaded.audio.getSample (sourceChannel, first);
        return a + (loaded.audio.getSample (sourceChannel, second) - a) * fraction;
    }

private:
    std::shared_ptr<const LoadedSample> current;
};
}
