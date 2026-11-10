# Order N Chaos

A turn-based tactics game built on a custom C++20 engine, made by a nine-person student
team (Team GGPals) at DigiPen Singapore. Two players deploy three units each on opposite
grids, then alternate preparation and resolution phases: queue moves and abilities under a
timer, submit, and the server resolves both sides' actions in a fixed order.

This repository is the whole stack: the engine, the game project and the installer build.

## What is in here

```
GGPals_CustomEngine/
  GGMath/            header-only math library, no dependencies
  GGEngine/          static library: core systems, ECS, subsystem orchestration
  GGNetworking/      static library: UDP sockets, reliability protocol, packet
                     serialization, fragmentation, checksums, connection tracking,
                     I/O threading
  GGEditor/          executable: editor UI and application entry point
  GGProjects/        the game project and its Lua gameplay scripts
  EngineAssets/      engine-side assets
  GGPals_NetworkingDoc.md   networking architecture: the three layers, wire protocol,
                            threading model, RPC system, turn flow
  GGScriptingReference.md   Lua API reference
  projectGuide.txt          engine layout and setup
  editorGuide.pdf           how to use the editor
GGPals_InstallerData/  Inno Setup script and build tooling for the shipped installer
```

Networking is split across three layers: `GGNetworking` (transport, Winsock 2.2),
`GGEngine` (client/server integration, RPC routing, Lua bindings) and the Lua gameplay
scripts that call it. Read `GGPals_CustomEngine/GGPals_NetworkingDoc.md` for the protocol
and threading details.

## Build

Windows, MSVC, C++20, CMake 3.15 or newer.

```
run.bat      clean, configure with CMake, open GGEngine.sln
clean.bat    remove build artifacts
```

Build configuration `ReleaseEditor` for normal use, `ReleaseGame` for a shipping build
(no editor). Then run the executable from the build output.

To test multiplayer locally, build `ReleaseEditor` and run three instances: one acts as the
server, the other two connect as clients.

## Team

| Discipline | Name | Role |
| --- | --- | --- |
| RTIS | Tai Xiao Xuan | Tech Lead, Programmer, Networking Champion |
| RTIS | Abhishek Akilandeswari Patchamuthu | Programmer, Graphics Champion |
| RTIS | Zafir Haqqym | Programmer, Tools Champion |
| IMGD | Tan Jia Qi Trish | Programmer, Objects Factory Champion |
| IMGD | Celeste Tong Jia Xuan | Programmer, Physics and Animation Editor Champion |
| IMGD | Tok Duan Jie Nelson | Programmer, Input Champion |
| UXGD | Sim Wei Jun | Product Manager, Designer, Mechanics Champion |
| UXGD | Xia Sirui | Design Lead, Artist, Audio Champion |
| BFA | Quak Wen Xuan Rhonda | Art Lead, Artist, Art Champion |

## Status

Coursework project, archived as submitted. Not maintained, and no licence is granted for
reuse of the engine or its assets.
