#pragma once
#include "DSP/SidechainEngine.h"
#include <juce_audio_utils/juce_audio_utils.h>
class SidechainProcessor final : public juce::AudioProcessor
{
  public:
    SidechainProcessor();
    juce::AudioProcessorValueTreeState state;
    mx::Ring<4096> scope;
    std::atomic<std::uint64_t> triggers{0};
    std::atomic<float> position{0}, reduction{0}, currentBpm{120};
    std::atomic<bool> sidechainConnected{false}, transportAvailable{false};
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    bool isBusesLayoutSupported(const BusesLayout &) const override;
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "MXND3X SIDECHAIN"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;
    void selectCurve(int);
    using juce::AudioProcessor::setParameter;
    void setParameter(const juce::String &, float);
    float value(const juce::String &) const;
    mx::Curve curve() const;
    void reset();
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();

  private:
    mx::Engine engine;
    struct Cached
    {
        std::atomic<float> *mix;
        std::atomic<float> *depth;
        std::atomic<float> *smooth;
        std::atomic<float> *crossover;
        std::atomic<float> *band;
        std::atomic<float> *mode;
        std::atomic<float> *timing;
        std::atomic<float> *modifier;
        std::atomic<float> *threshold;
        std::atomic<float> *sensitivity;
        std::atomic<float> *protection;
        std::atomic<float> *note;
        std::atomic<float> *any;
        std::atomic<float> *retrigger;
    } params{};
    std::atomic<float> *curveIndex = nullptr;
    double inputPower = 0, outputPower = 0;
    int meterCounter = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidechainProcessor)
};
