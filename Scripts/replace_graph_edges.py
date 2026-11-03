import io

path = r"D:\EbbingRainbow\Source\EbbingRainbow\WFC\Arch\WFCArchGrammar.cpp"
with io.open(path, "r", encoding="utf-8-sig", newline="") as f:
    text = f.read()

start_marker = "\t\t// Connect every adjacent pair of selected slots exactly once (E/S only)."
end_marker = "\t\t// Resolve walls, doors, floors, props. No roof: the graph is open-air."
start = text.index(start_marker)
end = text.index(end_marker, start)

new = '''		// Connect every adjacent pair of selected slots exactly once. Use all
		// four directions and normalize each pair to west/north -> east/south.
		int32 CorridorCount = 0;
		int32 EdgeCount = 0;
		for (const FIntPoint& S : Chosen)
		{
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const int32 NX = S.X + DX[Side];
				const int32 NY = S.Y + DY[Side];
				if (NX < 0 || NX >= Slots || NY < 0 || NY >= Slots) continue;
				if (!Selected[SlotIndex(NX, NY)]) continue;
				if (SlotIndex(NX, NY) < SlotIndex(S.X, S.Y)) continue;

				const int32 AIdx = RoomBySlot[SlotIndex(S.X, S.Y)];
				const int32 BIdx = RoomBySlot[SlotIndex(NX, NY)];
				if (AIdx == INDEX_NONE || BIdx == INDEX_NONE) continue;

				FGraphRoom* P = &Rooms[AIdx];
				FGraphRoom* Q = &Rooms[BIdx];
				if (NX < S.X || (NX == S.X && NY < S.Y))
				{
					Swap(P, Q);
				}

				if (P->Rect.X1 < Q->Rect.X0)
				{
					const int32 Y0 = FMath::Max(P->Rect.Y0, Q->Rect.Y0);
					const int32 Y1 = FMath::Min(P->Rect.Y1, Q->Rect.Y1);
					const int32 DoorY = (Y0 <= Y1) ? Rng.RandRange(Y0, Y1) : P->Rect.Y0;
					const FIntPoint PA(P->Rect.X1, DoorY);
					const FIntPoint PB(Q->Rect.X0, DoorY);
					DoorBit[PA.Y * W + PA.X] |= 1 << 1;
					DoorBit[PB.Y * W + PB.X] |= 1 << 3;
					for (int32 CX = P->Rect.X1 + 1; CX < Q->Rect.X0; ++CX)
					{
						FArchCell& C = Plan.Cell(CX, DoorY);
						if (C.Kind == EArchCellKind::Void)
						{
							C.Kind = EArchCellKind::Corridor;
							C.Space = INDEX_NONE;
							++CorridorCount;
						}
					}
					P->Degree++;
					Q->Degree++;
					++EdgeCount;
				}
				else if (P->Rect.Y1 < Q->Rect.Y0)
				{
					const int32 X0 = FMath::Max(P->Rect.X0, Q->Rect.X0);
					const int32 X1 = FMath::Min(P->Rect.X1, Q->Rect.X1);
					const int32 DoorX = (X0 <= X1) ? Rng.RandRange(X0, X1) : P->Rect.X0;
					const FIntPoint PA(DoorX, P->Rect.Y1);
					const FIntPoint PB(DoorX, Q->Rect.Y0);
					DoorBit[PA.Y * W + PA.X] |= 1 << 2;
					DoorBit[PB.Y * W + PB.X] |= 1 << 0;
					for (int32 CY = P->Rect.Y1 + 1; CY < Q->Rect.Y0; ++CY)
					{
						FArchCell& C = Plan.Cell(DoorX, CY);
						if (C.Kind == EArchCellKind::Void)
						{
							C.Kind = EArchCellKind::Corridor;
							C.Space = INDEX_NONE;
							++CorridorCount;
						}
					}
					P->Degree++;
					Q->Degree++;
					++EdgeCount;
				}
			}
		}

'''

text = text[:start] + new + text[end:]
with io.open(path, "w", encoding="utf-8-sig", newline="") as f:
    f.write(text)
print("replaced", len(new))
