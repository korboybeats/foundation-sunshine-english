# Image Path Migration Guide

## Overview

This migration script automatically moves existing local-file-path images into the new `covers/` directory and updates the references in `apps.json`.

## Migration Logic

### 1️⃣ Identify image paths that need to be migrated

The script inspects the `image-path` field of every app in `apps.json`:

**Skipped (no migration needed):**
- `desktop` — uses the desktop image
- `app_name_123.png` — already in the boxart format (filename only, no path separator)

**Needs migration:**
- `C:\Users\...\picture.png` — absolute path
- `./images/game.jpg` — relative path
- `covers/steam.png` — old `covers` path (handled on line 58)

### 2️⃣ Locate the source file

The script looks for the image file in this order:

1. **Absolute path** — check whether the path exists directly
2. **Relative to the config directory** — `config\<image-path>`
3. **Relative to the legacy Sunshine root** — `<parent>\<image-path>`

### 3️⃣ Copy into the covers directory

Once a source file is located:

```
Source: C:\Users\mohaha\Pictures\game.png
App name: Steam
↓
New filename: app_Steam_1234567890.png
↓
Copied to: config\covers\app_Steam_1234567890.png
```

### 4️⃣ Update apps.json

```json
// Before migration
{
  "name": "Steam",
  "image-path": "C:\\Users\\mohaha\\Pictures\\game.png"
}

// After migration
{
  "name": "Steam",
  "image-path": "app_Steam_1234567890.png"
}
```

## Workflow

```
┌─────────────────────────────────────────────────────────────┐
│ 1. migrate-config.bat runs                                   │
│    - Migrates the covers/ directory                          │
│    - Updates legacy ./covers/ paths to ./config/covers/      │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. Calls migrate-images.ps1                                  │
│    - Reads apps.json                                         │
│    - Locates the image file for each app                     │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. For each image file:                                      │
│    ✅ Copies it to config/covers/app_<name>_<timestamp>.png  │
│    ✅ Updates the image-path in apps.json                    │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│ 4. The Sunshine service starts                               │
│    ✅ getBoxArt loads images from the covers/ directory      │
│    ✅ Web UI accesses them via /boxart/<filename>            │
└─────────────────────────────────────────────────────────────┘
```

## Example Scenarios

### Scenario 1: absolute path

**Before:**
```json
{
  "apps": [
    {
      "name": "Game A",
      "image-path": "C:\\Users\\mohaha\\Pictures\\game1.jpg"
    }
  ]
}
```

**After:**
```json
{
  "apps": [
    {
      "name": "Game A",
      "image-path": "app_Game_A_1234567890.jpg"
    }
  ]
}
```

**File locations:**
- Source: `C:\Users\mohaha\Pictures\game1.jpg`
- Destination: `config\covers\app_Game_A_1234567890.jpg`

### Scenario 2: relative path

**Before:**
```json
{
  "apps": [
    {
      "name": "Steam",
      "image-path": "./assets/steam-icon.png"
    }
  ]
}
```

**After:**
```json
{
  "apps": [
    {
      "name": "Steam",
      "image-path": "app_Steam_1234567891.png"
    }
  ]
}
```

**Lookup order:**
1. ❌ `./assets/steam-icon.png` (as an absolute path)
2. ✅ `config\assets\steam-icon.png` (relative to config)
3. (found, stop searching)

### Scenario 3: already in the new format (skipped)

**apps.json:**
```json
{
  "apps": [
    {
      "name": "Desktop",
      "image-path": "desktop"
    },
    {
      "name": "Chrome",
      "image-path": "app_Chrome_1234567892.png"
    }
  ]
}
```

**Result:** both apps are skipped — no migration is performed.

## Error Handling

### File not found
```
⚠️  Image file not found for Game B: C:\old\path\missing.png
```
- **Behavior**: keep the original path; do not modify apps.json
- **Reason**: it may be a network path or become available later

### Copy failed
```
❌ Failed to copy image for Game C: Access denied
```
- **Behavior**: keep the original path; continue with the other apps
- **Reason**: insufficient permissions or disk space

## Compatibility

### Working with the new getBoxArt

```cpp
// src/confighttp.cpp - getBoxArt function
std::string imagePath = SUNSHINE_ASSETS_DIR "boxart/" + path;

// If not found in boxart/, try covers/
if (!fs::exists(imagePath)) {
  std::string coversPath = platf::appdata().string() + "/covers/" + path;
  if (fs::exists(coversPath)) {
    imagePath = coversPath;  // ✅ migrated images live here
  }
}
```

### Working with the Web UI

```javascript
// getImagePreviewUrl function
if (imagePath.includes('/') || imagePath.includes('\\')) {
  return imagePath;  // legacy path format (pre-migration)
} else {
  return `/boxart/${imagePath}`;  // ✅ new format (post-migration)
}
```

## Testing the Migration

### Manual test

1. Create a test apps.json:
```json
{
  "apps": [
    {
      "name": "Test",
      "image-path": "C:\\test\\image.png"
    }
  ]
}
```

2. Run the migration:
```cmd
powershell -ExecutionPolicy Bypass -File migrate-images.ps1 "C:\path\to\config"
```

3. Verify the result:
- ✅ The file has been copied to `config\covers\app_Test_*.png`
- ✅ apps.json has been updated to the new path
- ✅ The Sunshine Web UI displays the image correctly

## Sample Log Output

```
Reading apps.json from: C:\Sunshine\config\apps.json
Created covers directory: C:\Sunshine\config\covers
Found image file: C:\Users\mohaha\Pictures\game.png
✅ Migrated: Steam
   From: C:\Users\mohaha\Pictures\game.png
   To:   C:\Sunshine\config\covers\app_Steam_1234567890.png
   New path: app_Steam_1234567890.png

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
✅ Successfully migrated 1 image(s)
   Updated: C:\Sunshine\config\apps.json
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

## Notes

1. **Backups**: backups happen automatically (via `Copy-Item`); the originals are not deleted
2. **Permissions**: write permission to the config directory is required
3. **Filenames**: special characters are replaced with underscores (`[^a-zA-Z0-9_-]` → `_`)
4. **Timestamps**: a Unix timestamp ensures filenames are unique
5. **Encoding**: apps.json is saved as UTF-8
