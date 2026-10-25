// Fit the actual framebuffer without changing its resolution or mouse coordinates.
export function containSize(width: number, height: number, aspect: number) {
    const fittedWidth = Math.min(width, height * aspect);
    return { width: fittedWidth, height: fittedWidth / aspect };
}

export function observeGameSize(canvas: HTMLCanvasElement, stage: HTMLElement) {
    let frame = 0;
    const fit = () => {
        frame = 0;
        const size = containSize(
            stage.clientWidth,
            stage.clientHeight,
            canvas.width / canvas.height,
        );
        const width = `${size.width}px`;
        const height = `${size.height}px`;
        if (canvas.style.width !== width) canvas.style.width = width;
        if (canvas.style.height !== height) canvas.style.height = height;
    };
    const schedule = () => {
        if (!frame) frame = requestAnimationFrame(fit);
    };
    const resize = new ResizeObserver(schedule);
    const buffer = new MutationObserver(schedule);
    resize.observe(stage);
    buffer.observe(canvas, { attributes: true, attributeFilter: ['width', 'height'] });
    fit();
    return () => {
        resize.disconnect();
        buffer.disconnect();
        cancelAnimationFrame(frame);
    };
}
