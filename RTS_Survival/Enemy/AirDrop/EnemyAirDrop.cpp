// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyAirDrop.h"

#include "EngineUtils.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementShapesComponent.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/EnemyAirWarnings/EnemyAirWarningSubsystem.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftDrop/GA_AircraftDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/GA_Paratroopers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UObject/ConstructorHelpers.h"

namespace EnemyAirDropConstants
{
	// How often a Start When Destroyed air drop looks at its linked actor.
	constexpr float LinkWatchIntervalSeconds = 0.5f;

	// Editor drawing.
	constexpr float DropColumnTopZ = 1800.f;
	constexpr float DropColumnBottomZ = 150.f;
	constexpr float ColumnThickness = 8.f;
	constexpr float ColumnDashSize = 120.f;
	constexpr float GroundLiftZ = 60.f;
	// Roughly where the abilities spread their units (paratrooper LandingSpreadRadius, cargo RadiusSpawnSquads).
	constexpr float ParachuteLandingRadius = 750.f;
	constexpr float CargoLandingRadius = 600.f;
	constexpr float LandingRingThickness = 10.f;
	constexpr float SecondaryRingBrightness = 0.6f;
	// Cargo plane: a fuselage and wing cross at the top of the column.
	constexpr float FuselageHalfLength = 320.f;
	constexpr float WingHalfSpan = 420.f;
	constexpr float PlaneThickness = 14.f;
	// Parachute: a canopy ring with shroud lines down to the column.
	constexpr float CanopyRadius = 260.f;
	constexpr float CanopyDropZ = 260.f;
	constexpr int32 ShroudLineCount = 4;
	constexpr float ShroudThickness = 3.f;
	// Unit glyphs (detail layer): boxes for vehicles, spheres for squads; filled = every difficulty.
	const FVector VehicleGlyphHalfExtent(140.f, 90.f, 60.f);
	constexpr float SquadGlyphRadius = 80.f;
	constexpr float GlyphLiftZ = 90.f;
	// Linked actor marker.
	constexpr float LinkDashSize = 140.f;
	constexpr float LinkMarkerRadius = 450.f;
	constexpr float LinkMarkerThickness = 12.f;
	constexpr float LinkLiftZ = 120.f;
	constexpr float OriginLinkBrightness = 0.5f;
	constexpr float OriginLinkDashSize = 90.f;
}

namespace
{
	FString GetDropLabel(const AActor& AirDrop, const FName AirDropId)
	{
		return AirDropId.IsNone() ? AirDrop.GetName() : AirDropId.ToString();
	}

	UGlobalAbilitySetDataAsset* GetAbilitySet()
	{
		const URTSGlobalAbilitySettings* Settings = GetDefault<URTSGlobalAbilitySettings>();
		return IsValid(Settings) ? Settings->GlobalAbilityDataAsset.LoadSynchronous() : nullptr;
	}
}

void AEnemyAirDrop::InitRuntimeAirDrop(const FName InAirDropId, const FEnemyAirDropUnits& InUnits,
                                       const FEnemyAirDropDeliverySettings& InDelivery, const float FirstDropDelay)
{
	AirDropId = InAirDropId;
	LandingPoints = {FVector::ZeroVector};
	Units = InUnits;
	Delivery = InDelivery;
	Timing = FEnemyAirDropTiming();
	Timing.bStartOnBeginPlay = true;
	Timing.Repeat = EEnemyWaveSpawnerRepeat::Once;
	Timing.FirstDropDelay = FMath::Max(0.f, FirstDropDelay);
	Link = FEnemyAirDropLink();
}

AEnemyAirDrop::AEnemyAirDrop()
{
	LandingPoints = {FVector::ZeroVector};
#if WITH_EDITORONLY_DATA
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> AirDropSprite;

		FConstructorStatics()
			: AirDropSprite(TEXT("/Engine/EditorResources/S_SkyAtmosphere"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;
	M_AirDropSprite = ConstructorStatics.AirDropSprite.Get();
#endif
}

void AEnemyAirDrop::BeginPlay()
{
	Super::BeginPlay();
	if (Timing.bStartOnBeginPlay)
	{
		StartAirDrop();
	}
}

void AEnemyAirDrop::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_NextDropTimerHandle);
		World->GetTimerManager().ClearTimer(M_LinkWatchTimerHandle);
	}
	bM_IsRunning = false;
	UnbindAllRuns();
	Super::EndPlay(EndPlayReason);
}

