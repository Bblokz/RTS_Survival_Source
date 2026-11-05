// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AddOnArmorPanelsComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UAddOnArmorPanelsComponent::UAddOnArmorPanelsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

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
	SyncZoneFromModuleState(ZoneIndex);
}

void UAddOnArmorPanelsComponent::SyncAllZonesFromModuleState()
{
	for (int32 ZoneIndex = 0; ZoneIndex < M_Zones.Num(); ++ZoneIndex)
	{
		SyncZoneFromModuleState(ZoneIndex);
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
	for (const FAddOnArmorPanel& Panel : Panels)
	{
		M_PanelCentres.Add(OwnerTransform.InverseTransformPosition(MeshTransform.TransformPosition(Panel.MeshSpaceCentre)));
	}
	return true;
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
	SyncZoneFromModuleState(ZoneIndex);
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

void UAddOnArmorPanelsComponent::SyncZoneFromModuleState(const int32 ZoneIndex)
{
	if (not GetIsValidArmorCalculation() || not M_Zones.IsValidIndex(ZoneIndex))
	{
		return;
	}
	FAddOnArmorPanelZoneState& Zone = M_Zones[ZoneIndex];
	const int32 TargetHiddenCount = GetHiddenPanelTarget(
		M_ArmorCalculation->GetModuleSnapshot(Zone.ModuleId), Zone.PanelIndices.Num());
	const int32 PreviousMask = M_HiddenPanelMask;
	while (Zone.HiddenPanelOrder.Num() < TargetHiddenCount && HidePanelClosestToLastHit(Zone))
	{
	}
	while (Zone.HiddenPanelOrder.Num() > TargetHiddenCount)
	{
		RestoreLastHiddenPanel(Zone);
	}
	if (M_HiddenPanelMask != PreviousMask)
	{
		WriteHiddenPanelMask();
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
