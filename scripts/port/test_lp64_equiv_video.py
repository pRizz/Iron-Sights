"""ROM-free tests of lp64_equiv.py run --video and sensitivity (LP64-006 Tasks 5, 7 and 9) through
stand-in executables (lp64_equiv_testkit.py)."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_equiv  # noqa: E402
import lp64_equiv_profile  # noqa: E402
from lp64_equiv_testkit import (AUDIO_LOCKED, COMPARE_ON, DAM1_GOOD, DAM_GOOD, LOCKSTEP_ON,  # noqa: E402
                                StandIns, demo_sim, sim, wav)


class Video(StandIns):
    """`run --video` through a stand-in that writes the picture digest and the recording when named."""
    VIDEO_LOG = DAM_GOOD.replace(LOCKSTEP_ON, LOCKSTEP_ON + 'ge: video: window + GL context up\n' + AUDIO_LOCKED)
    GFX = 'F n=1 tick=1 draws=1 tris=2 vtx=3bee89fb tex=0:811c9dc5 state=0383debb post=e5d4f569\n'

    def capture(self, recording: bytes = wav((1, -1), (4000, -4000))) -> str:
        """A script line that writes RECORDING when the runner names a recording."""
        path = self.folder/f'canned{len(list(self.folder.iterdir()))}.wav'
        path.write_bytes(recording)
        return f'[ -n "$GE_AUDIO_CAPTURE" ] && cp "{path}" "$GE_AUDIO_CAPTURE"\n'

    def write_streams(self, recording: bytes = wav((1, -1), (4000, -4000))) -> str:
        """A script that writes the digest and RECORDING when the runner names them."""
        return f'[ -n "$GE_GFXDIGEST" ] && printf "{self.GFX}" > "$GE_GFXDIGEST"\n' + self.capture(recording)

    def env_seen(self) -> Path:
        return self.folder/'env.txt'

    def video_fake(self, script: str | None = None, log: str = VIDEO_LOG) -> Path:
        """A stand-in that runs SCRIPT (by default write_streams') and, in a case's runs, saves its GE_
        variables and midas.ini."""
        keep = (f'[ -n "$GE_SIMTRACE" ] && {{ env | grep "^GE_" | sort > "{self.env_seen()}"; '
                f'cat midas.ini > "{self.folder/"ini.txt"}"; }}')
        return self.fake(log, script=f'{self.write_streams() if script is None else script}\n{keep}')

    def seen(self) -> dict[str, str]:
        return dict(line.split('=', 1) for line in self.env_seen().read_text().splitlines())

    def test_video_names_the_digest_and_recording_and_skips_drawing(self):
        # Act
        status, out = self.run_dam(self.video_fake(), '--video')

        # Assert
        self.assertEqual(status, 0, out)
        env = self.seen()
        self.assertTrue(env['GE_GFXDIGEST'].endswith('/run1/stage-dam.gfx'))
        self.assertTrue(env['GE_AUDIO_CAPTURE'].endswith('/run1/stage-dam.wav'))
        self.assertEqual(env['GE_AUDIO_LOCK'], '1')
        self.assertEqual(env['GE_GFXDIGEST_NODRAW'], '1')

    def test_draw_leaves_drawing_on(self):
        # Act
        status, out = self.run_dam(self.video_fake(), '--video', '--draw')

        # Assert
        self.assertEqual(status, 0, out)
        self.assertNotIn('GE_GFXDIGEST_NODRAW', self.seen())

    def test_video_run_fails_without_its_digest_and_recording(self):
        # Act
        status, out = self.run_dam(self.video_fake(script=''), '--video')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam: FAIL (gfx: no stage-dam.gfx)', out)

    def test_video_run_fails_a_torn_digest(self):
        # Act
        script = (f'[ -n "$GE_GFXDIGEST" ] && printf "%s" "{self.GFX.strip()}" > "$GE_GFXDIGEST"\n'
                  + self.capture())
        status, out = self.run_dam(self.video_fake(script=script), '--video')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam: FAIL (gfx unreadable:', out)
        self.assertIn('the last line has no newline', out)

    def test_video_run_fails_a_recording_that_is_not_a_wav(self):
        # Act
        script = (f'[ -n "$GE_GFXDIGEST" ] && printf "{self.GFX}" > "$GE_GFXDIGEST"\n'
                  '[ -n "$GE_AUDIO_CAPTURE" ] && printf wav > "$GE_AUDIO_CAPTURE"\n')
        status, out = self.run_dam(self.video_fake(script=script), '--video')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam: FAIL (snd unreadable:', out)

    def test_video_run_fails_a_silent_recording(self):
        # Act
        status, out = self.run_dam(self.video_fake(script=self.write_streams(wav((0, 0), (999, -999)))), '--video')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam: FAIL (snd: stage-dam.wav peaks at 999; a recording must reach 1000)', out)

    def test_video_run_fails_without_the_sound_lock(self):
        # Act
        status, out = self.run_dam(self.video_fake(log=self.VIDEO_LOG.replace(AUDIO_LOCKED, '')), '--video')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam: FAIL (missing line: ge: audio: locked to game fields)', out)

    def test_video_agreement_fails_a_recording_that_differs_from_itself(self):
        # Arrange: the second run of the case (the third start, after the build check) records another sample
        counter = self.folder/'runs'
        first, second = self.capture(wav((1, -1), (4000, -4000))), self.capture(wav((1, -1), (4000, 9)))
        script = (f'echo x >> "{counter}"\n'
                  f'if [ "$(wc -l < "{counter}")" -gt 2 ]; then\n{second}else\n{first}fi\n'
                  f'[ -n "$GE_GFXDIGEST" ] && printf "{self.GFX.strip()}\\n" > "$GE_GFXDIGEST"\n')

        # Act
        status, out = self.run_dam(self.video_fake(script=script), '--video', '--repeat', '2', '--jobs', '1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: stage-dam snd differs from itself at 1 (R frame=0 field=1, run2)', out)

    def test_video_agreement_leaves_a_demo_recording_unjudged_and_says_why(self):
        # Arrange: a paced demo's recording differs between runs; its playback does not
        counter = self.folder/'runs'
        first, second = self.capture(wav((4000, -1))), self.capture(wav((4000, -1), (2, -2)))
        script = (f'echo x >> "{counter}"\n'
                  f'if [ "$(wc -l < "{counter}")" -gt 2 ]; then\n{second}else\n{first}fi\n'
                  f'[ -n "$GE_GFXDIGEST" ] && printf "{self.GFX.strip()} demo=1\\n" > "$GE_GFXDIGEST"\n')
        exe = self.fake(DAM1_GOOD.replace(COMPARE_ON, COMPARE_ON + 'ge: video: window + GL context up\n' + AUDIO_LOCKED),
                        trace=demo_sim(2), script=script)

        # Act
        status, out = self.equiv('run', '--rom', str(self.rom), '--set', 'demos', '--only', 'dam1', '--video',
                                 '--repeat', '2', '--jobs', '1', '--output', str(self.output), str(exe))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('dam1: snd not judged: ' + lp64_equiv.DEMO_SND_REASON, out)
        self.assertIn('OK: 1 of 1 cases agree with themselves', out)

    def test_video_agreement_fails_a_digest_that_differs_from_itself(self):
        # Arrange: the second run of the case (the third start, after the build check) draws another frame
        counter = self.folder/'runs'
        other = self.GFX.replace('vtx=3bee89fb', 'vtx=00000000')
        script = (f'echo x >> "{counter}"\n'
                  f'if [ "$(wc -l < "{counter}")" -gt 2 ]; then g="{other}"; else g="{self.GFX}"; fi\n'
                  '[ -n "$GE_GFXDIGEST" ] && printf "$g" > "$GE_GFXDIGEST"\n'
                  + self.capture())

        # Act
        status, out = self.run_dam(self.video_fake(script=script), '--video', '--repeat', '2', '--jobs', '1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: stage-dam gfx differs from itself at 1 (vtx, run2)', out)

    def test_video_env_refuses_what_the_video_runner_sets(self):
        # Arrange
        exe = self.video_fake()

        # Act
        results = {name: self.run_dam(exe, '--video', '--env', f'{name}=1') for name in ('GE_SHOT', 'HOME')}

        # Assert
        for name, (status, out) in results.items():
            self.assertEqual(status, 2, f'{name}: {out}')
            self.assertIn(f'--env {name} is refused with --video', out)

    def test_all_profile_writes_its_ini_and_loads_its_fonts(self):
        # Act
        status, out = self.run_dam(self.video_fake(), '--video', '--profile', 'all')

        # Assert
        self.assertEqual(status, 0, out)
        self.assertEqual((self.folder/'ini.txt').read_text(), lp64_equiv_profile.profile_ini('all'))
        self.assertEqual(self.seen()['GE_IRONSIGHTS_FONTS'], '1')


class Sensitivity(StandIns):
    def test_sensitivity_fails_when_two_seeds_agree(self):
        # Arrange: the trace ignores GE_SEED
        exe = self.fake(DAM_GOOD)

        # Act
        status, out = self.equiv('sensitivity', '--rom', str(self.rom), '--output', str(self.output), str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: sim is the same between seeds', out)

    def test_sensitivity_passes_when_the_seed_changes_the_trace(self):
        # Arrange
        other = self.canned('other.sim', sim(seed_at={1: '0000000000000002'}))
        log = self.canned('seed2.log', DAM_GOOD)
        exe = self.fake(DAM_GOOD, script=f'if [ "$GE_SEED" = 2 ]; then cp "{other}" "$GE_SIMTRACE"; '
                                         f'cat "{log}" >&2; exit 0; fi')

        # Act
        status, out = self.equiv('sensitivity', '--rom', str(self.rom), '--output', str(self.output), str(exe))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('OK: sim differs between seeds', out)

    def test_video_sensitivity_judges_sim_gfx_and_snd(self):
        # Arrange: GE_SEED=2 changes the trace, the digest and the recording
        other = self.canned('other.sim', sim(seed_at={1: '0000000000000002'}))
        gfx = Video.GFX
        log = self.canned('seed2.log', Video.VIDEO_LOG)
        one, two = self.canned('one.wav', ''), self.canned('two.wav', '')
        one.write_bytes(wav((4000, -1)))
        two.write_bytes(wav((4000, 5)))
        script = ('if [ "$GE_SEED" = 2 ]; then '
                  f'cp "{other}" "$GE_SIMTRACE"; printf "{gfx.replace("vtx=3bee89fb", "vtx=00000000")}" > "$GE_GFXDIGEST"; '
                  f'cp "{two}" "$GE_AUDIO_CAPTURE"; cat "{log}" >&2; exit 0; fi\n'
                  f'[ -n "$GE_GFXDIGEST" ] && printf "{gfx}" > "$GE_GFXDIGEST"\n'
                  f'[ -n "$GE_AUDIO_CAPTURE" ] && cp "{one}" "$GE_AUDIO_CAPTURE"\n')
        exe = self.fake(Video.VIDEO_LOG, script=script)

        # Act
        status, out = self.equiv('sensitivity', '--video', '--rom', str(self.rom), '--output', str(self.output),
                                 str(exe))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('OK: sim, gfx and snd differ between seeds', out)



if __name__ == '__main__':
    unittest.main()
