// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WallChunkDamageComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "RTS_Survival/MasterObjects/HealthBase/HPActorObjectsMaster.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/AddOnArmorPanels/AddOnArmorPanelDebris.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogWallChunkDamage, Log, All);

UWallChunkDamageComponent::UWallChunkDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// The add-on armour panel debris: a pooled CPU mesh burst, one particle per piece, mesh per burst.
	M_DebrisSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(AddOnArmorPanelDebris::SystemPath));
}

void UWallChunkDamageComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const ADestructableEnvActor* const DestructibleOwner = Cast<ADestructableEnvActor>(GetOwner()))
	{
		NoteOwnerHealth(DestructibleOwner->GetCurrentHealth());
	}
	M_LoadedDebrisSystem = M_DebrisSystem.LoadSynchronous();
	(void)GetIsValidDebrisSystem();
	BeginPlay_BindOwnerDeath();
	BeginPlay_BindOwnerHealing();
	// Buildings under construction carry their construction mesh now; the layout is found on the first hit then.
	(void)EnsureLayoutIsCurrent();
}

void UWallChunkDamageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ADestructableEnvActor* const DestructibleOwner = Cast<ADestructableEnvActor>(GetOwner()))
	{
		DestructibleOwner->OnDestructableEnvActorDied.Remove(M_OwnerDiedHandle);
	}
	else if (AHPActorObjectsMaster* const BuildingOwner = Cast<AHPActorObjectsMaster>(GetOwner()))
	{
		BuildingOwner->OnUnitDies.Remove(M_OwnerDiedHandle);
	}
	M_OwnerDiedHandle.Reset();
	if (M_OwnerHealth.IsValid())
	{
		M_OwnerHealth->GetOnHealthHealed().Remove(M_OwnerHealedHandle);
	}
	M_OwnerHealedHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void UWallChunkDamageComponent::BeginPlay_BindOwnerHealing()
{
	UHealthComponent* const OwnerHealth = GetOwner()->FindComponentByClass<UHealthComponent>();
	// Environment walls without a health component never heal.
	if (not IsValid(OwnerHealth))
	{
		return;
	}
	M_OwnerHealth = OwnerHealth;
	M_OwnerHealedHandle = OwnerHealth->GetOnHealthHealed().AddUObject(
		this, &UWallChunkDamageComponent::OnOwnerHealthHealed);
}

void UWallChunkDamageComponent::OnOwnerHealthHealed(const float HealthAfter, const float MaxHealth)
{
	OnOwnerHealed(HealthAfter, MaxHealth);
}

void UWallChunkDamageComponent::BeginPlay_BindOwnerDeath()
{
	if (not CollapseSettings.bCollapseIntoChunksOnDeath)
	{
		return;
	}
	if (ADestructableEnvActor* const DestructibleOwner = Cast<ADestructableEnvActor>(GetOwner()))
	{
		M_OwnerDiedHandle = DestructibleOwner->OnDestructableEnvActorDied.AddUObject(
			this, &UWallChunkDamageComponent::OnDestructibleOwnerDied);
		return;
	}
	if (AHPActorObjectsMaster* const BuildingOwner = Cast<AHPActorObjectsMaster>(GetOwner()))
	{
		M_OwnerDiedHandle = BuildingOwner->OnUnitDies.AddUObject(this, &UWallChunkDamageComponent::OnBuildingOwnerDied);
	}
}

bool UWallChunkDamageComponent::EnsureLayoutIsCurrent()
{
	UStaticMeshComponent* Found = nullptr;
	const UWallChunkLayout* FoundLayout = nullptr;
	TInlineComponentArray<UStaticMeshComponent*> MeshComponents(GetOwner());
	for (UStaticMeshComponent* const MeshComponent : MeshComponents)
	{
		if (not IsValid(MeshComponent))
		{
			continue;
		}
		const bool bIsNamed = ChunkMeshComponentName != NAME_None && MeshComponent->GetFName() == ChunkMeshComponentName;
		UStaticMesh* const Mesh = MeshComponent->GetStaticMesh();
		const UWallChunkLayout* const Layout = IsValid(Mesh) ? Mesh->GetAssetUserData<UWallChunkLayout>() : nullptr;
		if (bIsNamed || (ChunkMeshComponentName == NAME_None && IsValid(Layout)))
		{
			Found = MeshComponent;
			FoundLayout = Layout;
			break;
		}
	}
	// No chunked mesh right now (a building's construction mesh): nothing to hit yet.
	if (not IsValid(Found) || not IsValid(FoundLayout))
	{
		return false;
	}
	if (Found != M_ChunkMesh || FoundLayout != M_Layout)
	{
		CacheLayout(*Found, *FoundLayout);
	}
	return not M_Chunks.IsEmpty();
}

