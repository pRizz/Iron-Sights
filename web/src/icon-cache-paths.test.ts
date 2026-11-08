import { expect, test } from 'bun:test';

import { iconCachePath } from './icon-cache-paths';

test('the prepared weapon icon cache persists', () => {
  expect(iconCachePath('Data/Cache/weapon-icons-v9-b6330846.cache')).toBe(true);
  expect(iconCachePath('Data/Cache/weapon-icons-v10-0000abcd.cache')).toBe(true);
});

test('its temporary file, other names and other folders do not', () => {
  for (const path of [
    'Data/Cache/weapon-icons-v9-b6330846.cache.tmp',
    'Data/Cache/weapon-icons-v9-B6330846.cache',
    'Data/Cache/other.cache',
    'Data/Saves/weapon-icons-v9-b6330846.cache',
    '../Data/Cache/weapon-icons-v9-b6330846.cache',
  ])
    expect(iconCachePath(path)).toBe(false);
});
