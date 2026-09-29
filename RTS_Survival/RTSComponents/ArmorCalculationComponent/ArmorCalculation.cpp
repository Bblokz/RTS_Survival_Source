#include "ArmorCalculation.h"
#include "DrawDebugHelpers.h"
#include "DynamicMesh/MeshTransforms.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/ShieldComponent/ShieldComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h" // For RTSFunctionLibrary::ReportError
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

namespace ArmorCalculationConstants
{
	// Below this cosine the impact is treated as grazing to avoid dividing armor by (almost) zero.
	constexpr float MinimumImpactCosine = 0.01f;
}

// Constructor
UArmorCalculation::UArmorCalculation()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	for (int32 PlateRegistrationId = 0; PlateRegistrationId < VehicleModuleBalance::MaxPlateBindings;
	     ++PlateRegistrationId)
	{
		M_PlateRuleRows[PlateRegistrationId] = INDEX_NONE;
		M_PlateAddOnContribution[PlateRegistrationId] = 1.f;
	}
	for (int32 SlotIndex = 0; SlotIndex < VehicleModuleBalance::MaxModuleInstances; ++SlotIndex)
	{
		M_AddOnContributionBySlot[SlotIndex] = 1.f;
	}
}

void UArmorCalculation::SetShieldComponent(UShieldComponent* ShieldComponent)
{
	M_ShieldComponent = ShieldComponent;
}

static TMap<EArmorPlate, float> StaticArmorHierarchy =
{
	{EArmorPlate::Plate_Front, 2.0f},
	// Lower glacis will always go first as it is very hard to set up the boundaries for this properly so in case
	// they do overlap we assume that the upper glacis plate's bounds are too wide
	{EArmorPlate::Plate_FrontLowerGlacis, 1.1f},
	{EArmorPlate::Plate_FrontUpperGlacis, 1.f},
	{EArmorPlate::Plate_Rear, 1.f},
	// Lower glacis will always go first, Idem.
	{EArmorPlate::Plate_RearLowerGlacis, 0.8f},
	{EArmorPlate::Plate_RearUpperGlacis, 0.7f},
	{EArmorPlate::Plate_SideLeft, 0.5f},
	{EArmorPlate::Plate_SideRight, 0.5f},
	{EArmorPlate::Plate_SideLowerLeft, 0.5f},
	{EArmorPlate::Plate_SideLowerRight, 0.5f},
	{EArmorPlate::Turret_Cupola, 4.f},
	{EArmorPlate::Turret_Mantlet, 3.f},
	{EArmorPlate::Turret_Front, 2.f},
	{EArmorPlate::Turret_Rear, 0.8f},
	{EArmorPlate::Turret_SideLeft, 0.5f},
	{EArmorPlate::Turret_SideRight, 0.5f},
	{EArmorPlate::Turret_SidesAndRear, 0.5f}
};

void UArmorCalculation::ClearArmorSetup()
{
	M_ArmorSetup.MeshWithArmor0 = nullptr;
	M_ArmorSetup.MeshWithArmor1 = nullptr;
	M_ArmorSetup.MeshWithArmor2 = nullptr;
	M_ArmorSetup.NumArmorPlates0 = 0;
	M_ArmorSetup.NumArmorPlates1 = 0;
	M_ArmorSetup.NumArmorPlates2 = 0;
	for (int32 i = 0; i < DeveloperSettings::GameBalance::Weapons::MaxArmorPlatesPerMesh; i++)
	{
		M_ArmorSetup.ArmorSettings0[i].ArmorValue = 0.0f;
		M_ArmorSetup.ArmorSettings1[i].ArmorValue = 0.0f;
		M_ArmorSetup.ArmorSettings2[i].ArmorValue = 0.0f;
	}
	DisableAddOnCoverageForClearedRegistration();
	RebuildPlateModuleBindings();
	RefreshArmorContributionsAndRearCache();
}

float UArmorCalculation::GetRearArmor() const
{
	return M_RearArmor;
}

