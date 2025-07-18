
# Raycaster

A simple sector and linedef based raycaster written in C.

## Background
Once upon a time when taking part in a gamejam event I wrote a little [pseudo-3D RPG](https://eigen.itch.io/sunless-isle) that fit on a 64x64 screen. It started out as a standard raycaster experiment following [Lode's tutorial](https://lodev.org/cgtutor/raycasting.html) (most well known on the subject probably) but I decided to try and replace regular grid map with lines instead. So instead of checking collisions and intersections *optimally*, you do the opposite and find all lines the ray intersects and draw them in order. The upside is that you can have varying heights of walls and walls behind each other. This project follows that idea but has a bunch of improvements, although the underlying algorithm is still not optimal, since you have to find a lot of line intersections for each column instead of doing what DOOM does, for example. *BUT* the benefit is a lot simpler code to follow and modify, so yeah.. pros and cons.

## Features
* :black_square_button: Basic sector and linedef handling
* 🚧 Basic level construction: define some polygons and automatically build the sectors
* ⛅ Sector brightness with distance falloff for walls, floors and ceilings
* 💡 Point lights with dynamic shadows
* ⭐ Uses GeneralPolygonClipper for the sector differences and splitting
* :dash: Uses OpenMP to render columns in parallel = fast (optional)

## Unfeatures
* 🌲 Sprites & objects
* 🪞 Maybe portals or mirrors?

![image](https://github.com/user-attachments/assets/9fc0383a-bd30-4dce-a9a3-e3a858db8f85)
![image](https://github.com/user-attachments/assets/d8273d82-c590-4c58-a8dd-3c396a5b1353)
![image](https://github.com/user-attachments/assets/5a0f7275-709e-414f-873e-7df03bea65fb)

![SoftwareRenderingExample2025-07-0313-37-15-Trim-ezgif com-video-to-gif-converter](https://github.com/user-attachments/assets/8ffa15b4-2766-43e5-907c-5362a0faff6d)
> There is some support for dynamic lights but it's still quite early and inefficient at full resolution

![SoftwareRenderingExample2025-07-1815-52-431-ezgif com-video-to-gif-converter](https://github.com/user-attachments/assets/a7571be2-cfec-479c-90e6-e7f17f283ab3)

## How it works (WIP section)
Where normal Wolfenstein-3D-like raycasters work on a 2D grid, where each cell is either empty or a wall, here we can instead think of each cell consisting of 4 vertices and 4 lines connecting them. Then you can imagine we don't have to keep those 4 points as a square, and we can reshape it as a trapezoid or a rectangle. Then you can probably imagine we can add more vertices to the polygon. And then, imagine we flip everything. Instead of looking at the shape from the outside, we step inside it and start casting rays outward. This polygon becomes a sector we're standing in. Its edges are now outer walls, and the sector itself holds values like floor and ceiling heights, lighting, and more.

That sector can also hold more lines that are not part of the outer boundary. These lines are part of outer boundary of other sectors. That way we can have sectors within sectors - a raised platform in the middle of the room for example.

## Data structures (basically)
1. **Vertex**
    ```c
    vec2f       point
    ```
2. **Linedef**
   
    **Linedef** is basically one wall in the sector. It always has a reference to the sector that first created it (at index 0), but it can also have a reference to the sector behind it (generally at index 1).
    ```c
    vertex      *v0, *v1        // Vertices that define this line
    sector      *side_sector[2] // Sectors on one or both sides of the line
    ```
4. **Sector**
    ```c
    int32_t     floor_height
    int32_t     ceiling_height
    float       light
    linedef     **linedefs      // References to linedefs stored elsewhere
    size_t      linedefs_count
    ```
5. **Level / map data (optional)**
   
    Pointers to **Vertices**, **Linedefs** and **Sectors** refer to elements stored here, but this could also just reside in game state somewhere if you just have a singular map for example.
    ```c
    vertex      vertices[N]
    linedef     linedefs[N]
    sector      sectors[N]
    ```
6. **Camera**
   
    Defines where the viewpoint is and where it's looking at. In the future this structure (or concept) should be more game-specific, but for now the renderer reads data directly from this type.
    ```c
    vec2f       position
    vec2f       direction
    vec2f       plane           // Unit vector perpendicular to direction,
                                // scaled by FOV
    float       fov             // 1.0f ~ 90°
    float       z
    sector      *in_sector      // Sector from which the column render loop begins
                                // Should be updated when camera moves
    level_data  *level          // Reference to level data, so we know what sector
                                // to put the camera in when it moves
    ```

## Render loop
:construction: TODO :construction:

---
The general concept is to have **sectors** that define floor and ceiling height (and light in the future) and where each sector has some **linedefs** which can have a reference to the sector behind it. You start drawing from the sector the camera is currently in --- for each column you check that sector's visible linedefs for intersections and sort them by distance. If the linedef has no back sector, you draw a full wall segment and terminate that column. If there is a back sector, you draw an upper and lower wall segments based on the floor and ceiling height difference compared to current sector, and then move on the sector behind and repeat. You keep track of sectors that have been visited in each column to avoid cycling. As mentioned earlier, this is not an optimal algorithm but it's simple, and since drawing only happens within a column where global state is not mutated, it's easily parallelizable (thanks to OMP in this case).

### Getting started
The library uses CMake. You can use CMake GUI or command line arguments to set renderer related options.

1. Generate makefiles `cmake -S . -B build` and write them to `build` directory 
2. Build everything `cmake --build ./build` in the build directory (or just `.` if you cd in there)

> ℹ️ MSVC might need an additional `--config Debug|Release` in there

On Windows you can use the batch files to quickly run the demo and tests targets.

1. `./demo -level <int>` to run the demo (level 0 to 5). There's also `-f` option for fullscreen and `-s <int>` to set the scaling value
2. `./tests` to run the unit tests

# What now?
If any of this is interesting and you want to ask anything, or contribute even, then we can chat on [Discord](https://discord.gg/X379hyV37f) 👋
