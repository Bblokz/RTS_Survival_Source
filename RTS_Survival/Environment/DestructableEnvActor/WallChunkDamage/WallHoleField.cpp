// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WallHoleField.h"

#include "Engine/Texture2D.h"
#include "TextureResource.h"

namespace WallHoleFieldPrivate
{
	constexpr float MinSegmentLength = 1.f;
	constexpr float MinTexelSize = 1.f;
	constexpr int32 BytesPerTexel = 4;
	constexpr uint8 EncodedRemovedThreshold = 128;
	// Support test: a texel this close outside a hole edge (f > -0.35: 14 cm, about the shader's edge noise) already
	// counts as broken.
	constexpr uint8 EncodedSupportThreshold = 83;
	// Rows at the foot that stand on the ground.
	constexpr int32 SupportRows = 2;
	// A solid texel with broken-through texels within this many texels on two opposite sides is a sliver (under
	// about 18 cm at 6 cm texels): concrete that thin does not hang on, it falls with the hole.
	constexpr int32 SliverReachTexels = 2;
	// The V of a breach starts at this share of the wall's height when that is lower than BreachVStartShare x hull.
	constexpr float BreachVStartWallShare = 0.5f;
	// Breach top edge and foot stub use a narrower jag band than the sides.
	constexpr float BreachTopBandShare = 0.5f;
	constexpr float BreachStubBandShare = 0.35f;
	// The V starts a little below the hull's top: the hull shoulders crack the wall there first.
	constexpr float BreachVStartShare = 0.8f;
	// Exit spall: walls this thin against the shell (thickness / radius) blow out a wider crater at the back.
	constexpr float ThinWallThicknessShare = 0.5f;
	constexpr float ExitSpallShare = 1.15f;
}

void FWallHoleField::Init(const TArray<FVector2D>& InPathPoints, const float InThickness, const float ZMin,
                          const float ZMax, const float InTexelSize, const int32 MaxTexels)
{
	using namespace WallHoleFieldPrivate;
	BuildPath(InPathPoints);
	M_Thickness = FMath::Max(InThickness, 1.f);
	M_ZMin = ZMin;
	M_ZMax = FMath::Max(ZMax, ZMin + MinTexelSize);
	M_UMin = 0.f;
	const int32 TexelCap = FMath::Max(MaxTexels, 2);
	M_TexelSize = FMath::Max3(InTexelSize, M_PathLength / TexelCap, (M_ZMax - M_ZMin) / TexelCap);
	M_TexelSize = FMath::Max(M_TexelSize, MinTexelSize);
	M_Width = FMath::Clamp(FMath::CeilToInt32(M_PathLength / M_TexelSize), 2, TexelCap);
	M_Height = FMath::Clamp(FMath::CeilToInt32((M_ZMax - M_ZMin) / M_TexelSize), 2, TexelCap);
	// Allocated on the first write (EnsureTexels); until then every reader sees intact wall.
	M_Texels.Reset();
}

void FWallHoleField::BuildPath(const TArray<FVector2D>& InPathPoints)
{
	using namespace WallHoleFieldPrivate;
	M_PathPoints.Reset();
	M_PathDistances.Reset();
	for (const FVector2D& Point : InPathPoints)
	{
		if (M_PathPoints.Num() == WallHoleFieldConstants::MaxPathPoints)
		{
			break;
		}
		// Coincident points would make a zero-length segment the shader divides by.
		if (not M_PathPoints.IsEmpty() && FVector2D::Distance(M_PathPoints.Last(), Point) < MinSegmentLength)
		{
			continue;
		}
		M_PathPoints.Add(Point);
	}
	if (M_PathPoints.Num() < 2)
	{
		M_PathPoints.Reset();
		M_PathPoints.Add(FVector2D::ZeroVector);
		M_PathPoints.Add(FVector2D(100.0, 0.0));
	}
	float Distance = 0.f;
	M_PathDistances.Add(0.f);
	for (int32 PointIndex = 1; PointIndex < M_PathPoints.Num(); ++PointIndex)
	{
		Distance += static_cast<float>(FVector2D::Distance(M_PathPoints[PointIndex - 1], M_PathPoints[PointIndex]));
		M_PathDistances.Add(Distance);
	}
	M_PathLength = Distance;
}

