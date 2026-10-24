import { describe, expect, test } from 'bun:test';
import { createSyncQueue, type SaveStatus } from './sync-queue';

function deferred() {
  let resolve!: () => void;
  let reject!: (error: Error) => void;
  const promise = new Promise<void>((yes, no) => { resolve = yes; reject = no; });
  return { promise, resolve, reject };
}

describe('save synchronization', () => {
  test('serializes writes made during an active transaction', async () => {
    const first = deferred();
    const second = deferred();
    const passes = [first, second];
    let calls = 0;
    const statuses: SaveStatus[] = [];
    const queue = createSyncQueue(() => passes[calls++].promise, status => statuses.push(status));

    queue.changed();
    const finished = queue.flush();
    queue.changed();
    expect(calls).toBe(1);
    first.resolve();
    await Promise.resolve();
    expect(calls).toBe(2);
    expect(queue.pending()).toBe(true);
    second.resolve();
    await finished;

    expect(calls).toBe(2);
    expect(queue.pending()).toBe(false);
    expect(statuses.at(-1)).toEqual({ kind: 'saved' });
  });

  test('retains dirty data and exposes failure until a retry commits', async () => {
    const transaction = deferred();
    let calls = 0;
    const statuses: SaveStatus[] = [];
    const queue = createSyncQueue(() => ++calls === 1 ? transaction.promise : Promise.resolve(),
      status => statuses.push(status));

    queue.changed();
    const failed = queue.flush();
    transaction.reject(new Error('Quota exceeded'));
    await expect(failed).rejects.toThrow('Quota exceeded');

    expect(queue.pending()).toBe(true);
    expect(statuses.at(-1)).toEqual({ kind: 'error', message: 'Quota exceeded' });
    await queue.flush();
    expect(calls).toBe(2);
    expect(queue.pending()).toBe(false);
  });

  test('coalesces a synchronous burst without overlapping transactions', async () => {
    let calls = 0;
    const queue = createSyncQueue(async () => { calls++; }, () => {});

    queue.changed();
    queue.changed();
    queue.changed();
    await queue.flush();

    expect(calls).toBe(1);
    expect(queue.pending()).toBe(false);
  });
});
