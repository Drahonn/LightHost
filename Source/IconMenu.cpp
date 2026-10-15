#include "JuceHeader.h"
#include "IconMenu.hpp"
#include "PluginWindow.h"
#include <algorithm>

//==============================================================================
class IconMenu::PluginScanThread : public Thread
{
public:
    PluginScanThread (IconMenu& owner_,
                      AudioPluginFormatManager& fm,
                      KnownPluginList& kpl,
                      FileSearchPath searchPaths_,
                      bool rescanAll_)
        : Thread ("PluginScanThread"),
          owner (owner_),
          formatManager (fm),
          knownPluginList (kpl),
          searchPaths (searchPaths_),
          rescanAll (rescanAll_)
    {
    }

    ~PluginScanThread() override
    {
        stopThread (4000);
    }

    void run() override
    {
        const File deadMansPedalFile = getAppProperties().getUserSettings()
                                       ->getFile().getSiblingFile ("RecentlyCrashedPluginsList");

        if (rescanAll)
            deadMansPedalFile.deleteFile();

        const int initialCount = knownPluginList.getTypes().size();

        for (int i = 0; i < formatManager.getNumFormats(); ++i)
        {
            if (threadShouldExit())
                break;

            auto* format = formatManager.getFormat (i);
            if (format == nullptr)
                continue;

            FileSearchPath pathsToScan = searchPaths;
            if (pathsToScan.getNumPaths() == 0)
            {
                pathsToScan = format->getDefaultLocationsToSearch();
                for (const auto& p : owner.getCustomPluginPaths())
                    pathsToScan.add (File (p));
            }

            PluginDirectoryScanner scanner (knownPluginList,
                                           *format,
                                           pathsToScan,
                                           true, // search sub-directories recursively
                                           deadMansPedalFile,
                                           ! rescanAll);

            String pluginBeingScanned;
            while (! threadShouldExit() && scanner.scanNextFile (true, pluginBeingScanned))
            {
            }
        }

        const int finalCount = knownPluginList.getTypes().size();
        const int newPluginsFound = finalCount - initialCount;

        MessageManager::callAsync ([this, newPluginsFound, finalCount]()
        {
            owner.onPluginScanFinished (newPluginsFound, finalCount);
        });
    }

private:
    IconMenu& owner;
    AudioPluginFormatManager& formatManager;
    KnownPluginList& knownPluginList;
    FileSearchPath searchPaths;
    bool rescanAll;
};

//==============================================================================
class IconMenu::PluginListWindow : public DocumentWindow
{
public:
    PluginListWindow (IconMenu& owner_, AudioPluginFormatManager& pluginFormatManager)
        : DocumentWindow ("Available Plugins", Colours::white,
                          DocumentWindow::minimiseButton | DocumentWindow::closeButton),
          owner (owner_)
    {
        const File deadMansPedalFile (getAppProperties().getUserSettings()
                                      ->getFile().getSiblingFile ("RecentlyCrashedPluginsList"));

        setContentOwned (new PluginListComponent (pluginFormatManager,
                                                 owner.knownPluginList,
                                                 deadMansPedalFile,
                                                 getAppProperties().getUserSettings()), true);

        setUsingNativeTitleBar (true);
        setResizable (true, false);
        setResizeLimits (300, 400, 800, 1500);
        setTopLeftPosition (60, 60);

        restoreWindowStateFromString (getAppProperties().getUserSettings()->getValue ("listWindowPos"));
        setVisible (true);
    }

    ~PluginListWindow() override
    {
        getAppProperties().getUserSettings()->setValue ("listWindowPos", getWindowStateAsString());
        clearContentComponent();
    }

    void closeButtonPressed() override
    {
        owner.removePluginsLackingInputOutput();
        #if JUCE_MAC
        Process::setDockIconVisible (false);
        #endif
        owner.pluginListWindow = nullptr;
    }

private:
    IconMenu& owner;
};