void FWallHoleField::ProjectToPath(const FVector& MeshPoint, float& OutU, float& OutOffset) const
{
	const FVector2D Point(MeshPoint.X, MeshPoint.Y);
	float BestDistanceSquared = TNumericLimits<float>::Max();
	OutU = 0.f;
	OutOffset = 0.f;
	// Same rule as the material: the nearest segment wins; the offset sign is the side of its direction.
	for (int32 SegmentIndex = 0; SegmentIndex + 1 < M_PathPoints.Num(); ++SegmentIndex)
	{
		const FVector2D Start = M_PathPoints[SegmentIndex];
		const FVector2D Direction = M_PathPoints[SegmentIndex + 1] - Start;
		const double LengthSquared = FMath::Max(Direction.SizeSquared(), UE_KINDA_SMALL_NUMBER);
		const double Along = FMath::Clamp(FVector2D::DotProduct(Point - Start, Direction) / LengthSquared, 0.0, 1.0);
		const FVector2D Closest = Start + Direction * Along;
		const float DistanceSquared = static_cast<float>(FVector2D::DistSquared(Point, Closest));
		if (DistanceSquared >= BestDistanceSquared)
		{
			continue;
		}
		BestDistanceSquared = DistanceSquared;
		const double Length = FMath::Sqrt(LengthSquared);
		OutU = M_PathDistances[SegmentIndex] + static_cast<float>(Along * Length);
		OutOffset = static_cast<float>(FVector2D::CrossProduct(Direction, Point - Start) / Length);
	}
}

FVector FWallHoleField::GetMeshPoint(const float U, const float Z, const float FaceSide, FVector& OutFaceNormal) const
{
	int32 SegmentIndex = 0;
	while (SegmentIndex + 2 < M_PathPoints.Num() && U > M_PathDistances[SegmentIndex + 1])
	{
		++SegmentIndex;
	}
	const FVector2D Start = M_PathPoints[SegmentIndex];
	const FVector2D Direction = (M_PathPoints[SegmentIndex + 1] - Start).GetSafeNormal();
	const FVector2D FrontNormal(-Direction.Y, Direction.X);
	const FVector2D OnPath = Start + Direction * (U - M_PathDistances[SegmentIndex]);
	const FVector2D OnFace = OnPath + FrontNormal * (FaceSide * M_Thickness * 0.5f);
	OutFaceNormal = FVector(FrontNormal.X, FrontNormal.Y, 0.0);
	return FVector(OnFace.X, OnFace.Y, Z);
}

