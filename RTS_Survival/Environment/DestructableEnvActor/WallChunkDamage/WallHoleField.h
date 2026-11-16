// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UTexture2D;

namespace WallHoleFieldConstants
{
	// The material unrolls the wall along at most this many path points (two segments: straight walls and L corners).
	inline constexpr int32 MaxPathPoints = 3;
	// Channels of the field texture: front face (path left side), back face, tank breach (both faces).
	inline constexpr int32 FrontChannel = 0;
	inline constexpr int32 BackChannel = 1;
	inline constexpr int32 BreachChannel = 2;
	// Field values are stored as f in [-1, 1] -> byte (f * 0.5 + 0.5) * 255; f > 0 is removed.
	inline constexpr float FieldMin = -1.f;
	inline constexpr float FieldMax = 1.f;
	// A hole's rim reaches to 1.5 radii (f = -0.5); the rest of the field stays at its minimum.
	inline constexpr float HoleRimReachShare = 1.5f;
	// Material parameter names (Tools/WallChunks/make_wall_field_holes_material.py).
	inline const FName TextureParameter = TEXT("WallFieldTexture");
	inline const FName PathAParameter = TEXT("WallFieldPathA");
	inline const FName PathBParameter = TEXT("WallFieldPathB");
	inline const FName MapParameter = TEXT("WallFieldMap");
}

/** One tank breach: the stretch of the unrolled wall a hull crossed and how far the wall above it has fallen. */
struct FWallHoleFieldBreach
{
	// Along-wall interval the hull covered (cm along the path).
	float UMin = 0.f;
	float UMax = 0.f;
	// Mesh-space Z of the ground under the breach and the hull's height above it.
	float GroundZ = 0.f;
	float HullHeight = 250.f;
	// 0: only the hull-sized gap; 1: the wall above has fallen in a V up to the top.
	float TopCollapse = 0.f;
	// Widening of the V per cm above the hull (each side), and the jagged edge band.
	float VSlope = 0.45f;
	float EdgeBand = 110.f;
	// A ragged stub of wall that stays at the foot of the breach.
	float StubHeight = 18.f;
};

/** One shell hole on the unrolled wall: centre along the path, height, and the radius on each face. */
struct FWallHoleFieldHole
{
	float U = 0.f;
	float Z = 0.f;
	float FrontRadius = 0.f;
	float BackRadius = 0.f;
};

/**
 * @brief Wall damage as a small CPU-written texture of the unrolled wall face instead of hole slots, so any number of
 * holes stay apart and the shader pays one sample. Owned by UWallHoleDamageComponent in field mode.
 */
class RTS_SURVIVAL_API FWallHoleField
{
public:
	/**
	 * @brief Lays the field over the wall: the path the face unrolls along and the texel grid.
	 * @param InPathPoints Mesh-space XY centre line of the wall, 2 or 3 points.
	 * @param InThickness Wall thickness; a shell punches through to the other face when its radius exceeds it.
	 * @param ZMin Mesh-space bottom of the wall.
	 * @param ZMax Mesh-space top of the wall.
	 * @param InTexelSize Field resolution in cm.
	 * @param MaxTexels Field width and height cap.
	 */
	void Init(const TArray<FVector2D>& InPathPoints, float InThickness, float ZMin, float ZMax, float InTexelSize,
	          int32 MaxTexels);

	bool GetIsInitialised() const { return M_Width > 0; }

	/**
	 * @brief Where a mesh-space point lies on the unrolled wall.
	 * @param MeshPoint Point in the wall mesh's space.
	 * @param OutU Distance along the path.
	 * @param OutOffset Signed distance from the centre line; positive is the front face.
	 */
	void ProjectToPath(const FVector& MeshPoint, float& OutU, float& OutOffset) const;

	/**
	 * @brief Mesh-space point on one face of the unrolled wall.
	 * @param U Distance along the path.
	 * @param Z Mesh-space height.
	 * @param FaceSide +1 front face, -1 back face, 0 the centre line.
	 * @param OutFaceNormal Normal of the front face there (mesh space).
	 */
	FVector GetMeshPoint(float U, float Z, float FaceSide, FVector& OutFaceNormal) const;

	/**
	 * @brief Along-wall interval a footprint (a tank hull seen from above) covers where it crosses the centre line.
	 * @param Centre Footprint centre, mesh XY.
	 * @param AxisX Footprint forward axis (unit), mesh XY.
	 * @param AxisY Footprint right axis (unit), mesh XY.
	 * @param HalfExtent Half length and half width of the footprint.
	 * @param OutUMin Receives the start of the interval along the path.
	 * @param OutUMax Receives the end of the interval along the path.
	 * @return False when the footprint does not reach the centre line.
	 */
	bool GetFootprintInterval(const FVector2D& Centre, const FVector2D& AxisX, const FVector2D& AxisY,
	                          const FVector2D& HalfExtent, float& OutUMin, float& OutUMax) const;

