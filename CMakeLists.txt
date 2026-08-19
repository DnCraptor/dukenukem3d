# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.22)
include(pico_sdk_import.cmake)

project(duke3d C CXX ASM)
pico_sdk_init()

SET(BUILD_NAME "${PROJECT_NAME}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
set(COMPILED_DIR "${CMAKE_SOURCE_DIR}/compiled")
if(WIN32)
    set(ELF2EZ_TOOL "${CMAKE_SOURCE_DIR}/tools/elf2ez.exe")
else()
    set(ELF2EZ_TOOL "${CMAKE_SOURCE_DIR}/tools/elf2ez")
endif()
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR}")

# Build only the Duke3D executable.  Do not recursively glob the repository:
# SRC/ also contains the BUILD editor/test program, UTIL/ contains standalone
# tools, extras/ is a second game-code snapshot, and build/ contains CMake's
# compiler-identification sources.
set(DUKE_GAME_SOURCES
    source/GAME.C
    source/ACTORS.C
    source/GAMEDEF.C
    source/GLOBAL.C
    source/MENUES.C
    source/PLAYER.C
    source/PREMAP.C
    source/SECTOR.C
    source/SOUNDS.C
    source/RTS.C
    source/CONFIG.C
    source/ANIMLIB.C
)

# The released Duke makefile linked these Build-engine modules as prebuilt
# Watcom objects.  Their C sources are present in SRC/ and can be rebuilt for
# ARM.  A.ASM is deliberately not listed: it is x86 renderer assembly and will
# be replaced by a portable/ARM implementation in a later porting stage.
set(DUKE_BUILD_SOURCES
    SRC/CACHE1D.C
    SRC/ENGINE.C
    SRC/MMULTI.C
    SRC/PORTABLE_PRAGMAS.C
    SRC/PORTABLE_A.C
    source/PORTABLE_RUNTIME.C
    source/PORTABLE_KEYBOARD.C
    source/PORTABLE_CONTROL.C
    source/PORTABLE_SCRIPT.C
    source/PORTABLE_TASK.C
    audiolib/SOURCE/USER.C
    audiolib/SOURCE/LL_MAN.C
    audiolib/SOURCE/MIDI.C
    audiolib/SOURCE/AL_MIDI.C
    audiolib/SOURCE/MPU401.C
    audiolib/SOURCE/MUSIC.C
    audiolib/SOURCE/GMTIMBRE.C
    audiolib/SOURCE/PORTABLE_DPMI.C
    audiolib/SOURCE/PORTABLE_UNSUPPORTED.C
    audiolib/SOURCE/PORTABLE_INTERRUPT.C
    audiolib/SOURCE/PORTABLE_BLASTER_MUSIC.C
    audiolib/SOURCE/FX_MAN.C
    audiolib/SOURCE/MULTIVOC.C
    audiolib/SOURCE/PITCH.C
    audiolib/SOURCE/PORTABLE_MIX.C
    audiolib/SOURCE/PORTABLE_DMA.C
    audiolib/SOURCE/PORTABLE_BLASTER_DIGITAL.C
    source/PORTABLE_LEGACY.C
)

set(DUKE_SOURCES
    ${DUKE_GAME_SOURCES}
    ${DUKE_BUILD_SOURCES}
)

# Upper-case .C is normally classified as C++ by CMake.  These files were
# compiled by Watcom wcc386 and are C sources.
set_source_files_properties(${DUKE_SOURCES} PROPERTIES
    LANGUAGE C
    COMPILE_OPTIONS "-std=gnu90"
)

message(STATUS "Duke3D source files:")
foreach (SRC_FILE IN LISTS DUKE_SOURCES)
    message(STATUS "${SRC_FILE}")
endforeach ()
message(STATUS "")

add_executable(${PROJECT_NAME}
    ${DUKE_SOURCES}
)

include(${CMAKE_CURRENT_SOURCE_DIR}/api/native-dos-runtime.cmake)
native_dos_runtime_attach(${PROJECT_NAME})

# EZ applications carry their C runtime startup code in the relocatable image.
# The six __ez_*_array_* linker-style boundary symbols intentionally remain
# unresolved here; elf2ez synthesizes them after it has discovered and laid out
# the reachable startup-array sections.
target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/api/crt0.c
    ${CMAKE_CURRENT_SOURCE_DIR}/api/crt0.S
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_SOURCE_DIR}/SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/audiolib/SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/audiolib/PUBLIC/INCLUDE
)


pico_set_program_name(${PROJECT_NAME} "ARM DOS Applications: duke3d")
pico_set_program_version(${PROJECT_NAME} "duke3d")

target_link_libraries(${PROJECT_NAME} PRIVATE
#    pico_runtime
#    pico_stdlib
)

#family_configure_device_example(${PROJECT_NAME} noos)
# -fPIC -shared
target_link_options(${PROJECT_NAME} PRIVATE -Xlinker --print-memory-usage -r)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
target_compile_options(${PROJECT_NAME} PUBLIC
        -fno-jump-tables  # suppress __gnu_thumb1_case_uni symbol generation
        -fstack-usage       # emit per-function ARM stack frames (*.su)
        -Wno-error=suggest-attribute=format
        -Wno-error=cast-qual
        -Wno-error=unused-parameter
        -Wno-error=conversion
        -Wno-error=format=
        -Wno-error=sign-compare
        -Wno-error=missing-field-initializers
        -Wno-error=switch
        -Wno-error=implicit-fallthrough=
        -Wno-error=stringop-truncation
        -Wno-error=restrict
        -w
)
endif ()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ELF_MODE
    DIAG=1
)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${BUILD_NAME}")

pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 0)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    # The native loader consumes a relocatable ELF, so the link intentionally
    # uses -r.  GNU ld consequently permits unresolved symbols.  Reject them
    # explicitly here: native applications have no dynamic linker which could
    # resolve such references at run time.
    COMMAND ${CMAKE_COMMAND}
        -DNM=${CMAKE_NM}
        -DINPUT=$<TARGET_FILE:${PROJECT_NAME}>
        -DALLOW_UNRESOLVED=__ez_preinit_array_start,__ez_preinit_array_end,__ez_init_array_start,__ez_init_array_end,__ez_fini_array_start,__ez_fini_array_end
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_unresolved.cmake
    # Convert the relocatable ARM ELF with the repository host tool.
    # elf2ez derives the .exe name from its input, so stage the ET_REL
    # temporarily in apps/compiled and remove it after conversion.
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:${PROJECT_NAME}>
        ${COMPILED_DIR}/${BUILD_NAME}.com
    COMMAND ${ELF2EZ_TOOL}
        /y ${COMPILED_DIR}/${BUILD_NAME}.com
#    COMMAND ${CMAKE_COMMAND} -E rm -f
#        ${COMPILED_DIR}/${BUILD_NAME}.com
)