// ------------------------------------------------------------------
// Start / stop
// ------------------------------------------------------------------

void AEnemyAirDrop::StartAirDrop()
{
	if (bM_IsRunning)
	{
		return;
	}
	bM_IsRunning = true;
	bM_HasLaunchedLastDrop = false;
	M_DropsThisRun = 0;
	if (Link.Rule == EEnemyAirDropLinkRule::StartWhenDestroyed && GetIsLinkedActorAlive())
	{
		StartWatchingLinkedActor();
		return;
	}
	WaitForEnemyAI(Timing.FirstDropDelay);
}

void AEnemyAirDrop::StopAirDrop()
{
	if (not bM_IsRunning)
	{
		return;
	}
	CancelWaitForEnemyAI();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_NextDropTimerHandle);
		World->GetTimerManager().ClearTimer(M_LinkWatchTimerHandle);
	}
	MarkLastDropLaunched();
}

AEnemyAirDrop* AEnemyAirDrop::FindEnemyAirDrop(const UObject* WorldContextObject, const FName AirDropIdToFind)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (not IsValid(World) || AirDropIdToFind.IsNone())
	{
		RTSFunctionLibrary::ReportError("FindEnemyAirDrop needs a valid world context and an air drop ID.");
		return nullptr;
	}
	for (TActorIterator<AEnemyAirDrop> It(World); It; ++It)
	{
		if (It->AirDropId == AirDropIdToFind)
		{
			return *It;
		}
	}
	RTSFunctionLibrary::ReportError("FindEnemyAirDrop found no air drop with ID: " + AirDropIdToFind.ToString());
	return nullptr;
}

// ------------------------------------------------------------------
// Linked actor
// ------------------------------------------------------------------

bool AEnemyAirDrop::GetIsLinkedActorAlive() const
{
	// A destroyed actor is nulled by garbage collection, so "not set" and "destroyed" look the same here;
	// an actor that was never set is flagged on the editor label instead.
	return RTSFunctionLibrary::RTSIsValid(Link.LinkedActor);
}

void AEnemyAirDrop::StartWatchingLinkedActor()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<AEnemyAirDrop> WeakThis(this);
	World->GetTimerManager().SetTimer(M_LinkWatchTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->CheckLinkedActorDestroyed();
		}
	}), EnemyAirDropConstants::LinkWatchIntervalSeconds, true);
}

void AEnemyAirDrop::CheckLinkedActorDestroyed()
{
	if (not bM_IsRunning || GetIsLinkedActorAlive())
	{
		return;
	}
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_LinkWatchTimerHandle);
	}
	WaitForEnemyAI(Timing.FirstDropDelay);
}

// ------------------------------------------------------------------
// Launch
// ------------------------------------------------------------------

void AEnemyAirDrop::OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty)
{
	M_EnemyController = EnemyController;
	M_Difficulty = Difficulty;
	LaunchDrop();
}

void AEnemyAirDrop::OnEnemyAINeverReady()
{
	MarkLastDropLaunched();
}

