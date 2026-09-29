cmake_minimum_required(VERSION 3.22)

project(LivingSyntheticSoundEcology VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

add_compile_definitions(JUCE_USE_CURL=0)

add_library(memory_dsp STATIC
    shared/AudioHistory.cpp
    shared/AudioHistory.h
    shared/RecallEngine.cpp
    shared/RecallEngine.h
)

target_include_directories(memory_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(memory_dsp PUBLIC cxx_std_20)

juce_add_plugin(Memory
    VERSION 1.1.0
    COMPANY_NAME "LSSE"
    BUNDLE_ID "com.sounddesignsuite.memory"
    PLUGIN_MANUFACTURER_CODE SdSt
    PLUGIN_CODE Mem1
    FORMATS VST3
    PRODUCT_NAME "MEMORY"
    PLUGIN_NAME "MEMORY"
    DESCRIPTION "LSSE rolling audio-memory sound design effect"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(Memory
    PRIVATE
        plugin/PluginProcessor.cpp
        plugin/PluginProcessor.h
        plugin/PluginEditor.cpp
        plugin/PluginEditor.h
)

target_compile_features(Memory PRIVATE cxx_std_20)

target_compile_definitions(Memory
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Memory
    PRIVATE
        memory_dsp
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

add_library(texture_dsp STATIC
    texture/dsp/CaptureBuffer.cpp
    texture/dsp/CaptureBuffer.h
    texture/dsp/DeterministicRng.h
    texture/dsp/DetailWear.cpp
    texture/dsp/DetailWear.h
    texture/dsp/GrainEngine.cpp
    texture/dsp/GrainEngine.h
    texture/dsp/SpectralSurface.cpp
    texture/dsp/SpectralSurface.h
)

target_include_directories(texture_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(texture_dsp PUBLIC cxx_std_20)
target_link_libraries(texture_dsp PUBLIC juce::juce_dsp)

juce_add_plugin(Texture
    VERSION 1.1.0
    COMPANY_NAME "LSSE"
    BUNDLE_ID "com.sounddesignsuite.texture"
    PLUGIN_MANUFACTURER_CODE SdSt
    PLUGIN_CODE Tex1
    FORMATS VST3
    PRODUCT_NAME "TEXTURE"
    PLUGIN_NAME "TEXTURE"
    DESCRIPTION "LSSE deterministic microstructure sound design effect"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(Texture
    PRIVATE
        texture/plugin/TextureEditor.cpp
        texture/plugin/TextureEditor.h
        texture/plugin/TextureProcessor.cpp
        texture/plugin/TextureProcessor.h
)

target_compile_features(Texture PRIVATE cxx_std_20)
target_compile_definitions(Texture
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)
target_link_libraries(Texture
    PRIVATE
        texture_dsp
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

add_library(machine_dsp STATIC
    machine/dsp/MachineTiming.cpp
    machine/dsp/MachineTiming.h
    machine/dsp/MechanismEngine.cpp
    machine/dsp/MechanismEngine.h
)
target_include_directories(machine_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(machine_dsp PUBLIC cxx_std_20)

juce_add_plugin(Machine
    VERSION 1.1.0
    COMPANY_NAME "LSSE"
    BUNDLE_ID "com.sounddesignsuite.machine"
    PLUGIN_MANUFACTURER_CODE SdSt
    PLUGIN_CODE Mac1
    FORMATS VST3
    PRODUCT_NAME "MACHINE"
    PLUGIN_NAME "MACHINE"
    DESCRIPTION "LSSE deterministic rhythmic mechanical sound design effect"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)
target_sources(Machine PRIVATE
    machine/plugin/MachineEditor.cpp
    machine/plugin/MachineEditor.h
    machine/plugin/MachineProcessor.cpp
    machine/plugin/MachineProcessor.h
)
target_compile_features(Machine PRIVATE cxx_std_20)
target_compile_definitions(Machine PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
target_link_libraries(Machine
    PRIVATE
        machine_dsp
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Phase 1 products share build plumbing only. Each product owns its processor,
# parameter contract, state root, and future DSP implementation.
function(add_phase_one_effect target product_name bundle_id plugin_code processor_cpp processor_h)
    juce_add_plugin(${target}
        VERSION 1.1.0
        COMPANY_NAME "LSSE"
        BUNDLE_ID "${bundle_id}"
        PLUGIN_MANUFACTURER_CODE SdSt
        PLUGIN_CODE ${plugin_code}
        FORMATS VST3
        PRODUCT_NAME "${product_name}"
        PLUGIN_NAME "${product_name}"
        DESCRIPTION "LSSE ${product_name} sound design effect"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
    )
    target_sources(${target} PRIVATE ${processor_cpp} ${processor_h})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )
    target_link_libraries(${target}
        PRIVATE juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )
endfunction()

add_phase_one_effect(Material "MATERIAL" "com.sounddesignsuite.material" Mat1 material/plugin/MaterialProcessor.cpp material/plugin/MaterialProcessor.h)
add_phase_one_effect(Impact "IMPACT" "com.sounddesignsuite.impact" Imp1 impact/plugin/ImpactProcessor.cpp impact/plugin/ImpactProcessor.h)
add_phase_one_effect(Creature "CREATURE" "com.sounddesignsuite.creature" Crt1 creature/plugin/CreatureProcessor.cpp creature/plugin/CreatureProcessor.h)
add_phase_one_effect(Motion "MOTION" "com.sounddesignsuite.motion" Mot1 motion/plugin/MotionProcessor.cpp motion/plugin/MotionProcessor.h)
add_phase_one_effect(Environment "ENVIRONMENT" "com.sounddesignsuite.environment" Env1 environment/plugin/EnvironmentProcessor.cpp environment/plugin/EnvironmentProcessor.h)
add_phase_one_effect(ConvolutionLab "CONVOLUTION" "com.sounddesignsuite.convolutionlab" Cvl1 convolution_lab/plugin/ConvolutionLabProcessor.cpp convolution_lab/plugin/ConvolutionLabProcessor.h)

add_library(material_phase2_dsp STATIC material/dsp/ModalBank.cpp material/dsp/ModalBank.h)
add_library(impact_phase2_dsp STATIC impact/dsp/TransientBodyEngine.cpp impact/dsp/TransientBodyEngine.h)
add_library(creature_phase2_dsp STATIC creature/dsp/GestureEngine.cpp creature/dsp/GestureEngine.h)
add_library(motion_phase2_dsp STATIC motion/dsp/PathEngine.cpp motion/dsp/PathEngine.h)
add_library(environment_phase2_dsp STATIC environment/dsp/BedScheduler.cpp environment/dsp/BedScheduler.h)
add_library(convolution_phase2_dsp STATIC convolution_lab/dsp/ReferenceConvolver.cpp convolution_lab/dsp/ReferenceConvolver.h)
foreach(target material_phase2_dsp impact_phase2_dsp creature_phase2_dsp motion_phase2_dsp environment_phase2_dsp convolution_phase2_dsp)
    target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(${target} PUBLIC cxx_std_20)
endforeach()
target_link_libraries(Material PRIVATE material_phase2_dsp)
target_link_libraries(Impact PRIVATE impact_phase2_dsp)
target_link_libraries(Creature PRIVATE creature_phase2_dsp)
target_link_libraries(Motion PRIVATE motion_phase2_dsp)
target_link_libraries(Environment PRIVATE environment_phase2_dsp)
target_link_libraries(ConvolutionLab PRIVATE convolution_phase2_dsp juce::juce_dsp)

include(CTest)

if(BUILD_TESTING)
    add_executable(UserSampleSlotTests shared/tests/UserSampleSlotTests.cpp)
    target_include_directories(UserSampleSlotTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(UserSampleSlotTests PRIVATE cxx_std_20)
    target_link_libraries(UserSampleSlotTests PRIVATE juce::juce_audio_formats
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME LSSE.UserSampleSlot COMMAND UserSampleSlotTests)

    add_executable(MaterialPhase2Tests material/tests/ModalBankTests.cpp)
    target_link_libraries(MaterialPhase2Tests PRIVATE material_phase2_dsp)
    add_test(NAME MATERIAL.Phase2 COMMAND MaterialPhase2Tests)
    add_executable(ImpactPhase2Tests impact/tests/TransientBodyEngineTests.cpp)
    target_link_libraries(ImpactPhase2Tests PRIVATE impact_phase2_dsp)
    add_test(NAME IMPACT.Phase2 COMMAND ImpactPhase2Tests)
    add_executable(CreaturePhase2Tests creature/tests/GestureEngineTests.cpp)
    target_link_libraries(CreaturePhase2Tests PRIVATE creature_phase2_dsp)
    add_test(NAME CREATURE.Phase2 COMMAND CreaturePhase2Tests)
    add_executable(MotionPhase2Tests motion/tests/PathEngineTests.cpp)
    target_link_libraries(MotionPhase2Tests PRIVATE motion_phase2_dsp)
    add_test(NAME MOTION.Phase2 COMMAND MotionPhase2Tests)
    add_executable(EnvironmentPhase2Tests environment/tests/BedSchedulerTests.cpp)
    target_link_libraries(EnvironmentPhase2Tests PRIVATE environment_phase2_dsp)
    add_test(NAME ENVIRONMENT.Phase2 COMMAND EnvironmentPhase2Tests)
    add_executable(ConvolutionPhase2Tests convolution_lab/tests/ReferenceConvolverTests.cpp)
    target_link_libraries(ConvolutionPhase2Tests PRIVATE convolution_phase2_dsp)
    add_test(NAME CONVOLUTION.Phase2 COMMAND ConvolutionPhase2Tests)
    add_executable(ConvolutionLabProcessorTests convolution_lab/tests/ConvolutionLabProcessorTests.cpp)
    target_link_libraries(ConvolutionLabProcessorTests PRIVATE ConvolutionLab juce::juce_audio_utils juce::juce_dsp)
    target_include_directories(ConvolutionLabProcessorTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(ConvolutionLabProcessorTests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(ConvolutionLabProcessorTests PRIVATE /EHsc)
    endif()
    add_test(NAME CONVOLUTION.FileIR COMMAND ConvolutionLabProcessorTests)

    add_executable(MemoryTests tests/AudioHistoryTests.cpp)
    target_link_libraries(MemoryTests PRIVATE memory_dsp)
    target_compile_features(MemoryTests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(MemoryTests PRIVATE /EHsc)
    endif()
    add_test(NAME Memory.AudioHistory COMMAND MemoryTests)

    add_executable(MemoryRecallTests tests/RecallEngineTests.cpp)
    target_link_libraries(MemoryRecallTests PRIVATE memory_dsp)
    target_compile_features(MemoryRecallTests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(MemoryRecallTests PRIVATE /EHsc)
    endif()
    add_test(NAME Memory.RecallEngine COMMAND MemoryRecallTests)

    add_executable(MemoryProcessorTests tests/ProcessorTests.cpp)
    target_link_libraries(MemoryProcessorTests PRIVATE Memory juce::juce_audio_utils)
    target_include_directories(MemoryProcessorTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(MemoryProcessorTests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(MemoryProcessorTests PRIVATE /EHsc)
    endif()
    add_test(NAME Memory.Processor COMMAND MemoryProcessorTests)

    add_executable(MemoryVst3HostTests tests/MemoryVst3HostTests.cpp)
    target_link_libraries(MemoryVst3HostTests PRIVATE juce::juce_audio_utils)
    target_compile_features(MemoryVst3HostTests PRIVATE cxx_std_20)
    target_compile_definitions(MemoryVst3HostTests PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    if(MSVC)
        target_compile_options(MemoryVst3HostTests PRIVATE /EHsc)
    endif()
    add_dependencies(MemoryVst3HostTests Memory_VST3)
    add_test(NAME Memory.VST3Host COMMAND MemoryVst3HostTests
        "${CMAKE_BINARY_DIR}/Memory_artefacts/$<CONFIG>/VST3/MEMORY.vst3")

    add_executable(TextureDspTests texture/tests/TextureDspTests.cpp)
    target_link_libraries(TextureDspTests PRIVATE texture_dsp)
    target_compile_features(TextureDspTests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(TextureDspTests PRIVATE /EHsc)
    endif()
    add_test(NAME Texture.Dsp COMMAND TextureDspTests)

    add_executable(TextureProcessorTests texture/tests/TextureProcessorTests.cpp)
    target_link_libraries(TextureProcessorTests PRIVATE Texture juce::juce_audio_utils)
    target_include_directories(TextureProcessorTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(TextureProcessorTests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(TextureProcessorTests PRIVATE /EHsc)
    endif()
    add_test(NAME Texture.Processor COMMAND TextureProcessorTests)

    add_executable(TextureVst3HostTests texture/tests/TextureVst3HostTests.cpp)
    target_link_libraries(TextureVst3HostTests PRIVATE juce::juce_audio_utils)
    target_compile_features(TextureVst3HostTests PRIVATE cxx_std_20)
    target_compile_definitions(TextureVst3HostTests PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    if(MSVC)
        target_compile_options(TextureVst3HostTests PRIVATE /EHsc)
    endif()
    add_dependencies(TextureVst3HostTests Texture_VST3)
    add_test(NAME Texture.VST3Host COMMAND TextureVst3HostTests
        "${CMAKE_BINARY_DIR}/Texture_artefacts/$<CONFIG>/VST3/TEXTURE.vst3")

    add_executable(MachineTimingTests machine/tests/MachineTimingTests.cpp)
    target_link_libraries(MachineTimingTests PRIVATE machine_dsp)
    target_compile_features(MachineTimingTests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(MachineTimingTests PRIVATE /EHsc)
    endif()
    add_test(NAME Machine.Timing COMMAND MachineTimingTests)

    add_executable(MachineMechanismTests machine/tests/MechanismEngineTests.cpp)
    target_link_libraries(MachineMechanismTests PRIVATE machine_dsp)
    target_compile_features(MachineMechanismTests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(MachineMechanismTests PRIVATE /EHsc)
    endif()
    add_test(NAME Machine.Mechanism COMMAND MachineMechanismTests)

    add_executable(MachineProcessorTests machine/tests/MachineProcessorTests.cpp)
    target_link_libraries(MachineProcessorTests PRIVATE Machine juce::juce_audio_utils)
    target_include_directories(MachineProcessorTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(MachineProcessorTests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(MachineProcessorTests PRIVATE /EHsc)
    endif()
    add_test(NAME Machine.Processor COMMAND MachineProcessorTests)

    add_executable(MachineVst3HostTests machine/tests/MachineVst3HostTests.cpp)
    target_link_libraries(MachineVst3HostTests PRIVATE juce::juce_audio_utils)
    target_compile_features(MachineVst3HostTests PRIVATE cxx_std_20)
    target_compile_definitions(MachineVst3HostTests PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    if(MSVC)
        target_compile_options(MachineVst3HostTests PRIVATE /EHsc)
    endif()
    add_dependencies(MachineVst3HostTests Machine_VST3)
    add_test(NAME Machine.VST3Host COMMAND MachineVst3HostTests
        "${CMAKE_BINARY_DIR}/Machine_artefacts/$<CONFIG>/VST3/MACHINE.vst3")

    add_executable(LssePhaseOneVst3HostTests horizontal_tests/PhaseOneVst3HostTests.cpp)
    target_link_libraries(LssePhaseOneVst3HostTests PRIVATE juce::juce_audio_utils)
    target_compile_features(LssePhaseOneVst3HostTests PRIVATE cxx_std_20)
    target_compile_definitions(LssePhaseOneVst3HostTests PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    if(MSVC)
        target_compile_options(LssePhaseOneVst3HostTests PRIVATE /EHsc)
    endif()
    add_dependencies(LssePhaseOneVst3HostTests
        Material_VST3 Impact_VST3 Creature_VST3 Motion_VST3 Environment_VST3 ConvolutionLab_VST3)
    add_test(NAME LSSE.Phase6VST3 COMMAND LssePhaseOneVst3HostTests
        "${CMAKE_BINARY_DIR}/Material_artefacts/$<CONFIG>/VST3/MATERIAL.vst3"
        "${CMAKE_BINARY_DIR}/Impact_artefacts/$<CONFIG>/VST3/IMPACT.vst3"
        "${CMAKE_BINARY_DIR}/Creature_artefacts/$<CONFIG>/VST3/CREATURE.vst3"
        "${CMAKE_BINARY_DIR}/Motion_artefacts/$<CONFIG>/VST3/MOTION.vst3"
        "${CMAKE_BINARY_DIR}/Environment_artefacts/$<CONFIG>/VST3/ENVIRONMENT.vst3"
        "${CMAKE_BINARY_DIR}/ConvolutionLab_artefacts/$<CONFIG>/VST3/CONVOLUTION.vst3")

    add_executable(LssePhaseSixStressTests horizontal_tests/PhaseSixStressTests.cpp)
    target_link_libraries(LssePhaseSixStressTests PRIVATE juce::juce_audio_utils)
    target_compile_features(LssePhaseSixStressTests PRIVATE cxx_std_20)
    target_compile_definitions(LssePhaseSixStressTests PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    if(MSVC)
        target_compile_options(LssePhaseSixStressTests PRIVATE /EHsc)
    endif()
    add_dependencies(LssePhaseSixStressTests
        Material_VST3 Impact_VST3 Creature_VST3 Motion_VST3 Environment_VST3 ConvolutionLab_VST3)
    add_test(NAME LSSE.Phase6Stress COMMAND LssePhaseSixStressTests
        "${CMAKE_BINARY_DIR}/Material_artefacts/$<CONFIG>/VST3/MATERIAL.vst3"
        "${CMAKE_BINARY_DIR}/Impact_artefacts/$<CONFIG>/VST3/IMPACT.vst3"
        "${CMAKE_BINARY_DIR}/Creature_artefacts/$<CONFIG>/VST3/CREATURE.vst3"
        "${CMAKE_BINARY_DIR}/Motion_artefacts/$<CONFIG>/VST3/MOTION.vst3"
        "${CMAKE_BINARY_DIR}/Environment_artefacts/$<CONFIG>/VST3/ENVIRONMENT.vst3"
        "${CMAKE_BINARY_DIR}/ConvolutionLab_artefacts/$<CONFIG>/VST3/CONVOLUTION.vst3")
endif()
