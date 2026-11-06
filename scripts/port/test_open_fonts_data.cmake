# Script-mode wrapper for the open-font embedder, so the test builds the same
# generated data file as the game: cmake -DOUT=<file.c> -P this-file
set(CMAKE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
get_filename_component(dir "${OUT}" DIRECTORY)
set(CMAKE_BINARY_DIR "${dir}/cmake")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
function(set_property)
endfunction()
include("${CMAKE_SOURCE_DIR}/port/fast3d/open_fonts_embed.cmake")
midas_embed_open_fonts(generated)
file(COPY_FILE "${generated}" "${OUT}")
