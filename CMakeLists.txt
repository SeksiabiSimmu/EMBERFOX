cmake_minimum_required(VERSION 3.25)
project(Emberfox VERSION 1.3.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ea77444750de45e036e0279bb0291144bf8e3688)
    FetchContent_MakeAvailable(JUCE)
else()
    add_subdirectory(external/JUCE EXCLUDE_FROM_ALL)
endif()

enable_testing()
add_executable(EmberfoxVisualStateTests Tests/VisualStateTests.cpp)
target_include_directories(EmberfoxVisualStateTests PRIVATE Source)
add_test(NAME VisualState COMMAND EmberfoxVisualStateTests)
add_executable(EmberfoxAnimationTests Tests/AnimationTests.cpp)
target_include_directories(EmberfoxAnimationTests PRIVATE Source)
add_test(NAME Animation COMMAND EmberfoxAnimationTests)
juce_add_console_app(EmberfoxDspTests PRODUCT_NAME "Emberfox DSP Tests")
target_sources(EmberfoxDspTests PRIVATE Tests/DspTests.cpp Source/DistortionEngine.cpp)
target_include_directories(EmberfoxDspTests PRIVATE Source)
target_compile_definitions(EmberfoxDspTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(EmberfoxDspTests PRIVATE juce::juce_dsp juce::juce_recommended_config_flags)
add_test(NAME DSP COMMAND EmberfoxDspTests)

juce_add_console_app(EmberfoxHostSmoke PRODUCT_NAME "Emberfox VST3 Test Host")
target_sources(EmberfoxHostSmoke PRIVATE Tests/HostSmoke.cpp)
target_compile_definitions(EmberfoxHostSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(EmberfoxHostSmoke PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)

juce_add_plugin(Emberfox
    COMPANY_NAME "EmberSoft"
    BUNDLE_ID "audio.emberfox.effect"
    PLUGIN_MANUFACTURER_CODE Emfx
    PLUGIN_CODE Efx1
    FORMATS VST3 Standalone
    PRODUCT_NAME "EMBERFOX"
    DESCRIPTION "Five-mode oversampled distortion"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE VST3_CATEGORIES Fx Distortion)
target_sources(Emberfox PRIVATE Source/DistortionEngine.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(Emberfox PUBLIC Source)
target_compile_definitions(Emberfox PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Emberfox PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)
target_link_libraries(EmberfoxHostSmoke PRIVATE Emberfox juce::juce_dsp)
juce_add_console_app(EmberfoxReleaseTests PRODUCT_NAME "Emberfox Release Feature Tests")
target_sources(EmberfoxReleaseTests PRIVATE Tests/ReleaseFeaturesTests.cpp)
target_link_libraries(EmberfoxReleaseTests PRIVATE Emberfox juce::juce_dsp)
add_test(NAME ReleaseFeatures COMMAND EmberfoxReleaseTests)
add_test(NAME VST3Host COMMAND EmberfoxHostSmoke
    "$<TARGET_FILE_DIR:Emberfox_VST3>/../.."
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/validation")
add_test(NAME NativeUI COMMAND EmberfoxHostSmoke --internal-ui "${CMAKE_CURRENT_SOURCE_DIR}/docs/validation")

# Pinned JUCE's Windows rcfile rule omits Info.txt from its dependencies.
# Track it here so incremental version/publisher changes regenerate PE resources.
if(WIN32)
    foreach(app_target Emberfox EmberfoxDspTests EmberfoxHostSmoke EmberfoxReleaseTests)
        get_target_property(generated_code ${app_target} JUCE_GENERATED_SOURCES_DIRECTORY)
        get_target_property(info_file ${app_target} JUCE_INFO_FILE)
        add_custom_command(OUTPUT "${generated_code}/${app_target}_resources.rc"
            APPEND DEPENDS "${info_file}")
    endforeach()
endif()
