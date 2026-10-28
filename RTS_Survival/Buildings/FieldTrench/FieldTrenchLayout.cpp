// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FieldTrenchLayout.h"

namespace FieldTrenchLayoutPrivate
{
	const TCHAR* const MeshFolder = TEXT("/Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench/Meshes/");

	// Arc radius of the bends.
	constexpr float BendRadius = 250.0f;
	// The end cap closes the trench over about this much ground behind its port.
	constexpr float EndCapLengthCm = 125.0f;

	// How far ahead along the drag the planner aims; shorter makes the trench chase every wobble of the mouse.
	constexpr float LookAheadCm = 300.0f;
	// Smaller turns are drawn straight, so a hand that is not quite steady still gives a straight trench.
	constexpr float MinimumTurnForBendDegrees = 10.0f;
	// A turn this sharp gets the zigzag corner instead of two bends.
	constexpr float MinimumTurnForCornerDegrees = 75.0f;
	// A long straight is only used when the drag goes on straight for at least that far.
	constexpr float StraightConeDegrees = 7.5f;
	// The last straight may run this far past the end of the drag.
	constexpr float StraightOvershootCm = 30.0f;
	// Bends that do not move the trench on along the drag would circle forever.
	constexpr int32 MaximumBendsWithoutProgress = 4;
	constexpr int32 MaximumPieceCount = 400;

	FVector2D RotateByYaw(const FVector2D& Vector, const float YawDegrees)
	{
		float Sine = 0.0f;
		float Cosine = 1.0f;
		FMath::SinCos(&Sine, &Cosine, FMath::DegreesToRadians(YawDegrees));
		return FVector2D(Vector.X * Cosine - Vector.Y * Sine, Vector.X * Sine + Vector.Y * Cosine);
	}

	float GetYawTowards(const FVector2D& From, const FVector2D& To)
	{
		const FVector2D Direction = To - From;
		return FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
	}

	FFieldTrenchPieceDefinition MakeDefinition(
		const EFieldTrenchPiece Piece,
		const TCHAR* MeshName,
		const float LengthCm,
		std::initializer_list<FFieldTrenchPort> Ports)
	{
		FFieldTrenchPieceDefinition Definition;
		Definition.Piece = Piece;
		Definition.MeshName = MeshName;
		Definition.LengthCm = LengthCm;
		for (const FFieldTrenchPort& Port : Ports)
		{
			Definition.Ports.Add(Port);
		}
		return Definition;
	}

