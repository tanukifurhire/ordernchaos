================================================================================
                         GGPALS DEMO - USER GUIDE
================================================================================

Team GGPals - Technical Team

RTIS
Tech Lead/Programmer/Networking Champion: Tai Xiao Xuan
Programmer/Graphics Champion: Abhishek Akilandeswari Patchamuthu
Programmer/Tools Champion: Zafir Haqqym

IMGD
Programmer/Objects Factory Champion: Tan Jia Qi Trish
Programmer/Physics Champion/Animation editor champion: Celeste Tong Jia Xuan
Programmer/Input Champion: Tok Duan Jie Nelson

UXGD
Product Manager/Designer/Mechanics champion: Sim Wei Jun
Design Lead/Artist/Audio champion: Xia Sirui

BFA
Art Lead/Artist/Art champion: Quak Wen Xuan Rhonda

================================================================================
TABLE OF CONTENTS
================================================================================

1. GAME CONCEPT
2. M3 UPDATES
3. ENGINE DEMO OVERVIEW
4. HOW TO RUN
5. TEST CASES
6. CORE GAMEPLAY LOOP + HOW TO PLAY
7. PLAYABLE FEATURES
8. KNOWN LIMITATIONS

================================================================================
1. GAME CONCEPT
================================================================================

Order N Chaos is a turn-based, tactical online multiplayer game where players command 
the forces of Order or Chaos, aiming to bombard each other's grid with powerful abilities. 
Each player controls units on their own grid, while their opponent's grid is shrouded under 
fog of war. Players plan their actions in secret, and watch the chaos unfold when both 
players end their turn. Units can either protect their own grid or disrupt and destroy the 
opponent's grid. 

================================================================================
2. M3 UPDATES (Milestone 3 Changes)
================================================================================
This milestone focused on integrating gameplay functionality and extending GGEditor 
support to complete the overall production workflow. In this milestone, the goal of this
build is for local 1v1 pass-and-play prototype as online is in future scope. The following systems and
features were implemented or enhanced in M3:

**Gameplay Systems:**
  - Implemented the full turn-based action system described in the game concept
    (planning phase, execution phase, and resolution phase).
  - Integrated grid-based unit interactions following the Order/Chaos gameplay rules.

**Tilemap Editor:**
  - Supports loading, viewing, and editing tilemap assets within the GGEditor.

**Play/Pause/Stop System:**
  - Added top-bar simulation controls within the editor. "Play" switches the editor into 
    runtime simulation mode. "Pause" freezes the simulation. "Resume" resumes simulation.
    "Stop" restores the original scene using scene backup/restore logic.

**Dynamic Asset Discovery System:**
  - Auto-discover all asset files at runtime and Assets appear immediately(hot-reloading) 
    in the Asset Browser without restarting or refreshing.

**Gizmos:**
  - Now supports rotating and scaling.

**Drag and Drop Asset Importer:**
  - Assets (Textures, Audio, Scripts, Prefabs, Animations, Tilemaps) can be dragged into
    the GGEditor to be automatically imported. System detects file type, copies into the 
    correct subfolder, generates a UUID meta file, and displays the asset immediately in 
    the browser.

**Unsupported and Duplicated File Handling:**
  - Dragging unsupported files triggers a popup listing all invalid files. Dragging duplicate 
    asset files triggers a popup allowing Replace All / Skip All / Choose Individually.

**Prefab System Improvements:**
  - Added ability to edit prefab instances via Prefab Inspector mode. Saving a prefab 
    propagates changes to all existing prefab instances.

**Undo/Redo System:**
  - Added undo stack support for entity creation/deletion, component changes, and 
    transform modifications.
  - Supports Undo (Ctrl+Z) and Redo (Ctrl+Y).

**Animation System:**
  - Added spritesheet slicing workflow and .gganim export.
  - Animations can be assigned to Sprite Animator components and updated in real time.

================================================================================
3. ENGINE DEMO OVERVIEW
================================================================================

The GGPals demo showcases the core capabilities of the GGEngine ECS
architecture each test case demonstrates a core subsystem of the engine 
working in real time through the GGEditor interface, including:

