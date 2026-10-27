#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "DistortionEngine.h"
#include "FactoryPresets.h"

class EmberfoxProcessor final : public juce::AudioProcessor
{
public:
    EmberfoxProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "EMBERFOX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }
    void applyFactoryPreset(int index); // Editor/message-thread operation; never called by processBlock.
    int matchingFactoryPreset() const noexcept;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> inputMeter{0}, outputMeter{0};
    static const juce::StringArray modeNames;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    emberfox::Settings settings(bool forceBypass = false) const noexcept;
    void processAudio(juce::AudioBuffer<float>&, bool forceBypass) noexcept;
    emberfox::DistortionEngine engine;
    std::atomic<float>* driveValue = nullptr;
    std::atomic<float>* toneValue = nullptr;
    std::atomic<float>* mixValue = nullptr;
    std::atomic<float>* outputValue = nullptr;
    std::atomic<float>* modeValue = nullptr;
    std::atomic<float>* bypassValue = nullptr;
    juce::AudioProcessorParameter* bypassParameter = nullptr;
    float meterDecayPerSample = 0.99995f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EmberfoxProcessor)
};
