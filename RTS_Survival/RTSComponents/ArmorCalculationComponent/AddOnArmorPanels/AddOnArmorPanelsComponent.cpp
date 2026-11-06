// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AddOnArmorPanelsComponent.h"

#include "AddOnArmorPanelDebris.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace AddOnArmorPanelsComponentConstants
{
	// Shot-off panels leave the hull's face, drift away from the impact along the hull and get a small upward kick.
	constexpr float MinOutwardSpeed = 250.f;
	constexpr float MaxOutwardSpeed = 450.f;
	constexpr float MaxSpreadSpeed = 150.f;
	constexpr float MinUpKick = 120.f;
	constexpr float MaxUpKick = 320.f;
	// Tumble, in rad/s.
	constexpr float MinSpinSpeed = 4.f;
	constexpr float MaxSpinSpeed = 14.f;
	// How far below the burst the ground is searched for.
	constexpr float GroundTraceDepth = 2000.f;
}

UAddOnArmorPanelsComponent::UAddOnArmorPanelsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	M_DebrisSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(AddOnArmorPanelDebris::SystemPath));

	// Side skirts: each side's panels cover the upper and lower side plates of the hull.
	FAddOnArmorPanelZoneSettings LeftZone;
	LeftZone.Side = EAddOnArmorPanelSide::Left;
	LeftZone.CoveredPlateTypes = {EArmorPlate::Plate_SideLeft, EArmorPlate::Plate_SideLowerLeft};
	FAddOnArmorPanelZoneSettings RightZone;
	RightZone.Side = EAddOnArmorPanelSide::Right;
	RightZone.CoveredPlateTypes = {EArmorPlate::Plate_SideRight, EArmorPlate::Plate_SideLowerRight};
	M_ZoneSettings = {LeftZone, RightZone};
}

void UAddOnArmorPanelsComponent::InstallAddOnArmorZones(UArmorCalculation* ArmorCalculation)
{
	// The tank may retry its module finalization; the zones install (or report their setup error) once.
	if (M_ArmorCalculation.IsValid())
	{
		return;
	}
	M_ArmorCalculation = ArmorCalculation;
	const UAddOnArmorPanelLayout* Layout = nullptr;
	M_PanelMesh = FindPanelMesh(Layout);
	if (not GetIsValidArmorCalculation() || not GetIsValidPanelMesh())
	{
		return;
	}
	M_ArmorMesh = FindArmorMesh(*ArmorCalculation);
	if (not M_ArmorMesh.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("UAddOnArmorPanelsComponent: neither the panel mesh nor a parent of it is")
			TEXT(" registered for armour on ") + GetNameSafe(GetOwner())
			+ TEXT("; set up the hull armour before the module setup completes."));
		return;
	}
	if (not CachePanelCentres(*M_PanelMesh, *Layout))
	{
		return;
	}
	CacheDebris(*Layout);
	for (const FAddOnArmorPanelZoneSettings& Settings : M_ZoneSettings)
	{
		(void)InstallZone(Settings);
	}
	M_ZoneHitHandle = ArmorCalculation->GetOnAddOnArmorZoneHit().AddUObject(
		this, &UAddOnArmorPanelsComponent::OnAddOnArmorZoneHit);
}

void UAddOnArmorPanelsComponent::OnAddOnArmorModuleStateChanged(const FModuleStateChange& Change)
{
	const int32 ZoneIndex = FindZoneIndexByModuleId(Change.ModuleId);
	if (ZoneIndex == INDEX_NONE)
	{
		return;
	}
	SyncZoneFromModuleState(ZoneIndex, true);
}

void UAddOnArmorPanelsComponent::SyncAllZonesFromModuleState()
{
	for (int32 ZoneIndex = 0; ZoneIndex < M_Zones.Num(); ++ZoneIndex)
	{
		SyncZoneFromModuleState(ZoneIndex, false);
	}
}

void UAddOnArmorPanelsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (M_ArmorCalculation.IsValid() && M_ZoneHitHandle.IsValid())
	{
		M_ArmorCalculation->GetOnAddOnArmorZoneHit().Remove(M_ZoneHitHandle);
	}
	M_ZoneHitHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

