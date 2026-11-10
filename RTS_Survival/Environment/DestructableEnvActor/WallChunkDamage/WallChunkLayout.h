// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"

#include "WallChunkLayout.generated.h"

class UMaterialInterface;
class UStaticMesh;

namespace WallChunkMask
{
	// UV channel holding each chunk's index; channel 1 is the generated lightmap.
	inline constexpr int32 ChunkIndexUvChannel = 2;

	// First custom primitive data float of the hidden-chunk mask; 0 is the faction flag, 4-7 the team colour.
	inline constexpr int32 FirstMaskPrimitiveDataIndex = 8;

	// A float holds integers exactly up to 2^24; 20 bits keep the material's floor/exp2 decode exact.
	inline constexpr int32 ChunksPerMaskFloat = 20;
	inline constexpr int32 MaxMaskFloats = 5;
	inline constexpr int32 MaxChunksPerMesh = ChunksPerMaskFloat * MaxMaskFloats;
}

USTRUCT()
struct FWallChunk
{
	GENERATED_BODY()

	// Chunk centre in the static mesh's space (mid thickness).
	UPROPERTY(VisibleAnywhere, Category="WallChunks")
	FVector MeshSpaceCentre = FVector::ZeroVector;

	// Face area in cm², for the debris scale and hole sizes.
	UPROPERTY(VisibleAnywhere, Category="WallChunks")
	float Area = 0.f;

	// Index into UWallChunkLayout's debris pieces: the typical chunk this one flies off as.
	UPROPERTY(VisibleAnywhere, Category="WallChunks")
	int32 DebrisPiece = INDEX_NONE;

	// Uniform scale that gives the debris piece this chunk's area.
	UPROPERTY(VisibleAnywhere, Category="WallChunks")
	float DebrisScale = 1.f;
};

/**
 * @brief Written onto a chunked wall mesh by RTS.WallChunks.ApplyLayout; UWallChunkDamageComponent reads it to know
 * where each chunk sits, which piece it flies off as and which materials clip hidden chunks.
 * Array index = bit in the hidden-chunk mask = value in the chunk geometry's ChunkIndexUvChannel.
 */
UCLASS()
class RTS_SURVIVAL_API UWallChunkLayout : public UAssetUserData
{
	GENERATED_BODY()

public:
	const TArray<FWallChunk>& GetChunks() const { return M_Chunks; }
	const TArray<TObjectPtr<UStaticMesh>>& GetDebrisPieces() const { return M_DebrisPieces; }
	const TArray<TObjectPtr<UMaterialInterface>>& GetDamagedSlotMaterials() const { return M_DamagedSlotMaterials; }
	float GetHalfThickness() const { return M_HalfThickness; }

#if WITH_EDITOR
	void SetLayout(const TArray<FWallChunk>& Chunks, const TArray<UStaticMesh*>& DebrisPieces,
	               const TArray<UMaterialInterface*>& DamagedSlotMaterials, const float HalfThickness)
	{
		M_Chunks = Chunks;
		M_DebrisPieces = DebrisPieces;
		M_DamagedSlotMaterials = DamagedSlotMaterials;
		M_HalfThickness = HalfThickness;
	}
#endif

private:
	UPROPERTY(VisibleAnywhere, Category="WallChunks")
	TArray<FWallChunk> M_Chunks;

	// Typical chunks as their own static meshes, pivot at the chunk centre, drawn only by the Niagara debris.
	UPROPERTY(VisibleAnywhere, Category="WallChunks")
	TArray<TObjectPtr<UStaticMesh>> M_DebrisPieces;

	// Masked materials per slot. The wall keeps its opaque materials until the first chunk goes (no masked cost on
	// intact walls); then each slot with an entry here swaps to it.
	UPROPERTY(VisibleAnywhere, Category="WallChunks")
	TArray<TObjectPtr<UMaterialInterface>> M_DamagedSlotMaterials;

	// Half the slab thickness along mesh Y; splash further from the faces makes smaller holes.
	UPROPERTY(VisibleAnywhere, Category="WallChunks")
	float M_HalfThickness = 10.f;
};