bool FWallHoleField::GetFootprintInterval(const FVector2D& Centre, const FVector2D& AxisX, const FVector2D& AxisY,
                                          const FVector2D& HalfExtent, float& OutUMin, float& OutUMax) const
{
	OutUMin = TNumericLimits<float>::Max();
	OutUMax = TNumericLimits<float>::Lowest();
	for (int32 SegmentIndex = 0; SegmentIndex + 1 < M_PathPoints.Num(); ++SegmentIndex)
	{
		// The segment in the footprint's frame, clipped to its box (Liang-Barsky).
		const FVector2D StartOffset = M_PathPoints[SegmentIndex] - Centre;
		const FVector2D EndOffset = M_PathPoints[SegmentIndex + 1] - Centre;
		const FVector2D LocalStart(FVector2D::DotProduct(StartOffset, AxisX), FVector2D::DotProduct(StartOffset, AxisY));
		const FVector2D LocalDelta = FVector2D(FVector2D::DotProduct(EndOffset, AxisX),
		                                       FVector2D::DotProduct(EndOffset, AxisY)) - LocalStart;
		double ClipFrom = 0.0;
		double ClipTo = 1.0;
		bool bIsInside = true;
		for (int32 Axis = 0; Axis < 2 && bIsInside; ++Axis)
		{
			const double Start = Axis == 0 ? LocalStart.X : LocalStart.Y;
			const double Delta = Axis == 0 ? LocalDelta.X : LocalDelta.Y;
			const double Extent = Axis == 0 ? HalfExtent.X : HalfExtent.Y;
			if (FMath::Abs(Delta) < UE_KINDA_SMALL_NUMBER)
			{
				bIsInside = FMath::Abs(Start) <= Extent;
				continue;
			}
			const double EnterA = (-Extent - Start) / Delta;
			const double EnterB = (Extent - Start) / Delta;
			ClipFrom = FMath::Max(ClipFrom, FMath::Min(EnterA, EnterB));
			ClipTo = FMath::Min(ClipTo, FMath::Max(EnterA, EnterB));
			bIsInside = ClipFrom <= ClipTo;
		}
		if (not bIsInside)
		{
			continue;
		}
		const float SegmentLength = M_PathDistances[SegmentIndex + 1] - M_PathDistances[SegmentIndex];
		OutUMin = FMath::Min(OutUMin, M_PathDistances[SegmentIndex] + static_cast<float>(ClipFrom) * SegmentLength);
		OutUMax = FMath::Max(OutUMax, M_PathDistances[SegmentIndex] + static_cast<float>(ClipTo) * SegmentLength);
	}
	return OutUMin <= OutUMax;
}

void FWallHoleField::GetSegment(const int32 SegmentIndex, FVector2D& OutStart, FVector2D& OutEnd, float& OutUStart,
                                float& OutUEnd) const
{
	const int32 Index = FMath::Clamp(SegmentIndex, 0, FMath::Max(0, M_PathPoints.Num() - 2));
	OutStart = M_PathPoints[Index];
	OutEnd = M_PathPoints[Index + 1];
	OutUStart = M_PathDistances[Index];
	OutUEnd = M_PathDistances[Index + 1];
}

float FWallHoleField::GetThroughRadius(const float Radius) const
{
	using namespace WallHoleFieldPrivate;
	if (Radius <= M_Thickness)
	{
		return 0.f;
	}
	if (M_Thickness <= Radius * ThinWallThicknessShare)
	{
		return Radius * ExitSpallShare;
	}
	return FMath::Sqrt(FMath::Square(Radius) - FMath::Square(M_Thickness));
}

void FWallHoleField::Clear()
{
	const FColor Untouched(EncodeField(WallHoleFieldConstants::FieldMin), EncodeField(WallHoleFieldConstants::FieldMin),
	                       EncodeField(WallHoleFieldConstants::FieldMin), 255);
	for (FColor& Texel : M_Texels)
	{
		Texel = Untouched;
	}
}

template <typename FieldFunction>
void FWallHoleField::WriteMax(const int32 Channel, const float UFrom, const float UTo, const float ZFrom,
                              const float ZTo, FieldFunction&& GetField)
{
	EnsureTexels();
	const int32 ColumnFrom = FMath::Clamp(FMath::FloorToInt32((UFrom - M_UMin) / M_TexelSize), 0, M_Width - 1);
	const int32 ColumnTo = FMath::Clamp(FMath::CeilToInt32((UTo - M_UMin) / M_TexelSize), 0, M_Width - 1);
	const int32 RowFrom = FMath::Clamp(FMath::FloorToInt32((ZFrom - M_ZMin) / M_TexelSize), 0, M_Height - 1);
	const int32 RowTo = FMath::Clamp(FMath::CeilToInt32((ZTo - M_ZMin) / M_TexelSize), 0, M_Height - 1);
	for (int32 Row = RowFrom; Row <= RowTo; ++Row)
	{
		for (int32 Column = ColumnFrom; Column <= ColumnTo; ++Column)
		{
			const uint8 Encoded = EncodeField(GetField(GetTexelU(Column), GetTexelZ(Row)));
			FColor& Texel = M_Texels[Column + Row * M_Width];
			uint8& Stored = Channel == WallHoleFieldConstants::FrontChannel
				                ? Texel.R
				                : (Channel == WallHoleFieldConstants::BackChannel ? Texel.G : Texel.B);
			Stored = FMath::Max(Stored, Encoded);
		}
	}
}

