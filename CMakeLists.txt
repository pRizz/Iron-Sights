cmake_minimum_required(VERSION 3.16)
project(ge007 C)

# GoldenEye PC port build (docs/port/PORT-PLAN.md). 32-bit by default, which
# defuses the codebase's pointer<->s32 casts and keeps Gfx 8 bytes; 64-bit
# (Linux and macOS only) by -DGE_POINTER_BITS=64, per scripts/port/native_64bit_plan.md.
# The N64 ROM build stays on the classic Makefile; this builds native and Wasm.

# Pointer width of the native build. Code never reads this; it reads IS_64_BIT.
# Validated before anything else can fail, so the message is the one reported.
set(GE_POINTER_BITS 32 CACHE STRING "Pointer width of the native build: 32 (default) or 64")
if(NOT GE_POINTER_BITS MATCHES "^(32|64)$")
    message(FATAL_ERROR "GE_POINTER_BITS must be 32 or 64")
endif()
if(GE_POINTER_BITS EQUAL 64)
    if(EMSCRIPTEN)
        message(FATAL_ERROR "The web build is 32-bit; GE_POINTER_BITS=64 is not supported")
    endif()
    if(WIN32)
        message(FATAL_ERROR "Windows x64 is outside the 64-bit plan; see scripts/port/native_64bit_plan.md")
    endif()
    # option() lives further down; the plain cache lookup is enough here.
    if(GE_ROMDATA_FALLBACK)
        message(FATAL_ERROR "GE_POINTER_BITS=64 requires runtime ROM loading (GE_ROMDATA_FALLBACK=OFF)")
    endif()
endif()

# macOS (scripts/port/macos_plan.md): Apple Silicon and 64-bit pointers only.
# APPLE is false for the browser build and a MinGW cross build made on a Mac.
if(APPLE)
    if(NOT GE_POINTER_BITS EQUAL 64)
        message(FATAL_ERROR "macOS builds are 64-bit only: configure with -DGE_POINTER_BITS=64")
    endif()
    if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        message(FATAL_ERROR "macOS builds are Apple Silicon only")
    endif()
endif()

# The converter oracle (LP64-004, scripts/port/native_64bit_lp64_004.md): a
# 32-bit headless build that also compiles the 64-bit ROM data converters and
# compares their output with the game's in-place conversions. Never shipped;
# validated where GE_HEADLESS is known, below.
option(GE_LP64_ORACLE "32-bit headless test build that checks the 64-bit ROM data converters against the in-place conversions" OFF)

# The comparison build (LP64-006 Task 4, FORK-089): at either width, headless or
# with video, it links port_compare_math.c's tanf, atanf and cosf in place of
# libm's, so the i686, x86_64 and aarch64 builds compute them alike, and turns on the
# play harness (PORT_PLAY_HARNESS, port_playlog.h) in a 32-bit video build.
# Never a shipped build: package_linux.py and package_windows.py refuse it.
option(GE_COMPARE "LP64-006 comparison build: in-tree tanf/atanf/cosf and the play harness at either width. Never a shipped build" OFF)
option(GE_COMPARE_LIBM "With GE_COMPARE: keep libm's tanf, atanf and cosf (the twin that shows what the C library changes)" OFF)
if(GE_COMPARE_LIBM AND NOT GE_COMPARE)
    message(FATAL_ERROR "GE_COMPARE_LIBM=ON needs GE_COMPARE=ON")
endif()

set(CMAKE_C_STANDARD 11)

# No warnings in the 32-bit and web builds; the 64-bit branch replaces this.
set(GE_WARNING_OPTIONS -w)
if(EMSCRIPTEN)
    include(cmake/emscripten.cmake)
elseif(GE_POINTER_BITS EQUAL 64)
    # -ffp-contract=off: no fused multiply-add, so floats round as on the N64 FPU
    # (the 32-bit build gets that from -mfpmath=sse; x86-64 and arm64 default to SSE/NEON).
    add_compile_options(-ffp-contract=off)
    # The LP64-002 gate: code mixing pointers with 32-bit integers does not
    # compile at 64 bits, so neither C nor C++ has -w (which would switch
    # -Werror= off). C++ needs only int-to-pointer-cast: it rejects the other
    # two conversions without a flag. Every other class the 64-bit build
    # printed is switched off by name, with the warnings it printed (headless /
    # video trees, GCC 13.3 on aarch64 and x86_64, identical). C:
    # incompatible-pointer-types 1820 / 1828, implicit-function-declaration
    # 226 / 226 (these two recounted at LP64-006's final review, each C file
    # compiled with -fsyntax-only and the class on), discarded-qualifiers 7,
    # format-truncation 4, overflow 2,
    # format-overflow 2, unused-result 1. C++ (video only):
    # deprecated-declarations 3 (std::filesystem::u8path, weapon_cache.cpp).
    # The first two C classes are left for later milestones; GCC 14 and later
    # make them errors by default, and naming them here keeps the verdict the
    # same there (only GCC 13.3 was run). The C log is clean. The C++ log keeps
    # eleven warnings that have no -W option, all in port/fast3d: ten
    # "initialized and declared 'extern'" (gfx_pc.cpp 8, gfx_roomcache.inl 2)
    # and one "null character(s) preserved in literal" (gfx_sdl2.cpp).
    # discarded-qualifiers is GCC's name: clang does not know it, and under
    # port_macos.c's -Werror an unknown warning option is an error (MAC-001).
    set(GE_WARNING_OPTIONS
        $<$<COMPILE_LANGUAGE:CXX>:-Werror=int-to-pointer-cast>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-declarations>
        $<$<COMPILE_LANGUAGE:C>:-Werror=int-conversion>
        $<$<COMPILE_LANGUAGE:C>:-Werror=pointer-to-int-cast>
        $<$<COMPILE_LANGUAGE:C>:-Werror=int-to-pointer-cast>
        $<$<COMPILE_LANGUAGE:C>:-Wno-incompatible-pointer-types>
        $<$<COMPILE_LANGUAGE:C>:-Wno-implicit-function-declaration>
        $<$<AND:$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:GNU>>:-Wno-discarded-qualifiers>
        $<$<COMPILE_LANGUAGE:C>:-Wno-format-truncation>
        $<$<COMPILE_LANGUAGE:C>:-Wno-overflow>
        $<$<COMPILE_LANGUAGE:C>:-Wno-format-overflow>
        $<$<COMPILE_LANGUAGE:C>:-Wno-unused-result>)
else()
    add_compile_options(-m32 -msse2 -mfpmath=sse)
    if(GE_LP64_ORACLE)
        # GCC's -w also silences a -Werror=, so the oracle build gives -w to
        # every source but the converters, after add_executable below.
        set(GE_WARNING_OPTIONS)
    endif()
