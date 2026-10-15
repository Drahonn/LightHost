#include "JuceHeader.h"
#include "IconMenu.hpp"

#if ! (JUCE_PLUGINHOST_VST3 || JUCE_PLUGINHOST_AU)
 #error "If you're building the audio plugin host, you need to enable VST3 and/or AU support."
#endif

class PluginHostApp : public JUCEApplication
{
public:
    PluginHostApp() = default;

    void initialise (const String&) override
    {
        PropertiesFile::Options options;
        options.applicationName     = getApplicationName();
        options.filenameSuffix      = "settings";
        options.osxLibrarySubFolder = "Preferences";

        checkArguments (options);

        appProperties = std::make_unique<ApplicationProperties>();
        appProperties->setStorageParameters (options);

        LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

        mainWindow = std::make_unique<IconMenu>();

        #if JUCE_MAC
            Process::setDockIconVisible (false);
        #endif
    }

    void shutdown() override
    {
        mainWindow.reset();
        appProperties.reset();
        LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override
    {
        JUCEApplicationBase::quit();
    }

    const String getApplicationName() override    { return "Light Host"; }
    const String getApplicationVersion() override { return ProjectInfo::versionString; }

    bool moreThanOneInstanceAllowed() override
    {
        return ! getMultiInstanceId().isEmpty();
    }

    std::unique_ptr<ApplicationProperties> appProperties;
    LookAndFeel_V4 lookAndFeel;

private:
    std::unique_ptr<IconMenu> mainWindow;

    String getMultiInstanceId() const
    {
        const auto params = getCommandLineParameterArray();
        for (const auto& param : params)
        {
            if (param.startsWithIgnoreCase ("-multi-instance="))
                return param.fromFirstOccurrenceOf ("=", false, false);
        }
        return {};
    }

    void checkArguments (PropertiesFile::Options& options)
    {
        const String instanceId = getMultiInstanceId();
        if (instanceId.isNotEmpty())
            options.filenameSuffix = instanceId + "." + options.filenameSuffix;
    }
};

static PluginHostApp* getApp()
{
    return dynamic_cast<PluginHostApp*> (JUCEApplication::getInstance());
}

ApplicationProperties& getAppProperties()
{
    auto* app = getApp();
    jassert (app != nullptr && app->appProperties != nullptr);
    return *(app->appProperties);
}

START_JUCE_APPLICATION (PluginHostApp)
