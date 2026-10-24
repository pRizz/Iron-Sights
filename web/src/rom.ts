export const ROM_BYTES = 12_582_912;
export const ROM_SHA256 = '2cdcec8a9f0cb6e36337f3ee39d8ad105dc8afa6ba1c02d466e8f5b771f9a162';
export type RomOrder = 'z64' | 'v64' | 'n64';
export type ValidatedRom = { bytes: Uint8Array<ArrayBuffer>; order: RomOrder; name: string };

export function romOrder(bytes: Uint8Array): RomOrder {
  const header = Array.from(bytes.subarray(0, 4)).join(',');
  if (header === '128,55,18,64') return 'z64';
  if (header === '55,128,64,18') return 'v64';
  if (header === '64,18,55,128') return 'n64';
  throw new Error('This file has no supported N64 ROM header. Choose a .z64, .v64 or .n64 ROM.');
}

export function normalizedRom(bytes: Uint8Array, order: RomOrder): Uint8Array<ArrayBuffer> {
  if (bytes.length % 4) throw new Error('The ROM is truncated.');
  const normalized = new Uint8Array(bytes.length);
  const xor = order === 'n64' ? 3 : order === 'v64' ? 1 : 0;
  for (let index = 0; index < bytes.length; index++) normalized[index] = bytes[index ^ xor];
  return normalized;
}

export async function sha256(bytes: Uint8Array<ArrayBuffer>): Promise<string> {
  const digest = await crypto.subtle.digest('SHA-256', bytes);
  return Array.from(new Uint8Array(digest), byte => byte.toString(16).padStart(2, '0')).join('');
}

export async function validateRom(file: Blob & { name?: string }): Promise<ValidatedRom> {
  if (file.size !== ROM_BYTES) {
    throw new Error('Choose the original 12 MiB US GoldenEye 007 ROM. This file has a different size.');
  }
  const bytes = new Uint8Array(await file.arrayBuffer());
  const order = romOrder(bytes);
  if (await sha256(normalizedRom(bytes, order)) !== ROM_SHA256) {
    throw new Error('This ROM is not the supported original US release. Patched ROMs and other regions are unsupported.');
  }
  return { bytes, order, name: file.name || 'GoldenEye ROM' };
}