endif()

add_compile_options(
    # Use SSE scalar floats so every operation rounds to single precision like
    # the N64 FPU; x87's 80-bit excess precision flips borderline comparisons
    # (it broke the stan.c tile-walk edge test and room visibility in Egypt).
    -fno-strict-aliasing
    -fwrapv
    -funsigned-char
    # crash-handler EBP-chain walk (port/src/main.c) needs real frame pointers
    -fno-omit-frame-pointer
    # DWARF in every build (incl. Windows PE): zero runtime cost, and it
    # upgrades crash symbolization from symbol+offset to file:line
    # (addr2line/gdb work on the exe directly)
    -g
    # -O2, keeping the flags that defuse decompiled-code UB (-fno-strict-aliasing
    # -fwrapv -funsigned-char -msse2 -mfpmath=sse); verified pixel-identical to
    # the old -Og build (see docs/port/optimisation-inventory.md).
    -O2
    # (Audit recipe: swap the -w in GE_WARNING_OPTIONS for -Wreturn-type and rebuild,
    # since falling off a non-void function is live UB at -O2. Keep the tree clean.)
    ${GE_WARNING_OPTIONS}
    # C-only: game code is self-contained against repo-local headers;
    # fast3d C++ needs hosted stdlib and no ms-extensions quirks.
    $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
    $<$<COMPILE_LANGUAGE:C>:-fms-extensions>
    # A sanitizer build gives each global its own red zone (LP64-005 Task 2):
    # common symbols get none, and -fno-common makes a global defined twice a
    # link error. Shipped builds keep -fcommon. GE_SANITIZE is read here, before
    # its option() line below: a -DGE_SANITIZE=ON is already in the cache, and an
    # unset one reads as false, as the option's OFF default would. The choice
    # stays in -fcommon's place so the plain build's flags keep their order.
    $<$<COMPILE_LANGUAGE:C>:$<IF:$<BOOL:${GE_SANITIZE}>,-fno-common,-fcommon>>
)
# -no-pie: the compiled-in asset objects (below) are built -fno-pic so their
# pointer-laden .data needs no PIC indirection; a PIE final link would reject
# their R_386_32 relocations. MinGW PE has no PIE concept and its gcc driver
# rejects the flag, so it is ELF-only.
if(WIN32)
    # Statically link libstdc++/libgcc so the exe needs no extra DLLs, set the
    # image base above the N64 segment range so gfx_pc.cpp seg_addr can tell
    # addresses from segment refs, and use a 16MB stack (recursive room/portal
    # walks overflow 1MB).
    add_link_options(-m32 -static-libgcc -static-libstdc++
                     -Wl,--image-base=0x10000000 -Wl,--disable-dynamicbase
                     -Wl,--stack,16777216)
    link_libraries(dbghelp comctl32 comdlg32)
elseif(NOT EMSCRIPTEN AND GE_POINTER_BITS EQUAL 32)
    add_link_options(-m32 -no-pie)
endif()

add_compile_definitions(
    VERSION_US LANG_US REFRESH_NTSC
    LEFTOVERDEBUG LEFTOVERSPECTRUM BUGFIX_R0
    NONMATCHING AVOID_UB _LANGUAGE_C
)
if(WIN32)
    # MinGW's ANSI-stdio layer defines static inline sprintf/printf wrappers
    # that collide with GE's own sprintf (src/sprintf.c, bespoke semantics).
    add_compile_definitions(__USE_MINGW_ANSI_STDIO=0)
endif()

# port/include: shim<->scheduler interface (port_libultra.h). src/libultra:
# lets vitbl.c resolve <io/viint.h>. Both appended LAST, so no include that
# resolved before can be shadowed.
set(PORT_PUBLIC_INCLUDE include)
if(EMSCRIPTEN)
    # libc++ must find its standard-header wrappers before the N64 headers.
    set(PORT_PUBLIC_INCLUDE $<$<COMPILE_LANGUAGE:C>:${CMAKE_SOURCE_DIR}/include>)
endif()
set(PORT_GAME_INCLUDE src src/game src/inflate)
if(GE_POINTER_BITS EQUAL 64)
    # The game's src/sched.h shadows the C library's <sched.h>, which GCC 13's
    # libstdc++ reaches from C++20 <chrono> through <pthread.h>; the C++ renderer
    # (port/fast3d) needs no header from the game folders. The 32-bit list is
    # unchanged: the Linux release image's GCC 11 (Ubuntu 22.04) compiled and
    # linked the 32-bit renderer with it (at 79138e5); that its <chrono> never
    # reaches <sched.h> is inferred from that, not traced.
    set(PORT_GAME_INCLUDE $<$<COMPILE_LANGUAGE:C>:${CMAKE_SOURCE_DIR}/src>
        $<$<COMPILE_LANGUAGE:C>:${CMAKE_SOURCE_DIR}/src/game>
        $<$<COMPILE_LANGUAGE:C>:${CMAKE_SOURCE_DIR}/src/inflate>)
endif()
include_directories(. ${PORT_PUBLIC_INCLUDE} include/ultra64 include/PR ${PORT_GAME_INCLUDE}
                    port/include src/libultra)

