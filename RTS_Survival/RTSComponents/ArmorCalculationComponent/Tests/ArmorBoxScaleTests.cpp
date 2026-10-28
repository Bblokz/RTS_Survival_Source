// Copyright (C) Bas Blokzijl - All rights reserved.

// QA for the armour-box scale fix (B4b, 2026-10-27). Live hit test: InitArmorCalculation multiplies the box by the
// transform scale and the hit test divides the hit by that scale again, so a box with scale s covers 32*s^2 per axis.
// The fix keeps that legacy coverage and sorts unit-scale boxes (authored with their true half extents) first, so a
// variant's scale-1 box inside an inflated inherited box gets hit.
// RTS.Armor.BoxScale.* are fast unit tests; RTS.ArmorAudit.BoxScaleImpact loads every tank Blueprint, runs its
// SetupArmor and reports how the resolved plates change (stress filter, run on demand).
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"

/** @brief Reads the private plate storage and the sort helper of UArmorCalculation (friend). */
struct FArmorCalculationTestAccess
{
	static void Sort(TArray<FArmorSettings>& InOutSettings, const bool bUnitScaleBoxesFirst)
	{
		UArmorCalculation::SortArmorSettings(InOutSettings, bUnitScaleBoxesFirst);
	}

	static bool IsUnitScale(const FArmorSettings& Settings)
	{
		return UArmorCalculation::GetIsUnitScaleArmorBox(Settings);
	}

	static bool IsPromoted(const FArmorSettings& Settings)
	{
		return UArmorCalculation::GetIsPromotedArmorBox(Settings);
	}

	/**
	 * Pre-fix order rebuilt from the stored (fixed) order. The fix only moves promoted plates in front with a stable
	 * pass, so both groups are still in their old relative order; merging them with the legacy comparator gives the
	 * old order (ties between the groups go to the scaled plate). Merging instead of re-sorting also keeps plates whose
	 * armor a Blueprint changed after InitArmorCalculation (e.g. MultiplyArmorOfPlateType) in their registered place.
	 */
	static TArray<FArmorSettings> RebuildLegacyOrder(const TArray<FArmorSettings>& Stored)
	{
		TArray<FArmorSettings> Promoted;
		TArray<FArmorSettings> Others;
		for (const FArmorSettings& Plate : Stored)
		{
			(UArmorCalculation::GetIsPromotedArmorBox(Plate) ? Promoted : Others).Add(Plate);
		}
		TArray<FArmorSettings> Legacy;
		int32 PromotedIndex = 0;
		int32 OtherIndex = 0;
		while (PromotedIndex < Promoted.Num() || OtherIndex < Others.Num())
		{
			const bool bTakePromoted = OtherIndex >= Others.Num()
				|| (PromotedIndex < Promoted.Num()
					&& UArmorCalculation::GetIsLegacyPlateOrderBefore(Promoted[PromotedIndex], Others[OtherIndex]));
			Legacy.Add(bTakePromoted ? Promoted[PromotedIndex++] : Others[OtherIndex++]);
		}
		return Legacy;
	}

	static UMeshComponent* GetMesh(const UArmorCalculation& Armor, const int32 Slot)
	{
		return Armor.GetArmorMeshForSlot(Slot);
	}

	static TArray<FArmorSettings> GetPlates(const UArmorCalculation& Armor, const int32 Slot)
	{
		TArray<FArmorSettings> Plates;
		const FArmorSettings* Settings = Armor.GetArmorSettingsForSlot(Slot);
		const int32 NumPlates = Armor.GetRegisteredPlateCountForSlot(Slot);
		for (int32 Index = 0; Settings != nullptr && Index < NumPlates; ++Index)
		{
			Plates.Add(Settings[Index]);
		}
		return Plates;
	}
};

namespace ArmorBoxScaleTestsPrivate
{
	constexpr EAutomationTestFlags UnitTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	constexpr EAutomationTestFlags AuditFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::StressFilter;
	constexpr uint8 TestPlayer = 1;
	constexpr float Tolerance = 0.01f;
	constexpr float DefaultHalfExtent = 32.f;

