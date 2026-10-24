import { describe, expect, test } from 'bun:test';
import { normalizedRom, romOrder, sha256, validateRom } from './rom';

describe('local ROM validation', () => {
  test.each([
    ['z64', [128, 55, 18, 64]],
    ['v64', [55, 128, 64, 18]],
    ['n64', [64, 18, 55, 128]],
  ] as const)('normalizes %s without changing the original', (order, values) => {
    // Arrange
    const original = Uint8Array.from(values);
    // Act
    const normalized = normalizedRom(original, romOrder(original));
    // Assert
    expect(normalized).toEqual(Uint8Array.from([128, 55, 18, 64]));
    expect(original).toEqual(Uint8Array.from(values));
    expect(romOrder(original)).toBe(order);
  });

  test('rejects unrelated files', () => {
    // Arrange / Act / Assert
    expect(() => romOrder(new Uint8Array(4))).toThrow('N64 ROM header');
  });

  test('rejects incomplete words', () => {
    // Arrange / Act / Assert
    expect(() => normalizedRom(new Uint8Array(3), 'n64')).toThrow('truncated');
  });

  test('checks file size before reading the file', async () => {
    // Arrange
    const file = new Blob(['synthetic, not a ROM']);
    // Act / Assert
    await expect(validateRom(file)).rejects.toThrow('different size');
  });

  test('uses the standard SHA-256 digest', async () => {
    // Arrange
    const input = new TextEncoder().encode('abc');
    // Act
    const digest = await sha256(input);
    // Assert
    expect(digest).toBe('ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad');
  });

  test('rejects a correctly sized synthetic ROM with the wrong digest', async () => {
    // Arrange: synthetic zeros with an N64 header, no game data.
    const bytes = new Uint8Array(12_582_912);
    bytes.set([128, 55, 18, 64]);
    // Act / Assert
    await expect(validateRom(new Blob([bytes]))).rejects.toThrow('not the supported original');
  });
});
