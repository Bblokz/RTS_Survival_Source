// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WallChunkDamageComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/AddOnArmorPanels/AddOnArmorPanelDebris.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogWallChunkDamage, Log, All);

namespace WallChunkDamageConstants
{
	// About half a chunk's diagonal (1.1 x 1 m cells): the nearest chunk only goes when the hole reaches its edge.
	constexpr float NearestChunkReach = 75.f;
}

UWallChunkDamageComponent::UWallChunkDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// The add-on armour panel debris: a pooled CPU mesh burst, one particle per piece, mesh per burst.
	M_DebrisSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(AddOnArmorPanelDebris::SystemPath));
}

void UWallChunkDamageComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_FindChunkMesh();
	BeginPlay_BindOwnerDeath();
}

void UWallChunkDamageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ADestructableEnvActor* const DestructibleOwner = Cast<ADestructableEnvActor>(GetOwner()))
	{
		DestructibleOwner->OnDestructableEnvActorDied.Remove(M_OwnerDiedHandle);
	}
	M_OwnerDiedHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void UWallChunkDamageComponent::BeginPlay_FindChunkMesh()
{
	const AActor* const Owner = GetOwner();
	TInlineComponentArray<UStaticMeshComponent*> MeshComponents(Owner);
	for (UStaticMeshComponent* const MeshComponent : MeshComponents)
	{
		UStaticMesh* const Mesh = IsValid(MeshComponent) ? MeshComponent->GetStaticMesh() : nullptr;
		const UWallChunkLayout* const Layout = IsValid(Mesh) ? Mesh->GetAssetUserData<UWallChunkLayout>() : nullptr;
		if (not IsValid(Layout))
		{
			continue;
		}
		M_ChunkMesh = MeshComponent;
		M_Layout = Layout;
		break;
	}
	if (not GetIsValidChunkMesh() || not GetIsValidLayout())
	{
		return;
	}
	M_Chunks = M_Layout->GetChunks();
	if (M_Chunks.IsEmpty() || M_Chunks.Num() > WallChunkMask::MaxChunksPerMesh)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("UWallChunkDamageComponent: the chunk layout of %s has %d chunks (1 to %d supported)."),
			*GetNameSafe(M_ChunkMesh->GetStaticMesh()), M_Chunks.Num(), WallChunkMask::MaxChunksPerMesh));
		M_Chunks.Reset();
		return;
	}
	if (const ADestructableEnvActor* const DestructibleOwner = Cast<ADestructableEnvActor>(Owner))
	{
		M_MaxOwnerHealth = DestructibleOwner->GetCurrentHealth();
	}
	M_LoadedDebrisSystem = M_DebrisSystem.LoadSynchronous();
	(void)GetIsValidDebrisSystem();
	WriteHiddenChunkMask();
}

void UWallChunkDamageComponent::BeginPlay_BindOwnerDeath()
{
	ADestructableEnvActor* const DestructibleOwner = Cast<ADestructableEnvActor>(GetOwner());
	if (not IsValid(DestructibleOwner) || not CollapseSettings.bCollapseIntoChunksOnDeath)
	{
		return;
	}
	M_OwnerDiedHandle = DestructibleOwner->OnDestructableEnvActorDied.AddUObject(
		this, &UWallChunkDamageComponent::OnOwnerDied);
}

