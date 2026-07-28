Tray animation artist data comes directly from tray-hub. The website does not
keep a second author list or bundled manifest, so adding an eligible public
author repository does not require a Catime website change.

Production builds can load the registry from tray-hub by setting:

```bash
VITE_TRAY_HUB_URL=https://<worker-domain>/sections.json pnpm build
```

When unset, both production and local development use
`https://tray.cati.me/sections.json`. The repository name is the displayed
author name, `authorAvatar` points to its root `a.*` image, `authorLinks` comes
from the repository README, and `files` is the complete animation list.

The author repository README needs no heading. Put one standalone URL on each
line; tray-hub labels Bilibili, Pixiv, and X links automatically.