	// The manifest is in Blender space; a port at Blender (x, y, yaw) is at Unreal (x, -y, -yaw).
	TArray<FFieldTrenchPieceDefinition> BuildKit()
	{
		const FFieldTrenchPort EntryA{FVector2D(0.0f, 0.0f), 180.0f};
		auto StraightExit = [](const float Length) { return FFieldTrenchPort{FVector2D(Length, 0.0f), 0.0f}; };
		auto BendExit = [](const float AngleDegrees)
		{
			const float Radians = FMath::DegreesToRadians(AngleDegrees);
			return FFieldTrenchPort{
				FVector2D(BendRadius * FMath::Sin(Radians), -BendRadius * (1.0f - FMath::Cos(Radians))), -AngleDegrees
			};
		};
		auto BendLength = [](const float AngleDegrees) { return BendRadius * FMath::DegreesToRadians(AngleDegrees); };

		TArray<FFieldTrenchPieceDefinition> Kit;
		Kit.Add(MakeDefinition(EFieldTrenchPiece::None, TEXT(""), 0.0f, {}));
		Kit.Add(MakeDefinition(EFieldTrenchPiece::Straight_125, TEXT("SM_FieldTrench_Straight_125"), 125.0f, {EntryA, StraightExit(125.0f)}));
		Kit.Add(MakeDefinition(EFieldTrenchPiece::Straight_250_A, TEXT("SM_FieldTrench_Straight_250_A"), 250.0f, {EntryA, StraightExit(250.0f)}));
		Kit.Add(MakeDefinition(EFieldTrenchPiece::Straight_250_B, TEXT("SM_FieldTrench_Straight_250_B"), 250.0f, {EntryA, StraightExit(250.0f)}));
		Kit.Add(MakeDefinition(EFieldTrenchPiece::Straight_250_C, TEXT("SM_FieldTrench_Straight_250_C"), 250.0f, {EntryA, StraightExit(250.0f)}));
		Kit.Add(MakeDefinition(EFieldTrenchPiece::Straight_500_A, TEXT("SM_FieldTrench_Straight_500_A"), 500.0f, {EntryA, StraightExit(500.0f)}));
		Kit.Add(MakeDefinition(EFieldTrenchPiece::Straight_500_B, TEXT("SM_FieldTrench_Straight_500_B"), 500.0f, {EntryA, StraightExit(500.0f)}));
		Kit.Add(MakeDefinition(EFieldTrenchPiece::Bend_15, TEXT("SM_FieldTrench_Bend_15"), BendLength(15.0f), {EntryA, BendExit(15.0f)}));
		Kit.Add(MakeDefinition(EFieldTrenchPiece::Bend_30, TEXT("SM_FieldTrench_Bend_30"), BendLength(30.0f), {EntryA, BendExit(30.0f)}));
		Kit.Add(MakeDefinition(EFieldTrenchPiece::Bend_45, TEXT("SM_FieldTrench_Bend_45"), BendLength(45.0f), {EntryA, BendExit(45.0f)}));
		Kit.Add(MakeDefinition(EFieldTrenchPiece::Corner_90, TEXT("SM_FieldTrench_Corner_90"), 375.0f,
			{EntryA, FFieldTrenchPort{FVector2D(187.5f, -187.5f), -90.0f}}));
		Kit.Add(MakeDefinition(EFieldTrenchPiece::Tee, TEXT("SM_FieldTrench_Tee"), 375.0f,
			{EntryA, StraightExit(375.0f), FFieldTrenchPort{FVector2D(187.5f, -250.0f), -90.0f}}));
		Kit.Add(MakeDefinition(EFieldTrenchPiece::Cross, TEXT("SM_FieldTrench_Cross"), 375.0f,
			{
				EntryA, StraightExit(375.0f), FFieldTrenchPort{FVector2D(187.5f, -250.0f), -90.0f},
				FFieldTrenchPort{FVector2D(187.5f, 250.0f), 90.0f}
			}));
		Kit.Add(MakeDefinition(EFieldTrenchPiece::End, TEXT("SM_FieldTrench_End"), EndCapLengthCm,
			{FFieldTrenchPort{FVector2D(0.0f, 0.0f), 0.0f}}));
		return Kit;
	}

	const TArray<FFieldTrenchPieceDefinition>& GetKit()
	{
		static const TArray<FFieldTrenchPieceDefinition> Kit = BuildKit();
		return Kit;
	}

	// The variants of a straight are taken in turn by the piece's place in the trench, so planning the same
	// drag again gives the same pieces and the preview does not flicker while the mouse moves.
	EFieldTrenchPiece GetStraightVariant(const float LengthCm, const int32 PieceIndex)
	{
		if (LengthCm >= 500.0f)
		{
			return PieceIndex % 2 == 0 ? EFieldTrenchPiece::Straight_500_A : EFieldTrenchPiece::Straight_500_B;
		}
		if (LengthCm >= 250.0f)
		{
			constexpr EFieldTrenchPiece Variants[] = {
				EFieldTrenchPiece::Straight_250_A, EFieldTrenchPiece::Straight_250_B, EFieldTrenchPiece::Straight_250_C
			};
			return Variants[PieceIndex % 3];
		}
		return EFieldTrenchPiece::Straight_125;
	}

	EFieldTrenchPiece GetBendForTurn(const float AbsoluteTurnDegrees)
	{
		if (AbsoluteTurnDegrees < 22.5f)
		{
			return EFieldTrenchPiece::Bend_15;
		}
		if (AbsoluteTurnDegrees < 37.5f)
		{
			return EFieldTrenchPiece::Bend_30;
		}
		return EFieldTrenchPiece::Bend_45;
	}

