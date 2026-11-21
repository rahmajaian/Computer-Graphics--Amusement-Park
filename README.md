# 3D Amusement Park

*Course:* CSE-4742 (Computer Graphics)  
*Institution:* International Islamic University Chittagong  

## Team Members
* *Rahma Jaian* — ID: *C233498*
* *Mahia Akter* — ID: *C233501*
---

## Project Features

### Static Elements
* Trees and Decorative Plants
* Grass, Promenade, Slabs, and Road / Buildings
* Food and Game Stalls, Seating Sets, and Ice-Cream Truck / Cart Structure
* Park Fence and Entrance Gate (when visible)
* Lake, Hills, and Surrounding Mountains
* Princess Castle and Haunted Mansion Structures

### Animated Elements
* Rotating Ferris Wheel (with upright cabins)
* Moving Roller Coaster (with vertical loop and multi-car train)
* Merry-Go-Round / Carousel (with bobbing horses and chariots)
* Rising Hot Air Balloon
* Floating Balloons
* Moving Clouds
* Moving Birds (with flapping wings)
* Animated Sun and Brightness Effect
* Day, Sunset, and Night Mode (with sky transitions, sun/moon, stars, lit windows, and fireworks)
* Circus Tent with Lighting Effects
* Clown and Joker Characters (with animated limbs and juggling/sign motion)
* Running Kids and Running Girl with Dripping Ice Cream
* Animated Ice Cream Cart / Truck Scene
* Animated Decorative Lights (and toggleable lamp lights / synthesized music)


### Where Files Are Placed & Code::Blocks Setup

1. **Header Files (`freeglut.h`, `glut.h`, etc.):** Place inside the `GL` folder of our MinGW include directory (for example, `<MinGW_DIR>\include\GL\`) so `#include <GL/freeglut.h>` and `#include <GL/glu.h>` are found.
2. **Library Files (`libfreeglut.a` / import or static `freeglut` libraries):** Place inside our MinGW `lib` folder (for example, `<MinGW_DIR>\lib\`), alongside standard Windows/MinGW libraries (`opengl32`, `glu32`, `winmm`, `gdi32`).
3. **Runtime DLL (`freeglut.dll`):** Place in the same output folder where the compiled `.exe` is generated (such as `bin\Debug\` or `bin\Release\` inside our Code::Blocks project folder) or in a folder on Windows `PATH` so the executable runs without a missing-DLL error.
4. **Source File (`main.cpp`):** Place inside our Code::Blocks project folder and add it to the project in the **Management** pane.

#### Steps Inside Code::Blocks

1. Open **Code::Blocks** and create or open our C++ / GLUT project containing `main.cpp`.
2. If your `freeglut` include or library paths are outside the default MinGW directories, go to **Project > Build options... > Search directories** and add them under **Compiler** and **Linker**.
3. Go to **Project > Build options... > Linker settings** and add the following libraries in this exact order:
```text
-lfreeglut  -lopengl32  -lglu32  -lwinmm  -lgdi32

```


4. Build and run (`F9`). *(Audio uses the Windows `PlaySound` API under `#ifdef _WIN32`.)*)
