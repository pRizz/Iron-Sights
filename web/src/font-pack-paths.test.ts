import { describe, expect, test } from 'bun:test';
import { allowedUserPath } from './backup';
import { fontPackPath } from './font-pack-paths';

describe('Enhanced font files', () => {
  test.each(['Data/FontPacks/gepd/GOLDENEYE#CDE33EA7#4#1_rgb.png', 'Data/FontPacks/gepd/GOLDENEYE#CDE33EA7#4#1_rgb.png.part',
    'Data/FontPacks/gepd/SOURCE.txt'])('persists %s without backing it up', path => {
    // Arrange / Act / Assert
    expect(fontPackPath(path)).toBe(true);
    expect(allowedUserPath(path)).toBe(false);
  });

  test.each(['Data/FontPacks/gepd/../../midas.ini', 'Data/FontPacks/gepd/other.png', 'Data/FontPacks/other/SOURCE.txt',
    'Data/FontPacks/gepd/GOLDENEYE#cde33ea7#4#1_rgb.png', 'Data/FontPacks/gepd/GOLDENEYE#CDE33EA7#4#1_rgb.png/x'])('ignores %s', path => {
    // Arrange / Act / Assert
    expect(fontPackPath(path)).toBe(false);
  });
});
