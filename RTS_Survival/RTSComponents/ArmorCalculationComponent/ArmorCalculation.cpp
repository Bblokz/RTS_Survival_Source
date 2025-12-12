#include "ArmorCalculation.h"
#include "DrawDebugHelpers.h"
#include "DynamicMesh/MeshTransforms.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h" // For RTSFunctionLibrary::ReportError
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

// Constructor
UArmorCalculation::UArmorCalculation()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
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
	for (int32 i = 0; i < DeveloperSettings::GameBalance::Weapons::MaxArmorPlatesPerMesh; i++)
	{
		M_ArmorSetup.ArmorSettings0[i].ArmorValue = 0.0f;
		M_ArmorSetup.ArmorSettings1[i].ArmorValue = 0.0f;
		M_ArmorSetup.ArmorSettings2[i].ArmorValue = 0.0f;
	}
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
		for (int32 i = 0; i < NumPlatesToCopy; i++)
		{
			M_ArmorSetup.ArmorSettings0[i] = ArmorSettingsForMesh[i];
		}
	}
	else if (M_ArmorSetup.MeshWithArmor1 == nullptr)
	{
		M_ArmorSetup.MeshWithArmor1 = MeshWithArmor;
		for (int32 i = 0; i < NumPlatesToCopy; i++)
		{
			M_ArmorSetup.ArmorSettings1[i] = ArmorSettingsForMesh[i];
		}
	}
	else if (M_ArmorSetup.MeshWithArmor2 == nullptr)
	{
		M_ArmorSetup.MeshWithArmor2 = MeshWithArmor;
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
	}
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
	float CosAngle = FMath::Abs(FMath::Cos(AngleRadians));
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
                                                float& OutAdjustedArmorPenForAngle)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UArmorCalculation::GetEffectiveArmorOnHit);
	const FArmorSettings* SelectedArmorSettings = nullptr;
	UMeshComponent* RegisteredMesh = nullptr;
	const UPrimitiveComponent* HitComponentPtr = WeakHitComponent.Get();
	if (!IdentifyHitMesh(HitComponentPtr, SelectedArmorSettings, RegisteredMesh))
	{
		OutRawArmorValue = 0.0f;
		return 0.0f;
	}

	const FTransform MeshTransform = RegisteredMesh->GetComponentTransform();

	return EvaluateArmorPlatesForHit(SelectedArmorSettings, MeshTransform, HitLocation,
	                                 ProjectileDirection, ImpactNormal,
	                                 OutRawArmorValue, OutAdjustedArmorPenForAngle);
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
	constexpr int32 NumPlates = DeveloperSettings::GameBalance::Weapons::MaxArmorPlatesPerMesh;
	for (int32 i = 0; i < NumPlates; i++)
	{
		const FArmorSettings& ArmorPlate = SelectedArmorSettings[i];
		// Skip unused armor plates.
		if (ArmorPlate.ArmorValue <= 0.0f)
		{
			continue;
		}

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
	if (HitComponent == M_ArmorSetup.MeshWithArmor0)
	{
		OutSelectedArmorSettings = M_ArmorSetup.ArmorSettings0;
		OutRegisteredMesh = M_ArmorSetup.MeshWithArmor0;
		return true;
	}
	if (HitComponent == M_ArmorSetup.MeshWithArmor1)
	{
		OutSelectedArmorSettings = M_ArmorSetup.ArmorSettings1;
		OutRegisteredMesh = M_ArmorSetup.MeshWithArmor1;
		return true;
	}
	if (HitComponent == M_ArmorSetup.MeshWithArmor2)
	{
		OutSelectedArmorSettings = M_ArmorSetup.ArmorSettings2;
		OutRegisteredMesh = M_ArmorSetup.MeshWithArmor2;
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("GetEffectiveArmorOnHit: HitComponent not registered in ArmorCalculation."));
	return false;
}

