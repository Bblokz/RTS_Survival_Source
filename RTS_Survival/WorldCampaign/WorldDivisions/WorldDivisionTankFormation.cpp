// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionTankFormation.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/WorldCampaign/BatteryFire/WorldBatteryFireSettings.h"

namespace WorldDivisionTankFormationConstants
{
	// A mesh whose long side lies along Y is turned so that side points along the division's forward direction.
	constexpr float LongSideAlongYYaw = -90.f;
	// The click box is this much larger than the slot offsets on every side, so the outer tanks are inside it.
	constexpr float ClickBoxPadding = 12.f;
	constexpr float ClickBoxHalfHeight = 8.f;
	const FName WreckFireScaleParameterName = TEXT("Scale");
}

TArray<FVector2D> WorldDivisionFormationLayout::GetSlotOffsets(const int32 SlotCount, const int32 MaxPerRow,
                                                               const float ColumnSpacing, const float RowSpacing)
{
	TArray<FVector2D> SlotOffsets;
	if (SlotCount <= 0)
	{
		return SlotOffsets;
	}

	const int32 SafeMaxPerRow = FMath::Max(1, MaxPerRow);
	const int32 RowCount = FMath::DivideAndRoundUp(SlotCount, SafeMaxPerRow);
	const int32 SlotsPerFullRow = FMath::DivideAndRoundUp(SlotCount, RowCount);
	SlotOffsets.Reserve(SlotCount);
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		const int32 RowIndex = SlotIndex / SlotsPerFullRow;
		const int32 ColumnIndex = SlotIndex % SlotsPerFullRow;
		const int32 SlotsInThisRow = FMath::Min(SlotsPerFullRow, SlotCount - RowIndex * SlotsPerFullRow);
		// Row 0 is the front row; the rows as a whole are centred on the division.
		const float ForwardOffset = (static_cast<float>(RowCount - 1) * 0.5f - static_cast<float>(RowIndex)) * RowSpacing;
		const float RightOffset = (static_cast<float>(ColumnIndex) - static_cast<float>(SlotsInThisRow - 1) * 0.5f)
			* ColumnSpacing;
		SlotOffsets.Add(FVector2D(ForwardOffset, RightOffset));
	}
	return SlotOffsets;
}

FVector2D WorldDivisionFormationLayout::GetFootprintHalfSize(const TArray<FVector2D>& SlotOffsets)
{
	FVector2D HalfSize = FVector2D::ZeroVector;
	for (const FVector2D& SlotOffset : SlotOffsets)
	{
		HalfSize.X = FMath::Max(HalfSize.X, FMath::Abs(SlotOffset.X));
		HalfSize.Y = FMath::Max(HalfSize.Y, FMath::Abs(SlotOffset.Y));
	}
	return HalfSize;
}

UWorldDivisionTankFormation::UWorldDivisionTankFormation()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldDivisionTankFormation::RebuildFromRoster(const TArray<ETankSubtype>& TankSubtypes)
{
	const AActor* const Owner = GetOwner();
	const UWorld* const World = GetWorld();
	const bool bCanBuild = IsValid(Owner) && Owner->HasActorBegunPlay() && IsValid(Owner->GetRootComponent())
		&& IsValid(World) && World->IsGameWorld();
	DestroySlots();
	bM_DefersCasualtyVisuals = false;
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	if (not bCanBuild || Settings == nullptr)
	{
		RestoreOwnerMeshes();
		return;
	}

	const TArray<FVector2D> SlotOffsets = WorldDivisionFormationLayout::GetSlotOffsets(
		TankSubtypes.Num(), Settings->FormationMaxPerRow, Settings->FormationColumnSpacing,
		Settings->FormationRowSpacing);
	UStaticMesh* const PlateMesh = LoadPlateMesh();
	// Set before the pieces are made: they stand on the plate.
	M_PieceBaseHeight = IsValid(PlateMesh) ? Settings->FormationPlateHeight : 0.f;
	bool bHasAnyTankMesh = false;
	for (int32 SlotIndex = 0; SlotIndex < TankSubtypes.Num(); ++SlotIndex)
	{
		FWorldDivisionTankSlot NewSlot;
		NewSlot.Subtype = TankSubtypes[SlotIndex];
		NewSlot.LocalOffset = SlotOffsets[SlotIndex];
		UStaticMesh* const PieceMesh = LoadPieceMesh(NewSlot.Subtype);
		NewSlot.PieceMesh = IsValid(PieceMesh) ? CreatePieceComponent(PieceMesh, NewSlot.LocalOffset) : nullptr;
		bHasAnyTankMesh = bHasAnyTankMesh || IsValid(NewSlot.PieceMesh);
		M_Slots.Add(NewSlot);
	}

	if (not bHasAnyTankMesh)
	{
		// Nothing to show: the Blueprint's own look stays as it is.
		DestroySlots();
		RestoreOwnerMeshes();
		return;
	}

	HideOwnerMeshes();
	UpdateClickBox(SlotOffsets);
	CreatePlate(PlateMesh, SlotOffsets);
}

