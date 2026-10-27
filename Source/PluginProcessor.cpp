#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

const juce::StringArray EmberfoxProcessor::modeNames{"Velvet", "Crunch", "Razor", "Fold", "Fuzz"};

EmberfoxProcessor::EmberfoxProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "EMBERFOX_STATE", createParameters())
{
    driveValue = parameters.getRawParameterValue("drive");
    toneValue = parameters.getRawParameterValue("tone");
    mixValue = parameters.getRawParameterValue("mix");
    outputValue = parameters.getRawParameterValue("output");
    modeValue = parameters.getRawParameterValue("mode");
    bypassValue = parameters.getRawParameterValue("bypass");
    bypassParameter = parameters.getParameter("bypass");
}

juce::AudioProcessorValueTreeState::ParameterLayout EmberfoxProcessor::createParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using ID = juce::ParameterID;
    layout.add(std::make_unique<juce::AudioParameterFloat>(ID{"drive", 1}, "Drive",
        juce::NormalisableRange<float>(0, 36, 0.01f), 9.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(ID{"tone", 1}, "Tone",
        juce::NormalisableRange<float>(400, 20000, 1, 0.35f), 12000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(ID{"mix", 1}, "Mix",
        juce::NormalisableRange<float>(0, 100, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(ID{"output", 1}, "Output",
        juce::NormalisableRange<float>(-24, 6, 0.01f), -6.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<juce::AudioParameterChoice>(ID{"mode", 1}, "Mode", modeNames, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(ID{"bypass", 1}, "Bypass", false));
    return layout;
}

emberfox::Settings EmberfoxProcessor::settings(bool forceBypass) const noexcept
{
    return {driveValue->load(std::memory_order_relaxed), toneValue->load(std::memory_order_relaxed),
            mixValue->load(std::memory_order_relaxed) * 0.01f, outputValue->load(std::memory_order_relaxed),
            int(modeValue->load(std::memory_order_relaxed)),
            forceBypass || bypassValue->load(std::memory_order_relaxed) >= 0.5f};
}

void EmberfoxProcessor::prepareToPlay(double sampleRate, int maximumBlock)
{
    engine.prepare(sampleRate, maximumBlock, getTotalNumInputChannels(), settings());
    setLatencySamples(engine.latency());
    meterDecayPerSample = float(std::exp(-1.0 / (std::max(8000.0, sampleRate) * 0.22)));
    inputMeter.store(0); outputMeter.store(0);
}

bool EmberfoxProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto output = layout.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo())
        && layout.getMainInputChannelSet() == output;
}

void EmberfoxProcessor::processAudio(juce::AudioBuffer<float>& audio, bool forceBypass) noexcept
{
    engine.setSettings(settings(forceBypass)); engine.process(audio);
    const float decay = std::pow(meterDecayPerSample, float(audio.getNumSamples()));
    inputMeter.store(std::max(engine.inputPeak(), inputMeter.load(std::memory_order_relaxed) * decay), std::memory_order_relaxed);
    outputMeter.store(std::max(engine.outputPeak(), outputMeter.load(std::memory_order_relaxed) * decay), std::memory_order_relaxed);
}

void EmberfoxProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) { processAudio(audio, false); }
void EmberfoxProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) { processAudio(audio, true); }
juce::AudioProcessorEditor* EmberfoxProcessor::createEditor() { return new EmberfoxEditor(*this); }

void EmberfoxProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto tree = parameters.copyState();
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml, destination);
}

void EmberfoxProcessor::setStateInformation(const void* data, int bytes)
{
    if (data == nullptr || bytes <= 0) return;
    if (auto xml = getXmlFromBinary(data, bytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EmberfoxProcessor(); }

void EmberfoxProcessor::applyFactoryPreset(int index)
{
    if(index<0||index>=int(emberfox::factoryPresets.size()))return;
    const auto& preset=emberfox::factoryPresets[size_t(index)];
    const char* ids[]{"drive","tone","mix","output","mode","bypass"};
    const float values[]{preset.drive,preset.tone,preset.mix,preset.output,float(preset.mode),0};
    for(size_t i=0;i<6;++i)if(auto* parameter=parameters.getParameter(ids[i])){
        parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(values[i]));parameter->endChangeGesture();
    }
}
int EmberfoxProcessor::matchingFactoryPreset() const noexcept
{
    const std::array<float,6> values{driveValue->load(),toneValue->load(),mixValue->load(),outputValue->load(),modeValue->load(),bypassValue->load()};
    for(float value:values)if(!std::isfinite(value))return -1;
    if(values[5]>=.5f)return -1;
    for(size_t i=0;i<emberfox::factoryPresets.size();++i){
        const auto& p=emberfox::factoryPresets[i];
        if(std::abs(values[0]-p.drive)<.015f&&std::abs(values[1]-p.tone)<.6f&&std::abs(values[2]-p.mix)<.06f
           &&std::abs(values[3]-p.output)<.015f&&std::abs(values[4]-float(p.mode))<.01f)return int(i);
    }
    return -1;
}