//==============================================================================
IconMenu::IconMenu()
{
    juce::addDefaultFormatsToManager (formatManager);

    std::unique_ptr<XmlElement> savedAudioState (getAppProperties().getUserSettings()->getXmlValue ("audioDeviceState"));
    deviceManager.initialise (256, 256, savedAudioState.get(), true);
    deviceManager.addChangeListener (this);

    player.setProcessor (&graph);
    deviceManager.addAudioCallback (&player);

    std::unique_ptr<XmlElement> savedPluginList (getAppProperties().getUserSettings()->getXmlValue ("pluginList"));
    if (savedPluginList != nullptr)
        knownPluginList.recreateFromXml (*savedPluginList);

    pluginSortMethod = KnownPluginList::sortByManufacturer;
    knownPluginList.addChangeListener (this);

    loadActivePluginsFromSettings();
    loadActivePlugins();

    setIcon();
    if (auto* app = JUCEApplication::getInstance())
        setIconTooltip (app->getApplicationName());

    // Auto-scan default plugin paths in background on first run if list is empty
    if (knownPluginList.getTypes().isEmpty())
    {
        startPluginScan (false);
    }
}

IconMenu::~IconMenu()
{
    if (scanThread != nullptr)
        scanThread->stopThread (4000);

    deviceManager.removeAudioCallback (&player);
    deviceManager.removeChangeListener (this);
    savePluginStates();
    graph.clear();
}

void IconMenu::setIcon()
{
    Image icon;
#if JUCE_MAC
    const bool isDark = Desktop::getInstance().isDarkModeActive();
    if (isDark)
        icon = ImageFileFormat::loadFrom (BinaryData::menu_icon_white_png, BinaryData::menu_icon_white_pngSize);
    else
        icon = ImageFileFormat::loadFrom (BinaryData::menu_icon_png, BinaryData::menu_icon_pngSize);
    setIconImage (icon, icon);
#else
    String defaultColor;
#if JUCE_WINDOWS
    defaultColor = "white";
#elif JUCE_LINUX
    defaultColor = "black";
#endif
    if (! getAppProperties().getUserSettings()->containsKey ("icon"))
        getAppProperties().getUserSettings()->setValue ("icon", defaultColor);

    String color = getAppProperties().getUserSettings()->getValue ("icon");
    if (color.equalsIgnoreCase ("white"))
        icon = ImageFileFormat::loadFrom (BinaryData::menu_icon_white_png, BinaryData::menu_icon_white_pngSize);
    else
        icon = ImageFileFormat::loadFrom (BinaryData::menu_icon_png, BinaryData::menu_icon_pngSize);

    setIconImage (icon, icon);
#endif
}