	FArmorSettings MakePlate(const EArmorPlate Type, const float Mm, const FVector& Location, const FVector& Scale,
	                         const FVector& HalfExtent = FVector(DefaultHalfExtent))
	{
		FArmorSettings Plate;
		Plate.ArmorType = Type;
		Plate.ArmorValue = Mm;
		Plate.ArmorBoxTransform = FTransform(FQuat::Identity, Location, Scale);
		Plate.ArmorBox = FBox(-HalfExtent, HalfExtent);
		return Plate;
	}

	struct FHitResultPlate
	{
		EArmorPlate Plate = EArmorPlate::Plate_Front;
		float RawArmor = 0.f;
	};

	FHitResultPlate Hit(UArmorCalculation& Armor, UMeshComponent* Mesh, const FVector& Location)
	{
		FHitResultPlate Result;
		float AdjustedPen = 100.f;
		Armor.GetEffectiveArmorOnHit(Mesh, Location, FVector(0.f, -1.f, 0.f), FVector(0.f, 1.f, 0.f),
		                             Result.RawArmor, AdjustedPen, Result.Plate);
		return Result;
	}

	// ---------------------------------------------------------------------------------------------
	// Audit: resolve sample points the way the live C++ does, under three rules.
	// ---------------------------------------------------------------------------------------------
	enum class EResolveMode : uint8
	{
		// Before the fix: hierarchy, then thicker first; boxes cover 32*s^2.
		Legacy,
		// The fix: unit-scale boxes first, then as Legacy; coverage unchanged.
		UnitScaleFirst,
		// Reference only (not implemented): boxes cover 32*s, Legacy order.
		TrueScale
	};

	bool IsInsideForMode(const FArmorSettings& Plate, const FVector& MeshLocalPoint, const EResolveMode Mode)
	{
		if (Mode == EResolveMode::TrueScale)
		{
			const FTransform NoScale(Plate.ArmorBoxTransform.GetRotation(), Plate.ArmorBoxTransform.GetLocation());
			return Plate.ArmorBox.IsInside(NoScale.InverseTransformPosition(MeshLocalPoint));
		}
		return Plate.ArmorBox.IsInside(Plate.ArmorBoxTransform.InverseTransformPosition(MeshLocalPoint));
	}

