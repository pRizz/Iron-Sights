export function installMouseLock(canvas: HTMLCanvasElement, failed: (error: unknown) => void) {
    const maybeOriginal = canvas.requestPointerLock;
    const maybeRequest = maybeOriginal?.bind(canvas);
    let pending = false;
    let active = true;
    let requestMode = 'unavailable';
    let compatibility = false;
    if (maybeRequest) {
        canvas.requestPointerLock = (options) => {
            pending = true;
            const run = async () => {
                try {
                    requestMode = compatibility ? 'compatibility' : 'unadjusted';
                    await maybeRequest(
                        compatibility ? options : { ...options, unadjustedMovement: true },
                    );
                } catch (error) {
                    if (
                        compatibility ||
                        !(error instanceof Error) ||
                        error.name !== 'NotSupportedError'
                    )
                        throw error;
                    compatibility = true;
                    requestMode = 'compatibility';
                    await maybeRequest(options);
                }
            };
            const result = run()
                .catch((error) => {
                    if (active) failed(error);
                    throw error;
                })
                .finally(() => {
                    pending = false;
                });
            // The pinned SDL caller ignores returned promises.
            void result.catch(() => {});
            return result;
        };
    }
    return {
        pending: () => pending,
        mode: () => requestMode,
        dispose() {
            active = false;
            if (maybeOriginal) canvas.requestPointerLock = maybeOriginal;
        },
    };
}