void FWallHoleField::WriteHoleChannel(const int32 Channel, const float U, const float Z, const float Radius,
                                      const uint32 ShapeSeed)
{
	using namespace WallHoleFieldConstants;
	if (Radius <= 0.f)
	{
		return;
	}
	// Corners of the jittered polygon (unit radius), the same for both faces of one hole.
	const FRandomStream Stream(static_cast<int32>(ShapeSeed));
	const float Rotation = Stream.FRand() * UE_TWO_PI;
	TStaticArray<FVector2D, HolePolygonCorners> Corners;
	for (int32 CornerIndex = 0; CornerIndex < HolePolygonCorners; ++CornerIndex)
	{
		const float Angle = Rotation + UE_TWO_PI * CornerIndex / HolePolygonCorners;
		const float CornerRadius = 1.f + Stream.FRandRange(-HolePolygonJitter, HolePolygonJitter);
		Corners[CornerIndex] = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * CornerRadius;
	}
	// f reaches its minimum (-1) one edge scale outside the widest corner.
	const float Reach = Radius * (1.f + HolePolygonJitter) + HoleEdgeScale;
	WriteMax(Channel, U - Reach, U + Reach, Z - Reach, Z + Reach,
	         [U, Z, Radius, Rotation, Corners](const float TexelU, const float TexelZ)
	         {
		         const FVector2D Offset(TexelU - U, TexelZ - Z);
		         const double Distance = Offset.Size();
		         if (Distance < KINDA_SMALL_NUMBER)
		         {
			         return FieldMax;
		         }
		         // The polygon edge in this direction: the ray from the centre meets the side between two corners.
		         const FVector2D Direction = Offset / Distance;
		         const float Angle = FMath::Fmod(FMath::Atan2(Direction.Y, Direction.X) - Rotation + 2.f * UE_TWO_PI,
		                                         UE_TWO_PI);
		         const int32 Side = FMath::Clamp(FMath::FloorToInt32(Angle / (UE_TWO_PI / HolePolygonCorners)), 0,
		                                         HolePolygonCorners - 1);
		         const FVector2D& From = Corners[Side];
		         const FVector2D Edge = Corners[(Side + 1) % HolePolygonCorners] - From;
		         const double Denominator = FVector2D::CrossProduct(Direction, Edge);
		         const double EdgeRadius = FMath::Abs(Denominator) > KINDA_SMALL_NUMBER
			                                   ? FVector2D::CrossProduct(From, Edge) / Denominator
			                                   : From.Size();
		         return static_cast<float>((Radius * EdgeRadius - Distance) / HoleEdgeScale);
	         });
}

void FWallHoleField::AddHole(const FWallHoleFieldHole& Hole, const bool bIsFrontSide)
{
	using namespace WallHoleFieldConstants;
	const uint32 ShapeSeed = HashCombine(GetTypeHash(FMath::RoundToInt32(Hole.U * 10.f)),
	                                     GetTypeHash(FMath::RoundToInt32(Hole.Z * 10.f)));
	WriteHoleChannel(bIsFrontSide ? FrontChannel : BackChannel, Hole.U, Hole.Z, Hole.FrontRadius, ShapeSeed);
	WriteHoleChannel(bIsFrontSide ? BackChannel : FrontChannel, Hole.U, Hole.Z, Hole.BackRadius, ShapeSeed);
}

float FWallHoleField::GetBreachVStart(const FWallHoleFieldBreach& Breach) const
{
	using namespace WallHoleFieldPrivate;
	return FMath::Min(Breach.HullHeight * BreachVStartShare, (M_ZMax - Breach.GroundZ) * BreachVStartWallShare);
}