float UArmorCalculation::EvaluateArmorPlatesForHit(const FArmorSettings* SelectedArmorSettings,
                                                   const FTransform& MeshTransform,
                                                   const FVector& HitLocation,
                                                   const FVector& ProjectileDirection,
                                                   const FVector& ImpactNormal,
                                                   float& OutRawArmorValue,
                                                   float& OutAdjustedArmorPenForAngle) const
{
	const int32 NumPlates = DeveloperSettings::GameBalance::Weapons::MaxArmorPlatesPerMesh;
	for (int32 i = 0; i < NumPlates; i++)
	{
		const FArmorSettings& ArmorPlate = SelectedArmorSettings[i];
		// Skip unused armor plates.
		if (ArmorPlate.ArmorValue <= 0.0f)
		{
			continue;
		}

		// Compute the world transform for this armor plate by combining its local transform with the mesh's transform.
		FTransform WorldArmorTransform = ArmorPlate.ArmorBoxTransform * MeshTransform;
		// Convert the hit location into the armor plate's local space.
		FVector LocalHitLocation = WorldArmorTransform.InverseTransformPosition(HitLocation);

		if (ArmorPlate.ArmorBox.IsInside(LocalHitLocation))
		{
			OutRawArmorValue = ArmorPlate.ArmorValue;
			// Armor plate found.
			// todo instant return after debugging.
			float EffectiveArmor = GetEffectiveArmor(HitLocation, ProjectileDirection, ImpactNormal,
			                                         ArmorPlate.ArmorValue,
			                                         OutAdjustedArmorPenForAngle);
			if (DeveloperSettings::Debugging::GArmorCalculation_Compile_DebugSymbols)
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
	return NoArmorHitGetClosest(SelectedArmorSettings, MeshTransform, HitLocation, ProjectileDirection, ImpactNormal,
	                            OutRawArmorValue, OutAdjustedArmorPenForAngle);
}

float UArmorCalculation::GetEffectiveArmor(const FVector& HitLocation, const FVector& ProjectileDirection,
                                           const FVector& ImpactNormal, const float RawArmorValue,
                                           float& OutAdjustedArmorPenForAngle) const
{
	const float ImpactAngle = CalculateImpactAngle(ProjectileDirection, ImpactNormal);
	return GetArmorAtAngle(RawArmorValue, ImpactAngle, OutAdjustedArmorPenForAngle);
}

float UArmorCalculation::NoArmorHitGetClosest(const FArmorSettings* SelectedArmorSettings,
                                              const FTransform& MeshTransform,
                                              const FVector& HitLocation, const FVector& ProjectileDirection,
                                              const FVector& ImpactNormal,
                                              float& OutRawArmorValue, float& OutAdjustedArmorPenForAngle) const
{
	if (DeveloperSettings::Debugging::GArmorCalculation_Compile_DebugSymbols)
	{
		DrawDebugString(GetWorld(), HitLocation, TEXT("No Armor Hit"), nullptr, FColor::Red, 5.0f, false, 2);
		DrawDebugSphere(GetWorld(), HitLocation, 5.0f, 12, FColor::Red, false, 2.0f);
	}

	// Threshold distance in world units.
	constexpr float DistanceThreshold = 25.0f;
	float BestDistance = TNumericLimits<float>::Max();
	const FArmorSettings* BestPlate = nullptr;
	const int32 NumPlates = DeveloperSettings::GameBalance::Weapons::MaxArmorPlatesPerMesh;

	for (int32 i = 0; i < NumPlates; i++)
	{
		const FArmorSettings& ArmorPlate = SelectedArmorSettings[i];
		if (ArmorPlate.ArmorValue <= 0.0f)
		{
			continue;
		}

		// Calculate the world transform for this armor plate.
		FTransform WorldArmorTransform = ArmorPlate.ArmorBoxTransform * MeshTransform;
		// Compute the center of the armor box in world space.
		FVector ArmorCenter = WorldArmorTransform.TransformPosition(ArmorPlate.ArmorBox.GetCenter());
		const float Distance = FVector::DistSquared(HitLocation, ArmorCenter);
		if (Distance < BestDistance)
		{
			if (Distance < DistanceThreshold)
			{
				// This plate satisfies threshold; no more searching.
				return GetEffectiveArmor(HitLocation, ProjectileDirection, ImpactNormal, ArmorPlate.ArmorValue,
				                         OutAdjustedArmorPenForAngle);
			}
			BestDistance = Distance;
			BestPlate = &ArmorPlate;
		}
	}

	if (BestPlate)
	{
		OutRawArmorValue = BestPlate->ArmorValue;
		// Use helper to compute effective armor based on impact angle.
		OutAdjustedArmorPenForAngle = 0.0f;
		return GetEffectiveArmor(HitLocation, ProjectileDirection, ImpactNormal, BestPlate->ArmorValue,
		                         OutAdjustedArmorPenForAngle);
	}
	return 0.0f;
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
	                              const FColor Color)
	{
		if (!MeshComponent)
		{
			return;
		}
		FTransform MeshTransform = MeshComponent->GetComponentTransform();
		for (int32 i = 0; i < DeveloperSettings::GameBalance::Weapons::MaxArmorPlatesPerMesh; i++)
		{
			const FArmorSettings& ArmorPlate = ArmorSettingsArray[i];
			if (ArmorPlate.ArmorValue <= 0.0f)
			{
				continue;
			}
			FTransform WorldArmorTransform = ArmorPlate.ArmorBoxTransform * MeshTransform;
			DrawDebugBox(World, WorldArmorTransform.GetLocation(), ArmorPlate.ArmorBox.GetExtent(),
			             WorldArmorTransform.GetRotation(), Color, false, 5.0f, 0, 2.0f);
		}
	};

	if (M_ArmorSetup.MeshWithArmor0)
	{
		DebugDrawArmor(M_ArmorSetup.MeshWithArmor0, M_ArmorSetup.ArmorSettings0, FColor::Green);
	}
	if (M_ArmorSetup.MeshWithArmor1)
	{
		DebugDrawArmor(M_ArmorSetup.MeshWithArmor1, M_ArmorSetup.ArmorSettings1, FColor::Purple);
	}
	if (M_ArmorSetup.MeshWithArmor2)
	{
		DebugDrawArmor(M_ArmorSetup.MeshWithArmor2, M_ArmorSetup.ArmorSettings2, FColor::Blue);
	}
}
