# Legion C++ source architecture

Current game flow: Hall -> Campaign contract -> Legion battle -> Campaign result. The turn-based Battle, Run and WFC code remains a separate gameplay path.

- Campaign owns contracts, roster, rewards and persistence. Legion reads a contract at deployment and reports a result; tactical actor state is world-local.
- Legion/Terrain now owns the terrain grid and its path queries. LegionTerrainGrid.cpp samples world terrain and provides read-only terrain/cover data and the existing map rasterizer. LegionTerrainPathfinding.cpp uses grid data only, with no actor or HUD dependencies. FindPath keeps its direct-line shortcut, weighted A*, smoothing and fallback contract.
- Legion/Deployment owns StartPlay orchestration: load assets, build terrain, spawn forest, bake map textures, deploy the initial roster, then position the commander. GameMode retains the deployment gate, Tick, reinforcement scheduling and result reporting. Initial and delayed brigades share one spawn implementation.
- Legion/Debug owns the GameMode's screenshot and self-test implementations. Runtime calls, flags and Unreal class identity stay in place.
- Experimental/LegionBattle still hosts the actor lifecycle, units, controls and HUD. Experimental/AITacticalCommand is an adapter, not an owner of battle state. The source paths are not a reason to change reflected class names or assets.

## Migration order

1. Separate soldier simulation from representation writes using the existing tick order as the compatibility boundary.
2. Profile transform computation, instance updates and render-state invalidation separately before changing the instanced path. In UE 5.8, the HISM batch-transform wrapper still calls `UpdateInstanceTransform` for every instance; changing the call site alone does not eliminate that work.
3. Revisit spatial indexing only if a representative larger battle shows local queries dominating. Keep legacy gameplay and campaign persistence out of these changes.

## CPU profiling

The base `ER_Legion_` CPU trace scopes measure deployment and map baking, GameMode tick and active projectile simulation, and instance transform writes. Read nested scopes within their parent intervals; do not sum them into the inclusive parent time. NullRHI timing is useful for CPU investigation, not rendered frame rate.

Use `Lvl_Legion` with `-game -nullrhi -trace=cpu -tracefile=<path>` and a separate `-UserDir=<path>` to avoid touching campaign saves. `-LegionLegions=4 -LegionSoldiers=48 -LegionCloseup` exercises the instanced representation. Inspect complete GameMode tick intervals in Insights; for combat start at the first active `ER_Legion_Bullets` tick. A cold start may spend time in a synchronous asset-registry scan before the first battle tick; exclude that wait from per-tick results.
