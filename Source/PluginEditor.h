#pragma once
#include "PluginProcessor.h"
#include "FoxAnimation.h"
#include "VisualState.h"
class EmberfoxHelpPanel;

class EmberfoxLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    EmberfoxLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout(juce::Slider&) override;
};

class EmberfoxEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EmberfoxEditor(EmberfoxProcessor&);
    ~EmberfoxEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    friend struct EmberfoxEditorTestAccess;
    void timerCallback() override;
    void drawFox(juce::Graphics&, juce::Rectangle<float>);
    void drawMeter(juce::Graphics&, juce::Rectangle<float>, float, const emberfox::PeakHold&, const juce::String&);
    EmberfoxProcessor& processor;
    EmberfoxLookAndFeel look;
    juce::Slider drive, tone, mix, output;
    std::array<juce::TextButton, 5> modes;
    juce::TextButton bypass;
    juce::ComboBox factoryMenu;
    juce::TextButton help;
    std::unique_ptr<EmberfoxHelpPanel> helpPanel;
    juce::TooltipWindow tooltip{this,600};
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment, toneAttachment, mixAttachment, outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    emberfox::FoxAnimation animation;
    emberfox::ModePersonality personality;
    emberfox::PeakHold inputHold, outputHold;
    float inputLevel = 0, outputLevel = 0;
    double lastFrameMs = 0.0;
    double lastMeterMs = 0.0;
    int selectedMode = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EmberfoxEditor)
};