bool UAddOnArmorPanelsComponent::GetIsValidPanelMesh() const
{
	if (M_PanelMesh.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_PanelMesh",
		"GetIsValidPanelMesh",
		GetOwner());
	return false;
}

bool UAddOnArmorPanelsComponent::GetIsValidArmorCalculation() const
{
	if (M_ArmorCalculation.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_ArmorCalculation",
		"GetIsValidArmorCalculation",
		GetOwner());
	return false;
}

bool UAddOnArmorPanelsComponent::GetIsValidDebrisSystem() const
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

UStaticMeshComponent* UAddOnArmorPanelsComponent::FindPanelMesh(const UAddOnArmorPanelLayout*& OutLayout) const
{
	OutLayout = nullptr;
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return nullptr;
	}
	TInlineComponentArray<UStaticMeshComponent*> MeshComponents(Owner);
	for (UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		UStaticMesh* Mesh = IsValid(MeshComponent) ? MeshComponent->GetStaticMesh() : nullptr;
		if (not IsValid(Mesh))
		{
			continue;
		}
		OutLayout = Mesh->GetAssetUserData<UAddOnArmorPanelLayout>();
		if (IsValid(OutLayout))
		{
			return MeshComponent;
		}
	}
	return nullptr;
}

UMeshComponent* UAddOnArmorPanelsComponent::FindArmorMesh(const UArmorCalculation& ArmorCalculation) const
{
	// Panels baked into the hull share its armour; a bolted-on panel mesh covers the plates of the mesh it hangs on.
	for (USceneComponent* Component = M_PanelMesh.Get(); IsValid(Component); Component = Component->GetAttachParent())
	{
		UMeshComponent* const Mesh = Cast<UMeshComponent>(Component);
		if (IsValid(Mesh) && ArmorCalculation.GetIsMeshRegisteredForArmor(Mesh))
		{
			return Mesh;
		}
	}
	// A panel mesh beside the hull (both on the track mesh): the hull registers its armour first, turrets later.
	return ArmorCalculation.GetArmorSetup().MeshWithArmor0.Get();
}

bool UAddOnArmorPanelsComponent::CachePanelCentres(const UStaticMeshComponent& PanelMesh,
                                                   const UAddOnArmorPanelLayout& Layout)
{
	const TArray<FAddOnArmorPanel>& Panels = Layout.GetPanels();
	if (Panels.IsEmpty() || Panels.Num() > AddOnArmorPanelMask::MaxPanelsPerMesh)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("UAddOnArmorPanelsComponent: the panel layout of %s has %d panels (1 to %d supported)."),
			*GetNameSafe(PanelMesh.GetStaticMesh()), Panels.Num(), AddOnArmorPanelMask::MaxPanelsPerMesh));
		return false;
	}
	const FTransform OwnerTransform = GetOwner()->GetActorTransform();
	const FTransform MeshTransform = PanelMesh.GetComponentTransform();
	M_PanelCentres.Reset();
	M_PanelMeshSides.Reset();
	for (const FAddOnArmorPanel& Panel : Panels)
	{
		M_PanelCentres.Add(OwnerTransform.InverseTransformPosition(MeshTransform.TransformPosition(Panel.MeshSpaceCentre)));
		M_PanelMeshSides.Add(Panel.Side);
	}
	return true;
}

void UAddOnArmorPanelsComponent::CacheDebris(const UAddOnArmorPanelLayout& Layout)
{
	M_DebrisMesh = Layout.GetDebrisMesh();
	if (not IsValid(M_DebrisMesh))
	{
		RTSFunctionLibrary::ReportError(TEXT("UAddOnArmorPanelsComponent: the panel hull ")
			+ GetNameSafe(M_PanelMesh->GetStaticMesh()) + TEXT(" of ") + GetNameSafe(GetOwner())
			+ TEXT(" has no debris panel mesh, so shot-off panels vanish without debris. Model one panel as its own")
			TEXT(" static mesh and set it with RTS.AddOnArmor.SetPanelDebrisMesh (AddOnArmorPanels/AGENTS.md)."));
		return;
	}
	M_LoadedDebrisSystem = M_DebrisSystem.LoadSynchronous();
	(void)GetIsValidDebrisSystem();
}

