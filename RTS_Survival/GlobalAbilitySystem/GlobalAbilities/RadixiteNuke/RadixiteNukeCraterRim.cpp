// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RadixiteNukeCraterRim.h"

#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "ProceduralMeshComponent.h"
#include "RadixiteNuke.h"

namespace RadixiteNukeCraterRimConstants
{
	// About 1 m between vertices on the crest of a 18 m rim; the landscape has 1 m quads.
	constexpr int32 Segments = 128;
	// Signed place on the slope: -1 = inner foot, 0 = crest, 1 = outer foot. Denser near the crest.
	constexpr float SlopeSamples[] = {-1.f, -0.75f, -0.5f, -0.25f, 0.f, 0.1f, 0.25f, 0.4f, 0.6f, 0.8f, 1.f};
	constexpr int32 SlopeSampleCount = UE_ARRAY_COUNT(SlopeSamples);
	// Skirt ring + slope rings + skirt ring.
	constexpr int32 Rings = SlopeSampleCount + 2;
	// The feet sit a little under the ground so the landscape between two vertices does not show a seam.
	constexpr float FootSinkCm = 30.f;
	// Skirts hang this far under the feet: ground that falls away between samples shows earth, not a gap.
	constexpr float SkirtDepthCm = 300.f;
	// Share of RimIrregularity the crest radius wanders by; the height wanders by the full amount.
	constexpr float RadiusWobbleShare = 0.35f;
	constexpr float OuterRunWobbleShare = 0.5f;
	// Fresh spoil (R) fades out over this part of the slope; the foot blend (G) starts after FootBlendStart.
	constexpr float SpoilFadeSlope = 0.8f;
	constexpr float FootBlendStart = 0.4f;
	constexpr float UVWorldScaleCm = 1000.f;
}

namespace
{
	/** @brief Smooth periodic wobble in [-1, 1] around the ring; whole frequencies so it closes at the seam. */
	struct FRimWobble
	{
		float Phases[3] = {0.f, 0.f, 0.f};

		explicit FRimWobble(FRandomStream& Stream)
		{
			for (float& Phase : Phases)
			{
				Phase = Stream.FRandRange(0.f, UE_TWO_PI);
			}
		}

		float Sample(const float Angle) const
		{
			return 0.5f * FMath::Sin(3.f * Angle + Phases[0])
				+ 0.3f * FMath::Sin(5.f * Angle + Phases[1])
				+ 0.2f * FMath::Sin(11.f * Angle + Phases[2]);
		}
	};

	/** @brief Height of the slope above its feet, 0 at a foot and 1 on the crest. */
	float GetSlopeHeight01(const float SignedSlope)
	{
		if (SignedSlope <= 0.f)
		{
			// The inner wall: an S-curve, steepest halfway down.
			const float FromFoot = 1.f + SignedSlope;
			return FromFoot * FromFoot * (3.f - 2.f * FromFoot);
		}
		// The outer ejecta: steep at the crest, running out flat.
		const float ToFoot = 1.f - SignedSlope;
		return ToFoot * ToFoot;
	}

	FLinearColor GetSlopeColour(const float SignedSlope)
	{
		using namespace RadixiteNukeCraterRimConstants;
		const float SlopeDistance = FMath::Abs(SignedSlope);
		const float FreshSpoil = FMath::Clamp(1.f - SlopeDistance / SpoilFadeSlope, 0.f, 1.f);
		const float FootBlend = FMath::Clamp((SlopeDistance - FootBlendStart) / (1.f - FootBlendStart), 0.f, 1.f);
		return FLinearColor(FreshSpoil, FootBlend, 0.f, 1.f);
	}

	int32 GetVertexIndex(const int32 Segment, const int32 Ring)
	{
		using namespace RadixiteNukeCraterRimConstants;
		return (Segment % Segments) * Rings + Ring;
	}

