#include "DistortionEngine.h"
#include <cmath>

namespace emberfox
{
namespace
{
float safeSample(float value) noexcept
{
    return std::isfinite(value) ? juce::jlimit(-1000000.0f, 1000000.0f, value) : 0.0f;
}
float safeControl(float value, float fallback, float low, float high) noexcept
{
    return std::isfinite(value) ? juce::jlimit(low, high, value) : fallback;
}
std::array<float, 5> shape(float value) noexcept
{
    const float x = juce::jlimit(-128.0f, 128.0f, value);
    const float cubic = juce::jlimit(-1.0f, 1.0f, x);
    // A triangle wave folds continuously at each odd integer; no discontinuous wrap.
    const float folded = 1.0f - std::abs(std::fmod(std::abs(x + 1.0f), 4.0f) - 2.0f);
    return {std::tanh(x), 1.5f * (cubic - cubic * cubic * cubic / 3.0f),
            juce::jlimit(-1.0f, 1.0f, x), folded,
            0.75f * (std::tanh(1.8f * x + 0.45f) - 0.421899f)};
}
}

void DistortionEngine::prepare(double sampleRate, int maximumBlock, int channels, const Settings& initial)
{
    rate = std::isfinite(sampleRate) && sampleRate >= 8000 ? sampleRate : 48000.0;
    blockCapacity = juce::jlimit(1, 65536, maximumBlock);
    channelCount = juce::jlimit(1, 2, channels);
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        size_t(channelCount), 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampler->initProcessing(size_t(blockCapacity));
    latencySamples = int(std::lround(oversampler->getLatencyInSamples()));
    scratch.setSize(channelCount, blockCapacity);
    scratch.clear();
    for (auto& delay : dryDelay) delay.assign(size_t(std::max(1, latencySamples)), 0.0f);
    delayIndex = 0; dcInput.fill(0); dcOutput.fill(0); toneState.fill(0);
    dcCoefficient = float(std::exp(-2.0 * juce::MathConstants<double>::pi * 12.0 / rate));
    lastInputPeak = lastOutputPeak = 0;
    drive.reset(rate * 4.0, 0.03);
    for (auto& weight : modeWeights) weight.reset(rate * 4.0, 0.02);
    for (auto* smoothed : {&tone, &mix, &output, &enabled}) smoothed->reset(rate, 0.025);
    setSettings(initial);
    drive.setCurrentAndTargetValue(drive.getTargetValue());
    for (auto& weight : modeWeights) weight.setCurrentAndTargetValue(weight.getTargetValue());
    for (auto* smoothed : {&tone, &mix, &output, &enabled})
        smoothed->setCurrentAndTargetValue(smoothed->getTargetValue());
}

void DistortionEngine::setSettings(const Settings& settings) noexcept
{
    drive.setTargetValue(juce::Decibels::decibelsToGain(safeControl(settings.driveDb, 9, 0, 36)));
    const float cutoff = safeControl(settings.toneHz, 12000, 400, float(std::min(20000.0, rate * 0.45)));
    tone.setTargetValue(float(std::exp(-2.0 * juce::MathConstants<double>::pi * cutoff / rate)));
    mix.setTargetValue(safeControl(settings.mix, 1, 0, 1));
    output.setTargetValue(juce::Decibels::decibelsToGain(safeControl(settings.outputDb, -6, -24, 6)));
    enabled.setTargetValue(settings.bypass ? 0.0f : 1.0f);
    const int mode = juce::jlimit(0, 4, settings.mode);
    for (size_t i = 0; i < modeWeights.size(); ++i)
        modeWeights[i].setTargetValue(int(i) == mode ? 1.0f : 0.0f);
}

void DistortionEngine::process(juce::AudioBuffer<float>& buffer) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    lastInputPeak = lastOutputPeak = 0;
    if (oversampler == nullptr || buffer.getNumChannels() < channelCount)
    {
        buffer.clear(); return;
    }
    for (int start = 0; start < buffer.getNumSamples(); start += blockCapacity)
        processChunk(buffer, start, std::min(blockCapacity, buffer.getNumSamples() - start));
    for (int c = channelCount; c < buffer.getNumChannels(); ++c) buffer.clear(c, 0, buffer.getNumSamples());
}

void DistortionEngine::processChunk(juce::AudioBuffer<float>& buffer, int offset, int count) noexcept
{
    for (int c = 0; c < channelCount; ++c)
    {
        auto* wet = scratch.getWritePointer(c);
        const auto* input = buffer.getReadPointer(c, offset);
        for (int i = 0; i < count; ++i)
        {
            wet[i] = safeSample(input[i]);
            lastInputPeak = std::max(lastInputPeak, std::abs(wet[i]));
        }
    }
    auto wetBlock = juce::dsp::AudioBlock<float>(scratch).getSubBlock(0, size_t(count));
    auto highRate = oversampler->processSamplesUp(wetBlock);
    for (size_t i = 0; i < highRate.getNumSamples(); ++i)
    {
        const float boost = drive.getNextValue();
        std::array<float, 5> weights{};
        for (size_t m = 0; m < weights.size(); ++m) weights[m] = modeWeights[m].getNextValue();
        for (int c = 0; c < channelCount; ++c)
        {
            const auto values = shape(highRate.getChannelPointer(size_t(c))[i] * boost);
            float shaped = 0;
            for (size_t m = 0; m < weights.size(); ++m) shaped += values[m] * weights[m];
            highRate.getChannelPointer(size_t(c))[i] = shaped;
        }
    }
    oversampler->processSamplesDown(wetBlock);
    for (int i = 0; i < count; ++i)
    {
        const float pole = tone.getNextValue();
        const float blend = mix.getNextValue() * enabled.getNextValue();
        const float wetGain = output.getNextValue();
        for (int c = 0; c < channelCount; ++c)
        {
            auto* destination = buffer.getWritePointer(c, offset);
            const float input = safeSample(destination[i]);
            const auto index = size_t(c);
            float dry = input;
            if (latencySamples > 0)
            {
                dry = dryDelay[index][size_t(delayIndex)];
                dryDelay[index][size_t(delayIndex)] = input;
            }
            const float x = scratch.getSample(c, i);
            const float dcRemoved = x - dcInput[index] + dcCoefficient * dcOutput[index];
            dcInput[index] = x; dcOutput[index] = dcRemoved;
            toneState[index] = (1.0f - pole) * dcRemoved + pole * toneState[index];
            destination[i] = dry + blend * (wetGain * toneState[index] - dry);
            lastOutputPeak = std::max(lastOutputPeak, std::abs(destination[i]));
        }
        if (++delayIndex >= std::max(1, latencySamples)) delayIndex = 0;
    }
}
}