float FWallHoleField::GetBreachTopHalfWidth(const FWallHoleFieldBreach& Breach) const
{
	const float BaseHalfWidth = FMath::Max((Breach.UMax - Breach.UMin) * 0.5f, 1.f);
	const float VStart = GetBreachVStart(Breach);
	return BaseHalfWidth + FMath::Max(0.f, M_ZMax - Breach.GroundZ - VStart) * Breach.VSlope;
}

void FWallHoleField::AddBreach(const FWallHoleFieldBreach& Breach)
{
	using namespace WallHoleFieldPrivate;
	const float Centre = (Breach.UMin + Breach.UMax) * 0.5f;
	const float BaseHalfWidth = FMath::Max((Breach.UMax - Breach.UMin) * 0.5f, 1.f);
	const float EdgeBand = FMath::Max(Breach.EdgeBand, 1.f);
	const float WallTop = M_ZMax - Breach.GroundZ;
	const float VStart = GetBreachVStart(Breach);
	const float TopHalfWidth = BaseHalfWidth + FMath::Max(0.f, WallTop - VStart) * Breach.VSlope * Breach.TopCollapse;
	const bool bHasTopFallen = Breach.TopCollapse > 0.f;
	auto GetBreachField = [=](const float TexelU, const float TexelZ)
	{
		const float Height = TexelZ - Breach.GroundZ;
		const float HalfWidth = BaseHalfWidth + FMath::Max(0.f, Height - VStart) * Breach.VSlope * Breach.TopCollapse;
		const float SideField = (HalfWidth - FMath::Abs(TexelU - Centre)) / EdgeBand;
		// Until the wall above falls, the gap is as tall as the hull.
		const float TopField = bHasTopFallen
			                       ? WallHoleFieldConstants::FieldMax
			                       : (Breach.HullHeight - Height) / (EdgeBand * BreachTopBandShare);
		const float StubField = (Height - Breach.StubHeight) / (EdgeBand * BreachStubBandShare);
		return FMath::Min3(SideField, TopField, StubField);
	};
	WriteMax(WallHoleFieldConstants::BreachChannel, Centre - TopHalfWidth - EdgeBand, Centre + TopHalfWidth + EdgeBand,
	         M_ZMin, M_ZMax, GetBreachField);
}

void FWallHoleField::EnsureTexels()
{
	if (M_Texels.Num() == M_Width * M_Height)
	{
		return;
	}
	M_Texels.SetNumZeroed(M_Width * M_Height);
	Clear();
}

