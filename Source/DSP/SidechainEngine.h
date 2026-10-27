#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace mx
{
constexpr double pi = 3.14159265358979323846;
constexpr int pointCount = 9;
struct Point
{
    float x = 0, y = 1;
};
using Curve = std::array<Point, pointCount>;
inline constexpr std::array<const char *, 16> names{
    {"Classic Pump", "Smooth Duck", "Tight Kick", "Deep Pump", "Short Duck", "Long Release", "House Groove",
     "Techno Pump", "Hyperpop", "Trap Bounce", "Hard Duck", "Soft Curve", "Fast Recovery", "Slow Recovery",
     "Reverse Shape", "Custom"}};
inline Curve factory(int index)
{
    constexpr float shapes[16][9] = {{0, .01f, .12f, .40f, .73f, .92f, 1, 1, 1},
                                     {.12f, .16f, .30f, .50f, .72f, .87f, .96f, 1, 1},
                                     {0, .05f, .75f, 1, 1, 1, 1, 1, 1},
                                     {0, 0, 0, .03f, .16f, .40f, .72f, .93f, 1},
                                     {0, .65f, 1, 1, 1, 1, 1, 1, 1},
                                     {0, .01f, .05f, .13f, .26f, .44f, .65f, .85f, 1},
                                     {0, .03f, .30f, .80f, 1, .76f, .85f, 1, 1},
                                     {0, 0, .02f, .12f, .67f, 1, 1, 1, 1},
                                     {0, 0, .90f, 1, .64f, 1, 1, 1, 1},
                                     {0, .1f, .85f, 1, .30f, .70f, 1, 1, 1},
                                     {0, 0, 0, 0, .02f, .90f, 1, 1, 1},
                                     {.45f, .48f, .59f, .71f, .83f, .93f, .98f, 1, 1},
                                     {0, .90f, 1, 1, 1, 1, 1, 1, 1},
                                     {0, 0, .01f, .04f, .10f, .23f, .46f, .76f, 1},
                                     {1, 1, .98f, .90f, .75f, .50f, .25f, .08f, 0},
                                     {0, .05f, .18f, .38f, .62f, .82f, .95f, 1, 1}};
    Curve c{};
    for (int i = 0; i < pointCount; ++i)
        c[(size_t)i] = {float(i) / 8, shapes[std::clamp(index, 0, 15)][i]};
    return c;
}
inline float envelope(const Curve &c, double phase)
{
    const float p = std::clamp(float(phase), 0.f, 1.f);
    for (size_t i = 1; i < c.size(); ++i)
        if (p <= c[i].x)
        {
            float t = std::clamp((p - c[i - 1].x) / std::max(.00001f, c[i].x - c[i - 1].x), 0.f, 1.f);
            t = t * t * (3 - 2 * t);
            return std::clamp(c[i - 1].y + (c[i].y - c[i - 1].y) * t, 0.f, 1.f);
        }
    return std::clamp(c.back().y, 0.f, 1.f);
}
inline double cycleBeats(int timing, int modifier)
{
    double beats = 4.0 / double(1 << std::clamp(timing, 0, 5));
    return beats * (modifier == 1 ? 2.0 / 3.0 : modifier == 2 ? 1.5 : 1.0);
}
inline double wrap(double p)
{
    return p - std::floor(p);
}
struct Settings
{
    Curve curve = factory(0);
    float mix = 1, depth = 1, smoothMs = 3, crossover = 150, thresholdDb = -24, sensitivity = 1,
          protectionMs = 80;
    int band = 0, mode = 0, timing = 2, modifier = 0, midiNote = 36;
    bool anyNote = true, retrigger = true;
};
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void coefficients(double sr, double hz, bool high)
    {
        double w = 2 * pi * hz / sr, c = std::cos(w), a = std::sin(w) / std::sqrt(2.0), norm = 1 / (1 + a);
        b0 = (high ? 1 + c : 1 - c) * .5 * norm;
        b1 = (high ? -(1 + c) : 1 - c) * norm;
        b2 = b0;
        a1 = -2 * c * norm;
        a2 = (1 - a) * norm;
    }
    float process(float x)
    {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return float(y);
    }
};
struct Splitter
{
    std::array<Biquad, 2> low{}, high{};
    void coefficients(double sr, double hz)
    {
        for (auto &f : low)
            f.coefficients(sr, hz, false);
        for (auto &f : high)
            f.coefficients(sr, hz, true);
    }
    void process(float x, float &l, float &h)
    {
        l = low[1].process(low[0].process(x));
        h = high[1].process(high[0].process(x));
    }
};
struct MeterFrame
{
    float input = 0, output = 0, kick = 0, phase = 0, gain = 1;
};
// Single producer (audio), single consumer (editor). Never blocks or allocates.
template <size_t N> struct Ring
{
    std::array<MeterFrame, N> data{};
    std::atomic<size_t> read{0}, write{0};
    void push(MeterFrame f)
    {
        auto w = write.load(std::memory_order_relaxed), n = (w + 1) % N;
        if (n == read.load(std::memory_order_acquire))
            return;
        data[w] = f;
        write.store(n, std::memory_order_release);
    }
    bool pop(MeterFrame &f)
    {
        auto r = read.load(std::memory_order_relaxed);
        if (r == write.load(std::memory_order_acquire))
            return false;
        f = data[r];
        read.store((r + 1) % N, std::memory_order_release);
        return true;
    }
};
class Engine
{
    double sr = 48000, phase = 1, fallback = 0;
    float gain = 1, mix = 1, depth = 1, hz = 150;
    std::array<float, 3> bandWeight{{1, 0, 0}};
    std::array<Splitter, 2> split{};
    int cooldown = 0, coefficientCounter = 0;
    float detector = 0;
    bool armed = true;