void AEnemyAirDrop::LaunchDrop()
{
	if (not bM_IsRunning || bM_HasLaunchedLastDrop)
	{
		return;
	}
	if (Link.Rule == EEnemyAirDropLinkRule::OnlyWhileAlive && not GetIsLinkedActorAlive())
	{
		// The airfield is gone: no new drop comes from it.
		MarkLastDropLaunched();
		return;
	}
	const FString Problem = GetConfigurationProblem();
	if (not Problem.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Air drop " + GetName() + " cannot drop: " + Problem);
		MarkLastDropLaunched();
		return;
	}
	AEnemyController* EnemyController = M_EnemyController.Get();
	UGlobalAbilitiesManager* Manager = IsValid(EnemyController) ? EnemyController->GetGlobalAbilitiesManager() : nullptr;
	TArray<ETankSubtype> Vehicles;
	TArray<ESquadSubtype> Squads;
	TArray<UGlobalAbility*> Abilities;
	EGlobalAbility WarningAbility = EGlobalAbility::GA_None;
	if (not IsValid(Manager) || not TryBuildPayload(Vehicles, Squads)
		|| not TryCreateDropAbilities(Vehicles, Squads, Abilities, WarningAbility))
	{
		RTSFunctionLibrary::ReportError("Air drop " + GetName() + " could not start a drop (no enemy global abilities "
			"manager, no units on this difficulty or no drop ability template); see the error above.");
		MarkLastDropLaunched();
		return;
	}
	M_StartedDropCount++;
	M_DropsThisRun++;
	const int32 DropNumber = M_StartedDropCount;
	const FVector LandingPoint = PickLandingPoint();
	ExecuteDropAbilities(DropNumber, Abilities, *Manager, LandingPoint);
	// After the run is tracked: a mission that stops the air drop from this event still waits for these aircraft.
	OnDropStarted.Broadcast(this, DropNumber);
	WarnPlayer(WarningAbility);
	if (not GetHasMoreDropsToStart())
	{
		MarkLastDropLaunched();
	}
	else if (not Timing.bWaitForPreviousDrop)
	{
		ScheduleNextDrop();
	}
	// An ability that ended inside its execution (nothing to fly) finishes the drop only now.
	FinishRunIfDone(DropNumber);
}

TArray<const TArray<FEnemyAirDropUnitEntry>*> AEnemyAirDrop::GetUnitListsForDifficulty(
	const ERTSGameDifficulty Difficulty) const
{
	FRTSGameDifficulty GameDifficulty;
	GameDifficulty.DifficultyLevel = Difficulty;
	TArray<const TArray<FEnemyAirDropUnitEntry>*> UnitLists = {&Units.BaseUnits};
	if (GameDifficulty.IsNormalOrHigher())
	{
		UnitLists.Add(&Units.ExtraUnitsNormal);
	}
	if (GameDifficulty.IsHardOrHigher())
	{
		UnitLists.Add(&Units.ExtraUnitsHard);
	}
	// Ironman is above Brutal, so it gets the Brutal extras too.
	if (GameDifficulty.IsBrutalOrHigher())
	{
		UnitLists.Add(&Units.ExtraUnitsBrutal);
	}
	return UnitLists;
}

bool AEnemyAirDrop::TryBuildPayload(TArray<ETankSubtype>& OutVehicles, TArray<ESquadSubtype>& OutSquads) const
{
	for (const TArray<FEnemyAirDropUnitEntry>* UnitList : GetUnitListsForDifficulty(M_Difficulty))
	{
		for (const FEnemyAirDropUnitEntry& Entry : *UnitList)
		{
			AppendEntryPayload(Entry, OutVehicles, OutSquads);
		}
	}
	return not OutVehicles.IsEmpty() || not OutSquads.IsEmpty();
}

void AEnemyAirDrop::AppendEntryPayload(const FEnemyAirDropUnitEntry& Entry, TArray<ETankSubtype>& OutVehicles,
                                       TArray<ESquadSubtype>& OutSquads)
{
	if (not EnemyPlacement::GetAreUnitsSet(Entry.UnitOptions))
	{
		return;
	}
	for (int32 UnitIndex = 0; UnitIndex < Entry.Count; ++UnitIndex)
	{
		const FEnemyPlacementUnit& Unit = Entry.UnitOptions[FMath::RandRange(0, Entry.UnitOptions.Num() - 1)];
		if (Unit.Kind == EEnemyPlacementUnitKind::Vehicle)
		{
			OutVehicles.Add(Unit.VehicleSubtype);
			continue;
		}
		OutSquads.Add(Unit.SquadSubtype);
	}
}

