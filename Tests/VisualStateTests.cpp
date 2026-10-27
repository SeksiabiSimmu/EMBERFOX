#include "VisualState.h"
#include <iostream>
#include <stdexcept>
#include <limits>

namespace
{
int passed=0, failed=0;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<typename F> void test(const char* name,F run){try{run();++passed;std::cout<<"PASS "<<name<<'\n';}catch(const std::exception& e){++failed;std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n';}}
void advance(emberfox::PeakHold& p,double duration,int hz){const int n=int(std::round(duration*hz));for(int i=0;i<n;++i)p.update(0,1.0/hz);}
}
int main()
{
    test("0 dBFS calibrated on -60 to +6 scale",[]{
        require(std::abs(emberfox::PeakHold::position(0)-60.0f/66.0f)<1e-6f,"zero reference miscalibrated");
        require(emberfox::PeakHold::position(-60)==0&&emberfox::PeakHold::position(6)==1,"scale endpoints wrong");
        require(emberfox::PeakHold::position(30)==1&&emberfox::PeakHold::position(-100)==0,"marker not clamped");
    });
    test("peak holds for one second then falls at 18 dB/s",[]{
        emberfox::PeakHold p;p.update(1.0f,0);require(std::abs(p.db)<1e-5f,"peak was not captured");
        advance(p,0.9,60);require(std::abs(p.db)<1e-5f,"peak fell before one second");
        advance(p,0.6,60);require(std::abs(p.db+9)<0.01f,"release time or speed incorrect");
    });
    test("peak timing agrees across frame rates and a stalled UI",[]{
        emberfox::PeakHold a,b,c;a.update(1,0);b.update(1,0);c.update(1,0);
        advance(a,1.5,30);advance(b,1.5,120);c.update(0,1.5);
        require(std::abs(a.db+9)<.01f&&std::abs(a.db-b.db)<.01f&&std::abs(a.db-c.db)<.01f,"peak timing depends on update rate");
    });
    test("new peaks reset hold, live level floors release, meters independent",[]{
        emberfox::PeakHold in,out;in.update(1,0);out.update(0.5f,0);advance(in,1.4,60);
        require(out.db<-5.9f&&out.db>-6.1f,"meter states are not independent");
        in.update(0.8f,0);const float captured=in.db;advance(in,.8,60);require(std::abs(in.db-captured)<.01f,"new peak did not reset hold");
        for(int i=0;i<240;++i)in.update(0.4f,1.0/60.0);
        require(in.db>=20*std::log10(.4f)-.01f,"held marker fell below live level");
    });
    test("peak silence, above-ceiling and invalid values are safe",[]{
        emberfox::PeakHold p;p.update(2,0);require(p.db>6,"above-ceiling peak was lost");
        p.update(std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<double>::infinity());
        require(std::isfinite(p.db),"invalid input poisoned peak state");p.update(0,10);require(p.db==-60,"silence did not reach floor");
    });
    test("five personalities have distinct symmetric base poses",[]{
        emberfox::ModePersonality states[5];for(int mode=0;mode<5;++mode)for(int i=0;i<120;++i)states[mode].update(mode,false,1.0f/60);
        for(int a=0;a<5;++a)for(int b=a+1;b<5;++b){const auto& x=states[a].pose;const auto& y=states[b].pose;
            const float d=std::abs(x.openness-y.openness)+std::abs(x.focus-y.focus)+std::abs(x.tension-y.tension)
                +std::abs(x.earSpread-y.earSpread)+std::abs(x.earDrop-y.earDrop)+std::abs(x.mouthWidth-y.mouthWidth)*.1f
                +std::abs(x.eyeWidth-y.eyeWidth)*.1f;
            require(d>.8f,"two personalities indistinguishable");}
        require(states[0].pose.openness>states[2].pose.openness,"Velvet should be more relaxed than Razor");
        require(states[3].pose.mouthWidth>23,"Fold broad smile missing");
    });
    test("mode transition interpolates at early times and agrees across rates",[]{
        emberfox::ModePersonality a,b;a.update(2,false,1.0f/60);b.update(2,false,1.0f/120);b.update(2,false,1.0f/120);
        require(a.pose.focus>0.01f&&a.pose.focus<0.4f,"mode transition jumped or stayed still");
        require(std::abs(a.pose.focus-b.pose.focus)<1e-5f,"pose smoothing depends on frame rate");
    });
    test("Velvet has clearly sleepy lids at rest",[]{
        emberfox::ModePersonality p;for(int i=0;i<120;++i)p.update(0,false,1.0f/60);
        require(p.pose.openness<-.35f&&p.pose.tension<-.2f,"Velvet still reads as the neutral face");
    });
    test("Crunch has clearly open confident eyes",[]{
        emberfox::ModePersonality p;for(int i=0;i<120;++i)p.update(1,false,1.0f/60);
        require(p.pose.openness>.55f,"Crunch does not have a distinct alert aperture");
    });
    test("Razor combines narrow lids with a strong focused brow",[]{
        emberfox::ModePersonality p;for(int i=0;i<120;++i)p.update(2,false,1.0f/60);
        require(p.pose.openness<-.6f&&p.pose.focus>.8f,"Razor is not visually focused enough");
    });
    test("Fold has wide playful eyes and a broad centered smile",[]{
        emberfox::ModePersonality p;for(int i=0;i<120;++i)p.update(3,false,1.0f/60);
        require(p.pose.openness>1.8f&&p.pose.focus<0&&p.pose.tension<-.6f&&p.pose.mouthWidth>25&&p.pose.eyeWidth<31,"Fold still shares the generic expression");
    });
    test("Fuzz has a tense frown distinct from Razor",[]{
        emberfox::ModePersonality p;for(int i=0;i<120;++i)p.update(4,false,1.0f/60);
        require(p.pose.tension>1.25f&&p.pose.openness>.2f,"Fuzz still shares the generic squint");
    });
    test("bypass relaxes personalities and invalid modes remain bounded",[]{
        emberfox::ModePersonality p;for(int i=0;i<120;++i)p.update(4,false,1.0f/60);
        require(p.pose.tension>.2f&&p.pose.earSpread>.9f,"Fuzz tension or flared ears missing");
        require(p.pose.earDrop<0&&p.pose.mouthWidth<19,"Fuzz posture incomplete");
        for(int i=0;i<180;++i)p.update(4,true,1.0f/60);
        require(std::abs(p.pose.tension)<.001f&&std::abs(p.pose.focus)<.001f,"bypass did not relax pose");
        require(std::abs(p.pose.openness)<.001f
                &&std::abs(p.pose.earSpread)<.001f&&std::abs(p.pose.earDrop)<.001f
                &&std::abs(p.pose.mouthWidth-20)<.001f&&std::abs(p.pose.eyeWidth-42)<.001f,"bypass retained a mode posture");
        p.update(99,false,std::numeric_limits<float>::quiet_NaN());require(std::isfinite(p.pose.focus),"invalid state poisoned pose");
        for(int i=0;i<120;++i)p.update(99,false,1.0f/60);
        require(p.pose.tension>1.4f&&p.pose.earSpread>1.1f,"out-of-range mode did not safely clamp to Fuzz");
    });
    std::cout<<passed<<" passed, "<<failed<<" failed\n";return failed?1:0;
}