  public:
    std::uint64_t triggers = 0;
    Settings settings{};
    float lastGain = 1, lastPhase = 0;
    void prepare(double sampleRate)
    {
        sr = std::max(8000.0, sampleRate);
        phase = 1;
        fallback = 0;
        gain = 1;
        mix = settings.mix;
        depth = settings.depth;
        hz = settings.crossover;
        split = {};
        bandWeight = {0, 0, 0};
        bandWeight[(size_t)std::clamp(settings.band, 0, 2)] = 1;
        for (auto &s : split)
            s.coefficients(sr, hz);
        cooldown = 0;
        detector = 0;
        armed = true;
        coefficientCounter = 0;
    }
    void trigger()
    {
        if (settings.retrigger || phase >= 1)
        {
            phase = 0;
            ++triggers;
        }
    }
    double sampleRate() const { return sr; }
    void process(float *samples, int channels, float kick, bool midi, double bpm, double ppq, bool playing,
                 bool hasPosition)
    {
        bpm = std::isfinite(bpm) && bpm > 0 ? bpm : 120;
        hasPosition = hasPosition && std::isfinite(ppq);
        const double beats = cycleBeats(settings.timing, settings.modifier),
                     increment = bpm / (60 * sr * beats);
        if (cooldown > 0)
            --cooldown;
        float amplitude = std::isfinite(kick) ? std::abs(kick) * settings.sensitivity : 0;
        detector +=
            (amplitude > detector ? .5f : float(1 - std::exp(-1 / (.02 * sr)))) * (amplitude - detector);
        const float threshold = std::pow(10.f, settings.thresholdDb / 20);
        if (detector < threshold * .5f)
            armed = true;
        if (settings.mode == 1 && detector >= threshold && armed && cooldown == 0)
        {
            trigger();
            armed = false;
            cooldown = int(settings.protectionMs * .001 * sr);
        }
        if (settings.mode == 2 && midi)
            trigger();
        double p = phase;
        bool active = true;
        if (settings.mode == 0)
        {
            active = playing;
            p = hasPosition ? wrap(ppq / beats) : fallback;
            if (playing)
                fallback = wrap(p + increment);
        }
        const float target = active && p < 1 ? envelope(settings.curve, p) : 1;
        // Even 0 ms uses a 0.25 ms safety dezipper at discontinuities.
        const float alpha = float(1 - std::exp(-1 / (std::max(.25f, settings.smoothMs) * .001 * sr)));
        gain += alpha * (target - gain);
        const float control = float(1 - std::exp(-1 / (.005 * sr)));
        mix += control * (settings.mix - mix);
        depth += control * (settings.depth - depth);
        hz += control * (settings.crossover - hz);
        if (++coefficientCounter >= 16)
        {
            for (auto &s : split)
                s.coefficients(sr, hz);
            coefficientCounter = 0;
        }
        for (int b = 0; b < 3; ++b)
            bandWeight[(size_t)b] += control * ((settings.band == b ? 1.f : 0.f) - bandWeight[(size_t)b]);
        for (int ch = 0; ch < channels; ++ch)
        {
            float x = std::isfinite(samples[ch]) ? samples[ch] : 0, l = 0, h = 0;
            split[(size_t)ch].process(x, l, h);
            float wet =
                bandWeight[0] * (x * gain) + bandWeight[1] * (l * gain + h) + bandWeight[2] * (l + h * gain);
            samples[ch] = x + mix * depth * (wet - x);
            if (!std::isfinite(samples[ch]))
                samples[ch] = 0;
        }
        lastGain = std::clamp(1 - mix * depth * (1 - gain), 0.f, 1.f);
        lastPhase = float(p);
        if (settings.mode != 0)
            phase = std::min(1.0, phase + increment);
    }
};
} // namespace mx
