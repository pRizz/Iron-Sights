export function formatBuildTime(
    value: string,
    maybeLocale?: string,
    maybeTimeZone?: string,
): string {
    if (!value) return 'Unavailable';
    const date = new Date(value);
    if (!Number.isFinite(date.getTime())) return value;
    return new Intl.DateTimeFormat(maybeLocale, {
        year: 'numeric',
        month: 'short',
        day: 'numeric',
        hour: 'numeric',
        minute: '2-digit',
        timeZoneName: 'short',
        timeZone: maybeTimeZone,
    }).format(date);
}
