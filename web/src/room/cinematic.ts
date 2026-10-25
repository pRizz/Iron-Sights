export type Point3 = [number, number, number];
const clamp = (value: number) => Math.max(0, Math.min(1, value));
export const smooth = (value: number) => {
    const t = clamp(value);
    return t * t * t * (t * (t * 6 - 15) + 10);
};
const blend = (start: Point3, end: Point3, t: number): Point3 =>
    start.map((value, index) => value + (end[index] - value) * t) as Point3;

export function insertionPose(progress: number, yaw: number) {
    const t = clamp(progress);
    const floating: Point3 = [-0.2, 1.35, 1.65];
    const aligned: Point3 = [-0.2, 1.22, 0.98];
    const position =
        t < 0.55
            ? blend(floating, aligned, smooth(t / 0.55))
            : blend(aligned, [-0.2, 0.66, 0.98], 1 - (1 - (t - 0.55) / 0.45) ** 3);
    return { position, yaw: yaw * (1 - smooth(t / 0.55)) };
}

export function cameraPose(progress: number, aspect: number) {
    const t = smooth(progress);
    const widen = Math.max(0, 1 - Math.max(0.5, aspect));
    const start: Point3 = [2.6, 2.4 + widen * 0.5, 6.3 + widen * 3];
    const middle: Point3 = [0.45, 2.35, 2.8];
    const end: Point3 = [-0.06, 2.1, -0.45];
    const position = start.map(
        (value, index) =>
            (1 - t) ** 2 * value + 2 * (1 - t) * t * middle[index] + t * t * end[index],
    ) as Point3;
    return { position, look: blend([0, 1.12, 0.1], [-0.06, 2.1, -0.9], t) };
}