void IconMenu::loadActivePluginsFromSettings()
{
    activePlugins.clear();

    // Check modern XML list representation first
    std::unique_ptr<XmlElement> xml (getAppProperties().getUserSettings()->getXmlValue ("activePluginsXml"));
    if (xml != nullptr)
    {
        for (auto* child : xml->getChildIterator())
        {
            PluginDescription desc;
            if (desc.loadFromXml (*child))
                activePlugins.push_back (desc);
        }
        return;
    }

    // Fallback to legacy KnownPluginList storage for backwards compatibility
    std::unique_ptr<XmlElement> savedPluginListActive (getAppProperties().getUserSettings()->getXmlValue ("pluginListActive"));
    if (savedPluginListActive != nullptr)
    {
        KnownPluginList oldList;
        oldList.recreateFromXml (*savedPluginListActive);

        std::vector<std::pair<int, PluginDescription>> ordered;
        const auto& types = oldList.getTypes();
        for (int i = 0; i < types.size(); ++i)
        {
            const auto& desc = types.getReference (i);
            int order = getAppProperties().getUserSettings()->getIntValue (getKey ("order", desc), i);
            ordered.push_back ({ order, desc });
        }

        std::sort (ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

        for (const auto& item : ordered)
            activePlugins.push_back (item.second);
    }
}

void IconMenu::saveActivePluginsToSettings()
{
    auto xml = std::make_unique<XmlElement> ("ACTIVE_PLUGINS");
    for (size_t i = 0; i < activePlugins.size(); ++i)
    {
        if (auto child = activePlugins[i].createXml())
            xml->addChildElement (child.release());

        getAppProperties().getUserSettings()->setValue (getKey ("order", activePlugins[i]), (int) i);
    }

    getAppProperties().getUserSettings()->setValue ("activePluginsXml", xml.get());
    getAppProperties().saveIfNeeded();
}

void IconMenu::loadActivePlugins()
{
    PluginWindow::closeAllCurrentlyOpenWindows();
    graph.clear();

    inputNode = graph.addNode (std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor> (
                                   AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode),
                               AudioProcessorGraph::NodeID (INPUT_NODE_ID));
    outputNode = graph.addNode (std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor> (
                                    AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode),
                                AudioProcessorGraph::NodeID (OUTPUT_NODE_ID));

    for (size_t i = 0; i < activePlugins.size(); ++i)
    {
        const auto& plugin = activePlugins[i];
        String errorMessage;
        auto instance = formatManager.createPluginInstance (plugin, graph.getSampleRate(), graph.getBlockSize(), errorMessage);

        if (instance != nullptr)
        {
            const String pluginUid = getKey ("state", plugin);
            const String savedPluginState = getAppProperties().getUserSettings()->getValue (pluginUid);
            if (savedPluginState.isNotEmpty())
            {
                MemoryBlock savedPluginBinary;
                savedPluginBinary.fromBase64Encoding (savedPluginState);
                instance->setStateInformation (savedPluginBinary.getData(), (int) savedPluginBinary.getSize());
            }

            const uint32 nodeIdValue = (uint32) (i + 1);
            graph.addNode (std::move (instance), AudioProcessorGraph::NodeID (nodeIdValue));
        }
    }

    updateGraphConnections();
}

void IconMenu::updateGraphConnections()
{
    for (const auto& c : graph.getConnections())
        graph.removeConnection (c);

    constexpr int CHANNEL_LEFT  = 0;
    constexpr int CHANNEL_RIGHT = 1;

    AudioProcessorGraph::NodeID currentSource = AudioProcessorGraph::NodeID (INPUT_NODE_ID);
    int currentSourceChannels = 2; // Audio input defaults to stereo

    for (size_t i = 0; i < activePlugins.size(); ++i)
    {
        const auto& plugin = activePlugins[i];
        const String bypassKey = getKey ("bypass", plugin);
        const bool bypass = getAppProperties().getUserSettings()->getBoolValue (bypassKey, false);

        const AudioProcessorGraph::NodeID pluginNodeId ((uint32) (i + 1));
        auto* node = graph.getNodeForId (pluginNodeId);

        if (node == nullptr || bypass)
            continue;

        const int numInputs  = plugin.numInputChannels;
        const int numOutputs = plugin.numOutputChannels;

        // Connect source to plugin inputs
        if (numInputs >= 2)
        {
            graph.addConnection ({ { currentSource, CHANNEL_LEFT },  { pluginNodeId, CHANNEL_LEFT } });
            if (currentSourceChannels >= 2)
                graph.addConnection ({ { currentSource, CHANNEL_RIGHT }, { pluginNodeId, CHANNEL_RIGHT } });
            else
                graph.addConnection ({ { currentSource, CHANNEL_LEFT },  { pluginNodeId, CHANNEL_RIGHT } });
        }
        else if (numInputs == 1)
        {
            graph.addConnection ({ { currentSource, CHANNEL_LEFT }, { pluginNodeId, CHANNEL_LEFT } });
        }
        // If numInputs == 0 (Synth/Generator), no input connection needed

        currentSource = pluginNodeId;
        currentSourceChannels = numOutputs > 0 ? numOutputs : 2;
    }

    // Connect last node in chain to audio output
    const AudioProcessorGraph::NodeID destNode (OUTPUT_NODE_ID);
    if (currentSourceChannels >= 2)
    {
        graph.addConnection ({ { currentSource, CHANNEL_LEFT },  { destNode, CHANNEL_LEFT } });
        graph.addConnection ({ { currentSource, CHANNEL_RIGHT }, { destNode, CHANNEL_RIGHT } });
    }
    else
    {
        // Mono to stereo routing: duplicate mono signal to both Left and Right output channels
        graph.addConnection ({ { currentSource, CHANNEL_LEFT }, { destNode, CHANNEL_LEFT } });
        graph.addConnection ({ { currentSource, CHANNEL_LEFT }, { destNode, CHANNEL_RIGHT } });
    }
}

void IconMenu::changeListenerCallback (ChangeBroadcaster* changed)
{
    if (changed == &knownPluginList)
    {
        std::unique_ptr<XmlElement> savedPluginList (knownPluginList.createXml());
        if (savedPluginList != nullptr)
        {
            getAppProperties().getUserSettings()->setValue ("pluginList", savedPluginList.get());
            getAppProperties().saveIfNeeded();
        }
    }
    else if (changed == &deviceManager)
    {
        std::unique_ptr<XmlElement> audioState (deviceManager.createStateXml());
        if (audioState != nullptr)
        {
            getAppProperties().getUserSettings()->setValue ("audioDeviceState", audioState.get());
            getAppProperties().saveIfNeeded();
        }
    }
}

void IconMenu::timerCallback()
{
    stopTimer();
    menu.clear();

    const String appName = JUCEApplication::getInstance() != nullptr
                               ? JUCEApplication::getInstance()->getApplicationName()
                               : "Light Host";
    menu.addSectionHeader (appName);

    if (menuIconLeftClicked)
    {
        menu.addItem (CMD_PREFERENCES, "Audio Settings");
        menu.addItem (CMD_EDIT_PLUGINS, "Edit Plugins List...");
        menu.addSeparator();

        menu.addSectionHeader ("Plugin Scanner");
        if (isScanning.load())
        {
            menu.addItem (-1, "Scanning in progress...", false);
        }
        else
        {
            menu.addItem (CMD_SCAN_PLUGINS, "Scan for Plugins");
            menu.addItem (CMD_ADD_FOLDER, "Add Plugin Folder...");
            menu.addItem (CMD_CLEAR_RESCAN, "Clear & Rescan All");
        }

        menu.addSeparator();
        menu.addSectionHeader ("Active Plugins");

        for (size_t i = 0; i < activePlugins.size(); ++i)
        {
            const auto& plugin = activePlugins[i];
            const int idx = (int) i;

            PopupMenu options;
            options.addItem (INDEX_EDIT + idx, "Edit");

            const String bypassKey = getKey ("bypass", plugin);
            const bool bypass = getAppProperties().getUserSettings()->getBoolValue (bypassKey, false);
            options.addItem (INDEX_BYPASS + idx, "Bypass", true, bypass);
            options.addSeparator();
            options.addItem (INDEX_MOVE_UP + idx, "Move Up", idx > 0);
            options.addItem (INDEX_MOVE_DOWN + idx, "Move Down", idx < (int) activePlugins.size() - 1);
            options.addSeparator();
            options.addItem (INDEX_DELETE + idx, "Delete");

            String displayName = plugin.name;
            if (plugin.numOutputChannels == 1 || plugin.numInputChannels == 1)
                displayName += " [Mono]";

            menu.addSubMenu (displayName, options);
        }

        menu.addSeparator();
        menu.addSectionHeader ("Available Plugins");
        if (knownPluginList.getTypes().isEmpty())
        {
            if (isScanning.load())
                menu.addItem (-2, "(Scanning in progress...)", false);
            else
                menu.addItem (-2, "(No plugins found - click 'Scan for Plugins' above)", false);
        }
        else
        {
            KnownPluginList::addToMenu (menu, knownPluginList.getTypes(), pluginSortMethod);
        }
    }
    else
    {
        menu.addItem (CMD_QUIT, "Quit");
        menu.addSeparator();
        menu.addItem (CMD_DELETE_STATES, "Delete Plugin States");
        #if ! JUCE_MAC
        menu.addItem (CMD_INVERT_ICON, "Invert Icon Color");
        #endif
    }

    PopupMenu::Options popupOptions;
    #if JUCE_MAC || JUCE_LINUX
    popupOptions = popupOptions.withTargetComponent (this);
    #else
    popupOptions = popupOptions.withMousePosition();
    #endif

    menu.showMenuAsync (popupOptions, [this](int result)
    {
        menuInvocationCallback (result);
    });
}

void IconMenu::mouseDown (const MouseEvent& e)
{
    #if JUCE_MAC
    Process::setDockIconVisible (true);
    #endif
    Process::makeForegroundProcess();
    menuIconLeftClicked = e.mods.isLeftButtonDown();
    startTimer (50);
}

void IconMenu::menuInvocationCallback (int id)
{
    if (id <= 0)
    {
        #if JUCE_MAC
        if (! PluginWindow::containsActiveWindows())
            Process::setDockIconVisible (false);
        #endif
        return;
    }

    // Right-click menu options
    if (id == CMD_QUIT)
    {
        savePluginStates();
        if (auto* app = JUCEApplication::getInstance())
            app->quit();
        return;
    }
    if (id == CMD_DELETE_STATES)
    {
        deletePluginStates();
        loadActivePlugins();
        return;
    }
    if (id == CMD_INVERT_ICON)
    {
        String color = getAppProperties().getUserSettings()->getValue ("icon");
        getAppProperties().getUserSettings()->setValue ("icon", color.equalsIgnoreCase ("black") ? "white" : "black");
        setIcon();
        return;
    }

    // Left-click management commands
    if (id == CMD_PREFERENCES)
    {
        showAudioSettings();
        return;
    }
    if (id == CMD_EDIT_PLUGINS)
    {
        reloadPlugins();
        return;
    }
    if (id == CMD_SCAN_PLUGINS)
    {
        startPluginScan (false);
        return;
    }
    if (id == CMD_ADD_FOLDER)
    {
        chooseCustomPluginFolder();
        return;
    }
    if (id == CMD_CLEAR_RESCAN)
    {
        knownPluginList.clear();
        startPluginScan (true);
        return;
    }

    // Active plugins manipulation
    if (id >= INDEX_DELETE && id < INDEX_DELETE + 1000000)
    {
        savePluginStates();
        const int index = id - INDEX_DELETE;
        if (index >= 0 && index < (int) activePlugins.size())
        {
            PluginWindow::closeCurrentlyOpenWindowsFor ((uint32) (index + 1));
            const auto plugin = activePlugins[(size_t) index];
            getAppProperties().getUserSettings()->removeValue (getKey ("bypass", plugin));
            getAppProperties().getUserSettings()->removeValue (getKey ("order", plugin));
            getAppProperties().getUserSettings()->removeValue (getKey ("state", plugin));

            activePlugins.erase (activePlugins.begin() + index);
            saveActivePluginsToSettings();
            loadActivePlugins();
        }
        startTimer (50);
        return;
    }

    if (id >= INDEX_BYPASS && id < INDEX_BYPASS + 1000000)
    {
        const int index = id - INDEX_BYPASS;
        if (index >= 0 && index < (int) activePlugins.size())
        {
            const auto& plugin = activePlugins[(size_t) index];
            const String key = getKey ("bypass", plugin);
            const bool bypassed = getAppProperties().getUserSettings()->getBoolValue (key, false);
            getAppProperties().getUserSettings()->setValue (key, ! bypassed);
            getAppProperties().saveIfNeeded();

            updateGraphConnections();
        }
        startTimer (50);
        return;
    }

    if (id >= INDEX_EDIT && id < INDEX_EDIT + 1000000)
    {
        const int index = id - INDEX_EDIT;
        if (index >= 0 && index < (int) activePlugins.size())
        {
            if (auto* const node = graph.getNodeForId (AudioProcessorGraph::NodeID ((uint32) (index + 1))))
            {
                if (auto* const w = PluginWindow::getWindowFor (node, PluginWindow::Normal))
                    w->toFront (true);
            }
        }
        startTimer (50);
        return;
    }

    if (id >= INDEX_MOVE_UP && id < INDEX_MOVE_UP + 1000000)
    {
        const int index = id - INDEX_MOVE_UP;
        if (index > 0 && index < (int) activePlugins.size())
        {
            savePluginStates();
            std::swap (activePlugins[(size_t) index], activePlugins[(size_t) (index - 1)]);
            saveActivePluginsToSettings();
            loadActivePlugins();
        }
        startTimer (50);
        return;
    }

    if (id >= INDEX_MOVE_DOWN && id < INDEX_MOVE_DOWN + 1000000)
    {
        const int index = id - INDEX_MOVE_DOWN;
        if (index >= 0 && index < (int) activePlugins.size() - 1)
        {
            savePluginStates();
            std::swap (activePlugins[(size_t) index], activePlugins[(size_t) (index + 1)]);
            saveActivePluginsToSettings();
            loadActivePlugins();
        }
        startTimer (50);
        return;
    }

    // Available plugin selection from menu
    const int chosenIndex = KnownPluginList::getIndexChosenByMenu (knownPluginList.getTypes(), id);
    if (chosenIndex > -1 && chosenIndex < knownPluginList.getTypes().size())
    {
        savePluginStates();
        PluginDescription plugin = knownPluginList.getTypes().getReference (chosenIndex);
        activePlugins.push_back (plugin);
        saveActivePluginsToSettings();
        loadActivePlugins();
        notifyIfMonoPlugin (plugin);
        startTimer (50);
        return;
    }
}

String IconMenu::getKey (const String& type, const PluginDescription& plugin)
{
    return "plugin-" + type.toLowerCase() + "-" + plugin.name + plugin.version + plugin.pluginFormatName;
}

void IconMenu::deletePluginStates()
{
    for (const auto& plugin : activePlugins)
    {
        const String pluginUid = getKey ("state", plugin);
        getAppProperties().getUserSettings()->removeValue (pluginUid);
    }
    getAppProperties().saveIfNeeded();
}

void IconMenu::savePluginStates()
{
    for (size_t i = 0; i < activePlugins.size(); ++i)
    {
        const auto& plugin = activePlugins[i];
        if (auto* node = graph.getNodeForId (AudioProcessorGraph::NodeID ((uint32) (i + 1))))
        {
            if (auto* processor = node->getProcessor())
            {
                MemoryBlock savedStateBinary;
                processor->getStateInformation (savedStateBinary);
                const String pluginUid = getKey ("state", plugin);
                getAppProperties().getUserSettings()->setValue (pluginUid, savedStateBinary.toBase64Encoding());
            }
        }
    }
    getAppProperties().saveIfNeeded();
}

void IconMenu::showAudioSettings()
{
    auto audioSettingsComp = std::make_unique<AudioDeviceSelectorComponent> (
        deviceManager, 0, 256, 0, 256, false, false, true, false);
    audioSettingsComp->setSize (500, 600);

    DialogWindow::LaunchOptions o;
    o.content.set (audioSettingsComp.release(), true);
    o.dialogTitle                   = "Audio Settings";
    o.componentToCentreAround       = this;
    o.dialogBackgroundColour        = LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId);
    o.escapeKeyTriggersCloseButton  = true;
    o.useNativeTitleBar             = true;
    o.resizable                     = false;

    o.launchAsync();
}