FVector AEnemyAirDrop::PickLandingPoint()
{
	const TArray<FVector> WorldLandingPoints = GetWorldPoints(LandingPoints);
	if (WorldLandingPoints.IsEmpty())
	{
		return GetActorLocation();
	}
	int32 PointIndex = FMath::RandRange(0, WorldLandingPoints.Num() - 1);
	if (LandingOrder == EEnemyAirDropLandingOrder::InOrder)
	{
		PointIndex = M_NextLandingPointIndex % WorldLandingPoints.Num();
		M_NextLandingPointIndex++;
	}
	return EnemyPlacement::ProjectToNavigationIfPossible(GetWorld(), WorldLandingPoints[PointIndex]);
}

bool AEnemyAirDrop::TryCreateDropAbilities(const TArray<ETankSubtype>& Vehicles, const TArray<ESquadSubtype>& Squads,
                                           TArray<UGlobalAbility*>& OutAbilities,
                                           EGlobalAbility& OutWarningAbility) const
{
	const bool bSquadsJump = Delivery.Delivery == EEnemyAirDropDelivery::Parachute && not Squads.IsEmpty();
	const TArray<ESquadSubtype> CargoSquads = bSquadsJump ? TArray<ESquadSubtype>() : Squads;
	if (bSquadsJump)
	{
		UGA_Paratroopers* Parachute = CreateParachuteCopy(Squads);
		if (not IsValid(Parachute))
		{
			return false;
		}
		OutAbilities.Add(Parachute);
		OutWarningAbility = Delivery.ParachuteAbility;
	}
	if (Vehicles.IsEmpty() && CargoSquads.IsEmpty())
	{
		return true;
	}
	UGA_AircraftDrop* CargoPlane = CreateCargoPlaneCopy(Vehicles, CargoSquads);
	if (not IsValid(CargoPlane))
	{
		return false;
	}
	OutAbilities.Add(CargoPlane);
	if (OutWarningAbility == EGlobalAbility::GA_None)
	{
		OutWarningAbility = Delivery.CargoPlaneAbility;
	}
	return true;
}

UGA_AircraftDrop* AEnemyAirDrop::CreateCargoPlaneCopy(const TArray<ETankSubtype>& Vehicles,
                                                      const TArray<ESquadSubtype>& Squads) const
{
	UGlobalAbilitySetDataAsset* AbilitySet = GetAbilitySet();
	UGA_AircraftDrop* Copy = IsValid(AbilitySet)
		                         ? Cast<UGA_AircraftDrop>(
			                         AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(Delivery.CargoPlaneAbility))
		                         : nullptr;
	if (not IsValid(Copy))
	{
		RTSFunctionLibrary::ReportError("Air drop " + GetName() + ": cargo plane ability "
			+ UEnum::GetValueAsString(Delivery.CargoPlaneAbility) + " is not an aircraft drop ability in the global "
			"ability data asset.");
		return nullptr;
	}
	Copy->SetDropPayload(Vehicles, Squads);
	Copy->SetRopeSlungTanks(Delivery.bRopeSlungVehicles);
	Copy->SetHandPayloadToEnemyAI(Delivery.bHandUnitsToEnemyAI);
	return Copy;
}

UGA_Paratroopers* AEnemyAirDrop::CreateParachuteCopy(const TArray<ESquadSubtype>& Squads) const
{
	UGlobalAbilitySetDataAsset* AbilitySet = GetAbilitySet();
	UGA_Paratroopers* Copy = IsValid(AbilitySet)
		                         ? Cast<UGA_Paratroopers>(
			                         AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(Delivery.ParachuteAbility))
		                         : nullptr;
	if (not IsValid(Copy))
	{
		RTSFunctionLibrary::ReportError("Air drop " + GetName() + ": parachute ability "
			+ UEnum::GetValueAsString(Delivery.ParachuteAbility) + " is not a paratrooper ability in the global "
			"ability data asset.");
		return nullptr;
	}
	Copy->SetCinematicSquadSubtypes(Squads);
	return Copy;
}

