const LOCK = 'project-midas-ads:user-files';

export class UserFilesInUse extends Error {}

// Held for the lifetime of a running game. A second tab must not restore a
// stale filesystem snapshot and later overwrite another tab's newer saves.
export function acquireUserFilesLock(): Promise<() => void> {
    if (!navigator.locks)
        return Promise.reject(new Error('This browser cannot safely share saves between tabs.'));
    return new Promise((resolve, reject) => {
        let release!: () => void;
        const held = new Promise<void>((finished) => {
            release = finished;
        });
        void navigator.locks
            .request(LOCK, { ifAvailable: true }, (maybeLock) => {
                if (!maybeLock) {
                    reject(
                        new UserFilesInUse(
                            'Another Iron Sights tab owns your saves. Close it before starting here.',
                        ),
                    );
                    return;
                }
                resolve(release);
                return held;
            })
            .catch(reject);
    });
}