void UWorldDivisionTankFormation::HandleRosterShrunk(const int32 NewEntryCount)
{
	for (int32 SlotIndex = FMath::Max(0, NewEntryCount); SlotIndex < M_Slots.Num(); ++SlotIndex)
	{
		FWorldDivisionTankSlot& Slot = M_Slots[SlotIndex];
		if (Slot.State != EWorldDivisionTankSlotState::Alive)
		{
			continue;
		}

		if (bM_DefersCasualtyVisuals)
		{
			Slot.State = EWorldDivisionTankSlotState::DeferredCasualty;
			continue;
		}

		TurnSlotIntoWreck(Slot, false);
	}
}

void UWorldDivisionTankFormation::SetDefersCasualtyVisuals(const bool bDefers)
{
	bM_DefersCasualtyVisuals = bDefers;
}

bool UWorldDivisionTankFormation::ExplodeSlot(const int32 SlotIndex)
{
	if (not M_Slots.IsValidIndex(SlotIndex) || M_Slots[SlotIndex].State == EWorldDivisionTankSlotState::Wreck)
	{
		return false;
	}

	FVector SlotLocation = FVector::ZeroVector;
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	UNiagaraSystem* const ExplosionEffect = Settings != nullptr
		                                        ? UWorldBatteryFireSettings::LoadOptional(
			                                        Settings->TankExplosionEffect, TEXT("TankExplosionEffect"))
		                                        : nullptr;
	if (IsValid(ExplosionEffect) && TryGetSlotWorldLocation(SlotIndex, SlotLocation))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, ExplosionEffect, SlotLocation, FRotator::ZeroRotator, FVector(Settings->TankExplosionEffectScale));
	}

	TurnSlotIntoWreck(M_Slots[SlotIndex], true);
	return true;
}

void UWorldDivisionTankFormation::FlushDeferredCasualties()
{
	bM_DefersCasualtyVisuals = false;
	for (FWorldDivisionTankSlot& Slot : M_Slots)
	{
		if (Slot.State == EWorldDivisionTankSlotState::DeferredCasualty)
		{
			TurnSlotIntoWreck(Slot, false);
		}
	}
}

int32 UWorldDivisionTankFormation::GetWreckCount() const
{
	int32 WreckCount = 0;
	for (const FWorldDivisionTankSlot& Slot : M_Slots)
	{
		WreckCount += Slot.State == EWorldDivisionTankSlotState::Wreck ? 1 : 0;
	}
	return WreckCount;
}

bool UWorldDivisionTankFormation::TryGetSlotWorldLocation(const int32 SlotIndex, FVector& OutLocation) const
{
	const AActor* const Owner = GetOwner();
	if (not M_Slots.IsValidIndex(SlotIndex) || not IsValid(Owner))
	{
		return false;
	}

	const FVector2D& LocalOffset = M_Slots[SlotIndex].LocalOffset;
	OutLocation = Owner->GetActorTransform().TransformPosition(FVector(LocalOffset.X, LocalOffset.Y, 0.f));
	return true;
}

void UWorldDivisionTankFormation::DestroySlots()
{
	for (FWorldDivisionTankSlot& Slot : M_Slots)
	{
		if (IsValid(Slot.WreckFire))
		{
			Slot.WreckFire->DestroyComponent();
		}
		if (IsValid(Slot.PieceMesh))
		{
			Slot.PieceMesh->DestroyComponent();
		}
	}
	M_Slots.Reset();

	if (IsValid(M_ClickBox))
	{
		M_ClickBox->DestroyComponent();
	}
	M_ClickBox = nullptr;

	if (IsValid(M_Plate))
	{
		M_Plate->DestroyComponent();
	}
	M_Plate = nullptr;
}

UStaticMeshComponent* UWorldDivisionTankFormation::CreatePieceComponent(UStaticMesh* PieceMesh,
                                                                        const FVector2D& LocalOffset)
{
	AActor* const Owner = GetOwner();
	if (not IsValid(Owner) || not IsValid(Owner->GetRootComponent()) || not IsValid(PieceMesh))
	{
		return nullptr;
	}

	UStaticMeshComponent* const PieceComponent = NewObject<UStaticMeshComponent>(Owner);
	PieceComponent->SetStaticMesh(PieceMesh);
	// The tanks never take the cursor trace; the click box and the Blueprint's own collision do.
	PieceComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PieceComponent->SetGenerateOverlapEvents(false);
	PieceComponent->SetCanEverAffectNavigation(false);
	PieceComponent->SetupAttachment(Owner->GetRootComponent());
	PieceComponent->RegisterComponent();
	FitPieceToMap(*PieceComponent, *PieceMesh, LocalOffset);
	return PieceComponent;
}

