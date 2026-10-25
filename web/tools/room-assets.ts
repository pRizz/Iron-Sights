// Explicit additions to the public artifact allowlist. Never glob user images.
export const ROOM_ASSETS = [
    {
        path: 'room/cartridge-art.avif',
        bytes: 68421,
        sha256: '23bc1f312fe2964ceb995539d6d118efceef341bb0fd01750e8a68d3069a9dca',
    },
    {
        path: 'room/cartridge-art.png',
        bytes: 588603,
        sha256: '5325d67f6436c6e350c76d436b6de1f976a2209077e97f31c13022f695688ca9',
    },
] as const;
