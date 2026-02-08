# EdgeClock

A lightweight, unobtrusive digital clock for Windows designed to sit on the edge of your screen.

## Features

*   **Edge-Clinging Design**: Sits neatly at the bottom-right of your screen (or wherever you position it).
*   **Auto-Hide**: Automatically slides out of view when you hover over it or when other windows need the space (smart detection).
*   **Efficiency Mode**: Automatically enters Windows "EcoQoS" mode to minimize battery and CPU usage.
*   **Settings Dialog**: A comprehensive, modern UI to configure:
    *   **Font**: Choose any installed font.
    *   **Colors**: Custom Text and Outline colors.
    *   **Animation**: Adjust slide speed/duration.
    *   **Position**: Fine-tune X/Y offsets.
*   **Lightweight**: Built with raw Win32 API and GDI+, ensuring minimal footprint (no heavy frameworks).

## Installation & Usage

1.  Download the latest `EdgeClock.exe` from the [Releases](https://github.com/BBatWRP/tiny-overlay-clock/releases) page (or build it yourself).
2.  Run `EdgeClock.exe`.
3.  **Right-click** on the clock or the system tray icon to access the menu:
    *   Change Size
    *   Toggle "Run on Startup"
    *   Exit

## Building from Source

Requirements:
*   MinGW (g++) or MSVC.
*   Windows SDK (for headers).

**Using g++ (MinGW):**
```bat
g++ -o EdgeClock.exe EdgeClock.cpp -lgdi32 -luser32 -lgdiplus -mwindows -static
```

## Credits

*   **Concept & Development**: Bat
*   **AI Assistance**: Code architecture, refactoring, and feature implementation (EcoQoS, Registry logic) provided by **Google Gemini**.

## License

**PolyForm Noncommercial License 1.0.0**
Free for personal and non-commercial use.
**NOT FOR RESALE.**
[View License](LICENSE)
