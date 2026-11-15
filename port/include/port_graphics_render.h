/* Graphics (FORK-082): what the renderer reads each frame. Free of the N64
 * headers so the C++ renderer can include it; the rules are in port_graphics.h. */
#ifndef PORT_GRAPHICS_RENDER_H
#define PORT_GRAPHICS_RENDER_H

#include <stdint.h>

/* HDR Output's state as the renderer last saw it (hdr_state) */
enum {
    PORT_HDR_OFF,
    PORT_HDR_ACTIVE,
    PORT_HDR_UNSUPPORTED, /* no presenter on this platform, or it failed */
    PORT_HDR_SDR_DISPLAY  /* Windows has HDR off for the window's monitor */
};

/* written by the sim thread (settings, hotkeys), read by the render thread;
 * plain ints, last value wins */
struct PortGraphicsRender {
    int32_t tone_mapping;
    int32_t vibrance;   /* percent */
    int32_t hdr_output;
    int32_t hdr_peak;   /* nits */
    int32_t hdr_paper;  /* nits */
    int32_t compare;    /* split the view: left stock, right graded */
    int32_t hdr_state;  /* written by the renderer */
    int32_t hdr_display_peak; /* nits, written by the renderer (0 = unknown) */
    int32_t brightness; /* percent: exposure, 100 = stock */
    int32_t contrast;   /* percent: around mid grey, 100 = none */
    int32_t intensity;  /* percent: how much of the filmic curve */
    int32_t clarity;    /* percent: local contrast */
    int32_t sharpen;    /* percent: adaptive sharpening */
    int32_t glow;       /* percent: bloom from bright areas */
    int32_t saturation; /* percent: 0 = black and white, 100 = stock */
    int32_t temperature; /* -100 cool .. 100 warm */
    int32_t split;      /* percent: split-tone strength */
    int32_t split_colours; /* 0 teal / orange, 1 green / gold, 2 blue / amber, 3 sepia */
    int32_t fade;       /* percent: lifted blacks */
    int32_t vignette;   /* percent: darkened corners */
    int32_t grain;      /* percent: film grain */
    int32_t auto_exposure; /* percent: eye adaptation strength */
    int32_t adapt_speed;   /* percent: eye adaptation speed, 100 = normal */
};

#ifdef __cplusplus
extern "C" {
#endif
extern struct PortGraphicsRender g_PortGraphicsRender;
#ifdef __cplusplus
}
#endif

#endif
