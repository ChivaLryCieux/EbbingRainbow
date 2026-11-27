// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Legion/Terrain/LegionTerrainGrid.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegionTerrainPathfindingTest, "EbbingRainbow.Legion.Terrain.Pathfinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLegionTerrainPathfindingTest::RunTest(const FString& Parameters)
{
	FLegionTerrainGrid Grid;
	const FVector UnbuiltGoal(123.0, 456.0, 789.0);
	TArray<FVector> Waypoints;
	Waypoints.Add(FVector::ZeroVector);
	TestFalse(TEXT("unbuilt grid cannot find a path"), Grid.FindPath(FVector::ZeroVector, UnbuiltGoal, Waypoints));
	if (TestEqual(TEXT("unbuilt path replaces output"), Waypoints.Num(), 1))
	{
		TestEqual(TEXT("unbuilt path preserves requested goal"), Waypoints[0], UnbuiltGoal);
	}

	Grid.GridWidth = Grid.GridHeight = 32;
	Grid.CellSize = 100.0f;
	Grid.Cells.SetNum(Grid.GridWidth * Grid.GridHeight);
	const auto MarkWalkable = [&Grid](int32 X, int32 Y, float Z = 0.0f)
	{
		FLegionTerrainGrid::FCell& Cell = Grid.Cells[Grid.IndexOf(X, Y)];
		Cell.HeightZ = Z;
		Cell.bValid = Cell.bBaseWalkable = Cell.bWalkable = true;
	};
	const auto Center = [&Grid](int32 X, int32 Y) { return Grid.GridToWorld(X, Y); };

	MarkWalkable(6, 6);
	MarkWalkable(9, 5);
	TestEqual(TEXT("first walkable ring wins over a closer farther-ring cell"), Grid.ClampToWalkable(Center(9, 9)), Center(6, 6));
	MarkWalkable(8, 7);
	MarkWalkable(7, 8);
	TestEqual(TEXT("equal distance in a ring preserves row order"), Grid.ClampToWalkable(Center(9, 9)), Center(8, 7));
	MarkWalkable(9, 7);
	TestEqual(TEXT("within a ring the smaller squared distance wins"), Grid.ClampToWalkable(Center(9, 9)), Center(9, 7));

	const FVector LoneStart = Center(10, 28);
	MarkWalkable(21, 28, 42.0f);
	TestEqual(TEXT("clamp does not reach cell in ring eleven"), Grid.ClampToWalkable(LoneStart), LoneStart);
	TestTrue(TEXT("path snaps with its larger radius"), Grid.FindPath(LoneStart, Center(21, 28), Waypoints));
	if (TestEqual(TEXT("same snapped cell returns one waypoint"), Waypoints.Num(), 1))
	{
		TestEqual(TEXT("same snapped cell returns the center"), Waypoints[0], Center(21, 28));
	}

	Grid.Cells.Reset();
	Grid.Cells.SetNum(Grid.GridWidth * Grid.GridHeight);
	for (int32 X = 2; X <= 8; ++X) { MarkWalkable(X, 2); }
	for (int32 Y = 3; Y <= 5; ++Y)
	{
		MarkWalkable(2, Y);
		MarkWalkable(8, Y, 75.0f);
	}
	const FVector Goal = Center(8, 5) + FVector(17.0, -17.0, 100.0);
	const FVector GroundGoal(Goal.X, Goal.Y, 75.0);
	TestTrue(TEXT("straight segment keeps original goal XY"), Grid.FindPath(Center(8, 2), Goal, Waypoints));
	if (TestEqual(TEXT("straight segment has one waypoint"), Waypoints.Num(), 1))
	{
		TestEqual(TEXT("straight segment uses sampled ground height"), Waypoints[0], GroundGoal);
	}
	TestTrue(TEXT("A* finds the U-shaped detour"), Grid.FindPath(Center(2, 5), Goal, Waypoints));
	if (TestEqual(TEXT("detour retains two corners and the goal"), Waypoints.Num(), 3))
	{
		TestEqual(TEXT("first corner"), Waypoints[0], Center(2, 2));
		TestEqual(TEXT("second corner"), Waypoints[1], Center(8, 2));
		TestEqual(TEXT("detour uses original goal XY and sampled Z"), Waypoints[2], GroundGoal);
	}
	Grid.Cells[Grid.IndexOf(5, 2)].bWalkable = false;
	TestFalse(TEXT("blocked detour has no path"), Grid.FindPath(Center(2, 5), Goal, Waypoints));
	if (TestEqual(TEXT("blocked path has one fallback waypoint"), Waypoints.Num(), 1))
	{
		TestEqual(TEXT("blocked path preserves original goal"), Waypoints[0], Goal);
	}
	return true;
}

#endif