void IconMenu::reloadPlugins()
{
    if (pluginListWindow == nullptr)
        pluginListWindow = std::make_unique<PluginListWindow> (*this, formatManager);

    pluginListWindow->toFront (true);
}

void IconMenu::removePluginsLackingInputOutput()
{
    Array<PluginDescription> toRemove;
    for (const auto& plugin : knownPluginList.getTypes())
    {
        if (plugin.numOutputChannels < 1)
            toRemove.add (plugin);
    }

    for (const auto& plugin : toRemove)
        knownPluginList.removeType (plugin);
}

void IconMenu::notifyIfMonoPlugin (const PluginDescription& plugin)
{
    if (plugin.numOutputChannels == 1 || plugin.numInputChannels == 1)
    {
        AlertWindow::showMessageBoxAsync (
            AlertWindow::WarningIcon,
            "Mono Plugin Routed to Stereo",
            "Warning: '" + plugin.name + "' is a mono plugin.\n"
            "Its audio output will be automatically routed to both Left and Right stereo output channels.",
            "OK");
    }
}

void IconMenu::startPluginScan (bool rescanAll, const File& specificFolder)
{
    if (isScanning.load())
    {
        AlertWindow::showMessageBoxAsync (
            AlertWindow::InfoIcon,
            "Scan in Progress",
            "A plugin scan is already in progress. Please wait for it to complete.",
            "OK");
        return;
    }

    FileSearchPath paths;
    if (specificFolder.exists())
        paths.add (specificFolder);

    isScanning.store (true);
    scanThread = std::make_unique<PluginScanThread> (*this, formatManager, knownPluginList, paths, rescanAll);
    scanThread->startThread (Priority::normal);
}

