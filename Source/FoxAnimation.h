#pragma once
#include <algorithm>
#include <cmath>

namespace emberfox
{
class FoxAnimation
{
public:
    void update(float inputPeak, float outputPeak, float drive, bool bypass, float seconds) noexcept
    {
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.0f, 0.1f) : 0.0f;
        if (dt <= 0.0f) return;
        auto normalized = [](float value)
        {
            return std::isfinite(value) ? std::sqrt(std::clamp(value, 0.0f, 1.0f)) : 0.0f;
        };
        const float inputTarget = bypass ? 0.0f : normalized(inputPeak);
        const float outputTarget = bypass ? 0.0f : normalized(outputPeak);
        const auto smooth = [dt](float current, float target, float attack, float release)
        {
            const float tau = target > current ? attack : release;
            return target + (current - target) * std::exp(-dt / tau);
        };
        inputEnergy = smooth(inputEnergy, inputTarget, 0.025f, 0.15f);
        outputEnergy = smooth(outputEnergy, outputTarget, 0.035f, 0.22f);
        const float driveTarget = std::isfinite(drive) ? std::clamp(drive, 0.0f, 1.0f) : 0.0f;
        driveAmount = smooth(driveAmount, driveTarget, 0.08f, 0.08f);
        expression = inputEnergy * (0.72f + 0.28f * driveAmount);

        transient *= std::exp(-dt / 0.075f);
        cooldown = std::max(0.0f, cooldown - dt);
        const float rise = inputTarget - previousInput;
        if (!bypass && cooldown <= 0.0f && rise > 0.12f)
        {
            transient = std::max(transient, std::min(1.0f, rise * 1.8f));
            cooldown = 0.12f;
        }
        previousInput = inputTarget;
        // Bounded phase prevents precision loss in long DAW sessions.
        phase = std::fmod(phase + dt, 5.8f);
        const float blink = (phase - 5.25f) / 0.18f;
        eyeOpen = blink > 0.0f && blink < 1.0f
            ? std::clamp(1.0f - std::sin(blink * 3.14159265359f), 0.0f, 1.0f) : 1.0f;
    }
    float inputEnergy = 0.0f;
    float outputEnergy = 0.0f;
    float expression = 0.0f;
    float transient = 0.0f;
    float phase = 0.0f;
    float eyeOpen = 1.0f;

private:
    float driveAmount = 0.0f;
    float previousInput = 0.0f;
    float cooldown = 0.0f;
};
}
