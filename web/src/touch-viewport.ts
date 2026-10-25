// Mobile browser chrome and rotation can change the visible viewport before
// layout viewport units catch up. Keep touch targets inside the visible area.
export function observeTouchViewport(stage: HTMLElement): () => void {
    const viewport = window.visualViewport;
    const update = () => {
        // Either viewport can lag during rotation; never place targets beyond
        // either the layout bounds or the visible browser area.
        const width = Math.min(innerWidth, viewport?.width || innerWidth);
        const height = Math.min(innerHeight, viewport?.height || innerHeight);
        stage.style.setProperty('--touch-view-width', `${width}px`);
        stage.style.setProperty('--touch-view-height', `${height}px`);
    };
    window.addEventListener('resize', update);
    viewport?.addEventListener('resize', update);
    update();
    return () => {
        window.removeEventListener('resize', update);
        viewport?.removeEventListener('resize', update);
        stage.style.removeProperty('--touch-view-width');
        stage.style.removeProperty('--touch-view-height');
    };
}