void UWorldDivisionTankFormation::FitPieceToMap(UStaticMeshComponent& PieceComponent, const UStaticMesh& PieceMesh,
                                                const FVector2D& LocalOffset) const
{
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	if (Settings == nullptr)
	{
		return;
	}

	const FBoxSphereBounds MeshBounds = PieceMesh.GetBounds();
	const float LongestSide = 2.f * static_cast<float>(FMath::Max(MeshBounds.BoxExtent.X, MeshBounds.BoxExtent.Y));
	const bool bFitsToLength = Settings->TankPieceTargetLength > 0.f && LongestSide > KINDA_SMALL_NUMBER;
	const float FitScale = bFitsToLength ? Settings->TankPieceTargetLength / LongestSide : 1.f;
	const float PieceScale = FitScale * Settings->TankPieceScale;
	const bool bIsLongSideAlongY = MeshBounds.BoxExtent.Y > MeshBounds.BoxExtent.X;
	const float Yaw = Settings->TankPieceYawOffset
		+ (bIsLongSideAlongY ? WorldDivisionTankFormationConstants::LongSideAlongYYaw : 0.f);
	// The mesh's underside sits on the division's root height.
	const float UndersideZ = static_cast<float>(MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z) * PieceScale;
	PieceComponent.SetRelativeScale3D(FVector(PieceScale));
	PieceComponent.SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
	PieceComponent.SetRelativeLocation(FVector(LocalOffset.X, LocalOffset.Y, M_PieceBaseHeight - UndersideZ));
}

UStaticMesh* UWorldDivisionTankFormation::LoadPieceMesh(const ETankSubtype Subtype) const
{
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	if (Settings == nullptr)
	{
		return nullptr;
	}

	const TSoftObjectPtr<UStaticMesh>* const SubtypeMesh = Settings->TankPieceMeshes.Find(Subtype);
	UStaticMesh* const PieceMesh = SubtypeMesh != nullptr
		                               ? UWorldBatteryFireSettings::LoadOptional(*SubtypeMesh, TEXT("TankPieceMeshes"))
		                               : nullptr;
	if (IsValid(PieceMesh))
	{
		return PieceMesh;
	}

	return UWorldBatteryFireSettings::LoadOptional(Settings->FallbackTankPieceMesh, TEXT("FallbackTankPieceMesh"));
}

UStaticMesh* UWorldDivisionTankFormation::LoadWreckMesh(const ETankSubtype Subtype) const
{
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	const TSoftObjectPtr<UStaticMesh>* const SubtypeWreck = Settings != nullptr
		                                                        ? Settings->TankWreckMeshes.Find(Subtype)
		                                                        : nullptr;
	return SubtypeWreck != nullptr
		       ? UWorldBatteryFireSettings::LoadOptional(*SubtypeWreck, TEXT("TankWreckMeshes"))
		       : nullptr;
}

void UWorldDivisionTankFormation::TurnSlotIntoWreck(FWorldDivisionTankSlot& Slot, const bool bWithFire)
{
	Slot.State = EWorldDivisionTankSlotState::Wreck;
	if (not IsValid(Slot.PieceMesh))
	{
		return;
	}

	UStaticMesh* const WreckMesh = LoadWreckMesh(Slot.Subtype);
	if (not IsValid(WreckMesh))
	{
		// No wreck model: the destroyed tank simply disappears.
		Slot.PieceMesh->SetVisibility(false);
		return;
	}

	Slot.PieceMesh->SetStaticMesh(WreckMesh);
	FitPieceToMap(*Slot.PieceMesh, *WreckMesh, Slot.LocalOffset);
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	UNiagaraSystem* const WreckFireEffect = bWithFire && Settings != nullptr
		                                        ? UWorldBatteryFireSettings::LoadOptional(
			                                        Settings->WreckFireEffect, TEXT("WreckFireEffect"))
		                                        : nullptr;
	if (not IsValid(WreckFireEffect))
	{
		return;
	}

	Slot.WreckFire = UNiagaraFunctionLibrary::SpawnSystemAttached(
		WreckFireEffect, Slot.PieceMesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget, false);
	if (IsValid(Slot.WreckFire))
	{
		Slot.WreckFire->SetWorldScale3D(FVector(Settings->WreckFireEffectScale));
		Slot.WreckFire->SetVariableFloat(WorldDivisionTankFormationConstants::WreckFireScaleParameterName,
		                                 Settings->WreckFireEffectScale);
	}
}

