import { describe, expect, test } from 'bun:test';
import { allowedUserPath, createBackup, parseBackup, MAX_BACKUP_TEXT, type UserFile } from './backup';

describe('save backup boundary', () => {
  test.each(['rom.z64', 'Data/SaveStates/save.mstate', '../midas.ini', '/midas.ini',
    'Data/Profiles/../../rom.z64', 'Data/Profiles/x.geprofile/evil', 'Data/Saves/other.bin'])('rejects %s', path => {
    // Arrange / Act / Assert
    expect(allowedUserPath(path)).toBe(false);
  });

  test('round trips binary campaign data and text settings', async () => {
    // Arrange
    const files: UserFile[] = [
      { path: 'Data/Saves/eeprom.bin', bytes: Uint8Array.from({ length: 512 }, (_, n) => n % 256) },
      { path: 'midas.ini', bytes: new TextEncoder().encode('[display]\nmsaa=1\n') },
    ];
    // Act
    const restored = await parseBackup(await createBackup(files, '2026-10-24T00:00:00Z'));
    // Assert
    expect(restored).toEqual(files);
  });

  test('rejects duplicate paths', async () => {
    // Arrange
    const file = { path: 'midas.ini', bytes: new Uint8Array() };
    // Act / Assert
    await expect(createBackup([file, file], '')).rejects.toThrow('duplicate');
  });

  test('rejects truncated EEPROM data', async () => {
    // Arrange
    const file = { path: 'Data/Saves/eeprom.bin', bytes: new Uint8Array(511) };
    // Act / Assert
    await expect(createBackup([file], '')).rejects.toThrow('512 bytes');
  });

  test('rejects future schema versions', async () => {
    // Arrange
    const raw = JSON.stringify({ format: 'project-midas-saves', version: 2, files: [] });
    // Act / Assert
    await expect(parseBackup(raw)).rejects.toThrow('version 1');
  });

  test('detects changed file contents', async () => {
    // Arrange
    const raw = JSON.parse(await createBackup([{ path: 'midas.ini', bytes: Uint8Array.of(1) }], ''));
    raw.files[0].data = 'Ag==';
    // Act / Assert
    await expect(parseBackup(JSON.stringify(raw))).rejects.toThrow('checksum');
  });

  test('rejects malformed base64', async () => {
    // Arrange
    const raw = { format: 'project-midas-saves', version: 1, files: [{ path: 'midas.ini', data: '!!!', sha256: '' }] };
    // Act / Assert
    await expect(parseBackup(JSON.stringify(raw))).rejects.toThrow('invalid');
  });

  test.each(['Data/Profiles/Agent 007.geprofile', 'Data/Devices/kbm.gedevice',
    'gamecontrolleruser.txt'])('allows %s', path => {
    // Arrange / Act / Assert
    expect(allowedUserPath(path)).toBe(true);
  });

  test.each(['null', '[]', '"string"'])('rejects a non-object envelope %s', async text => {
    // Arrange / Act / Assert
    await expect(parseBackup(text)).rejects.toThrow('format is invalid');
  });

  test('bounds input before JSON parsing', async () => {
    // Arrange
    const text = ' '.repeat(MAX_BACKUP_TEXT + 1);
    // Act / Assert
    await expect(parseBackup(text)).rejects.toThrow('too large');
  });

  test('bounds the number of file records before decoding', async () => {
    // Arrange
    const raw = { format: 'project-midas-saves', version: 1, files: Array(257).fill(null) };
    // Act / Assert
    await expect(parseBackup(JSON.stringify(raw))).rejects.toThrow('too many');
  });

  test('bounds individual files when creating backups', async () => {
    // Arrange
    const file = { path: 'midas.ini', bytes: new Uint8Array(256 * 1024 + 1) };
    // Act / Assert
    await expect(createBackup([file], '')).rejects.toThrow('too large');
  });

  test('bounds the total decoded size', async () => {
    // Arrange
    const files = Array.from({ length: 17 }, (_, n) => ({
      path: `Data/Profiles/Agent${n}.geprofile`, bytes: new Uint8Array(256 * 1024),
    }));
    // Act / Assert
    await expect(createBackup(files, '')).rejects.toThrow('too large');
  });

  test('rejects incorrectly typed file records', async () => {
    // Arrange
    const raw = { format: 'project-midas-saves', version: 1, files: [{ path: 42, data: '', sha256: '' }] };
    // Act / Assert
    await expect(parseBackup(JSON.stringify(raw))).rejects.toThrow('invalid file record');
  });

  test('rejects non-canonical base64 padding bits', async () => {
    // Arrange
    const raw = { format: 'project-midas-saves', version: 1, files: [{ path: 'midas.ini', data: 'AB==', sha256: '' }] };
    // Act / Assert
    await expect(parseBackup(JSON.stringify(raw))).rejects.toThrow('invalid file data');
  });
});