void UWallChunkDamageComponent::CacheLayout(UStaticMeshComponent& MeshComponent, const UWallChunkLayout& Layout)
{
	M_ChunkMesh = &MeshComponent;
	M_Layout = &Layout;
	M_Chunks = Layout.GetChunks();
	FMemory::Memzero(M_HiddenChunkWords);
	M_HiddenOrder.Reset();
	M_HiddenChunkCount = 0;
	M_MeshBounds = IsValid(MeshComponent.GetStaticMesh()) ? MeshComponent.GetStaticMesh()->GetBoundingBox() : FBox(ForceInit);
	if (M_Chunks.Num() > WallChunkMask::MaxChunksPerMesh)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("UWallChunkDamageComponent: the chunk layout of %s has %d chunks (at most %d supported)."),
			*GetNameSafe(MeshComponent.GetStaticMesh()), M_Chunks.Num(), WallChunkMask::MaxChunksPerMesh));
		M_Chunks.Reset();
		return;
	}
	float AreaSum = 0.f;
	for (const FWallChunk& Chunk : M_Chunks)
	{
		AreaSum += Chunk.Area;
	}
	M_TypicalChunkRadius = M_Chunks.IsEmpty() ? M_TypicalChunkRadius : FMath::Sqrt(AreaSum / M_Chunks.Num() / UE_PI);
	WriteHiddenChunkMask();
}

