#include "PluginEditor.h"
#ifdef MXND3X_HAS_CHARACTER
#include <BinaryData.h>
#endif
using namespace juce;
void EnvelopeView::paint(Graphics &g)
{
    auto r = plot();
    g.setColour(ui::bg);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 12);
    for (int i = 0; i <= 8; ++i)
    {
        g.setColour(ui::surface);
        float x = r.getX() + r.getWidth() * float(i) / 8;
        g.drawVerticalLine(int(x), r.getY(), r.getBottom());
    }
    for (int i = 0; i <= 4; ++i)
    {
        g.setColour(ui::surface);
        float y = r.getY() + r.getHeight() * float(i) / 4;
        g.drawHorizontalLine(int(y), r.getX(), r.getRight());
    }
    auto wave = [&](int which, Colour colour)
    {
        Path path;
        for (size_t i = 0; i < history.size(); ++i)
        {
            auto f = history[(cursor + i) % history.size()];
            float v = which == 0 ? f.input : which == 1 ? f.output : f.kick;
            float x = r.getX() + r.getWidth() * float(i) / float(history.size() - 1),
                  y = r.getCentreY() - std::clamp(v, -1.f, 1.f) * r.getHeight() * .4f;
            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }
        g.setColour(colour);
        g.strokePath(path, PathStrokeType(1));
    };
    if (showInput)
        wave(0, ui::muted.withAlpha(.35f));
    if (showOutput)
        wave(1, ui::purple.withAlpha(.65f));
    if (showKick && p.sidechainConnected.load())
        wave(2, Colours::orange.withAlpha(.6f));
    auto c = p.curve();
    Path path;
    for (int i = 0; i <= 400; ++i)
    {
        float phase = float(i) / 400, x = r.getX() + phase * r.getWidth(),
              y = r.getBottom() - mx::envelope(c, phase) * r.getHeight();
        if (i == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }
    auto fill = path;
    fill.lineTo(r.getBottomRight());
    fill.lineTo(r.getBottomLeft());
    fill.closeSubPath();
    g.setGradientFill(ColourGradient(ui::purple.withAlpha(.2f), r.getTopLeft(), ui::purple.withAlpha(.01f),
                                     r.getBottomLeft(), false));
    g.fillPath(fill);
    g.setColour(ui::purple.withAlpha(.13f));
    g.strokePath(path, PathStrokeType(9));
    g.setColour(ui::pink);
    g.strokePath(path, PathStrokeType(2.4f));
    for (auto pt : c)
    {
        auto x = r.getX() + pt.x * r.getWidth(), y = r.getBottom() - pt.y * r.getHeight();
        g.setColour(ui::panel);
        g.fillEllipse(x - 5, y - 5, 10, 10);
        g.setColour(ui::pink);
        g.drawEllipse(x - 5, y - 5, 10, 10, 1.4f);
    }
    float x = r.getX() + std::clamp(p.position.load(), 0.f, 1.f) * r.getWidth();
    g.setColour(ui::text.withAlpha(.7f));
    g.drawLine(x, r.getY(), x, r.getBottom(), 1);
    g.setColour(ui::muted);
    g.setFont(FontOptions(10.f));
    g.drawText("100%", int(r.getX()), 2, 50, 18, Justification::left);
    g.drawText("DRAG POINTS TO SHAPE YOUR GROOVE", int(r.getX()), getHeight() - 20, 330, 16,
               Justification::left);
}
void EnvelopeView::mouseDown(const MouseEvent &e)
{
    auto r = plot();
    auto c = p.curve();
    float distance = 100000;
    for (int i = 0; i < mx::pointCount; ++i)
    {
        Point<float> pt(r.getX() + c[(size_t)i].x * r.getWidth(),
                        r.getBottom() - c[(size_t)i].y * r.getHeight());
        float d = pt.getDistanceFrom(e.position);
        if (d < distance)
        {
            distance = d;
            selected = i;
        }
    }
    if (distance > 25)
    {
        selected = -1;
        return;
    }
    p.state.getParameter("x" + String(selected))->beginChangeGesture();
    p.state.getParameter("y" + String(selected))->beginChangeGesture();
    mouseDrag(e);
}
void EnvelopeView::mouseDrag(const MouseEvent &e)
{
    if (selected < 0)
        return;
    auto r = plot();
    auto c = p.curve();
    float x = selected == 0 ? 0
              : selected == 8
                  ? 1
                  : std::clamp((e.position.x - r.getX()) / r.getWidth(), c[(size_t)selected - 1].x + .0001f,
                               c[(size_t)selected + 1].x - .0001f);
    float y = std::clamp((r.getBottom() - e.position.y) / r.getHeight(), 0.f, 1.f);
    p.state.getParameter("x" + String(selected))->setValueNotifyingHost(x);
    p.state.getParameter("y" + String(selected))->setValueNotifyingHost(y);
    p.setParameter("modified", 1);
    repaint();
}
void EnvelopeView::mouseUp(const MouseEvent &)
{
    if (selected >= 0)
    {
        p.state.getParameter("x" + String(selected))->endChangeGesture();
        p.state.getParameter("y" + String(selected))->endChangeGesture();
        selected = -1;
    }
}
void EnvelopeView::mouseDoubleClick(const MouseEvent &)
{
    p.selectCurve(int(p.value("curve")));
    repaint();
}
void CurveButton::paintButton(Graphics &g, bool over, bool)
{
    bool active = int(p.value("curve")) == index;
    auto r = getLocalBounds().toFloat().reduced(2);
    g.setColour(active ? ui::purple.withAlpha(.20f) : over ? ui::surface : ui::panel);
    g.fillRoundedRectangle(r, 6);
    g.setColour(active ? ui::purple : ui::surface);
    g.drawRoundedRectangle(r, 6, 1);
    Path path;
    auto c = index == 15 ? p.curve() : mx::factory(index);
    auto plot = r.reduced(8, 6).withTrimmedBottom(14);
    for (int i = 0; i <= 60; ++i)
    {
        float x = float(i) / 60, y = mx::envelope(c, x);
        Point<float> pt(plot.getX() + x * plot.getWidth(), plot.getBottom() - y * plot.getHeight());
        if (i == 0)
            path.startNewSubPath(pt);
        else
            path.lineTo(pt);
    }
    g.setColour(active ? ui::pink : ui::muted);
    g.strokePath(path, PathStrokeType(1.4f));
    g.setFont(FontOptions(9.f));
    g.drawText(mx::names[(size_t)index], r.toNearestInt().withTop(getHeight() - 20), Justification::centred);
}
SidechainEditor::SidechainEditor(SidechainProcessor &proc) : AudioProcessorEditor(proc), p(proc), graph(proc)
{
    setLookAndFeel(&theme);
    addAndMakeVisible(canvas);
    canvas.addAndMakeVisible(graph);
    const char *ids[] = {"mix",         "depth",      "smooth", "crossover", "threshold",
                         "sensitivity", "protection", "note",   "unused"};
    const char *labels[] = {"MIX",
                            "DEPTH",
                            "SMOOTH / ms",
                            "CROSSOVER / Hz",
                            "THRESHOLD / dB",
                            "SENSITIVITY",
                            "PROTECTION / ms",
                            "MIDI NOTE",
                            ""};
    for (size_t i = 0; i < 8; ++i)
    {
        auto &d = dials[i];
        canvas.addAndMakeVisible(d.slider);
        canvas.addAndMakeVisible(d.label);
        d.label.setText(labels[i], dontSendNotification);
        d.label.setJustificationType(Justification::centred);
        d.label.setColour(Label::textColourId, ui::muted);
        d.label.setFont(FontOptions(11.f));
        d.slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
        d.slider.setTextBoxStyle(Slider::TextBoxBelow, false, 78, 20);
        d.attachment =
            std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.state, ids[i], d.slider);
        if (i < 2)
        {
            d.slider.textFromValueFunction = [](double v) { return String(v * 100, 0) + "%"; };
            d.slider.valueFromTextFunction = [](const String &s) { return s.getDoubleValue() / 100; };
            d.slider.updateText();
        }
    }
    const char *ci[] = {"mode", "timing", "modifier", "band"};
    const char *cl[] = {"TRIGGER MODE", "CYCLE", "GROOVE", "BAND MODE"};
    for (size_t i = 0; i < choices.size(); ++i)
    {
        auto &c = choices[i];
        canvas.addAndMakeVisible(c.box);
        canvas.addAndMakeVisible(c.label);
        c.label.setText(cl[i], dontSendNotification);
        c.label.setColour(Label::textColourId, ui::muted);
        c.label.setFont(FontOptions(10.f));
        auto *param = dynamic_cast<AudioParameterChoice *>(p.state.getParameter(ci[i]));
        c.box.addItemList(param->choices, 1);
        c.attachment =
            std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, ci[i], c.box);
    }
    for (int i = 0; i < 16; ++i)
    {
        curves[(size_t)i] = std::make_unique<CurveButton>(p, i);
        canvas.addAndMakeVisible(*curves[(size_t)i]);
        curves[(size_t)i]->onClick = [this, i]
        {
            p.selectCurve(i);
            graph.repaint();
        };
    }
    for (auto *b : {&any, &retrigger, &input, &output, &kick})
        canvas.addAndMakeVisible(b);
    anyAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.state, "any", any);
    retriggerAttachment =
        std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.state, "retrigger", retrigger);
    layerAttachments[0] =
        std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.state, "viewInput", input);
    layerAttachments[1] =
        std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.state, "viewOutput", output);
    layerAttachments[2] =
        std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.state, "viewKick", kick);
    input.onClick = [this]
    {
        auto count = p.triggers.load();
        float phase = p.position.load();
        if (count != previousTrigger || (int(p.value("mode")) == 0 && phase < previousPhase))
            activity = 1;
        else
            activity *= .82f;
        previousTrigger = count;
        previousPhase = phase;
        graph.showInput = input.getToggleState();
    };
    output.onClick = [this] { graph.showOutput = output.getToggleState(); };
    kick.onClick = [this] { graph.showKick = kick.getToggleState(); };
    for (auto *b : {&save, &load, &resetButton, &resetCurve})
        canvas.addAndMakeVisible(b);
    save.onClick = [this] { preset(true); };
    load.onClick = [this] { preset(false); };
    resetButton.onClick = [this] { p.reset(); };
    resetCurve.onClick = [this] { p.selectCurve(int(p.value("curve"))); };
    for (auto *l : {&status, &gr, &bpmLabel})
    {
        canvas.addAndMakeVisible(l);
        l->setColour(Label::textColourId, ui::muted);
        l->setFont(11);
    }
    gr.setColour(Label::textColourId, ui::pink);
    gr.setFont(FontOptions(18.f));
