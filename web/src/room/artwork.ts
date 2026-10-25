import * as THREE from 'three';

export function loadCartridgeArtwork(scene: THREE.Scene) {
    const texture = new THREE.Texture();
    const loader = new THREE.ImageLoader();
    const load = (format: 'avif' | 'png') =>
        loader.load(
            import.meta.env.BASE_URL + 'room/cartridge-art.' + format,
            (image) => {
                texture.image = image;
                texture.needsUpdate = true;
                scene.userData.artworkFormat = format;
            },
            undefined,
            () => {
                if (format === 'avif') load('png');
                else scene.userData.assetError = 'The cartridge image could not be loaded.';
            },
        );
    load('avif');
    texture.colorSpace = THREE.SRGBColorSpace;
    texture.anisotropy = 4;
    // Identical UVs for the unchanged source photo and its lossless PNG fallback.
    texture.offset.set(0.272, 0.205);
    texture.repeat.set(0.47, 0.525);
    return texture;
}
