#pragma once
#include <algorithm>
#include <cmath>

namespace emberfox
{
class PeakHold
{
public:
    static float position(float value) noexcept
    {
        return std::isfinite(value) ? std::clamp((value+60.0f)/66.0f,0.0f,1.0f) : 0.0f;
    }
    void update(float peak, double seconds) noexcept
    {
        const float live=std::isfinite(peak)&&peak>0.0f ? std::max(-60.0f,20.0f*std::log10(peak)) : -60.0f;
        const double dt=std::isfinite(seconds)?std::clamp(seconds,0.0,3600.0):0.0;
        if(live>=db)
        {
            db=live; remaining=1.0;
        }
        else
        {
            const double held=std::min(remaining,dt);
            remaining-=held;
            db=std::max(live,db-float((dt-held)*18.0));
        }
    }
    float db = -60.0f;
private:
    double remaining=0.0;
};
struct ModePose
{
    float openness = 0.0f, focus = 0.0f, tension = 0.0f;
    float earSpread = 0.0f, earDrop = 0.0f, mouthWidth = 20.0f, eyeWidth = 42.0f;
};
class ModePersonality
{
public:
    void update(int mode, bool bypass, float seconds) noexcept
    {
        if(!std::isfinite(seconds)||seconds<=0.0f)return;
        // One shared rig, with readable poses even before any input arrives.
        //                      eyes    brow   mouth   ear X  ear Y  mouth half-width / eye width
        static constexpr ModePose modes[]{
            {-0.55f,-0.45f,-0.35f, 0.25f, 0.70f,18.0f,44.0f}, // Velvet: content
            { 0.75f, 0.10f, 0.10f, 0.00f,-0.25f,20.0f,46.0f}, // Crunch: confident
            {-0.78f, 0.95f, 0.85f,-0.45f, 0.30f,15.0f,42.0f}, // Razor: focused
            { 2.00f,-0.25f,-0.80f, 0.10f,-0.40f,26.0f,30.0f}, // Fold: playful
            { 0.50f, 1.15f, 1.45f, 1.15f,-0.10f,18.0f,44.0f}};// Fuzz: bristling
        const auto target=bypass?ModePose{}:modes[std::clamp(mode,0,4)];
        const float blend=1.0f-std::exp(-std::min(seconds,0.1f)/0.14f);
        pose.openness+=(target.openness-pose.openness)*blend;
        pose.focus+=(target.focus-pose.focus)*blend;
        pose.tension+=(target.tension-pose.tension)*blend;
        pose.earSpread+=(target.earSpread-pose.earSpread)*blend;
        pose.earDrop+=(target.earDrop-pose.earDrop)*blend;
        pose.mouthWidth+=(target.mouthWidth-pose.mouthWidth)*blend;
        pose.eyeWidth+=(target.eyeWidth-pose.eyeWidth)*blend;
    }
    ModePose pose;
};
}