bool UArmorCalculation::MultiplyArmorOfPlateType(
	UMeshComponent* MeshWithArmor, const EArmorPlate PlateType, const float ArmorValueMultiplier)
{
	if (not FMath::IsFinite(ArmorValueMultiplier) || ArmorValueMultiplier < 0.0f)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("MultiplyArmorOfPlateType: ArmorValueMultiplier must be finite and nonnegative"));
		return false;
	}

	const TArrayView<FArmorSettings> ArmorSettings = GetMutableArmorSettingsForMesh(MeshWithArmor);
	bool bUpdatedArmor = false;
	for (FArmorSettings& ArmorSetting : ArmorSettings)
	{
		if (ArmorSetting.ArmorType != PlateType)
		{
			continue;
		}

		ArmorSetting.ArmorValue *= ArmorValueMultiplier;
		bUpdatedArmor = true;
	}

	if (bUpdatedArmor)
	{
		RefreshRearArmorCache();
	}
	return bUpdatedArmor;
}

bool UArmorCalculation::SetArmorOfPlateType(
	UMeshComponent* MeshWithArmor, const EArmorPlate PlateType, const float NewArmorValue)
{
	if (not FMath::IsFinite(NewArmorValue) || NewArmorValue < 0.0f)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("SetArmorOfPlateType: NewArmorValue must be finite and nonnegative"));
		return false;
	}

	const TArrayView<FArmorSettings> ArmorSettings = GetMutableArmorSettingsForMesh(MeshWithArmor);
	bool bUpdatedArmor = false;
	for (FArmorSettings& ArmorSetting : ArmorSettings)
	{
		if (ArmorSetting.ArmorType != PlateType)
		{
			continue;
		}

		ArmorSetting.ArmorValue = NewArmorValue;
		bUpdatedArmor = true;
	}

	if (bUpdatedArmor)
	{
		RefreshRearArmorCache();
	}
	return bUpdatedArmor;
}

TArrayView<FArmorSettings> UArmorCalculation::GetMutableArmorSettingsForMesh(const UMeshComponent* MeshWithArmor)
{
	if (not IsValid(MeshWithArmor))
	{
		RTSFunctionLibrary::ReportError(TEXT("GetMutableArmorSettingsForMesh: MeshWithArmor is invalid"));
		return TArrayView<FArmorSettings>();
	}

	if (MeshWithArmor == M_ArmorSetup.MeshWithArmor0)
	{
		return MakeArrayView(M_ArmorSetup.ArmorSettings0, M_ArmorSetup.NumArmorPlates0);
	}
	if (MeshWithArmor == M_ArmorSetup.MeshWithArmor1)
	{
		return MakeArrayView(M_ArmorSetup.ArmorSettings1, M_ArmorSetup.NumArmorPlates1);
	}
	if (MeshWithArmor == M_ArmorSetup.MeshWithArmor2)
	{
		return MakeArrayView(M_ArmorSetup.ArmorSettings2, M_ArmorSetup.NumArmorPlates2);
	}

	RTSFunctionLibrary::ReportError(TEXT("GetMutableArmorSettingsForMesh: MeshWithArmor is not registered"));
	return TArrayView<FArmorSettings>();
}

void UArmorCalculation::ApplyArmorValueMultiplierToMatchingPlates(
	const TArray<EArmorPlate>& ArmorPlatesToAdjust,
	const float ArmorValueMultiplier)
{
	if (ArmorPlatesToAdjust.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ApplyArmorValueMultiplierToMatchingPlates: ArmorPlatesToAdjust is empty"));
		return;
	}

	if (ArmorValueMultiplier <= 0.0f)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("ApplyArmorValueMultiplierToMatchingPlates: ArmorValueMultiplier must be positive"));
		return;
	}

	ApplyArmorValueMultiplierToArmorSettings(M_ArmorSetup.ArmorSettings0, M_ArmorSetup.NumArmorPlates0,
	                                         ArmorPlatesToAdjust, ArmorValueMultiplier);
	ApplyArmorValueMultiplierToArmorSettings(M_ArmorSetup.ArmorSettings1, M_ArmorSetup.NumArmorPlates1,
	                                         ArmorPlatesToAdjust, ArmorValueMultiplier);
	ApplyArmorValueMultiplierToArmorSettings(M_ArmorSetup.ArmorSettings2, M_ArmorSetup.NumArmorPlates2,
	                                         ArmorPlatesToAdjust, ArmorValueMultiplier);
	RefreshRearArmorCache();
}

