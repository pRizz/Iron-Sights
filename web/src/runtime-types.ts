export type FSNode = { mode: number };
export type FSStream = { node: FSNode; flags: number; path: string };
export type FSStat = { mode: number; size: number; mtime: Date };

// Pinned Emscripten 6.0.13's classic FS, not WasmFS. Keep this boundary small.
export type GameFS = {
  mkdirTree(path: string): void;
  mount(type: unknown, options: object, path: string): void;
  symlink(target: string, path: string): void;
  syncfs(populate: boolean, finished: (maybeError?: unknown) => void): void;
  analyzePath(path: string): { exists: boolean };
  readFile(path: string): Uint8Array<ArrayBuffer>;
  writeFile(path: string, bytes: Uint8Array<ArrayBuffer> | string): void;
  readdir(path: string): string[];
  lstat(path: string): FSStat;
  isDir(mode: number): boolean;
  isFile(mode: number): boolean;
  getPath(node: FSNode): string;
  lookupPath(path: string, options: { follow: boolean }): { node: FSNode };
  utime(path: string, accessed: number, modified: number): void;
  close(stream: FSStream): void;
  rename(from: string, to: string): void;
  unlink(path: string): void;
};

export type GameModule = {
  FS: GameFS;
  IDBFS: unknown;
  ENV: Record<string, string>;
  callMain(args: string[]): void;
  _portWebSetPaused(paused: number): void;
  _portWebControlsLive(): number;
  _portWebCaptureMouse(): number;
  _portWebGetGfxTasks(): number;
};

export type GameOptions = {
  canvas: HTMLCanvasElement;
  noInitialRun: true;
  noExitRuntime: true;
  print(line: string): void;
  printErr(line: string): void;
  onPointerRelease(): void;
  onAbort(reason: unknown): void;
  onExit(code: number): void;
  preRun?(module: GameModule): void;
  locateFile?(name: string): string;
  instantiateWasm?(imports: WebAssembly.Imports, ready: (instance: WebAssembly.Instance) => void): WebAssembly.Exports;
};
export type CreateGame = (options: GameOptions) => Promise<GameModule>;
