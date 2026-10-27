// Explicit additions to the public artifact allowlist. Never glob user images.
export const ROOM_ASSETS = [
    {
        path: 'room/console.glb',
        bytes: 203896,
        sha256: '26ea8b9dae78d1d377923bd87b0034633e0e78c0f4303217bf29ad53f25a7c78',
    },
    {
        path: 'room/controller.glb',
        bytes: 1204344,
        sha256: 'd707d6a1b67218dd5239984ad95e716a769b03359655fc255a16196c6a5fb58d',
    },
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
