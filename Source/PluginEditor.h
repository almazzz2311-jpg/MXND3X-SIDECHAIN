#pragma once
#include "PluginProcessor.h"
#include "UI/Theme.h"
class EnvelopeView final : public juce::Component
{
    SidechainProcessor &p;
    int selected = -1;
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced(18, 25); }

  public:
    std::array<mx::MeterFrame, 512> history{};
    size_t cursor = 0;
    bool showInput = true, showOutput = true, showKick = false;
    explicit EnvelopeView(SidechainProcessor &processor) : p(processor) {}
    void paint(juce::Graphics &) override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;
    void mouseDoubleClick(const juce::MouseEvent &) override;
};
class CurveButton final : public juce::Button
{
    SidechainProcessor &p;
    int index;

  public:
    CurveButton(SidechainProcessor &processor, int n) : Button(mx::names[(size_t)n]), p(processor), index(n)
    {
        setTooltip(mx::names[(size_t)n]);
    }
    void paintButton(juce::Graphics &, bool, bool) override;
};
class SidechainEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
    SidechainProcessor &p;
    float activity = 0, previousPhase = 0;
    std::uint64_t previousTrigger = 0;
    ui::Theme theme;
    juce::Component canvas;
    EnvelopeView graph;
    std::array<std::unique_ptr<CurveButton>, 16> curves;
    struct Dial
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };
    std::array<Dial, 9> dials;
    std::array<Choice, 4> choices;
    juce::ToggleButton any{"Any note"}, retrigger{"Retrigger"}, input{"Input"}, output{"Output"},
        kick{"Sidechain"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> anyAttachment, retriggerAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 3> layerAttachments;
    juce::TextButton save{"SAVE PRESET"}, load{"LOAD PRESET"}, resetButton{"RESET"},
        resetCurve{"RESET CURVE"};
    juce::Label status, gr, bpmLabel;
    juce::Image character;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltip{this, 700};
    void timerCallback() override;
    void preset(bool saving);

  public:
    explicit SidechainEditor(SidechainProcessor &);
    ~SidechainEditor() override;
    void paint(juce::Graphics &) override;
    void resized() override;
};