	float GetPathLength() const { return M_PathLength; }
	int32 GetSegmentCount() const { return FMath::Max(0, M_PathPoints.Num() - 1); }

	/**
	 * @brief One straight run of the wall's centre line.
	 * @param SegmentIndex 0 .. GetSegmentCount() - 1.
	 * @param OutStart Mesh-space XY start.
	 * @param OutEnd Mesh-space XY end.
	 * @param OutUStart Distance along the path at the start.
	 * @param OutUEnd Distance along the path at the end.
	 */
	void GetSegment(int32 SegmentIndex, FVector2D& OutStart, FVector2D& OutEnd, float& OutUStart, float& OutUEnd) const;
	float GetThickness() const { return M_Thickness; }
	float GetZMin() const { return M_ZMin; }
	float GetZMax() const { return M_ZMax; }

	/**
	 * @brief Exit hole of a shell on the far face. A thin wall spalls a wider crater at the back than the entry hole;
	 * a thick one (sandbags) is only punched through by shells wider than it is thick, and then narrower.
	 * @param Radius Entry hole radius.
	 * @return Exit radius; zero when the shell does not get through.
	 */
	float GetThroughRadius(float Radius) const;

	/** @return Half width of a breach at the top of the wall once its top has fully fallen (V included). */
	float GetBreachTopHalfWidth(const FWallHoleFieldBreach& Breach) const;

	void Clear();
	void AddHole(const FWallHoleFieldHole& Hole, bool bIsFrontSide);
	void AddBreach(const FWallHoleFieldBreach& Breach);

	/** @return True when the face at this unrolled point is already gone (hole on that face or breach). */
	bool GetIsRemoved(float U, float Z, bool bIsFrontSide) const;

	/** @return True when a tank breach has removed the wall at this unrolled point. */
	bool GetIsInBreach(float U, float Z) const;

	/**
	 * @return Removed area of the wall face, cm²: shell holes on the front face, plus the breaches when asked.
	 * @param bIncludeBreaches False counts shell holes only (the shell hole budget ignores what tanks broke).
	 */
	float GetRemovedArea(bool bIncludeBreaches = true) const;

	/** @return Face area of the unrolled wall, cm². */
	float GetFaceArea() const { return M_PathLength * (M_ZMax - M_ZMin); }

	/**
	 * @brief Creates the texture on first use and uploads the field.
	 * @param InOutTexture The component's texture; created (transient) when null.
	 */
	void Upload(TObjectPtr<UTexture2D>& InOutTexture) const;

	// Material parameters: path points (A: P0, P1; B: P2, segment count) and the texel map (UMin, ZMin, 1/W, 1/H).
	FLinearColor GetPathAParameter() const;
	FLinearColor GetPathBParameter() const;
	FLinearColor GetMapParameter() const;

private:
	void BuildPath(const TArray<FVector2D>& InPathPoints);
	// Writes max(existing, f) into one channel for every texel in the U/Z rectangle, f from the callback.
	template <typename FieldFunction>
	void WriteMax(int32 Channel, float UFrom, float UTo, float ZFrom, float ZTo, FieldFunction&& GetField);
	void WriteHoleChannel(int32 Channel, float U, float Z, float Radius);
	float GetTexelU(int32 Column) const { return M_UMin + (Column + 0.5f) * M_TexelSize; }
	float GetTexelZ(int32 Row) const { return M_ZMin + (Row + 0.5f) * M_TexelSize; }
	uint8 GetChannel(int32 Column, int32 Row, int32 Channel) const;
	static uint8 EncodeField(float Field);

	TArray<FVector2D, TInlineAllocator<WallHoleFieldConstants::MaxPathPoints>> M_PathPoints;
	// Path length up to each point.
	TArray<float, TInlineAllocator<WallHoleFieldConstants::MaxPathPoints>> M_PathDistances;
	float M_PathLength = 0.f;
	float M_Thickness = 20.f;
	float M_UMin = 0.f;
	float M_ZMin = 0.f;
	float M_ZMax = 0.f;
	float M_TexelSize = 6.f;
	int32 M_Width = 0;
	int32 M_Height = 0;
	// BGRA texels as the texture stores them: R front, G back, B breach.
	TArray<FColor> M_Texels;
};