void UArmorCalculation::ApplyArmorValueMultiplierToArmorSettings(
	FArmorSettings* ArmorSettings,
	const int32 NumRegisteredPlates,
	const TArray<EArmorPlate>& ArmorPlatesToAdjust,
	const float ArmorValueMultiplier)
{
	if (ArmorSettings == nullptr)
	{
		return;
	}

	const int32 NumPlatesToAdjust = FMath::Clamp(NumRegisteredPlates, 0,
	                                             DeveloperSettings::GameBalance::Weapons::MaxArmorPlatesPerMesh);
	for (int32 ArmorPlateIndex = 0; ArmorPlateIndex < NumPlatesToAdjust; ArmorPlateIndex++)
	{
		FArmorSettings& ArmorSetting = ArmorSettings[ArmorPlateIndex];
		if (ArmorSetting.ArmorValue <= 0.0f)
		{
			continue;
		}

		if (not ArmorPlatesToAdjust.Contains(ArmorSetting.ArmorType))
		{
			continue;
		}

		ArmorSetting.ArmorValue *= ArmorValueMultiplier;
	}
}

void UArmorCalculation::RefreshRearArmorCache()
{
	M_RearArmor = 0.0f;

	for (int32 ArmorMeshSlot = 0; ArmorMeshSlot < VehicleModuleBalance::MaxRegisteredArmorMeshes; ++ArmorMeshSlot)
	{
		if (TryRefreshRearArmorCacheFromSettings(ArmorMeshSlot))
		{
			return;
		}
	}
}

bool UArmorCalculation::TryRefreshRearArmorCacheFromSettings(const int32 ArmorMeshSlot)
{
	const FArmorSettings* ArmorSettings = GetArmorSettingsForSlot(ArmorMeshSlot);
	const int32 NumPlates = GetRegisteredPlateCountForSlot(ArmorMeshSlot);
	if (ArmorSettings == nullptr)
	{
		return false;
	}

	for (int32 PlateIndex = 0; PlateIndex < NumPlates; ++PlateIndex)
	{
		if (not GetIsRearHullArmor(ArmorSettings[PlateIndex].ArmorType))
		{
			continue;
		}

		M_RearArmor = GetPlateArmorValue(ArmorMeshSlot, PlateIndex);
		return true;
	}

	return false;
}

void UArmorCalculation::BeginPlay()
{
	Super::BeginPlay();
}

float UArmorCalculation::GetDamageFromHitPlate(const FArmorSettings& ArmorPlate,
                                               const FDamageMltPerSide ResistanceMultipliers,
                                               const float BaseDamage) const
{
	const float Mlt = Global_GetResistanceMultiplierFromPlate(ResistanceMultipliers, ArmorPlate.ArmorType);
	FRTSWeaponHelpers::Debug_Resistances("Damage Multiplier: " + FString::SanitizeFloat(Mlt) + " \n armor plate"
		+ UEnum::GetValueAsString(ArmorPlate.ArmorType));
	return BaseDamage * Mlt;
}

void UArmorCalculation::InitArmorCalculation(
	UMeshComponent* MeshWithArmor,
	TArray<FArmorSettings> ArmorSettingsForMesh,
	const uint8 PlayerOwningArmor)
{
	if (!MeshWithArmor)
	{
		RTSFunctionLibrary::ReportError(TEXT("InitArmorCalculation: MeshWithArmor is null"));
		return;
	}
	SortArmorArray(ArmorSettingsForMesh);
	// Debug_PostSort(ArmorSettingsForMesh);
	FRTS_CollisionSetup::SetupArmorCalculationMeshCollision(MeshWithArmor, PlayerOwningArmor, false);

	// For each armor setting scale the box bounds by the scale in the transform.
	for (FArmorSettings& ArmorSetting : ArmorSettingsForMesh)
	{
		ArmorSetting.ArmorBox.Max *= ArmorSetting.ArmorBoxTransform.GetScale3D();
		ArmorSetting.ArmorBox.Min *= ArmorSetting.ArmorBoxTransform.GetScale3D();
	}

	// Limit the number of plates copied to the fixed maximum.
	const int32 NumPlatesToCopy = FMath::Min(ArmorSettingsForMesh.Num(),
	                                         DeveloperSettings::GameBalance::Weapons::MaxArmorPlatesPerMesh);

	if (M_ArmorSetup.MeshWithArmor0 == nullptr)
	{
		M_ArmorSetup.MeshWithArmor0 = MeshWithArmor;
		M_ArmorSetup.NumArmorPlates0 = NumPlatesToCopy;
		for (int32 i = 0; i < NumPlatesToCopy; i++)
		{
			M_ArmorSetup.ArmorSettings0[i] = ArmorSettingsForMesh[i];
		}
	}
	else if (M_ArmorSetup.MeshWithArmor1 == nullptr)
	{
		M_ArmorSetup.MeshWithArmor1 = MeshWithArmor;
		M_ArmorSetup.NumArmorPlates1 = NumPlatesToCopy;
		for (int32 i = 0; i < NumPlatesToCopy; i++)
		{
			M_ArmorSetup.ArmorSettings1[i] = ArmorSettingsForMesh[i];
		}
	}
	else if (M_ArmorSetup.MeshWithArmor2 == nullptr)
	{
		M_ArmorSetup.MeshWithArmor2 = MeshWithArmor;
		M_ArmorSetup.NumArmorPlates2 = NumPlatesToCopy;
		for (int32 i = 0; i < NumPlatesToCopy; i++)
		{
			M_ArmorSetup.ArmorSettings2[i] = ArmorSettingsForMesh[i];
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(
				TEXT("InitArmorCalculation: No available armor slot for Mesh %s"), *MeshWithArmor->GetName()));
		return;
	}

	// Registration changes plate IDs, so module routes and add-on contributions are rebuilt once here.
	RebuildPlateModuleBindings();
	RefreshArmorContributionsAndRearCache();
}

