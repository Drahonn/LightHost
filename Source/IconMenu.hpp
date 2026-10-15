#pragma once

#include "JuceHeader.h"
#include <vector>
#include <memory>

ApplicationProperties& getAppProperties();

class IconMenu : public SystemTrayIconComponent,
                 private Timer,
                 public ChangeListener
{
public:
    IconMenu();
    ~IconMenu() override;

    void mouseDown (const MouseEvent&) override;
    void menuInvocationCallback (int id);
    void changeListenerCallback (ChangeBroadcaster* changed) override;

    static String getKey (const String& type, const PluginDescription& plugin);

    static constexpr int INDEX_EDIT      = 1000000;
    static constexpr int INDEX_BYPASS    = 2000000;
    static constexpr int INDEX_DELETE    = 3000000;
    static constexpr int INDEX_MOVE_UP   = 4000000;
    static constexpr int INDEX_MOVE_DOWN = 5000000;

    static constexpr uint32 INPUT_NODE_ID  = 1000000;
    static constexpr uint32 OUTPUT_NODE_ID = 1000001;

private:
    void timerCallback() override;
    void reloadPlugins();
    void showAudioSettings();
    void loadActivePlugins();
    void updateGraphConnections();
    void savePluginStates();
    void deletePluginStates();
    void saveActivePluginsToSettings();
    void loadActivePluginsFromSettings();
    void removePluginsLackingInputOutput();
    void notifyIfMonoPlugin (const PluginDescription& plugin);
    void setIcon();

    AudioDeviceManager deviceManager;
    AudioPluginFormatManager formatManager;
    KnownPluginList knownPluginList;
    std::vector<PluginDescription> activePlugins;
    KnownPluginList::SortMethod pluginSortMethod;
    PopupMenu menu;
    bool menuIconLeftClicked { false };

    AudioProcessorGraph graph;
    AudioProcessorPlayer player;
    AudioProcessorGraph::Node::Ptr inputNode;
    AudioProcessorGraph::Node::Ptr outputNode;

    class PluginListWindow;
    std::unique_ptr<PluginListWindow> pluginListWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IconMenu)
};
