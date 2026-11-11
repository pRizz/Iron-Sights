// Fit the actual framebuffer without changing its resolution or mouse coordinates.
export function containSize(width: number, height: number, aspect: number) {
    const fittedWidth = Math.min(width, height * aspect);
    return { width: fittedWidth, height: fittedWidth / aspect };
}

// The renderer widens the view for shapes wider than the N64's 4:3 but
// stretches anything taller (gfx_pc.cpp), so a portrait stage gets 4:3.
const NARROWEST_ASPECT = 4 / 3;

// The framebuffer that fills a stage of this CSS size at this pixel scale.
export function stageFramebuffer(width: number, height: number, scale: number) {
    const ratio = Number.isFinite(scale) && scale > 0 ? scale : 1;
    const shownHeight = Math.min(height, width / NARROWEST_ASPECT);
    return {
        width: Math.max(1, Math.round(width * ratio)),
        height: Math.max(1, Math.round(shownHeight * ratio)),
    };
}

// The running game, which can render at a new size. `resize` returns false
// when the game cannot take it yet (no window, or the icon stage is running).
export type FramebufferOwner = {
    scale: number;
    resize: (width: number, height: number) => boolean;
};

// Rotation and window drags send bursts of sizes; render once they settle.
const FOLLOW_DELAY_MS = 150;
const RETRY_DELAY_MS = 250;

export function observeGameSize(
    canvas: HTMLCanvasElement,
    stage: HTMLElement,
    maybeOwner: () => FramebufferOwner | undefined = () => undefined,
) {
    let frame = 0;
    let follow = 0;
    const requestFramebuffer = () => {
        follow = 0;
        const owner = maybeOwner();
        if (!owner || stage.clientWidth < 1 || stage.clientHeight < 1) return;
        const size = stageFramebuffer(stage.clientWidth, stage.clientHeight, owner.scale);
        if (canvas.width === size.width && canvas.height === size.height) return;
        if (!owner.resize(size.width, size.height))
            follow = window.setTimeout(requestFramebuffer, RETRY_DELAY_MS);
    };
    const fit = () => {
        frame = 0;
        // Until the framebuffer follows, show the current one uncropped and unstretched.
        const size = containSize(
            stage.clientWidth,
            stage.clientHeight,
            canvas.width / canvas.height,
        );
        const width = `${size.width}px`;
        const height = `${size.height}px`;
        if (canvas.style.width !== width) canvas.style.width = width;
        if (canvas.style.height !== height) canvas.style.height = height;
        window.clearTimeout(follow);
        follow = window.setTimeout(requestFramebuffer, FOLLOW_DELAY_MS);
    };
    const schedule = () => {
        if (!frame) frame = requestAnimationFrame(fit);
    };
    const resize = new ResizeObserver(schedule);
    const buffer = new MutationObserver(schedule);
    resize.observe(stage);
    buffer.observe(canvas, { attributes: true, attributeFilter: ['width', 'height'] });
    fit();
    return {
        // Ask again, for example once the game starts and can take a size.
        refit: schedule,
        stop: () => {
            resize.disconnect();
            buffer.disconnect();
            cancelAnimationFrame(frame);
            window.clearTimeout(follow);
        },
    };
}