FDamageMltPerSide UArmorCalculation::GetResistanceForDamageType(const ERTSDamageType DamageType) const
{
	switch (DamageType)
	{
	case ERTSDamageType::Kinetic:
		RTSFunctionLibrary::ReportError("Tested for resistance type to get for damage type but found kenetic! This"
			"should have been handled by an armor plate instead!");
		return M_LaserRadiationDamageMlt.LaserMltPerPart;
	case ERTSDamageType::Fire:
		RTSFunctionLibrary::ReportError("Tested for resistance type to get for damage type but found Flame! This"
			"should have been handled by an armor plate instead!");
		return M_LaserRadiationDamageMlt.LaserMltPerPart;
	case ERTSDamageType::Laser:
		return M_LaserRadiationDamageMlt.LaserMltPerPart;
	case ERTSDamageType::Radiation:
		return M_LaserRadiationDamageMlt.RadiationMltPerPart;
	case ERTSDamageType::None:
		break;
	}
	RTSFunctionLibrary::ReportError("Could not find reistance for damage type."
		"See UArmorCalculatin::GetResistanceForDamageType");
	return M_LaserRadiationDamageMlt.LaserMltPerPart;
}

float UArmorCalculation::CalculateImpactAngle(const FVector& ProjectileDirection, const FVector& ImpactNormal) const
{
	FVector ImpactDir = ProjectileDirection.GetSafeNormal();
	float DotProduct = FVector::DotProduct(ImpactDir, ImpactNormal);
	float ClampedDot = FMath::Clamp(DotProduct, -1.0f, 1.0f);
	float ImpactAngleRadians = FMath::Acos(ClampedDot);
	return FMath::RadiansToDegrees(ImpactAngleRadians);
}

float UArmorCalculation::GetArmorAtAngle(float ArmorValue, float AngleDegrees, float& OutPenAdjusted) const
{
	float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
	// Clamp so grazing hits produce very high, but finite, effective armor.
	float CosAngle = FMath::Max(FMath::Abs(FMath::Cos(AngleRadians)), ArmorCalculationConstants::MinimumImpactCosine);
	constexpr float Decay = DeveloperSettings::GameBalance::Weapons::PenetrationExponentialDecayFactor;
	float ReductionFactor = FMath::Exp(-Decay * (1.0f - CosAngle));
	OutPenAdjusted *= ReductionFactor;
	return ArmorValue / CosAngle;
}

float UArmorCalculation::GetEffectiveArmorOnHit(TWeakObjectPtr<UPrimitiveComponent> WeakHitComponent,
                                                const FVector& HitLocation,
                                                const FVector& ProjectileDirection,
                                                const FVector& ImpactNormal,
                                                float& OutRawArmorValue,
                                                float& OutAdjustedArmorPenForAngle, EArmorPlate& OutPlateHit)
{
	int32 PlateRegistrationId = INDEX_NONE;
	return GetEffectiveArmorOnHit(WeakHitComponent, HitLocation, ProjectileDirection, ImpactNormal,
	                              OutRawArmorValue, OutAdjustedArmorPenForAngle, OutPlateHit, PlateRegistrationId);
}

