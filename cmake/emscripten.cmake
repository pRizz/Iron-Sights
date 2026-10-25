# Browser platform configuration; native compiler/linker options stay separate.
set(GE_WEB_LINK_OPT "2" CACHE STRING "Wasm link optimization: 0 for fast runtime debugging, 2 for release")
if(NOT GE_WEB_LINK_OPT MATCHES "^[0-3]$")
    message(FATAL_ERROR "GE_WEB_LINK_OPT must be 0, 1, 2 or 3")
endif()
add_compile_definitions(GE_WEB=1 strtol=ge_strtol)
set_source_files_properties(src/str.c PROPERTIES
    COMPILE_DEFINITIONS "toupper=ge_n64_toupper;isdigit=ge_n64_isdigit;isalpha=ge_n64_isalpha;isspace=ge_n64_isspace")
# Inherited decompiled interfaces predate C99. Preserve their native build
# policy while declaration conflicts are fixed; do not demote linker failures.
add_compile_options(-Wno-error=implicit-function-declaration
    -Wno-error=int-conversion -Wno-error=incompatible-pointer-types
    -ffp-contract=off
    $<$<COMPILE_LANGUAGE:CXX>:-idirafter${CMAKE_SOURCE_DIR}/include>
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_SOURCE_DIR}/port/web/compat.h>)

function(midas_web_sources source_variable)
    if(GE_ROMDATA_FALLBACK)
        message(FATAL_ERROR "The web target must load assets from a user-supplied ROM")
    endif()
    set(sources ${${source_variable}})
    list(REMOVE_ITEM sources ${CMAKE_SOURCE_DIR}/port/src/port_state.c)
    list(APPEND sources ${CMAKE_SOURCE_DIR}/port/web/state.c)
    list(APPEND sources ${CMAKE_SOURCE_DIR}/port/web/runtime.c)
    list(APPEND sources ${CMAKE_SOURCE_DIR}/port/web/logo.c)
    if(GE_HEADLESS)
        list(APPEND sources ${CMAKE_SOURCE_DIR}/port/web/headless.c)
    else()
        list(REMOVE_ITEM sources ${CMAKE_SOURCE_DIR}/port/src/raphnet.c)
        list(APPEND sources ${CMAKE_SOURCE_DIR}/port/web/devices.c)
        list(APPEND sources ${CMAKE_SOURCE_DIR}/port/web/sampler.cpp)
        list(APPEND sources ${CMAKE_SOURCE_DIR}/port/web/lifecycle.c)
        list(APPEND sources ${CMAKE_SOURCE_DIR}/port/web/touch.c)
    endif()
    set(${source_variable} ${sources} PARENT_SCOPE)
    # These TUs synthesize segments; Wasm's assembler has no native -Wa,-I path.
    set_source_files_properties(port/src/segments.c port/src/ramrom_data.c
        port/src/port_weapon_images.c PROPERTIES COMPILE_OPTIONS "")
endfunction()

function(midas_web_assets source_variable)
    find_program(WEB_ASSET_PYTHON NAMES python3 REQUIRED)
    set(generator ${CMAKE_SOURCE_DIR}/scripts/port/web_logo.py)
    set(original ${CMAKE_SOURCE_DIR}/assets/rarewarelogo.c)
    set(generated ${CMAKE_BINARY_DIR}/generated/web_logo.c)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${generator} ${original})
    execute_process(COMMAND ${WEB_ASSET_PYTHON} ${generator} --output ${generated}
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "ROM-free web logo generation failed")
    endif()
    set(sources ${${source_variable}})
    list(REMOVE_ITEM sources assets/rarewarelogo.c)
    list(APPEND sources ${generated})
    set(${source_variable} ${sources} PARENT_SCOPE)
    include_directories(${CMAKE_SOURCE_DIR}/port/web)
endfunction()

function(midas_web_target target)
    set(runtime_methods "FS,callMain,ENV")
    if(GE_VIDEO)
        string(APPEND runtime_methods ",IDBFS")
    endif()
    set_target_properties(${target} PROPERTIES SUFFIX ".mjs")
    target_link_options(${target} PRIVATE
        -O${GE_WEB_LINK_OPT} -g2 -Wl,--error-limit=0 -Wl,--fatal-warnings
        -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createMidas
        -sENVIRONMENT=web,node -sFORCE_FILESYSTEM=1
        -sEXPORTED_RUNTIME_METHODS=${runtime_methods}
        -sINVOKE_RUN=0 -sEXIT_RUNTIME=1 -sASSERTIONS=1
        -sASYNCIFY=1 -sASYNCIFY_STACK_SIZE=1048576
        -sGLOBAL_BASE=268435456 -sSTACK_SIZE=16777216
        -sINITIAL_MEMORY=536870912 -sMAXIMUM_MEMORY=1073741824
        -sALLOW_MEMORY_GROWTH=1 -Wl,-Map=${CMAKE_BINARY_DIR}/ProjectMidas.map)
    if(GE_VIDEO)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            ${CMAKE_SOURCE_DIR}/port/web/sampler.glsl)
        file(READ ${CMAKE_SOURCE_DIR}/port/web/sampler.glsl MIDAS_WEB_SAMPLER_GLSL)
        configure_file(${CMAKE_SOURCE_DIR}/port/web/sampler_shader.h.in
            ${CMAKE_BINARY_DIR}/generated/web_sampler_shader.h @ONLY)
        target_include_directories(${target} PRIVATE ${CMAKE_BINARY_DIR}/generated)
        target_compile_options(${target} PRIVATE --use-port=sdl2)
        target_link_options(${target} PRIVATE --use-port=sdl2 -lidbfs.js
            -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/tools/gamecontrollerdb.txt
                ${CMAKE_SOURCE_DIR}/tools/gamecontrollerdefault.txt
                $<TARGET_FILE_DIR:${target}>
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${target}>/Data
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/tools/shaders_base.txt $<TARGET_FILE_DIR:${target}>/Data/shaders_base.txt
            VERBATIM)
    endif()
endfunction()
