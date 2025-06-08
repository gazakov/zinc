# zinc

A terminal-based productivity suite designed for simplicity and performance on low-resource systems. I'm developing it for my old laptop with just 512MB of RAM, envisioning it as a personal tool for discipline and focus.

## Features

- **Habit Tracking**: A dedicated module to build and maintain good habits.
- **Task Management**: Simple and effective to-do list.
- **Pomodoro Timer**: Stay focused with the Pomodoro technique.
- **Minimalist Interface**: A clean, hackable ncurses UI that stays out of your way.
- **Lightweight**: Designed to run smoothly on systems with limited resources.

## Getting Started

### Prerequisites

You'll need a C compiler (like `gcc`) and the `ncurses` library to build zinc.

- **On Debian/Ubuntu:**
  ```bash
  sudo apt-get update && sudo apt-get install build-essential libncurses5-dev
  ```
- **On macOS (using Homebrew):**
  ```bash
  brew install ncurses
  ```
- **On Arch Linux:**
  ```bash
  sudo pacman -Syu base-devel ncurses
  ```
- **On Windows (using MSYS2):**
  1.  Install [MSYS2](https://www.msys2.org/).
  2.  Open the MSYS2 MinGW 64-bit terminal and run:
      ```bash
      pacman -Syu mingw-w64-x86_64-gcc mingw-w64-x86_64-ncurses
      ```

### Building

To compile the application, you'll need to first compile the source files into object files and then link them together.

```bash
# Create a directory for object files
mkdir -p obj

# Compile main source files
gcc -Wall -Isrc -Imodules -g -c src/main.c -o obj/main.o
gcc -Wall -Isrc -Imodules -g -c src/minimal_tui.c -o obj/minimal_tui.o

# Compile module files
gcc -Wall -Isrc -Imodules -g -c modules/habit_manager.c -o obj/habit_manager.o
gcc -Wall -Isrc -Imodules -g -c modules/pomodoro_manager.c -o obj/pomodoro_manager.o
gcc -Wall -Isrc -Imodules -g -c modules/task_manager.c -o obj/task_manager.o

# Link object files to create the executable
gcc -Wall -Isrc -Imodules -g -o zinc obj/main.o obj/minimal_tui.o obj/habit_manager.o obj/pomodoro_manager.o obj/task_manager.o -lncurses
```

## Usage

Run the compiled application from the project root:

```bash
./zinc
```

### Navigation
- **Arrow Keys (↑/↓)**: Navigate through lists and menus.
- **Enter**: Select an item or confirm an action.
- **b**: Go back to the previous screen (from inside a module).
- **q**: Quit the application.

## Future Plans

I'm actively developing zinc for my personal use on a low-spec laptop. I plan to continue adding and refining features as I see fit. While this is primarily a personal project, I'm open to new ideas and contributions if the need arises.

## License

This project is licensed under the GNU General Public License v3.0. See the [LICENSE](LICENSE) file for details.