#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "PluginEditor.h"
#include <cstdlib>
#include <new>

thread_local bool trackCallbackAllocations = false;
thread_local int callbackAllocations = 0;
void* operator new(std::size_t n)
{
    if (trackCallbackAllocations) ++callbackAllocations;
    if (void* p = std::malloc(n == 0 ? 1 : n)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

// Only the test executable defines this friend. No test controls ship in the UI.
struct EmberfoxEditorTestAccess
{
    static void settlePortrait(EmberfoxEditor& editor)
    {
        editor.stopTimer();
        editor.animation={};editor.personality={};
        editor.inputLevel=editor.processor.inputMeter.load();
        editor.outputLevel=editor.processor.outputMeter.load();
        for(int i=0;i<120;++i)
        {
            editor.animation.update(editor.inputLevel,editor.outputLevel,float(editor.drive.getValue()/36),editor.bypass.getToggleState(),1.0f/60);
            editor.personality.update(editor.selectedMode,editor.bypass.getToggleState(),1.0f/60);
        }
        editor.inputHold.update(editor.inputLevel,2.0);editor.outputHold.update(editor.outputLevel,2.0);
    }
    static void resume(EmberfoxEditor& editor){editor.lastFrameMs=0;editor.startTimerHz(60);}
    static void worstTransient(EmberfoxEditor& editor)
    {
        editor.stopTimer();
        editor.animation.phase=2.175f;editor.animation.inputEnergy=1;editor.animation.outputEnergy=1;
        editor.animation.expression=1;editor.animation.transient=1;editor.animation.eyeOpen=1;
    }
};

namespace
{
void require(bool value, const juce::String& message)
{
    if (!value) throw std::runtime_error(message.toStdString());
}
void pump(int milliseconds = 60)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
}
juce::AudioProcessorParameter* parameter(juce::AudioProcessor& p, const juce::String& name)
{
    for (auto* entry : p.getParameters())
        if (entry->getName(128).equalsIgnoreCase(name)) return entry;
    throw std::runtime_error("Missing parameter: " + name.toStdString());
}
void set(juce::AudioProcessorParameter* p, float value)
{
    p->beginChangeGesture(); p->setValueNotifyingHost(value); p->endChangeGesture();
}
void writeWav(const juce::File& file, const juce::AudioBuffer<float>& buffer)
{
    file.deleteFile();
    juce::WavAudioFormat format;
    auto stream = file.createOutputStream();
    require(stream != nullptr, "Cannot open WAV output");
    auto* writer = format.createWriterFor(stream.get(), 48000.0, (unsigned)buffer.getNumChannels(), 24, {}, 0);
    require(writer != nullptr, "Cannot create WAV writer"); stream.release();
    std::unique_ptr<juce::AudioFormatWriter> owner(writer);
    require(writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()), "WAV write failed");
}
void writePng(const juce::File& file, const juce::Image& image)
{
    juce::MemoryOutputStream bytes;
    require(juce::PNGImageFormat().writeImageToStream(image, bytes), "PNG encode failed");
    require(file.replaceWithData(bytes.getData(), bytes.getDataSize()), "PNG replacement failed");
}
void inspect(juce::AudioPluginInstance& plugin, const juce::PluginDescription& description)
{
    std::cout << "Name: " << description.name << "\nVendor: " << description.manufacturerName
              << "\nVersion: " << description.version << "\nCategory: " << description.category << '\n';
    for (auto* p : plugin.getParameters())
    {
        std::cout << "Parameter " << p->getParameterIndex() << ": " << p->getName(128)
                  << " | default=" << p->getDefaultValue() << " | current=" << p->getValue()
                  << " | text=" << p->getCurrentValueAsText() << " | label=" << p->getLabel() << '\n';
        if (p->getNumSteps() <= 16)
        {
            const int steps = p->getNumSteps();
            for (int i = 0; i < steps; ++i)
                std::cout << "  " << i << ": " << p->getText(float(i) / float(std::max(1, steps - 1)), 128) << '\n';
        }
    }
}
void clickControls(juce::Component& root, int& sliders, int& buttons, int& combos)
{
    if(root.getName()=="factoryPresets"||root.getName()=="help"||root.getName()=="helpPanel")return;
    if (auto* slider = dynamic_cast<juce::Slider*>(&root))
    {
        slider->setValue(slider->getRange().getStart(), juce::sendNotificationSync);
        slider->setValue(slider->getRange().getEnd(), juce::sendNotificationSync); ++sliders;
    }
    if (auto* button = dynamic_cast<juce::Button*>(&root))
    {
        button->triggerClick(); ++buttons;
    }
    if (auto* combo = dynamic_cast<juce::ComboBox*>(&root))
    {
        if (combo->getNumItems() > 0) combo->setSelectedItemIndex(combo->getNumItems() - 1, juce::sendNotificationSync);
        ++combos;
    }
    for (auto* child : root.getChildren()) clickControls(*child, sliders, buttons, combos);
}
void verifyReleaseControls(juce::AudioProcessorEditor& editor,EmberfoxProcessor& processor,const juce::File& output)
{
    juce::ComboBox* presets=nullptr;juce::Button* help=nullptr;
    for(auto* child:editor.getChildren()){
        if(child->getName()=="factoryPresets")presets=dynamic_cast<juce::ComboBox*>(child);
        if(child->getName()=="help")help=dynamic_cast<juce::Button*>(child);
    }
    require(presets!=nullptr&&help!=nullptr,"Factory preset and Help/About controls are missing");
    require(presets->getNumItems()==int(emberfox::factoryPresets.size()),"Factory menu incomplete");
    for(int i=0;i<int(emberfox::factoryPresets.size());++i){
        presets->setSelectedId(i+1,juce::sendNotificationSync);pump(25);
        require(processor.matchingFactoryPreset()==i,"Preset UI did not load host parameters");
    }
    set(processor.parameters.getParameter("drive"),.321f);pump(40);
    require(presets->getSelectedId()==0&&presets->getText()=="Custom","Edited preset did not become Custom");
    help->triggerClick();pump(30);
    juce::Component* panel=nullptr;juce::TextEditor* guide=nullptr;juce::Button* close=nullptr;
    for(auto* child:editor.getChildren())if(child->getName()=="helpPanel")panel=child;
    require(panel!=nullptr&&panel->isVisible(),"Help/About panel did not open");
    for(auto* child:panel->getChildren()){
        if(child->getName()=="helpText")guide=dynamic_cast<juce::TextEditor*>(child);
        if(child->getName()=="closeHelp")close=dynamic_cast<juce::Button*>(child);
    }
    require(guide&&guide->isReadOnly()&&guide->getText().contains("EmberSoft")&&guide->getText().contains("wet signal")
            &&guide->getText().contains("AGPL")&&close,"Offline guide or publisher/license information incomplete");
    writePng(output.getChildFile("emberfox-help.png"),editor.createComponentSnapshot(editor.getLocalBounds()));
    editor.setSize(770,503);pump(20);writePng(output.getChildFile("emberfox-help-min.png"),editor.createComponentSnapshot(editor.getLocalBounds()));
    close->triggerClick();pump(20);require(!panel->isVisible(),"Help/About did not close");
    editor.setSize(980,640);processor.applyFactoryPreset(0);pump(25);
    std::cout<<"PASS factory menu, Custom edits, offline Help/About, publisher/license, min-size help and close lifecycle\n";
}
void verifyValueEditing(juce::AudioProcessorEditor& editor, EmberfoxProcessor& processor, const juce::File& output)
{
    const juce::StringArray names{"drive","tone","mix","output"};
    const double typedValues[]{18.5,3456,42,-3.5};
    const double defaults[]{9,12000,100,-6};
    for(int index=0;index<4;++index)
    {
        juce::Slider* slider=nullptr;
        for(auto* child:editor.getChildren()) if(auto* candidate=dynamic_cast<juce::Slider*>(child))
            if(candidate->getName()==names[index]) slider=candidate;
        require(slider!=nullptr,"Missing editable knob");
        juce::Label* label=nullptr;
        for(auto* child:slider->getChildren()) if(auto* candidate=dynamic_cast<juce::Label*>(child)) label=candidate;
        require(label!=nullptr&&label->isEditableOnSingleClick(),"Click-to-edit value label missing");
        require(label->findColour(juce::Label::outlineColourId).isTransparent(),"Normal numeric values still have rectangular borders");
        if(index==0)
        {
            require(label->getFont().getHeight()>=23.0f,"Drive value is not enlarged");
            juce::LookAndFeel_V4 originalLayout;
            require(slider->getLookAndFeel().getSliderLayout(*slider).sliderBounds==originalLayout.getSliderLayout(*slider).sliderBounds,
                    "Larger Drive value shrank or moved the knob");
        }
        // Off-screen peers intentionally cannot pass OS hit-testing; assert single-click
        // editability above, then exercise the same native Label editor lifecycle directly.
        label->showEditor();auto* textEditor=label->getCurrentTextEditor();
        require(textEditor!=nullptr,"Value editor did not open");
        textEditor->setText(juce::String(typedValues[index])+slider->getTextValueSuffix(),false);
        if(index==0)writePng(output.getChildFile("emberfox-editing.png"),editor.createComponentSnapshot(editor.getLocalBounds()));
        textEditor->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));pump(25);
        require(std::abs(slider->getValue()-typedValues[index])<0.02,"Enter failed to commit numeric value");
        require(std::abs(processor.parameters.getRawParameterValue(names[index])->load()-typedValues[index])<0.02,"Typed value did not reach host parameter");
        label->showEditor();textEditor=label->getCurrentTextEditor();require(textEditor!=nullptr,"Value editor did not reopen");
        textEditor->setText(juce::String(slider->getMaximum()),false);
        textEditor->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));pump(25);
        require(std::abs(slider->getValue()-typedValues[index])<0.02,"Escape failed to cancel numeric edit");
        require(slider->isDoubleClickReturnEnabled()&&std::abs(slider->getDoubleClickReturnValue()-defaults[index])<.02,"Double-click default changed");
        const auto now=juce::Time::getCurrentTime();
        juce::MouseEvent doubleClick(juce::Desktop::getInstance().getMainMouseSource(),{10,10},{},1,0,0,0,0,
                                     slider,slider,now,{10,10},now,2,false);
        slider->mouseDoubleClick(doubleClick);pump(20);
        require(std::abs(slider->getValue()-defaults[index])<.02,"Double-click failed to reset the value");
    }
    std::cout<<"PASS borderless values, larger Drive text, single-click editors, typed units, Enter/Escape and parameter attachments\n";
}
void nativeUi(const juce::File& output)
{
    require(output.createDirectory().wasOk(), "Cannot create UI report folder");
    EmberfoxProcessor processor; processor.prepareToPlay(48000, 128);
    juce::AudioBuffer<float> oversized(2, 4096); juce::MidiBuffer emptyMidi;
    oversized.clear(); callbackAllocations = 0; trackCallbackAllocations = true;
    processor.processBlock(oversized, emptyMidi);
    processor.processBlockBypassed(oversized, emptyMidi);
    trackCallbackAllocations = false;
    require(callbackAllocations == 0, "Prepared processor callback allocated C++ heap memory");
    std::cout << "PASS processor callbacks: first block, host bypass, oversized 4096 vs prepared 128, zero C++ heap allocations\n";
    oversized.clear(); oversized.setSample(0, 0, 0.5f); oversized.setSample(1, 0, 0.5f);
    processor.processBlockBypassed(oversized, emptyMidi);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < oversized.getNumSamples(); ++i)
            require(std::abs(oversized.getSample(c, i) - (i == processor.getLatencySamples() ? 0.5f : 0.0f)) < 0.00001f,
                    "Native host bypass dry identity/latency failed");
    std::cout << "PASS native host bypass equals delayed unity dry\n";
    require(std::abs(processor.inputMeter.load()-0.5f)<1e-6f&&std::abs(processor.outputMeter.load()-0.5f)<1e-6f,
            "Host bypass meters no longer measure dry input/output");
    set(processor.parameters.getParameter("bypass"),1.0f);
    oversized.clear();oversized.setSample(0,0,1.25f);oversized.setSample(1,0,1.25f);
    processor.processBlock(oversized,emptyMidi);
    require(std::abs(processor.inputMeter.load()-1.25f)<1e-6f&&std::abs(processor.outputMeter.load()-1.25f)<1e-6f,
            "Plugin bypass meters lost an above-zero dry peak");
    set(processor.parameters.getParameter("bypass"),0.0f);
    std::cout << "PASS input/output peak calibration through plugin and host bypass, including above 0 dBFS\n";
    processor.releaseResources();processor.prepareToPlay(48000,128);
    juce::MemoryBlock initial; processor.getStateInformation(initial);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditorIfNeeded());
    require(editor != nullptr, "Native editor missing");
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowIgnoresMouseClicks
                        | juce::ComponentPeer::windowIgnoresKeyPresses);
    editor->setTopLeftPosition(-20000, -20000); editor->setVisible(true); pump(120);
    verifyReleaseControls(*editor,processor,output);
    verifyValueEditing(*editor,processor,output);
    processor.setStateInformation(initial.getData(),int(initial.getSize()));pump();
    const std::array<std::pair<int, int>, 3> sizes{{{980, 640}, {770, 503}, {1470, 960}}};
    for (size_t i = 0; i < sizes.size(); ++i)
    {
        editor->setSize(sizes[i].first, sizes[i].second); pump();
        auto image = editor->createComponentSnapshot(editor->getLocalBounds());
        writePng(output.getChildFile("emberfox-ui-" + juce::String(int(i)) + ".png"), image);
    }
    int sliders = 0, buttons = 0, combos = 0;
    clickControls(*editor, sliders, buttons, combos); pump(120);
    require(sliders == 4 && buttons >= 6, "Not all native controls were exercised");
    require(std::abs(processor.parameters.getRawParameterValue("drive")->load() - 36.0f) < 0.01f, "Drive UI attachment failed");
    require(std::abs(processor.parameters.getRawParameterValue("tone")->load() - 20000.0f) < 0.01f, "Tone UI attachment failed");
    require(std::abs(processor.parameters.getRawParameterValue("mix")->load() - 100.0f) < 0.01f, "Mix UI attachment failed");
    require(std::abs(processor.parameters.getRawParameterValue("output")->load() - 6.0f) < 0.01f, "Output UI attachment failed");
    require(processor.parameters.getRawParameterValue("mode")->load() == 4.0f, "Mode UI attachment failed");
    require(processor.parameters.getRawParameterValue("bypass")->load() == 1.0f, "Bypass UI attachment failed");
    // Restore defaults through host state and ensure attachments update visible controls.
    processor.setStateInformation(initial.getData(), int(initial.getSize())); pump();
    for (auto* child : editor->getChildren())
        if (auto* slider = dynamic_cast<juce::Slider*>(child))
            if (slider->getName() == "drive") require(std::abs(slider->getValue() - 9.0) < 0.01, "Host-to-UI update failed");
    std::cout << "PASS native UI: four knobs, five mode buttons, bypass, automation attachments, state refresh, three sizes\n";
    editor->setSize(980, 640);
    processor.inputMeter.store(0.0f); processor.outputMeter.store(0.0f); pump(650);
    const auto unlit = editor->createComponentSnapshot(editor->getLocalBounds());
    writePng(output.getChildFile("emberfox-calm.png"), unlit);
    processor.outputMeter.store(0.95f); pump(300);
    const auto lit = editor->createComponentSnapshot(editor->getLocalBounds());
    auto meanEyeGreen = [](const juce::Image& frame)
    {
        double sum = 0;
        for (int y = 270; y < 325; ++y)
            for (int x = 680; x < 845; ++x) sum += frame.getPixelAt(x,y).getFloatGreen();
        return sum / (165.0 * 55.0);
    };
    const double outputLightChange = meanEyeGreen(lit) - meanEyeGreen(unlit);
    std::cout << "MEASURE output-only eye brightness change: " << outputLightChange << '\n';
    writePng(output.getChildFile("emberfox-output-only.png"), lit);
    require(outputLightChange > 0.008, "Output energy did not independently brighten the fox eyes");
    processor.outputMeter.store(0.0f); processor.inputMeter.store(0.0f); pump(650);
    const auto quiet = editor->createComponentSnapshot(editor->getLocalBounds());
    processor.inputMeter.store(0.9f); pump(180);
    const auto inputOnly = editor->createComponentSnapshot(editor->getLocalBounds());
    int expressionPixels = 0;
    for(int y=200;y<420;++y)
        for(int x=650;x<875;++x)
            if(quiet.getPixelAt(x,y)!=inputOnly.getPixelAt(x,y)) ++expressionPixels;
    require(expressionPixels>500,"Input energy did not animate the fox expression");
    writePng(output.getChildFile("emberfox-input-only.png"),inputOnly);
    std::cout << "PASS independent input expression and output eye light (" << expressionPixels << " input pixels)\n";
    set(processor.parameters.getParameter("drive"), 0.9f); pump(60);
    auto beforeSignal = editor->createComponentSnapshot(editor->getLocalBounds());
    processor.inputMeter.store(0.75f); processor.outputMeter.store(0.8f); pump(240);
    auto image = editor->createComponentSnapshot(editor->getLocalBounds());
    int changedPixels = 0;
    for (int y = 150; y < 490; ++y)
        for (int x = 580; x < 930; ++x)
            if (beforeSignal.getPixelAt(x, y) != image.getPixelAt(x, y)) ++changedPixels;
    require(changedPixels > 100, "Signal-reactive artwork/meters did not animate");
    writePng(output.getChildFile("emberfox-ui-active.png"), image);
    set(processor.parameters.getParameter("bypass"),1.0f); pump(900);
    writePng(output.getChildFile("emberfox-bypassed.png"),editor->createComponentSnapshot(editor->getLocalBounds()));
    set(processor.parameters.getParameter("bypass"),0.0f);
    auto* nativeEditor=dynamic_cast<EmberfoxEditor*>(editor.get());require(nativeEditor!=nullptr,"Native renderer missing");
    for(int intensity=0;intensity<2;++intensity)
    {
    const juce::String suffix=intensity==0?"":"-driven";
    processor.inputMeter.store(intensity==0?0.25f:1.4f);processor.outputMeter.store(intensity==0?0.65f:1.2f);
    juce::Image comparison(juce::Image::ARGB,1300,310,true);
    std::array<juce::Image,5> modeThumbnails;
    {
    juce::Graphics comparisonGraphics(comparison);
    comparisonGraphics.fillAll(juce::Colour(0xff1a1e20));
    for(int modeIndex=0;modeIndex<5;++modeIndex)
    {
        set(processor.parameters.getParameter("mode"),float(modeIndex)/4.0f);pump(25);
        EmberfoxEditorTestAccess::settlePortrait(*nativeEditor);
        auto modeImage=editor->createComponentSnapshot(editor->getLocalBounds());
        int eyeUnion=0,eyeMismatch=0;
        auto eyePixel=[](juce::Colour colour){return int(colour.getGreen())>int(colour.getRed())+8&&colour.getGreen()>80;};
        for(int y=250;y<335;++y)for(int x=675;x<760;++x)
        {
            const bool left=eyePixel(modeImage.getPixelAt(x,y)),right=eyePixel(modeImage.getPixelAt(1519-x,y));
            if(left||right)++eyeUnion;if(left!=right)++eyeMismatch;
        }
        const double mirrorError=double(eyeMismatch)/std::max(1,eyeUnion);
        std::cout<<"MEASURE eye mirror error "<<modeIndex<<"/"<<intensity<<": "<<mirrorError<<'\n';
        require(eyeUnion>100&&mirrorError<.12,"Mode eyes are not mirrored around the face centre");
        modeThumbnails[size_t(modeIndex)]=modeImage.getClippedImage({630,170,260,260}).rescaled(96,96);
        writePng(output.getChildFile("emberfox-mode-"+EmberfoxProcessor::modeNames[modeIndex].toLowerCase()+suffix+".png"),modeImage);
        comparisonGraphics.setColour(juce::Colour(0xfff2eadb));
        comparisonGraphics.setFont(juce::Font(juce::FontOptions(17.0f)));
        comparisonGraphics.drawText(EmberfoxProcessor::modeNames[modeIndex],modeIndex*260,0,260,26,juce::Justification::centred);
        comparisonGraphics.drawImageWithin(modeImage.getClippedImage({605,160,310,280}),modeIndex*260,30,260,250,juce::RectanglePlacement::centred,false);
    }
    }
    require(comparison.getPixelAt(130,150)!=juce::Colour(0xff1a1e20),"Mode comparison render is blank");
    writePng(output.getChildFile("emberfox-modes"+suffix+".png"),comparison);
    double minimumModeDifference=1.0;
    for(size_t a=0;a<5;++a)for(size_t b=a+1;b<5;++b)
    {
        double difference=0;
        for(int y=0;y<96;++y)for(int x=0;x<96;++x)
        {
            const auto first=modeThumbnails[a].getPixelAt(x,y),second=modeThumbnails[b].getPixelAt(x,y);
            difference+=std::abs(first.getFloatRed()-second.getFloatRed())
                       +std::abs(first.getFloatGreen()-second.getFloatGreen())
                       +std::abs(first.getFloatBlue()-second.getFloatBlue());
        }
        difference/=(96.0*96.0*3.0);minimumModeDifference=std::min(minimumModeDifference,difference);
        std::cout<<"MEASURE thumbnail difference "<<(intensity==0?"steady ":"driven ")<<a<<"/"<<b<<": "<<difference<<'\n';
    }
    require(minimumModeDifference>.012,"Mode personalities are too similar at thumbnail size");
    }
    EmberfoxEditorTestAccess::resume(*nativeEditor);
    processor.inputMeter.store(1.4f);processor.outputMeter.store(1.2f);pump(80);
    writePng(output.getChildFile("emberfox-over-zero.png"),editor->createComponentSnapshot(editor->getLocalBounds()));
    EmberfoxEditorTestAccess::worstTransient(*nativeEditor);editor->setSize(770,503);
    writePng(output.getChildFile("emberfox-transient-min.png"),editor->createComponentSnapshot(editor->getLocalBounds()));
    set(processor.parameters.getParameter("mode"),.75f);pump(25);
    EmberfoxEditorTestAccess::settlePortrait(*nativeEditor);EmberfoxEditorTestAccess::worstTransient(*nativeEditor);
    writePng(output.getChildFile("emberfox-transient-fold-min.png"),editor->createComponentSnapshot(editor->getLocalBounds()));
    std::cout<<"PASS all five mode portraits and over-zero meter preview\n";
    editor->setVisible(false); editor.reset(); processor.releaseResources();
    std::cout << "PASS message-thread animation and meter response (" << changedPixels << " changed pixels)\nNATIVE UI PASSED\n";
}
}

