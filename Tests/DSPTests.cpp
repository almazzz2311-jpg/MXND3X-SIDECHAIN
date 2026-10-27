#include "DSP/SidechainEngine.h"
#include <iostream>
#include <stdexcept>
#include <vector>
void require(bool b, const char *message)
{
    if (!b)
        throw std::runtime_error(message);
}
float sample(mx::Engine &e, float x, int n, double bpm = 120, bool midi = false, float kick = 0)
{
    float f[2] = {x, x};
    e.process(f, 2, kick, midi, bpm, n * bpm / (60 * e.sampleRate()), true, true);
    require(std::isfinite(f[0]), "finite output");
    return f[0];
}
int main()
{
    try
    {
        int checks = 0;
        for (double sr : {44100., 48000., 88200., 96000., 192000.})
            for (int preset = 0; preset < 16; ++preset)
            {
                mx::Engine e;
                e.settings.curve = mx::factory(preset);
                e.prepare(sr);
                double energy = 0;
                for (int n = 0; n < int(sr); ++n)
                {
                    float y = sample(e, .5f, n);
                    energy += y * y;
                }
                require(energy > 0 && energy < sr * .25, "curves attenuate");
                ++checks;
            }
        for (int shape = 0; shape < 16; ++shape)
        {
            auto c = mx::factory(shape);
            for (size_t i = 1; i < c.size(); ++i)
            {
                require(c[i].x > c[i - 1].x, "fixed knot times strictly ordered");
                for (int n = 0; n <= 100; ++n)
                {
                    double phase = c[i - 1].x + (c[i].x - c[i - 1].x) * n / 100.;
                    float y = mx::envelope(c, phase);
                    require(std::isfinite(y) && y >= std::min(c[i - 1].y, c[i].y) - .00001f &&
                                y <= std::max(c[i - 1].y, c[i].y) + .00001f,
                            "shape interpolation preserves each silhouette without overshoot");
                }
            }
            ++checks;
        }
        for (bool depth : {false, true})
            for (int band = 0; band < 3; ++band)
            {
                mx::Engine e;
                e.settings.band = band;
                if (depth)
                    e.settings.depth = 0;
                else
                    e.settings.mix = 0;
                e.prepare(48000);
                for (int n = 0; n < 48000; ++n)
                {
                    float x = float(std::sin(n * .17));
                    require(sample(e, x, n) == x, "exact dry at Mix or Depth zero");
                }
                ++checks;
            }
        for (int a = 0; a < 16; ++a)
            for (int b = a + 1; b < 16; ++b)
            {
                double difference = 0;
                for (int n = 0; n < 100; ++n)
                    difference += std::abs(mx::envelope(mx::factory(a), n / 100.) -
                                           mx::envelope(mx::factory(b), n / 100.));
                require(difference > .01, "distinct presets");
                ++checks;
            }
        require(mx::cycleBeats(2, 0) == 1 && mx::cycleBeats(2, 1) == 2. / 3. && mx::cycleBeats(2, 2) == 1.5,
                "timing modifiers");
        ++checks;
        for (double bpm : {60., 120., 175., 240.})
        {
            mx::Engine e;
            e.prepare(48000);
            sample(e, 1, 6000, bpm);
            require(std::abs(e.lastPhase - mx::wrap(6000 * bpm / (60 * 48000.))) < .00001,
                    "host phase tracks BPM/seek");
            sample(e, 1, 0, bpm);
            require(e.lastPhase == 0, "loop returns to downbeat");
            ++checks;
        }
        {
            mx::Engine e;
            e.settings.mode = 2;
            e.prepare(48000);
            require(sample(e, 1, 0) == 1, "MIDI idle dry");
            sample(e, 1, 1, 120, true);
            require(e.lastPhase == 0, "MIDI trigger exact sample");
            for (int n = 2; n < 100; ++n)
                sample(e, 1, n);
            require(e.lastGain < .8, "MIDI envelope attenuates");
            ++checks;
        }
        {
            mx::Engine e;
            e.settings.mode = 1;
            e.prepare(48000);
            for (int n = 0; n < 1000; ++n)
                sample(e, 1, n, 120, false, n < 100 ? 1.f : 0.f);
            require(e.lastGain < .2, "audio trigger duck");
            ++checks;
        }
        for (int band : {1, 2})
        {
            auto rms = [&](double freq)
            {
                mx::Engine e;
                e.settings.band = band;
                auto zero = mx::factory(0);
                for (auto &pt : zero)
                    pt.y = 0;
                e.settings.curve = zero;
                e.prepare(48000);
                double sum = 0;
                for (int n = 0; n < 96000; ++n)
                {
                    float y = sample(e, float(std::sin(2 * mx::pi * freq * n / 48000)), n);
                    if (n >= 48000)
                        sum += y * y;
                }
                return std::sqrt(sum / 48000);
            };
            double low = rms(40), high = rms(5000);
            require(band == 1 ? high > .69 && low < .02 : low > .69 && high < .02,
                    "band frequency selectivity");
            ++checks;
        }
        {
            mx::Engine e;
            e.prepare(48000);
            float previous = sample(e, 1, 0);
            float maxDelta = 0;
            for (int n = 1; n < 96000; ++n)
            {
                if (n == 10000)
                    e.settings.curve = mx::factory(14);
                if (n == 20000)
                    e.settings.band = 1;
                if (n == 30000)
                    e.settings.crossover = 1000;
                float y = sample(e, 1, n);
                maxDelta = std::max(maxDelta, std::abs(y - previous));
                previous = y;
            }
            require(maxDelta < .025, "gain dezipper limits discontinuities");
            ++checks;
        }
        {
            mx::Engine a, b;
            a.prepare(48000);
            b.prepare(48000);
            b.settings.mix = 0;
            for (int n = 0; n < 24000; ++n)
            {
                sample(a, 1, n);
                sample(b, 1, n);
            }
            require(a.lastGain < 1 && b.lastGain > .99, "instances independent");
            ++checks;
        }
        std::cout << "PASS: " << checks << " DSP scenarios\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