	struct FRimMeshData
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colours;
	};

	/** @brief One column of vertices from the inner skirt over the crest to the outer skirt, at one angle. */
	void AddRimColumn(FRimMeshData& Mesh, const float Angle, const FRadixiteNukeCrater& Crater,
	                  const FRimWobble& RadiusWobble, const FRimWobble& HeightWobble,
	                  const FRadixiteNukeGroundSampler& Ground, const FVector& Origin)
	{
		using namespace RadixiteNukeCraterRimConstants;
		const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
		const float Irregularity = Crater.RimIrregularity;
		const float CrestRadius = Crater.RimRadius * (1.f + Irregularity * RadiusWobbleShare
			* RadiusWobble.Sample(Angle));
		const float CrestHeight = Crater.RimHeight * (1.f + Irregularity * HeightWobble.Sample(Angle));
		const float InnerRun = Crater.RimRadius * Crater.RimInnerRunFraction;
		const float OuterRun = Crater.RimRadius * Crater.RimOuterRunFraction
			* (1.f + Irregularity * OuterRunWobbleShare * HeightWobble.Sample(Angle + UE_HALF_PI));

		const auto AddVertex = [&](const float Radius, const float HeightAboveGround, const FLinearColor& Colour)
		{
			const FVector2D LocalXY = Direction * Radius;
			const FVector2D WorldXY(Origin.X + LocalXY.X, Origin.Y + LocalXY.Y);
			const float GroundHeight = Ground.GetGroundHeight(WorldXY);
			Mesh.Vertices.Add(FVector(LocalXY.X, LocalXY.Y, GroundHeight + HeightAboveGround - Origin.Z));
			Mesh.UVs.Add(WorldXY / UVWorldScaleCm);
			Mesh.Colours.Add(Colour);
		};

		const FLinearColor SkirtColour(0.f, 1.f, 0.f, 1.f);
		const float InnerFootRadius = FMath::Max(CrestRadius - InnerRun, 0.f);
		AddVertex(InnerFootRadius, -FootSinkCm - SkirtDepthCm, SkirtColour);
		for (const float SignedSlope : SlopeSamples)
		{
			const float Radius = SignedSlope <= 0.f
				                     ? CrestRadius + SignedSlope * InnerRun
				                     : CrestRadius + SignedSlope * OuterRun;
			const float Height01 = GetSlopeHeight01(SignedSlope);
			const float HeightAboveGround = Height01 * CrestHeight - (1.f - Height01) * FootSinkCm;
			AddVertex(FMath::Max(Radius, 0.f), HeightAboveGround, GetSlopeColour(SignedSlope));
		}
		AddVertex(CrestRadius + OuterRun, -FootSinkCm - SkirtDepthCm, SkirtColour);
	}

	/**
	 * Quads between columns. A triangle (A, C, B), with B the next ring and C the next angle, faces (B - A) x (C - A):
	 * up on the slopes, toward the pit on the inner skirt (B above A) and away from it on the outer skirt (B below A).
	 */
	void AddRimTriangles(FRimMeshData& Mesh)
	{
		using namespace RadixiteNukeCraterRimConstants;
		for (int32 Segment = 0; Segment < Segments; ++Segment)
		{
			for (int32 Ring = 0; Ring < Rings - 1; ++Ring)
			{
				const int32 A = GetVertexIndex(Segment, Ring);
				const int32 B = GetVertexIndex(Segment, Ring + 1);
				const int32 C = GetVertexIndex(Segment + 1, Ring);
				const int32 D = GetVertexIndex(Segment + 1, Ring + 1);
				Mesh.Triangles.Append({A, C, B, C, D, B});
			}
		}
	}

	void ComputeSmoothNormals(FRimMeshData& Mesh)
	{
		Mesh.Normals.Init(FVector::ZeroVector, Mesh.Vertices.Num());
		for (int32 First = 0; First + 2 < Mesh.Triangles.Num(); First += 3)
		{
			const int32 A = Mesh.Triangles[First];
			const int32 B = Mesh.Triangles[First + 1];
			const int32 C = Mesh.Triangles[First + 2];
			// Front face of (A, B, C) in Unreal's winding; area weighted.
			const FVector FaceNormal = FVector::CrossProduct(Mesh.Vertices[C] - Mesh.Vertices[A],
			                                                 Mesh.Vertices[B] - Mesh.Vertices[A]);
			Mesh.Normals[A] += FaceNormal;
			Mesh.Normals[B] += FaceNormal;
			Mesh.Normals[C] += FaceNormal;
		}
		for (FVector& Normal : Mesh.Normals)
		{
			Normal = Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		}
	}
}

FRadixiteNukeGroundSampler::FRadixiteNukeGroundSampler(const UWorld& World, const FVector& GroundZero)
	: M_FallbackHeight(GroundZero.Z)
{
	for (TActorIterator<ALandscapeProxy> It(&World); It; ++It)
	{
		M_Landscapes.Add(*It);
	}
}

float FRadixiteNukeGroundSampler::GetGroundHeight(const FVector2D& WorldXY) const
{
	const FVector Probe(WorldXY.X, WorldXY.Y, M_FallbackHeight);
	for (const TWeakObjectPtr<ALandscapeProxy>& Landscape : M_Landscapes)
	{
		if (not Landscape.IsValid())
		{
			continue;
		}
		const TOptional<float> Height = Landscape->GetHeightAtLocation(Probe);
		if (Height.IsSet())
		{
			return Height.GetValue();
		}
	}
	return M_FallbackHeight;
}

void RadixiteNukeCraterRim::BuildRimMesh(UProceduralMeshComponent& RimComponent, const FRadixiteNukeCrater& Crater,
                                         const FRadixiteNukeGroundSampler& Ground, const bool bCreateCollision)
{
	using namespace RadixiteNukeCraterRimConstants;
	const FVector Origin = RimComponent.GetComponentLocation();
	const FIntVector RoundedOrigin(FMath::RoundToInt(Origin.X), FMath::RoundToInt(Origin.Y),
	                               FMath::RoundToInt(Origin.Z));
	FRandomStream Stream(static_cast<int32>(GetTypeHash(RoundedOrigin)));
	const FRimWobble RadiusWobble(Stream);
	const FRimWobble HeightWobble(Stream);

	FRimMeshData Mesh;
	Mesh.Vertices.Reserve(Segments * Rings);
	Mesh.UVs.Reserve(Segments * Rings);
	Mesh.Colours.Reserve(Segments * Rings);
	Mesh.Triangles.Reserve(Segments * (Rings - 1) * 6);
	for (int32 Segment = 0; Segment < Segments; ++Segment)
	{
		const float Angle = UE_TWO_PI * Segment / Segments;
		AddRimColumn(Mesh, Angle, Crater, RadiusWobble, HeightWobble, Ground, Origin);
	}
	AddRimTriangles(Mesh);
	ComputeSmoothNormals(Mesh);
	RimComponent.CreateMeshSection_LinearColor(0, Mesh.Vertices, Mesh.Triangles, Mesh.Normals, Mesh.UVs, Mesh.Colours,
	                                           TArray<FProcMeshTangent>(), bCreateCollision);
}