	/** @return Index into Plates (the order given), using NoArmorHitGetClosest's fallback when no box contains it. */
	int32 Resolve(const TArray<FArmorSettings>& Plates, const FVector& Point, const EResolveMode Mode)
	{
		for (int32 Index = 0; Index < Plates.Num(); ++Index)
		{
			if (IsInsideForMode(Plates[Index], Point, Mode))
			{
				return Index;
			}
		}
		float BestDistanceSquared = TNumericLimits<float>::Max();
		int32 BestIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Plates.Num(); ++Index)
		{
			const FVector Centre = Plates[Index].ArmorBoxTransform.TransformPosition(Plates[Index].ArmorBox.GetCenter());
			const float DistanceSquared = FVector::DistSquared(Point, Centre);
			if (DistanceSquared >= BestDistanceSquared)
			{
				continue;
			}
			BestDistanceSquared = DistanceSquared;
			BestIndex = Index;
			if (DistanceSquared < 25.f * 25.f)
			{
				break;
			}
		}
		return BestIndex;
	}

	FString PlateLabel(const FArmorSettings& Plate)
	{
		const FVector Scale = Plate.ArmorBoxTransform.GetScale3D();
		return FString::Printf(TEXT("%s %.0fmm s(%.2f,%.2f,%.2f)"),
		                       *UEnum::GetValueAsString(Plate.ArmorType).Replace(TEXT("EArmorPlate::"), TEXT("")),
		                       Plate.ArmorValue, Scale.X, Scale.Y, Scale.Z);
	}

	bool SameEffect(const FArmorSettings& A, const FArmorSettings& B)
	{
		return A.ArmorType == B.ArmorType && FMath::IsNearlyEqual(A.ArmorValue, B.ArmorValue, 0.01f);
	}

	struct FModeDiff
	{
		int32 Changed = 0;
		TMap<FString, int32> Transitions;
	};

	FString TopTransitions(const FModeDiff& Diff, const int32 MaxEntries)
	{
		TArray<TPair<FString, int32>> Sorted;
		for (const TPair<FString, int32>& Pair : Diff.Transitions)
		{
			Sorted.Add(Pair);
		}
		Sorted.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) { return A.Value > B.Value; });
		FString Out;
		for (int32 Index = 0; Index < Sorted.Num() && Index < MaxEntries; ++Index)
		{
			Out += FString::Printf(TEXT("\n      %5d  %s"), Sorted[Index].Value, *Sorted[Index].Key);
		}
		return Out;
	}

	struct FProbe
	{
		const TCHAR* ClassNameContains;
		const TCHAR* Label;
		FVector HullLocal;
	};

	// Hetzer Flame fuel-tank centres (UE_STATUS B4 Hetzer) and the Bollwerk armsim points (JagdPantherUE/armsim_out.txt).
	const FProbe Probes[] = {
		{TEXT("Ger_Vehicle_Hetzer_Flame"), TEXT("fuel tank front-left"), FVector(65.f, -122.f, 147.25f)},
		{TEXT("Ger_Vehicle_Hetzer_Flame"), TEXT("fuel tank rear-left"), FVector(-56.f, -115.1f, 147.25f)},
		{TEXT("Ger_Vehicle_Hetzer_Flame"), TEXT("fuel tank front-right"), FVector(65.f, 120.35f, 147.25f)},
		{TEXT("Ger_Vehicle_Hetzer_Flame"), TEXT("fuel tank rear-right"), FVector(-56.f, 113.25f, 147.25f)},
		{TEXT("Ger_Vehicle_JagdPanther_Bollwerk"), TEXT("casemate front face"), FVector(120.f, 0.f, 170.f)},
		{TEXT("Ger_Vehicle_JagdPanther_Bollwerk"), TEXT("ball mantlet"), FVector(150.f, 7.f, 190.f)},
		{TEXT("Ger_Vehicle_JagdPanther_Bollwerk"), TEXT("casemate side R"), FVector(-66.f, 125.f, 200.f)},
		{TEXT("Ger_Vehicle_JagdPanther_Bollwerk"), TEXT("casemate side L"), FVector(-66.f, -125.f, 200.f)},
		{TEXT("Ger_Vehicle_JagdPanther_Bollwerk"), TEXT("casemate rear"), FVector(-225.f, 0.f, 200.f)},
		{TEXT("Ger_Vehicle_JagdPanther_Bollwerk"), TEXT("barbette"), FVector(-28.f, -64.f, 262.f)},
		{TEXT("Ger_Vehicle_JagdPanther_Bollwerk"), TEXT("lower side skirt R"), FVector(-150.f, 150.f, 100.f)},
		{TEXT("Ger_Vehicle_JagdPanther_Bollwerk"), TEXT("upper glacis"), FVector(150.f, 0.f, 110.f)},
		{TEXT("Ger_Vehicle_JagdPanther_Bollwerk"), TEXT("engine deck rear"), FVector(-400.f, 0.f, 190.f)},
	};
}

// -------------------------------------------------------------------------------------------------
// Unit tests
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArmorBoxScaleVariantBoxWinsTest, "RTS.Armor.BoxScale.VariantBoxInsideInheritedBox",
                                 ArmorBoxScaleTestsPrivate::UnitTestFlags)

