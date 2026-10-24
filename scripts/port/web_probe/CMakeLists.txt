cmake_minimum_required(VERSION 3.16)
project(midas_web_probe C)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Use emcmake: this probe must execute as Wasm32")
endif()

get_filename_component(MIDAS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

foreach(layout compact high)
    add_executable(probe_${layout} probe.c)
    target_include_directories(probe_${layout} PRIVATE
        ${MIDAS_ROOT} ${MIDAS_ROOT}/include ${MIDAS_ROOT}/include/ultra64
        ${MIDAS_ROOT}/include/PR ${MIDAS_ROOT}/src ${MIDAS_ROOT}/src/game
        ${MIDAS_ROOT}/port/include)
    target_compile_definitions(probe_${layout} PRIVATE
        VERSION_US LANG_US REFRESH_NTSC _LANGUAGE_C)
    target_compile_options(probe_${layout} PRIVATE
        -O2 -ffreestanding -funsigned-char -fno-strict-aliasing -fwrapv -ffp-contract=off)
    target_link_options(probe_${layout} PRIVATE
        -sENVIRONMENT=node -sASSERTIONS=1 -sALLOW_MEMORY_GROWTH=1
        -sSTACK_SIZE=16777216 -sMAXIMUM_MEMORY=1073741824)
    set_target_properties(probe_${layout} PROPERTIES SUFFIX ".js")
    if(layout STREQUAL "high")
        target_compile_definitions(probe_${layout} PRIVATE PROBE_HIGH=1)
        target_link_options(probe_${layout} PRIVATE -sGLOBAL_BASE=268435456
            -sINITIAL_MEMORY=301989888)
    else()
        target_compile_definitions(probe_${layout} PRIVATE PROBE_HIGH=0)
        target_link_options(probe_${layout} PRIVATE -sGLOBAL_BASE=1024
            -sINITIAL_MEMORY=33554432)
    endif()
endforeach()
