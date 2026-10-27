#include "PluginProcessor.h"
class Capture : public juce::JUCEApplication, private juce::Timer
{
    std::unique_ptr<SidechainProcessor> processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;

  public:
    const juce::String getApplicationName() override { return "MXND3X Capture"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    void initialise(const juce::String &) override
    {
        processor = std::make_unique<SidechainProcessor>();
        editor.reset(processor->createEditor());
        editor->addToDesktop(0);
        editor->setVisible(true);
        startTimer(600);
    }
    void timerCallback() override
    {
        stopTimer();
        auto image = editor->createComponentSnapshot(editor->getLocalBounds());
        juce::File file(juce::File::getCurrentWorkingDirectory().getChildFile("interface.png"));
        juce::FileOutputStream stream(file);
        if (!stream.openedOk())
        {
            setApplicationReturnValue(1);
            quit();
            return;
        }
        stream.setPosition(0);
        stream.truncate();
        juce::PNGImageFormat png;
        if (!png.writeImageToStream(image, stream))
            setApplicationReturnValue(1);
        quit();
    }
    void shutdown() override
    {
        editor.reset();
        processor.reset();
    }
};
START_JUCE_APPLICATION(Capture)
