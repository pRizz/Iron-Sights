# Room console and controller models

These are independently authored models of Nintendo hardware, not official
Nintendo assets. They replace the procedural console/controller shells. The
cartridge and CRT remain original geometry; the supplied cartridge label has
its separate [artwork notice](ARTWORK.md). No ROM data is included.

## Console

"[Simple N64 model](https://sketchfab.com/3d-models/simple-n64-model-a17efae58ebd4f1d8e7db0f5cf6f0023)"
by [Swayne](https://sketchfab.com/Swayne), licensed
[Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/).
The original author's public Sketchfab API reports that license and 5,739
triangles for the complete set. The selected console has 2,228 triangles.

The acquisition source is the attributed redistribution in
[k0bolde/tfjam23](https://github.com/k0bolde/tfjam23/blob/7cd0cc7f65917a56fa4232a71f2d005bb5f911be/src/Credits.gd),
at commit `7cd0cc7f65917a56fa4232a71f2d005bb5f911be`, `assets/n64.glb`.
Its source SHA-256 is
`7461092b58cfa3e91214dcd5dbdf04a34532a0e76275c93572cbd82124265a12`.

Changes: keep only the console, normalize its plastic to the room's standard
material and dark expansion cover, isolate the existing 72-triangle power switch
and recolor it red for the accepted interaction; re-export as an embedded GLB.
The application fits it uniformly to the room, animates that switch, adds the
front-center power LED and original procedural gray controller-plug barrel with
black strain relief. The controller cable starts at the plug's outer end; other
cable curves are retained. These additions are runtime geometry; the pinned GLB
is unchanged. The source cartridge/controller
are excluded. Joseph Rohm's earlier candidate still requires an authenticated
Sketchfab download; no files from that candidate are used or claimed here.

## Controller

"[N64 Controller](https://sketchfab.com/3d-models/n64-controller-8fb694b8c4bf4bbb8bc66f38503f4ffb)"
by [Marshle, formerly CodySmith1](https://sketchfab.com/Marshle1), licensed
[Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/).

Acquired from the attributed compact redistribution in
[maxnau89/mupen64plus-ae-quest](https://github.com/maxnau89/mupen64plus-ae-quest/blob/e77a89c3388a27ae29c464ad2f75e3985dffc869/app/src/main/assets/quest/CREDITS.txt),
commit `e77a89c3388a27ae29c464ad2f75e3985dffc869`. That redistribution flattened
the scene, scaled it, assigned button parts and downscaled its base-color image
to 1024x1024; normal/metallic/roughness maps were omitted. Its model files retain
their explicitly stated CC BY 4.0 asset license. No application code is copied.

Source hashes:

- `n64_controller.bin`:
  `aeed171ad33c1366baf466cc1dcc823628307cc0729166dd712b5d00db3d1716`.
- `n64_controller.png`:
  `b36feec550afc2f3dffe857022d0ccfb0eece8d0bef2c306eae7a48feeaee99c`.

Our conversion packages its 9,948 triangles, original normals, UVs and unchanged
PNG in one embedded GLB, with a room plastic material. Runtime placement uses
uniform scaling/rotation. No model/texture URI is fetched from a third party.

## Regeneration and shipped files

From `web/`, run `node tools/prepare-room-models.mjs`. It downloads only the pinned
sources, checks every source digest before conversion and records input files,
output sizes/hashes and a report under ignored `build/web-room/model-preparation`.
The generated, licensed public props belong in Git. Review them and update the
explicit `tools/room-assets.ts` allowlist when deliberately regenerating.

| Public file           |   Bytes | SHA-256                                                            |
| --------------------- | ------: | ------------------------------------------------------------------ |
| `room/console.glb`    |  203896 | `26ea8b9dae78d1d377923bd87b0034633e0e78c0f4303217bf29ad53f25a7c78` |
| `room/controller.glb` | 1204348 | `a4edc2ac9dc2907c9bd1a14b22c3c8ed794ded27f11e02c4d168edf7a10cad6a` |

Site assembly includes this complete attribution/change notice in
`THIRD_PARTY_NOTICES.txt`, accessible from the retained F2 controls.