void AEnemyAirDrop::ExecuteDropAbilities(const int32 DropNumber, const TArray<UGlobalAbility*>& Abilities,
                                         UGlobalAbilitiesManager& Manager, const FVector& LandingPoint)
{
	FEnemyAirDropRun& Run = M_Runs.AddDefaulted_GetRef();
	Run.DropNumber = DropNumber;
	// Counted before any executes: an ability with nothing to fly ends inside ExecuteAbilityAtLocation.
	Run.PendingAbilityCount = Abilities.Num();
	for (UGlobalAbility* Ability : Abilities)
	{
		Run.Abilities.Add(Ability);
		Ability->OnGlobalAbilityEnded.AddUObject(this, &AEnemyAirDrop::HandleAbilityEnded);
		if (UGA_AircraftDrop* CargoPlane = Cast<UGA_AircraftDrop>(Ability))
		{
			CargoPlane->OnUnitReleased.AddUObject(this, &AEnemyAirDrop::HandleCargoUnitReleased);
		}
	}
	// The manager is the one of the enemy controller this air drop runs on; the abilities get that controller's player.
	const AEnemyController* EnemyController = M_EnemyController.Get();
	const int32 DropOwningPlayer = IsValid(EnemyController) ? EnemyController->GetControlledPlayer() : GetEnemyPlayer();
	bM_IsLaunching = true;
	for (UGlobalAbility* Ability : Abilities)
	{
		// The same owner, manager and controller the enemy's loaded abilities get (the manager has no player
		// controller), so the aircraft start on the enemy's edge and the units are the enemy AI's.
		Ability->InitGlobalAbility(DropOwningPlayer, &Manager, nullptr);
		Ability->ExecuteAbilityAtLocation(LandingPoint);
	}
	bM_IsLaunching = false;
}

void AEnemyAirDrop::WarnPlayer(const EGlobalAbility WarningAbility) const
{
	if (not Delivery.bWarnPlayer || WarningAbility == EGlobalAbility::GA_None)
	{
		return;
	}
	// The manager warns for the abilities it fires itself; these copies are fired here.
	if (UEnemyAirWarningSubsystem* AirWarnings = UEnemyAirWarningSubsystem::Get(this))
	{
		AirWarnings->OnEnemyAbilityExecuted(WarningAbility);
	}
}

bool AEnemyAirDrop::GetHasMoreDropsToStart() const
{
	switch (Timing.Repeat)
	{
	case EEnemyWaveSpawnerRepeat::Once:
		return false;
	case EEnemyWaveSpawnerRepeat::FixedCount:
		return M_DropsThisRun < FMath::Max(Timing.DropCount, 1);
	default:
		return true;
	}
}

void AEnemyAirDrop::ScheduleNextDrop()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<AEnemyAirDrop> WeakThis(this);
	World->GetTimerManager().SetTimer(M_NextDropTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->LaunchDrop();
		}
	}), FMath::Max(Timing.DropInterval, 1.f), false);
}

void AEnemyAirDrop::MarkLastDropLaunched()
{
	bM_HasLaunchedLastDrop = true;
	TryFinishAirDrop();
}

void AEnemyAirDrop::TryFinishAirDrop()
{
	if (not bM_IsRunning || not bM_HasLaunchedLastDrop || not M_Runs.IsEmpty() || bM_IsLaunching)
	{
		return;
	}
	bM_IsRunning = false;
	OnAirDropFinished.Broadcast(this);
}

// ------------------------------------------------------------------
// Ability callbacks
// ------------------------------------------------------------------

void AEnemyAirDrop::HandleAbilityEnded(UGlobalAbility* Ability)
{
	for (FEnemyAirDropRun& Run : M_Runs)
	{
		if (not Run.Abilities.Contains(Ability))
		{
			continue;
		}
		Ability->OnGlobalAbilityEnded.RemoveAll(this);
		Run.PendingAbilityCount = FMath::Max(Run.PendingAbilityCount - 1, 0);
		FinishRunIfDone(Run.DropNumber);
		return;
	}
}