float UArmorCalculation::GetEffectiveArmorOnHit(TWeakObjectPtr<UPrimitiveComponent> WeakHitComponent,
                                                const FVector& HitLocation,
                                                const FVector& ProjectileDirection,
                                                const FVector& ImpactNormal,
                                                float& OutRawArmorValue,
                                                float& OutAdjustedArmorPenForAngle, EArmorPlate& OutPlateHit,
                                                int32& OutPlateRegistrationId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UArmorCalculation::GetEffectiveArmorOnHit);
	// An unresolved hit must not silently become Plate_Front for module routing.
	OutPlateHit = static_cast<EArmorPlate>(VehicleModuleBalance::ArmorPlateRuleCount);
	OutPlateRegistrationId = INDEX_NONE;
	const UPrimitiveComponent* HitComponentPtr = WeakHitComponent.Get();
	const int32 ArmorMeshSlot = GetArmorMeshSlot(HitComponentPtr);
	if (ArmorMeshSlot == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("GetEffectiveArmorOnHit: HitComponent not registered in ArmorCalculation."));
		OutRawArmorValue = 0.0f;
		return 0.0f;
	}

	const FTransform MeshTransform = GetArmorMeshForSlot(ArmorMeshSlot)->GetComponentTransform();
	int32 PlateIndex = INDEX_NONE;
	const float EffectiveArmor = EvaluateArmorPlatesForHit(ArmorMeshSlot, MeshTransform, HitLocation,
	                                                       ProjectileDirection, ImpactNormal,
	                                                       OutRawArmorValue, OutAdjustedArmorPenForAngle,
	                                                       OutPlateHit, PlateIndex);
	if (PlateIndex != INDEX_NONE)
	{
		OutPlateRegistrationId = MakePlateRegistrationId(ArmorMeshSlot, PlateIndex);
	}
	return EffectiveArmor;
}

float UArmorCalculation::GetEffectiveDamageOnHit(
	const ERTSDamageType DamageType,
	const float BaseDamage,
	const TWeakObjectPtr<UPrimitiveComponent> WeakHitComponent,
	const FVector& HitLocation) const
{
	const FDamageMltPerSide ResistancePerSide = GetResistanceForDamageType(DamageType);
	const FArmorSettings* SelectedArmorSettings = nullptr;
	UMeshComponent* RegisteredMesh = nullptr;
	const UPrimitiveComponent* HitComponentPtr = WeakHitComponent.Get();
	if (!IdentifyHitMesh(HitComponentPtr, SelectedArmorSettings, RegisteredMesh))
	{
		FRTSWeaponHelpers::Debug_ResistancesAtLocation(HitLocation, this, "Could not identify hit mesh"
		                                               "\n will use front", FColor::Red);
		return ResistancePerSide.FrontMlt * BaseDamage;
	}

	const FTransform MeshTransform = RegisteredMesh->GetComponentTransform();

	return GetDamageOnArmorPlateResistanceAdjusted(
		BaseDamage,
		SelectedArmorSettings,
		ResistancePerSide,
		MeshTransform,
		HitLocation);
}


float UArmorCalculation::GetDamageOnArmorPlateResistanceAdjusted(
	const float BaseDamage,
	const FArmorSettings* SelectedArmorSettings,
	const FDamageMltPerSide ResistanceMultipliers,
	const FTransform& MeshTransform,
	const FVector& HitLocation
) const
{
	const int32 NumPlates = GetRegisteredPlateCount(SelectedArmorSettings);
	for (int32 i = 0; i < NumPlates; i++)
	{
		const FArmorSettings& ArmorPlate = SelectedArmorSettings[i];

		// Compute the world transform for this armor plate by combining its local transform with the mesh's transform.
		FTransform WorldArmorTransform = ArmorPlate.ArmorBoxTransform * MeshTransform;
		// Convert the hit location into the armor plate's local space.
		FVector LocalHitLocation = WorldArmorTransform.InverseTransformPosition(HitLocation);

		if (ArmorPlate.ArmorBox.IsInside(LocalHitLocation))
		{
			return GetDamageFromHitPlate(ArmorPlate, ResistanceMultipliers, BaseDamage);
		}
	}
	// No plate found; use front resistance.
	RTSFunctionLibrary::ReportWarning("No plate hit for special damage type; using front mlt"
		"\n eventhough selected armor settings of mesh context was obtained");
	return BaseDamage * ResistanceMultipliers.FrontMlt;
}


