#include "DistortionEngine.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
thread_local bool trackAllocations = false;
thread_local int allocations = 0;
int passed = 0, failed = 0;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<typename F> void test(const std::string& name, F f)
{
    try { f(); ++passed; std::cout << "PASS " << name << '\n'; }
    catch (const std::exception& e) { ++failed; std::cout << "FAIL " << name << ": " << e.what() << '\n'; }
}
std::vector<float> render(emberfox::Settings settings, double rate, int channels, int block,
                          int total, bool rightSilent = false)
{
    emberfox::DistortionEngine engine;
    engine.prepare(rate, 128, channels, settings);
    std::vector<float> result(static_cast<size_t>(total * channels));
    juce::AudioBuffer<float> buffer(channels, block);
    for (int position = 0; position < total; position += block)
    {
        const int count = std::min(block, total - position);
        buffer.setSize(channels, count, false, false, true);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < count; ++i)
                buffer.setSample(c, i, rightSilent && c == 1 ? 0.0f :
                    0.4f * std::sin(float(2.0 * juce::MathConstants<double>::pi * 997.0 * (position + i) / rate)));
        engine.process(buffer);
        for (int c = 0; c < channels; ++c)
            std::copy_n(buffer.getReadPointer(c), count, result.data() + c * total + position);
    }
    return result;
}
}

