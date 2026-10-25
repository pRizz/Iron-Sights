import * as THREE from 'three';

// Split whole triangles while preserving normalized integer attributes (colors).
export function partitionGeometry(geometry, moving) {
    const source = geometry.toNonIndexed();
    const groups = [[], []];
    const positions = source.attributes.position;
    for (let index = 0; index < positions.count; index += 3) {
        const selected = [0, 1, 2].every((offset) =>
            moving(new THREE.Vector3().fromBufferAttribute(positions, index + offset)),
        );
        groups[Number(selected)].push(index, index + 1, index + 2);
    }
    const result = groups.map((indices) => {
        const target = new THREE.BufferGeometry();
        for (const [name, attribute] of Object.entries(source.attributes)) {
            const values = indices.flatMap((index) =>
                Array.from(
                    attribute.array.slice(
                        index * attribute.itemSize,
                        (index + 1) * attribute.itemSize,
                    ),
                ),
            );
            const ArrayType = attribute.array.constructor;
            target.setAttribute(
                name,
                new THREE.BufferAttribute(
                    new ArrayType(values),
                    attribute.itemSize,
                    attribute.normalized,
                ),
            );
        }
        return target;
    });
    source.dispose();
    return result;
}