void AEnemyAirDrop::HandleCargoUnitReleased(UGA_AircraftDrop* Ability, AActor* Unit)
{
	if (IsValid(Unit))
	{
		OnUnitDelivered.Broadcast(this, Unit);
	}
}

void AEnemyAirDrop::FinishRunIfDone(const int32 DropNumber)
{
	if (bM_IsLaunching)
	{
		return;
	}
	const int32 RunIndex = M_Runs.IndexOfByPredicate([DropNumber](const FEnemyAirDropRun& Run)
	{
		return Run.DropNumber == DropNumber;
	});
	if (RunIndex == INDEX_NONE || M_Runs[RunIndex].PendingAbilityCount > 0)
	{
		return;
	}
	M_Runs.RemoveAt(RunIndex);
	M_FinishedDropCount++;
	OnDropFinished.Broadcast(this, DropNumber);
	if (bM_IsRunning && not bM_HasLaunchedLastDrop && Timing.bWaitForPreviousDrop)
	{
		ScheduleNextDrop();
	}
	TryFinishAirDrop();
}

void AEnemyAirDrop::UnbindAllRuns()
{
	for (const FEnemyAirDropRun& Run : M_Runs)
	{
		UnbindRun(Run);
	}
	M_Runs.Empty();
}

void AEnemyAirDrop::UnbindRun(const FEnemyAirDropRun& Run)
{
	for (UGlobalAbility* Ability : Run.Abilities)
	{
		if (IsValid(Ability))
		{
			Ability->OnGlobalAbilityEnded.RemoveAll(this);
		}
		if (UGA_AircraftDrop* CargoPlane = Cast<UGA_AircraftDrop>(Ability))
		{
			CargoPlane->OnUnitReleased.RemoveAll(this);
		}
	}
}

// ------------------------------------------------------------------
// Editor
// ------------------------------------------------------------------

FColor AEnemyAirDrop::GetPlacementColor() const
{
	return EnemyPlacementColors::AirDrop;
}

FString AEnemyAirDrop::GetPlacementKindName() const
{
	return Delivery.Delivery == EEnemyAirDropDelivery::Parachute ? TEXT("PARADROP") : TEXT("AIRLANDING");
}

UTexture2D* AEnemyAirDrop::GetEditorSpriteTexture() const
{
#if WITH_EDITORONLY_DATA
	return M_AirDropSprite.Get();
#else
	return nullptr;
#endif
}

bool AEnemyAirDrop::GetHasAnyUnitEntry() const
{
	return not Units.BaseUnits.IsEmpty() || not Units.ExtraUnitsNormal.IsEmpty() || not Units.ExtraUnitsHard.IsEmpty()
		|| not Units.ExtraUnitsBrutal.IsEmpty();
}

bool AEnemyAirDrop::GetHasUnsetUnitEntry() const
{
	const TArray<FEnemyAirDropUnitEntry>* UnitLists[] = {
		&Units.BaseUnits, &Units.ExtraUnitsNormal, &Units.ExtraUnitsHard, &Units.ExtraUnitsBrutal
	};
	for (const TArray<FEnemyAirDropUnitEntry>* UnitList : UnitLists)
	{
		const bool bHasUnset = UnitList->ContainsByPredicate([](const FEnemyAirDropUnitEntry& Entry)
		{
			return not EnemyPlacement::GetAreUnitsSet(Entry.UnitOptions);
		});
		if (bHasUnset)
		{
			return true;
		}
	}
	return false;
}

FString AEnemyAirDrop::GetAbilityTemplateProblem() const
{
	const UGlobalAbilitySetDataAsset* AbilitySet = GetAbilitySet();
	if (not IsValid(AbilitySet))
	{
		return TEXT("no global ability data asset");
	}
	if (not IsValid(Cast<UGA_AircraftDrop>(AbilitySet->FindAbilityTemplate(Delivery.CargoPlaneAbility))))
	{
		return TEXT("cargo plane ability is not an aircraft drop");
	}
	const bool bUsesParachute = Delivery.Delivery == EEnemyAirDropDelivery::Parachute;
	if (bUsesParachute && not IsValid(Cast<UGA_Paratroopers>(AbilitySet->FindAbilityTemplate(Delivery.ParachuteAbility))))
	{
		return TEXT("parachute ability is not a paratrooper drop");
	}
	return FString();
}