bool FArmorBoxScaleVariantBoxWinsTest::RunTest(const FString& Parameters)
{
	using namespace ArmorBoxScaleTestsPrivate;
	UArmorCalculation* Armor = NewObject<UArmorCalculation>();
	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Armor);
	// Inherited base box: scale (3,6,3) on the default +-32 box -> live half extents 288 x 1152 x 288 (priority 2).
	const FArmorSettings InheritedFront = MakePlate(EArmorPlate::Plate_Front, 80.f, FVector::ZeroVector,
	                                                FVector(3.f, 6.f, 3.f));
	// Variant box: scale 1 with its true half extents, inside the inherited box (priority 0.5).
	const FArmorSettings VariantSide = MakePlate(EArmorPlate::Plate_SideLeft, 8.f, FVector(0.f, -120.f, 150.f),
	                                             FVector::OneVector, FVector(60.f, 25.f, 24.f));
	Armor->InitArmorCalculation(Mesh, {InheritedFront, VariantSide}, TestPlayer);

	const FHitResultPlate InsideVariant = Hit(*Armor, Mesh, FVector(10.f, -120.f, 150.f));
	TestEqual(TEXT("A hit inside the scale-1 variant box resolves to it"),
	          static_cast<int32>(InsideVariant.Plate), static_cast<int32>(EArmorPlate::Plate_SideLeft));
	TestEqual(TEXT("... with the variant box's armour"), InsideVariant.RawArmor, 8.f, Tolerance);

	const FHitResultPlate OutsideVariant = Hit(*Armor, Mesh, FVector(10.f, 120.f, 150.f));
	TestEqual(TEXT("A hit outside the variant box still resolves to the inherited box"),
	          static_cast<int32>(OutsideVariant.Plate), static_cast<int32>(EArmorPlate::Plate_Front));
	TestEqual(TEXT("... with the inherited box's armour"), OutsideVariant.RawArmor, 80.f, Tolerance);

	// 250 cm is outside the box's drawn 3 x 32 = 96 cm but inside its live 288 cm: legacy coverage is kept.
	const FHitResultPlate LegacyCoverage = Hit(*Armor, Mesh, FVector(250.f, 0.f, 0.f));
	TestEqual(TEXT("The inherited box keeps its live (32*s^2) coverage"),
	          static_cast<int32>(LegacyCoverage.Plate), static_cast<int32>(EArmorPlate::Plate_Front));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArmorBoxScaleLegacyOrderTest, "RTS.Armor.BoxScale.LegacyOrderUnchanged",
                                 ArmorBoxScaleTestsPrivate::UnitTestFlags)

