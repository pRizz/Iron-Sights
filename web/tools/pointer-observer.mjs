// QA-only trace of browser calls and events, including legacy void-return APIs.
export function observePointer() {
  globalThis.midasPointerTrace = [];
  const trace = (event, extra = {}) => midasPointerTrace.push({ event, at: performance.now(),
    locked: !!document.pointerLockElement, focus: document.hasFocus(), hidden: document.hidden,
    frames: globalThis.midasPrototype?._portWebGetFrames(), ...extra });
  for (const [prototype, name] of [[Document.prototype, 'exitPointerLock'], [Element.prototype, 'requestPointerLock']]) {
    const maybeOriginal = prototype[name];
    if (!maybeOriginal) continue;
    prototype[name] = function (...args) {
      trace(name, { stack: new Error().stack });
      const result = Reflect.apply(maybeOriginal, this, args);
      result?.then(() => trace(name + ':resolved'), error => trace(name + ':rejected', { error: String(error) }));
      return result;
    };
  }
  for (const event of ['pointerlockchange', 'pointerlockerror', 'visibilitychange']) {
    document.addEventListener(event, () => trace(event), true);
  }
}
