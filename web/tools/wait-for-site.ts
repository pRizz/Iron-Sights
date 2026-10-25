export {};
const url = process.argv[2];
if (!url) throw new Error('Pass the local preview URL');
let ready = false;
for (let attempt = 0; attempt < 40; attempt++) {
    try {
        ready = (await fetch(url)).ok;
    } catch {
        // The preview process may still be binding its local port.
    }
    if (ready) break;
    await Bun.sleep(250);
}
if (!ready) throw new Error('The local preview did not start: ' + url);
