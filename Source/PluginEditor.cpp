#include "PluginEditor.h"
#include <cmath>
#include "ProductHelp.h"

namespace
{
const juce::Colour background(0xff111416), panel(0xff1a1e20), border(0xff303638);
const juce::Colour cream(0xfff2eadb), muted(0xff8e9996), ember(0xffff794f), lime(0xffd6eb97);
juce::Font font(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions("Bahnschrift", size, bold ? juce::Font::bold : juce::Font::plain));
}
void text(juce::Graphics& g, const juce::String& string, juce::Rectangle<float> box,
          float size, juce::Colour colour, juce::Justification alignment = juce::Justification::centredLeft)
{
    g.setColour(colour); g.setFont(font(size)); g.drawText(string, box, alignment, false);
}
juce::Path polygon(std::initializer_list<juce::Point<float>> points)
{
    juce::Path path; bool first = true;
    for (auto point : points) { if (first) { path.startNewSubPath(point); first = false; } else path.lineTo(point); }
    path.closeSubPath(); return path;
}
}

class EmberfoxHelpPanel final : public juce::Component
{
public:
    std::function<void()> onClose;
    EmberfoxHelpPanel()
    {
        setName("helpPanel");setWantsKeyboardFocus(true);
        guide.setName("helpText");guide.setMultiLine(true);guide.setReadOnly(true);guide.setScrollbarsShown(true);
        guide.setFont(font(15));guide.setText(emberfoxHelpText(),false);
        guide.setColour(juce::TextEditor::backgroundColourId,panel);
        guide.setColour(juce::TextEditor::textColourId,cream);
        guide.setColour(juce::TextEditor::outlineColourId,juce::Colours::transparentBlack);
        addAndMakeVisible(guide);
        close.setName("closeHelp");close.setButtonText("CLOSE");close.onClick=[this]{if(onClose)onClose();};
        addAndMakeVisible(close);
    }
    void resized() override
    {
        guide.setBounds(getLocalBounds().reduced(22).withTrimmedBottom(42));
        close.setBounds(getWidth()-135,getHeight()-51,110,30);
    }
    void paint(juce::Graphics& g) override
    {
        g.setColour(panel);g.fillRoundedRectangle(getLocalBounds().toFloat(),12);
        g.setColour(ember.withAlpha(.55f));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),12,1);
    }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if(key==juce::KeyPress::escapeKey){if(onClose)onClose();return true;}return false;
    }
private:
    juce::TextEditor guide;
    juce::TextButton close;
};

EmberfoxLookAndFeel::EmberfoxLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, cream);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, cream);
    setColour(juce::TextEditor::backgroundColourId, background);
    setColour(juce::TextEditor::textColourId, cream);
    setColour(juce::TextEditor::highlightColourId, ember.withAlpha(0.4f));
}

juce::Label* EmberfoxLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label=juce::LookAndFeel_V4::createSliderTextBox(slider);
    label->setColour(juce::Label::outlineColourId,juce::Colours::transparentBlack);
    label->setColour(juce::Label::backgroundColourId,juce::Colours::transparentBlack);
    label->setColour(juce::Label::textColourId,cream);
    label->setFont(font(slider.getName()=="drive"?24.0f:14.0f,slider.getName()=="drive"));
    label->onEditorShow=[label]
    {
        if(auto* editor=label->getCurrentTextEditor())
        {
            editor->setFont(label->getFont());
            editor->setColour(juce::TextEditor::backgroundColourId,panel);
            editor->setColour(juce::TextEditor::outlineColourId,ember.withAlpha(0.35f));
            editor->setColour(juce::TextEditor::focusedOutlineColourId,ember.withAlpha(0.55f));
        }
    };
    return label;
}

juce::Slider::SliderLayout EmberfoxLookAndFeel::getSliderLayout(juce::Slider& slider)
{
    auto layout=juce::LookAndFeel_V4::getSliderLayout(slider);
    if(slider.getName()=="drive")
    {
        const float scale=float(slider.getHeight())/270.0f;
        layout.textBoxBounds=juce::Rectangle<int>(juce::roundToInt(200*scale),juce::roundToInt(32*scale))
            .withCentre({slider.getWidth()/2,slider.getHeight()-juce::roundToInt(16*scale)});
        // Keep the base slider drawing rectangle: a larger value must not shrink the knob.
        for(auto* child:slider.getChildren())if(auto* label=dynamic_cast<juce::Label*>(child))
            label->setFont(font(std::max(18.0f,24*scale),true));
    }
    return layout;
}

void EmberfoxLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float proportion, float start, float end, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>(float(x), float(y), float(width), float(height));
    const auto centre = bounds.getCentre();
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f - 10.0f;
    if (radius < 2) return;
    const bool large = slider.getName() == "drive";
    for (int i = 0; i <= (large ? 36 : 16); ++i)
    {
        const float angle = start + float(i) / float(large ? 36 : 16) * (end - start);
        const float r = radius + 5;
        g.setColour(border.brighter(0.18f));
        g.drawLine(centre.x + r * std::sin(angle), centre.y - r * std::cos(angle),
                   centre.x + (r + 3) * std::sin(angle), centre.y - (r + 3) * std::cos(angle), 1);
    }
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius - 4, radius - 4, 0, start, end, true);
    g.setColour(border); g.strokePath(track, juce::PathStrokeType(large ? 5.0f : 3.0f));
    juce::Path active;
    active.addCentredArc(centre.x, centre.y, radius - 4, radius - 4, 0, start, start + proportion * (end - start), true);
    g.setColour(large ? ember : lime);
    g.strokePath(active, juce::PathStrokeType(large ? 5.0f : 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto disc = juce::Rectangle<float>(radius * 1.54f, radius * 1.54f).withCentre(centre);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff343b3d), disc.getTopLeft(),
                                          juce::Colour(0xff171b1d), disc.getBottomRight(), false));
    g.fillEllipse(disc);
    g.setColour(juce::Colour(0xff4b5253)); g.drawEllipse(disc.reduced(0.5f), 1);
    g.setColour(background.withAlpha(0.6f)); g.drawEllipse(disc.reduced(4), 1);
    const float angle = start + proportion * (end - start);
    g.setColour(large ? ember : cream);
    g.drawLine(centre.x + radius * 0.40f * std::sin(angle), centre.y - radius * 0.40f * std::cos(angle),
               centre.x + radius * 0.65f * std::sin(angle), centre.y - radius * 0.65f * std::cos(angle), large ? 4.0f : 2.5f);
    if (large)
    {
        text(g, "DRIVE", {centre.x - 40, centre.y + 15, 80, 20}, 12, muted, juce::Justification::centred);
        g.setColour(cream.withAlpha(0.07f)); g.fillEllipse(centre.x - 15, centre.y - 15, 30, 30);
    }
}

void EmberfoxLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                              const juce::Colour&, bool over, bool down)
{
    auto area = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool selected = button.getToggleState();
    if (button.getName() == "bypass")
    {
        g.setColour(selected ? juce::Colour(0xff373b3a) : lime); g.fillRoundedRectangle(area, 6);
        return;
    }
    g.setColour(selected ? ember : (over || down ? juce::Colour(0xff303638) : panel));
    g.fillRoundedRectangle(area, 7);
    g.setColour(selected ? ember : border); g.drawRoundedRectangle(area, 7, 1);
}

void EmberfoxLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    if(button.getName()=="help"||button.getName()=="closeHelp"){
        text(g,button.getButtonText(),button.getLocalBounds().toFloat(),11,cream,juce::Justification::centred);return;
    }
    if (button.getName() == "bypass")
    {
        text(g, button.getToggleState() ? "BYPASSED" : "ACTIVE", button.getLocalBounds().toFloat(),
             13, button.getToggleState() ? muted : background, juce::Justification::centred); return;
    }
    const bool selected = button.getToggleState();
    const auto bounds = button.getLocalBounds().toFloat();
    const float scale = bounds.getHeight() / 53.0f;
    g.setFont(font(15 * scale, true)); g.setColour(selected ? background : cream);
    g.drawText(button.getButtonText(), bounds.reduced(15 * scale, 0).withHeight(29 * scale), juce::Justification::centredLeft);
    const juce::StringArray captions{"ROUND / WARM", "DENSE / PUNCHY", "HARD / BRIGHT", "FOLDED / WILD", "RAW / UNEVEN"};
    const int index = button.getName().getIntValue();
    text(g, captions[index], {15 * scale, 30 * scale, bounds.getWidth() - 25 * scale, 15 * scale}, 10 * scale,
         selected ? background.withAlpha(0.7f) : muted);
}

