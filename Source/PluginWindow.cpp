#include "JuceHeader.h"
#include "PluginWindow.h"

static OwnedArray<PluginWindow> activePluginWindows;

PluginWindow::PluginWindow (Component* const pluginEditor,
                            AudioProcessorGraph::Node* const o,
                            WindowFormatType t)
    : DocumentWindow (pluginEditor != nullptr ? pluginEditor->getName() : "Plugin",
                      Colours::lightgrey,
                      DocumentWindow::minimiseButton | DocumentWindow::closeButton),
      owner (o),
      type (t)
{
    setSize (400, 300);
    setUsingNativeTitleBar (true);
    if (auto* const ed = dynamic_cast<AudioProcessorEditor*> (pluginEditor))
    {
        if (ed->isResizable())
            setResizable (true, false);
    }
    setContentOwned (pluginEditor, true);

    const int defaultX = Random::getSystemRandom().nextInt (300) + 50;
    const int defaultY = Random::getSystemRandom().nextInt (300) + 50;

    if (owner != nullptr)
    {
        setTopLeftPosition (owner->properties.getWithDefault (getLastXProp (type), defaultX),
                            owner->properties.getWithDefault (getLastYProp (type), defaultY));
        owner->properties.set (getOpenProp (type), true);
    }
    else
    {
        setTopLeftPosition (defaultX, defaultY);
    }

    setVisible (true);
}

PluginWindow::~PluginWindow()
{
    clearContentComponent();
}

void PluginWindow::closeCurrentlyOpenWindowsFor (const uint32 nodeUid)
{
    for (int i = activePluginWindows.size(); --i >= 0;)
    {
        if (auto* w = activePluginWindows.getUnchecked (i))
        {
            if (w->owner != nullptr && w->owner->nodeID.uid == nodeUid)
                activePluginWindows.remove (i);
        }
    }
}

void PluginWindow::closeAllCurrentlyOpenWindows()
{
    activePluginWindows.clear();
}

bool PluginWindow::containsActiveWindows()
{
    return activePluginWindows.size() > 0;
}

//==============================================================================
class ProcessorProgramPropertyComp : public PropertyComponent,
                                     private AudioProcessorListener
{
public:
    ProcessorProgramPropertyComp (const String& name, AudioProcessor& p)
        : PropertyComponent (name),
          owner (p)
    {
        owner.addListener (this);
    }

    ~ProcessorProgramPropertyComp() override
    {
        owner.removeListener (this);
    }

    void refresh() override {}
    void audioProcessorChanged (AudioProcessor*, const ChangeDetails&) override {}
    void audioProcessorParameterChanged (AudioProcessor*, int, float) override {}

private:
    AudioProcessor& owner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessorProgramPropertyComp)
};

class ProgramAudioProcessorEditor : public AudioProcessorEditor
{
public:
    explicit ProgramAudioProcessorEditor (AudioProcessor* const p)
        : AudioProcessorEditor (p)
    {
        jassert (p != nullptr);
        setOpaque (true);

        addAndMakeVisible (panel);

        Array<PropertyComponent*> programs;
        const int numPrograms = p->getNumPrograms();
        int totalHeight = 0;

        for (int i = 0; i < numPrograms; ++i)
        {
            String name (p->getProgramName (i).trim());
            if (name.isEmpty())
                name = "Unnamed";

            auto* pc = new ProcessorProgramPropertyComp (name, *p);
            programs.add (pc);
            totalHeight += pc->getPreferredHeight();
        }

        panel.addProperties (programs);
        setSize (400, jlimit (25, 400, totalHeight));
    }

    void paint (Graphics& g) override
    {
        g.fillAll (Colours::grey);
    }

    void resized() override
    {
        panel.setBounds (getLocalBounds());
    }

private:
    PropertyPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgramAudioProcessorEditor)
};

//==============================================================================
PluginWindow* PluginWindow::getWindowFor (AudioProcessorGraph::Node* const node,
                                          WindowFormatType type)
{
    if (node == nullptr)
        return nullptr;

    for (auto* w : activePluginWindows)
    {
        if (w->owner == node && w->type == type)
            return w;
    }

    AudioProcessor* processor = node->getProcessor();
    if (processor == nullptr)
        return nullptr;

    std::unique_ptr<AudioProcessorEditor> ui;

    if (type == Normal)
    {
        ui.reset (processor->createEditorAndMakeActive());
        if (ui == nullptr)
            type = Generic;
    }

    if (ui == nullptr)
    {
        if (type == Generic || type == Parameters)
            ui = std::make_unique<GenericAudioProcessorEditor> (*processor);
        else if (type == Programs)
            ui = std::make_unique<ProgramAudioProcessorEditor> (processor);
    }

    if (ui != nullptr)
    {
        if (auto* const plugin = dynamic_cast<AudioPluginInstance*> (processor))
            ui->setName (plugin->getName());

        auto* window = new PluginWindow (ui.release(), node, type);
        activePluginWindows.add (window);
        return window;
    }

    return nullptr;
}

void PluginWindow::moved()
{
    if (owner != nullptr)
    {
        owner->properties.set (getLastXProp (type), getX());
        owner->properties.set (getLastYProp (type), getY());
    }
}

void PluginWindow::closeButtonPressed()
{
    if (owner != nullptr)
        owner->properties.set (getOpenProp (type), false);

    setVisible (false);

    MessageManager::callAsync ([this]()
    {
        for (int i = activePluginWindows.size(); --i >= 0;)
        {
            if (activePluginWindows.getUnchecked (i) == this)
            {
                activePluginWindows.remove (i);
                break;
            }
        }
    });
}
