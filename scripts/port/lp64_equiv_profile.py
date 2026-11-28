"""The two settings profiles of lp64_equiv.py (LP64-006 Task 5): the midas.ini each case of a run
with video gets in its folder. Only video builds read it.

`default` is lp64_play_run.WINDOWED_INI, every setting at its code default. `all` also sets every
option key this fork adds (the keys of port/src/port_options.c that settings_upstream_keys.txt,
the inherited baseline, lacks) to its value in ALL_ON, under the section port_options.c gives it.
EXCLUDED names each fork key left out, with its reason; test_lp64_equiv.py fails when a fork key
is in neither.
"""
import re

from lp64_play_run import WINDOWED_INI
from settings_placement import OPTIONS, ROOT

PROFILES = ('default', 'all')

EXCLUDED = {
    'crash_reporting': 'it would call the network',
    'texture_pack': 'no pack is in the containers, and packs are ROM-derived',
    'mode': 'the updater (FORK-095) is never in a comparison build, and it would call the network',
}

# Toggles on (1), sliders away from their defaults, enums at their strongest choice, except two:
# graphics_look stays Custom (0), so the tone sliders below take effect rather than a Look's, and
# aim_mode stays ADS (1), the fork's aiming.
ALL_ON = {
    'ads_crosshair': 1, 'ads_recoil': 50, 'ads_spread': 50, 'aim_mode': 1, 'allow_noclip': 1,
    'always_crosshair_size': 150, 'always_health': 1, 'arc_crosshair': 1, 'baked_lighting': 1,
    'baked_lighting_strength': 150, 'close_hd_matches': 1, 'damage_indicators': 1, 'debrief_hit_map': 1,
    'decorative_pellets': 1, 'explosive_rounds': 2, 'explosive_rounds_guards': 1, 'fog': 1,
    # The Iron Sights style (2), not Enhanced (1): the Enhanced pack is downloaded only after the
    # player consents (AGENTS.md, the font styles plan). While the Iron Sights font is hidden,
    # port/src/video.c loads 2 as Original unless GE_IRONSIGHTS_FONTS=1, so the profile sets that
    # too (PROFILE_ENV).
    'font_style': 2,
    'glass_reflections': 1, 'graphics_look': 0, 'graphics_viewmodel': 1, 'gun_glint': 1,
    'hdr_output': 1, 'hdr_paper_white': 200, 'hdr_peak': 1000, 'headshots_only': 1,
    'headshots_only_melee': 1, 'hip_arms': 1, 'hit_markers': 1, 'hit_sounds': 1, 'knife_ads': 1,
    'lean_tilt': 12, 'leaning': 1, 'ludicrous_sprint': 1, 'mission_hints': 1, 'noclip_view': 10,
    'objective_notifications': 1, 'projectile_arc': 1, 'reinforcements': 2, 'reload_speed': 2,
    'shiny_weapons': 1, 'shotgun_tracers': 1, 'show_debug_pos': 1, 'sprint': 1, 'sprint_fov': 3,
    'sprint_indicator': 1, 'text_antialiasing': 1, 'tone_adapt_speed': 200, 'tone_auto_exposure': 50,
    'tone_brightness': 120, 'tone_clarity': 50, 'tone_contrast': 150, 'tone_fade': 50, 'tone_glow': 50,
    'tone_grain': 50, 'tone_intensity': 100, 'tone_mapping': 1, 'tone_saturation': 150,
    'tone_sharpen': 50, 'tone_split': 50, 'tone_split_colours': 1, 'tone_temperature': 50,
    'tone_vignette': 50, 'touch_controls_opacity': 75, 'touch_double_tap_fire': 1,
    'tracer_strength': 400, 'vibrance': 50, 'weapon_hud': 1, 'weapon_hud_icons': 1, 'weapon_kick': 3,
}

# The variables a profile's runs get besides its midas.ini; lp64_equiv.py's --env refuses them.
PROFILE_ENV = {'default': {}, 'all': {'GE_IRONSIGHTS_FONTS': '1'}}

# An option row's key and section: `TOGGLE("fog", "[display]", ...`.
OPTION_SECTION = re.compile(r'^\s*(?:TOGGLET?|ENUM[RP]|INTR|TENTHS|PCT)\s*\(\s*"([a-z0-9_]+)",\s*"\[([a-z0-9_]+)\]"',
                            re.M)


def option_sections(source: str) -> dict[str, str]:
    """Each option key of port_options.c's SOURCE and the ini section it is read from."""
    return dict(OPTION_SECTION.findall(source))


def profile_env(name: str) -> dict[str, str]:
    """The variables profile NAME's runs get (GE_IRONSIGHTS_FONTS=1 for `all`, see font_style)."""
    return dict(PROFILE_ENV[name])


def profile_ini(name: str, source: str | None = None) -> str:
    """The midas.ini of profile NAME; SOURCE is port_options.c's text (read from the checkout when
    None). Raises ValueError for an unknown profile or an ALL_ON key the source does not define."""
    if name not in PROFILES:
        raise ValueError(f'unknown profile {name!r}; the profiles are {", ".join(PROFILES)}')
    if name == 'default':
        return WINDOWED_INI
    sections = option_sections((ROOT/OPTIONS).read_text() if source is None else source)
    unknown = sorted(key for key in ALL_ON if key not in sections)
    if unknown:
        raise ValueError(f'ALL_ON names keys {OPTIONS} does not define: {", ".join(unknown)}')
    by_section: dict[str, list[str]] = {'display': WINDOWED_INI.splitlines()[1:]}
    for key in sorted(ALL_ON):
        by_section.setdefault(sections[key], []).append(f'{key}={ALL_ON[key]}')
    return ''.join(f'[{section}]\n' + ''.join(f'{line}\n' for line in lines) for section, lines in by_section.items())