void UWallChunkDamageComponent::OnOwnerDamaged(const float DamageAmount, const FDamageEvent& DamageEvent,
                                               const AActor* DamageCauser, const float HealthBefore,
                                               const float HealthAfter)
{
	if (bM_HasCollapsed || M_Chunks.IsEmpty() || DamageAmount <= 0.f || not GetIsValidChunkMesh())
	{
		return;
	}
	M_MaxOwnerHealth = FMath::Max(M_MaxOwnerHealth, HealthBefore);
	FVector WorldHit;
	FVector ShotDirection;
	bool bIsDirectHit = false;
	if (not WallDamageShared::ResolveHitLocation(DamageEvent, DamageCauser, WorldHit, ShotDirection, bIsDirectHit))
	{
		return;
	}
	M_LastHitLocation = WorldHit;
	M_LastShotDirection = ShotDirection.IsNearlyZero() ? M_LastShotDirection : ShotDirection;
	// The wall dies from this hit: the collapse throws everything, so the hole would only be thrown twice.
	if (HealthAfter <= 0.f)
	{
		return;
	}
	const FVector MeshSpaceHit = M_ChunkMesh->GetComponentTransform().InverseTransformPosition(WorldHit);
	const float PlaneDistance = FMath::Max(0.f, static_cast<float>(FMath::Abs(MeshSpaceHit.Y - M_Chunks[0].MeshSpaceCentre.Y))
	                                       - M_Layout->GetHalfThickness());
	const float HoleRadius = WallDamageShared::ResolveHoleRadius(HoleSettings, DamageAmount, DamageCauser, PlaneDistance);
	const float HealthFractionAfter = M_MaxOwnerHealth > 0.f ? HealthAfter / M_MaxOwnerHealth : 1.f;
	TArray<int32> NewlyHidden;
	HideChunksAroundHit(MeshSpaceHit, HoleRadius, bIsDirectHit, GetMaxHiddenChunks(HealthFractionAfter), NewlyHidden);
	if (NewlyHidden.IsEmpty())
	{
		return;
	}
	SwapToDamagedMaterials();
	WriteHiddenChunkMask();
	ThrowChunkDebris(NewlyHidden, WorldHit, ShotDirection, 1.f);
	UE_LOG(LogWallChunkDamage, Verbose, TEXT("RTS_WALL_CHUNKS hit %s radius=%.0f direct=%d hidden=%d/%d (+%d)"),
	       *GetNameSafe(GetOwner()), HoleRadius, bIsDirectHit ? 1 : 0, M_HiddenChunkCount, M_Chunks.Num(),
	       NewlyHidden.Num());
}

int32 UWallChunkDamageComponent::Debug_FireTestHit(const FVector WorldLocation, const FVector ShotDirection,
                                                   const float CalibreMm)
{
	if (bM_HasCollapsed || M_Chunks.IsEmpty() || not GetIsValidChunkMesh())
	{
		return 0;
	}
	const FVector MeshSpaceHit = M_ChunkMesh->GetComponentTransform().InverseTransformPosition(WorldLocation);
	TArray<int32> NewlyHidden;
	// No health cap: the test drives the wall directly.
	HideChunksAroundHit(MeshSpaceHit, WallDamageShared::ResolveHoleRadiusFromCalibre(HoleSettings, CalibreMm, 0.f), true,
	                    M_Chunks.Num(), NewlyHidden);
	M_LastHitLocation = WorldLocation;
	M_LastShotDirection = ShotDirection.GetSafeNormal();
	if (NewlyHidden.IsEmpty())
	{
		return 0;
	}
	SwapToDamagedMaterials();
	WriteHiddenChunkMask();
	ThrowChunkDebris(NewlyHidden, WorldLocation, M_LastShotDirection, 1.f);
	UE_LOG(LogWallChunkDamage, Display, TEXT("RTS_WALL_CHUNKS test hit %s calibre=%.0f hidden=%d/%d (+%d)"),
	       *GetNameSafe(GetOwner()), CalibreMm, M_HiddenChunkCount, M_Chunks.Num(), NewlyHidden.Num());
	return NewlyHidden.Num();
}

int32 UWallChunkDamageComponent::GetMaxHiddenChunks(const float HealthFractionAfter) const
{
	const float LostHealth01 = 1.f - FMath::Clamp(HealthFractionAfter, 0.f, 1.f);
	const int32 Cap = FMath::CeilToInt32(LostHealth01 * HoleSettings.MaxHiddenShareWhileAlive * M_Chunks.Num());
	// A damaged wall always shows its damage, and only the collapse takes the last chunk.
	return FMath::Clamp(Cap, 1, M_Chunks.Num() - 1);
}

