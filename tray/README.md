# Tray animation artist data

The page groups collections by the `author` field in `sections.json`. Collections
with the same author are rendered inside the same expandable artist row.

```json
{
  "count": 24,
  "title": "collection-name",
  "author": "Artist name",
  "authorBio": "Short artist introduction or collaboration note.",
  "authorAvatar": "/assets/artists/example.webp",
  "authorUrl": "https://artist.example.com/",
  "authorTag": "动画作者",
  "rating": 5.0,
  "reviewCount": 85,
  "description": "Optional collection description.",
  "cdnBase": "https://cdn.example.com/collection-name/"
}
```

For compatibility with older data, `creator` and `contributor` are accepted as
aliases of `author`; `bio` and `avatar` are accepted as profile aliases.

Production builds can load the registry from tray-hub by setting:

```bash
VITE_TRAY_HUB_URL=https://<worker-domain>/sections.json pnpm build
```

When unset, both production and local development use
`https://tray.cati.me/sections.json`. A collection may provide a `files` array
for arbitrary GIF filenames; the legacy
`0001_<collection>.gif` convention remains supported when `files` is absent.