EmberfoxEditor::EmberfoxEditor(EmberfoxProcessor& p) : AudioProcessorEditor(p), processor(p)
{
    setLookAndFeel(&look);
    auto configure = [this](juce::Slider& slider, const juce::String& name, const juce::String& suffix)
    {
        slider.setName(name); slider.setTitle(name); slider.setDescription(name + " parameter");
        // Sliders create their value labels before parenting; explicitly apply our factory.
        slider.setLookAndFeel(&look);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 100, 25);
        slider.setTextValueSuffix(suffix);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setScrollWheelEnabled(false); addAndMakeVisible(slider);
    };
    configure(drive, "drive", " dB"); configure(tone, "tone", " Hz");
    configure(mix, "mix", " %"); configure(output, "output", " dB");
    drive.setNumDecimalPlacesToDisplay(1); tone.setNumDecimalPlacesToDisplay(0);
    mix.setNumDecimalPlacesToDisplay(1); output.setNumDecimalPlacesToDisplay(1);
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    driveAttachment = std::make_unique<Attachment>(processor.parameters, "drive", drive);
    toneAttachment = std::make_unique<Attachment>(processor.parameters, "tone", tone);
    mixAttachment = std::make_unique<Attachment>(processor.parameters, "mix", mix);
    outputAttachment = std::make_unique<Attachment>(processor.parameters, "output", output);
    drive.setDoubleClickReturnValue(true, 9); tone.setDoubleClickReturnValue(true, 12000);
    mix.setDoubleClickReturnValue(true, 100); output.setDoubleClickReturnValue(true, -6);
    for (size_t i = 0; i < modes.size(); ++i)
    {
        auto& button = modes[i]; button.setName(juce::String(int(i)));
        button.setButtonText(juce::String(int(i) + 1).paddedLeft('0', 2) + "  " + EmberfoxProcessor::modeNames[int(i)].toUpperCase());
        button.setTitle(EmberfoxProcessor::modeNames[int(i)]);
        button.onClick = [this, i] { modeAttachment->setValueAsCompleteGesture(float(i)); };
        addAndMakeVisible(button);
    }
    modeAttachment = std::make_unique<juce::ParameterAttachment>(*processor.parameters.getParameter("mode"),
        [this](float value)
        {
            selectedMode = juce::jlimit(0, 4, int(value));
            for (size_t i = 0; i < modes.size(); ++i) modes[i].setToggleState(int(i) == selectedMode, juce::dontSendNotification);
            repaint();
        }, nullptr);
    modeAttachment->sendInitialUpdate();
    bypass.setName("bypass"); bypass.setTitle("Bypass"); bypass.setClickingTogglesState(true);
    addAndMakeVisible(bypass);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "bypass", bypass);
    factoryMenu.setName("factoryPresets");factoryMenu.setTitle("Factory starting points");factoryMenu.setLookAndFeel(&look);
    factoryMenu.setColour(juce::ComboBox::backgroundColourId,panel);factoryMenu.setColour(juce::ComboBox::textColourId,cream);
    factoryMenu.setColour(juce::ComboBox::outlineColourId,border);factoryMenu.setColour(juce::ComboBox::arrowColourId,muted);
    factoryMenu.setTextWhenNothingSelected("Custom");
    for(size_t i=0;i<emberfox::factoryPresets.size();++i)factoryMenu.addItem(emberfox::factoryPresets[i].name,int(i)+1);
    factoryMenu.onChange=[this]{if(factoryMenu.getSelectedId()>0)processor.applyFactoryPreset(factoryMenu.getSelectedId()-1);};
    factoryMenu.setSelectedId(processor.matchingFactoryPreset()+1,juce::dontSendNotification);addAndMakeVisible(factoryMenu);
    help.setName("help");help.setTitle("Help and About");help.setButtonText("HELP / ABOUT");
    helpPanel=std::make_unique<EmberfoxHelpPanel>();addChildComponent(*helpPanel);
    helpPanel->onClose=[this]{helpPanel->setVisible(false);help.grabKeyboardFocus();};
    help.onClick=[this]{helpPanel->setVisible(!helpPanel->isVisible());if(helpPanel->isVisible()){helpPanel->toFront(true);helpPanel->grabKeyboardFocus();}};
    addAndMakeVisible(help);
    drive.setTooltip("Drive: boost the signal entering distortion.");tone.setTooltip("Tone: reduce high frequencies after distortion.");
    mix.setTooltip("Mix: blend original and processed audio.");output.setTooltip("Output: wet signal level only; dry audio is unchanged.");
    bypass.setTooltip("Bypass: hear latency-aligned dry audio.");factoryMenu.setTooltip("Factory starting points. Your changes become Custom; DAW state saves the complete sound.");
    help.setTooltip("Offline user guide, version and licensing.");
    setResizable(true, true); setResizeLimits(770, 503, 1470, 960);
    if (auto* constrainer = getConstrainer()) constrainer->setFixedAspectRatio(980.0 / 640.0);
    setSize(980, 640); startTimerHz(60);
}