#ifdef MXND3X_HAS_CHARACTER
    character =
        ImageCache::getFromMemory(BinaryData::MXND3XCharacter_png, BinaryData::MXND3XCharacter_pngSize);
#endif
    setResizable(true, true);
    setResizeLimits(900, 630, 1600, 1120);
    setSize(1000, 700);
    startTimerHz(30);
}
SidechainEditor::~SidechainEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}
void SidechainEditor::paint(Graphics &g)
{
    g.fillAll(ui::bg);
    auto scale = float(getWidth()) / 1000;
    g.addTransform(AffineTransform::scale(scale, float(getHeight()) / 700));
    g.setColour(ui::panel);
    g.fillRoundedRectangle(16, 84, 210, 550, 16);
    g.setColour(ui::purple);
    g.setFont(FontOptions(27, Font::bold));
    g.drawText("MXND3X", 26, 16, 240, 34, Justification::left);
    g.setFont(FontOptions(12, Font::bold));
    g.setColour(ui::text);
    g.drawText("S I D E C H A I N", 28, 49, 220, 20, Justification::left);
    g.setColour(ui::muted);
    g.setFont(FontOptions(11.f));
    g.drawText("VOLUME SHAPING / 01", 248, 78, 250, 25, Justification::left);
    if (character.isValid())
        g.drawImageWithin(character, 28, 101, 186, 172,
                          RectanglePlacement::centred | RectanglePlacement::onlyReduceInSize);
    else
    {
        g.setColour(ui::purple.withAlpha(.12f));
        g.fillRoundedRectangle(33, 104, 176, 160, 14);
        g.setColour(ui::purple);
        g.setFont(FontOptions(34, Font::bold));
        g.drawText("M", 33, 125, 176, 60, Justification::centred);
        g.setColour(ui::muted);
        g.setFont(FontOptions(11.f));
        g.drawText("ORIGINAL ART PENDING", 33, 193, 176, 26, Justification::centred);
    }
    g.setColour(ui::purple.withAlpha(.18f + activity * .82f));
    g.fillEllipse(780, 59, 7, 7);
    g.setColour(ui::surface);
    g.drawHorizontalLine(642, 20, 980);
    g.setColour(ui::muted);
    g.setFont(FontOptions(10.f));
    g.drawText("MXND3X  /  REALTIME VOLUME SHAPER  /  ZERO REPORTED LATENCY", 24, 670, 700, 20,
               Justification::left);
}
void SidechainEditor::resized()
{
    canvas.setBounds(getLocalBounds());
    canvas.setTransform(AffineTransform());
    canvas.setSize(1000, 700);
    canvas.setTransform(AffineTransform::scale(float(getWidth()) / 1000, float(getHeight()) / 700));
    graph.setBounds(248, 108, 728, 257);
    auto dial = [&](int i, int x, int y, int w, int h)
    {
        dials[(size_t)i].label.setBounds(x, y, w, 18);
        dials[(size_t)i].slider.setBounds(x, y + 18, w, h - 18);
    };
    dial(0, 40, 280, 162, 160);
    dial(1, 40, 438, 80, 82);
    dial(2, 125, 438, 80, 82);
    dial(3, 55, 566, 130, 72);
    for (int i = 0; i < 4; ++i)
    {
        auto &c = choices[(size_t)i];
        int x = i == 0 ? 380 : i == 1 ? 558 : i == 2 ? 670 : 38;
        int y = i == 3 ? 530 : 23;
        int w = i == 0 ? 164 : i == 3 ? 164 : 100;
        c.label.setBounds(x, y - 15, w, 15);
        c.box.setBounds(x, y, w, 30);
    }
    for (int i = 0; i < 16; ++i)
        curves[(size_t)i]->setBounds(248 + (i % 8) * 91, 403 + (i / 8) * 60, 89, 56);
    input.setBounds(246, 370, 85, 24);
    output.setBounds(332, 370, 90, 24);
    kick.setBounds(424, 370, 110, 24);
    gr.setBounds(710, 370, 265, 25);
    dial(4, 248, 540, 102, 95);
    dial(5, 355, 540, 102, 95);
    dial(6, 462, 540, 110, 95);
    dial(7, 598, 540, 96, 95);
    any.setBounds(711, 551, 120, 24);
    retrigger.setBounds(711, 583, 120, 24);
    save.setBounds(800, 15, 83, 26);
    load.setBounds(891, 15, 83, 26);
    resetButton.setBounds(891, 48, 83, 24);
    resetCurve.setBounds(855, 641, 120, 25);
    status.setBounds(245, 642, 600, 24);
    bpmLabel.setBounds(248, 24, 130, 28);
}
void SidechainEditor::timerCallback()
{
    graph.showInput = input.getToggleState();
    graph.showOutput = output.getToggleState();
    graph.showKick = kick.getToggleState();
    mx::MeterFrame frame;
    while (p.scope.pop(frame))
    {
        graph.history[graph.cursor] = frame;
        graph.cursor = (graph.cursor + 1) % graph.history.size();
    }
    graph.repaint();
    for (auto &c : curves)
        c->repaint();
    gr.setText("GR  -" + String(p.reduction.load(), 1) + " dB", dontSendNotification);
    bpmLabel.setText(String(p.currentBpm.load(), 1) + " BPM", dontSendNotification);
    String info = p.transportAvailable.load() ? "HOST TRANSPORT" : "NO PPQ / 120 BPM FALLBACK";
    if (int(p.value("mode")) == 1)
        info = p.sidechainConnected.load() ? "EXTERNAL SIDECHAIN BUS ENABLED"
                                           : "NO EXTERNAL SIDECHAIN / WAITING FOR ROUTING";
    if (p.value("modified") > .5f)
        info += "  /  CUSTOM";
    status.setText(info, dontSendNotification);
    const bool audio = int(p.value("mode")) == 1, midi = int(p.value("mode")) == 2;
    for (int i = 4; i <= 6; ++i)
        dials[(size_t)i].slider.setEnabled(audio);
    dials[7].slider.setEnabled(midi);
    any.setEnabled(midi);
    retrigger.setEnabled(audio || midi);
    repaint();
}
void SidechainEditor::preset(bool saving)
{
    chooser =
        std::make_unique<FileChooser>(saving ? "Save MXND3X preset" : "Load MXND3X preset",
                                      File::getSpecialLocation(File::userDocumentsDirectory), "*.mxpreset");
    Component::SafePointer<SidechainEditor> safe(this);
    chooser->launchAsync(
        (saving ? FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting
                : FileBrowserComponent::openMode) |
            FileBrowserComponent::canSelectFiles,
        [safe, saving](const FileChooser &f)
        {
            if (!safe)
                return;
            auto file = f.getResult();
            if (file == File())
                return;
            if (saving)
            {
                MemoryBlock b;
                safe->p.getStateInformation(b);
                if (!file.withFileExtension("mxpreset").replaceWithData(b.getData(), b.getSize()))
                    safe->status.setText("PRESET SAVE FAILED", dontSendNotification);
            }
            else
            {
                MemoryBlock b;
                if (file.loadFileAsData(b) && b.getSize() < 1024 * 1024)
                {
                    auto xml = AudioProcessor::getXmlFromBinary(b.getData(), int(b.getSize()));
                    if (xml && xml->hasTagName("MXND3X"))
                        safe->p.setStateInformation(b.getData(), int(b.getSize()));
                    else
                        safe->status.setText("INVALID PRESET", dontSendNotification);
                }
            }
        });
}
