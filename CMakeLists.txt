cmake_minimum_required(VERSION 3.22)
project(Curve VERSION 0.1.0)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")

add_subdirectory(JUCE)

juce_add_gui_app(Curve
    APP_CATEGORY                    public.app-category.music
    BUNDLE_ID                       com.thomasderham.curve
    ICON_BIG                        "${CMAKE_CURRENT_SOURCE_DIR}/Source/JUCEAppIcon.png"
    MICROPHONE_PERMISSION_ENABLED   TRUE
    BACKGROUND_AUDIO_ENABLED        TRUE
    PLUGINHOST_AU                   TRUE
    PLIST_TO_MERGE "<plist><dict><key>LSUIElement</key><true/><key>NSAppSleepDisabled</key><true/></dict></plist>"
)

juce_generate_juce_header(Curve)

target_sources(Curve PRIVATE
    Source/HostStartup.cpp
    Source/Plugins/ARAPlugin.cpp
    Source/Plugins/IOConfigurationWindow.cpp
    Source/Plugins/InternalPlugins.cpp
    Source/Plugins/PluginGraph.cpp
    Source/UI/GraphEditorPanel.cpp
    Source/UI/MainHostWindow.cpp)

target_compile_definitions(Curve PRIVATE
    JUCE_ALSA=1
    JUCE_CONTENT_SHARING=1
    JUCE_DIRECTSOUND=1
    JUCE_DISABLE_CAUTIOUS_PARAMETER_ID_CHECKING=1
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_PLUGINHOST_LADSPA=1
    JUCE_PLUGINHOST_LV2=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_ARA=0
    JUCE_USE_CAMERA=0
    JUCE_USE_CDBURNER=0
    JUCE_USE_CDREADER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=0
    JUCE_USE_OGGVORBIS=1
    JUCE_VST3_HOST_CROSS_PLATFORM_UID=1
    JUCE_WASAPI=1
    JUCE_WEB_BROWSER=0
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1)

target_link_libraries(Curve PRIVATE
    juce::juce_audio_utils
    juce::juce_cryptography
    juce::juce_dsp
    juce::juce_opengl
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

juce_add_bundle_resources_directory(Curve  ${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/)
