export function backupFilename(date: Date): string {
    const pad = (part: number) => String(part).padStart(2, '0');
    const day = `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`;
    const time = `${pad(date.getHours())}-${pad(date.getMinutes())}-${pad(date.getSeconds())}`;
    return `iron-sights-saves-${day}T${time}.json`;
}
