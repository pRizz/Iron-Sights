import { allowedUserPath, MAX_USER_FILE_BYTES, MAX_BACKUP_TEXT, createBackup, parseBackup, validateUserFiles, type UserFile } from './backup';
import type { GameFS } from './runtime-types';
import { createSyncQueue, type SaveStatus } from './sync-queue';

export const USER_MOUNT = '/midas-ads-user';
const PENDING_RESTORE = USER_MOUNT + '/.pending-save-restore.json';

function synchronize(fs: GameFS, populate: boolean): Promise<void> {
  return new Promise((resolve, reject) => fs.syncfs(populate, maybeError => {
    if (maybeError) reject(maybeError instanceof Error ? maybeError : new Error(String(maybeError)));
    else resolve();
  }));
}

function knownPaths(fs: GameFS): string[] {
  const candidates = ['midas.ini', 'gamecontrolleruser.txt', 'Data/Saves/eeprom.bin'];
  for (const directory of ['Data/Profiles', 'Data/Devices']) {
    const path = `${USER_MOUNT}/${directory}`;
    if (!fs.analyzePath(path).exists) continue;
    if (!fs.isDir(fs.lstat(path).mode)) throw new Error(`Saved ${directory} is not a directory.`);
    candidates.push(...fs.readdir(path).map(name => `${directory}/${name}`).filter(allowedUserPath));
  }
  return candidates.filter(path => fs.analyzePath(`${USER_MOUNT}/${path}`).exists);
}

function knownFiles(fs: GameFS): UserFile[] {
  const files: UserFile[] = [];
  for (const path of knownPaths(fs)) {
    const absolute = `${USER_MOUNT}/${path}`;
    const stat = fs.lstat(absolute);
    if (!fs.isFile(stat.mode) || stat.size > MAX_USER_FILE_BYTES) throw new Error(`Saved ${path} is invalid or too large.`);
    files.push({ path, bytes: fs.readFile(absolute) });
  }
  validateUserFiles(files);
  return files;
}

function installWriteTracking(fs: GameFS, changed: () => void): () => void {
  const close = fs.close.bind(fs);
  const rename = fs.rename.bind(fs);
  const unlink = fs.unlink.bind(fs);
  let stamp = Date.now();
  const canonical = (path: string) => fs.getPath(fs.lookupPath(path, { follow: true }).node);
  const userPath = (path: string) => path.startsWith(USER_MOUNT + '/') && allowedUserPath(path.slice(USER_MOUNT.length + 1));
  const touch = (path: string) => {
    // IDBFS compares millisecond mtimes. Two quick writes must not look equal.
    stamp = Math.max(Date.now(), stamp + 1);
    fs.utime(path, stamp, stamp);
  };
  fs.close = stream => {
    const path = fs.getPath(stream.node);
    const wrote = (stream.flags & 3) !== 0 && userPath(path);
    close(stream);
    if (!wrote) return;
    if (fs.analyzePath(path).exists) touch(path);
    changed();
  };
  fs.rename = (from, to) => {
    const removedUserFile = userPath(canonical(from));
    rename(from, to);
    const destination = canonical(to);
    if (userPath(destination)) touch(destination);
    if (removedUserFile || userPath(destination)) changed();
  };
  fs.unlink = path => {
    const removedUserFile = userPath(canonical(path));
    unlink(path);
    if (removedUserFile) changed();
  };
  return () => { fs.close = close; fs.rename = rename; fs.unlink = unlink; };
}

export async function mountPersistentFiles(fs: GameFS, idbfs: unknown, report: (status: SaveStatus) => void) {
  fs.mkdirTree(USER_MOUNT);
  fs.mount(idbfs, {}, USER_MOUNT);
  await synchronize(fs, true);
  knownFiles(fs);
  fs.symlink(USER_MOUNT, '/user');
  let started = false;
  let tracking = true;
  const queue = createSyncQueue(async () => {
    knownFiles(fs); // Reject malformed/truncated campaign data before overwriting durable files.
    await synchronize(fs, false);
  }, report);
  const stopTracking = installWriteTracking(fs, () => { if (tracking) queue.changed(); });
  const snapshot = () => knownFiles(fs);

  function writeSnapshot(files: UserFile[]): void {
    for (const old of knownPaths(fs)) fs.unlink(`${USER_MOUNT}/${old}`);
    for (const file of files) {
      const path = `${USER_MOUNT}/${file.path}`;
      fs.mkdirTree(path.slice(0, path.lastIndexOf('/')));
      fs.writeFile(path, file.bytes);
    }
  }

  async function replace(files: UserFile[], consumePending = false): Promise<void> {
    if (started) throw new Error('Reload the page before restoring a backup; the running game holds its own save data.');
    validateUserFiles(files);
    await queue.flush();
    const previous = snapshot();
    const maybePending = consumePending ? fs.readFile(PENDING_RESTORE) : undefined;
    tracking = false;
    try {
      writeSnapshot(files);
      if (consumePending) fs.unlink(PENDING_RESTORE);
      queue.changed();
      await queue.flush();
    } catch (error) {
      writeSnapshot(previous);
      if (maybePending) fs.writeFile(PENDING_RESTORE, maybePending);
      throw error;
    } finally {
      tracking = true;
    }
  }

  // Stage outside the backup allowlist. The live game's cached EEPROM is never
  // replaced: only a fresh runtime applies this file, before native startup.
  async function stageRestore(files: UserFile[]): Promise<void> {
    const text = await createBackup(files, new Date().toISOString());
    await queue.flush();
    if (fs.analyzePath(PENDING_RESTORE).exists) throw new Error('A save import is already pending. Reload to finish it.');
    try {
      fs.writeFile(PENDING_RESTORE, text);
      queue.changed();
      await queue.flush();
    } catch (error) {
      if (fs.analyzePath(PENDING_RESTORE).exists) fs.unlink(PENDING_RESTORE);
      queue.changed();
      await queue.flush().catch(() => {});
      throw error;
    }
  }

  if (fs.analyzePath(PENDING_RESTORE).exists) {
    if (fs.lstat(PENDING_RESTORE).size > MAX_BACKUP_TEXT) throw new Error('Pending save import is too large.');
    const files = await parseBackup(new TextDecoder().decode(fs.readFile(PENDING_RESTORE)));
    // Replacement and marker removal share one IDBFS transaction. Failure
    // restores the prior files/marker and stops startup for a safe retry.
    await replace(files, true);
  }

  return { snapshot, replace: (files: UserFile[]) => replace(files), stageRestore, flush: queue.flush, pending: queue.pending,
    markStarted: () => { started = true; }, stopTracking };
}
export type PersistentFiles = Awaited<ReturnType<typeof mountPersistentFiles>>;
