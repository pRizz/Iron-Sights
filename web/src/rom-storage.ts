import type { ValidatedRom } from './rom';

const DATABASE = 'project-midas-ads:rom';
const STORE = 'rom';
const KEY = 'original';

async function openRomDatabase(): Promise<IDBDatabase> {
  return new Promise((resolve, reject) => {
    let abandoned = false;
    const request = indexedDB.open(DATABASE, 1);
    request.onupgradeneeded = () => request.result.createObjectStore(STORE);
    request.onerror = () => reject(request.error);
    request.onblocked = () => {
      abandoned = true;
      reject(new Error('Close other Project Midas tabs before updating ROM storage.'));
    };
    request.onsuccess = () => {
      const database = request.result;
      if (abandoned) { database.close(); return; }
      database.onversionchange = () => database.close();
      resolve(database);
    };
  });
}

async function romTransaction(mode: IDBTransactionMode, operation: (store: IDBObjectStore) => IDBRequest): Promise<unknown> {
  const database = await openRomDatabase();
  try {
    return await new Promise((resolve, reject) => {
      const transaction = database.transaction(STORE, mode);
      const request = operation(transaction.objectStore(STORE));
      transaction.oncomplete = () => resolve(request.result);
      transaction.onabort = () => reject(transaction.error || new Error('ROM storage transaction was aborted.'));
      transaction.onerror = () => {}; // The abort event is the authoritative failure.
    });
  } finally {
    database.close();
  }
}

export async function rememberRom(rom: ValidatedRom): Promise<void> {
  // Blob snapshots the original byte order before the game can mutate a copy.
  const value = { blob: new Blob([rom.bytes]), name: rom.name };
  try {
    await romTransaction('readwrite', store => store.put(value, KEY));
  } catch (error) {
    // Some WebKit backends cannot prepare Blob data for IndexedDB. Preserve
    // the same original snapshot as bytes; quota/permission failures still fail.
    if (!error || typeof error !== 'object' || !('name' in error) ||
        !['DataCloneError', 'UnknownError', 'NotSupportedError'].includes(String(error.name))) throw error;
    const bytes = new Uint8Array(await value.blob.arrayBuffer());
    await romTransaction('readwrite', store => store.put({ bytes, name: rom.name }, KEY));
  }
}

export async function recalledRom(): Promise<(Blob & { name: string }) | undefined> {
  const maybeValue = await romTransaction('readonly', store => store.get(KEY));
  if (maybeValue === undefined) return undefined;
  if (!maybeValue || typeof maybeValue !== 'object' || !('name' in maybeValue) || typeof maybeValue.name !== 'string') {
    throw new Error('The remembered ROM is damaged. Forget it and choose your original ROM again.');
  }
  const blob = 'blob' in maybeValue && maybeValue.blob instanceof Blob ? maybeValue.blob
    : 'bytes' in maybeValue && maybeValue.bytes instanceof Uint8Array ? new Blob([new Uint8Array(maybeValue.bytes)]) : undefined;
  if (!blob) throw new Error('The remembered ROM is damaged. Forget it and choose your original ROM again.');
  return Object.assign(blob, { name: maybeValue.name });
}

export async function forgetRom(): Promise<void> {
  await romTransaction('readwrite', store => store.delete(KEY));
}