- **Entity-Component-System (ECS):** Demonstrates 4095-entity stress testing
  with cache-coherent component iteration.
  
- **Fixed-Timestep Physics:** Deterministic collision resolution using AABB
  swept collision detection with 60 FPS lockstep simulation.
  
- **Editor Integration:** ImGui-based runtime scene manipulation, entity
  inspection, and performance profiling.

**Execution Entry Point:**
  - File: GGEditor/src/main.cpp
  - Function: wWinMain() -> GGEngine::StartEngine()
  
**Default Scene:**
  - Scene: TEST_ADD_REMOVE_ENTITY (see GGEngine.hpp::Scenes enum)
  - Location: Loaded from ../../scenes/scene.json

================================================================================
4. HOW TO RUN
================================================================================

Launch run_or_cleanup.bat, select option 2 (type in "2"). This will clean up build 
artifacts and launch Visual Studio for you.

To build the executable, choose one of the following build configurations:
  - **DebugEditor**: Editor with debug symbols and console window (slow, for debugging)
  - **ReleaseEditor**: Optimized editor without console (fast, for content creation)
  - **ReleaseGame**: Final game build without editor (for distribution)

Press "Build" to compile.

To run the executable, use Local Windows Debugger or navigate to:
  - GGEngine/bin/Debug for DebugEditor builds
  - GGEngine/bin/Release for ReleaseEditor and ReleaseGame builds

================================================================================
5. TEST CASES
================================================================================

The following details how to test the available test cases


**Camera Settings:**
  - Dev Tools Tab > CameraSettings > Select "Editor Camera" > adjust Position, 
    Zoom, and Rotation using Sliders
  - Use middle mouse button to pan and scroll wheel to zoom in/out
  - Press "Reset Position Editor" or "Reset All Editor" to restore default camera state
  - Switch to "Game Camera" tab and adjust Zoom, Position, and Rotation the same way


**Gizmo Settings:**
  - Dev Tools Tab > GizmoSettings > click "Gizmo: ON" to show gizmos in viewport
  - Select an entity in Hierarchy Tab (must have "MeshRenderer" component) > gizmo
    appears at its position
  - Choose Translate(Hotkey: G), Rotate(Hotkey: R), or Scale (Hotkey: S) modes to 
    manipulate the entity
  - Adjust Line Length, Handle Size, or Circle Radius under "Mode Settings"
  - Click "Reset Current Mode" to reset gizmo visual parameters
  - Selecting another entity updates the Selection Info panel

**Audio Controls:**
  - Asset Manager Tab > select an audio file 
  - Dev Tools Tab > Audio Controls
  - Click "Play" to preview the selected audio
  - Click "Stop All" to stop all preview audio instances
  - Adjust "Master Volume" slider to modify global audio volume
  - View currently playing audio under the "Currently Playing" panel

**Frame Rate:**
  - Dev Tools Tab > Frame Rate > adjust "Frame Rate Cap" slider(30-240 FPS)
  - Observe changes in Target FPS and Actual FPS displayed in real time

**Performance:**
  - Dev Tools Tab > Performance > enable profiling by checking "Enable Profiling"
  - Perform actions in the editor to generate system activity
  - Adjust "Sample Window" slider to change averaging smoothness
  - Toggle "Sort By: Time / Name" to reorder system timing list
  - Click "Clear Data" to reset all recorded samples

================================================================================
6. CORE GAMEPLAY LOOP + HOW TO PLAY
================================================================================

**Overview:**
- 1v1 multiplayer duel in pass and play format (Planned online in future milestones)
- Each turn, players move their units and cast their abilities on the opponent's board. After a player has finished their turn, please pass the device to the next player. Players should not be watching each other's screens when they are planning their turns. Players can observe what happens during the resolution phase together.
-In the intended future updates, unit movement will be hidden to the other player and the opposing player will not know if the units will be moving and will only see the unit's last position on the grid. In this particular demo as a demonstration, unit movement is immediate and 

**How to play:**
You may press the '?' button at the top right of the screen to open the How to play menu

