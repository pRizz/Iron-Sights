export type SaveStatus =
  | { kind: 'saved' | 'pending' | 'saving' }
  | { kind: 'error'; message: string };

// Each sync must be atomic (IDBFS uses one IndexedDB transaction). Writes made
// during that sync require another pass; failure leaves the revision pending.
export function createSyncQueue(sync: () => Promise<void>, report: (status: SaveStatus) => void) {
  let revision = 0;
  let committed = 0;
  let maybeActive: Promise<void> | undefined;

  async function perform(): Promise<void> {
    while (committed < revision) {
      const target = revision;
      report({ kind: 'saving' });
      try {
        await sync();
      } catch (error) {
        report({ kind: 'error', message: error instanceof Error ? error.message : String(error) });
        throw error;
      }
      committed = target;
    }
    report({ kind: 'saved' });
  }

  function flush(): Promise<void> {
    if (maybeActive) return maybeActive;
    maybeActive = perform().finally(() => { maybeActive = undefined; });
    return maybeActive;
  }

  function changed(): void {
    revision++;
    report({ kind: 'pending' });
    queueMicrotask(() => { void flush().catch(() => {}); });
  }

  return { changed, flush, pending: () => committed < revision };
}