void FWallHoleField::FindUnsupportedIslands(TArray<FWallHoleFieldIsland>& OutIslands) const
{
	using namespace WallHoleFieldPrivate;
	OutIslands.Reset();
	if (not GetIsInitialised() || M_Texels.Num() != M_Width * M_Height)
	{
		return;
	}
	const int32 TexelCount = M_Width * M_Height;
	// Solid for support: not broken through on both faces and not in a breach, with a margin beyond the hole edge so
	// a bridge the shader's edge noise eats away does not hold anything up.
	TBitArray<> Solid(false, TexelCount);
	for (int32 Index = 0; Index < TexelCount; ++Index)
	{
		const FColor& Texel = M_Texels[Index];
		const bool bIsThrough = Texel.R >= EncodedSupportThreshold && Texel.G >= EncodedSupportThreshold;
		Solid[Index] = not bIsThrough && Texel.B < EncodedSupportThreshold;
	}
	// Slivers: solid texels with broken-through texels close by on two opposite sides fall even while still joined.
	FWallHoleFieldIsland Slivers;
	Slivers.bIsSlivers = true;
	FWallHoleFieldIsland BreachSlivers;
	BreachSlivers.bIsSlivers = true;
	BreachSlivers.bTouchesBreach = true;
	const TBitArray<> SolidBefore = Solid;
	const auto IsOpenAt = [this, &SolidBefore](const int32 Column, const int32 Row)
	{
		// Outside the field is wall that is not drawn here (ends, the top): it holds, it is not open.
		return Column >= 0 && Column < M_Width && Row >= 0 && Row < M_Height
			&& not SolidBefore[Column + Row * M_Width];
	};
	for (int32 Index = 0; Index < TexelCount; ++Index)
	{
		if (not SolidBefore[Index])
		{
			continue;
		}
		const int32 Column = Index % M_Width;
		const int32 Row = Index / M_Width;
		bool bOpenLeft = false;
		bool bOpenRight = false;
		bool bOpenBelow = false;
		bool bOpenAbove = false;
		for (int32 Step = 1; Step <= SliverReachTexels; ++Step)
		{
			bOpenLeft = bOpenLeft || IsOpenAt(Column - Step, Row);
			bOpenRight = bOpenRight || IsOpenAt(Column + Step, Row);
			bOpenBelow = bOpenBelow || IsOpenAt(Column, Row - Step);
			bOpenAbove = bOpenAbove || IsOpenAt(Column, Row + Step);
		}
		if ((bOpenLeft && bOpenRight) || (bOpenBelow && bOpenAbove))
		{
			Solid[Index] = false;
			(GetIsNearBreach(Index, SliverReachTexels) ? BreachSlivers : Slivers).Texels.Add(Index);
		}
	}
	Slivers.Area = Slivers.Texels.Num() * FMath::Square(M_TexelSize);
	BreachSlivers.Area = BreachSlivers.Texels.Num() * FMath::Square(M_TexelSize);
	TBitArray<> Reached(false, TexelCount);
	TArray<int32> Stack;
	const auto Flood = [this, &Solid, &Reached, &Stack](TArray<int32>* const OutTexels)
	{
		while (not Stack.IsEmpty())
		{
			const int32 Index = Stack.Pop(EAllowShrinking::No);
			if (OutTexels != nullptr)
			{
				OutTexels->Add(Index);
			}
			const int32 Column = Index % M_Width;
			const int32 Row = Index / M_Width;
			const int32 Neighbours[4] = {
				Column > 0 ? Index - 1 : INDEX_NONE, Column < M_Width - 1 ? Index + 1 : INDEX_NONE,
				Row > 0 ? Index - M_Width : INDEX_NONE, Row < M_Height - 1 ? Index + M_Width : INDEX_NONE
			};
			for (const int32 Neighbour : Neighbours)
			{
				if (Neighbour != INDEX_NONE && Solid[Neighbour] && not Reached[Neighbour])
				{
					Reached[Neighbour] = true;
					Stack.Add(Neighbour);
				}
			}
		}
	};
	// Held up from the foot: the lowest rows stand on the ground.
	for (int32 Row = 0; Row < FMath::Min(SupportRows, M_Height); ++Row)
	{
		for (int32 Column = 0; Column < M_Width; ++Column)
		{
			const int32 Index = Column + Row * M_Width;
			if (Solid[Index] && not Reached[Index])
			{
				Reached[Index] = true;
				Stack.Add(Index);
			}
		}
	}
	Flood(nullptr);
	for (int32 Index = 0; Index < TexelCount; ++Index)
	{
		if (not Solid[Index] || Reached[Index])
		{
			continue;
		}
		FWallHoleFieldIsland& Island = OutIslands.AddDefaulted_GetRef();
		Reached[Index] = true;
		Stack.Add(Index);
		Flood(&Island.Texels);
		double SumU = 0.0;
		double SumZ = 0.0;
		for (const int32 TexelIndex : Island.Texels)
		{
			SumU += GetTexelU(TexelIndex % M_Width);
			SumZ += GetTexelZ(TexelIndex / M_Width);
		}
		const int32 Count = FMath::Max(Island.Texels.Num(), 1);
		Island.CentreU = static_cast<float>(SumU / Count);
		Island.CentreZ = static_cast<float>(SumZ / Count);
		Island.Area = Island.Texels.Num() * FMath::Square(M_TexelSize);
		constexpr int32 IslandBreachReach = 1;
		Island.bTouchesBreach = Island.Texels.ContainsByPredicate([this](const int32 TexelIndex)
		{
			return GetIsNearBreach(TexelIndex, IslandBreachReach);
		});
	}
	if (not Slivers.Texels.IsEmpty())
	{
		OutIslands.Add(MoveTemp(Slivers));
	}
	if (not BreachSlivers.Texels.IsEmpty())
	{
		OutIslands.Add(MoveTemp(BreachSlivers));
	}
}

