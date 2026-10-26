"""ROM-free font mapping and native decoder checks; no Pillow needed."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
import hashlib
import json
from generate_enhanced_fonts import ROOT, ink_rows, mask_layout, native_glyphs, native_kerning, overhang_limits


def generated_masks():
    text = (ROOT/'port/fast3d/fonts/gepd_font_data.inc').read_text()
    rows = re.findall(r'\{0x([0-9a-f]+)ULL, (\d+), (\d+), (\d+), (\d+), (\d+), (\d+)\}', text)
    records = {int(row[0],16): tuple(map(int,row[1:])) for row in rows}
    payload = bytes(map(int,re.findall(r'\d+',text.split('font_masks[] = {',1)[1].split('}',1)[0])))
    sources = json.loads((ROOT/'port/fast3d/fonts/sources.json').read_text())['glyphs']
    for glyph in sources:
        w,h,gw,iw,offset,size = records[glyph['key']]
        encoded = payload[offset:offset+size]
        mask = b''.join(bytes([value])*count for count,value in zip(encoded[::2],encoded[1::2]))
        yield glyph,(w,h,gw,iw),mask


class EnhancedFonts(unittest.TestCase):
    def test_overhang_trimming_respects_native_kerning_and_baselines(self):
        # Arrange: a wide stroke and a neighbor with three blank leading texels.
        first = dict(glyph=dict(width=1,kerningindex=0), ink_width=12, rows={0:(1<<12)-1})
        second = dict(glyph=dict(width=1,kerningindex=1), ink_width=8, rows={0:0b11111000})
        kern = [0]*169
        # Act / assert: a normal one-unit gap preserves the complete stroke.
        self.assertEqual(overhang_limits([first,second], kern)[0], 12)
        kern[1] = 1
        # Tight kerning trims only protruding fringe; baseline-disjoint ink stays.
        self.assertEqual(overhang_limits([first,second], kern)[0], 11)
        second['rows'] = {1:0b11111000}
        self.assertEqual(overhang_limits([first,second], kern)[0], 12)

    def test_ink_rows_ignore_soft_fringe_and_align_native_baselines(self):
        # Arrange / act: two texture rows with samples either side of half intensity.
        rows = ink_rows(bytes([0,127,128,255, 255,0,128,0]), 4, 2)
        # Assert: bit positions retain X coordinates; native baseline uses 8x rows.
        self.assertEqual(rows, {16:0b1100,17:0b0101})

    def test_authored_overhang_does_not_squeeze_narrow_letters(self):
        # Arrange: uppercase I extends beyond the stock one-pixel box.
        glyph = dict(width=1, stride=8, height=8)
        # Act.
        layout = mask_layout((64,64), (0,4,12,57), glyph)
        # Assert: crop padding, preserving the authored pixel coordinates.
        self.assertEqual(layout['crop'], layout['fit'])
        self.assertEqual(layout['canvas'], (64,64))
        self.assertFalse(layout['horizontal_fit'])

    def test_long_punctuation_is_bounded_without_fitting_its_height(self):
        # Arrange / act: the source dash is much longer than its advance.
        layout = mask_layout((64,16), (3,4,51,13), dict(width=3, stride=8, height=2))
        # Assert: retain a bounded overhang, and keep the vertical stroke weight.
        self.assertTrue(layout['horizontal_fit'])
        self.assertEqual(layout['fit'], (32,16))
        self.assertEqual(layout['crop'], (51,16))
        self.assertFalse(layout['vertical_fit'])

    def test_vertical_fitting_is_independent_of_horizontal_overhang(self):
        # Arrange / act: Zurich digit 1 keeps the existing vertical policy.
        layout = mask_layout((64,128), (5,1,33,125), dict(width=4, stride=8, height=11))
        # Assert: only height is fitted; spare texture columns retain the stroke.
        self.assertEqual(layout['crop'], (40,125))
        self.assertEqual(layout['fit'], (40,88))
        self.assertFalse(layout['horizontal_fit'])
        self.assertTrue(layout['vertical_fit'])

    def test_empty_and_full_stride_masks_remain_bounded(self):
        # Arrange / act: empty input and a glyph with no spare texture columns.
        empty = mask_layout((64,64), None, dict(width=1, stride=8, height=8))
        full = mask_layout((128,64), (0,0,70,64), dict(width=8, stride=8, height=8))
        # Assert.
        self.assertEqual(empty['fit'], (16,64))
        self.assertFalse(empty['horizontal_fit'])
        self.assertEqual(full['fit'], (64,64))
        self.assertTrue(full['horizontal_fit'])

    def test_generated_masks_preserve_strokes_and_bound_all_overhangs(self):
        # Arrange: checked-in masks, independently decoded without Pillow/ROM.
        sources = []
        strokes = {}
        # Act / assert: every metadata bound/hash matches the actual embedded ink.
        for glyph,(w,h,gw,iw),mask in generated_masks():
            sources.append(glyph)
            self.assertEqual(len(mask), w*h*64)
            self.assertEqual(hashlib.sha256(mask).hexdigest(), glyph['mask_sha256'])
            self.assertEqual(gw, glyph['width'])
            self.assertEqual(iw, glyph['ink_width'])
            self.assertLessEqual(iw, min(w*8,(gw+1)*8))
            self.assertFalse(any(mask[x] for x in range(len(mask)) if x%(w*8) >= iw))
            strokes[glyph['font'],glyph['character']] = sum(value >= 128 for value in mask[(h*4)*(w*8):(h*4+1)*(w*8)])
        self.assertEqual(strokes['BankGothic','I'], 11)
        self.assertEqual(strokes['BankGothic','T'], 13)
        self.assertEqual({(g['font'],g['character']) for g in sources if g['horizontal_fit']},
                         {('BankGothic','-'),('BankGothic','_')})
        self.assertEqual({(g['font'],g['character'],g['overhang_clip']) for g in sources if g['overhang_clip']},
                         {('BankGothic','p',1),('ZurichBold','r',3)})

    def test_all_generated_pairs_preserve_native_spacing_without_ink_collisions(self):
        # Arrange: independent pixel-coordinate sets, not generator row bitsets.
        fonts = {}
        for glyph,(w,h,gw,iw),mask in generated_masks():
            points = {(i%(w*8),i//(w*8)+glyph['baseline']*8) for i,v in enumerate(mask) if v >= 128}
            fonts.setdefault(glyph['font'], []).append((glyph,points))
        # Act / assert: both families, all 17,298 ordered supported glyph pairs.
        for font,entries in fonts.items():
            kern = native_kerning(font)
            for a,first in entries:
                for b,second in entries:
                    shift = (a['width']+1-kern[a['kerningindex']*13+b['kerningindex']])*8
                    self.assertFalse(first & {(x+shift,y) for x,y in second},
                                     (font,a['character']+b['character']))

    def test_native_mapping_and_unsupported_character_fallback(self):
        text = (ROOT/"port/fast3d/fonts/gepd_font_data.inc").read_text()
        records = {int(key,16):(int(w),int(h)) for key,w,h in re.findall(r"\{0x([0-9a-f]+)ULL, (\d+), (\d+),", text)}
        sources = {g['key']: g for g in json.loads((ROOT/'port/fast3d/fonts/sources.json').read_text())['glyphs']}
        self.assertEqual(len(records), 186)
        for font in ("BankGothic", "ZurichBold"):
            for glyph in native_glyphs(font):
                key = glyph["key"]
                if glyph["character"] == "~":
                    self.assertNotIn(key, records)
                else:
                    self.assertEqual(records[key], (glyph["stride"],glyph["height"]))
                    for field in ('width','height','stride','baseline','kerningindex'):
                        self.assertEqual(sources[key][field], glyph[field])

    @unittest.skipUnless(shutil.which("g++"), "Native font decoder test requires a C++ compiler")
    def test_native_masks_and_malformed_encoding(self):
        with tempfile.TemporaryDirectory() as folder:
            binary = Path(folder)/"fonts-test"
            subprocess.run(["g++", "-std=c++20", "-O2", "-I"+str(ROOT/"port/fast3d"),
                            str(ROOT/"scripts/port/test_enhanced_fonts.cpp"), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