void* operator new(std::size_t n)
{
    if (trackAllocations) ++allocations;
    if (void* p = std::malloc(n == 0 ? 1 : n)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

int main()
{
    test("oversampling reports nonzero integer latency", []
    {
        emberfox::DistortionEngine e; e.prepare(48000, 128, 2);
        require(e.latency() > 0 && e.latency() < 512, "expected bounded oversampling latency");
        std::cout << "MEASURE compensated latency: " << e.latency() << " samples\n";
    });
    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0})
        for (int channels : {1, 2})
            test("silence/range " + std::to_string(int(rate)) + "/" + std::to_string(channels), [=]
            {
                for (int mode = 0; mode < 5; ++mode)
                {
                    emberfox::Settings s; s.mode = mode; s.driveDb = 36; s.outputDb = 6;
                    emberfox::DistortionEngine e; e.prepare(rate, 64, channels, s);
                    juce::AudioBuffer<float> b(channels, 4096); b.clear(); e.process(b);
                    require(b.getMagnitude(0, b.getNumSamples()) < 1.0e-7f, "silence generated audio");
                    for (int c = 0; c < channels; ++c)
                        for (int i = 0; i < b.getNumSamples(); ++i)
                            b.setSample(c, i, (i % 2 ? 1000.0f : -1000.0f));
                    e.process(b);
                    for (int c = 0; c < channels; ++c)
                        for (int i = 0; i < b.getNumSamples(); ++i)
                            require(std::isfinite(b.getSample(c, i)) && std::abs(b.getSample(c, i)) < 5.0f,
                                    "saturation must produce bounded finite audio");
                }
            });
    test("five nonlinear modes have distinct outputs", []
    {
        std::array<std::vector<float>, 5> outputs;
        emberfox::Settings s; s.driveDb = 24;
        for (int m = 0; m < 5; ++m) { s.mode = m; outputs[size_t(m)] = render(s, 48000, 1, 127, 8192); }
        for (int a = 0; a < 5; ++a)
            for (int b = a + 1; b < 5; ++b)
            {
                double error = 0;
                for (int i = 1024; i < 8192; ++i)
                    error += std::abs(outputs[size_t(a)][size_t(i)] - outputs[size_t(b)][size_t(i)]);
                require(error > 10.0, "two modes are indistinguishable");
            }
    });
    for (bool bypass : {false, true})
        test(bypass ? "bypass equals delayed unity input" : "zero mix equals delayed unity input", [=]
        {
            emberfox::Settings s; s.mix = bypass ? 1.0f : 0.0f; s.bypass = bypass; s.outputDb = 6;
            emberfox::DistortionEngine e; e.prepare(48000, 64, 2, s);
            const int size = 4096; juce::AudioBuffer<float> b(2, size);
            std::vector<float> source(size);
            std::mt19937 random(42); std::uniform_real_distribution<float> values(-0.7f, 0.7f);
            for (int i = 0; i < size; ++i) source[size_t(i)] = values(random);
            for (int c = 0; c < 2; ++c) b.copyFrom(c, 0, source.data(), size);
            e.process(b);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < size; ++i)
                    require(std::abs(b.getSample(c, i) - (i < e.latency() ? 0.0f : source[size_t(i - e.latency())])) < 2.0e-6f,
                            "dry/bypass sample or latency mismatch");
        });
    test("results independent of host block segmentation", []
    {
        for (int mode = 0; mode < 5; ++mode)
        {
            emberfox::Settings s; s.mode = mode;
            auto a = render(s, 48000, 2, 1, 16384);
            for (int block : {17, 127, 512, 4096})
            {
                auto b = render(s, 48000, 2, block, 16384);
                for (size_t i = 0; i < a.size(); ++i)
                    require(std::abs(a[i] - b[i]) < 1.0e-5f, "block segmentation changed output");
            }
        }
    });
    test("stereo channels do not leak", []
    {
        for (int mode = 0; mode < 5; ++mode)
        {
            emberfox::Settings s; s.mode = mode;
            auto result = render(s, 48000, 2, 67, 8192, true);
            for (size_t i = 8192; i < result.size(); ++i) require(std::abs(result[i]) < 1.0e-7f, "crosstalk");
        }
    });
    test("parameter/mode/bypass transitions stay finite and smooth", []
    {
        emberfox::DistortionEngine e; e.prepare(48000, 128, 2);
        juce::AudioBuffer<float> b(2, 128); float previous = 0, biggestStep = 0;
        for (int k = 0; k < 600; ++k)
        {
            emberfox::Settings s; s.mode = (k / 13) % 5; s.driveDb = k % 2 ? 36.0f : 0.0f;
            s.toneHz = k % 2 ? 400.0f : 20000.0f; s.mix = k % 3 ? 1.0f : 0.0f;
            s.outputDb = -6.0f; s.bypass = k % 19 == 0; e.setSettings(s);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 128; ++i)
                    b.setSample(c, i, 0.2f * std::sin(float(2.0 * juce::MathConstants<double>::pi * 100 * (128 * k + i) / 48000.0)));
            e.process(b);
            for (int i = 0; i < 128; ++i)
            {
                float x = b.getSample(0, i); require(std::isfinite(x), "transition generated nonfinite output");
                biggestStep = std::max(biggestStep, std::abs(x - previous)); previous = x;
            }
        }
        require(biggestStep < 0.15f, "parameter transition produced a large discontinuity");
    });
    test("no C++ heap allocations in processing or settings updates", []
    {
        emberfox::DistortionEngine e; e.prepare(48000, 64, 2);
        juce::AudioBuffer<float> b(2, 4096); b.clear();
        emberfox::Settings s; s.mode = 4;
        allocations = 0; trackAllocations = true;
        e.setSettings(s); e.process(b);
        trackAllocations = false;
        require(allocations == 0, "audio processing allocated memory");
    });
    test("zero blocks and nonfinite input recover", []
    {
        emberfox::DistortionEngine e; e.prepare(48000, 16, 1);
        juce::AudioBuffer<float> empty(1, 0); e.process(empty);
        juce::AudioBuffer<float> b(1, 1024); b.clear();
        b.setSample(0, 0, std::numeric_limits<float>::infinity());
        b.setSample(0, 1, std::numeric_limits<float>::quiet_NaN()); e.process(b);
        for (int i = 0; i < 1024; ++i) require(std::isfinite(b.getSample(0, i)), "invalid input poisoned processing");
        b.clear(); e.process(b); require(b.getMagnitude(0, 1024) < 0.001f, "invalid input did not recover");
    });
    test("asymmetric fuzz rejects sustained DC", []
    {
        emberfox::Settings s; s.mode = 4; s.driveDb = 30; s.outputDb = 0;
        emberfox::DistortionEngine e; e.prepare(48000, 128, 1, s);
        juce::AudioBuffer<float> b(1, 48000);
        for (int i = 0; i < 48000; ++i) b.setSample(0, i, 0.3f);
        e.process(b); double mean = 0;
        for (int i = 44000; i < 48000; ++i) mean += b.getSample(0, i);
        require(std::abs(mean / 4000.0) < 0.0001, "DC component remains after settling");
    });
    test("4x oversampling reduces high-frequency hard-clip alias energy", []
    {
        constexpr int order = 15, size = 1 << order, settle = 8192;
        emberfox::Settings s; s.mode = 2; s.driveDb = 24; s.outputDb = 0; s.toneHz = 20000;
        emberfox::DistortionEngine e; e.prepare(48000, 128, 1, s);
        juce::AudioBuffer<float> b(1, size + settle);
        for (int i = 0; i < size + settle; ++i)
            b.setSample(0, i, 0.5f * std::sin(float(2.0 * juce::MathConstants<double>::pi * 9000 * i / 48000.0)));
        e.process(b);
        auto aliasRatio = [=](const std::vector<float>& values)
        {
            std::vector<float> fftData(size * 2, 0); std::copy(values.begin(), values.end(), fftData.begin());
            juce::dsp::FFT fft(order); fft.performFrequencyOnlyForwardTransform(fftData.data());
            double fundamental = 0, residual = 0;
            for (int bin = 1; bin < size / 2; ++bin)
            {
                double power = double(fftData[size_t(bin)]) * fftData[size_t(bin)];
                if (std::abs(bin - 6144) <= 2) fundamental += power;
                else residual += power;
            }
            return residual / std::max(1.0e-20, fundamental);
        };
        std::vector<float> wet(size), naive(size);
        for (int i = 0; i < size; ++i)
        {
            wet[size_t(i)] = b.getSample(0, i + settle);
            const float x = 0.5f * std::sin(float(2.0 * juce::MathConstants<double>::pi * 9000 * (i + settle) / 48000.0));
            naive[size_t(i)] = juce::jlimit(-1.0f, 1.0f, x * juce::Decibels::decibelsToGain(24.0f));
        }
        double improvement = 10.0 * std::log10(aliasRatio(naive) / aliasRatio(wet));
        std::cout << "MEASURE alias reduction: " << improvement << " dB (9 kHz, 48 kHz, Razor, 24 dB drive)\n";
        require(improvement > 10.0, "expected at least 10 dB alias energy reduction for this signal");
    });
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