FString AEnemyAirDrop::GetConfigurationProblem() const
{
	if (LandingPoints.IsEmpty())
	{
		return TEXT("no landing points");
	}
	if (not GetHasAnyUnitEntry())
	{
		return TEXT("no units");
	}
	if (GetHasUnsetUnitEntry())
	{
		return TEXT("unit entry with no unit picked");
	}
	if (Link.Rule != EEnemyAirDropLinkRule::None && Link.LinkedActor == nullptr && not HasActorBegunPlay())
	{
		// In game a destroyed linked actor is nulled too; that is the rule at work, not a setup problem.
		return TEXT("no linked actor");
	}
	return GetAbilityTemplateProblem();
}

void AEnemyAirDrop::BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const
{
	using namespace EnemyAirDropConstants;
	const TArray<FVector> WorldLandingPoints = GetWorldPoints(LandingPoints);
	const FColor OriginLinkColor = EnemyPlacementColors::GetDimmed(GetPlacementColor(), OriginLinkBrightness);
	for (int32 PointIndex = 0; PointIndex < WorldLandingPoints.Num(); ++PointIndex)
	{
		AddLandingPointShapes(WorldLandingPoints[PointIndex], PointIndex == 0, OutSummary, OutDetail);
		OutDetail.DashedLines.Emplace(GetActorLocation(), WorldLandingPoints[PointIndex], OriginLinkColor,
		                              OriginLinkDashSize);
	}
	if (not WorldLandingPoints.IsEmpty())
	{
		AddUnitGlyphShapes(WorldLandingPoints[0], OutDetail);
	}
	AddLinkShapes(OutSummary);
}