void UWallChunkDamageComponent::HideChunksAroundHit(const FVector& MeshSpaceHit, const float HoleRadius,
                                                    const bool bIsDirectHit, const int32 MaxHiddenChunks,
                                                    TArray<int32>& OutNewlyHidden)
{
	OutNewlyHidden.Reset();
	if (HoleRadius <= 0.f || M_HiddenChunkCount >= MaxHiddenChunks)
	{
		return;
	}
	// Distance on the wall face (mesh X/Z); the slab is thin, so both faces hole through alike.
	TArray<TPair<float, int32>> Candidates;
	for (int32 ChunkIndex = 0; ChunkIndex < M_Chunks.Num(); ++ChunkIndex)
	{
		if (GetIsChunkHidden(ChunkIndex))
		{
			continue;
		}
		const FVector& Centre = M_Chunks[ChunkIndex].MeshSpaceCentre;
		Candidates.Emplace(FVector2D(Centre.X - MeshSpaceHit.X, Centre.Z - MeshSpaceHit.Z).Size(), ChunkIndex);
	}
	if (Candidates.IsEmpty())
	{
		return;
	}
	Candidates.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	// The nearest chunk goes on a direct hit with a big enough hole, and by chance with a smaller one; only when the
	// hole reaches that chunk (shrapnel reuses its shell's old impact point, which can lie far along the wall).
	const float NearestChance = HoleRadius / FMath::Max(HoleSettings.GuaranteedChunkRadius, 1.f);
	const bool bNearestIsReached = Candidates[0].Key <= HoleRadius + WallChunkDamageConstants::NearestChunkReach;
	const bool bTakesNearest = bIsDirectHit && bNearestIsReached && FMath::FRand() < NearestChance;
	for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
	{
		const bool bIsInHole = Candidates[CandidateIndex].Key <= HoleRadius;
		const bool bIsTakenNearest = CandidateIndex == 0 && bTakesNearest;
		if ((not bIsInHole && not bIsTakenNearest) || M_HiddenChunkCount >= MaxHiddenChunks)
		{
			break;
		}
		SetChunkHidden(Candidates[CandidateIndex].Value);
		OutNewlyHidden.Add(Candidates[CandidateIndex].Value);
	}
}

bool UWallChunkDamageComponent::GetIsChunkHidden(const int32 ChunkIndex) const
{
	const int32 Word = ChunkIndex / WallChunkMask::ChunksPerMaskFloat;
	const int32 Bit = ChunkIndex % WallChunkMask::ChunksPerMaskFloat;
	return (M_HiddenChunkWords[Word] & (1u << Bit)) != 0;
}

void UWallChunkDamageComponent::SetChunkHidden(const int32 ChunkIndex)
{
	if (GetIsChunkHidden(ChunkIndex))
	{
		return;
	}
	const int32 Word = ChunkIndex / WallChunkMask::ChunksPerMaskFloat;
	const int32 Bit = ChunkIndex % WallChunkMask::ChunksPerMaskFloat;
	M_HiddenChunkWords[Word] |= 1u << Bit;
	++M_HiddenChunkCount;
}

void UWallChunkDamageComponent::WriteHiddenChunkMask() const
{
	if (not GetIsValidChunkMesh())
	{
		return;
	}
	for (int32 Word = 0; Word < WallChunkMask::MaxMaskFloats; ++Word)
	{
		M_ChunkMesh->SetCustomPrimitiveDataFloat(WallChunkMask::FirstMaskPrimitiveDataIndex + Word,
		                                         static_cast<float>(M_HiddenChunkWords[Word]));
	}
}