EmberfoxEditor::~EmberfoxEditor()
{
    stopTimer();
    for(auto* slider:{&drive,&tone,&mix,&output})slider->setLookAndFeel(nullptr);
    factoryMenu.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void EmberfoxEditor::resized()
{
    const float sx = float(getWidth()) / 980.0f, sy = float(getHeight()) / 640.0f;
    auto place = [sx, sy](juce::Component& c, int x, int y, int w, int h)
    { c.setBounds(juce::roundToInt(x * sx), juce::roundToInt(y * sy), juce::roundToInt(w * sx), juce::roundToInt(h * sy)); };
    place(bypass, 843, 28, 111, 37);
    place(drive, 54, 161, 268, 270);
    place(tone, 370, 145, 145, 102); place(mix, 370, 269, 145, 102); place(output, 370, 393, 145, 102);
    for (int i = 0; i < 5; ++i) place(modes[size_t(i)], 37 + 184 * i, 540, 170, 53);
    place(factoryMenu,178,609,290,25);place(help,492,609,135,25);
    if(helpPanel)helpPanel->setBounds(getLocalBounds().reduced(juce::roundToInt(36*sx),juce::roundToInt(45*sy)));
}

void EmberfoxEditor::timerCallback()
{
    if (!isShowing()) { lastFrameMs = 0.0; return; }
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float elapsed = lastFrameMs == 0.0 ? 1.0f / 60.0f : float((now - lastFrameMs) * 0.001);
    lastFrameMs = now;
    const int presetId=processor.matchingFactoryPreset()+1;
    if(factoryMenu.getSelectedId()!=presetId){
        if(presetId>0)factoryMenu.setSelectedId(presetId,juce::dontSendNotification);
        else factoryMenu.setText("Custom",juce::dontSendNotification);
    }
    inputLevel = processor.inputMeter.load(std::memory_order_relaxed);
    outputLevel = processor.outputMeter.load(std::memory_order_relaxed);
    animation.update(inputLevel, outputLevel, float(drive.getValue() / 36.0), bypass.getToggleState(), elapsed);
    personality.update(selectedMode,bypass.getToggleState(),elapsed);
    const double meterElapsed=lastMeterMs==0.0?1.0/60.0:(now-lastMeterMs)*0.001;
    lastMeterMs=now;
    inputHold.update(inputLevel,meterElapsed);outputHold.update(outputLevel,meterElapsed);
    // Knobs and static chrome need no animation repaint; parameter attachments update them separately.
    repaint(juce::Rectangle<float>(566, 117, 388, 391)
        .transformedBy(juce::AffineTransform::scale(float(getWidth()) / 980.0f, float(getHeight()) / 640.0f)).getSmallestIntegerContainer());
}

void EmberfoxEditor::drawMeter(juce::Graphics& g, juce::Rectangle<float> area, float peak, const emberfox::PeakHold& held, const juce::String& name)
{
    text(g, name, area.withHeight(16), 10, muted);
    const float db = juce::Decibels::gainToDecibels(peak, -60.0f);
    text(g, db <= -59.9f ? "-inf" : juce::String(db, 1), area.withHeight(16), 10,
         peak > 1.0f ? ember : cream, juce::Justification::centredRight);
    auto bar = area.withTrimmedTop(22).withHeight(5);
    g.setColour(border); g.fillRoundedRectangle(bar, 2);
    g.setColour(peak > 1.0f ? ember : lime);
    g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * emberfox::PeakHold::position(db)), 2);
    const float zero=bar.getX()+bar.getWidth()*emberfox::PeakHold::position(0);
    g.setColour(cream.withAlpha(0.40f));g.drawLine(zero,bar.getY()-3,zero,bar.getBottom()+3,0.8f);
    text(g,"0",{zero-6,bar.getBottom()+4,12,12},10,muted,juce::Justification::centred);
    if(held.db>-59.9f)
    {
        const float marker=bar.getX()+juce::jlimit(0.75f,bar.getWidth()-0.75f,bar.getWidth()*emberfox::PeakHold::position(held.db));
        g.setColour(held.db>0?ember:cream.withAlpha(0.90f));
        g.fillRect(marker-0.75f,bar.getY()-2,1.5f,9.0f);
    }
}

void EmberfoxEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
    g.addTransform(juce::AffineTransform::scale(float(getWidth()) / 980.0f, float(getHeight()) / 640.0f));
    g.setColour(ember); g.fillPath(polygon({{26, 28}, {36, 45}, {26, 63}, {46, 45}}));
    g.setFont(font(32, true)); g.setColour(cream); g.drawText("EMBERFOX", 57, 22, 300, 42, juce::Justification::centredLeft);
    text(g, "DISTORTION WITH INSTINCT", {59, 68, 300, 18}, 11, muted);
    text(g, "4x FIR  /  MONO + STEREO", {610, 38, 211, 18}, 11, muted, juce::Justification::centredRight);
    g.setColour(border); g.drawLine(26, 101, 954, 101, 1);
    g.setColour(panel); g.fillRoundedRectangle(26, 117, 524, 391, 12); g.fillRoundedRectangle(566, 117, 388, 391, 12);
    g.setColour(border); g.drawRoundedRectangle(26.5f, 117.5f, 523, 390, 12, 1);
    g.drawRoundedRectangle(566.5f, 117.5f, 387, 390, 12, 1);
    text(g, "01 / DRIVE", {50, 133, 220, 22}, 12, muted);
    for (int i = 0; i < 3; ++i)
        text(g, juce::StringArray{"TONE", "MIX", "OUTPUT / WET"}[i], {370, float(128 + 124 * i), 145, 20},
             11, muted, juce::Justification::centred);
    g.setColour(border); g.drawLine(345, 145, 345, 479, 1);
    text(g, "MAKE IT BITE.", {62, 439, 252, 29}, 24, cream, juce::Justification::centred);
    text(g, "Double-click any knob to reset", {53, 478, 270, 16}, 10, muted, juce::Justification::centred);
    text(g, "02 / SIGNAL CREATURE", {588, 133, 240, 22}, 12, muted);
    g.setColour(bypass.getToggleState() ? muted : lime); g.fillEllipse(921, 140, 6, 6);
    drawFox(g, {605, 163, 310, 268});
    drawMeter(g, {590, 455, 153, 35}, inputLevel, inputHold, "INPUT / dBFS");
    drawMeter(g, {773, 455, 153, 35}, outputLevel, outputHold, "OUTPUT / dBFS");
    text(g, "03 / CHARACTER", {28, 516, 300, 19}, 11, muted);
    text(g, "FIVE WAYS TO LOSE YOUR MANNERS", {583, 516, 370, 19}, 10, muted, juce::Justification::centredRight);
    text(g, "EmberSoft", {28, 613, 135, 17}, 10, muted);
    text(g, juce::String("ORIGINAL DSP  /  v") + JucePlugin_VersionString, {690, 613, 264, 17}, 10, muted, juce::Justification::centredRight);
}

