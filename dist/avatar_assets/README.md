# Avatar Assets

This directory holds the thumbnail images used by the Avatar Studio category grids
(hair, eyebrows, nose, mustache, beard).

**These files are not distributed with this repository.** They are copyrighted game
assets and must be supplied by each user from their own legally obtained game dump.

Expected layout (PNG files, ignored by git):

```
avatar_assets/
  hair/      hair_0.png … hair_N.png
  eyebrow/   eyebrow_0.png … eyebrow_N.png
  nose/      nose_0.png … nose_N.png
  mustache/  mustache_0.png … mustache_N.png
  beard/     beard_0.png … beard_N.png
  manifest.json
```

Without these files the category grids simply render without thumbnails; nothing
else is affected. Supply your own assets locally to populate them.