# Game logic: everything in src/game.
file(GLOB GAME_SRC CONFIGURE_DEPENDS src/game/*.c)

# Engine layer: the src/*.c files the N64 build compiles, minus N64-only
# machinery the port deletes (boot chain, TLB pager, rmon, usb, crash handler).
file(GLOB ENGINE_SRC_ALL CONFIGURE_DEPENDS src/*.c)
set(ENGINE_EXCLUDE boot tlb_manage tlb_hardwire rmon usb debug crash motor)
# motor.c is GE's own osMotor* implementation over raw PIF-ram DMA - not
# backable by the shim; port/src/motor_stub.c provides the API.
set(ENGINE_SRC)
foreach(f ${ENGINE_SRC_ALL})
    get_filename_component(base ${f} NAME_WE)
    if(NOT base IN_LIST ENGINE_EXCLUDE)
        list(APPEND ENGINE_SRC ${f})
    endif()
endforeach()

# gu math: explicit list — only the files resolving actually-undefined gu/sins/
# coss symbols. Do NOT glob the dir: sqrtf.s/libm_vals.s are asm-backed, and
# sinf.c/cosf.c (fsin/fcos) are superseded by libm sinf/cosf (link -lm below).
set(GU_SRC
    src/libultra/gu/align.c        # guAlignF
    src/libultra/gu/coss.c         # coss (s16 sine-table cosine)
    src/libultra/gu/lookat.c       # guLookAt
    src/libultra/gu/lookatref.c    # guLookAtReflect
    src/libultra/gu/mtxutil.c      # guMtxF2L (+ guMtxIdentF used by ortho/perspective)
    src/libultra/gu/normalize.c    # guNormalize
    src/libultra/gu/ortho.c        # guOrtho
    src/libultra/gu/perspective.c  # guPerspective/guPerspectiveF
    src/libultra/gu/rotate.c       # guRotate/guRotateF
    src/libultra/gu/scale.c        # guScale/guScaleF
    src/libultra/gu/sins.c         # sins (s16 sine-table)
    src/libultra/gu/translate.c    # guTranslate
    # ---- AUDIO: the real AL synthesizer, which only builds Acmd lists that
    # port/src/audio_pc.c executes. bnkf.c is deliberately absent because
    # audio_stubs.c has the big-endian-aware alBnkfNew.
    src/libultra/audio/sl.c
    src/libultra/audio/synthesizer.c
    src/libultra/audio/event.c
    src/libultra/audio/heapinit.c
    src/libultra/audio/heapalloc.c
    src/libultra/audio/cents2ratio.c
    src/libultra/audio/copy.c
    src/libultra/audio/synaddplayer.c
    src/libultra/audio/synallocfx.c
    src/libultra/audio/synallocvoice.c
    src/libultra/audio/syndelete.c
    src/libultra/audio/synfreevoice.c
    src/libultra/audio/synsetfxmix.c
    src/libultra/audio/synsetpan.c
    src/libultra/audio/synsetpitch.c
    src/libultra/audio/synsetpriority.c
    src/libultra/audio/synsetvol.c
    src/libultra/audio/synstartvoice.c
    src/libultra/audio/synstartvoiceparam.c
    src/libultra/audio/synstopvoice.c
    src/libultra/audio/load.c
    src/libultra/audio/save.c
    src/libultra/audio/resample.c
    src/libultra/audio/filter.c
    src/libultra/audio/mainbus.c
    src/libultra/audio/auxbus.c
    src/libultrare/audio/drvrNew.c
    src/libultrare/audio/reverb.c
    src/libultrare/audio/env.c
    # MUSIC: the compressed-MIDI sequencer
    src/libultra/audio/cseq.c
    src/libultra/audio/csplayer.c
    src/libultra/audio/cspgetstate.c
    src/libultra/audio/cspplay.c
    src/libultra/audio/cspsetseq.c
    src/libultra/audio/cspsetvol.c
    src/libultra/audio/cspstop.c
    src/libultra/audio/seqpsetbank.c
    src/libultra/audio/seqplayer.c   # __lookupVoice/__vsVol/__initFromBank... shared voice machinery
    src/libultra/audio/seq.c
)

# ---------------------------------------------------------------------------
# Compiled-in assets.
#
# The N64 build compiles each asset .c into its own elf, so every Model.c reuses
# the same internal symbol names (proptextures, ModelNode_0x018, ...) and
# stan/setup/bg files reuse tile_0/padlist/header. Globbing them into one
# executable would cause thousands of duplicate definitions, so each asset TU is
# compiled separately and objcopy'd so only its file symbol (CarmourguardZ,
# Ump_setupameZ, ...) stays global:
#
#   DIRECT assets define their file symbol in the C; objcopy -G keeps just it.
#   SEG    assets (models, bg) don't define it anywhere (on N64 it's the
#          address of the linked blob). The TU is compiled with
#          -fno-toplevel-reorder -malign-data=abi -fno-common
#          -fno-zero-initialized-in-bss, which reproduces the N64 file layout
#          in .data byte-for-byte, and objcopy --add-symbol plants the file
#          symbol at .data offset 0. The layout must match because
#          load_object_fill_header finds Textures/RootNode by offset arithmetic.
#
# The per-file list is generated (port/generated/asset_manifest.cmake) by
# scripts/port/gen_asset_aliases.py. Rerun it if assets or the link surface
# change; it is not a configure-time glob.
#
# Known caveats: bg .c interior point tables, ANIM_DATA_* streams and
# oddtextures texel data are big-endian blobs.
set(ASSET_C_FLAGS
    -std=gnu11
    -m32 -malign-data=abi
    -fms-extensions -ffreestanding -fno-strict-aliasing -fwrapv -funsigned-char
    -fno-common -fno-zero-initialized-in-bss -fno-toplevel-reorder -fno-pic
    -Og -w
    -DVERSION_US -DLANG_US -DREFRESH_NTSC
    -DLEFTOVERDEBUG -DLEFTOVERSPECTRUM -DBUGFIX_R0
    -DNONMATCHING -DAVOID_UB -D_LANGUAGE_C
    -I${CMAKE_SOURCE_DIR}
    -I${CMAKE_SOURCE_DIR}/include
    -I${CMAKE_SOURCE_DIR}/include/ultra64
    -I${CMAKE_SOURCE_DIR}/include/PR
    -I${CMAKE_SOURCE_DIR}/src
    -I${CMAKE_SOURCE_DIR}/src/game
    -I${CMAKE_SOURCE_DIR}/src/inflate
    -I${CMAKE_SOURCE_DIR}/port/include
    -I${CMAKE_SOURCE_DIR}/src/libultra)

if(NOT CMAKE_OBJCOPY)
    set(CMAKE_OBJCOPY objcopy)
endif()
set(ASSET_OBJ_DIR ${CMAKE_BINARY_DIR}/asset_obj)
file(MAKE_DIRECTORY ${ASSET_OBJ_DIR})

# i386 PE/COFF prefixes C symbols with `_` (ELF doesn't), so add it here to the
# manifest's C-level names before objcopy uses them (like
# port/include/asmsym.h).
if(WIN32)
    set(ASSET_SYM_PREFIX "_")
else()
    set(ASSET_SYM_PREFIX "")
endif()

# NOTE: setup ai-list bytes are built from aicommands2.h's macros, so it must
# stay in these DEPENDS; if an AI macro edit has no effect, check the asset
# object is stale.
# Asset TU whose C defines the file symbol itself (stan/setup/brief/text):
# localize everything else.
# Every file asset, including inline-texture menu models, comes from the user's
# ROM; with GE_ROMDATA_FALLBACK=OFF all generated asset objects are dropped and
# their weak file symbols resolve to NULL (ob_end.seg always lives in ob_seg.s).
set(PORT_ASSET_KEEP_SYMS)
function(port_asset_record obj sym)
    if(${sym} IN_LIST PORT_ASSET_KEEP_SYMS)
        set_property(GLOBAL APPEND PROPERTY PORT_ASSET_OBJS_KEEP ${obj})
    else()
        set_property(GLOBAL APPEND PROPERTY PORT_ASSET_OBJS_SERVED ${obj})
    endif()
endfunction()

function(port_add_asset_direct src sym)
    set(obj ${ASSET_OBJ_DIR}/${sym}.o)
    add_custom_command(OUTPUT ${obj}
        COMMAND ${CMAKE_C_COMPILER} ${ASSET_C_FLAGS} -c ${CMAKE_SOURCE_DIR}/${src} -o ${obj}.raw
        COMMAND ${CMAKE_OBJCOPY} -G ${ASSET_SYM_PREFIX}${sym} ${obj}.raw ${obj}
        DEPENDS ${CMAKE_SOURCE_DIR}/${src}
                ${CMAKE_SOURCE_DIR}/src/bondtypes.h
                ${CMAKE_SOURCE_DIR}/src/bondconstants.h
                ${CMAKE_SOURCE_DIR}/src/bondaicommands.h
                ${CMAKE_SOURCE_DIR}/src/aicommands2.h
        COMMENT "asset ${sym}" VERBATIM)
    port_asset_record(${obj} ${sym})
endfunction()

# Asset TU with no file symbol of its own (Model.c, bg .c): localize all
# internals and plant the file symbol at .data offset 0 (the file start).
function(port_add_asset_seg src sym)
    set(obj ${ASSET_OBJ_DIR}/${sym}.o)
    add_custom_command(OUTPUT ${obj}
        COMMAND ${CMAKE_C_COMPILER} ${ASSET_C_FLAGS} -c ${CMAKE_SOURCE_DIR}/${src} -o ${obj}.raw
        COMMAND ${CMAKE_OBJCOPY} -G ${ASSET_SYM_PREFIX}${sym} --add-symbol ${ASSET_SYM_PREFIX}${sym}=.data:0,global,object ${obj}.raw ${obj}
        DEPENDS ${CMAKE_SOURCE_DIR}/${src}
                ${CMAKE_SOURCE_DIR}/src/bondtypes.h
                ${CMAKE_SOURCE_DIR}/src/bondconstants.h
                ${CMAKE_SOURCE_DIR}/src/bondaicommands.h
                ${CMAKE_SOURCE_DIR}/src/aicommands2.h
        COMMENT "asset ${sym}" VERBATIM)
    port_asset_record(${obj} ${sym})
endfunction()

include(${CMAKE_SOURCE_DIR}/port/generated/asset_manifest.cmake)
get_property(ASSET_OBJS_KEEP GLOBAL PROPERTY PORT_ASSET_OBJS_KEEP)
get_property(ASSET_OBJS_SERVED GLOBAL PROPERTY PORT_ASSET_OBJS_SERVED)
# GE_ROMDATA_FALLBACK OFF: drop the served asset objects (models/bg/setups/
# stans/text/briefs) - the exe then carries none of Rare's data and the ROM
# is mandatory. ON keeps them for the GE_ROMDATA=0 / missing-ROM fallback.
if(GE_ROMDATA_FALLBACK)
    set(ASSET_OBJS ${ASSET_OBJS_KEEP} ${ASSET_OBJS_SERVED})
else()
    set(ASSET_OBJS ${ASSET_OBJS_KEEP})
    message(STATUS "GE_ROMDATA_FALLBACK=OFF: dropping the served asset objects from the exe (ROM mandatory)")
endif()
set_source_files_properties(${ASSET_OBJS} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)

# Loose single-TU asset data (unique symbol names; compiled like game code):
set(ASSET_LOOSE_SRC
    assets/oddtextures.c         # s_*image + globalDL_* (texel data is BE: renders wrong until swapped; resolves image_bank externs)
    assets/rarewarelogo.c        # logo data; only reachable once _rarewarelogoSegment* aliases land (romdata wave)
    assets/font_dl.c             # dlBasicGeometry / dlZBufferGeometry / dlFastPipelineSetup: front.c/lv.c/title.c
                                 # gSPDisplayList(&dl...) them BY SYMBOL, so they stay in every build (the fontdl
                                 # SEGMENT copy comes off the ROM); 184 bytes
)
# ROM loading (docs/port/romdata.md): animation + fonts come off the
# user's ROM; these compiled copies stay only for GE_ROMDATA_FALLBACK (below).
set(ASSET_LOOSE_FALLBACK_SRC
    assets/animationtable_data.c    # animation_data segment blob (see animdata.c)
    assets/animationtable_entries.c # animation frame streams (see animdata.c)
    assets/font/fontBankGothic.c # TEXT WAVE: kerning + chartable + glyph bytes (composed by portFontBuildSegments)
    assets/font/fontZurichBold.c # TEXT WAVE: chartable + glyph bytes (kerning not extracted; zeros)
)
# animdata.c rebuilds the N64 animation segment images from these blobs by
# offset arithmetic - they must tile contiguously in declaration order.
# Kept at -Og with the tree at -O2: data-only TUs, layout is the contract.
set_source_files_properties(assets/animationtable_data.c assets/animationtable_entries.c
    PROPERTIES COMPILE_OPTIONS "-fno-toplevel-reorder;-malign-data=abi;-Og")
# ---------------------------------------------------------------------------

# Port layer (grows as PD-derived pieces land; see docs/port/).
file(GLOB PORT_SRC CONFIGURE_DEPENDS port/src/*.c)
# port_lp64.c (the 64-bit address check) has no code at 32 bits, but even its
# empty object adds a compiler ident to the Windows executable's .rdata.
# The 32-bit oracle build compiles it for the asset-class names.
if(NOT GE_POINTER_BITS EQUAL 64 AND NOT GE_LP64_ORACLE)
    list(REMOVE_ITEM PORT_SRC ${CMAKE_SOURCE_DIR}/port/src/port_lp64.c
        ${CMAKE_SOURCE_DIR}/port/src/port_lp64_synth.c ${CMAKE_SOURCE_DIR}/port/src/port_lp64_budget.c)
endif()
# The 64-bit stops' message box (port_lp64_notice.c, LP64-007 Task 6): only a
# 64-bit build has those stops (the oracle build compiles port_lp64.c for its
# class names only).
if(NOT GE_POINTER_BITS EQUAL 64)
    list(REMOVE_ITEM PORT_SRC ${CMAKE_SOURCE_DIR}/port/src/port_lp64_notice.c)
endif()
# The XWayland pointer reset (port_xwayland_pointer.c, FORK-091) is for Linux
# native video builds (the video lists below drop it with input_pc.c); elsewhere
# its empty object would still add a compiler ident.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(REMOVE_ITEM PORT_SRC ${CMAKE_SOURCE_DIR}/port/src/port_xwayland_pointer.c)
endif()
# The macOS host functions (port_macos.h, MAC-001): the file includes dyld's
# headers, so only a macOS build compiles it, and under every warning.
if(APPLE)
    set_property(SOURCE ${CMAKE_SOURCE_DIR}/port/src/port_macos.c APPEND PROPERTY
        COMPILE_OPTIONS -Wall -Wextra -Werror)
else()
    list(REMOVE_ITEM PORT_SRC ${CMAKE_SOURCE_DIR}/port/src/port_macos.c)
endif()
# The models' list store (port_gdl_store.h, LP64-006 Task 11b) is the 64-bit
# build's alone: a 32-bit build converts as it always has (objecthandler_2.c).
if(NOT GE_POINTER_BITS EQUAL 64)
    list(REMOVE_ITEM PORT_SRC ${CMAKE_SOURCE_DIR}/port/src/port_gdl_store.c)
endif()
# The ROM data converters (port_romconv.h) exist for 64-bit builds; at 32 bits
# only the oracle build compiles them, so no shipped 32-bit executable holds
# one. They make incompatible pointer types an error at both widths: the
# 64-bit build's -Wno-incompatible-pointer-types would hide a mismatched
# pointer, which the browser build's audit rejects.
file(GLOB ROMCONV_SRC CONFIGURE_DEPENDS port/src/romconv_*.c)
if(ROMCONV_SRC)
    if(NOT GE_POINTER_BITS EQUAL 64 AND NOT GE_LP64_ORACLE)
        list(REMOVE_ITEM PORT_SRC ${ROMCONV_SRC})
    else()
        set_property(SOURCE ${ROMCONV_SRC} APPEND PROPERTY COMPILE_OPTIONS -Werror=incompatible-pointer-types)
    endif()
endif()
if(NOT GE_LP64_ORACLE)
    list(REMOVE_ITEM PORT_SRC ${CMAKE_SOURCE_DIR}/port/src/port_lp64_oracle.c)
endif()
# The comparison build's maths and sound lock (port_audio_lock.h): only in a
# GE_COMPARE build. Its arithmetic must not fuse into multiply-adds at either
# width (64-bit builds already pass this).
if(GE_COMPARE)
    set_property(SOURCE ${CMAKE_SOURCE_DIR}/port/src/port_compare_math.c APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off)
else()
    list(REMOVE_ITEM PORT_SRC ${CMAKE_SOURCE_DIR}/port/src/port_compare_math.c
        ${CMAKE_SOURCE_DIR}/port/src/port_audio_lock.c)
endif()
# Suspend images record 32-bit addresses: 64-bit builds report suspend points
# unavailable (port_state_unavailable.c) instead of compiling the serializer.
if(GE_POINTER_BITS EQUAL 64)
    list(REMOVE_ITEM PORT_SRC ${CMAKE_SOURCE_DIR}/port/src/port_state.c)
else()
    list(REMOVE_ITEM PORT_SRC ${CMAKE_SOURCE_DIR}/port/src/port_state_unavailable.c)
endif()
file(GLOB WEAPON_IMAGE_PACK CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/assets/port/weapon_icons/weapon_icons.rle")
set_source_files_properties(port/src/port_weapon_images.c PROPERTIES
    OBJECT_DEPENDS "${WEAPON_IMAGE_PACK}"
    COMPILE_OPTIONS "-Wa,-I${CMAKE_SOURCE_DIR}")

# GE_HEADLESS: no SDL at all (null renderer, silent audio), for the Linux -m32
# ASan build where no 32-bit SDL exists. GE_SANITIZE adds ASan+UBSan (Linux gcc
# only).
option(GE_HEADLESS "Build without SDL (null renderer, no audio)" OFF)
option(GE_SANITIZE "AddressSanitizer + UBSan build (implies headless)" OFF)
if(GE_SANITIZE)
    # Only default to headless when GE_VIDEO wasn't explicitly requested -
    # a GE_VIDEO=ON + GE_SANITIZE=ON build (real fast3d/SDL2 renderer,
    # e.g. via Docker's from-source 32-bit SDL2) must NOT also pull in
    # headless_stubs.c, or duplicate-definition link errors result
    # (ge_sky_push, gfx_texture_cache_* defined in both gfx_pc.cpp and
    # headless_stubs.c).
    if(NOT GE_VIDEO)
        set(GE_HEADLESS ON)
    endif()
    # -g3 gives file:line in reports and no-sanitize-recover=all makes every UBSan
    # finding fatal; if deliberate decomp UB floods it, exempt specific checks here.
    add_compile_options(-g3 -fsanitize=address -fsanitize=undefined
                        -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address -fsanitize=undefined)
    # gfx_pc.cpp's check of each display-list command word (LP64-003): a
    # development build only. It compiles to nothing at 32 bits.
    add_compile_definitions(GE_LP64_DL_CHECK)
    # ASan redzones put gaps between globals and break animdata's contiguous
    # tiling, so exempt the data-only TUs (manifest assets are already
    # uninstrumented).
    set_source_files_properties(assets/animationtable_data.c assets/animationtable_entries.c
        PROPERTIES COMPILE_OPTIONS "-fno-toplevel-reorder;-malign-data=abi;-Og;-fno-sanitize=all")
    set_source_files_properties(assets/oddtextures.c assets/rarewarelogo.c
        assets/font_dl.c assets/font/fontBankGothic.c assets/font/fontZurichBold.c
        PROPERTIES COMPILE_OPTIONS "-fno-sanitize=all")
endif()
if(GE_LP64_ORACLE)
    if(NOT GE_POINTER_BITS EQUAL 32)
        message(FATAL_ERROR "GE_LP64_ORACLE=ON is a 32-bit build (GE_POINTER_BITS=32)")
    endif()
    if(NOT GE_HEADLESS)
        message(FATAL_ERROR "GE_LP64_ORACLE=ON needs a headless build (GE_HEADLESS=ON)")
    endif()
    add_compile_definitions(GE_LP64_ORACLE)
endif()
if(GE_HEADLESS)
    # No SDL, so no box: the 64-bit stop notice does nothing.
    set_property(SOURCE ${CMAKE_SOURCE_DIR}/port/src/port_lp64_notice.c APPEND PROPERTY
        COMPILE_DEFINITIONS PORT_LP64_NOTICE_HEADLESS)
    list(REMOVE_ITEM PORT_SRC
        ${CMAKE_SOURCE_DIR}/port/src/audio_pc.c
        ${CMAKE_SOURCE_DIR}/port/src/video.c
        ${CMAKE_SOURCE_DIR}/port/src/input_pc.c
        ${CMAKE_SOURCE_DIR}/port/src/port_xwayland_pointer.c
        ${CMAKE_SOURCE_DIR}/port/src/raphnet.c)
    # port_baked_trace.c includes SDL.h for its bake thread; headless has no
    # SDL2, so give it the CI shim, which also selects the thread-free path.
    set_source_files_properties(port/src/port_baked_trace.c PROPERTIES
        INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/scripts/port/sdl_shim")
else()
    list(REMOVE_ITEM PORT_SRC ${CMAKE_SOURCE_DIR}/port/src/headless_stubs.c)
endif()
# segments.c .incbin's the raw image bank (texture wave); ramrom_data.c
# .incbin's the gunbarrel swirl RLE blob and the 14 attract-demo
# recordings. Give the assembler the include paths and re-assemble when
# the blobs change.
set_source_files_properties(port/src/segments.c PROPERTIES
    COMPILE_OPTIONS "-Wa,-I${CMAKE_SOURCE_DIR}/assets/images/combined;-Wa,-I${CMAKE_SOURCE_DIR}/assets/music"
    OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/assets/images/combined/combined.bin;${CMAKE_SOURCE_DIR}/assets/music/sfx.ctl;${CMAKE_SOURCE_DIR}/assets/music/sfx.tbl;${CMAKE_SOURCE_DIR}/assets/music/instruments.ctl;${CMAKE_SOURCE_DIR}/assets/music/instruments.tbl;${CMAKE_SOURCE_DIR}/assets/music/music.sbk")
set(PORT_RAMROM_DEMO_BINS
    ramrom_Dam_1 ramrom_Dam_2 ramrom_Facility_1 ramrom_Facility_2
    ramrom_Facility_3 ramrom_Runway_1 ramrom_Runway_2 ramrom_BunkerI_1
    ramrom_BunkerI_2 ramrom_Silo_1 ramrom_Silo_2 ramrom_Frigate_1
    ramrom_Frigate_2 ramrom_Train)
list(TRANSFORM PORT_RAMROM_DEMO_BINS PREPEND ${CMAKE_SOURCE_DIR}/assets/ramrom/)
list(TRANSFORM PORT_RAMROM_DEMO_BINS APPEND .bin)
set_source_files_properties(port/src/ramrom_data.c PROPERTIES
    COMPILE_OPTIONS "-Wa,-I${CMAKE_SOURCE_DIR}/assets"
    OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/assets/ge007.u.2A4D50.usedby7F008DE4.bin;${PORT_RAMROM_DEMO_BINS}")
# osViModeTable[42]: compile the repo's REAL table (fr.c/sched.c read
# comRegs.hStart/width/xScale + fldRegs[].vStart/yScale/origin from it)
# instead of transcribing data into the shim.
list(APPEND PORT_SRC src/libultrare/io/vitbl.c)

# ---------------------------------------------------------------------------
# Video: fast3d interpreter + SDL2/OpenGL. OFF by default until the
# GE microcode opcode work lands (docs/port/ge-microcode.md); the null
# renderer in pcsched.c stays the headless default.
option(GE_VIDEO "Build the fast3d renderer (SDL2 + OpenGL)" OFF)

# Testing convenience: every solo mission, bonus stage and difficulty
# (incl. 007 mode) selectable regardless of save state. Progression logic
# untouched otherwise - saves still record normally.
option(GE_UNLOCK_ALL "Unlock all missions and difficulties (testing)" OFF)
if(GE_UNLOCK_ALL)
    add_compile_definitions(GE_UNLOCK_ALL=1)
endif()
# GE_COMPARE (declared at the top): the comparison build's defines.
if(GE_COMPARE)
    add_compile_definitions(GE_COMPARE=1)
    if(GE_COMPARE_LIBM)
        add_compile_definitions(GE_COMPARE_LIBM=1)
    endif()
endif()

# By default the exe carries none of Rare's data and exits without the user's ROM.
# -DGE_ROMDATA_FALLBACK=ON keeps compiled-in copies as a no-ROM fallback (see docs/port/romdata.md).
option(GE_ROMDATA_FALLBACK "Keep compiled-in copies of the ROM-served asset classes (GE_ROMDATA=0 / missing-ROM fallback)" OFF)
if(GE_ROMDATA_FALLBACK)
    set_source_files_properties(port/src/segments.c port/src/ramrom_data.c port/src/romdata.c port/src/animdata.c
        PROPERTIES COMPILE_DEFINITIONS GE_ROMDATA_FALLBACK=1)
    list(APPEND ASSET_LOOSE_SRC ${ASSET_LOOSE_FALLBACK_SRC})
else()
    # The normal player build reads these blobs from the user's ROM at runtime.
    # Do not require extracted files merely to compile the guarded .incbin TUs.
    set_source_files_properties(port/src/segments.c port/src/ramrom_data.c
        PROPERTIES OBJECT_DEPENDS "")
endif()

if(EMSCRIPTEN)
    midas_web_sources(PORT_SRC)
    midas_web_assets(ASSET_LOOSE_SRC)
endif()

if(GE_VIDEO)
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 20) # gfx_cc.h uses operator<=> (PD builds C++20)
    include(port/fast3d/open_fonts_embed.cmake)
    midas_embed_open_fonts(OPEN_FONTS_DATA_SRC)
    set(FAST3D_SRC
        port/fast3d/about.cpp
        port/fast3d/weapon_cache.cpp
        port/fast3d/weapon_worker.cpp
        port/fast3d/third_party/puff/puff.c
        ${OPEN_FONTS_DATA_SRC}
        port/fast3d/gfx_pc.cpp
        port/fast3d/gfx_shiny.cpp
        port/fast3d/gfx_cc.cpp
        port/fast3d/gfx_opengl.cpp
        port/fast3d/gfx_sdl2.cpp
        port/fast3d/glad/glad.c
    )
    # FORK-051, FORK-052: the font sources need no game headers, and the game's
    # include directories shadow system ones (src/sched.h for <sched.h>, which
    # GCC 11's <algorithm> reaches through <pthread.h>), so they build with none.
    add_library(ge_open_fonts OBJECT port/fast3d/open_fonts.cpp port/fast3d/png_decode.cpp
        port/fast3d/enhanced_fonts.cpp port/fast3d/enhanced_font_masks.cpp
        port/fast3d/enhanced_fonts_download.cpp port/fast3d/enhanced_fonts_http.cpp)
    set_target_properties(ge_open_fonts PROPERTIES INCLUDE_DIRECTORIES "")
    list(APPEND FAST3D_SRC $<TARGET_OBJECTS:ge_open_fonts>)
    # FORK-082: HDR Output's DXGI presenter (stubs off Windows). The Windows
    # SDK headers collide with the game's include directories, so like the
    # font sources it builds without them.
    add_library(ge_hdr_dxgi OBJECT port/fast3d/gfx_hdr_dxgi.cpp)
    set_target_properties(ge_hdr_dxgi PROPERTIES INCLUDE_DIRECTORIES "")
    if(WIN32 AND NOT GE_XBOX)
        if(SDL2_MINGW_ROOT)
            target_include_directories(ge_hdr_dxgi PRIVATE ${SDL2_MINGW_ROOT}/include/SDL2)
        else()
            target_include_directories(ge_hdr_dxgi PRIVATE
                ${CMAKE_SOURCE_DIR}/third_party/SDL2-2.30.11/i686-w64-mingw32/include/SDL2)
        endif()
    endif()
    list(APPEND FAST3D_SRC $<TARGET_OBJECTS:ge_hdr_dxgi>)
    if(NOT EMSCRIPTEN AND NOT GE_XBOX)
        # FORK-053: player-installed HD texture packs (native only). Like the
        # font sources, it builds without the game's include directories.
        include(port/fast3d/hd_texture_scope_embed.cmake)
        midas_embed_hd_texture_scope(HD_TEXTURE_SCOPE_SRC)
        add_library(ge_hd_textures OBJECT port/fast3d/hd_textures.cpp
            port/fast3d/hd_texture_pack.cpp port/fast3d/hd_texture_format.cpp port/fast3d/hd_texture_prefetch.cpp
            port/fast3d/hd_textures_download.cpp port/fast3d/hd_download_engine.cpp)
        set_target_properties(ge_hd_textures PROPERTIES
            INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/port/include")
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND GE_POINTER_BITS EQUAL 32)
            # i686 stat otherwise rejects large inode numbers on modern mounts.
            target_compile_definitions(ge_hd_textures PRIVATE _FILE_OFFSET_BITS=64 _TIME_BITS=64)
        endif()
        list(APPEND FAST3D_SRC $<TARGET_OBJECTS:ge_hd_textures> ${HD_TEXTURE_SCOPE_SRC})
    endif()
    if(NOT EMSCRIPTEN AND NOT GE_XBOX)
        add_compile_definitions(GE_SAVE_TRANSFER=1)
        add_library(ge_save_transfer OBJECT port/fast3d/save_backup.cpp
            port/fast3d/save_restore.cpp port/fast3d/save_transfer.cpp)
        if(NOT WIN32)
            # FORK-092: zenity / kdialog runner for the Linux dialogs
            target_sources(ge_save_transfer PRIVATE port/fast3d/posix_dialog.cpp)
        endif()
        set_target_properties(ge_save_transfer PROPERTIES
            INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/port/include")
        if(WIN32)
            if(SDL2_MINGW_ROOT)
                target_include_directories(ge_save_transfer PRIVATE ${SDL2_MINGW_ROOT}/include/SDL2)
            else()
                target_include_directories(ge_save_transfer PRIVATE
                    ${CMAKE_SOURCE_DIR}/third_party/SDL2-2.30.11/i686-w64-mingw32/include/SDL2)
            endif()
        elseif(SDL2_LINUX_ROOT)
            target_include_directories(ge_save_transfer PRIVATE ${SDL2_LINUX_ROOT}/include/SDL2)
        else()
            target_include_directories(ge_save_transfer PRIVATE /usr/include/SDL2)
        endif()
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND GE_POINTER_BITS EQUAL 32)
            target_compile_definitions(ge_save_transfer PRIVATE _FILE_OFFSET_BITS=64 _TIME_BITS=64)
        endif()
        list(APPEND FAST3D_SRC $<TARGET_OBJECTS:ge_save_transfer>)
    endif()
    add_compile_definitions(GE_VIDEO=1)
    # The picture digest (port_gfx_digest.h, LP64-006 Task 7) wraps the
    # renderer only in a comparison build; no shipped build compiles it.
    if(GE_COMPARE)
        list(APPEND FAST3D_SRC port/src/port_gfx_digest.cpp)
    endif()
    # The play log (port_playlog.h, LP64-005 Task 8), lockstep
    # (port_lockstep.h, LP64-006 Task 2) and the simulation trace
    # (port_simtrace.h, Task 3) are empty in a 32-bit build with video, the
    # shipped kind; like port_lp64.c, an empty object would add a compiler
    # ident to the Windows executable's .rdata. A comparison build
    # (GE_COMPARE) keeps them: there PORT_PLAY_HARNESS is 1 at either width.
    if(NOT GE_POINTER_BITS EQUAL 64 AND NOT GE_COMPARE)
        list(REMOVE_ITEM PORT_SRC ${CMAKE_SOURCE_DIR}/port/src/port_playlog.c
            ${CMAKE_SOURCE_DIR}/port/src/port_lockstep.c
            ${CMAKE_SOURCE_DIR}/port/src/port_simtrace.c)
    endif()
else()
    set(FAST3D_SRC)
    # video.c/input_pc.c/raphnet.c are GE_VIDEO-only; exclude them from the port glob,
    # with input_pc.c's XWayland pointer reset.
    list(REMOVE_ITEM PORT_SRC
        ${CMAKE_SOURCE_DIR}/port/src/video.c
        ${CMAKE_SOURCE_DIR}/port/src/input_pc.c
        ${CMAKE_SOURCE_DIR}/port/src/port_xwayland_pointer.c
        ${CMAKE_SOURCE_DIR}/port/src/raphnet.c)
endif()

add_executable(ge ${GAME_SRC} ${ENGINE_SRC} ${GU_SRC} ${PORT_SRC}
               ${ASSET_LOOSE_SRC} ${ASSET_OBJS} ${FAST3D_SRC})
include(scripts/port/sentry.cmake)
# Native player filenames use the fork identity. Keep the `ge` target, logs,
# profile identities and deployed Wasm runtime filenames stable.
if(EMSCRIPTEN)
    set_target_properties(ge PROPERTIES OUTPUT_NAME ProjectMidas)
else()
    set_target_properties(ge PROPERTIES OUTPUT_NAME IronSights)
endif()
if(EMSCRIPTEN)
    midas_web_target(ge)
else()
# The suspend-point serializer records only registered game-owned sections;
# the linker map supplies their exact bounds for this particular executable.
if(APPLE)
    # Apple's linker spells the map option differently. A 64-bit build has no
    # compiled-in assets, so several hundred weak references (the file symbols
    # of assets/obseg/obseg.h, and video-only functions a headless build tests
    # for) are undefined: ELF makes such a reference null at link time, and
    # Mach-O does only with dynamic lookup. That also lets a strong undefined
    # symbol through, so macos_link_check.py refuses one after every link.
    target_link_options(ge PRIVATE "-Wl,-map,${CMAKE_BINARY_DIR}/ProjectMidas.map"
        "-Wl,-undefined,dynamic_lookup")
    find_program(MAC_PYTHON NAMES python3 REQUIRED)
    set_property(TARGET ge APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/scripts/port/macos_link_check.py)
    add_custom_command(TARGET ge POST_BUILD
        COMMAND ${MAC_PYTHON} ${CMAKE_SOURCE_DIR}/scripts/port/macos_link_check.py $<TARGET_FILE:ge>
        VERBATIM)
else()
    target_link_options(ge PRIVATE "-Wl,-Map=${CMAKE_BINARY_DIR}/ProjectMidas.map")
endif()
# 64-bit builds have no serializer (port_state_unavailable.c), so no registry.
if(NOT GE_POINTER_BITS EQUAL 64)
    find_program(STATE_PYTHON NAMES python3 REQUIRED)
    set_property(TARGET ge APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/scripts/port/embed_state_layout.py)
    add_custom_command(TARGET ge POST_BUILD
        COMMAND ${STATE_PYTHON} ${CMAKE_SOURCE_DIR}/scripts/port/embed_state_layout.py
            --exe $<TARGET_FILE:ge> --map ${CMAKE_BINARY_DIR}/ProjectMidas.map
            --root ${CMAKE_SOURCE_DIR} --nm ${CMAKE_NM}
        VERBATIM)
endif()
endif()
# The exe's own icon (Explorer, a pinned taskbar entry): port/win/ProjectMidas.ico,
# generated with the window icon by scripts/port/make_brand_art.py.
if(WIN32)
    enable_language(RC)
    target_sources(ge PRIVATE ${CMAKE_SOURCE_DIR}/port/win/ProjectMidas.rc)
    # No console window: the GUI subsystem, keeping main() as the entry (unlike
    # -mwindows, which wants WinMain). The log goes to ge_direct.log instead
    # (main.c portDirectLaunchLog); redirected runs keep their output.
    target_link_options(ge PRIVATE -Wl,--subsystem,windows)
endif()

# Project Midas build stamp: <git commit count>-<YYYYMMDD>, regenerated on
# every build (scripts/port/buildinfo.cmake has the why). sysstubs.c owns the
# string; the legal screen and the `ge: sys: build` log line both read it.
# build-info.json's "architecture" uses portBuildArchitecture()'s names
# (port/include/port_build_arch.h); a 32-bit native build is always x86 (-m32).
if(EMSCRIPTEN)
    set(GE_BUILD_ARCHITECTURE wasm32)
elseif(GE_POINTER_BITS EQUAL 32)
    set(GE_BUILD_ARCHITECTURE i686)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    set(GE_BUILD_ARCHITECTURE x86_64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(GE_BUILD_ARCHITECTURE aarch64)
else()
    set(GE_BUILD_ARCHITECTURE unknown)
endif()
add_custom_target(ge_buildinfo
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR}
            -DOUT=${CMAKE_BINARY_DIR}/generated/port_buildinfo.h
            -DARCH=${GE_BUILD_ARCHITECTURE}
            -P ${CMAKE_SOURCE_DIR}/scripts/port/buildinfo.cmake
    BYPRODUCTS ${CMAKE_BINARY_DIR}/generated/port_buildinfo.h
    COMMENT "build stamp"
    VERBATIM)
add_dependencies(ge ge_buildinfo)
target_include_directories(ge PRIVATE ${CMAKE_BINARY_DIR}/generated)

# libm: sinf/cosf/sqrtf (math_sincos.s and gu asm on N64; libm on PC).
target_link_libraries(ge m)

if(GE_VIDEO AND NOT EMSCRIPTEN)
    if(WIN32)
        # the SDL2 MinGW development package: third_party/ by default, or
        # SDL2_MINGW_ROOT (the Docker image unpacks it in the home folder)
        set(SDL2_MINGW_ROOT "" CACHE PATH "SDL2 MinGW package's i686-w64-mingw32 folder (default: third_party/SDL2-2.30.11/i686-w64-mingw32)")
        if(SDL2_MINGW_ROOT)
            set(SDL2_ROOT ${SDL2_MINGW_ROOT})
        else()
            set(SDL2_ROOT ${CMAKE_SOURCE_DIR}/third_party/SDL2-2.30.11/i686-w64-mingw32)
        endif()
        target_include_directories(ge PRIVATE ${SDL2_ROOT}/include ${SDL2_ROOT}/include/SDL2)
        target_link_libraries(ge ${SDL2_ROOT}/lib/libSDL2.dll.a opengl32 gdi32 imm32 winmm setupapi hid version ole32 oleaut32
            winhttp)  # winhttp: the Enhanced fonts' download (FORK-052)
        # the game needs SDL2.dll beside the exe
        add_custom_command(TARGET ge POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SDL2_ROOT}/bin/SDL2.dll $<TARGET_FILE_DIR:ge>)
    else()
        # 32-bit SDL2: needs libsdl2-dev:i386 (+ libgl-dev:i386), or set
        # SDL2_LINUX_ROOT to a self-built prefix (e.g. CI's cached, user-writable
        # install, avoiding a system-wide /usr install).
        if(SDL2_LINUX_ROOT AND GE_POINTER_BITS EQUAL 64)
            target_include_directories(ge PRIVATE ${SDL2_LINUX_ROOT}/include/SDL2)
            target_link_directories(ge PRIVATE ${SDL2_LINUX_ROOT}/lib)
            target_link_libraries(ge SDL2 GL dl pthread)
        elseif(SDL2_LINUX_ROOT)
            target_include_directories(ge PRIVATE ${SDL2_LINUX_ROOT}/include/SDL2)
            target_link_libraries(ge ${SDL2_LINUX_ROOT}/lib/i386-linux-gnu/libSDL2.so GL dl pthread)
            set_target_properties(ge PROPERTIES BUILD_RPATH ${SDL2_LINUX_ROOT}/lib/i386-linux-gnu)
        else()
            target_include_directories(ge PRIVATE /usr/include/SDL2)
            target_link_libraries(ge SDL2 GL dl pthread)
        endif()
    endif()
    # the files a release ships beside the game, so a fresh build runs the
    # same: controller mappings and layouts (without them a new install has
    # no default binds), the ini help and the base shader list
    add_custom_command(TARGET ge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/tools/gamecontrollerdb.txt
            ${CMAKE_SOURCE_DIR}/tools/gamecontrollerdefault.txt
            ${CMAKE_SOURCE_DIR}/tools/midas_ini_help.txt
            $<TARGET_FILE_DIR:ge>
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ge>/Data
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/tools/shaders_base.txt $<TARGET_FILE_DIR:ge>/Data/shaders_base.txt)
endif()

if(GE_LP64_ORACLE)
    # The 32-bit build's -w, for every source but the converters (above). Last,
    # so it also reaches the sources added after add_executable (sentry.cmake's
    # crash adapter).
    get_target_property(GE_ORACLE_QUIET_SRC ge SOURCES)
    list(FILTER GE_ORACLE_QUIET_SRC EXCLUDE REGEX "/romconv_[^/]*\\.c$")
    set_property(SOURCE ${GE_ORACLE_QUIET_SRC} APPEND PROPERTY COMPILE_OPTIONS -w)
endif()