	// The point the next piece aims for: the first point of the drag, from where the trench has got to, that
	// lies far enough ahead; the end of the drag when none does.
	FVector2D FindAimPoint(const TArray<FVector2D>& DragPath, const FVector2D& From, int32& InOutProgressIndex)
	{
		while (InOutProgressIndex + 1 < DragPath.Num() &&
			FVector2D::DistSquared(From, DragPath[InOutProgressIndex + 1]) <
			FVector2D::DistSquared(From, DragPath[InOutProgressIndex]))
		{
			++InOutProgressIndex;
		}
		for (int32 PathIndex = InOutProgressIndex; PathIndex < DragPath.Num(); ++PathIndex)
		{
			if (FVector2D::Distance(From, DragPath[PathIndex]) >= LookAheadCm)
			{
				return DragPath[PathIndex];
			}
		}
		return DragPath.Last();
	}
}

const FFieldTrenchPieceDefinition& FieldTrenchLayout::GetPieceDefinition(const EFieldTrenchPiece Piece)
{
	const TArray<FFieldTrenchPieceDefinition>& Kit = FieldTrenchLayoutPrivate::GetKit();
	const int32 PieceIndex = static_cast<int32>(Piece);
	return Kit.IsValidIndex(PieceIndex) ? Kit[PieceIndex] : Kit[0];
}

FSoftObjectPath FieldTrenchLayout::GetPieceMeshPath(const EFieldTrenchPiece Piece)
{
	const FString MeshName = GetPieceDefinition(Piece).MeshName;
	if (MeshName.IsEmpty())
	{
		return FSoftObjectPath();
	}
	return FSoftObjectPath(FString(FieldTrenchLayoutPrivate::MeshFolder) + MeshName + TEXT(".") + MeshName);
}

void FieldTrenchLayout::PlacePieceOnPort(
	const EFieldTrenchPiece Piece,
	const int32 EntryPortIndex,
	const FFieldTrenchOpenPort& OpenPort,
	FFieldTrenchPiecePlacement& OutPlacement,
	FFieldTrenchOpenPort& OutExitPort)
{
	using namespace FieldTrenchLayoutPrivate;
	const FFieldTrenchPieceDefinition& Definition = GetPieceDefinition(Piece);
	OutExitPort = OpenPort;
	if (not Definition.Ports.IsValidIndex(EntryPortIndex))
	{
		return;
	}
	const FFieldTrenchPort& EntryPort = Definition.Ports[EntryPortIndex];
	// The entry port points back at the open port: their yaws are opposite.
	const float PieceYaw = FRotator::NormalizeAxis(OpenPort.Yaw + 180.0f - EntryPort.Yaw);
	const FVector2D PieceLocation = OpenPort.Location - RotateByYaw(EntryPort.Location, PieceYaw);
	OutPlacement.Piece = Piece;
	OutPlacement.Location = FVector(PieceLocation, 0.0f);
	OutPlacement.Yaw = PieceYaw;
	OutPlacement.LengthCm = Definition.LengthCm;
	if (Definition.Ports.Num() < 2)
	{
		return;
	}
	// Two-port pieces leave through the other port; branches are not planned along a drag.
	const FFieldTrenchPort& ExitPort = Definition.Ports[EntryPortIndex == 0 ? 1 : 0];
	OutExitPort.Location = PieceLocation + RotateByYaw(ExitPort.Location, PieceYaw);
	OutExitPort.Yaw = FRotator::NormalizeAxis(PieceYaw + ExitPort.Yaw);
}

