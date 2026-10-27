#include "PluginProcessor.h"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
static thread_local bool measureAllocations = false;
static thread_local size_t allocations = 0;
void *operator new(std::size_t n)
{
    if (measureAllocations)
        ++allocations;
    if (auto *p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t n)
{
    return ::operator new(n);
}
void operator delete(void *p) noexcept
{
    std::free(p);
}
void operator delete[](void *p) noexcept
{
    std::free(p);
}
void operator delete(void *p, std::size_t) noexcept
{
    std::free(p);
}
void operator delete[](void *p, std::size_t) noexcept
{
    std::free(p);
}
using namespace juce;
void check(bool b, const char *m)
{
    if (!b)
        throw std::runtime_error(m);
}
struct Playhead : AudioPlayHead
{
    double ppq = 0, bpm = 120;
    bool playing = true;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setPpqPosition(ppq);
        p.setBpm(bpm);
        p.setIsPlaying(playing);
        return p;
    }
};
int main()
{
    ScopedJuceInitialiser_GUI init;
    try
    {
        SidechainProcessor p;
        Playhead host;
        p.setPlayHead(&host);
        p.setRateAndBufferSizeDetails(48000, 512);
        p.prepareToPlay(48000, 512);
        int scenarios = 0;
        for (int size : {1, 7, 64, 257, 512, 2048})
        {
            AudioBuffer<float> b(2, size);
            MidiBuffer m;
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < size; ++n)
                    b.setSample(c, n, .5f);
            p.processBlock(b, m);
            for (int n = 0; n < size; ++n)
                check(std::isfinite(b.getSample(0, n)), "buffer sizes finite");
            host.ppq += double(size) / 24000;
            ++scenarios;
        }
        p.setParameter("mix", .27f);
        p.setParameter("depth", .63f);
        p.setParameter("band", 2);
        p.setParameter("x3", .34f);
        p.setParameter("y3", .19f);
        p.setParameter("modified", 1);
        MemoryBlock state;
        p.getStateInformation(state);
        SidechainProcessor restored;
        restored.setStateInformation(state.getData(), int(state.getSize()));
        for (auto *param : p.getParameters())
        {
            auto *a = dynamic_cast<AudioProcessorParameterWithID *>(param);
            check(std::abs(p.value(a->paramID) - restored.value(a->paramID)) < .00001f,
                  "all parameters restore");
        }
        ++scenarios;
        p.reset();
        p.setParameter("mode", 2);
        p.setRateAndBufferSizeDetails(48000, 512);
        p.prepareToPlay(48000, 512);
        AudioBuffer<float> b(2, 512);
        for (int c = 0; c < 2; ++c)
            for (int n = 0; n < 512; ++n)
                b.setSample(c, n, 1);
        MidiBuffer midi;
        midi.addEvent(MidiMessage::noteOn(1, 36, (uint8)100), 200);
        p.processBlock(b, midi);
        for (int n = 0; n < 200; ++n)
            check(b.getSample(0, n) == 1, "MIDI before offset remains dry");
        check(b.getSample(0, 400) < .4f, "MIDI trigger at offset");
        ++scenarios;
        p.reset();
        p.setRateAndBufferSizeDetails(48000, 512);
        p.prepareToPlay(48000, 512);
        host.ppq = 8.25;
        host.bpm = 140;
        midi.clear();
        p.processBlock(b, midi);
        check(std::abs(p.position.load() - float(mx::wrap(host.ppq + 511 * 140. / (60 * 48000)))) < .00001,
              "processor host sync");
        ++scenarios;
        auto layout = p.getBusesLayout();
        layout.inputBuses.set(0, AudioChannelSet::mono());
        layout.outputBuses.set(0, AudioChannelSet::mono());
        check(p.setBusesLayout(layout), "mono layout");
        ++scenarios;
        layout.inputBuses.set(1, AudioChannelSet::mono());
        check(p.setBusesLayout(layout), "external sidechain layout");
        p.setParameter("mode", 1);
        p.setRateAndBufferSizeDetails(48000, 512);
        p.prepareToPlay(48000, 512);
        AudioBuffer<float> sc(2, 512);
        for (int n = 0; n < 512; ++n)
        {
            sc.setSample(0, n, .5f);
            sc.setSample(1, n, n < 100 ? 1.f : 0.f);
        }
        p.processBlock(sc, midi);
        check(p.sidechainConnected.load() && sc.getSample(0, 400) < .2f, "external bus actually ducks audio");
        ++scenarios;
        p.reset();
        p.setRateAndBufferSizeDetails(48000, 512);
        p.prepareToPlay(48000, 512);
        allocations = 0;
        measureAllocations = true;
        p.processBlock(sc, midi);
        measureAllocations = false;
        check(allocations == 0, "no operator new allocations in processBlock");
        ++scenarios;
        auto render = [](int block)
        {
            SidechainProcessor processor;
            Playhead transport;
            processor.setPlayHead(&transport);
            processor.setRateAndBufferSizeDetails(48000, block);
            processor.prepareToPlay(48000, block);
            std::vector<float> out;
            out.reserve(24000);
            for (int start = 0; start < 24000; start += block)
            {
                int count = std::min(block, 24000 - start);
                AudioBuffer<float> audio(2, count);
                MidiBuffer events;
                for (int c = 0; c < 2; ++c)
                    for (int n = 0; n < count; ++n)
                        audio.setSample(c, n, float(std::sin((start + n) * .017)));
                transport.ppq = double(start) / 24000;
                processor.processBlock(audio, events);
                for (int n = 0; n < count; ++n)
                    out.push_back(audio.getSample(0, n));
            }
            return out;
        };
        auto one = render(1), odd = render(257);
        for (size_t i = 0; i < one.size(); ++i)
            check(std::abs(one[i] - odd[i]) < .00001f, "audio invariant under block partition");
        ++scenarios;
        std::cout << "PASS: " << scenarios
                  << " processor scenarios (state, MIDI offsets, transport, buses, buffers)\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
