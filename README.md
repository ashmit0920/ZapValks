# ZapValks: A 2D-Shooter Game

**ZapValks** is a fast-paced 2D side-scrolling shooter game built using modern C++ and OpenGL. You control a Space Soldier gliding through space, blasting hostile valkyries while dodging their onslaught. Every bullet counts — every miss could cost your wings.

## Features

- ?? Modern OpenGL rendering (GLFW + GLAD)
- ?? PNG-based sprite textures
- ?? Real-time input and shooting mechanics
- ?? Sound effects using the lightweight [MiniAudio](https://miniaud.io/)
- ? Starfield background and health bar
- ?? In-game text rendering using `stb_easy_font`

## Controls

| Key           | Action                   |
|---------------|--------------------------|
| `W / Up`      | Move Up                  |
| `S / Down`    | Move Down                |
| `Space`       | Shoot                    |
| `Enter`       | Start or Restart Game    |
| `I`           | View Instructions        |
| `Backspace`   | Return to Main Menu      |
| `Escape`      | Exit Game                |

## Build Instructions

1. Clone this repository:

```bash
   git clone https://github.com/ashmit0920/ZapValks.git
```

2. Open the project in **Visual Studio**.

3. Make sure these dependencies are set up:

   * `glfw3.dll` and headers
   * `glad.c` and headers
   * `stb_image.h` and `stb_easy_font.h`
   * `miniaudio.h` (no DLL needed)

4. Build and run in **Release mode** for best performance.

---

## Assets

All sprites and sounds are placed inside the `assets/` folder. Player and enemy textures are PNGs, and sound effects are `.mp3` files triggered on shooting.