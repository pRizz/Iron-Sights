import type { Config } from 'tailwindcss';
import { withMysticUI } from 'mystic-ui/tailwind/setup';

export default withMysticUI({ content: ['./index.html', './src/**/*.{ts,tsx}'] }) satisfies Config;
