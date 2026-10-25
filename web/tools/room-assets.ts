// Explicit additions to the public artifact allowlist. Never glob user images.
export const ROOM_ASSETS = [
    {
        path: 'room/cartridge-art.avif',
        bytes: 68421,
        sha256: '23bc1f312fe2964ceb995539d6d118efceef341bb0fd01750e8a68d3069a9dca',
    },
] as const;