bool UAddOnArmorPanelsComponent::InstallZone(const FAddOnArmorPanelZoneSettings& Settings)
{
	FAddOnArmorPanelZoneState Zone;
	// The vehicle's side, not the mesh's: hull components can be mounted rotated on the actor.
	for (int32 PanelIndex = 0; PanelIndex < M_PanelCentres.Num(); ++PanelIndex)
	{
		const EAddOnArmorPanelSide ActorSide = M_PanelCentres[PanelIndex].Y < 0.0
			? EAddOnArmorPanelSide::Left
			: EAddOnArmorPanelSide::Right;
		if (ActorSide == Settings.Side)
		{
			Zone.PanelIndices.Add(PanelIndex);
		}
	}
	if (Zone.PanelIndices.IsEmpty() || Settings.CoveredPlateTypes.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(TEXT("UAddOnArmorPanelsComponent: a zone without panels or covered plates on ")
			+ GetNameSafe(GetOwner()));
		return false;
	}

	UArmorCalculation* ArmorCalculation = M_ArmorCalculation.Get();
	TArray<FAddOnArmorPlateBinding> CoveredPlates;
	for (const EArmorPlate PlateType : Settings.CoveredPlateTypes)
	{
		// Not every hull registers every side plate type (some have no lower side box); cover the ones it has.
		if (not ArmorCalculation->SetAddOnArmorOfPlateType(M_ArmorMesh.Get(), PlateType, Settings.AddOnArmorValue))
		{
			RTSFunctionLibrary::ReportWarning(TEXT("UAddOnArmorPanelsComponent: ") + UEnum::GetValueAsString(PlateType)
				+ TEXT(" is not registered on the armour mesh of ") + GetNameSafe(GetOwner()) + TEXT("; the zone skips it."));
			continue;
		}
		FAddOnArmorPlateBinding Binding;
		Binding.MeshWithArmor = M_ArmorMesh.Get();
		Binding.PlateType = PlateType;
		CoveredPlates.Add(Binding);
	}
	if (CoveredPlates.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(TEXT("UAddOnArmorPanelsComponent: none of a zone's plate types is registered on the")
			TEXT(" armour mesh of ") + GetNameSafe(GetOwner()) + TEXT("; the zone is not installed."));
		return false;
	}

	FVehicleModuleSetup ModuleSetup;
	ModuleSetup.Type = EVehicleModuleTypes::AddOnArmor;
	// Installation and coverage failures are reported by the armour calculation.
	if (not ArmorCalculation->SetupModule(ModuleSetup, Zone.ModuleId)
		|| not ArmorCalculation->SetAddOnArmorPlateCoverage(Zone.ModuleId, CoveredPlates))
	{
		return false;
	}
	M_Zones.Add(Zone);
	return true;
}

void UAddOnArmorPanelsComponent::OnAddOnArmorZoneHit(const int32 ModuleId, const FVector& HullLocalHitPosition)
{
	const int32 ZoneIndex = FindZoneIndexByModuleId(ModuleId);
	if (ZoneIndex == INDEX_NONE)
	{
		return;
	}
	FAddOnArmorPanelZoneState& Zone = M_Zones[ZoneIndex];
	Zone.LastHitPosition = HullLocalHitPosition;
	Zone.bHasLastHit = true;
	SyncZoneFromModuleState(ZoneIndex, true);
}

