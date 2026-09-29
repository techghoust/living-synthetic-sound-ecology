#include "shared/audio/UserSampleSlot.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

int main()
{
    const auto file = juce::File::createTempFile ("lsse-user-sample.wav");
    try
    {
        juce::WavAudioFormat format;
        auto stream = file.createOutputStream();
        if (stream == nullptr)
            throw std::runtime_error ("could not create temporary WAV");
        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.release(), 48000.0, 1, 16, {}, 0));
        if (writer == nullptr)
            throw std::runtime_error ("could not create WAV writer");
        juce::AudioBuffer<float> source (1, 480);
        for (int sample = 0; sample < source.getNumSamples(); ++sample)
            source.setSample (0, sample, 0.5f * std::sin (sample * 0.071f));
        if (! writer->writeFromAudioSampleBuffer (source, 0, source.getNumSamples()))
            throw std::runtime_error ("could not write WAV source");
        writer.reset();

        lsse::audio::UserSampleSlot slot;
        if (! slot.load (file))
            throw std::runtime_error ("sample slot rejected a valid WAV");
        const auto loaded = slot.get();
        if (loaded == nullptr || loaded->audio.getNumSamples() != source.getNumSamples()
            || loaded->sampleRate != 48000.0 || slot.getPath() != file.getFullPathName())
            throw std::runtime_error ("loaded sample metadata changed");
        const auto interpolated = lsse::audio::UserSampleSlot::readLinear (*loaded, 0, 13.5);
        if (! std::isfinite (interpolated) || std::abs (interpolated) > 1.0f)
            throw std::runtime_error ("sample interpolation was invalid");
        slot.clear();
        if (slot.get() != nullptr)
            throw std::runtime_error ("sample slot did not clear");
        file.deleteFile();
        std::cout << "LSSE user sample slot tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        file.deleteFile();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
