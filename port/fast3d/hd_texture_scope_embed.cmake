# Embed the HD texture scope (port/fast3d/texture_packs: our environment
# inventory and reviewed palette aliases, no artwork) into a generated C
# source at configure time, in plain CMake. Editing a file re-runs configure
# (FORK-053).
function(midas_embed_hd_texture_scope out_var)
    set(out "${CMAKE_BINARY_DIR}/generated/hd_texture_scope_data.c")
    set(text "/* Generated from port/fast3d/texture_packs by port/fast3d/hd_texture_scope_embed.cmake; do not edit. */\n#include <stddef.h>\n")
    foreach(name hd_level_sources hd_level_native_aliases hd_dam_native_aliases)
        set(path "${CMAKE_SOURCE_DIR}/port/fast3d/texture_packs/${name}.json")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
        file(READ "${path}" hex HEX)
        string(LENGTH "${hex}" digits)
        math(EXPR size "${digits} / 2")
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
        string(APPEND text "const unsigned char ${name}_json[] = {${bytes}};\nconst size_t ${name}_json_size = ${size};\n")
    endforeach()
    file(WRITE "${out}.tmp" "${text}")
    configure_file("${out}.tmp" "${out}" COPYONLY)
    set(${out_var} "${out}" PARENT_SCOPE)
endfunction()