int32 UAddOnArmorPanelsComponent::FindZoneIndexByModuleId(const int32 ModuleId) const
{
	if (ModuleId == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	return M_Zones.IndexOfByPredicate([ModuleId](const FAddOnArmorPanelZoneState& Zone)
	{
		return Zone.ModuleId == ModuleId;
	});
}

void UAddOnArmorPanelsComponent::SyncZoneFromModuleState(const int32 ZoneIndex, const bool bThrowDebris)
{
	if (not GetIsValidArmorCalculation() || not M_Zones.IsValidIndex(ZoneIndex))
	{
		return;
	}
	FAddOnArmorPanelZoneState& Zone = M_Zones[ZoneIndex];
	const int32 TargetHiddenCount = GetHiddenPanelTarget(
		M_ArmorCalculation->GetModuleSnapshot(Zone.ModuleId), Zone.PanelIndices.Num());
	const int32 PreviousMask = M_HiddenPanelMask;
	const int32 PreviousHiddenCount = Zone.HiddenPanelOrder.Num();
	while (Zone.HiddenPanelOrder.Num() < TargetHiddenCount && HidePanelClosestToLastHit(Zone))
	{
	}
	while (Zone.HiddenPanelOrder.Num() > TargetHiddenCount)
	{
		RestoreLastHiddenPanel(Zone);
	}
	if (M_HiddenPanelMask == PreviousMask)
	{
		return;
	}
	WriteHiddenPanelMask();
	const int32 NewlyHiddenCount = Zone.HiddenPanelOrder.Num() - PreviousHiddenCount;
	if (bThrowDebris && NewlyHiddenCount > 0)
	{
		ThrowPanelDebris(Zone, MakeArrayView(Zone.HiddenPanelOrder).Slice(PreviousHiddenCount, NewlyHiddenCount));
	}
}

int32 UAddOnArmorPanelsComponent::GetHiddenPanelTarget(const FVehicleModuleSnapshot& Snapshot, const int32 PanelCount)
{
	if (not Snapshot.bInstalled || Snapshot.State == EVehicleModuleState::Healthy || PanelCount <= 0)
	{
		return 0;
	}
	if (Snapshot.State == EVehicleModuleState::Destroyed || Snapshot.MaxHp <= 0.f)
	{
		return PanelCount;
	}
	const float LostHealth01 = 1.f - FMath::Clamp(Snapshot.CurrentHp / Snapshot.MaxHp, 0.f, 1.f);
	const int32 ProportionalCount = FMath::CeilToInt32(LostHealth01 * PanelCount);
	// A damaged zone always shows its damage and keeps one panel; only a destroyed zone loses them all.
	return FMath::Max(1, FMath::Min(ProportionalCount, PanelCount - 1));
}

bool UAddOnArmorPanelsComponent::HidePanelClosestToLastHit(FAddOnArmorPanelZoneState& Zone)
{
	const FVector Anchor = Zone.bHasLastHit ? Zone.LastHitPosition : GetZoneCentre(Zone);
	int32 ClosestPanel = INDEX_NONE;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	for (const int32 PanelIndex : Zone.PanelIndices)
	{
		const float DistanceSquared = FVector::DistSquared(M_PanelCentres[PanelIndex], Anchor);
		if (Zone.HiddenPanelOrder.Contains(PanelIndex) || DistanceSquared >= ClosestDistanceSquared)
		{
			continue;
		}
		ClosestPanel = PanelIndex;
		ClosestDistanceSquared = DistanceSquared;
	}
	if (ClosestPanel == INDEX_NONE)
	{
		return false;
	}
	Zone.HiddenPanelOrder.Add(ClosestPanel);
	M_HiddenPanelMask |= 1 << ClosestPanel;
	return true;
}

void UAddOnArmorPanelsComponent::RestoreLastHiddenPanel(FAddOnArmorPanelZoneState& Zone)
{
	const int32 PanelIndex = Zone.HiddenPanelOrder.Pop(EAllowShrinking::No);
	M_HiddenPanelMask &= ~(1 << PanelIndex);
}

FVector UAddOnArmorPanelsComponent::GetZoneCentre(const FAddOnArmorPanelZoneState& Zone) const
{
	FVector Sum = FVector::ZeroVector;
	for (const int32 PanelIndex : Zone.PanelIndices)
	{
		Sum += M_PanelCentres[PanelIndex];
	}
	return Zone.PanelIndices.IsEmpty() ? Sum : Sum / Zone.PanelIndices.Num();
}

void UAddOnArmorPanelsComponent::WriteHiddenPanelMask() const
{
	if (not GetIsValidPanelMesh())
	{
		return;
	}
	M_PanelMesh->SetCustomPrimitiveDataFloat(AddOnArmorPanelMask::HiddenPanelsPrimitiveDataIndex,
	                                         static_cast<float>(M_HiddenPanelMask));
}

void UAddOnArmorPanelsComponent::ThrowPanelDebris(const FAddOnArmorPanelZoneState& Zone,
                                                  const TConstArrayView<int32> PanelIndices) const
{
	using namespace AddOnArmorPanelDebris;
	using namespace AddOnArmorPanelsComponentConstants;
	// A hull without a debris panel was reported once when the zones installed.
	if (PanelIndices.IsEmpty() || not IsValid(M_DebrisMesh) || not GetIsValidPanelMesh() || not GetIsValidDebrisSystem())
	{
		return;
	}
	const AActor* const Owner = GetOwner();
	const FTransform OwnerTransform = Owner->GetActorTransform();
	const FTransform MeshTransform = M_PanelMesh->GetComponentTransform();
	const FQuat MeshRotation = MeshTransform.GetRotation();
	const double PieceScale = MeshTransform.GetMaximumAxisScale();
	const FVector HitLocation = OwnerTransform.TransformPosition(Zone.bHasLastHit ? Zone.LastHitPosition : GetZoneCentre(Zone));
	const FVector OwnerVelocity = Owner->GetVelocity();
	const int32 PieceCount = FMath::Min(PanelIndices.Num(), MaxPiecesPerBurst);
	TArray<FVector4> Starts;
	TArray<FVector4> Velocities;
	TArray<FVector4> Spins;
	TArray<FVector4> Orientations;
	FVector BurstCentre = FVector::ZeroVector;
	for (int32 PieceIndex = 0; PieceIndex < PieceCount; ++PieceIndex)
	{
		const int32 PanelIndex = PanelIndices[PieceIndex];
		const bool bIsMeshRightSide = M_PanelMeshSides[PanelIndex] == EAddOnArmorPanelSide::Right;
		const FVector Centre = OwnerTransform.TransformPosition(M_PanelCentres[PanelIndex]);
		const FVector Outward = MeshRotation.RotateVector(FVector(0.0, bIsMeshRightSide ? 1.0 : -1.0, 0.0));
		// The debris mesh is a left-side panel; a right-side panel is the same plate turned around.
		const FQuat Orientation = bIsMeshRightSide ? MeshRotation * FQuat(FVector::UpVector, UE_PI) : MeshRotation;
		const FVector Velocity = Outward * FMath::FRandRange(MinOutwardSpeed, MaxOutwardSpeed)
			+ (Centre - HitLocation).GetSafeNormal() * FMath::FRandRange(0.f, MaxSpreadSpeed)
			+ FVector::UpVector * FMath::FRandRange(MinUpKick, MaxUpKick)
			+ OwnerVelocity;
		Starts.Add(FVector4(Centre, PieceScale));
		Velocities.Add(FVector4(Velocity, 0.0));
		Spins.Add(FVector4(FMath::VRand(), FMath::FRandRange(MinSpinSpeed, MaxSpinSpeed)));
		Orientations.Add(FVector4(Orientation.X, Orientation.Y, Orientation.Z, Orientation.W));
		BurstCentre += Centre;
	}
	BurstCentre /= PieceCount;
	UNiagaraComponent* const Burst = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(), M_LoadedDebrisSystem, BurstCentre, FRotator::ZeroRotator, FVector::OneVector, false, false,
		ENCPoolMethod::AutoRelease, false);
	if (not IsValid(Burst))
	{
		return;
	}
	Burst->SetVariableStaticMesh(DebrisMeshParameter, M_DebrisMesh);
	Burst->SetVariableInt(PieceCountParameter, PieceCount);
	Burst->SetVariableFloat(GroundHeightParameter, GetGroundHeightBelow(BurstCentre));
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(Burst, PieceStartParameter, Starts);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(Burst, PieceVelocityParameter, Velocities);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(Burst, PieceSpinParameter, Spins);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(Burst, PieceOrientationParameter, Orientations);
	Burst->Activate(true);
}

float UAddOnArmorPanelsComponent::GetGroundHeightBelow(const FVector& WorldLocation) const
{
	const AActor* const Owner = GetOwner();
	const UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return static_cast<float>(Owner->GetActorLocation().Z);
	}
	FHitResult GroundHit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AddOnArmorDebrisGround), false, Owner);
	const FVector TraceEnd = WorldLocation - FVector(0.0, 0.0, AddOnArmorPanelsComponentConstants::GroundTraceDepth);
	if (World->LineTraceSingleByChannel(GroundHit, WorldLocation, TraceEnd, COLLISION_TRACE_LANDSCAPE, QueryParams))
	{
		return static_cast<float>(GroundHit.ImpactPoint.Z);
	}
	return static_cast<float>(Owner->GetActorLocation().Z);
}