void UWallChunkDamageComponent::OnOwnerDamaged(const float DamageAmount, const FDamageEvent& DamageEvent,
                                               const AActor* DamageCauser, const float HealthBefore,
                                               const float HealthAfter)
{
	if (bM_HasCollapsed || DamageAmount <= 0.f || GetIsOwnerInTransition() || not EnsureLayoutIsCurrent())
	{
		return;
	}
	NoteOwnerHealth(HealthBefore);
	FVector WorldHit;
	FVector ShotDirection;
	bool bIsDirectHit = false;
	if (not WallDamageShared::ResolveHitLocation(DamageEvent, DamageCauser, WorldHit, ShotDirection, bIsDirectHit))
	{
		return;
	}
	M_LastHitLocation = WorldHit;
	M_LastShotDirection = ShotDirection.IsNearlyZero() ? M_LastShotDirection : ShotDirection;
	// The owner dies from this hit: its death throws the pieces.
	if (HealthAfter <= 0.f)
	{
		return;
	}
	const FVector MeshSpaceHit = M_ChunkMesh->GetComponentTransform().InverseTransformPosition(WorldHit);
	// Splash outside the mesh makes a smaller hole.
	const float OutsideDistance = FMath::Sqrt(static_cast<float>(M_MeshBounds.ComputeSquaredDistanceToPoint(MeshSpaceHit)));
	const float HoleRadius = WallDamageShared::ResolveHoleRadius(HoleSettings, DamageAmount, DamageCauser, OutsideDistance);
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

void UWallChunkDamageComponent::OnOwnerHealed(const float HealthAfter, const float MaxHealth)
{
	if (bM_HasCollapsed || not EnsureLayoutIsCurrent())
	{
		return;
	}
	SetOwnerMaxHealth(MaxHealth);
	const float HealthFraction = M_MaxOwnerHealth > 0.f ? HealthAfter / M_MaxOwnerHealth : 1.f;
	// A fully repaired building has all its chunks back; otherwise the newest come back until the rest fits.
	const int32 AllowedHidden = HealthFraction >= 1.f ? 0 : GetMaxHiddenChunks(HealthFraction);
	bool bChanged = false;
	while (M_HiddenChunkCount > AllowedHidden && not M_HiddenOrder.IsEmpty())
	{
		SetChunkVisible(M_HiddenOrder.Pop(EAllowShrinking::No));
		bChanged = true;
	}
	if (not bChanged)
	{
		return;
	}
	WriteHiddenChunkMask();
	// Fully repaired: back to the opaque materials, so an intact building pays nothing for the mask again.
	if (M_HiddenChunkCount == 0)
	{
		RestoreIntactMaterials();
	}
}

int32 UWallChunkDamageComponent::Debug_FireTestHit(const FVector WorldLocation, const FVector ShotDirection,
                                                   const float CalibreMm)
{
	if (bM_HasCollapsed || not EnsureLayoutIsCurrent())
	{
		return 0;
	}
	const FVector MeshSpaceHit = M_ChunkMesh->GetComponentTransform().InverseTransformPosition(WorldLocation);
	TArray<int32> NewlyHidden;
	// No health cap: the test drives the mesh directly.
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
	return FMath::Clamp(Cap, 1, FMath::Max(M_Chunks.Num() - 1, 1));
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
	// Distance in mesh space: a sphere around the impact (on a thin wall the same as a disc on its face).
	TArray<TPair<float, int32>> Candidates;
	for (int32 ChunkIndex = 0; ChunkIndex < M_Chunks.Num(); ++ChunkIndex)
	{
		if (not GetIsChunkHidden(ChunkIndex))
		{
			Candidates.Emplace(static_cast<float>(FVector::Distance(M_Chunks[ChunkIndex].MeshSpaceCentre, MeshSpaceHit)),
			                   ChunkIndex);
		}
	}
	if (Candidates.IsEmpty())
	{
		return;
	}
	Candidates.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	// The nearest chunk goes on a direct hit with a big enough hole, and by chance with a smaller one; only when the
	// hole reaches that chunk (shrapnel reuses its shell's old impact point, which can lie far along the wall).
	const float NearestChance = HoleRadius / FMath::Max(HoleSettings.GuaranteedChunkRadius, 1.f);
	const bool bNearestIsReached = Candidates[0].Key <= HoleRadius + M_TypicalChunkRadius;
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
	M_HiddenChunkWords[ChunkIndex / WallChunkMask::ChunksPerMaskFloat] |= 1u << (ChunkIndex % WallChunkMask::ChunksPerMaskFloat);
	M_HiddenOrder.Add(ChunkIndex);
	++M_HiddenChunkCount;
}

void UWallChunkDamageComponent::SetChunkVisible(const int32 ChunkIndex)
{
	if (not GetIsChunkHidden(ChunkIndex))
	{
		return;
	}
	M_HiddenChunkWords[ChunkIndex / WallChunkMask::ChunksPerMaskFloat] &= ~(1u << (ChunkIndex % WallChunkMask::ChunksPerMaskFloat));
	--M_HiddenChunkCount;
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
	// Intact meshes keep their opaque materials; masked surfaces lose early-Z in the depth and shadow passes.
	// Checked on every hit, not once: building expansions put their own materials back after construction.
	if (not GetIsValidChunkMesh() || not GetIsValidLayout())
	{
		return;
	}
	const TArray<TObjectPtr<UMaterialInterface>>& DamagedMaterials = M_Layout->GetDamagedSlotMaterials();
	const int32 SlotCount = FMath::Max(DamagedMaterials.Num(), DamagedMaterialOverrides.Num());
	M_IntactMaterials.SetNum(FMath::Max(M_IntactMaterials.Num(), SlotCount));
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		UMaterialInterface* const Damaged = GetDamagedMaterialForSlot(SlotIndex);
		if (IsValid(Damaged) && M_ChunkMesh->GetMaterial(SlotIndex) != Damaged)
		{
			M_IntactMaterials[SlotIndex] = M_ChunkMesh->GetMaterial(SlotIndex);
			M_ChunkMesh->SetMaterial(SlotIndex, Damaged);
		}
	}
}

UMaterialInterface* UWallChunkDamageComponent::GetDamagedMaterialForSlot(const int32 SlotIndex) const
{
	UMaterialInterface* const Override = DamagedMaterialOverrides.IsValidIndex(SlotIndex)
		                                     ? DamagedMaterialOverrides[SlotIndex].Get()
		                                     : nullptr;
	if (IsValid(Override))
	{
		return Override;
	}
	const TArray<TObjectPtr<UMaterialInterface>>& DamagedMaterials = M_Layout->GetDamagedSlotMaterials();
	return DamagedMaterials.IsValidIndex(SlotIndex) ? DamagedMaterials[SlotIndex].Get() : nullptr;
}