1. With two players, assign one player to play as Order, while the other to play as Chaos.
2. After pressing either 'Play as Order' or 'Play as Chaos' in the main menu, the Order player will place their units on the board before passing the device to the Chaos player to deploy their units
3. Press E to advance to the next phase
4. After deploying units, it will be the planning phase where players can either move each unit once per turn or cast abilities. In this milestone submission, movement will happen immediately during the planning phase and the opponent can see the result. In future milestones, movement will be hidden to the opposing player for mind games.
5. For each unit that stands on a colour, the corresponding abilities of the required colour can be casted. For if an ability requires 1 red and 1 blue, the player must position their units to at least 1 red tile and 1 blue tile. In this milestone, you can cast any number of abilities per turn as a demonstration. All abilities casted will resolve during the resolution phase
6. There are two types of abilities - Assault and Support Abilities. Assault abilities target your opponent's grid to deal damage. Support abilities will heal you.
7. The resolution phase will show all the casted abilities in sequence and damage the opponent's grid and units. This starts applying on Order side, followed by Chaos side

**Controls:**
1. Left Mouse button for various unit interactions and ability casting
2. E button to swap between grids

**Player Interactables:**
  - 2 characters depending on what faction they are playing (Order side plays Oracle and Wyvern while Chaos side plays Imp and Eye).
  - An ability arsenal bar in the middle bottom of the screen which shows castable abilities. Abilities are castable corresponding to the number of colours the units are standing on (e.g. The player can cast abilities that require 1 red and 1 blue if their units are standing on at least 1 red and 1 blue tile)
  - A grid swap button to look between the two different grids
  - An end turn button
  - A shared faction HP bar that shows their remaining health
  - A pause button
  - A help button for how to play

**Unts, Grid and Abilities:**
  - Move up to once per turn	
  - Can cast abilities dependent on standing on red and blue tiles.

- Win Condition:
  - Deplete the enemy faction’s HP bar to 0 by using offensive abilities on the opponent's grid

================================================================================
7. PLAYABLE FEATURES
================================================================================

**Main Menu:**
  - The player can hover over each button to see its highlighted form
  - Clicking on 'Play as Order' or 'Play as Chaos' will bring the player to the game scene
  - Clicking on the 'Quit' button will quit the game

**Tiles and Grid**
  - Tiles exist in 12 red and 12 blue tiles in a 6 x 4 format. 
  - Hovering over tiles with left mouse button will highlight the tiles

**Deployment Phase**
  - Units can be deployed on the grid from their pedestals

**Planning Phase**
  - Units can be left clicked to indicate where they can move to with highlighted tiles. Units can be moved once per turn. Left clicking on a highlighted
tile will move the unit to the location
  - For each unit that stands on a coloured tile, abilities can be casted if their required colours are met. (No limit for this milestone)
  - Assault abilities will target the opponent's grid and deal damage to units and tiles during the resolution phase
  - Support abilities will heal the player and resolve right away
  
**Resolution Phase**
- All assault abilities casted will resolve in sequence. This starts applying on Order side, followed by Chaos side. Once this phase is over, the
phase will advance to the next planning phase

**GUI**
  - Pause button will show the pause screen
  - How to Play button will show the how to play panels where different panels can be viewed by left clicking the arrows beside the panels
  - Change grid button will swap the view between the Order and Chaos grids

**Buttons**
  - Press E to advance to the next phase

**Health**
  - Depletes when a unit takes damage. If a player has no more health remaining, they will lose the game

================================================================================
8. KNOWN LIMITATIONS
================================================================================

1. Only 2 units per faction are available in this prototype
2. Pass and Play format this prototype for multiplayer. Future milestones will include online multiplayer
3. Balancing are prototype level and subject to change as there are currently no special effects
4. Movement for units happen immediately, taking away the mind games element of the game. In future milestones, movement is hidden to the opposing player for deceptive scenarios where they player does not know where the opponent's units will be
5. Need additional feedback regarding clear ability selections and casting abilities which will be revised in future milestones
6. There are no turn timers at the moment. In future milestones, a timer will be implemented to prevent indefinite stalling