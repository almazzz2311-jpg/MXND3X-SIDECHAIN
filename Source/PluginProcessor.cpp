#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;
AudioProcessorValueTreeState::ParameterLayout SidechainProcessor::layout()
{
    AudioProcessorValueTreeState::ParameterLayout p;
    auto f = [&](const char *id, const char *name, float lo, float hi, float def, float step = 0.001f)
    {
        p.add(std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, name,
                                                    NormalisableRange<float>(lo, hi, step), def));
    };
    auto c = [&](const char *id, const char *name, StringArray choices, int def = 0)
    { p.add(std::make_unique<AudioParameterChoice>(ParameterID{id, 1}, name, choices, def)); };
    f("mix", "Mix", 0, 1, 1);
    f("depth", "Depth", 0, 1, 1);
    f("smooth", "Smooth (ms)", 0, 20, 3);
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"crossover", 1}, "Crossover (Hz)",
                                                NormalisableRange<float>(40, 1000, 1, .4f), 150));
    c("band", "Band", {"Full Band", "Low Band", "High Band"});
    c("mode", "Trigger", {"BPM Sync", "Audio Trigger", "MIDI Trigger"});
    c("timing", "Cycle", {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32"}, 2);
    c("modifier", "Timing", {"Straight", "Triplet", "Dotted"});
    StringArray ns;
    for (auto n : mx::names)
        ns.add(n);
    c("curve", "Curve", ns);
    f("threshold", "Threshold (dB)", -60, 0, -24);
    f("sensitivity", "Sensitivity", .1f, 4, 1);
    f("protection", "Retrigger protection (ms)", 10, 500, 80, 1);
    f("note", "MIDI note", 0, 127, 36, 1);
    p.add(std::make_unique<AudioParameterBool>(ParameterID{"any", 1}, "Any MIDI note", true));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{"retrigger", 1}, "Retrigger while active", true));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{"modified", 1}, "Custom curve", false));
    for (auto id : {"viewInput", "viewOutput", "viewKick"})
        p.add(std::make_unique<AudioParameterBool>(ParameterID{id, 1}, id, String(id) != "viewKick"));
    auto initial = mx::factory(0);
    for (int i = 0; i < mx::pointCount; ++i)
    {
        p.add(std::make_unique<AudioParameterFloat>(
            ParameterID{"x" + String(i), 1}, "Point " + String(i) + " time", 0.f, 1.f, initial[(size_t)i].x));
        p.add(std::make_unique<AudioParameterFloat>(
            ParameterID{"y" + String(i), 1}, "Point " + String(i) + " gain", 0.f, 1.f, initial[(size_t)i].y));
    }
    return p;
}
SidechainProcessor::SidechainProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", AudioChannelSet::stereo(), true)
                         .withOutput("Output", AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", AudioChannelSet::stereo(), false)),
      state(*this, nullptr, "MXND3X", layout())
{
    curveIndex = state.getRawParameterValue("curve");
    modified = state.getRawParameterValue("modified");
    params = {state.getRawParameterValue("mix"),        state.getRawParameterValue("depth"),
              state.getRawParameterValue("smooth"),     state.getRawParameterValue("crossover"),
              state.getRawParameterValue("band"),       state.getRawParameterValue("mode"),
              state.getRawParameterValue("timing"),     state.getRawParameterValue("modifier"),
              state.getRawParameterValue("threshold"),  state.getRawParameterValue("sensitivity"),
              state.getRawParameterValue("protection"), state.getRawParameterValue("note"),
              state.getRawParameterValue("any"),        state.getRawParameterValue("retrigger")};
    for (int i = 0; i < mx::pointCount; ++i)
    {
        xs[(size_t)i] = state.getRawParameterValue("x" + String(i));
        ys[(size_t)i] = state.getRawParameterValue("y" + String(i));
    }
}
float SidechainProcessor::value(const String &id) const
{
    return state.getRawParameterValue(id)->load();
}
void SidechainProcessor::setParameter(const String &id, float v)
{
    if (auto *p = state.getParameter(id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(v));
        p->endChangeGesture();
    }
}
void SidechainProcessor::selectCurve(int n)
{
    setParameter("curve", float(n));
    auto c = mx::factory(n);
    for (int i = 0; i < mx::pointCount; ++i)
    {
        setParameter("x" + String(i), c[(size_t)i].x);
        setParameter("y" + String(i), c[(size_t)i].y);
    }
    setParameter("modified", 0);
}
mx::Curve SidechainProcessor::curve() const
{
    if (modified->load() < .5f)
        return mx::factory(int(curveIndex->load()));
    mx::Curve c{};
    for (int i = 0; i < mx::pointCount; ++i)
        c[(size_t)i] = {xs[(size_t)i]->load(), ys[(size_t)i]->load()};
    c.front().x = 0;
    c.back().x = 1;
    for (int i = 1; i < mx::pointCount - 1; ++i)
        c[(size_t)i].x = std::clamp(c[(size_t)i].x, c[(size_t)i - 1].x + .001f,
                                    1.f - float(mx::pointCount - 1 - i) * .001f);
    return c;
}
bool SidechainProcessor::isBusesLayoutSupported(const BusesLayout &l) const
{
    auto main = l.getMainInputChannelSet();
    if (main != AudioChannelSet::mono() && main != AudioChannelSet::stereo())
        return false;
    if (main != l.getMainOutputChannelSet())
        return false;
    auto sc = l.getChannelSet(true, 1);
    return sc.isDisabled() || sc == AudioChannelSet::mono() || sc == AudioChannelSet::stereo();
}
void SidechainProcessor::prepareToPlay(double rate, int)
{
    engine.settings.mix = params.mix->load();
    engine.settings.depth = params.depth->load();
    engine.settings.band = int(params.band->load());
    engine.settings.crossover = params.crossover->load();
    engine.prepare(rate);
    inputPower = outputPower = 0;
    meterCounter = 0;
    setLatencySamples(0);
}
void SidechainProcessor::processBlock(AudioBuffer<float> &buffer, MidiBuffer &midi)
{
    ScopedNoDenormals noDenormals;
    auto main = getBusBuffer(buffer, false, 0);
    auto side = getBusBuffer(buffer, true, 1);
    bool connected = getBus(true, 1)->isEnabled() && side.getNumChannels() > 0;
    sidechainConnected.store(connected);
    auto &s = engine.settings;
    s.curve = curve();
    s.mix = params.mix->load();
    s.depth = params.depth->load();
    s.smoothMs = params.smooth->load();
    s.crossover = params.crossover->load();
    s.band = int(params.band->load());
    s.mode = int(params.mode->load());
    s.timing = int(params.timing->load());
    s.modifier = int(params.modifier->load());
    s.thresholdDb = params.threshold->load();
    s.sensitivity = params.sensitivity->load();
    s.protectionMs = params.protection->load();
    s.midiNote = int(params.note->load());
    s.anyNote = params.any->load() > .5f;
    s.retrigger = params.retrigger->load() > .5f;
    double bpm = 120, ppq = 0;
    bool playing = true, hasPosition = false;
    if (auto *play = getPlayHead())
        if (auto pos = play->getPosition())
        {
            if (auto tempo = pos->getBpm())
                bpm = *tempo;
            if (auto beat = pos->getPpqPosition())
            {
                ppq = *beat;
                hasPosition = true;
            }
            playing = pos->getIsPlaying();
        }
    if (!std::isfinite(bpm) || bpm <= 0)
        bpm = 120;
    if (!std::isfinite(ppq))
    {
        ppq = 0;
        hasPosition = false;
    }
    transportAvailable.store(hasPosition);
    currentBpm.store(float(bpm));
    auto it = midi.cbegin();
    const auto end = midi.cend();
    for (int i = 0; i < main.getNumSamples(); ++i)
    {
        bool note = false;
        while (it != end && (*it).samplePosition <= i)
        {
            auto event = *it;
            if (event.numBytes == 3 && (event.data[0] & 0xf0) == 0x90 && event.data[2] > 0 &&
                (s.anyNote || event.data[1] == s.midiNote))
                note = true;
            ++it;
        }
        float kick = 0;
        if (connected)
            for (int ch = 0; ch < side.getNumChannels(); ++ch)
                if (std::abs(side.getSample(ch, i)) > std::abs(kick))
                    kick = side.getSample(ch, i);
        float frame[2] = {0, 0};
        for (int ch = 0; ch < main.getNumChannels(); ++ch)
            frame[ch] = main.getSample(ch, i);
        const float input = frame[0];
        double inEnergy = 0;
        for (int ch = 0; ch < main.getNumChannels(); ++ch)
            inEnergy += double(frame[ch]) * frame[ch];
        engine.process(frame, main.getNumChannels(), kick, note, bpm,
                       ppq + double(i) * bpm / (60 * engine.sampleRate()), playing, hasPosition);
        for (int ch = 0; ch < main.getNumChannels(); ++ch)
            main.setSample(ch, i, frame[ch]);
        double outEnergy = 0;
        for (int ch = 0; ch < main.getNumChannels(); ++ch)
            outEnergy += double(frame[ch]) * frame[ch];
        const double meterAlpha = 1 - std::exp(-1 / (.03 * engine.sampleRate()));
        inputPower += meterAlpha * (inEnergy - inputPower);
        outputPower += meterAlpha * (outEnergy - outputPower);
        if (++meterCounter >= 64)
        {
            scope.push({input, frame[0], kick, engine.lastPhase, engine.lastGain});
            meterCounter = 0;
        }
    }
    triggers.store(engine.triggers);
    position.store(engine.lastPhase);
    reduction.store(inputPower > 1.e-12
                        ? float(std::max(0.0, 10 * std::log10(inputPower / std::max(1.e-12, outputPower))))
                        : 0.f);
    midi.clear();
}
void SidechainProcessor::getStateInformation(MemoryBlock &b)
{
    auto tree = state.copyState();
    tree.setProperty("schemaVersion", 1, nullptr);
    auto xml = tree.createXml();
    copyXmlToBinary(*xml, b);
}
void SidechainProcessor::setStateInformation(const void *d, int n)
{
    if (auto xml = getXmlFromBinary(d, n))
        if (xml->hasTagName("MXND3X"))
        {
            auto t = ValueTree::fromXml(*xml);
            if (t.isValid())
                state.replaceState(t);
        }
}
void SidechainProcessor::reset()
{
    for (auto *p : getParameters())
        if (auto *r = dynamic_cast<RangedAudioParameter *>(p))
        {
            r->beginChangeGesture();
            r->setValueNotifyingHost(r->getDefaultValue());
            r->endChangeGesture();
        }
}
AudioProcessorEditor *SidechainProcessor::createEditor()
{
    return new SidechainEditor(*this);
}
AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new SidechainProcessor();
}