void IconMenu::onPluginScanFinished (int newCount, int totalCount)
{
    isScanning.store (false);
    scanThread.reset();

    std::unique_ptr<XmlElement> xml (knownPluginList.createXml());
    if (xml != nullptr)
    {
        getAppProperties().getUserSettings()->setValue ("pluginList", xml.get());
        getAppProperties().saveIfNeeded();
    }

    String message = "Scan completed.\nTotal plugins found: " + String (totalCount);
    if (newCount > 0)
        message += " (" + String (newCount) + " new)";

    AlertWindow::showMessageBoxAsync (
        AlertWindow::InfoIcon,
        "Plugin Scan Completed",
        message,
        "OK");
}

void IconMenu::chooseCustomPluginFolder()
{
    folderChooser = std::make_unique<FileChooser> (
        "Select Plugin Folder to Scan",
        File::getSpecialLocation (File::userHomeDirectory),
        "*",
        true);

    const auto flags = FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories;

    folderChooser->launchAsync (flags, [this](const FileChooser& fc)
    {
        const File chosenDir = fc.getResult();
        if (chosenDir.isDirectory())
        {
            addCustomPluginPath (chosenDir.getFullPathName());
            startPluginScan (false, chosenDir);
        }
    });
}

void IconMenu::addCustomPluginPath (const String& path)
{
    String existing = getAppProperties().getUserSettings()->getValue ("customPluginPaths");
    StringArray paths;
    paths.addTokens (existing, ";\n", "");
    if (! paths.contains (path))
    {
        paths.add (path);
        getAppProperties().getUserSettings()->setValue ("customPluginPaths", paths.joinIntoString (";"));
        getAppProperties().saveIfNeeded();
    }
}

StringArray IconMenu::getCustomPluginPaths() const
{
    String existing = getAppProperties().getUserSettings()->getValue ("customPluginPaths");
    StringArray paths;
    paths.addTokens (existing, ";\n", "");
    return paths;
}