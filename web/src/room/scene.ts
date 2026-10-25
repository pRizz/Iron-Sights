import * as THREE from 'three';
import { box, canvasTexture, disposeScene, plastic } from './geometry';
import { createCartridge, createConsole, createTelevision } from './props';
import { easeOut, roomDuration, type RoomFlow } from './flow';

export type RoomTarget = 'cartridge' | 'power' | 'controls';

function furnishRoom(scene: THREE.Scene) {
    const grain = canvasTexture(128, 128);
    const ctx = grain.context;
    ctx.fillStyle = '#372d24';
    ctx.fillRect(0, 0, 128, 128);
    for (let index = 0; index < 450; index++) {
        ctx.fillStyle = `rgba(0,0,0,${(index % 7) / 60})`;
        ctx.fillRect((index * 17) % 128, (index * 53) % 128, 1, 35);
    }
    grain.texture.wrapS = grain.texture.wrapT = THREE.RepeatWrapping;
    grain.texture.repeat.set(1, 5);
    const floor = new THREE.MeshStandardMaterial({ map: grain.texture, roughness: 0.48 });
    for (let index = -12; index <= 12; index++)
        box(scene, [0.48, 0.06, 14], [index * 0.49, -0.035, 0], floor, 0.006);
    box(scene, [14, 7, 0.2], [0, 3.4, -3.1], plastic(0x272a27, 0.9));
    box(scene, [14, 0.12, 0.12], [0, 0.08, -2.95], plastic(0x38372f));
    const ambient = new THREE.HemisphereLight(0xa0b7d8, 0x2d1d12, 0.8);
    scene.add(ambient);
    const fill = new THREE.SpotLight(0xffd1a0, 28, 13, Math.PI / 5, 0.8, 2);
    fill.position.set(3.5, 5.5, 4.5);
    fill.target.position.set(0, 0.5, 1);
    fill.castShadow = true;
    fill.shadow.mapSize.set(1024, 1024);
    fill.shadow.bias = -0.001;
    scene.add(fill, fill.target);
}

function paintScreen(
    tv: ReturnType<typeof createTelevision>,
    seconds: number,
    label: string,
    powered: boolean,
    reduced: boolean,
) {
    const ctx = tv.context;
    const flicker = reduced ? 0 : Math.sin(seconds * 27) * 3;
    ctx.fillStyle = `rgb(9,25,${101 + flicker})`;
    ctx.fillRect(0, 0, 640, 480);
    if (!reduced) {
        for (let row = 0; row < 480; row += 4) {
            ctx.fillStyle = 'rgba(0,0,0,0.16)';
            ctx.fillRect(0, row, 640, 1);
        }
        const line = ((seconds * 18) % 560) - 80;
        ctx.fillStyle = '#b5caff0a';
        ctx.fillRect(0, line, 640, 50);
    }
    ctx.fillStyle = '#d0d9ff';
    ctx.font = '23px monospace';
    ctx.textAlign = 'left';
    ctx.fillText('VIDEO 3', 42, 60);
    ctx.textAlign = 'center';
    ctx.font = '26px monospace';
    ctx.fillText(powered ? 'SIGNAL FOUND' : 'NO SIGNAL', 320, 220);
    ctx.font = '17px monospace';
    ctx.fillStyle = '#aebdea';
    ctx.fillText(label.toUpperCase().slice(0, 52), 320, 270);
    ctx.font = '14px monospace';
    ctx.fillText('F2  CONTROLS', 320, 420);
    tv.texture.needsUpdate = true;
}

