#pragma once
#include <array>

namespace emberfox
{
struct FactoryPreset
{
    const char* name;
    float drive, tone, mix, output;
    int mode;
};
inline constexpr std::array<FactoryPreset,8> factoryPresets{{
    {"Default / Velvet",9,12000,100,-6,0},
    {"Velvet / Warmth",6,9000,45,-3,0},
    {"Crunch / Punch",16,10000,65,-8,1},
    {"Razor / Edge",24,14000,40,-12,2},
    {"Fold / Motion",18,10000,55,-10,3},
    {"Fuzz / Growl",22,6500,60,-10,4},
    {"Crunch / Parallel Bite",27,11000,25,-12,1},
    {"Fuzz / Dark Heat",20,2400,70,-10,4}
}};
}