void FWallHoleField::RemoveIsland(const FWallHoleFieldIsland& Island)
{
	for (const int32 TexelIndex : Island.Texels)
	{
		if (M_Texels.IsValidIndex(TexelIndex))
		{
			M_Texels[TexelIndex].R = MAX_uint8;
			M_Texels[TexelIndex].G = MAX_uint8;
			if (Island.bTouchesBreach)
			{
				M_Texels[TexelIndex].B = MAX_uint8;
			}
		}
	}
}

bool FWallHoleField::GetIsNearBreach(const int32 TexelIndex, const int32 Reach) const
{
	using namespace WallHoleFieldPrivate;
	if (not M_Texels.IsValidIndex(TexelIndex))
	{
		return false;
	}
	const int32 Column = TexelIndex % M_Width;
	const int32 Row = TexelIndex / M_Width;
	for (int32 Step = 1; Step <= Reach; ++Step)
	{
		const int32 Neighbours[4][2] = {{Column - Step, Row}, {Column + Step, Row}, {Column, Row - Step},
		                                {Column, Row + Step}};
		for (const int32* const Neighbour : Neighbours)
		{
			const bool bIsInField = Neighbour[0] >= 0 && Neighbour[0] < M_Width && Neighbour[1] >= 0
				&& Neighbour[1] < M_Height;
			if (bIsInField && M_Texels[Neighbour[0] + Neighbour[1] * M_Width].B >= EncodedRemovedThreshold)
			{
				return true;
			}
		}
	}
	return false;
}

uint8 FWallHoleField::GetChannel(const int32 Column, const int32 Row, const int32 Channel) const
{
	if (M_Texels.Num() != M_Width * M_Height)
	{
		// Never damaged: intact everywhere.
		return EncodeField(WallHoleFieldConstants::FieldMin);
	}
	const FColor& Texel = M_Texels[FMath::Clamp(Column, 0, M_Width - 1) + FMath::Clamp(Row, 0, M_Height - 1) * M_Width];
	return Channel == WallHoleFieldConstants::FrontChannel
		       ? Texel.R
		       : (Channel == WallHoleFieldConstants::BackChannel ? Texel.G : Texel.B);
}

bool FWallHoleField::GetIsRemoved(const float U, const float Z, const bool bIsFrontSide) const
{
	using namespace WallHoleFieldConstants;
	if (not GetIsInitialised())
	{
		return false;
	}
	const int32 Column = FMath::FloorToInt32((U - M_UMin) / M_TexelSize);
	const int32 Row = FMath::FloorToInt32((Z - M_ZMin) / M_TexelSize);
	return GetChannel(Column, Row, bIsFrontSide ? FrontChannel : BackChannel)
		>= WallHoleFieldPrivate::EncodedRemovedThreshold
		|| GetChannel(Column, Row, BreachChannel) >= WallHoleFieldPrivate::EncodedRemovedThreshold;
}

bool FWallHoleField::GetIsInBreach(const float U, const float Z) const
{
	if (not GetIsInitialised())
	{
		return false;
	}
	const int32 Column = FMath::FloorToInt32((U - M_UMin) / M_TexelSize);
	const int32 Row = FMath::FloorToInt32((Z - M_ZMin) / M_TexelSize);
	return GetChannel(Column, Row, WallHoleFieldConstants::BreachChannel) >= WallHoleFieldPrivate::EncodedRemovedThreshold;
}