void EmberfoxEditor::drawFox(juce::Graphics& g, juce::Rectangle<float> area)
{
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(area.getWidth() / 300.0f, area.getHeight() / 260.0f)
                       .translated(area.getX(), area.getY()));
    const float light = animation.outputEnergy;
    const float expression = animation.expression;
    const float pulse = animation.transient;
    const bool active = !bypass.getToggleState();
    const auto furTop = active ? juce::Colour(0xffffad7b) : juce::Colour(0xffb18e7b);
    const auto furBottom = active ? juce::Colour(0xffb34d33) : juce::Colour(0xff705a50);
    juce::ColourGradient halo(ember.withAlpha(0.025f + light * 0.24f), 150, 130,
                              ember.withAlpha(0.0f), 295, 130, true);
    halo.addColour(0.55, ember.withAlpha(light * 0.09f));
    g.setGradientFill(halo); g.fillEllipse(5, -15, 290, 290);
    g.setColour(background.withAlpha(0.8f)); g.fillEllipse(78.5f,249,143,12);

    const float idle = std::sin(animation.phase * juce::MathConstants<float>::twoPi / 2.9f)
                     * (0.6f + animation.inputEnergy * 1.2f);
    const auto& pose=personality.pose;
    g.addTransform(juce::AffineTransform::translation(150, 128 + idle - pulse * 4));
    g.addTransform(juce::AffineTransform::scale((0.97f + pulse * 0.012f)*1.10f));
    const float ear = pulse * 3;
    const juce::Point<float> leftTip{-108-ear-pose.earSpread*10,-112-ear+pose.earDrop*12};
    const juce::Point<float> rightTip{-leftTip.x,leftTip.y};
    auto silhouette = polygon({leftTip, {-43, -77}, {0, -62}, {43, -77}, rightTip,
        {90, -21}, {106, 18}, {68, 62}, {47, 91}, {0, 114}, {-47, 91}, {-68, 62}, {-106, 18}, {-90, -21}});
    g.setGradientFill(juce::ColourGradient(furTop, -70, -100, furBottom, 90, 110, false));
    g.fillPath(silhouette); g.setColour(furTop.withAlpha(0.75f)); g.strokePath(silhouette, juce::PathStrokeType(0.9f));
    g.setColour(furTop.brighter(0.1f));
    g.fillPath(polygon({leftTip, {-91, -27}, {-43, -77}}));
    g.setColour(furBottom.brighter(0.14f));
    g.fillPath(polygon({rightTip, {91, -27}, {43, -77}}));
    g.setColour(juce::Colour(0xff542d29));
    g.fillPath(polygon({{-94+(leftTip.x+108)*0.7f,-93+(leftTip.y+112)*0.7f}, {-52, -74}, {-83, -40}}));
    g.fillPath(polygon({{94+(rightTip.x-108)*0.7f,-93+(rightTip.y+112)*0.7f}, {52, -74}, {83, -40}}));
    g.setColour(juce::Colour(0xffffbf80).withAlpha(0.16f));
    g.fillPath(polygon({{0, -62}, {0, 29}, {-40, 8}, {-67, -50}}));
    g.setColour(juce::Colour(0xffefe4cb));
    g.fillPath(polygon({{-94, 19}, {-43, 35}, {0, 82}, {0, 107}, {-43, 85}, {-62, 55}}));
    g.setColour(juce::Colour(0xffded4bc));
    g.fillPath(polygon({{94, 19}, {43, 35}, {0, 82}, {0, 107}, {43, 85}, {62, 55}}));

    const float slant=3+pose.focus*11+expression*3;
    const float aperture=7.4f+pose.openness*5-expression*2;
    const float outer=-12-slant*0.5f,inner=-12+slant*0.5f;
    const float eyeHeight=0.6f+std::max(1.0f,aperture)*animation.eyeOpen;
    const float brow=5+std::max(0.0f,pose.focus)*2;
    const float outside=-44-pose.eyeWidth*0.5f,inside=-44+pose.eyeWidth*0.5f;
    const auto mirror=juce::AffineTransform::scale(-1.0f,1.0f);
    g.setColour(juce::Colour(0xff4b3028));
    const auto socket=polygon({{outside-6,outer-brow},{inside+5,inner-brow},{inside+3,inner+eyeHeight+4},{outside-6,outer+eyeHeight+4}});
    g.fillPath(socket);g.fillPath(socket,mirror);
    const auto leftEye = polygon({{outside,outer},{inside,inner},{inside-2,inner+eyeHeight},{outside+1,outer+eyeHeight}});
    auto rightEye=leftEye;rightEye.applyTransform(mirror);
    for (float width : {13.0f, 8.0f, 4.0f})
    {
        g.setColour(lime.withAlpha(light * 0.055f));
        g.strokePath(leftEye, juce::PathStrokeType(width, juce::PathStrokeType::curved));
        g.strokePath(rightEye, juce::PathStrokeType(width, juce::PathStrokeType::curved));
    }
    g.setColour(juce::Colour(0xff82926b).interpolatedWith(juce::Colour(0xffecffc5), light));
    g.fillPath(leftEye); g.fillPath(rightEye);
    g.setColour(juce::Colour(0xff202624));
    g.fillPath(polygon({{-12, 54}, {12, 54}, {0, 65}}));
    const float mouthCurve=84-pose.tension*12+expression*3;
    // The nose stem meets the closed mouth's Bezier midpoint in every pose.
    g.drawLine(0,65,0,37+mouthCurve*0.5f,1.6f);
    juce::Path mouth; mouth.startNewSubPath(-pose.mouthWidth,74);
    mouth.quadraticTo(0,mouthCurve,pose.mouthWidth,74);
    g.strokePath(mouth, juce::PathStrokeType(1.6f));
}
