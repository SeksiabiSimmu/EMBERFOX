#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>
#include <vector>

namespace emberfox
{
struct Settings
{
    float driveDb = 9.0f;
    float toneHz = 12000.0f;
    float mix = 1.0f;
    float outputDb = -6.0f;
    int mode = 0;
    bool bypass = false;
};

class DistortionEngine
{
public:
    void prepare(double sampleRate, int maximumBlock, int channels, const Settings& initial = {});
    void setSettings(const Settings&) noexcept;
    void process(juce::AudioBuffer<float>&) noexcept;
    int latency() const noexcept { return latencySamples; }
    float inputPeak() const noexcept { return lastInputPeak; }
    float outputPeak() const noexcept { return lastOutputPeak; }

private:
    void processChunk(juce::AudioBuffer<float>&, int offset, int count) noexcept;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> scratch;
    std::array<std::vector<float>, 2> dryDelay;
    std::array<float, 2> dcInput{}, dcOutput{}, toneState{};
    juce::SmoothedValue<float> drive, tone, mix, output, enabled;
    std::array<juce::SmoothedValue<float>, 5> modeWeights;
    double rate = 48000.0;
    int blockCapacity = 0, channelCount = 0, latencySamples = 0, delayIndex = 0;
    float lastInputPeak = 0.0f, lastOutputPeak = 0.0f, dcCoefficient = 0.0f;
};
}