bool UArmorCalculation::IdentifyHitMesh(const UPrimitiveComponent* HitComponent,
                                        const FArmorSettings*& OutSelectedArmorSettings,
                                        UMeshComponent*& OutRegisteredMesh) const
{
	const int32 ArmorMeshSlot = GetArmorMeshSlot(HitComponent);
	if (ArmorMeshSlot == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("GetEffectiveArmorOnHit: HitComponent not registered in ArmorCalculation."));
		return false;
	}
	OutSelectedArmorSettings = GetArmorSettingsForSlot(ArmorMeshSlot);
	OutRegisteredMesh = GetArmorMeshForSlot(ArmorMeshSlot);
	return true;
}

float UArmorCalculation::EvaluateArmorPlatesForHit(const int32 ArmorMeshSlot,
                                                   const FTransform& MeshTransform,
                                                   const FVector& HitLocation,
                                                   const FVector& ProjectileDirection,
                                                   const FVector& ImpactNormal,
                                                   float& OutRawArmorValue,
                                                   float& OutAdjustedArmorPenForAngle, EArmorPlate& OutPlateHit,
                                                   int32& OutPlateIndex) const
{
	const FArmorSettings* SelectedArmorSettings = GetArmorSettingsForSlot(ArmorMeshSlot);
	const int32 NumPlates = GetRegisteredPlateCountForSlot(ArmorMeshSlot);
	for (int32 i = 0; i < NumPlates; i++)
	{
		const FArmorSettings& ArmorPlate = SelectedArmorSettings[i];

		// Compute the world transform for this armor plate by combining its local transform with the mesh's transform.
		FTransform WorldArmorTransform = ArmorPlate.ArmorBoxTransform * MeshTransform;
		// Convert the hit location into the armor plate's local space.
		FVector LocalHitLocation = WorldArmorTransform.InverseTransformPosition(HitLocation);

		if (ArmorPlate.ArmorBox.IsInside(LocalHitLocation))
		{
			// Registered zero-armor plates keep their identity for module routing.
			const float PlateArmorValue = GetPlateArmorValue(ArmorMeshSlot, i);
			OutRawArmorValue = PlateArmorValue;
			OutPlateHit = ArmorPlate.ArmorType;
			OutPlateIndex = i;
			// Armor plate found.
			// todo instant return after debugging.
			float EffectiveArmor = GetEffectiveArmor(HitLocation, ProjectileDirection, ImpactNormal,
			                                         PlateArmorValue,
			                                         OutAdjustedArmorPenForAngle);
			if constexpr (DeveloperSettings::Debugging::GArmorCalculation_Compile_DebugSymbols)
			{
				if (OutAdjustedArmorPenForAngle >= EffectiveArmor)
				{
					DrawDebugString(GetWorld(), HitLocation, UEnum::GetValueAsString(ArmorPlate.ArmorType), nullptr,
					                FColor::Green, 5.0f, false, 1);
				}
			}
			return EffectiveArmor;
		}
	}
	// No armor plate found; return closest plate instead.
	return NoArmorHitGetClosest(ArmorMeshSlot, MeshTransform, HitLocation, ProjectileDirection, ImpactNormal,
	                            OutRawArmorValue, OutAdjustedArmorPenForAngle, OutPlateHit, OutPlateIndex);
}

float UArmorCalculation::GetEffectiveArmor(const FVector& HitLocation, const FVector& ProjectileDirection,
                                           const FVector& ImpactNormal, const float RawArmorValue,
                                           float& OutAdjustedArmorPenForAngle) const
{
	const float ImpactAngle = CalculateImpactAngle(ProjectileDirection, ImpactNormal);
	return GetArmorAtAngle(RawArmorValue, ImpactAngle, OutAdjustedArmorPenForAngle);
}

