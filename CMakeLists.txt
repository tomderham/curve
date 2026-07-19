cmake_minimum_required(VERSION 3.22)
project(Curve VERSION 0.7.0)

set(CMAKE_OSX_DEPLOYMENT_TARGET "14.2" CACHE STRING "Minimum macOS version" FORCE)
set(CMAKE_OSX_SYSROOT "macosx" CACHE STRING "Use the latest installed macOS SDK")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")

add_compile_definitions(MAC_OS_X_VERSION_MIN_REQUIRED=140200)
add_compile_options(-Wno-deprecated-declarations)

add_subdirectory(JUCE)

juce_add_gui_app(Curve
    APP_CATEGORY                    public.app-category.music
    BUNDLE_ID                       com.thomasderham.curve
    ICON_BIG                        "${CMAKE_CURRENT_SOURCE_DIR}/Source/JUCEAppIcon.png"
    MICROPHONE_PERMISSION_ENABLED   TRUE
    MICROPHONE_PERMISSION_TEXT      "Curve requires access to your audio interface to operate. If you don't select an external audio interface, the microphone might be selected."
    BACKGROUND_AUDIO_ENABLED        TRUE
    PLUGINHOST_AU                   TRUE
    OSX_DEPLOYMENT_TARGET           14.2
    OSX_SYSROOT                     macosx
    PLIST_TO_MERGE "<plist><dict><key>LSMinimumSystemVersion</key><string>14.2</string><key>LSUIElement</key><true/><key>NSAppSleepDisabled</key><true/><key>NSAudioCaptureUsageDescription</key><string>Curve needs audio access to capture and process system audio.</string></dict></plist>"
    HARDENED_RUNTIME_ENABLED        TRUE
    HARDENED_RUNTIME_OPTIONS        com.apple.security.device.audio-input com.apple.security.cs.disable-library-validation com.apple.security.cs.allow-jit com.apple.security.cs.allow-unsigned-executable-memory
)

juce_generate_juce_header(Curve)

target_sources(Curve PRIVATE
    Source/HostStartup.cpp
    Source/Plugins/ARAPlugin.cpp
    Source/Plugins/IOConfigurationWindow.cpp
    Source/Plugins/InternalPlugins.cpp
    Source/Plugins/PluginGraph.cpp
    Source/Plugins/SystemAudioCaptureNode.mm
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
    juce::juce_recommended_warning_flags
    "-framework CoreAudio")

juce_add_bundle_resources_directory(Curve  ${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets/)