void UWallChunkDamageComponent::SwapToDamagedMaterials()
{
	// Intact walls keep their opaque materials; masked surfaces lose early-Z in the depth and shadow passes.
	if (bM_HasSwappedToDamagedMaterials || not GetIsValidChunkMesh() || not GetIsValidLayout())
	{
		return;
	}
	bM_HasSwappedToDamagedMaterials = true;
	const TArray<TObjectPtr<UMaterialInterface>>& DamagedMaterials = M_Layout->GetDamagedSlotMaterials();
	for (int32 SlotIndex = 0; SlotIndex < DamagedMaterials.Num(); ++SlotIndex)
	{
		if (IsValid(DamagedMaterials[SlotIndex]))
		{
			M_ChunkMesh->SetMaterial(SlotIndex, DamagedMaterials[SlotIndex]);
		}
	}
}

void UWallChunkDamageComponent::ThrowChunkDebris(const TConstArrayView<int32> ChunkIndices, const FVector& HitLocation,
                                                 const FVector& ShotDirection, const float SpeedScale) const
{
	if (ChunkIndices.IsEmpty() || not IsValid(M_LoadedDebrisSystem) || not GetIsValidLayout() || not GetIsValidChunkMesh())
	{
		return;
	}
	const FTransform MeshTransform = M_ChunkMesh->GetComponentTransform();
	const double MeshScale = MeshTransform.GetMaximumAxisScale();
	TArray<FWallDebrisPiece> Pieces;
	for (const int32 ChunkIndex : ChunkIndices)
	{
		const FWallChunk& Chunk = M_Chunks[ChunkIndex];
		FWallDebrisPiece& Piece = Pieces.AddDefaulted_GetRef();
		Piece.WorldCentre = MeshTransform.TransformPosition(Chunk.MeshSpaceCentre);
		Piece.Scale = static_cast<float>(MeshScale) * Chunk.DebrisScale;
		Piece.MeshIndex = Chunk.DebrisPiece;
	}
	WallDamageShared::ThrowDebrisPieces(*GetOwner(), M_LoadedDebrisSystem, M_Layout->GetDebrisPieces(), Pieces,
	                                    MeshTransform.GetRotation(), HitLocation, ShotDirection, SpeedScale);
}

void UWallChunkDamageComponent::OnOwnerDied()
{
	CollapseIntoChunks();
}

void UWallChunkDamageComponent::CollapseIntoChunks()
{
	if (bM_HasCollapsed || not GetIsValidChunkMesh())
	{
		return;
	}
	bM_HasCollapsed = true;
	TArray<int32> Remaining;
	for (int32 ChunkIndex = 0; ChunkIndex < M_Chunks.Num(); ++ChunkIndex)
	{
		if (not GetIsChunkHidden(ChunkIndex))
		{
			SetChunkHidden(ChunkIndex);
			Remaining.Add(ChunkIndex);
		}
	}
	const FVector Centre = M_ChunkMesh->Bounds.Origin;
	ThrowChunkDebris(Remaining, Centre, M_LastShotDirection, CollapseSettings.DeathSpeedScale);
	// Nothing of the wall is left to draw or to hit.
	M_ChunkMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_ChunkMesh->SetVisibility(false, false);
	WallDamageShared::PlayCollapseSound(*GetOwner(), CollapseSettings, Centre);
	GetOwner()->SetLifeSpan(FMath::Max(CollapseSettings.DestroyOwnerDelaySeconds, UE_KINDA_SMALL_NUMBER));
	UE_LOG(LogWallChunkDamage, Display, TEXT("RTS_WALL_CHUNKS collapse %s threw=%d"), *GetNameSafe(GetOwner()),
	       Remaining.Num());
}

bool UWallChunkDamageComponent::GetIsValidChunkMesh() const
{
	if (IsValid(M_ChunkMesh))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_ChunkMesh",
		"GetIsValidChunkMesh",
		GetOwner());
	return false;
}

bool UWallChunkDamageComponent::GetIsValidLayout() const
{
	if (IsValid(M_Layout))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_Layout",
		"GetIsValidLayout",
		GetOwner());
	return false;
}

bool UWallChunkDamageComponent::GetIsValidDebrisSystem() const
{
	if (IsValid(M_LoadedDebrisSystem))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_LoadedDebrisSystem",
		"GetIsValidDebrisSystem",
		GetOwner());
	return false;
}