bool FArmorBoxScaleLegacyOrderTest::RunTest(const FString& Parameters)
{
	using namespace ArmorBoxScaleTestsPrivate;
	// All boxes scaled (a base vehicle): the order is the old hierarchy-then-thickness order.
	TArray<FArmorSettings> Scaled = {
		MakePlate(EArmorPlate::Plate_SideLeft, 20.f, FVector(0.f, -90.f, 90.f), FVector(1.f, 7.f, 2.f)),
		MakePlate(EArmorPlate::Turret_Rear, 8.f, FVector(-120.f, 0.f, 120.f), FVector(3.f, 4.f, 1.5f)),
		MakePlate(EArmorPlate::Plate_Front, 60.f, FVector(200.f, 0.f, 90.f), FVector(1.33f, 4.f, 1.5f)),
		MakePlate(EArmorPlate::Turret_Mantlet, 100.f, FVector(130.f, 40.f, 170.f), FVector(4.f, 1.5f, 1.f)),
		MakePlate(EArmorPlate::Plate_SideRight, 25.f, FVector(0.f, 90.f, 90.f), FVector(1.f, 7.f, 2.f)),
	};
	TArray<FArmorSettings> LegacySorted = Scaled;
	TArray<FArmorSettings> FixedSorted = Scaled;
	FArmorCalculationTestAccess::Sort(LegacySorted, false);
	FArmorCalculationTestAccess::Sort(FixedSorted, true);
	for (int32 Index = 0; Index < Scaled.Num(); ++Index)
	{
		TestTrue(FString::Printf(TEXT("All-scaled order unchanged at %d"), Index),
		         SameEffect(LegacySorted[Index], FixedSorted[Index]));
	}
	TestEqual(TEXT("Mantlet first"), static_cast<int32>(FixedSorted[0].ArmorType),
	          static_cast<int32>(EArmorPlate::Turret_Mantlet));
	TestEqual(TEXT("Thicker side first at equal priority"), FixedSorted[3].ArmorValue, 25.f, Tolerance);

	// All boxes unit scale (a re-authored vehicle): also the old order.
	TArray<FArmorSettings> Unit = Scaled;
	for (FArmorSettings& Plate : Unit)
	{
		Plate.ArmorBoxTransform.SetScale3D(FVector::OneVector);
	}
	TArray<FArmorSettings> UnitLegacy = Unit;
	TArray<FArmorSettings> UnitFixed = Unit;
	FArmorCalculationTestAccess::Sort(UnitLegacy, false);
	FArmorCalculationTestAccess::Sort(UnitFixed, true);
	for (int32 Index = 0; Index < Unit.Num(); ++Index)
	{
		TestTrue(FString::Printf(TEXT("All-unit order unchanged at %d"), Index),
		         SameEffect(UnitLegacy[Index], UnitFixed[Index]));
	}

	// Mixed: unit-scale boxes first, each group in hierarchy order.
	TArray<FArmorSettings> Mixed = {
		MakePlate(EArmorPlate::Plate_Front, 80.f, FVector::ZeroVector, FVector(3.f, 6.f, 3.f)),
		MakePlate(EArmorPlate::Turret_Front, 80.f, FVector::ZeroVector, FVector::OneVector),
		MakePlate(EArmorPlate::Plate_SideLeft, 60.f, FVector::ZeroVector, FVector::OneVector),
		MakePlate(EArmorPlate::Turret_Mantlet, 120.f, FVector::ZeroVector, FVector::OneVector),
		MakePlate(EArmorPlate::Plate_SideLowerLeft, 55.f, FVector::ZeroVector, FVector(1.f, 7.f, 2.f)),
		// A zero-armor unit plate (module marker / untouched default plate) stays in the legacy group.
		MakePlate(EArmorPlate::Turret_Cupola, 0.f, FVector::ZeroVector, FVector::OneVector),
	};
	FArmorCalculationTestAccess::Sort(Mixed, true);
	const EArmorPlate Expected[] = {
		EArmorPlate::Turret_Mantlet, EArmorPlate::Turret_Front, EArmorPlate::Plate_SideLeft,
		EArmorPlate::Turret_Cupola, EArmorPlate::Plate_Front, EArmorPlate::Plate_SideLowerLeft
	};
	for (int32 Index = 0; Index < Mixed.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("Mixed order at %d"), Index), static_cast<int32>(Mixed[Index].ArmorType),
		          static_cast<int32>(Expected[Index]));
	}
	TestTrue(TEXT("Scale (1,1,1) counts as unit"),
	         FArmorCalculationTestAccess::IsUnitScale(MakePlate(EArmorPlate::Plate_Front, 1.f, FVector::ZeroVector,
		         FVector::OneVector)));
	TestFalse(TEXT("A zero-armor unit plate is not promoted"),
	          FArmorCalculationTestAccess::IsPromoted(MakePlate(EArmorPlate::Plate_Front, 0.f, FVector::ZeroVector,
		          FVector::OneVector)));
	TestFalse(TEXT("Scale (1,1,1.5) does not"),
	          FArmorCalculationTestAccess::IsUnitScale(MakePlate(EArmorPlate::Plate_Front, 1.f, FVector::ZeroVector,
		          FVector(1.f, 1.f, 1.5f))));
	return true;
}

// -------------------------------------------------------------------------------------------------
// Audit over every tank Blueprint (stress filter; writes Saved/ArmorAudit/ArmorBoxScaleAudit.txt)
// -------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArmorBoxScaleImpactAuditTest, "RTS.ArmorAudit.BoxScaleImpact",
                                 ArmorBoxScaleTestsPrivate::AuditFlags)

