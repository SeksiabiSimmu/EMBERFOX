#include "FoxAnimation.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
int passed = 0, failed = 0;
void require(bool condition, const char* reason) { if (!condition) throw std::runtime_error(reason); }
template<typename Fn> void test(const char* name, Fn run)
{
    try { run(); ++passed; std::cout << "PASS " << name << '\n'; }
    catch (const std::exception& error) { ++failed; std::cout << "FAIL " << name << ": " << error.what() << '\n'; }
}
void settle(emberfox::FoxAnimation& state, float input, float output, float drive = 0.5f, bool bypass = false, int frames = 90)
{
    for (int i = 0; i < frames; ++i) state.update(input, output, drive, bypass, 1.0f / 60.0f);
}
}
int main()
{
    test("input affects expression independently of output", []
    {
        emberfox::FoxAnimation state; settle(state, 0.8f, 0);
        require(state.inputEnergy > 0.8f && state.expression > 0.6f, "input did not energize the expression");
        require(state.outputEnergy == 0, "input incorrectly lit the output");
    });
    test("output affects light independently of input", []
    {
        emberfox::FoxAnimation state; settle(state, 0, 0.8f);
        require(state.outputEnergy > 0.8f, "output did not light the fox");
        require(state.inputEnergy == 0 && state.expression == 0 && state.transient == 0, "output incorrectly drove input motion");
    });
    test("a rising input transient triggers once and settles on a held peak", []
    {
        emberfox::FoxAnimation state; state.update(0.9f, 0, 1, false, 1.0f/60.0f);
        require(state.transient > 0.3f, "attack produced no transient");
        settle(state, 0.9f, 0, 1, false, 60);
        require(state.transient < 0.001f, "held peak retriggered the transient");
    });
    test("smoothing agrees at 30, 60 and 120 updates per second", []
    {
        emberfox::FoxAnimation states[3];
        int rates[] = {30,60,120};
        for(int n=0;n<3;++n)
            for(int i=0;i<rates[n];++i) states[n].update(0.6f,0.4f,0.8f,false,1.0f/float(rates[n]));
        require(states[0].inputEnergy > 0.7f, "smoothing never reached its target");
        for(int n=1;n<3;++n)
        {
            require(std::abs(states[n].inputEnergy-states[0].inputEnergy)<0.001f,"input depends on frame rate");
            require(std::abs(states[n].outputEnergy-states[0].outputEnergy)<0.001f,"output depends on frame rate");
        }
    });
    test("silence and bypass relax the fox even with drive raised", []
    {
        emberfox::FoxAnimation state; settle(state,0.9f,0.9f,1);
        require(state.expression > 0.8f, "initial drive state missing");
        settle(state,0.9f,0.9f,1,true,180);
        require(state.expression<0.001f && state.outputEnergy<0.001f && state.transient<0.001f,"bypass did not settle");
        settle(state,0,0,1,false,180);
        require(state.expression<0.001f,"silent drive should not keep an aggressive expression");
    });
    test("invalid readings and clock deltas remain finite and bounded", []
    {
        emberfox::FoxAnimation state;
        for(float dt : {0.0f,-1.0f,1000.0f,std::numeric_limits<float>::quiet_NaN()})
            state.update(std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),99,false,dt);
        for(int i=0;i<100000;++i) state.update(0.2f,0.4f,0.7f,false,0.1f);
        require(std::isfinite(state.phase) && state.phase>=0 && state.phase<6,"phase not bounded");
        require(std::isfinite(state.inputEnergy) && state.inputEnergy>=0 && state.inputEnergy<=1,"input not bounded");
        require(std::isfinite(state.outputEnergy) && state.outputEnergy>=0 && state.outputEnergy<=1,"output not bounded");
        require(state.eyeOpen>=0 && state.eyeOpen<=1,"blink not bounded");
    });
    test("idle blink briefly closes then reopens", []
    {
        emberfox::FoxAnimation state; float smallest=1;
        for(int i=0;i<360;++i) { state.update(0,0,0,false,1.0f/60.0f); smallest=std::min(smallest,state.eyeOpen); }
        require(smallest<0.1f && state.eyeOpen>0.99f,"blink cycle missing or stuck");
    });
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