float UArmorCalculation::NoArmorHitGetClosest(const int32 ArmorMeshSlot,
                                              const FTransform& MeshTransform,
                                              const FVector& HitLocation, const FVector& ProjectileDirection,
                                              const FVector& ImpactNormal,
                                              float& OutRawArmorValue, float& OutAdjustedArmorPenForAngle,
                                              EArmorPlate& OutPlateHit, int32& OutPlateIndex) const
{
	if constexpr (DeveloperSettings::Debugging::GArmorCalculation_Compile_DebugSymbols)
	{
		DrawDebugString(GetWorld(), HitLocation, TEXT("No Armor Hit"), nullptr, FColor::Red, 5.0f, false, 2);
		DrawDebugSphere(GetWorld(), HitLocation, 5.0f, 12, FColor::Red, false, 2.0f);
	}

	// Threshold distance in world units; compared against squared distances below.
	constexpr float DistanceThreshold = 25.0f;
	constexpr float DistanceThresholdSquared = DistanceThreshold * DistanceThreshold;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	int32 BestPlateIndex = INDEX_NONE;
	const FArmorSettings* SelectedArmorSettings = GetArmorSettingsForSlot(ArmorMeshSlot);
	const int32 NumPlates = GetRegisteredPlateCountForSlot(ArmorMeshSlot);

	for (int32 i = 0; i < NumPlates; i++)
	{
		const FArmorSettings& ArmorPlate = SelectedArmorSettings[i];

		// Calculate the world transform for this armor plate.
		FTransform WorldArmorTransform = ArmorPlate.ArmorBoxTransform * MeshTransform;
		// Compute the center of the armor box in world space.
		FVector ArmorCenter = WorldArmorTransform.TransformPosition(ArmorPlate.ArmorBox.GetCenter());
		const float DistanceSquared = FVector::DistSquared(HitLocation, ArmorCenter);
		if (DistanceSquared >= BestDistanceSquared)
		{
			continue;
		}
		BestDistanceSquared = DistanceSquared;
		BestPlateIndex = i;
		if (DistanceSquared < DistanceThresholdSquared)
		{
			// This plate satisfies the threshold; no more searching.
			break;
		}
	}

	if (BestPlateIndex == INDEX_NONE)
	{
		OutRawArmorValue = 0.0f;
		return 0.0f;
	}

	// Fill every output and keep the incoming penetration; GetEffectiveArmor adjusts it for the angle.
	const float PlateArmorValue = GetPlateArmorValue(ArmorMeshSlot, BestPlateIndex);
	OutRawArmorValue = PlateArmorValue;
	OutPlateHit = SelectedArmorSettings[BestPlateIndex].ArmorType;
	OutPlateIndex = BestPlateIndex;
	return GetEffectiveArmor(HitLocation, ProjectileDirection, ImpactNormal, PlateArmorValue,
	                         OutAdjustedArmorPenForAngle);
}

bool UArmorCalculation::GetIsRearHullArmor(const EArmorPlate Plate) const
{
	return Plate == EArmorPlate::Plate_Rear || Plate == EArmorPlate::Plate_RearLowerGlacis || Plate ==
		EArmorPlate::Plate_RearUpperGlacis;
}


void UArmorCalculation::SortArmorArray(TArray<FArmorSettings>& OutSortedArmorSettings) const
{
	OutSortedArmorSettings.Sort([](const FArmorSettings& A, const FArmorSettings& B)
	{
		const float ValueA = StaticArmorHierarchy.Contains(A.ArmorType) ? StaticArmorHierarchy[A.ArmorType] : 0.f;
		const float ValueB = StaticArmorHierarchy.Contains(B.ArmorType) ? StaticArmorHierarchy[B.ArmorType] : 0.f;
		if (FMath::IsNearlyEqual(ValueA, ValueB))
		{
			// If same hierarchy priority, sort by higher ArmorValue first.
			return A.ArmorValue > B.ArmorValue;
		}
		return ValueA > ValueB;
	});
}


void UArmorCalculation::Debug_PostSort(TArray<FArmorSettings>& ArmorSettingsSorted) const
{
	FString DebugString = TEXT("Armor Settings Sorted: ");
	int Counter = 0;
	for (auto EachSetting : ArmorSettingsSorted)
	{
		FString EnumValueAsString = UEnum::GetValueAsString(EachSetting.ArmorType);
		DebugString += "\n" + FString::FromInt(Counter) + "-- ArmorType : " + EnumValueAsString;
		Counter++;
	}
	RTSFunctionLibrary::PrintString(DebugString);
}


