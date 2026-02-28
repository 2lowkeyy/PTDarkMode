# PTDarkMode

Dark theme injection for Pro Tools on Windows. Turns the white menu bar, window captions, and plugin title bars dark without modifying Pro Tools itself. Just drop two DLLs in the install folder and you're done.

![Pro Tools with PTDarkMenu](https://i.imgur.com/WT1ivFZ.png)
![](https://i.imgur.com/TgqcTQa.png)
![](https://i.imgur.com/dJYJOxB.png)
---

## Features

- **Dark menu bar** — File, Edit, View, Track and all other menu items styled with a dark background and hover highlight
- **Dark window caption** — Main window and all plugin title bars darkened via DWM
- **4 fullscreen / focus modes** — toggled with keyboard shortcuts to hide the caption and/or menu bar for a cleaner workspace

## Keyboard Shortcuts

| Shortcut | Mode |
|---|---|
| `Ctrl+Shift+F9` | Windowed — caption + menu visible, MDI borderless |
| `Ctrl+Shift+F10` | No menu — caption visible, menu hidden |
| `Ctrl+Shift+F11` | No caption — menu visible, caption hidden |
| `Ctrl+Shift+F12` | Full — caption + menu hidden |

Press the same shortcut again to toggle back to normal.

## Installation

1. Download `version.dll` and `PTDarkMenu.dll` from [Releases](../../releases)
2. Copy both files to your Pro Tools installation folder (e.g. `C:\Program Files\Avid\Pro Tools\`)
3. Launch Pro Tools — the dark theme applies automatically

To uninstall, delete both DLL files from the Pro Tools folder.

## Requirements

- Windows 10 version 1903 or later (Windows 11 recommended)
- Pro Tools (any modern version)
- [Visual C++ Redistributable 2022 x64](https://aka.ms/vc14/vc_redist.x64.exe)

## Building from Source

**Requirements:** Visual Studio 2022, Windows SDK

1. Clone the repository
2. Open `PTDarkMenu.sln` in Visual Studio 2022
3. Set configuration to **Release | x64**
4. Build solution — outputs go to `bin\`

The solution contains two projects:
- `PTDarkMenu` — the main dark theme DLL
- `version_proxy` — the proxy DLL that loads PTDarkMenu on Pro Tools startup

## How It Works

PTDarkMenu uses a `version.dll` proxy to inject itself into Pro Tools at startup. Once loaded, it:

- **Subclasses the main window** to intercept `WM_NCPAINT` and overlay a custom-drawn dark menu bar using an offscreen bitmap cache
- **Applies DWM attributes** (`DWMWA_USE_IMMERSIVE_DARK_MODE`, caption color) to the main window and all plugin windows
- **Uses a WinEventHook** (`EVENT_OBJECT_SHOW`) to detect plugin windows as they open and darken their captions instantly
- **Monitors for session load** (Edit:/Mix: MDI children) to apply the default windowed fullscreen mode automatically

No files are patched or modified. Removing the DLLs fully restores Pro Tools to its original state.

## Disclaimer

This project is not affiliated with or endorsed by Avid Technology. Use at your own risk. DLL injection may conflict with Pro Tools' copy protection or support agreements — you are responsible for any issues that arise.

## Note
Uncommented code. This is my first project in C++, and honestly the code is not a great example of how to write C++, but it works. Soon I will be adding comments and optimizing certain parts of the code, as well as fixing some bugs present in the current version.