bool FArmorBoxScaleImpactAuditTest::RunTest(const FString& Parameters)
{
	using namespace ArmorBoxScaleTestsPrivate;
	constexpr int32 GridSteps = 12;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AssetRegistry.SearchAllAssets(true);
	TArray<FAssetData> Blueprints;
	AssetRegistry.GetAssetsByPath(FName(TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles")), Blueprints, true);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	FString Report = TEXT("Armour box scale audit (B4b). Per registered mesh: plates (unit-scale / scaled), then the share of a ")
		TEXT("12^3 grid over the mesh's local bounds whose resolved plate (type + mm) differs from the live (legacy) result.\n")
		TEXT("UnitFirst = the implemented fix. TrueScale = reference only: boxes at their drawn 32*s size, legacy order.\n\n");
	FString ProbeReport = TEXT("\n\nProbe points (hull-local cm), legacy -> fixed:\n");
	int32 NumTanks = 0;
	int32 NumMeshes = 0;
	int32 NumMixedMeshes = 0;
	int32 NumChangedMeshes = 0;
	TArray<FString> ChangedSummary;

	for (const FAssetData& AssetData : Blueprints)
	{
		// Template Blueprints (e.g. ArmoredCar_ToCopy) are not units and do not compile cleanly.
		if (AssetData.AssetClassPath != UBlueprint::StaticClass()->GetClassPathName()
			|| AssetData.AssetName.ToString().Contains(TEXT("ToCopy")))
		{
			continue;
		}
		const FString NativeParentPath = AssetData.GetTagValueRef<FString>(FBlueprintTags::NativeParentClassPath);
		const UClass* NativeParent = NativeParentPath.IsEmpty()
			                             ? nullptr
			                             : FindObject<UClass>(nullptr, *FPackageName::ExportTextPathToObjectPath(
				                             NativeParentPath));
		if (NativeParent == nullptr || not NativeParent->IsChildOf(ATankMaster::StaticClass()))
		{
			continue;
		}
		const UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
		UClass* TankClass = Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
		if (TankClass == nullptr || TankClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			continue;
		}
		UFunction* SetupArmor = TankClass->FindFunctionByName(TEXT("SetupArmor"));
		if (SetupArmor == nullptr || SetupArmor->ParmsSize != 0)
		{
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Tank = World->SpawnActor(TankClass, &FTransform::Identity, SpawnParameters);
		UArmorCalculation* Armor = Tank ? Tank->FindComponentByClass<UArmorCalculation>() : nullptr;
		if (Armor == nullptr)
		{
			if (Tank)
			{
				Tank->Destroy();
			}
			continue;
		}
		Tank->ProcessEvent(SetupArmor, nullptr);
		++NumTanks;
		const FString TankName = Blueprint->GetName();
		Report += FString::Printf(TEXT("%s (%s)\n"), *TankName, *AssetData.PackageName.ToString());

		for (int32 Slot = 0; Slot < VehicleModuleBalance::MaxRegisteredArmorMeshes; ++Slot)
		{
			UMeshComponent* Mesh = FArmorCalculationTestAccess::GetMesh(*Armor, Slot);
			const TArray<FArmorSettings> Stored = FArmorCalculationTestAccess::GetPlates(*Armor, Slot);
			if (Mesh == nullptr || Stored.IsEmpty())
			{
				continue;
			}
			++NumMeshes;
			int32 NumUnit = 0;
			for (const FArmorSettings& Plate : Stored)
			{
				NumUnit += FArmorCalculationTestAccess::IsPromoted(Plate) ? 1 : 0;
			}
			const bool bMixed = NumUnit > 0 && NumUnit < Stored.Num();
			NumMixedMeshes += bMixed ? 1 : 0;

			// Stored is the live (fixed) order; the legacy order is rebuilt from it.
			const TArray<FArmorSettings> Legacy = FArmorCalculationTestAccess::RebuildLegacyOrder(Stored);
			const TArray<FArmorSettings>& Fixed = Stored;

			const FBox Bounds = Mesh->CalcBounds(FTransform::Identity).GetBox();
			FModeDiff UnitFirstDiff;
			FModeDiff TrueScaleDiff;
			int32 NumSamples = 0;
			for (int32 X = 0; X < GridSteps; ++X)
			{
				for (int32 Y = 0; Y < GridSteps; ++Y)
				{
					for (int32 Z = 0; Z < GridSteps; ++Z)
					{
						const FVector Alpha((X + 0.5f) / GridSteps, (Y + 0.5f) / GridSteps, (Z + 0.5f) / GridSteps);
						const FVector Point = Bounds.Min + Bounds.GetSize() * Alpha;
						++NumSamples;
						const int32 LegacyIndex = Resolve(Legacy, Point, EResolveMode::Legacy);
						const int32 FixedIndex = Resolve(Fixed, Point, EResolveMode::UnitScaleFirst);
						const int32 TrueIndex = Resolve(Legacy, Point, EResolveMode::TrueScale);
						if (LegacyIndex == INDEX_NONE)
						{
							continue;
						}
						if (FixedIndex != INDEX_NONE && not SameEffect(Legacy[LegacyIndex], Fixed[FixedIndex]))
						{
							++UnitFirstDiff.Changed;
							UnitFirstDiff.Transitions.FindOrAdd(
								PlateLabel(Legacy[LegacyIndex]) + TEXT(" -> ") + PlateLabel(Fixed[FixedIndex]))++;
						}
						if (TrueIndex != INDEX_NONE && not SameEffect(Legacy[LegacyIndex], Legacy[TrueIndex]))
						{
							++TrueScaleDiff.Changed;
							TrueScaleDiff.Transitions.FindOrAdd(
								PlateLabel(Legacy[LegacyIndex]) + TEXT(" -> ") + PlateLabel(Legacy[TrueIndex]))++;
						}
					}
				}
			}
			const float UnitFirstPct = 100.f * UnitFirstDiff.Changed / FMath::Max(1, NumSamples);
			const float TrueScalePct = 100.f * TrueScaleDiff.Changed / FMath::Max(1, NumSamples);
			Report += FString::Printf(
				TEXT("  mesh %d %s: %d plates (%d promoted unit-scale / %d other)%s | UnitFirst changes %.1f%% | TrueScale changes %.1f%%\n"),
				Slot, *Mesh->GetName(), Stored.Num(), NumUnit, Stored.Num() - NumUnit, bMixed ? TEXT(" MIXED") : TEXT(""),
				UnitFirstPct, TrueScalePct);
			for (const FArmorSettings& Plate : Legacy)
			{
				const FVector Live = Plate.ArmorBox.GetExtent() * Plate.ArmorBoxTransform.GetScale3D().GetAbs();
				Report += FString::Printf(TEXT("      plate %-48s live half (%.0f, %.0f, %.0f)\n"), *PlateLabel(Plate),
				                          Live.X, Live.Y, Live.Z);
			}
			if (UnitFirstDiff.Changed > 0)
			{
				++NumChangedMeshes;
				Report += TEXT("    UnitFirst transitions:") + TopTransitions(UnitFirstDiff, 6) + TEXT("\n");
				ChangedSummary.Add(FString::Printf(TEXT("%s mesh %s: %.1f%%"), *TankName, *Mesh->GetName(), UnitFirstPct));
			}
			if (TrueScaleDiff.Changed > 0)
			{
				Report += TEXT("    TrueScale transitions:") + TopTransitions(TrueScaleDiff, 4) + TEXT("\n");
			}

			for (const FProbe& Probe : Probes)
			{
				if (not TankName.Contains(Probe.ClassNameContains) || not Mesh->GetName().Contains(TEXT("Hull")))
				{
					continue;
				}
				const int32 LegacyIndex = Resolve(Legacy, Probe.HullLocal, EResolveMode::Legacy);
				const int32 FixedIndex = Resolve(Fixed, Probe.HullLocal, EResolveMode::UnitScaleFirst);
				ProbeReport += FString::Printf(TEXT("  %s %-22s %s -> %s\n"), *TankName, Probe.Label,
				                               LegacyIndex == INDEX_NONE ? TEXT("none") : *PlateLabel(Legacy[LegacyIndex]),
				                               FixedIndex == INDEX_NONE ? TEXT("none") : *PlateLabel(Fixed[FixedIndex]));
			}
		}
		Tank->Destroy();
	}

	const FString Summary = FString::Printf(
		TEXT("Tanks audited: %d, armour meshes: %d, meshes mixing unit-scale and scaled boxes: %d, meshes whose resolution changes with the fix: %d"),
		NumTanks, NumMeshes, NumMixedMeshes, NumChangedMeshes);
	Report = Summary + TEXT("\nChanged meshes:\n  ") + FString::Join(ChangedSummary, TEXT("\n  ")) + TEXT("\n\n") + Report +
		ProbeReport;
	const FString ReportPath = FPaths::ProjectSavedDir() / TEXT("ArmorAudit/ArmorBoxScaleAudit.txt");
	FFileHelper::SaveStringToFile(Report, *ReportPath);
	AddInfo(Summary);
	AddInfo(TEXT("Report: ") + ReportPath);
	TestTrue(TEXT("At least one tank Blueprint was audited"), NumTanks > 0);

	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
