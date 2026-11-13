// Meow Box: what the controllers show and the CC each control sends (channel 1).
// src/app.cpp binds the same numbers in kBind.
export const profile = {
  name: 'Meow Box',
  accent: '#ce5b32',
  pages: [
    {key: 'P', label: 'PLAY', title: 'Play the cat', color: '#E59866', controls: [
      {id: 'roar', cc: 30, kind: 'trigger', label: 'ROAR', key: 'A'},
      {id: 'mew', cc: 31, kind: 'trigger', label: 'MEW', key: 'S'},
      {id: 'hush', cc: 32, kind: 'trigger', label: 'HUSH', key: 'D'},
      {id: 'space', kind: 'xy', label: 'Size × Meow mix', x: 'size', y: 'mix'},
    ]},
    {key: 'C', label: 'CAT', title: 'The cat', color: '#F5A524', controls: [
      {id: 'size', cc: 20, kind: 'range', label: 'Size', min: 0, max: 100, def: 45,
        format: v => `${v < 34 ? 'Kitten' : v < 68 ? 'Cat' : 'Lion'} · ${v}`},
      {id: 'length', cc: 21, kind: 'choice', label: 'Meow length', options: ['MEW', 'MEOW', 'MEEOOOW'], def: 1},
      {id: 'bend', cc: 23, kind: 'range', label: 'Bend', min: 0, max: 100, unit: '%', def: 60},
    ]},
    {key: 'V', label: 'VOICE', title: 'The voice', color: '#6FA0B8', controls: [
      {id: 'mix', cc: 22, kind: 'range', label: 'Meow mix', min: 0, max: 100, unit: '%', def: 80},
      {id: 'purr', cc: 24, kind: 'toggle', label: 'Purr', def: 1},
      {id: 'purrAmt', cc: 25, kind: 'range', label: 'Purr amount', min: 0, max: 100, unit: '%', def: 45},
      {id: 'level', cc: 26, kind: 'range', label: 'Level', min: -12, max: 6, unit: 'dB', def: 0},
    ]},
  ],
};