void UWorldDivisionTankFormation::HideOwnerMeshes()
{
	const AActor* const Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}

	TArray<UStaticMeshComponent*> OwnerMeshes;
	Owner->GetComponents<UStaticMeshComponent>(OwnerMeshes);
	for (UStaticMeshComponent* const OwnerMesh : OwnerMeshes)
	{
		const bool bIsFormationPiece = M_Slots.ContainsByPredicate([OwnerMesh](const FWorldDivisionTankSlot& Slot)
		{
			return Slot.PieceMesh == OwnerMesh;
		});
		if (not IsValid(OwnerMesh) || bIsFormationPiece || not OwnerMesh->IsVisible())
		{
			continue;
		}

		// Visibility only: the mesh keeps its collision and stays the click target it was.
		OwnerMesh->SetVisibility(false);
		M_HiddenOwnerMeshes.AddUnique(OwnerMesh);
	}
}

void UWorldDivisionTankFormation::RestoreOwnerMeshes()
{
	for (const TWeakObjectPtr<UStaticMeshComponent>& HiddenMesh : M_HiddenOwnerMeshes)
	{
		if (HiddenMesh.IsValid())
		{
			HiddenMesh->SetVisibility(true);
		}
	}
	M_HiddenOwnerMeshes.Reset();
}

UStaticMesh* UWorldDivisionTankFormation::LoadPlateMesh() const
{
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	if (Settings == nullptr || Settings->FormationPlateMesh.IsNull())
	{
		return nullptr;
	}

	return UWorldBatteryFireSettings::LoadOptional(Settings->FormationPlateMesh, TEXT("FormationPlateMesh"));
}

void UWorldDivisionTankFormation::CreatePlate(UStaticMesh* PlateMesh, const TArray<FVector2D>& SlotOffsets)
{
	// The plate mesh is a block of this size with its base at height 0.
	constexpr float PlateMeshSize = 100.f;
	AActor* const Owner = GetOwner();
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	if (not IsValid(PlateMesh) || not IsValid(Owner) || not IsValid(Owner->GetRootComponent()) || Settings == nullptr)
	{
		return;
	}

	const FVector2D FootprintHalfSize = WorldDivisionFormationLayout::GetFootprintHalfSize(SlotOffsets);
	const FVector PlateSize(2.f * (FootprintHalfSize.X + Settings->FormationPlatePadding),
	                        2.f * (FootprintHalfSize.Y + Settings->FormationPlatePadding),
	                        Settings->FormationPlateHeight);
	M_Plate = NewObject<UStaticMeshComponent>(Owner);
	M_Plate->SetStaticMesh(PlateMesh);
	// Like the tanks, the plate never takes the cursor trace: the click box does.
	M_Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_Plate->SetGenerateOverlapEvents(false);
	M_Plate->SetCanEverAffectNavigation(false);
	M_Plate->SetupAttachment(Owner->GetRootComponent());
	M_Plate->RegisterComponent();
	M_Plate->SetRelativeScale3D(PlateSize / PlateMeshSize);
	M_Plate->SetRelativeLocation(FVector::ZeroVector);
}

void UWorldDivisionTankFormation::UpdateClickBox(const TArray<FVector2D>& SlotOffsets)
{
	using namespace WorldDivisionTankFormationConstants;
	AActor* const Owner = GetOwner();
	if (not IsValid(Owner) || not IsValid(Owner->GetRootComponent()))
	{
		return;
	}

	M_ClickBox = NewObject<UBoxComponent>(Owner);
	const FVector2D FootprintHalfSize = WorldDivisionFormationLayout::GetFootprintHalfSize(SlotOffsets);
	M_ClickBox->SetBoxExtent(FVector(FootprintHalfSize.X + ClickBoxPadding, FootprintHalfSize.Y + ClickBoxPadding,
	                                 ClickBoxHalfHeight));
	// Query only on visibility, like the build nodes, so the world controller's cursor trace finds the division.
	M_ClickBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	M_ClickBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	M_ClickBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	M_ClickBox->SetGenerateOverlapEvents(false);
	M_ClickBox->SetCanEverAffectNavigation(false);
	M_ClickBox->SetupAttachment(Owner->GetRootComponent());
	M_ClickBox->RegisterComponent();
	M_ClickBox->SetRelativeLocation(FVector(0.f, 0.f, ClickBoxHalfHeight));
}
