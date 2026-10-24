import { sha256 } from './rom';

export type UserFile = { path: string; bytes: Uint8Array<ArrayBuffer> };
type BackupFile = { path: string; data: string; sha256: string };
export type Backup = { format: 'project-midas-saves'; version: 1; createdAt: string; files: BackupFile[] };
export const MAX_BACKUP_TEXT = 8 * 1024 * 1024;
const MAX_FILES = 256;
export const MAX_USER_FILE_BYTES = 256 * 1024;
const MAX_TOTAL_BYTES = 4 * 1024 * 1024;

export function allowedUserPath(path: string): boolean {
  if (path === 'Data/Saves/eeprom.bin' || path === 'midas.ini' || path === 'gamecontrolleruser.txt') return true;
  return /^Data\/Profiles\/[A-Za-z0-9 _-]{1,63}\.geprofile$/.test(path)
    || /^Data\/Devices\/[A-Za-z0-9_-]{1,128}\.gedevice$/.test(path);
}

export function validateUserFiles(files: UserFile[]): void {
  if (files.length > MAX_FILES) throw new Error('The backup contains too many files.');
  const paths = new Set<string>();
  let total = 0;
  for (const file of files) {
    if (!allowedUserPath(file.path) || paths.has(file.path)) {
      throw new Error('The backup contains an unsupported or duplicate file path.');
    }
    paths.add(file.path);
    total += file.bytes.length;
    if (file.bytes.length > MAX_USER_FILE_BYTES || total > MAX_TOTAL_BYTES) throw new Error('The backup is too large.');
    if (file.path === 'Data/Saves/eeprom.bin' && file.bytes.length !== 512) {
      throw new Error('The campaign save must contain exactly 512 bytes.');
    }
  }
}

function base64(bytes: Uint8Array): string {
  let binary = '';
  for (let start = 0; start < bytes.length; start += 8192) {
    binary += String.fromCharCode(...bytes.subarray(start, start + 8192));
  }
  return btoa(binary);
}

function decodeBase64(text: string): Uint8Array<ArrayBuffer> {
  if (text.length > 4 * Math.ceil(MAX_USER_FILE_BYTES / 3)
      || !/^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(text)) {
    throw new Error('The backup contains invalid or oversized file data.');
  }
  const bytes = Uint8Array.from(atob(text), value => value.charCodeAt(0));
  if (base64(bytes) !== text) throw new Error('The backup contains invalid file data.');
  return bytes;
}

export async function createBackup(files: UserFile[], createdAt: string): Promise<string> {
  validateUserFiles(files);
  const encoded = await Promise.all([...files].sort((a, b) => a.path.localeCompare(b.path)).map(async file => ({
    path: file.path, data: base64(file.bytes), sha256: await sha256(file.bytes),
  })));
  const backup: Backup = { format: 'project-midas-saves', version: 1, createdAt, files: encoded };
  return JSON.stringify(backup, null, 2) + '\n';
}

function object(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error('The backup format is invalid.');
  return value as Record<string, unknown>;
}

export async function parseBackup(text: string): Promise<UserFile[]> {
  if (text.length > MAX_BACKUP_TEXT) throw new Error('The backup is too large.');
  const backup = object(JSON.parse(text));
  if (backup.format !== 'project-midas-saves' || backup.version !== 1 || !Array.isArray(backup.files)) {
    throw new Error('Choose a Project Midas save backup with supported format version 1.');
  }
  if (backup.files.length > MAX_FILES) throw new Error('The backup contains too many files.');
  const files: UserFile[] = [];
  for (const raw of backup.files) {
    const file = object(raw);
    if (typeof file.path !== 'string' || typeof file.data !== 'string' || typeof file.sha256 !== 'string') {
      throw new Error('The backup contains an invalid file record.');
    }
    const bytes = decodeBase64(file.data);
    if (await sha256(bytes) !== file.sha256) throw new Error('The backup is damaged: a file checksum does not match.');
    files.push({ path: file.path, bytes });
  }
  validateUserFiles(files);
  return files;
}
