export const ROOM_FOV = 42;
export const TV_SCREEN = {
    width: 2.35,
    height: 1.7,
    center: [-0.06, 2.1, -0.95] as [number, number, number],
};

export function screenDistance(aspect: number) {
    const tangent = Math.tan((ROOM_FOV * Math.PI) / 360);
    return Math.max(TV_SCREEN.height / 2, TV_SCREEN.width / (2 * aspect)) / tangent;
}
