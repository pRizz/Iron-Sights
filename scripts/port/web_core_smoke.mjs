// The ROM is copied only into the ephemeral virtual filesystem of this process.
import { readFile } from 'node:fs/promises';
import { pathToFileURL } from 'node:url';

const [modulePath, romPath, frameText] = process.argv.slice(2);
const frames = Number(frameText);
if (!modulePath || !romPath || !Number.isInteger(frames) || frames < 1) {
  throw new Error('Usage: web_core_smoke.mjs module.mjs local-rom frames');
}
const { default: createMidas } = await import(pathToFileURL(modulePath).href);
let reachedLimit = false;
let maybeStage = null;
let damLoaded = false;
let maybeResolveExit;
const exited = new Promise(resolve => { maybeResolveExit = resolve; });
const game = await createMidas({
  noInitialRun: true,
  noExitRuntime: true,
  preRun(module) {
    Object.assign(module.ENV, {
      GE_FRAMES: String(frames), GE_NOSLEEP: '1', GE_STAGE: 'dam',
      GE_STAGE_AFTER: '1', GE_SKIPINTRO: '1', GE_SEED: '1',
    });
  },
  print(line) {
    console.log(line);
    if (line.startsWith('{"web_frame_limit":')) {
      const report = JSON.parse(line);
      maybeStage = report.stage;
      damLoaded = report.mission_is_dam;
    }
    if (line.includes('ge: frame limit reached:')) {
      reachedLimit = true;
      maybeResolveExit(0);
    }
  },
  printErr: console.error,
  onExit(status) {
    if (!reachedLimit) console.error(new Error('Unexpected runtime exit').stack);
    maybeResolveExit(status);
  },
});
game.FS.writeFile('/rom.z64', new Uint8Array(await readFile(romPath)));
game.callMain(['--rom-file', '/rom.z64']);
const status = await exited;
console.log(JSON.stringify({ smoke_exit: status, reached_frame_limit: reachedLimit, stage: maybeStage }));
if (status || !reachedLimit || !damLoaded) throw new Error('Core smoke did not reach the frame limit in Dam');
