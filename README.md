# Iron Sights

<!-- bright-builds-rules-readme-badges:begin -->

<!-- Managed upstream by bright-builds-rules. If this badge block needs a fix, open an upstream PR or issue instead of editing the downstream managed block. Keep repo-local README content outside this managed badge block. -->

[![GitHub Stars](https://img.shields.io/github/stars/pRizz/Iron-Sights)](https://github.com/pRizz/Iron-Sights)
[![License](https://img.shields.io/github/license/pRizz/Iron-Sights?style=flat-square)](./LICENSE)
[![Bright Builds: Rules](https://raw.githubusercontent.com/bright-builds-llc/bright-builds-rules/main/public/badges/bright-builds-rules-flat.svg)](https://github.com/bright-builds-llc/bright-builds-rules)
[![OpenLinks profile](https://img.shields.io/badge/OpenLinks-profile-0F172A)](https://openlinks.us/)

<!-- bright-builds-rules-readme-badges:end -->

Iron Sights is an independent fork of [Project Midas](https://github.com/murkantor/Project-Midas), adding modern aiming, movement, and HUD options to the GE PC port. This fork is maintained separately from upstream.

[![Watch on YouTube: Iron Sights, aim down sights and more for the GoldenEye 007 PC port](https://img.youtube.com/vi/lq_tJ_B7bDY/maxresdefault.jpg)](https://youtu.be/lq_tJ_B7bDY)

**[▶ Watch on YouTube: Aim Down Sights and More for the GoldenEye 007 PC Port (3 min)](https://youtu.be/lq_tJ_B7bDY)**

Earlier video: [ADS Update — Hands, Better Sights & Sniper Scope Zoom (60 FPS)](https://youtu.be/dI-G7Lo12hA)

**[See the full list of changes from upstream →](FORK_CHANGES.md)** — the maintained inventory of this fork's additions, modifications, settings, and fixes.

This fork includes upstream **1.0.2b** at `9062f9f120b7`, including its audio,
high-tick aiming, collision and controller fixes. See the inherited
[changelog](CHANGELOG.md). Optional Weapon FOV applies in hip fire and blends
back to the calibrated scene projection for ADS; HUD Scale also scales our
vector sights. Existing ADS, HUD and browser controls retain their defaults.

## Download and play

**[DOWNLOAD HERE — latest Windows ZIP](https://github.com/pRizz/Iron-Sights/releases/latest/download/Iron-Sights-windows-x86.zip)** · [Release details and provenance](https://github.com/pRizz/Iron-Sights/releases/latest) · [Build status](https://github.com/pRizz/Iron-Sights/actions/workflows/windows-release.yml)

1. Download **Iron-Sights-windows-x86.zip** and **extract the entire ZIP** into a writable folder, such as Documents. Keep `SDL2.dll` beside the executable. GitHub's “Source code” archives are for building, not playing.
2. Put your own **unmodified GoldenEye 007 USA ROM** (`.z64`, `.v64`, or `.n64`) in the extracted **Data** folder. Extract zipped ROMs first. No ROM is included or downloaded by the game.
3. Double-click **IronSights.exe**. On first launch, the game prepares weapon icons from your ROM in the background (a few seconds) and caches them locally. No Python, compiler, administrator access or asset downloads are required; a mission started before they are ready shows weapon names.

Windows 10/11, an x86-compatible CPU and an OpenGL 3.3-capable graphics driver are required. The 32-bit build also runs on 64-bit Windows. These are continuously published development builds from successful pushes to `main`.

**Linux i686:** [portable tar.gz](https://github.com/pRizz/Iron-Sights/releases/latest/download/Iron-Sights-linux-i686.tar.gz) · [AppImage](https://github.com/pRizz/Iron-Sights/releases/latest/download/Iron-Sights-linux-i686.AppImage). Both require glibc 2.35+, a 32-bit loader/OpenGL driver stack and the system's 32-bit C++ runtime (`libstdc++6:i386` and `libgcc-s1:i386` on Ubuntu or Debian); this change from bundled copies has not yet been confirmed on a system with Mesa 25. Extract the tarball and run `./IronSights`, with your ROM in its `Data` folder. The AppImage uses `${XDG_DATA_HOME:-$HOME/.local/share}/iron-sights` for writable data and the ROM; mark it executable before launching. See the included `START-HERE.txt`. The AppImage packages the same 32-bit game. The i686 packages keep suspend points.

**Linux x86_64 (64-bit):** [portable tar.gz](https://github.com/pRizz/Iron-Sights/releases/latest/download/Iron-Sights-linux-x86_64.tar.gz) · [AppImage](https://github.com/pRizz/Iron-Sights/releases/latest/download/Iron-Sights-linux-x86_64.AppImage). A native 64-bit build: it requires an x86_64 Linux with glibc 2.35+ and OpenGL 3.3 drivers, takes the C++ runtime from your distribution, and needs no 32-bit libraries. It runs the same way and shares the AppImage data folder with the i686 AppImage. Suspend points are unavailable in 64-bit builds; campaign saves work. It has run on GitHub's x86_64 runner and in one WSL session on Windows 11 (an RTX 3070, through Mesa's d3d12 driver), not yet on a native Linux desktop; see its `START-HERE.txt`. Known issue: under WSL with NVIDIA's driver, quitting the game ends in a crash inside the driver (exit status 139). WSL is not a way to play this build: mouse-look cannot be played under WSLg, and there the game reaches the graphics card only with `GALLIUM_DRIVER=d3d12` (and, beside an Intel chip, `MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA`); without them it draws with Mesa's software renderer.

**Steam Deck / SteamOS:** [Steam Deck ZIP](https://github.com/pRizz/Iron-Sights/releases/latest/download/Iron-Sights-steamdeck-i686.zip) packages the same i686 game with a `run.sh` launcher. Extract it to a writable folder, put your ROM in `Data`, then run `sh run.sh` in Desktop Mode. The ZIP includes Gaming Mode setup steps and a `set-steam-art.py` helper that gives the Steam shortcut its icon, cover and library artwork. It still needs the system's 32-bit loader, glibc 2.35+, 32-bit C++ runtime and 32-bit OpenGL drivers; a Steam Deck device test remains pending. A 64-bit [Steam Deck ZIP](https://github.com/pRizz/Iron-Sights/releases/latest/download/Iron-Sights-steamdeck-x86_64.zip) packages the x86_64 game the same way, without 32-bit libraries and without suspend points; it has not been run on a Steam Deck either.

For the embedded build time, commit and clickable source/build/release links, open **Options > Gameplay > Iron Sights > System > About / Build Info**; the watch has the same entry. Windows links are blue and underlined and open in your default browser. Local or modified builds are labeled accordingly. The release page supplies signed provenance, hashes and build-input details; see [verification and maintainer instructions](scripts/port/releases.md).

To update, replace the supplied program files while keeping **Data** and **midas.ini**. Normal campaign saves and profiles are separate from executable-specific suspend points.

## Play the browser preview

**[PLAY IN YOUR BROWSER](https://prizz.github.io/Iron-Sights/)** with your own stock US ROM. The desktop preview runs locally through WebAssembly and remembers your ROM, campaign saves, profiles and settings on this device. GoldenEye's four-save Files screen has **Import Save** and **Export Save** beside Copy, Erase, Options and Exit Game. Backups include all campaigns, profiles and settings; importing confirms replacement and restarts safely. The ROM stays separate. Chrome, Edge and Firefox have been tested; Safari is experimental.

On phones and tablets, touch controls turn on automatically. Startup, mission
selection and briefings use the cursor: drag to move it and tap to select.
Tap a slider such as Music Volume to edit it, then drag sideways or use − / +;
Done keeps the value and Cancel restores it. Long pick lists scroll with ▲ / ▼,
a rebind row offers Unbind and Cancel while it waits for a key, and the Files
screen's Erase prompt answers by tapping cancel or confirm. Drag along a 007-mode
bar to set it. Multiplayer screens built for controllers, such as Player Options,
show an on-screen D-pad with A, B and Start. When naming a layout, weapon set
or saved settings, tap **Phone keyboard** to type with your phone's keyboard; the
name shows in a bar at the top while you type.
Controller buttons stay hidden through cutscenes, then fade in when the game
hands control to you. Move with the left
thumbstick and drag any free screen area to look. **Touch layout** defaults to **N64
controller**, with blue A, green B and red Start in a vertical column (Start on
top, A on the bottom), beside gray Z on the right.
Hold/drag Z to shoot and turn; the gray rectangular **R** toggles ADS, and the weapon icon switches
weapons. You can also tap a free screen area, then touch it again and hold to fire while
that finger keeps aiming (**Double-Tap Fire** under Gameplay > Iron Sights > Aiming and Movement,
on by default; a single tap never fires). The controls rest at half strength while you play and
brighten to 80% under your finger (**Touch Controls Opacity** under Gameplay > Iron Sights > HUD
Additions: 25-100%). Start opens the watch; A/B navigate its menus. Choose **Modern actions** in
settings for the previous Fire/Use/Reload controls and More tray. Touch controls
sit farther inside the safe screen edges. The mobile overlay omits browser
Pause/Options buttons and leaves gaps between controls available for gestures.
Hints near the top disappear after five seconds in missions or three in menus.
Use Start or Watch for the native pause menu. The
retained launcher has an
**On-screen touch controls** switch and saves the chosen layout. Mobile viewport tests are automated;
physical iPhone/iPad/Android performance and compatibility remain experimental.

Successful `main` builds deploy automatically to GitHub Pages. See [compatibility and measured limits](scripts/port/web_compatibility.md), [browser build instructions](web/README.md) and [milestone evidence](scripts/port/web_target_progress.md). Browser suspend points and offline application support are deferred.

## What's different in this fork?

- **Aim down sights (ADS):** right-click ADS by default, individual sight alignment for 20 supported firearms, a sight picture for the taser (prongs either side of the aim point) and the watch laser (watch face just below it), and half-speed movement while aiming. Added hands and forearms support guns that originally lacked them. Rifle ADS arms, native pistol forearms and the karate-chop arm match the mission's watch-arm wardrobe with original fabric/skin textures and rolled or long sleeves. Unarmed HUD icons match that outfit too. The original red reticle mode remains selectable.
- **Sniper scope and wheel zoom:** aim through a centered scope, turn the whole view with the mouse, and adjust magnification with the mouse wheel.
- **Shift to sprint:** hold Left Shift to sprint; a gameplay setting can restore the original slow-walk behavior.
- **Sharper sights and combat feedback:** smooth reticles, optional hit markers, and directional damage indicators.
- **Objective notifications:** named alerts show when mission objectives are completed or failed.
- **Weapon inventory HUD:** a compact strip highlights your selected weapon, supports text or rendered model icons, and fades while idle.
- **Always-visible health and armor:** persistent gauges become thinner and dimmer between hits; the original temporary display is also available.
- **Fonts (Original by default):** **Gameplay > Iron Sights > System > Fonts** chooses **Original** or **Enhanced**. Our own MIT-licensed fonts (a squared title font and a grotesque body font, drawn as sharp distance fields) are in the code but hidden for now, because they still have deficiencies; see the [open assets plan](scripts/port/open_assets_plan.md). **Enhanced** draws both fonts from Carnivorous's CC0 GEPD redraw, which Iron Sights does not bundle: **Gameplay > Iron Sights > System > Enhanced Fonts** (also **Get Enhanced Fonts** in the Options folder until they are installed) downloads its 186 font images (250 KB) from GitHub when you choose Download there, in the browser version too, or you can copy them into `Data/FontPacks/gepd/` yourself (for example with `python scripts/port/enhanced_fonts_pack.py install --dest Data/FontPacks/gepd`). **Text Antialiasing** (on by default, beside Fonts) keeps small Enhanced text smooth with any Texture Filter. See the [font styles plan](scripts/port/font_styles_plan.md).
- **HD textures (Windows and Linux):** Iron Sights includes no texture pack. **Get HD Textures** in the Options folder (also **Gameplay > Iron Sights > System > HD Textures**) downloads intermissionfb's [GoldenEye 007 HD](https://github.com/GhostlyDark/GoldenEye-007-HD) pack, 150 MB, from its makers' GitHub when you choose Download; or put its PNG version's ZIP (`...-gliden64-png-hd.zip`), or the extracted folder, in `Data/TexturePacks` yourself. **Texture Pack** set to **HD** (the default) draws its campaign environment textures, the gunbarrel intro, sky clouds, explosions, smoke and bullet-impact effects, and the HUD's ammo icons. Any other GLideN64 PNG pack in that folder is a custom pack, used for every texture it names and ahead of the download: for example the artist repository's own ZIP (**Code > Download ZIP**, with more textures than the release) or a pack you are making, whose saved PNGs show up within seconds. **Gameplay > Iron Sights > System > Custom Texture Packs** explains this in the game, shows how many custom textures were found, and opens the folder. A custom pack's images load in the background and are reduced to HD size (at most 8x the original), so very large art appears a moment after a level starts. **Close HD Matches** (on by default, beside Texture Pack) also gives HD art to 17 textures whose colours only roughly match the pack's; turn it off to keep those originals. The first-person weapon and hands always keep their original look.
- **Fog (on by default):** **Gameplay > Iron Sights > System > Fog** turns the levels' distance fog off. How far you can see and how far guards see you stay the same.
- **Debug Position (off by default):** **Gameplay > Iron Sights > System > Debug Position** shows your room, x/y/z position, heading and pitch across the top of the view, and turns on or off while you play. The `GE_DEBUGPOS` environment variable still overrides it (`0` hides it, anything else shows it).
- **Graphics (on by default on Windows, Linux and desktop browsers, with the Vibrant Look):** **Gameplay > Iron Sights > Graphics** adds **Tone Mapping** (the 3D view regraded for richer colour and contrast without darkening it, with **Brightness**, **Contrast**, **Intensity**, **Vibrance**, **Clarity**, **Sharpen**, **Glow**, **Saturation**, **Temperature**, **Split Tone**, **Fade**, **Vignette**, **Film Grain**, **Auto Exposure** (eye adaptation: bright snow and walls are brought down, dark levels stay dark) and **Adaptation Speed** sliders, and **Looks** that set them all at once: Natural, Vibrant, Cinematic, Noir, Muted, Cold War, Warm Retro and Dark; the HUD keeps its colours), **Baked Lighting** (ray-traced shadowing in corners and under objects, with a touch of colour from nearby surfaces, baked into each level as it loads), **HDR Output** (Windows, while Windows HDR is on: graded highlights brighter than white, with **HDR Peak Brightness** and **HDR Paper White**) and **Gun and Hands** (whether these also reach your gun, hands and arms). Tone Mapping and Baked Lighting start on, HDR Output off; turn Tone Mapping off for the original look. In the browser they start off on phones and tablets, where they have not yet been measured. See [Graphics hotkeys](#graphics-hotkeys).
- **Suspend points:** four manual slots, quicksave/quickload, and undo-load for solo missions (Windows and the i686 Linux and Steam Deck builds; 64-bit builds have none).
- **Reinforcements (Original by default):** each mission's difficulty page sets how many extra guards come after you: **Original**, **More** or **Relentless**, through clones of guards who hear you and fresh guards sent from out of sight once you are spotted. See [Reinforcements](#reinforcements).
- **Headshots Only (off by default):** a second row on each mission's difficulty page makes only your head hits hurt guards. Runs completed this way get their own best-times column. See [Headshots Only](#headshots-only).

See [Playing](#playing) below for controls and settings.

The underlying port is a work in progress built from the N64 decompilation. It runs on Windows and Linux and reads its game data from your own copy of the US cart.

## AI Disclaimer

Claude was used in the making of this project. If you don't agree with this, please do not download either the executables or the source.

## Playing

1. Put your GE (USA) cart image in the `Data` folder next to the executable. See `Data/README.txt` for the accepted formats and other ways to point the game at it.
2. Run `IronSights.exe` (Windows) or `IronSights` (Linux).

Files next to the executable:

| Path | What it is |
| --- | --- |
| `midas.ini` | Settings. Written on first run; delete it to restore the defaults. See `midas_ini_help.txt`. |
| `Data/` | Your cart image goes here. |
| `Data/Saves/eeprom.bin` | Save file. |
| `Data/SaveStates/` | Solo suspend points, quicksave, and undo-load, separated by campaign folder. |
| `Data/Profiles/` | Player profiles (`.geprofile`) and lifetime multiplayer stats (`.gestats`). |
| `Data/WeaponSets/` | Custom multiplayer weapon sets (`.gewset`). |
| `Data/MatchSettings/` | Saved multiplayer settings (`.gemp`), from Load / Save Settings. |
| `Data/Devices/` | Per-device settings (`.gedevice`). Keyboard and mouse binds are in `kbm.gedevice`. |
| `gamecontrollerdb.txt` | SDL controller mappings. |
| `gamecontrollerdefault.txt` | Built-in controller layouts. |
| `gamecontrolleruser.txt` | Your saved controller layouts. |
| `Data/Logs/ge_direct.log` | The run log on Windows (the game opens no console window). |
| `Data/Logs/crash.log` | Written if the game crashes. |
| `Data/CrashReports` | Windows/Linux crash-report queue under the writable data root. Reporting defaults ON; opt out in Gameplay > Iron Sights > System or `[diagnostics] crash_reporting=0` in `midas.ini`. [Payload and privacy details](scripts/port/crash_reporting.md). |
| `Mods/` | Reserved for modded carts. This build loads the stock cart only. |

An older `ge.ini` is renamed to `midas.ini` automatically on first run.

**Mouse Right** aims down the sights by default. Change **Gameplay > Iron Sights > Aiming and
Movement > Aim Mode** to **Red Reticle** for the original aiming mode. The
existing **Aim Control** setting selects hold or toggle for either mode.
Hold **Mouse Right** for ADS with the default Hold setting. The separate
hold-to-ADS action is unbound by default and remains available for custom
keyboard/controller layouts; the former middle-mouse default is removed from
older saved keyboard bindings once, preserving other custom keys.
You can walk and strafe at half speed while aiming down sights.
Desktop missions briefly show the current gadget/ADS bindings when control
begins. **Gameplay > Iron Sights > HUD Additions > Mission Start Hints** turns them off manually;
after three complete displays they turn off automatically. Re-enabling resets
the count for subsequent missions.
ADS adds basic low-poly hands and forearms to the 13 firearms that lack them,
including the KF7 Soviet. Pistols retain their original hands.
**Gameplay > Iron Sights > Aiming and Movement > Hip-Fire Arms** (off by
default) keeps those hands and forearms on at the hip too, held the way they
are in ADS and moving with the gun. Taking up a second gun of the same kind
keeps the right hand on its gun while the left arm slides down out of view,
and it slides back when you go back to one gun.
Each firearm's ADS pose has a slight upward angle to reveal its front sight
and give a view along the top of the gun.
Sniper ADS raises the rifle into a zoomed scope view with the gun and hands hidden
and a centered red crosshair. Mouse movement turns the view while scoped;
the existing zoom controls still adjust magnification.
While ADS with the sniper rifle, **Mouse Wheel Up/Down** zooms in/out instead
of switching weapons. Outside sniper ADS, the wheel uses its normal bindings.

Aiming reticles, the sniper crosshair, the optional always-on crosshair,
and the menu/watch target cursor use thin, smoothed lines and circles.
Custom crosshair colors still apply. Directional damage indicators use smooth curves.

Hold **Left Shift** while moving to sprint at 1.5 times normal speed. On a
controller, click the **left stick** to sprint until you stop moving, click again,
aim or crouch; the right stick click crouches.
**Gameplay > Iron Sights > Aiming and Movement > Sprint** is on by default; turn it off to
restore Shift-to-walk at half speed. The **Walk / Sprint** and **Sprint (Toggle)**
controls can be rebound.
Aiming and crouching prevent the sprint boost; ADS keeps its half-speed movement.
While you sprint, the view widens slightly
(**Aiming and Movement > Sprint FOV**: Off, Normal, Wider or Crazy Wide; Normal
by default). **HUD Additions > Sprint Indicator**, off by default, also fades in
small chevrons at the bottom of the screen.

ADS steadies your aim. **Gameplay > Iron Sights > Aiming and Movement > ADS Spread**
sets how much of a weapon's bullet spread remains while aiming down sights, and
**ADS Recoil** sets how much of its visual kick remains. Both default to 50%;
100% restores the original. Shotguns keep their full pellet spread and kick.

**Weapon Kick**, on the same page, makes every round kick the gun back and up,
including each shot of held automatic fire, which also climbs a little until
you let go. ADS Recoil scales it while aiming down sights. It is visual only:
where your shots go does not change. **Ludicrous** triples the muzzle flip,
sideways kick and climb, and **Cartoon** multiplies them by 5.5, both for fun
rather than realism. **Original** restores the native kick, which plays once
after the trigger is released.

**Reload Speed**, also on that page, speeds up your reloads: **2x**, **3x**,
**5x** or **10x** play the same reload that many times faster, and **Instant**
refills the magazine with no reload at all, so a gun keeps firing until your
spare ammo runs out. **Original** is the default. Guards, weapon switches and
demo playback keep the original speed.

**Leaning**, on by default, lets you lean with **Z** (left) and **C** (right),
rebindable as **Lean Left** and **Lean Right** in Controls. It is the game's
own lean, the sidestep you could already do by strafing while aiming with the
red reticle, now available at the hip and in ADS too. **Lean Tilt** (Off by
default, Subtle or Strong) also rolls the view as you lean, which the stock
lean does not; where you aim stays the same.

**Allow Noclip**, also there and off by default, lets **N** (the rebindable
**Noclip** control in Controls) toggle Quake-style noclip: you fly where you
look, through walls, floors and doors, with no gravity, at three times walking
speed, or twelve times while holding **Left Shift** (Walk / Sprint). Hold
**Space** to rise and **Left Ctrl** (Crouch Hold) to fall; both are rebindable
as **Noclip Up** and **Noclip Down**. A **NOCLIP** label shows while it is on.
While flying, every room of the level draws, so the map no longer vanishes
when you rise above or sink below it, and **Noclip View Distance** (Normal,
Far by default, or Maximum) pushes the fog and far clip out with you. Drawing
everything costs frame time on big levels; choose Normal if flying stutters. Press it again to
drop onto the floor below you; with no floor there, you go back to where you
last flew over one. It is off in demo playback, in the tank and after death,
and each mission starts on foot. Browser touch has no Noclip button yet.

Iron Sights adds these HUD and feedback options under **Gameplay > Iron Sights >
Combat Feedback** and **HUD Additions**. Mission Timer is the original setting on
**Gameplay > HUD**, extended by Iron Sights:

- **Hit Markers:** thin, smoothed white lines confirm damage to a character; headshots use gold.
- **Hit Sounds:** in solo play, a confirmed hit keeps its close-range flesh impact sound at any distance.
- **Damage Direction:** short red arcs point toward incoming damage and follow your view as you turn.
- **Debrief Hit Map:** the solo mission debrief shows front and back body figures, drawn as ink-outlined ovals shaded red by how often each part was hit, with a dot where each of your bullets hit; headshots are gold and kill shots ringed in red. The hit counts stack under the kill total to make room; select them or the figures for a closer look, whose PARTS tab lists hits, kill shots and shares of hits and damage by body part, plus gun and hat hits.
- **Tracer Strength:** bullet tracers, yours and enemies', are drawn wider and longer with a soft glow, and keep a minimum on-screen width so long shots stay visible. 100% is the original look; the default is 200%, up to 400%. Drawing only: hits are unchanged. The Moonraker laser and watch laser keep their own beams.
- **Shotgun Tracers:** on by default. Shotgun and Automatic Shotgun blasts draw a thinner tracer for each of the five pellets, which other players also see in multiplayer; a guard's shotgun fires a single shot and draws one, lighter than your own pellets. Styled by Tracer Strength. Drawing only: hits are unchanged.
- **Decorative Pellets:** on by default, and only in effect with Shotgun Tracers on. Enemies really fire a shotgun as a single shot; this adds four fake pellet tracers in a small cone around it so it looks like a blast, drawn at half opacity so the real shot stands out. Only the real shot can hit.
- **Explosive Rounds:** off by default. Every bullet makes a small blast where it lands: on a wall or floor, on the guard, object or door that stops it, and once per shotgun blast. **Visual** adds a quick flash, a brief pale puff, shrapnel and a small explosion sound without changing any damage; each blast is gone in under a second. **Explosive** also does splash damage to characters (you included, so mind point-blank walls) and to objects, so rounds can set off barrels and other explosive props. **Whose Rounds** chooses **Everyone** (the default: guards' bullets explode too, apart from shots that hit you) or **Players**. Rounds never shake the screen, always leave two explosion slots free for grenades and mines, and are off in demo playback.
- **Objective Alerts:** queued notifications show the objective name and its updated status. Turn this off for the original messages.
- **Mission Timer:** enable this to show elapsed mission time with hundredths at the top left, clear of gadget names and ammo. The clock stays visible during dialogue, with subtitles below its full timer/BEST block and objective alerts below the subtitles. On a mission/difficulty with a timed cheat unlock, its target appears beside your time (`00:42:50 / CHEAT 02:40`). The target is green while within time, amber with ten seconds remaining, and red once exceeded. Uses the same mission clock and whole-second deadline as the unlock check; pauses and suspend restores follow that clock. A saved best time for this mission and difficulty appears below as `BEST 02:03`; it is omitted if no record exists or in 007 mode. The campaign format stores whole seconds and caps records at 17:03, shown as `17:03+`. Difficulties without a cheat target still show elapsed time and any best record. Defaults to Off (`[display] mission_timer=1` enables it). Objectives and normal cheat-unlock eligibility still apply.
- **Weapon Strip:** a compact list near the bottom, above the ammo counters, highlights your equipped weapon. Cards, icons and text are scaled down by 30%. Larger inventories scroll with selection; `+N` shows how many weapons are outside the visible strip. After three seconds without switching weapons, the strip smoothly fades to 20% opacity; switching weapons brings it back. Each two-gun pair is its own card, in the order the weapon cycle visits it: doubles show **x2**, and a mismatched pair (such as an AR33 with an RC-P90) shows both guns, left hand first. While you own the sniper rifle, unarmed attacks club with its butt, so that card shows the rifle butt and reads **Buttstroke**.

**Weapon Strip Style** switches between **Icons** (the default) and **Text**. Icon mode shows full-colour, static renders of the actual weapon models, including silenced guns, rifles, launchers, and explosives; the selected weapon stays highlighted. Its name appears underneath the selected icon, then fades out completely after three seconds without switching weapons (over a one-second transition). Set `[display] weapon_hud_icons=1` in `midas.ini` to select icons directly. Windows and Linux prepare these images from your ROM in the background on first launch, while the game starts; the browser version prepares them during its first start (a **Preparing weapon icons** step on the loading screen). All reuse a per-user cache; no rebuild or external tools are needed. Icons appear from the first mission that starts after they are ready; until then, or if preparation fails, the strip shows weapon names. See [the icon generation guide](scripts/port/weapon_icons.md) for cache details and optional developer exports.

**Health Display** defaults to **Always** on all platforms, keeping slimmer health and armor arcs visible during gameplay. Damage or a health/armor change restores full opacity at 75% of the original thickness; after three quiet seconds, they smoothly fade to 25% opacity and 20% thickness. Select **Automatic** for the original temporary gauges, or set `[display] always_health=0` in `midas.ini`. Existing saved preferences are preserved.

These displays hide during menus, cutscenes, and death. Each setting can be changed independently.

### Reinforcements

In the original game, a few guards (in Archives, Runway, Surface and Bunker)
call one copy of themselves when they hear you before you have seen them: the
copy runs at you while the original holds its post. **Reinforcements** sets how
many guards the level sends after you. It sits on each mission's difficulty page,
below the best times (point at it and confirm with A, Enter, a click or a tap to
step to the next level; a level above Original shows in red), and under
**Gameplay > Iron Sights > Enemies**:

- **Original** (the default): the original game's rule, unchanged.
- **More:** any placed soldier can call clones, not just those few guards, with
  up to two of its clones alive at once and a new one at most every eight seconds
  while it still hears gunfire. Named characters, civilians, scientists,
  invincible characters and clones never call one. The level gets room for 12
  more guards.
- **Relentless:** up to three clones per guard, one every four seconds, room for
  24 more guards, and a guard you have already seen can call one more while it is
  off screen.

Clones need a guard to hear you before you see it, which open levels such as
Dam rarely allow, so More and Relentless also send reinforcements of their own.
Once a guard has spotted you or the alarm is on, a guard arrives every 20 seconds
(More, up to 4 of them alive) or every 10 seconds (Relentless, up to 8), for as
long as you stay noticed and 20 seconds after. Each starts somewhere you cannot
see, a short walk away along the level's own paths, dressed and armed like the
nearest soldier the level placed, and comes straight for you.

None of these extra guards appear inside another character: each keeps a small
gap around it. They only appear while the level still has the original's ten
spare guard slots free, so the level's own scripted reinforcements always have
room. A change applies from the next mission start. Demo playback and
multiplayer keep Original.

### Headshots Only

**Headshots Only** turns a solo mission into a test of aim: only your shots to
a guard's head do any damage. It sits on each mission's difficulty page, on the
Reinforcements line at the left (point at it and confirm with A, Enter, a click
or a tap to switch it; **ON** shows in red), and under **Gameplay > Iron Sights >
Enemies**.

- A bullet or thrown knife that hits anywhere else makes the guard flinch and
  notice you, and the hit sound still plays, but it does no damage and shows no
  hit marker. It still counts in your accuracy and on the debrief hit map.
- Shooting off a hat still exposes the head; a steel helmet still stops the shot.
- **Headshots Melee** (**Allowed** by default, under **Enemies**) decides whether
  punches and knife slashes still hurt, since they never count as head hits.
  Leave it on for missions that start you unarmed, such as Bunker's cell or
  Archives' interrogation room; set it to **Blocked** for a stricter run.
- Explosions are not affected (Explosive Rounds' splash included), nor are
  guards' own shots or multiplayer.

The setting is read when a mission starts, so a change made from the pause watch
applies to the next one. Completing a mission with it on (Agent, Secret Agent or
00 Agent, with no cheats on, as for the normal records) records your time in
the Headshots Only column. The page always shows two best-time
columns: your normal records under a red checkmark, then your Headshots Only
records under a red crosshair-and-head mark. A time slower than the save's
17:03 cap reads `17:03+`. The debrief shows **HEADSHOTS ONLY** after
**STATISTICS** for such a run. These records are kept per campaign folder in
`Data/Saves/headshots.ini` (in the browser, with the other saves and in save
backups); erasing or copying a folder erases or copies them too.

### Mission cheat hints

The mission grid shows the highlighted mission's cheat status, required difficulty,
and target time. On the difficulty screen, a **Cheat target** badge marks the row
that can unlock it. **Cheat unlocked** means the selected campaign save has
earned it; **Cheat to unlock** means it is still to be earned. **Cheat available
- Unlock All** distinguishes temporary availability through that setting.
Both are printed on the dossier in the game's own text, with unlocked and
available status in its dark red. On the difficulty screen the footer appears
only when the required difficulty is still locked, so its row and badge are
hidden. The difficulty rows show best times rather than completion checkmarks; 007 has no timed
cheat target. These hints are always shown and do not require the HUD timer.

### Graphics hotkeys

Each Graphics setting has a key that flips it during play and saves it like
the menu row, with a console message: **F3** Tone Mapping, **F4** Baked
Lighting, **F2** HDR Output, **F7** Gun and Hands. **F11** (**F8** in the
browser, which keeps F11 for fullscreen) splits the view (left stock, right
tone mapped) for comparing; it is not saved. Every slider
on the page can also be changed in play: **Home** / **End** pick the slider
(each Tone Mapping and grading slider, Lighting Strength, and on Windows HDR
Peak Brightness and HDR Paper White) and
**Page Up** / **Page Down** step it, with a message naming the slider and its
value. **Insert** cycles the Looks (and turns Tone Mapping on). All ten keys
can be rebound, and bound to a controller, in Controls. Baked Lighting takes about
half a second per level on a modern CPU (4-7 seconds in the browser, which
bakes in slices between frames); until it finishes the level shows its own
lighting.

### Suspend points

Press **F6** or open **Watch > Gameplay > Iron Sights > Suspend Points** for four manual slots,
a separate quicksave, and undo-load. Use arrows to choose a slot and Save/Load,
then Enter to accept. Saving over an occupied manual slot asks for a second
confirmation. Escape closes the menu. A controller can use its Move stick or
D-pad, Use/Menu Accept, and Menu Back; the actions can be rebound in Controls.

- **F5:** quicksave, replacing the previous quicksave.
- **F9:** quickload.
- **F8:** undo the last load and return to the mission state immediately before it.

Loads return directly to gameplay. A small toast identifies the loaded slot
and mission; saves and quicksaves show matching confirmations. Notifications
last three seconds, fading over the last half-second. You can load after dying
or from the title screen using the same campaign folder. Saving is
limited to solo gameplay while Bond is alive, or the fully opened watch menu.
Campaign progress and settings are not rolled back.

**64-bit builds have no suspend points.** The Linux x86_64 and 64-bit Steam
Deck packages show `Suspend Points: unavailable` on the watch row, and the
keys above say that suspend points are unavailable and that campaign saves
work as usual. Use a Windows or i686 Linux build for suspend points.

**First-version compatibility:** keep the same executable and ROM. A rebuild or
different executable makes existing suspend points incompatible; the game
refuses those files. These are local game-state images, separate from ordinary
campaign saves. See [save-state details and validation](scripts/port/save_states.md).

## Building

The 64-bit Linux build, its images and its package checks are described in the [64-bit Linux guide](scripts/port/linux_64bit.md).

The build runs in Docker, which has every tool and library both builds need. You also need the GE (USA) cart as `baserom.u.z64` in the repository root:

```
sha1: abe01e4aeb033b6c0836819f549c791b26cfde83
```

### On Windows: use WSL

The commands below are for a Linux shell. On Windows, run them in WSL:

1. Install WSL with Ubuntu (`wsl --install` in PowerShell), and turn on WSL integration in Docker Desktop (Settings > Resources > WSL integration).
2. Open the Ubuntu terminal and clone the repository inside WSL, not under `/mnt/c` (much faster):
   ```
   cd ~
   git clone https://github.com/pRizz/Iron-Sights.git
   cd Iron-Sights
   ```
3. Copy your cart in, for example from your Windows Downloads folder:
   ```
   cp /mnt/c/Users/<you>/Downloads/baserom.u.z64 .
   ```

### Build steps

1. Build the image (once):
   ```
   docker build --network=host --build-arg HOST_UID=$(id -u) --build-arg HOST_GID=$(id -g) -t goldeneye .
   ```
2. Start a container in the repository:
   ```
   docker run --rm -it -v $(pwd):/home/dev/project goldeneye
   ```
3. Extract the assets (once):
   ```
   ./scripts/extract_baserom.u.sh
   make convert_props convert_chrs convert_guns combine_images
   ```
4. Build for Windows (x86):
   ```
   cmake -B build_win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake -DCMAKE_BUILD_TYPE=Debug -DGE_VIDEO=ON -DSDL2_MINGW_ROOT=$HOME/sdl2-mingw/SDL2-2.30.11/i686-w64-mingw32
   cmake --build build_win -j$(nproc)
   ```
5. Build for Linux:
   ```
   cmake -B build_pc -DCMAKE_BUILD_TYPE=Debug -DGE_VIDEO=ON -DSDL2_LINUX_ROOT=$HOME/sdl2-linux-i686
   cmake --build build_pc -j$(nproc)
   ```

| Build | Game | Next to it |
| --- | --- | --- |
| Windows | `build_win/IronSights.exe` | `SDL2.dll`, the controller files, `Data/` |
| Linux | `build_pc/IronSights` | the controller files, `Data/` |

To play, put a copy of `baserom.u.z64` in that build's `Data/` folder. On Windows, copy the whole `build_win` folder somewhere in Windows (from WSL: `explorer.exe build_win`) and run `IronSights.exe` there. The Linux build needs 32-bit SDL2 and OpenGL on the machine that runs it. On Ubuntu or Debian:

```
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install libsdl2-2.0-0:i386 libgl1:i386
```

## Known issues

- On some Linux systems, levels draw as a few large blank triangles. Starting the game with these environment variables works around it:
  ```
  GE_FULLHEIGHT=0 GE_EDGEINSET=1 ./IronSights
  ```

## Contributing

Every repository change must update the [fork change inventory](FORK_CHANGES.md); see [AGENTS.md](AGENTS.md).

For hands, arms and weapon rendering, open the [visual review gallery](review/hand-gallery/index.html) locally in a browser. It compares PP7, AK47/KF7 and Klobb in regular, ADS and Hip-Fire Arms poses across six wardrobes, plus all generated weapon icons. See [generation and pre-commit setup](scripts/port/hand_gallery.md).

For aiming changes and bug reports, use [this fork's issues](https://github.com/pRizz/Iron-Sights/issues) and pull requests. The inherited [CONTRIBUTING.md](CONTRIBUTING.md) describes upstream contribution guidelines, including the rules for AI-assisted changes. General port development is coordinated in [upstream Project Midas](https://github.com/murkantor/Project-Midas).

## License

Iron Sights retains the upstream Project Midas port code's MIT license and attribution; see [LICENSE](LICENSE). The license does not cover the decompiled game code, GE itself or its assets. Bundled third-party code keeps its own license; see `tools/THIRD_PARTY_NOTICES.txt`.

## Acknowledgements

Iron Sights builds on [Project Midas](https://github.com/murkantor/Project-Midas) by murkantor and contributors, and the work of the people and projects credited below.

- [GoldenEye decompilation](https://github.com/n64decomp/007) by KholdFuzion and contributors
- [GoldenEye docs](https://github.com/kholdfuzion/goldeneye_docs)
- [Perfect Dark decompilation](https://github.com/n64decomp/perfect_dark) by Ryan Dwyer and contributors
- [Perfect Dark PC port](https://github.com/fgsfdsfgs/perfect_dark) by fgsfds
- [Perfect Dark netplay](https://github.com/murkantor/perfect_dark_netplay) (fork of fgsfds' `port-net`)
- [GE Plus](https://github.com/Joshua-1248/GoldenEye-007-Plus) by Joshua
- *The DOOM III Network Architecture* by J.M.P. van Waveren (id Software, 2006)
- Fast3D renderer by Emill and MaikelChan (MIT)
- [SDL2](https://github.com/libsdl-org/SDL) and [glad](https://github.com/Dav1dde/glad)
- [SDL_GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB)
- [ENet](https://github.com/lsalzman/enet)
- [xdelta3](https://github.com/jmacd/xdelta)
- [ido-static-recomp](https://github.com/decompals/ido-static-recomp), [rabbitizer](https://github.com/Decompollaboration/rabbitizer), [qemu-irix](https://github.com/n64decomp/qemu-irix) and [armips](https://github.com/Kingcom/armips)
- The 1964 emulator and its GE/Perfect Dark mouse injector
- [pj64raphnetraw](https://github.com/raphnet/pj64raphnetraw) by Raphael Assenat ([raphnet-tech](https://www.raphnet-tech.com)), the reference for the raphnet adapter protocol

GE is owned by Rare, Nintendo and the Bond rights holders. No ROM or extracted game assets are distributed. The review gallery contains rendered gameplay-model references; the underlying artwork belongs to its rights holders.