TArray<FFieldTrenchPiecePlacement> FieldTrenchLayout::PlanAlongPath(
	const TArray<FVector2D>& DragPath,
	const float MaximumLengthCm,
	const float MaximumStraightLengthCm)
{
	using namespace FieldTrenchLayoutPrivate;
	TArray<FFieldTrenchPiecePlacement> Pieces;
	const float ShortestStraight = GetPieceDefinition(EFieldTrenchPiece::Straight_125).LengthCm;
	if (DragPath.Num() < 2 || FVector2D::Distance(DragPath[0], DragPath.Last()) < ShortestStraight)
	{
		return Pieces;
	}
	const FVector2D& PathEnd = DragPath.Last();
	int32 ProgressIndex = 0;
	FFieldTrenchOpenPort OpenPort;
	OpenPort.Location = DragPath[0];
	OpenPort.Yaw = GetYawTowards(OpenPort.Location, FindAimPoint(DragPath, OpenPort.Location, ProgressIndex));

	// The start cap's port faces along the trench, so it joins an open port that faces back from the start.
	FFieldTrenchOpenPort StartCapPort = OpenPort;
	StartCapPort.Yaw = FRotator::NormalizeAxis(OpenPort.Yaw + 180.0f);
	FFieldTrenchOpenPort UnusedExit;
	PlacePieceOnPort(EFieldTrenchPiece::End, 0, StartCapPort, Pieces.AddDefaulted_GetRef(), UnusedExit);
	float TotalLength = Pieces.Last().LengthCm;

	int32 BendsWithoutProgress = 0;
	int32 ProgressIndexAtLastPiece = ProgressIndex;
	while (Pieces.Num() < MaximumPieceCount)
	{
		const float DistanceToEnd = FVector2D::Distance(OpenPort.Location, PathEnd);
		if (DistanceToEnd + StraightOvershootCm < ShortestStraight)
		{
			break;
		}
		const FVector2D AimPoint = FindAimPoint(DragPath, OpenPort.Location, ProgressIndex);
		const float Turn = FRotator::NormalizeAxis(GetYawTowards(OpenPort.Location, AimPoint) - OpenPort.Yaw);
		const float AbsoluteTurn = FMath::Abs(Turn);
		EFieldTrenchPiece NextPiece = EFieldTrenchPiece::None;
		int32 EntryPortIndex = 0;
		if (AbsoluteTurn >= MinimumTurnForBendDegrees)
		{
			NextPiece = AbsoluteTurn >= MinimumTurnForCornerDegrees
				? EFieldTrenchPiece::Corner_90
				: GetBendForTurn(AbsoluteTurn);
			// Entered from its first port a bend turns to a smaller yaw; from its second port to a larger one.
			EntryPortIndex = Turn < 0.0f ? 0 : 1;
			BendsWithoutProgress = ProgressIndex > ProgressIndexAtLastPiece ? 0 : BendsWithoutProgress + 1;
			if (BendsWithoutProgress > MaximumBendsWithoutProgress)
			{
				break;
			}
		}
		else
		{
			const float TurnToEnd = FMath::Abs(FRotator::NormalizeAxis(
				GetYawTowards(OpenPort.Location, PathEnd) - OpenPort.Yaw));
			// Straight on to the end of the drag allows the long pieces; otherwise only up to the aim point.
			const float StraightRoom = (TurnToEnd < StraightConeDegrees
				? DistanceToEnd
				: FVector2D::Distance(OpenPort.Location, AimPoint)) + StraightOvershootCm;
			const float StraightLength = FMath::Min(
				MaximumStraightLengthCm,
				StraightRoom >= 500.0f ? 500.0f : StraightRoom >= 250.0f ? 250.0f : 125.0f);
			NextPiece = GetStraightVariant(StraightLength, Pieces.Num());
			BendsWithoutProgress = 0;
		}
		const FFieldTrenchPieceDefinition& Definition = GetPieceDefinition(NextPiece);
		// The end cap always has to fit.
		const float EndCapLength = GetPieceDefinition(EFieldTrenchPiece::End).LengthCm;
		if (TotalLength + Definition.LengthCm + EndCapLength > MaximumLengthCm)
		{
			if (Definition.LengthCm <= ShortestStraight ||
				TotalLength + ShortestStraight + EndCapLength > MaximumLengthCm)
			{
				break;
			}
			NextPiece = EFieldTrenchPiece::Straight_125;
		}
		FFieldTrenchPiecePlacement& Placement = Pieces.AddDefaulted_GetRef();
		PlacePieceOnPort(NextPiece, EntryPortIndex, OpenPort, Placement, OpenPort);
		TotalLength += Placement.LengthCm;
		ProgressIndexAtLastPiece = ProgressIndex;
	}
	if (Pieces.Num() < 2)
	{
		Pieces.Reset();
		return Pieces;
	}
	PlacePieceOnPort(EFieldTrenchPiece::End, 0, OpenPort, Pieces.AddDefaulted_GetRef(), UnusedExit);
	return Pieces;
}

float FieldTrenchLayout::GetTotalLengthCm(const TArray<FFieldTrenchPiecePlacement>& Pieces)
{
	float TotalLength = 0.0f;
	for (const FFieldTrenchPiecePlacement& Placement : Pieces)
	{
		TotalLength += Placement.LengthCm;
	}
	return TotalLength;
}
