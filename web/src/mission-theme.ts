// Shared watch/HUD palette for DOM startup panels and the introductory CRT.
export const MISSION_THEME = {
    background: '#0c151a',
    panel: '#19272c',
    edge: '#5f7376',
    text: '#e0e6dc',
    muted: '#a5b6af',
    gold: '#d5c48e',
    green: '#b9ed9f',
    red: '#e39c8d',
    font: '"Bank Gothic", "Eurostile", "Arial Narrow", Arial, sans-serif',
} as const;

export const missionStyle = {
    '--mission-background': MISSION_THEME.background,
    '--mission-panel': MISSION_THEME.panel,
    '--mission-edge': MISSION_THEME.edge,
    '--mission-text': MISSION_THEME.text,
    '--mission-muted': MISSION_THEME.muted,
    '--mission-gold': MISSION_THEME.gold,
    '--mission-green': MISSION_THEME.green,
    '--mission-red': MISSION_THEME.red,
    '--mission-font': MISSION_THEME.font,
};