void UWallChunkDamageComponent::RestoreIntactMaterials()
{
	if (not GetIsValidChunkMesh() || not GetIsValidLayout())
	{
		return;
	}
	for (int32 SlotIndex = 0; SlotIndex < M_IntactMaterials.Num(); ++SlotIndex)
	{
		UMaterialInterface* const Intact = M_IntactMaterials[SlotIndex];
		const bool bShowsDamaged = M_ChunkMesh->GetMaterial(SlotIndex) == GetDamagedMaterialForSlot(SlotIndex);
		// A slot the owner re-skinned since (construction, unpacking) keeps its new material.
		if (IsValid(Intact) && bShowsDamaged)
		{
			M_ChunkMesh->SetMaterial(SlotIndex, Intact);
		}
	}
	M_IntactMaterials.Reset();
}

bool UWallChunkDamageComponent::GetIsOwnerInTransition() const
{
	const ABuildingExpansion* const BuildingOwner = Cast<ABuildingExpansion>(GetOwner());
	if (not IsValid(BuildingOwner))
	{
		return false;
	}
	// These states show construction, placement or packing looks; chunks would replace them. Placed expansions that
	// never ran construction keep their other states and do take damage.
	switch (BuildingOwner->GetBuildingExpansionStatus())
	{
	case EBuildingExpansionStatus::BXS_LookingForPlacement:
	case EBuildingExpansionStatus::BXS_BeingBuild:
	case EBuildingExpansionStatus::BXS_BeingPackedUp:
	case EBuildingExpansionStatus::BXS_LookingForUnpackingLocation:
	case EBuildingExpansionStatus::BXS_BeingUnpacked:
		return true;
	default:
		return false;
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

int32 UWallChunkDamageComponent::ThrowDeathPieces(const int32 MaxPieces, const float SpeedScale)
{
	if (bM_HasCollapsed || MaxPieces <= 0 || not EnsureLayoutIsCurrent())
	{
		return 0;
	}
	bM_HasCollapsed = true;
	TArray<int32> Standing;
	for (int32 ChunkIndex = 0; ChunkIndex < M_Chunks.Num(); ++ChunkIndex)
	{
		if (not GetIsChunkHidden(ChunkIndex))
		{
			Standing.Add(ChunkIndex);
		}
	}
	// A random handful is shot out of the building; the rest stays on the mesh and sinks with it.
	for (int32 Index = Standing.Num() - 1; Index > 0; --Index)
	{
		Standing.Swap(Index, FMath::RandRange(0, Index));
	}
	Standing.SetNum(FMath::Min(Standing.Num(), MaxPieces));
	for (const int32 ChunkIndex : Standing)
	{
		SetChunkHidden(ChunkIndex);
	}
	if (Standing.IsEmpty())
	{
		return 0;
	}
	SwapToDamagedMaterials();
	WriteHiddenChunkMask();
	ThrowChunkDebris(Standing, M_ChunkMesh->Bounds.Origin, FVector::ZeroVector, SpeedScale);
	UE_LOG(LogWallChunkDamage, Display, TEXT("RTS_WALL_CHUNKS death pieces %s threw=%d"), *GetNameSafe(GetOwner()),
	       Standing.Num());
	return Standing.Num();
}

void UWallChunkDamageComponent::OnDestructibleOwnerDied()
{
	CollapseIntoChunks(true);
}

void UWallChunkDamageComponent::OnBuildingOwnerDied()
{
	CollapseIntoChunks(false);
}

void UWallChunkDamageComponent::CollapseIntoChunks(const bool bHideAndDestroyOwner)
{
	if (bM_HasCollapsed || not EnsureLayoutIsCurrent())
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
	UE_LOG(LogWallChunkDamage, Display, TEXT("RTS_WALL_CHUNKS collapse %s threw=%d hide=%d"), *GetNameSafe(GetOwner()),
	       Remaining.Num(), bHideAndDestroyOwner ? 1 : 0);
	if (not bHideAndDestroyOwner)
	{
		SwapToDamagedMaterials();
		WriteHiddenChunkMask();
		return;
	}
	// Nothing of the wall is left to draw or to hit.
	M_ChunkMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_ChunkMesh->SetVisibility(false, false);
	WallDamageShared::PlayCollapseSound(*GetOwner(), CollapseSettings, Centre);
	GetOwner()->SetLifeSpan(FMath::Max(CollapseSettings.DestroyOwnerDelaySeconds, UE_KINDA_SMALL_NUMBER));
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