void AEnemyAirDrop::AddLandingPointShapes(const FVector& LandingPoint, const bool bIsFirst,
                                          FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const
{
	using namespace EnemyAirDropConstants;
	const FColor Color = GetPlacementColor();
	const bool bIsParachute = Delivery.Delivery == EEnemyAirDropDelivery::Parachute;
	const FVector Top = LandingPoint + FVector(0.f, 0.f, DropColumnTopZ);
	const FVector Bottom = LandingPoint + FVector(0.f, 0.f, DropColumnBottomZ);
	// A column from the sky with an arrow down: nothing else of the placed enemy actors points down.
	OutSummary.ArrowLines.Emplace(Top, Bottom, Color);
	const float LandingRadius = bIsParachute ? ParachuteLandingRadius : CargoLandingRadius;
	const FColor RingColor = bIsFirst ? Color : EnemyPlacementColors::GetDimmed(Color, SecondaryRingBrightness);
	OutSummary.AddCircle(LandingPoint + FVector(0.f, 0.f, GroundLiftZ), LandingRadius, RingColor, LandingRingThickness);
	if (not bIsParachute)
	{
		OutSummary.Lines.Emplace(Top, Bottom, Color, ColumnThickness);
		OutSummary.Lines.Emplace(Top - FVector(FuselageHalfLength, 0.f, 0.f), Top + FVector(FuselageHalfLength, 0.f, 0.f),
		                         Color, PlaneThickness);
		OutSummary.Lines.Emplace(Top - FVector(0.f, WingHalfSpan, 0.f), Top + FVector(0.f, WingHalfSpan, 0.f), Color,
		                         PlaneThickness);
		return;
	}
	OutSummary.DashedLines.Emplace(Top, Bottom, Color, ColumnDashSize);
	const FVector CanopyCentre = Top;
	OutSummary.AddCircle(CanopyCentre, CanopyRadius, Color, ColumnThickness);
	const FVector Harness = Top - FVector(0.f, 0.f, CanopyDropZ);
	for (int32 LineIndex = 0; LineIndex < ShroudLineCount; ++LineIndex)
	{
		const float Yaw = 360.f / ShroudLineCount * LineIndex;
		const FVector Edge = CanopyCentre + FRotator(0.f, Yaw, 0.f).Vector() * CanopyRadius;
		OutDetail.Lines.Emplace(Edge, Harness, Color, ShroudThickness);
	}
}

void AEnemyAirDrop::AddUnitGlyphShapes(const FVector& LandingPoint, FEnemyPlacementShapes& OutDetail) const
{
	const TArray<const TArray<FEnemyAirDropUnitEntry>*> UnitLists = {
		&Units.BaseUnits, &Units.ExtraUnitsNormal, &Units.ExtraUnitsHard, &Units.ExtraUnitsBrutal
	};
	int32 UseIndex = 0;
	for (int32 ListIndex = 0; ListIndex < UnitLists.Num(); ++ListIndex)
	{
		// Filled = dropped on every difficulty, outline = only on harder ones (the strongpoint's convention).
		const bool bEveryDifficulty = ListIndex == 0;
		for (const FEnemyAirDropUnitEntry& Entry : *UnitLists[ListIndex])
		{
			AddEntryGlyphShapes(Entry, LandingPoint, bEveryDifficulty, UseIndex, OutDetail);
		}
	}
}

void AEnemyAirDrop::AddEntryGlyphShapes(const FEnemyAirDropUnitEntry& Entry, const FVector& LandingPoint,
                                        const bool bEveryDifficulty, int32& InOutUseIndex,
                                        FEnemyPlacementShapes& OutDetail) const
{
	using namespace EnemyAirDropConstants;
	const FColor Color = GetPlacementColor();
	const bool bIsVehicle = not Entry.UnitOptions.IsEmpty()
		&& Entry.UnitOptions[0].Kind == EEnemyPlacementUnitKind::Vehicle;
	for (int32 UnitIndex = 0; UnitIndex < Entry.Count; ++UnitIndex)
	{
		const FVector GlyphLocation = LandingPoint + EnemyPlacement::GetRingSpreadOffset(InOutUseIndex++)
			+ FVector(0.f, 0.f, GlyphLiftZ);
		if (bIsVehicle)
		{
			OutDetail.AddFlatBox(GlyphLocation, VehicleGlyphHalfExtent, 0.f, Color, bEveryDifficulty);
			continue;
		}
		OutDetail.Spheres.Emplace(SquadGlyphRadius, GlyphLocation, FLinearColor(Color),
		                          bEveryDifficulty
			                          ? FDebugRenderSceneProxy::EDrawType::SolidAndWireMeshes
			                          : FDebugRenderSceneProxy::EDrawType::WireMesh);
	}
}

void AEnemyAirDrop::AddLinkShapes(FEnemyPlacementShapes& OutSummary) const
{
	using namespace EnemyAirDropConstants;
	const AActor* LinkedActor = Link.LinkedActor.Get();
	if (Link.Rule == EEnemyAirDropLinkRule::None || not IsValid(LinkedActor))
	{
		return;
	}
	const FColor Color = GetPlacementColor();
	const FVector Lift(0.f, 0.f, LinkLiftZ);
	const FVector Target = LinkedActor->GetActorLocation() + Lift;
	OutSummary.DashedLines.Emplace(GetActorLocation() + Lift, Target, Color, LinkDashSize);
	if (Link.Rule == EEnemyAirDropLinkRule::OnlyWhileAlive)
	{
		// A ring: the drops live as long as this actor does.
		OutSummary.AddCircle(Target, LinkMarkerRadius, Color, LinkMarkerThickness);
		return;
	}
	// A cross: the drops start once this actor is destroyed.
	const float Arm = LinkMarkerRadius * UE_INV_SQRT_2;
	OutSummary.Lines.Emplace(Target + FVector(-Arm, -Arm, 0.f), Target + FVector(Arm, Arm, 0.f), Color,
	                         LinkMarkerThickness);
	OutSummary.Lines.Emplace(Target + FVector(-Arm, Arm, 0.f), Target + FVector(Arm, -Arm, 0.f), Color,
	                         LinkMarkerThickness);
}