void UArmorCalculation::DebugArmorPlates() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	auto DebugDrawArmor = [World](UMeshComponent* MeshComponent, const FArmorSettings* ArmorSettingsArray,
	                              const int32 NumPlates, const FColor Color)
	{
		if (!MeshComponent)
		{
			return;
		}
		FTransform MeshTransform = MeshComponent->GetComponentTransform();
		for (int32 i = 0; i < NumPlates; i++)
		{
			const FArmorSettings& ArmorPlate = ArmorSettingsArray[i];
			FTransform WorldArmorTransform = ArmorPlate.ArmorBoxTransform * MeshTransform;
			DrawDebugBox(World, WorldArmorTransform.GetLocation(), ArmorPlate.ArmorBox.GetExtent(),
			             WorldArmorTransform.GetRotation(), Color, false, 5.0f, 0, 2.0f);
		}
	};

	if (M_ArmorSetup.MeshWithArmor0)
	{
		DebugDrawArmor(M_ArmorSetup.MeshWithArmor0, M_ArmorSetup.ArmorSettings0, M_ArmorSetup.NumArmorPlates0,
		               FColor::Green);
	}
	if (M_ArmorSetup.MeshWithArmor1)
	{
		DebugDrawArmor(M_ArmorSetup.MeshWithArmor1, M_ArmorSetup.ArmorSettings1, M_ArmorSetup.NumArmorPlates1,
		               FColor::Purple);
	}
	if (M_ArmorSetup.MeshWithArmor2)
	{
		DebugDrawArmor(M_ArmorSetup.MeshWithArmor2, M_ArmorSetup.ArmorSettings2, M_ArmorSetup.NumArmorPlates2,
		               FColor::Blue);
	}
}

int32 UArmorCalculation::GetArmorMeshSlot(const UPrimitiveComponent* Component) const
{
	if (not IsValid(Component))
	{
		return INDEX_NONE;
	}
	if (Component == M_ArmorSetup.MeshWithArmor0)
	{
		return 0;
	}
	if (Component == M_ArmorSetup.MeshWithArmor1)
	{
		return 1;
	}
	if (Component == M_ArmorSetup.MeshWithArmor2)
	{
		return 2;
	}
	return INDEX_NONE;
}

UMeshComponent* UArmorCalculation::GetArmorMeshForSlot(const int32 ArmorMeshSlot) const
{
	switch (ArmorMeshSlot)
	{
	case 0:
		return M_ArmorSetup.MeshWithArmor0;
	case 1:
		return M_ArmorSetup.MeshWithArmor1;
	case 2:
		return M_ArmorSetup.MeshWithArmor2;
	default:
		return nullptr;
	}
}

const FArmorSettings* UArmorCalculation::GetArmorSettingsForSlot(const int32 ArmorMeshSlot) const
{
	switch (ArmorMeshSlot)
	{
	case 0:
		return M_ArmorSetup.ArmorSettings0;
	case 1:
		return M_ArmorSetup.ArmorSettings1;
	case 2:
		return M_ArmorSetup.ArmorSettings2;
	default:
		return nullptr;
	}
}

int32 UArmorCalculation::GetRegisteredPlateCountForSlot(const int32 ArmorMeshSlot) const
{
	if (GetArmorMeshForSlot(ArmorMeshSlot) == nullptr)
	{
		return 0;
	}
	switch (ArmorMeshSlot)
	{
	case 0:
		return M_ArmorSetup.NumArmorPlates0;
	case 1:
		return M_ArmorSetup.NumArmorPlates1;
	case 2:
		return M_ArmorSetup.NumArmorPlates2;
	default:
		return 0;
	}
}

int32 UArmorCalculation::GetRegisteredPlateCount(const FArmorSettings* Settings) const
{
	for (int32 ArmorMeshSlot = 0; ArmorMeshSlot < VehicleModuleBalance::MaxRegisteredArmorMeshes; ++ArmorMeshSlot)
	{
		if (Settings == GetArmorSettingsForSlot(ArmorMeshSlot))
		{
			return GetRegisteredPlateCountForSlot(ArmorMeshSlot);
		}
	}
	return 0;
}

int32 UArmorCalculation::MakePlateRegistrationId(const int32 ArmorMeshSlot, const int32 PlateIndex)
{
	return ArmorMeshSlot * VehicleModuleBalance::MaxArmorPlatesPerRegisteredMesh + PlateIndex;
}

float UArmorCalculation::GetPlateArmorValue(const int32 ArmorMeshSlot, const int32 PlateIndex) const
{
	const FArmorSettings* ArmorSettings = GetArmorSettingsForSlot(ArmorMeshSlot);
	if (ArmorSettings == nullptr || PlateIndex < 0 || PlateIndex >= GetRegisteredPlateCountForSlot(ArmorMeshSlot))
	{
		return 0.f;
	}
	const FArmorSettings& Plate = ArmorSettings[PlateIndex];
	const int32 PlateRegistrationId = MakePlateRegistrationId(ArmorMeshSlot, PlateIndex);
	return Plate.ArmorValue + Plate.AddOnArmorValue * M_PlateAddOnContribution[PlateRegistrationId];
}
