cmake_minimum_required(VERSION 3.25)

project(FolkPark VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum supported macOS version")

if(NOT CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
    message(FATAL_ERROR "Release 0.1 must be configured with CMAKE_OSX_ARCHITECTURES=x86_64")
endif()

set(FOLK_PARK_JUCE_COMMIT "7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2")

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/CMakeLists.txt")
    message(FATAL_ERROR "JUCE is missing. Run ./scripts/bootstrap_macos.sh")
endif()

add_subdirectory(third_party/JUCE EXCLUDE_FROM_ALL)

juce_add_binary_data(FolkParkAssets
    NAMESPACE FolkParkAssets
    SOURCES
        resources/ui/index.html
        resources/ui/app.js
        resources/ui/app.css)

juce_add_plugin(FolkPark
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Hermann Pauwells"
    BUNDLE_ID "com.folkpark.audio.folkpark"
    PLUGIN_MANUFACTURER_CODE FlPk
    PLUGIN_CODE FkP1
    FORMATS Standalone VST3
    PRODUCT_NAME "folk park"
    DESCRIPTION "Original wavetable instrument and deterministic composition assistant"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)

target_sources(FolkPark PRIVATE
    src/effects/EffectChain.cpp
    src/effects/EffectChain.h
    src/plugin/PluginEditor.cpp
    src/plugin/PluginEditor.h
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginProcessor.h
    src/plugin/ParameterIds.h
    src/common/ParameterIds.h
    src/persistence/Preset.cpp
    src/persistence/Preset.h
    src/render/OfflinePreviewRenderer.cpp
    src/render/OfflinePreviewRenderer.h
    src/midi/MidiProof.cpp
    src/midi/MidiProof.h
    src/midi/Composition.cpp
    src/midi/Composition.h
    src/midi/CompositionSession.cpp
    src/midi/CompositionSession.h
    src/midi/MidiDelivery.cpp
    src/midi/MidiDelivery.h
    src/midi/PreviewMidi.cpp
    src/midi/PreviewMidi.h
    src/synth/SynthEngine.cpp
    src/synth/SynthEngine.h
    src/synth/Modulation.cpp
    src/synth/Modulation.h
    src/synth/WavetableBank.cpp
    src/synth/WavetableBank.h
    src/synth/WavetableConverter.cpp
    src/synth/WavetableConverter.h
    src/synth/WavetableExchange.cpp
    src/synth/WavetableExchange.h
    src/synth/WavetableImportService.cpp
    src/synth/WavetableImportService.h)

target_include_directories(FolkPark PRIVATE src)

target_compile_definitions(FolkPark
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    PRIVATE
        FOLK_PARK_VERSION="${PROJECT_VERSION}"
        FOLK_PARK_JUCE_COMMIT="${FOLK_PARK_JUCE_COMMIT}")

target_link_libraries(FolkPark
    PRIVATE
        FolkParkAssets
        juce::juce_audio_utils
        juce::juce_cryptography
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

juce_generate_juce_header(FolkPark)

juce_add_console_app(FolkParkTests PRODUCT_NAME "folk park native tests")
target_sources(FolkParkTests PRIVATE
    tests/M0Tests.cpp
    src/midi/MidiProof.cpp
    src/midi/MidiProof.h
    src/midi/Composition.cpp
    src/midi/Composition.h
    src/synth/SynthEngine.cpp
    src/synth/SynthEngine.h
    src/synth/Modulation.cpp
    src/synth/Modulation.h
    src/synth/WavetableBank.cpp
    src/synth/WavetableBank.h
    src/synth/WavetableConverter.cpp
    src/synth/WavetableConverter.h
    src/synth/WavetableExchange.cpp
    src/synth/WavetableExchange.h
    src/synth/WavetableImportService.cpp
    src/synth/WavetableImportService.h)
target_include_directories(FolkParkTests PRIVATE src)
target_compile_definitions(FolkParkTests PRIVATE JUCE_WEB_BROWSER=1 JUCE_USE_CURL=0)
target_link_libraries(FolkParkTests PRIVATE
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_cryptography
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

enable_testing()
add_test(NAME FolkParkNativeTests COMMAND FolkParkTests)

juce_add_console_app(FolkParkEffectsTests PRODUCT_NAME "folk park effects tests")
target_sources(FolkParkEffectsTests PRIVATE
    tests/EffectsTests.cpp
    src/effects/EffectChain.cpp
    src/effects/EffectChain.h)
target_include_directories(FolkParkEffectsTests PRIVATE src)
target_compile_definitions(FolkParkEffectsTests PRIVATE JUCE_USE_CURL=0)
target_link_libraries(FolkParkEffectsTests PRIVATE
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_test(NAME FolkParkEffectsChainTests COMMAND FolkParkEffectsTests)

juce_add_console_app(FolkParkOfflinePreviewTests PRODUCT_NAME "folk park offline preview tests")
target_sources(FolkParkOfflinePreviewTests PRIVATE
    tests/OfflinePreviewTests.cpp
    src/render/OfflinePreviewRenderer.cpp
    src/render/OfflinePreviewRenderer.h
    src/effects/EffectChain.cpp
    src/effects/EffectChain.h
    src/midi/Composition.cpp
    src/midi/Composition.h
    src/midi/MidiDelivery.cpp
    src/midi/MidiDelivery.h
    src/synth/SynthEngine.cpp
    src/synth/SynthEngine.h
    src/synth/Modulation.cpp
    src/synth/Modulation.h
    src/synth/WavetableBank.cpp
    src/synth/WavetableBank.h
    src/synth/WavetableExchange.cpp
    src/synth/WavetableExchange.h)
target_include_directories(FolkParkOfflinePreviewTests PRIVATE src)
target_compile_definitions(FolkParkOfflinePreviewTests PRIVATE JUCE_USE_CURL=0)
target_link_libraries(FolkParkOfflinePreviewTests PRIVATE
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_test(NAME FolkParkOfflinePreviewIsolationTests COMMAND FolkParkOfflinePreviewTests)

juce_add_console_app(FolkParkPresetTests PRODUCT_NAME "folk park preset tests")
target_sources(FolkParkPresetTests PRIVATE
    tests/PresetTests.cpp
    src/persistence/Preset.cpp
    src/persistence/Preset.h
    src/common/ParameterIds.h
    src/synth/Modulation.cpp
    src/synth/Modulation.h
    src/synth/WavetableBank.cpp
    src/synth/WavetableBank.h
    src/synth/WavetableConverter.cpp
    src/synth/WavetableConverter.h)
target_include_directories(FolkParkPresetTests PRIVATE src)
target_compile_definitions(FolkParkPresetTests PRIVATE
    JUCE_USE_CURL=0
    FOLK_PARK_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(FolkParkPresetTests PRIVATE
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_cryptography
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_test(NAME FolkParkPresetPersistenceTests COMMAND FolkParkPresetTests)

juce_add_console_app(FolkParkCompositionTests PRODUCT_NAME "folk park composition tests")
target_sources(FolkParkCompositionTests PRIVATE
    tests/CompositionTests.cpp
    src/midi/Composition.cpp
    src/midi/Composition.h)
target_include_directories(FolkParkCompositionTests PRIVATE src)
target_compile_definitions(FolkParkCompositionTests PRIVATE JUCE_USE_CURL=0)
target_link_libraries(FolkParkCompositionTests PRIVATE
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_test(NAME FolkParkCompositionPropertyTests COMMAND FolkParkCompositionTests)

juce_add_console_app(FolkParkMidiDeliveryTests PRODUCT_NAME "folk park MIDI delivery tests")
target_sources(FolkParkMidiDeliveryTests PRIVATE
    tests/MidiDeliveryTests.cpp
    src/midi/Composition.cpp
    src/midi/Composition.h
    src/midi/CompositionSession.cpp
    src/midi/CompositionSession.h
    src/midi/MidiDelivery.cpp
    src/midi/MidiDelivery.h
    src/midi/PreviewMidi.cpp
    src/midi/PreviewMidi.h)
target_include_directories(FolkParkMidiDeliveryTests PRIVATE src)
target_compile_definitions(FolkParkMidiDeliveryTests PRIVATE JUCE_USE_CURL=0)
target_link_libraries(FolkParkMidiDeliveryTests PRIVATE
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_test(NAME FolkParkMidiDeliveryParityTests COMMAND FolkParkMidiDeliveryTests)

juce_add_console_app(FolkParkAssistantModelTests PRODUCT_NAME "folk park assistant model tests")
target_sources(FolkParkAssistantModelTests PRIVATE
    tests/AssistantModelsTests.cpp
    src/assistant/AssistantModels.cpp
    src/assistant/AssistantModels.h
    src/midi/Composition.cpp
    src/midi/Composition.h)
target_include_directories(FolkParkAssistantModelTests PRIVATE src)
target_compile_definitions(FolkParkAssistantModelTests PRIVATE JUCE_USE_CURL=0)
target_link_libraries(FolkParkAssistantModelTests PRIVATE
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_test(NAME FolkParkAssistantSchemaModelTests COMMAND FolkParkAssistantModelTests)

juce_add_console_app(FolkParkRealtimeTests PRODUCT_NAME "folk park real-time allocation tests")
target_sources(FolkParkRealtimeTests PRIVATE
    tests/RealtimeTests.cpp
    src/midi/Composition.cpp
    src/midi/Composition.h
    src/midi/MidiDelivery.cpp
    src/midi/MidiDelivery.h
    src/midi/PreviewMidi.cpp
    src/midi/PreviewMidi.h
    src/effects/EffectChain.cpp
    src/effects/EffectChain.h
    src/synth/SynthEngine.cpp
    src/synth/SynthEngine.h
    src/synth/Modulation.cpp
    src/synth/Modulation.h
    src/synth/WavetableBank.cpp
    src/synth/WavetableBank.h
    src/synth/WavetableExchange.cpp
    src/synth/WavetableExchange.h)
target_include_directories(FolkParkRealtimeTests PRIVATE src)
target_compile_definitions(FolkParkRealtimeTests PRIVATE JUCE_USE_CURL=0)
target_link_libraries(FolkParkRealtimeTests PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_test(NAME FolkParkRealtimeAllocationTests COMMAND FolkParkRealtimeTests)

juce_add_console_app(FolkParkPluginTests PRODUCT_NAME "folk park plug-in tests")
target_sources(FolkParkPluginTests PRIVATE
    tests/PluginTests.cpp
    src/plugin/PluginEditor.cpp
    src/plugin/PluginEditor.h
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginProcessor.h
    src/plugin/ParameterIds.h
    src/effects/EffectChain.cpp
    src/effects/EffectChain.h
    src/render/OfflinePreviewRenderer.cpp
    src/render/OfflinePreviewRenderer.h
    src/midi/MidiProof.cpp
    src/midi/MidiProof.h
    src/midi/Composition.cpp
    src/midi/Composition.h
    src/midi/CompositionSession.cpp
    src/midi/CompositionSession.h
    src/midi/MidiDelivery.cpp
    src/midi/MidiDelivery.h
    src/midi/PreviewMidi.cpp
    src/midi/PreviewMidi.h
    src/synth/SynthEngine.cpp
    src/synth/SynthEngine.h
    src/synth/Modulation.cpp
    src/synth/Modulation.h
    src/synth/WavetableBank.cpp
    src/synth/WavetableBank.h
    src/synth/WavetableConverter.cpp
    src/synth/WavetableConverter.h
    src/synth/WavetableExchange.cpp
    src/synth/WavetableExchange.h
    src/synth/WavetableImportService.cpp
    src/synth/WavetableImportService.h)
target_include_directories(FolkParkPluginTests PRIVATE src)
target_compile_definitions(FolkParkPluginTests PRIVATE
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    FOLK_PARK_VERSION="${PROJECT_VERSION}"
    FOLK_PARK_JUCE_COMMIT="${FOLK_PARK_JUCE_COMMIT}")
target_link_libraries(FolkParkPluginTests PRIVATE
    FolkParkAssets
    juce::juce_audio_utils
    juce::juce_cryptography
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
add_test(NAME FolkParkPluginStateAndUiTests COMMAND FolkParkPluginTests)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    juce_add_console_app(FolkParkVST3SmokeTests PRODUCT_NAME "folk park VST3 smoke tests")
    target_sources(FolkParkVST3SmokeTests PRIVATE tests/Vst3SmokeTests.cpp)
    target_compile_definitions(FolkParkVST3SmokeTests PRIVATE
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_USE_CURL=0)
    target_link_libraries(FolkParkVST3SmokeTests PRIVATE
        juce::juce_audio_processors
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    add_dependencies(FolkParkVST3SmokeTests FolkPark_VST3)
    add_test(
        NAME FolkParkBuiltVST3MidiSmokeTest
        COMMAND FolkParkVST3SmokeTests "$<TARGET_BUNDLE_DIR:FolkPark_VST3>")
endif()
