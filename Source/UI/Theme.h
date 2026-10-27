#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace ui
{
inline const juce::Colour bg{0xff0b0b10}, panel{0xff17131f}, surface{0xff25202d}, purple{0xffc83bff},
    pink{0xffe15bff}, text{0xfff7f5fa}, muted{0xff92909b};
class Theme : public juce::LookAndFeel_V4
{
  public:
    Theme()
    {
        setColour(juce::Slider::textBoxTextColourId, text);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::ComboBox::backgroundColourId, surface);
        setColour(juce::ComboBox::outlineColourId, surface);
        setColour(juce::ComboBox::textColourId, text);
        setColour(juce::TextButton::buttonColourId, surface);
        setColour(juce::TextButton::textColourOffId, text);
        setColour(juce::ToggleButton::textColourId, muted);
        setColour(juce::ToggleButton::tickColourId, purple);
    }
    void drawRotarySlider(juce::Graphics &g, int x, int y, int w, int h, float p, float start, float end,
                          juce::Slider &) override
    {
        auto r = juce::Rectangle<float>(float(x), float(y), float(w), float(h)).reduced(9);
        auto size = juce::jmin(r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre(size, size);
        g.setColour(surface);
        g.fillEllipse(r);
        auto arc = r.reduced(3);
        juce::Path track, active;
        track.addCentredArc(arc.getCentreX(), arc.getCentreY(), arc.getWidth() / 2, arc.getHeight() / 2, 0,
                            start, end, true);
        active.addCentredArc(arc.getCentreX(), arc.getCentreY(), arc.getWidth() / 2, arc.getHeight() / 2, 0,
                             start, start + p * (end - start), true);
        g.setColour(muted.withAlpha(.18f));
        g.strokePath(track, juce::PathStrokeType(4));
        g.setColour(purple.withAlpha(.15f));
        g.strokePath(active, juce::PathStrokeType(11));
        g.setColour(pink);
        g.strokePath(active, juce::PathStrokeType(3));
        g.setColour(panel);
        g.fillEllipse(r.reduced(12));
        float a = start + p * (end - start);
        auto centre = r.getCentre();
        juce::Point<float> tip(centre.x + std::sin(a) * (size * .28f),
                               centre.y - std::cos(a) * (size * .28f));
        g.setColour(text);
        g.drawLine(centre.x, centre.y, tip.x, tip.y, 2);
    }
};
} // namespace ui
