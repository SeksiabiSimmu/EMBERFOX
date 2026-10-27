#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>
#include <set>

namespace
{
int passed=0,failed=0;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class F>void test(const char* name,F run){try{run();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failed;std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';}}
std::array<float,6> values(EmberfoxProcessor& p)
{
    return {p.parameters.getRawParameterValue("drive")->load(),p.parameters.getRawParameterValue("tone")->load(),
            p.parameters.getRawParameterValue("mix")->load(),p.parameters.getRawParameterValue("output")->load(),
            p.parameters.getRawParameterValue("mode")->load(),p.parameters.getRawParameterValue("bypass")->load()};
}
}
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    test("factory catalog covers all five modes with unique useful names",[]{
        require(emberfox::factoryPresets.size()>=8,"Factory preset collection is missing");
        std::set<std::string> names;std::set<int> modes;
        for(const auto& preset:emberfox::factoryPresets){
            require(preset.name!=nullptr&&std::string(preset.name).size()>4,"Preset name is not descriptive");
            require(names.insert(preset.name).second,"Duplicate preset name");modes.insert(preset.mode);
            require(preset.drive>=0&&preset.drive<=36&&preset.tone>=400&&preset.tone<=20000
                &&preset.mix>=0&&preset.mix<=100&&preset.output>=-24&&preset.output<=6,"Preset outside parameter range");
        }
        require(modes.size()==5,"Not every distortion mode has an example");
    });
    test("factory default restores existing startup values and edits become Custom",[]{
        EmberfoxProcessor p;require(p.matchingFactoryPreset()==0,"Startup default changed or was not recognized");
        p.parameters.getParameter("drive")->setValueNotifyingHost(.9f);
        require(p.matchingFactoryPreset()==-1,"Modified settings still show a factory name");
        p.applyFactoryPreset(0);require(values(p)==std::array<float,6>{9,12000,100,-6,0,0},"Default preset changed original settings");
    });
    test("every preset survives complete state restoration and invalid selection is harmless",[]{
        require(!emberfox::factoryPresets.empty(),"No presets to restore");
        EmberfoxProcessor p;
        for(int i=0;i<int(emberfox::factoryPresets.size());++i){
            p.applyFactoryPreset(i);require(p.matchingFactoryPreset()==i,"Applied preset not recognized");
            const auto expected=values(p);juce::MemoryBlock state;p.getStateInformation(state);
            p.parameters.getParameter("mix")->setValueNotifyingHost(.123f);
            p.setStateInformation(state.getData(),int(state.getSize()));
            require(values(p)==expected&&p.matchingFactoryPreset()==i,"Preset state round trip failed");
        }
        const auto expected=values(p);p.applyFactoryPreset(-1);p.applyFactoryPreset(999);
        require(values(p)==expected,"Invalid preset altered settings");
        require(p.getNumPrograms()==1,"Factory menu changed host program semantics");
    });
    test("legacy parameter-only state stays compatible",[]{
        EmberfoxProcessor p;
        const juce::String legacy="<EMBERFOX_STATE><PARAM id=\"drive\" value=\"17.25\"/><PARAM id=\"tone\" value=\"4567\"/><PARAM id=\"mix\" value=\"32.1\"/><PARAM id=\"output\" value=\"-8.75\"/><PARAM id=\"mode\" value=\"3\"/><PARAM id=\"bypass\" value=\"1\"/></EMBERFOX_STATE>";
        auto xml=juce::parseXML(legacy);juce::MemoryBlock state;juce::AudioProcessor::copyXmlToBinary(*xml,state);
        p.setStateInformation(state.getData(),int(state.getSize()));
        const auto actual=values(p);const std::array<float,6> expected{17.25f,4567,32.1f,-8.75f,3,1};
        for(size_t i=0;i<actual.size();++i)require(std::abs(actual[i]-expected[i])<.001f,"Legacy state parameter changed");
        require(p.matchingFactoryPreset()==-1,"Legacy custom state got a false preset name");
    });
    std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
