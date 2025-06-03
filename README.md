# SWGT - Suckless Widget

A lightweight X11 widget that displays multiple toggleable buttons with custom icons and appears when the mouse cursor reaches the right edge of the screen.

## Features

- Slide-in animation from screen edge
- Multiple toggleable buttons with custom icons and text
- Each button executes different commands for on/off states
- Visual feedback with active/inactive states
- High-quality text rendering with Xft
- Configurable colors and fonts
- Low resource usage
- Customizable through config.h

## Dependencies

- X11 development libraries
- Xft development libraries
- FontConfig development libraries
- GCC compiler

On Debian/Ubuntu:
```bash
sudo apt install libx11-dev libxext-dev libxft-dev libfontconfig1-dev build-essential
```

On Arch Linux:
```bash
sudo pacman -S libx11 libxext libxft fontconfig base-devel
```

## Building

### Quick Start
```bash
# Make build script executable
chmod +x build.sh

# Development build (fast compilation)
./build.sh dev

# Release build (optimized)
./build.sh release

# Clean build
./build.sh --clean release
```

### Build Modes

- **dev**: Fast compilation with debug symbols (`-g -O0`)
- **release**: Maximum optimization (`-O3 -march=native -flto`)

### Manual Building
```bash
# Development
make dev

# Release
make release

# Clean
make clean
```

## Configuration

Edit `config.h` to customize:
- Widget dimensions and colors (now using simple hex format)
- Animation settings
- Font preferences (now using modern Xft fonts)
- Button count, icons, text, and commands
- Individual button toggle/untoggle commands
- Hover zone width for precise activation

## Installation

```bash
# Build and install to /usr/local/bin
make install

# Uninstall
make uninstall
```

## Usage

Simply run the executable:
```bash
./swgt
```

The widget will appear when you move your mouse to the designated hover zone at the right edge of the screen. The buttons are visible immediately during animation for better visual feedback. Click any button to toggle its state - buttons will change color and execute different commands based on their active/inactive state.
