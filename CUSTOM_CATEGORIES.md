# Custom categories

This fork adds data-driven categories to the native picker and web catalog.
Install the updated app once; later packs need only local files. It does not
install game character mods. The game must separately support each supplied tag.

Create a folder next to the executable:

```text
Categories/
  ExamplePack/
    category.ini
    logo.png
    Example Hero.bin
    Example Hero.png
```

`category.ini`:

```ini
[Category]
Schema=1
Name=Example Universe
```

The category name comes from the manifest; each character name comes from its
BIN filename. Supply your own 180-byte tag. An optional same-name PNG/JPG/JPEG
provides the character portrait. `logo.png` supplies category artwork. Images
retain their aspect ratio and transparency. Packs sharing a category name extend
one roster; duplicate character labels do not replace existing entries.
Stock category artwork stays intact.

Metadata accepts UTF-8, UTF-8 with BOM, and UTF-16 with BOM. Names are limited to
64 characters. Invalid/duplicate metadata keys, unsupported schemas, redirected
files, images over 8 MiB or 2048 pixels, and tag files with the wrong size are
ignored. Scanning is bounded to 64 folders per root, 512 entries per folder and
2048 custom tags overall. Packs are local data, with no executable plugins or
remote downloads.

The existing `Special/<folder>/` custom tags remain supported. Creator-managed
folders named `cc_<32 lowercase hex characters>` can use `creator.ini`:

```ini
[Creator]
Schema=1
Uuid=0123456789abcdef0123456789abcdef
Name=Example Hero
Category=Example Universe
```

Optional `portrait.png` and `category.png` provide managed artwork. A separate
category folder with only metadata and `logo.png` can also brand a category.

Press **F5** to refresh while browsing. The app also checks for changes every
two seconds when the picker is hidden or on the pad view, avoiding a roster
rebuild during selection/reordering. Existing pad values stay in place and no
load/remove command is sent merely because a category refreshed.

For inspection without starting the picker, controller input, portal socket or
single-instance lock:

```text
LegoToypad.exe --workshop-catalog <new-output.json>
```

This writes the actual native/web catalog and does not change app settings.
Tests create their own temporary executable copy and synthetic local fixtures:

```text
set TOYPAD_TEST_EXE=<absolute-built-executable>
python tests/test_custom_categories.py
```

AI-assisted extension and tests. No custom character assets or game mods are
provided by this change. The upstream MIT license and attribution are retained.