float FWallHoleField::GetRemovedArea(const bool bIncludeBreaches) const
{
	using namespace WallHoleFieldPrivate;
	int32 RemovedTexels = 0;
	for (const FColor& Texel : M_Texels)
	{
		const bool bIsBreach = Texel.B >= EncodedRemovedThreshold;
		// Either face: a back-face hole that did not go through is as much damage as a front one.
		const bool bIsHole = Texel.R >= EncodedRemovedThreshold || Texel.G >= EncodedRemovedThreshold;
		if (bIncludeBreaches ? (bIsHole || bIsBreach) : (bIsHole && not bIsBreach))
		{
			++RemovedTexels;
		}
	}
	return RemovedTexels * FMath::Square(M_TexelSize);
}

void FWallHoleField::Upload(TObjectPtr<UTexture2D>& InOutTexture) const
{
	using namespace WallHoleFieldPrivate;
	if (not GetIsInitialised() || M_Texels.Num() != M_Width * M_Height)
	{
		return;
	}
	const int32 ByteCount = M_Width * M_Height * BytesPerTexel;
	if (not IsValid(InOutTexture))
	{
		// CreateTransient puts the texture in the transient package: a name made unique in Outer could collide there and
		// replace another wall's texture, so the engine picks the name.
		UTexture2D* const Texture = UTexture2D::CreateTransient(M_Width, M_Height, PF_B8G8R8A8, NAME_None);
		if (not IsValid(Texture))
		{
			return;
		}
		Texture->SRGB = false;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->NeverStream = true;
		Texture->CompressionSettings = TC_VectorDisplacementmap;
		if (FTexturePlatformData* const PlatformData = Texture->GetPlatformData())
		{
			if (not PlatformData->Mips.IsEmpty())
			{
				void* const MipData = PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(MipData, M_Texels.GetData(), ByteCount);
				PlatformData->Mips[0].BulkData.Unlock();
			}
		}
		Texture->UpdateResource();
		InOutTexture = Texture;
		return;
	}
	// The render thread reads the copy later; it frees it when done.
	uint8* const DataCopy = static_cast<uint8*>(FMemory::Malloc(ByteCount));
	FMemory::Memcpy(DataCopy, M_Texels.GetData(), ByteCount);
	FUpdateTextureRegion2D* const Region = new FUpdateTextureRegion2D(0, 0, 0, 0, M_Width, M_Height);
	InOutTexture->UpdateTextureRegions(0, 1, Region, M_Width * BytesPerTexel, BytesPerTexel, DataCopy,
	                                   [](uint8* const SourceData, const FUpdateTextureRegion2D* const Regions)
	                                   {
		                                   FMemory::Free(SourceData);
		                                   delete Regions;
	                                   });
}

FLinearColor FWallHoleField::GetPathAParameter() const
{
	return FLinearColor(M_PathPoints[0].X, M_PathPoints[0].Y, M_PathPoints[1].X, M_PathPoints[1].Y);
}

FLinearColor FWallHoleField::GetPathBParameter() const
{
	const bool bHasSecondSegment = M_PathPoints.Num() > 2;
	const FVector2D Third = bHasSecondSegment ? M_PathPoints[2] : M_PathPoints[1];
	return FLinearColor(Third.X, Third.Y, bHasSecondSegment ? 2.f : 1.f, M_PathDistances[1]);
}

FLinearColor FWallHoleField::GetMapParameter() const
{
	return FLinearColor(M_UMin, M_ZMin, 1.f / (M_Width * M_TexelSize), 1.f / (M_Height * M_TexelSize));
}

uint8 FWallHoleField::EncodeField(const float Field)
{
	const float Clamped = FMath::Clamp(Field, WallHoleFieldConstants::FieldMin, WallHoleFieldConstants::FieldMax);
	return static_cast<uint8>(FMath::RoundToInt32((Clamped * 0.5f + 0.5f) * 255.f));
}