int main(int argc, char** argv)
{
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        require(argc >= 2, "Usage: EmberfoxHostSmoke <plugin.vst3> [output-folder | --inspect]");
        if (juce::String::fromUTF8(argv[1]) == "--internal-ui")
        {
            require(argc >= 3, "Native UI requires an output folder");
            nativeUi(juce::File(juce::String::fromUTF8(argv[2]))); return 0;
        }
        const juce::File pluginFile(juce::String::fromUTF8(argv[1]));
        require(pluginFile.exists(), "Built VST3 is missing: " + pluginFile.getFullPathName());
        juce::AudioPluginFormatManager manager;
        manager.addFormat(std::make_unique<juce::VST3PluginFormat>());
        juce::OwnedArray<juce::PluginDescription> types;
        manager.getFormat(0)->findAllTypesForFile(types, pluginFile.getFullPathName());
        require(!types.isEmpty(), "VST3 discovery found no effect");
        juce::String error;
        auto plugin = manager.createPluginInstance(*types[0], 48000.0, 256, error);
        require(plugin != nullptr, "VST3 load failed: " + error);
        inspect(*plugin, *types[0]);
        if (argc >= 3 && juce::String::fromUTF8(argv[2]) == "--inspect")
        {
            std::cout << "PASS reference loaded and parameters inspected; no reference audio measurements performed\n";
            return 0;
        }
        juce::File output = argc >= 3 ? juce::File(juce::String::fromUTF8(argv[2])) :
            juce::File::getCurrentWorkingDirectory().getChildFile("docs/validation");
        require(output.createDirectory().wasOk(), "Cannot create output directory");
        auto* drive = parameter(*plugin, "Drive");
        auto* tone = parameter(*plugin, "Tone");
        auto* mix = parameter(*plugin, "Mix");
        auto* gain = parameter(*plugin, "Output");
        auto* mode = parameter(*plugin, "Mode");
        auto* bypass = parameter(*plugin, "Bypass");
        std::array<juce::AudioProcessorParameter*, 6> controls{drive, tone, mix, gain, mode, bypass};
        std::array<float, 6> expected{0.67f, 0.44f, 0.78f, 0.50f, 0.75f, 0.0f};
        for (size_t i = 0; i < controls.size(); ++i) set(controls[i], expected[i]);
        pump();
        for (size_t i = 0; i < controls.size(); ++i) expected[i] = controls[i]->getValue();
        juce::MemoryBlock state; plugin->getStateInformation(state);
        require(state.getSize() > 20, "Empty plugin state");
        for (auto* p : controls) set(p, 0.0f);
        plugin->setStateInformation(state.getData(), int(state.getSize())); pump();
        for (size_t i = 0; i < controls.size(); ++i)
            require(std::abs(controls[i]->getValue() - expected[i]) < 0.001f, "Parameter state round trip failed");
        std::cout << "PASS automation and complete parameter state round trip\n";
        // Malformed state must leave valid parameters intact.
        plugin->setStateInformation("invalid", 7); pump();
        require(std::abs(drive->getValue() - expected[0]) < 0.001f, "Malformed state damaged parameters");
        std::cout << "PASS malformed state rejected\n";

        for (double rate : {44100.0, 48000.0, 96000.0, 192000.0})
            for (int channels : {1, 2})
            {
                std::cout << "Processing layout " << rate << " Hz / " << channels << " channels\n";
                plugin->releaseResources();
                auto layout = plugin->getBusesLayout();
                layout.inputBuses.getReference(0) = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
                layout.outputBuses.getReference(0) = layout.inputBuses[0];
                require(plugin->setBusesLayout(layout), "Mono/stereo layout rejected");
                // VST3 setupProcessing specifies a hard maximum, unlike JUCE's block-size hint.
                plugin->setRateAndBufferSizeDetails(rate, 4096); plugin->prepareToPlay(rate, 4096);
                require(plugin->getLatencySamples() > 0, "VST3 failed to report oversampling latency");
                for (int size : {0, 1, 17, 64, 511, 4096})
                {
                    std::cout << "  block " << size << '\n';
                    juce::AudioBuffer<float> buffer(channels, size); buffer.clear(); juce::MidiBuffer midi;
                    for (int c = 0; c < channels; ++c)
                        for (int i = 0; i < size; ++i)
                            buffer.setSample(c, i, 0.2f * std::sin(float(i * 0.1)));
                    plugin->processBlock(buffer, midi);
                    for (int c = 0; c < channels; ++c)
                        for (int i = 0; i < size; ++i)
                            require(std::isfinite(buffer.getSample(c, i)), "VST3 emitted nonfinite samples");
                }
            }
        std::cout << "PASS VST3 processing: 4 sample rates, mono/stereo, blocks 0..4096 within announced maximum, release/reprepare\n";

        // Both plugin automation bypass and host bypass must retain reported latency.
        for (bool hostBypass : {false, true})
        {
            set(bypass, hostBypass ? 0.0f : 1.0f); plugin->releaseResources();
            plugin->setRateAndBufferSizeDetails(48000, 1024); plugin->prepareToPlay(48000, 1024);
            juce::AudioBuffer<float> buffer(2, 1024); buffer.clear();
            juce::MidiBuffer midi;
            // Allow the intentional bypass crossfade to settle before testing exact dry identity.
            for (int warmup = 0; warmup < 4; ++warmup)
            {
                buffer.clear();
                if (hostBypass) plugin->processBlockBypassed(buffer, midi); else plugin->processBlock(buffer, midi);
            }
            buffer.clear();
            buffer.setSample(0, 0, 0.5f); buffer.setSample(1, 0, 0.5f);
            if (hostBypass) plugin->processBlockBypassed(buffer, midi); else plugin->processBlock(buffer, midi);
            const int latency = plugin->getLatencySamples();
            for (int i = 0; i < 1024; ++i)
                require(std::abs(buffer.getSample(0, i) - (i == latency ? 0.5f : 0.0f)) < 0.00001f,
                        "VST3 bypass latency mismatch");
        }
        std::cout << "PASS plugin and host bypass latency\n";

        for (int cycle = 0; cycle < 3; ++cycle)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorIfNeeded());
            require(editor != nullptr, "Plugin has no editor");
            editor->setVisible(true); pump(120);
            const std::array<std::pair<int, int>, 3> sizes{{{980, 640}, {770, 503}, {1470, 960}}};
            for (size_t index = 0; index < sizes.size(); ++index)
            {
                editor->setSize(sizes[index].first, sizes[index].second); pump();
                require(editor->getWidth() >= 760 && editor->getHeight() >= 490, "Editor resize failed");
            }
            int sliders = 0, buttons = 0, combos = 0;
            clickControls(*editor, sliders, buttons, combos); pump(120);
            // VST3 editors are wrapped as native child windows, so discovery may be opaque to this host.
            std::cout << "Editor cycle " << cycle << " observed JUCE controls: " << sliders << '/' << buttons << '/' << combos << '\n';
            editor->setVisible(false); editor.reset(); pump();
            plugin->setStateInformation(state.getData(), int(state.getSize())); pump();
        }
        std::cout << "PASS repeated VST3 editor creation, resize, destruction and state restoration\n";

        set(bypass, 0.0f); set(drive, 0.67f); set(mix, 1.0f); set(gain, 0.6f); set(tone, 0.8f);
        const int total = 96000, blockSize = 256;
        juce::AudioBuffer<float> source(2, total), rendered(2, total), block(2, blockSize);
        for (int i = 0; i < total; ++i)
        {
            const double t = i / 48000.0;
            const float x = 0.23f * float(std::sin(2.0 * juce::MathConstants<double>::pi * 110.0 * t)
                + 0.45 * std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * t));
            source.setSample(0, i, x); source.setSample(1, i, x);
        }
        writeWav(output.getChildFile("demo-dry.wav"), source);
        for (int m = 0; m < 5; ++m)
        {
            set(mode, float(m) / 4.0f); plugin->releaseResources(); plugin->prepareToPlay(48000, blockSize);
            for (int position = 0; position < total; position += blockSize)
            {
                const int count = std::min(blockSize, total - position);
                block.setSize(2, count, false, false, true);
                for (int c = 0; c < 2; ++c) block.copyFrom(c, 0, source, c, position, count);
                juce::MidiBuffer midi; plugin->processBlock(block, midi);
                for (int c = 0; c < 2; ++c) rendered.copyFrom(c, position, block, c, 0, count);
            }
            require(rendered.getMagnitude(0, total) > 0.01f, "Processed demo is silent");
            writeWav(output.getChildFile("demo-mode-" + juce::String(m + 1) + ".wav"), rendered);
        }
        plugin->releaseResources();
        std::cout << "PASS five processed demo WAVs plus dry source\nHOST SMOKE PASSED\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "HOST SMOKE FAILED: " << error.what() << '\n'; return 1;
    }
}