export function createRoomScene(canvas: HTMLCanvasElement) {
    const renderer = new THREE.WebGLRenderer({
        canvas,
        antialias: true,
        powerPreference: 'low-power',
    });
    renderer.setPixelRatio(Math.min(devicePixelRatio, 1.5));
    renderer.outputColorSpace = THREE.SRGBColorSpace;
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.3;
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x080a0e);
    scene.fog = new THREE.FogExp2(0x080a0e, 0.065);
    const camera = new THREE.PerspectiveCamera(42, 1, 0.05, 35);
    const origin = new THREE.Vector3(2.6, 2.4, 6.3);
    const look = new THREE.Vector3(0, 1.12, 0.1);
    const destination = new THREE.Vector3(-0.06, 2.1, -0.45);
    const tvLook = new THREE.Vector3(-0.06, 2.1, -0.9);
    furnishRoom(scene);
    const consoleProp = createConsole(scene);
    const cartridge = createCartridge(scene);
    const tv = createTelevision(scene);
    // Larger invisible hit surfaces retain the physical switches' visual size.
    const hitMaterial = new THREE.MeshBasicMaterial({ visible: false });
    const powerHit = box(consoleProp.group, [0.4, 0.16, 0.35], [-0.57, 0.35, 0.28], hitMaterial);
    powerHit.name = 'power';
    const controlsHit = box(tv.knob, [0.2, 0.2, 0.2], [0, 0, 0], hitMaterial);
    controlsHit.name = 'controls';
    const raycaster = new THREE.Raycaster();
    const floating = new THREE.Vector3(-0.2, 1.35, 1.65);
    const inserted = new THREE.Vector3(-0.2, 0.66, 0.98);
    let seconds = 0;
    let lastScreen = -1;
    let idleAngle = 0.28;
    let insertionAngle = idleAngle;
    let previousPhase = 'empty';
    let width = 0;
    let height = 0;

    function render(state: RoomFlow, delta: number, reduced: boolean, label: string): void {
        seconds += delta;
        const bounds = canvas.getBoundingClientRect();
        if (bounds.width !== width || bounds.height !== height) {
            width = Math.max(1, bounds.width);
            height = Math.max(1, bounds.height);
            renderer.setSize(width, height, false);
            camera.aspect = width / height;
            camera.updateProjectionMatrix();
        }
        if (state.phase === 'empty') {
            if (!reduced) idleAngle += delta * 0.3;
            cartridge.position.copy(floating);
            cartridge.position.y += reduced ? 0 : Math.sin(seconds * 1.4) * 0.035;
            cartridge.rotation.set(0, idleAngle, 0);
        } else if (state.phase === 'inserting') {
            if (previousPhase !== 'inserting')
                insertionAngle = Math.atan2(Math.sin(idleAngle), Math.cos(idleAngle));
            const progress = easeOut(state.elapsed / roomDuration('inserting', reduced));
            cartridge.position.lerpVectors(floating, inserted, progress);
            cartridge.rotation.set(0, THREE.MathUtils.lerp(insertionAngle, 0, progress), 0);
        } else {
            cartridge.position.copy(inserted);
            cartridge.rotation.set(0, 0, 0);
        }
        const powered = ['powering', 'flying', 'complete'].includes(state.phase);
        const powerProgress =
            state.phase === 'powering'
                ? easeOut(state.elapsed / roomDuration('powering', reduced))
                : powered
                  ? 1
                  : 0;
        consoleProp.power.position.z = 0.28 - powerProgress * 0.07;
        consoleProp.ledMaterial.emissiveIntensity = powered ? 2 : 0;
        tv.glow.intensity = 19 + (reduced ? 0 : Math.sin(seconds * 27) * 0.5);
        const flight =
            state.phase === 'flying'
                ? easeOut(state.elapsed / roomDuration('flying', reduced))
                : state.phase === 'complete'
                  ? 1
                  : 0;
        camera.position.lerpVectors(origin, destination, reduced ? 0 : flight);
        camera.lookAt(new THREE.Vector3().lerpVectors(look, tvLook, reduced ? 0 : flight));
        if (seconds - lastScreen > 1 / 15 || previousPhase !== state.phase) {
            paintScreen(tv, seconds, label, powered, reduced);
            lastScreen = seconds;
        }
        renderer.render(scene, camera);
        previousPhase = state.phase;
    }

    function hit(clientX: number, clientY: number): RoomTarget | undefined {
        const rect = canvas.getBoundingClientRect();
        raycaster.setFromCamera(
            new THREE.Vector2(
                ((clientX - rect.left) / rect.width) * 2 - 1,
                -((clientY - rect.top) / rect.height) * 2 + 1,
            ),
            camera,
        );
        const hits = raycaster.intersectObjects([cartridge, powerHit, controlsHit], true);
        const maybeHit = hits[0]?.object;
        if (!maybeHit) return undefined;
        if (maybeHit === powerHit) return 'power';
        if (maybeHit === controlsHit) return 'controls';
        return 'cartridge';
    }

    return {
        render,
        hit,
        hover(target: RoomTarget | undefined) {
            for (const [object, active] of [
                [cartridge, target === 'cartridge'],
                [consoleProp.power, target === 'power'],
                [tv.knob, target === 'controls'],
            ] as const) {
                object.traverse((part) => {
                    if (!(part instanceof THREE.Mesh)) return;
                    const material = part.material;
                    if (material instanceof THREE.MeshStandardMaterial) {
                        material.emissive.setHex(active ? 0xc29a4f : 0);
                        material.emissiveIntensity = active ? 0.15 : 0;
                    }
                });
            }
        },
        dispose() {
            disposeScene(scene);
            renderer.dispose();
            renderer.forceContextLoss();
        },
    };
}
