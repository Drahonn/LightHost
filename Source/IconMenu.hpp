#pragma once

#include "JuceHeader.h"
#include <vector>
#include <memory>
#include <atomic>

ApplicationProperties& getAppProperties();

struct ActivePlugin
{
    PluginDescription desc;
    String instanceId;
    bool bypassed = false;
    MemoryBlock stateData;
};

class IconMenu : public SystemTrayIconComponent,
                 private Timer,
                 public ChangeListener
{
public:
    IconMenu();
    ~IconMenu() override;

    void mouseDown (const MouseEvent&) override;
    void mouseUp (const MouseEvent&) override;
    void menuInvocationCallback (int id);
    void changeListenerCallback (ChangeBroadcaster* changed) override;

    static String getKey (const String& type, const PluginDescription& plugin);

    static constexpr int CMD_PREFERENCES      = 100000;
    static constexpr int CMD_EDIT_PLUGINS     = 100001;
    static constexpr int CMD_SCAN_PLUGINS     = 100002;
    static constexpr int CMD_ADD_FOLDER       = 100003;
    static constexpr int CMD_CLEAR_RESCAN     = 100004;
    static constexpr int CMD_RESTART_AUDIO    = 100005;

    static constexpr int CMD_QUIT             = 100010;
    static constexpr int CMD_DELETE_STATES    = 100011;
    static constexpr int CMD_INVERT_ICON      = 100012;
    static constexpr int CMD_OPEN_SETTINGS    = 100013;
    static constexpr int CMD_RESET_SETTINGS   = 100014;

    static constexpr int INDEX_EDIT           = 1000000;
    static constexpr int INDEX_BYPASS         = 2000000;
    static constexpr int INDEX_DELETE         = 3000000;
    static constexpr int INDEX_MOVE_UP        = 4000000;
    static constexpr int INDEX_MOVE_DOWN      = 5000000;

    static constexpr uint32 INPUT_NODE_ID     = 1000000;
    static constexpr uint32 OUTPUT_NODE_ID    = 1000001;

private:
    void timerCallback() override;
    void reloadPlugins();
    void showAudioSettings();
    void restartAudioEngine();
    void resetAllSettings();
    void openSettingsFolder();
    void loadActivePlugins();
    void updateGraphConnections();
    void savePluginStates();
    void deletePluginStates();
    void saveActivePluginsToSettings();
    void loadActivePluginsFromSettings();
    void removePluginsLackingInputOutput();
    void notifyIfMonoPlugin (const PluginDescription& plugin);
    void setIcon();

    // Plugin Scanning & Custom Folders
    void startPluginScan (bool rescanAll, const File& specificFolder = {});
    void onPluginScanFinished (int newCount, int totalCount);
    void chooseCustomPluginFolder();
    void addCustomPluginPath (const String& path);
    StringArray getCustomPluginPaths() const;

    AudioDeviceManager deviceManager;
    AudioPluginFormatManager formatManager;
    KnownPluginList knownPluginList;
    std::vector<ActivePlugin> activePlugins;
    KnownPluginList::SortMethod pluginSortMethod;
    PopupMenu menu;

    AudioProcessorGraph graph;
    AudioProcessorPlayer player;
    AudioProcessorGraph::Node::Ptr inputNode;
    AudioProcessorGraph::Node::Ptr outputNode;

    class PluginScanThread;
    std::unique_ptr<PluginScanThread> scanThread;
    std::atomic<bool> isScanning { false };
    std::unique_ptr<FileChooser> folderChooser;

    class PluginListWindow;
    std::unique_ptr<PluginListWindow> pluginListWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IconMenu)
};

