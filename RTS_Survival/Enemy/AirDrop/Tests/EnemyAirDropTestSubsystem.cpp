// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyAirDropTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "NavigationSystem.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Enemy/AirDrop/EnemyAirDrop.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftDrop/GA_AircraftDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/GA_Paratroopers.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSEnemyAirDropTest, Log, All);

namespace EnemyAirDropTestPrivate
{
	const TCHAR* const Switch = TEXT("EnemyAirDropValidate");
	constexpr double StartDelaySeconds = 8.0;
	constexpr double CheckLinksAfterSeconds = 4.0;
	// After the last drop finished: paratroopers that landed last still load their squads.
	constexpr double SettleAfterDropsSeconds = 8.0;
	constexpr double GiveUpSeconds = 360.0;
	constexpr float GroundTraceHalfHeight = 100000.f;
	const FVector NavExtent(2000.f, 2000.f, 5000.f);
	// Units the enemy AI trains appear at its base; dropped units first stand within this of their landing point.
	constexpr float LandingZoneRadius = 3500.f;

	const FVector ParaOffset(0.f, -2500.f, 0.f);
	const FVector CargoOffset(0.f, 2500.f, 0.f);
	const FVector WhileAliveOffset(-3000.f, 0.f, 0.f);
	const FVector WhenDestroyedOffset(3000.f, 0.f, 0.f);
	const FVector TriggerActorOffset(3000.f, 1500.f, 0.f);

	constexpr int32 ParaDropCount = 2;
	constexpr float ParaDropIntervalSeconds = 2.f;
	constexpr float FirstDropDelaySeconds = 1.f;
	constexpr int32 EnemyPlayer = 2;

	// Each air drop uses its own unit types, so delivered units tell which drop brought them.
	constexpr ESquadSubtype ParaSquad = ESquadSubtype::Squad_Rus_Mosin;
	constexpr ETankSubtype ParaVehicle = ETankSubtype::Tank_SU_85;
	// Not Squad_Rus_LargePTRS: its blueprint reports the Okhotnik subtype (seen 2026-11-15).
	constexpr ESquadSubtype CargoSquad = ESquadSubtype::Squad_Rus_HazmatEngineers;
	constexpr ETankSubtype CargoHardVehicle = ETankSubtype::Tank_T34_76;
	constexpr ESquadSubtype LinkedSquad = ESquadSubtype::Squad_Rus_Okhotnik;

	FEnemyAirDropUnitEntry MakeEntry(const FEnemyPlacementUnit& Unit)
	{
		FEnemyAirDropUnitEntry Entry;
		Entry.UnitOptions = {Unit};
		Entry.Count = 1;
		return Entry;
	}
}

bool UEnemyAirDropTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if RTS_WITH_MAP_TESTS
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), EnemyAirDropTestPrivate::Switch);
#else
	return false;
#endif
}

TStatId UEnemyAirDropTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEnemyAirDropTestSubsystem, STATGROUP_Tickables);
}

void UEnemyAirDropTestSubsystem::Deinitialize()
{
	if (UWorld* const World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(M_ActorSpawnedHandle);
	}
	Super::Deinitialize();
}

void UEnemyAirDropTestSubsystem::Tick(const float DeltaTime)
{
#if RTS_WITH_MAP_TESTS
	const UWorld* const World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay() || M_Step == EStep::Finished)
	{
		return;
	}
	ACPPController* const Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (IsValid(Controller) && Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		return;
	}
	if (M_TestStartSeconds < 0.0)
	{
		M_TestStartSeconds = World->GetTimeSeconds() + EnemyAirDropTestPrivate::StartDelaySeconds;
		return;
	}
	if (M_Step == EStep::WaitingForStart)
	{
		if (World->GetTimeSeconds() >= M_TestStartSeconds)
		{
			StartTest();
		}
		return;
	}
	if (GetTestSeconds() > EnemyAirDropTestPrivate::GiveUpSeconds)
	{
		Check(false, TEXT("finished_in_time"), FString::Printf(TEXT("stuck at step %d"), static_cast<int32>(M_Step)));
		Evaluate();
		return;
	}
	TrackParaRuns();
	TrackDeliveredUnits();
	TickSteps();
#endif
}

double UEnemyAirDropTestSubsystem::GetTestSeconds() const
{
	return GetWorld()->GetTimeSeconds() - M_TestStartSeconds;
}

#if RTS_WITH_MAP_TESTS

void UEnemyAirDropTestSubsystem::TickSteps()
{
	using namespace EnemyAirDropTestPrivate;
	switch (M_Step)
	{
	case EStep::DestroyTriggerActor:
		if (GetTestSeconds() >= M_LastStepSeconds + CheckLinksAfterSeconds)
		{
			CheckLinkedDropsBeforeTrigger();
			if (IsValid(M_TriggerActor))
			{
				M_TriggerActor->Destroy();
			}
			AdvanceStep(EStep::WaitForDrops);
		}
		break;
	case EStep::WaitForDrops:
		if (M_ParaFinishedBroadcasts > 0 && M_CargoFinishedBroadcasts > 0 && M_WhenDestroyedStartedBroadcasts > 0
			&& IsValid(M_WhenDestroyedDrop) && not M_WhenDestroyedDrop->GetIsAirDropRunning())
		{
			AdvanceStep(EStep::CheckUnits);
		}
		break;
	case EStep::CheckUnits:
		if (GetTestSeconds() >= M_LastStepSeconds + SettleAfterDropsSeconds)
		{
			CheckWhenDestroyedStarted();
			CheckParaDrops();
			CheckDeliveredUnits();
			CheckNothingVisibleInGame(M_ParaDrop, TEXT("para_drop"));
			CheckNothingVisibleInGame(M_CargoDrop, TEXT("cargo_drop"));
			Evaluate();
		}
		break;
	default:
		break;
	}
}

void UEnemyAirDropTestSubsystem::AdvanceStep(const EStep NextStep)
{
	M_Step = NextStep;
	M_LastStepSeconds = GetTestSeconds();
}

void UEnemyAirDropTestSubsystem::StartTest()
{
	AEnemyController* const EnemyController = FRTS_Statics::GetEnemyController(this);
	const bool bHasCenter = FindNavigableCenter(M_Center);
	const bool bHasDifficulty = EnemyPlacement::TryGetGameDifficulty(this, M_Difficulty);
	const bool bHasManager = IsValid(EnemyController) && IsValid(EnemyController->GetGlobalAbilitiesManager());
	Check(IsValid(EnemyController) && bHasCenter && bHasDifficulty && bHasManager, TEXT("setup"),
	      FString::Printf(TEXT("enemy_controller=%d nav_center=%d difficulty=%d ability_manager=%d"),
	                      IsValid(EnemyController), bHasCenter, bHasDifficulty, bHasManager));
	if (not bHasManager || not bHasCenter)
	{
		Evaluate();
		return;
	}
	UE_LOG(LogRTSEnemyAirDropTest, Display, TEXT("RTS_AIRDROP_TEST difficulty %s"),
	       *UEnum::GetValueAsString(M_Difficulty));
	M_EnemyController = EnemyController;
	M_ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &UEnemyAirDropTestSubsystem::OnActorSpawned));
	CheckTemplates();
	SpawnParaDrop();
	SpawnCargoDrop();
	SpawnLinkedDrops();
	CheckConfiguredDropHasNoProblem();
	AdvanceStep(EStep::DestroyTriggerActor);
}

void UEnemyAirDropTestSubsystem::OnActorSpawned(AActor* SpawnedActor)
{
	if (IsValid(Cast<ASquadController>(SpawnedActor)) || IsValid(Cast<ATankMaster>(SpawnedActor)))
	{
		M_SpawnedUnits.Add(SpawnedActor);
	}
}

void UEnemyAirDropTestSubsystem::TrackParaRuns()
{
	if (not IsValid(M_ParaDrop))
	{
		return;
	}
	for (const FEnemyAirDropRun& Run : M_ParaDrop->M_Runs)
	{
		int32& Seen = M_ParaAbilitiesPerDrop.FindOrAdd(Run.DropNumber);
		Seen = FMath::Max(Seen, Run.Abilities.Num());
	}
}

void UEnemyAirDropTestSubsystem::TrackDeliveredUnits()
{
	const AEnemyController* const EnemyController = M_EnemyController.Get();
	const UEnemyDirectControlComponent* const DirectControl = IsValid(EnemyController)
		                                                          ? EnemyController->GetEnemyDirectControlComponent()
		                                                          : nullptr;
	const TArray<AActor*> Registered = IsValid(DirectControl)
		                                   ? DirectControl->GetRegisteredDirectControlUnits()
		                                   : TArray<AActor*>();
	for (const TWeakObjectPtr<AActor>& WeakUnit : M_SpawnedUnits)
	{
		AActor* const Unit = WeakUnit.Get();
		const URTSComponent* const RTSComponent = IsValid(Unit) ? Unit->FindComponentByClass<URTSComponent>() : nullptr;
		if (not IsValid(RTSComponent) || RTSComponent->GetUnitSubType() == 0
			|| RTSComponent->GetOwningPlayer() != EnemyAirDropTestPrivate::EnemyPlayer)
		{
			continue;
		}
		FSeenUnit* Seen = M_SeenUnits.Find(FObjectKey(Unit));
		if (Seen == nullptr)
		{
			// A vehicle still hangs under its aircraft far away; it is taken once it stands near its landing point.
			if (Unit->GetAttachParentActor() != nullptr || not GetIsNearLandingPoint(Unit->GetActorLocation()))
			{
				continue;
			}
			Seen = &M_SeenUnits.Add(FObjectKey(Unit));
			Seen->bVehicle = IsValid(Cast<ATankMaster>(Unit));
			Seen->Subtype = RTSComponent->GetUnitSubType();
			UE_LOG(LogRTSEnemyAirDropTest, Display, TEXT("RTS_AIRDROP_TEST seen %s subtype %d at %s"),
			       *Unit->GetClass()->GetName(), Seen->Subtype, *Unit->GetActorLocation().ToCompactString());
		}
		Seen->bWasRegistered = Seen->bWasRegistered || Registered.Contains(Unit);
	}
}

bool UEnemyAirDropTestSubsystem::GetIsNearLandingPoint(const FVector& Location) const
{
	for (const FVector& LandingPoint : M_LandingPoints)
	{
		if (FVector::Dist2D(LandingPoint, Location) <= EnemyAirDropTestPrivate::LandingZoneRadius)
		{
			return true;
		}
	}
	return false;
}

// ------------------------------------------------------------------
// Placing the air drops
// ------------------------------------------------------------------

AEnemyAirDrop* UEnemyAirDropTestSubsystem::SpawnAirDrop(const FVector& Offset, const FName AirDropId)
{
	const FVector Location = EnemyPlacement::ProjectToNavigationIfPossible(GetWorld(), M_Center + Offset);
	// Each test air drop has the one landing point on its own location.
	M_LandingPoints.Add(Location);
	AEnemyAirDrop* const AirDrop = GetWorld()->SpawnActorDeferred<AEnemyAirDrop>(
		AEnemyAirDrop::StaticClass(), FTransform(Location));
	if (not IsValid(AirDrop))
	{
		return nullptr;
	}
	AirDrop->AirDropId = AirDropId;
	AirDrop->Timing.FirstDropDelay = EnemyAirDropTestPrivate::FirstDropDelaySeconds;
	return AirDrop;
}

void UEnemyAirDropTestSubsystem::SpawnParaDrop()
{
	using namespace EnemyAirDropTestPrivate;
	M_ParaDrop = SpawnAirDrop(ParaOffset, TEXT("Test_Para"));
	if (not IsValid(M_ParaDrop))
	{
		return;
	}
	// Squads and a vehicle in one parachute drop: the squads jump, the vehicle comes in a landing cargo plane.
	M_ParaDrop->Units.BaseUnits = {MakeEntry(FEnemyPlacementUnit::MakeSquad(ParaSquad)),
		MakeEntry(FEnemyPlacementUnit::MakeVehicle(ParaVehicle))};
	M_ParaDrop->Delivery.Delivery = EEnemyAirDropDelivery::Parachute;
	M_ParaDrop->Timing.Repeat = EEnemyWaveSpawnerRepeat::FixedCount;
	M_ParaDrop->Timing.DropCount = ParaDropCount;
	M_ParaDrop->Timing.bWaitForPreviousDrop = true;
	M_ParaDrop->Timing.DropInterval = ParaDropIntervalSeconds;
	M_ParaDrop->OnDropStarted.AddDynamic(this, &UEnemyAirDropTestSubsystem::OnParaDropStarted);
	M_ParaDrop->OnDropFinished.AddDynamic(this, &UEnemyAirDropTestSubsystem::OnParaDropFinished);
	M_ParaDrop->OnAirDropFinished.AddDynamic(this, &UEnemyAirDropTestSubsystem::OnParaAirDropFinished);
	M_ParaDrop->FinishSpawning(M_ParaDrop->GetActorTransform());
}

void UEnemyAirDropTestSubsystem::SpawnCargoDrop()
{
	using namespace EnemyAirDropTestPrivate;
	M_CargoDrop = SpawnAirDrop(CargoOffset, TEXT("Test_Cargo"));
	if (not IsValid(M_CargoDrop))
	{
		return;
	}
	M_CargoDrop->Units.BaseUnits = {MakeEntry(FEnemyPlacementUnit::MakeSquad(CargoSquad))};
	M_CargoDrop->Units.ExtraUnitsHard = {MakeEntry(FEnemyPlacementUnit::MakeVehicle(CargoHardVehicle))};
	M_CargoDrop->Delivery.Delivery = EEnemyAirDropDelivery::CargoPlane;
	M_CargoDrop->OnAirDropFinished.AddDynamic(this, &UEnemyAirDropTestSubsystem::OnCargoAirDropFinished);
	M_CargoDrop->FinishSpawning(M_CargoDrop->GetActorTransform());
}

void UEnemyAirDropTestSubsystem::SpawnLinkedDrops()
{
	using namespace EnemyAirDropTestPrivate;
	UWorld* const World = GetWorld();
	AActor* const DeadActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(M_Center));
	M_TriggerActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(M_Center + TriggerActorOffset));

	M_WhileAliveDrop = SpawnAirDrop(WhileAliveOffset, TEXT("Test_WhileAlive"));
	if (IsValid(M_WhileAliveDrop))
	{
		M_WhileAliveDrop->Units.BaseUnits = {MakeEntry(FEnemyPlacementUnit::MakeSquad(LinkedSquad))};
		M_WhileAliveDrop->Link.Rule = EEnemyAirDropLinkRule::OnlyWhileAlive;
		M_WhileAliveDrop->Link.LinkedActor = DeadActor;
		M_WhileAliveDrop->OnAirDropFinished.AddDynamic(this, &UEnemyAirDropTestSubsystem::OnWhileAliveAirDropFinished);
		// The airfield falls before the first drop: nothing may come.
		if (IsValid(DeadActor))
		{
			DeadActor->Destroy();
		}
		M_WhileAliveDrop->FinishSpawning(M_WhileAliveDrop->GetActorTransform());
	}

	M_WhenDestroyedDrop = SpawnAirDrop(WhenDestroyedOffset, TEXT("Test_WhenDestroyed"));
	if (IsValid(M_WhenDestroyedDrop))
	{
		M_WhenDestroyedDrop->Units.BaseUnits = {MakeEntry(FEnemyPlacementUnit::MakeSquad(LinkedSquad))};
		M_WhenDestroyedDrop->Delivery.Delivery = EEnemyAirDropDelivery::Parachute;
		M_WhenDestroyedDrop->Link.Rule = EEnemyAirDropLinkRule::StartWhenDestroyed;
		M_WhenDestroyedDrop->Link.LinkedActor = M_TriggerActor;
		M_WhenDestroyedDrop->OnDropStarted.AddDynamic(this, &UEnemyAirDropTestSubsystem::OnWhenDestroyedDropStarted);
		M_WhenDestroyedDrop->FinishSpawning(M_WhenDestroyedDrop->GetActorTransform());
	}
}

// ------------------------------------------------------------------
// Events
// ------------------------------------------------------------------

void UEnemyAirDropTestSubsystem::OnParaDropStarted(AEnemyAirDrop* AirDrop, const int32 DropNumber)
{
	M_ParaDropStartSeconds.Add(GetTestSeconds());
}

void UEnemyAirDropTestSubsystem::OnParaDropFinished(AEnemyAirDrop* AirDrop, const int32 DropNumber)
{
	M_ParaDropFinishSeconds.Add(GetTestSeconds());
}

void UEnemyAirDropTestSubsystem::OnParaAirDropFinished(AEnemyAirDrop* AirDrop)
{
	M_ParaFinishedBroadcasts++;
}

void UEnemyAirDropTestSubsystem::OnCargoAirDropFinished(AEnemyAirDrop* AirDrop)
{
	M_CargoFinishedBroadcasts++;
}

void UEnemyAirDropTestSubsystem::OnWhileAliveAirDropFinished(AEnemyAirDrop* AirDrop)
{
	M_WhileAliveFinishedBroadcasts++;
}

void UEnemyAirDropTestSubsystem::OnWhenDestroyedDropStarted(AEnemyAirDrop* AirDrop, const int32 DropNumber)
{
	M_WhenDestroyedStartedBroadcasts++;
}

// ------------------------------------------------------------------
// Checks
// ------------------------------------------------------------------

void UEnemyAirDropTestSubsystem::CheckTemplates()
{
	const URTSGlobalAbilitySettings* const Settings = GetDefault<URTSGlobalAbilitySettings>();
	const UGlobalAbilitySetDataAsset* const AbilitySet = IsValid(Settings)
		                                                     ? Settings->GlobalAbilityDataAsset.LoadSynchronous()
		                                                     : nullptr;
	const FEnemyAirDropDeliverySettings Defaults;
	const UGlobalAbility* const Cargo = IsValid(AbilitySet)
		                                    ? AbilitySet->FindAbilityTemplate(Defaults.CargoPlaneAbility)
		                                    : nullptr;
	const UGlobalAbility* const Parachute = IsValid(AbilitySet)
		                                        ? AbilitySet->FindAbilityTemplate(Defaults.ParachuteAbility)
		                                        : nullptr;
	Check(IsValid(Cast<UGA_AircraftDrop>(Cargo)) && IsValid(Cast<UGA_Paratroopers>(Parachute)),
	      TEXT("default_templates_have_the_right_classes"),
	      FString::Printf(TEXT("cargo=%s parachute=%s"), IsValid(Cargo) ? *Cargo->GetClass()->GetName() : TEXT("none"),
	                      IsValid(Parachute) ? *Parachute->GetClass()->GetName() : TEXT("none")));
}

void UEnemyAirDropTestSubsystem::CheckConfiguredDropHasNoProblem()
{
	const FString ParaProblem = IsValid(M_ParaDrop) ? M_ParaDrop->GetConfigurationProblem() : TEXT("not spawned");
	Check(ParaProblem.IsEmpty(), TEXT("configured_drop_has_no_problem"), ParaProblem);
}

void UEnemyAirDropTestSubsystem::CheckLinkedDropsBeforeTrigger()
{
	const bool bWhileAliveOk = IsValid(M_WhileAliveDrop) && M_WhileAliveDrop->GetStartedDropCount() == 0
		&& not M_WhileAliveDrop->GetIsAirDropRunning() && M_WhileAliveFinishedBroadcasts == 1;
	Check(bWhileAliveOk, TEXT("only_while_alive_dead_actor_drops_nothing"),
	      FString::Printf(TEXT("started=%d running=%d finished=%d"),
	                      IsValid(M_WhileAliveDrop) ? M_WhileAliveDrop->GetStartedDropCount() : -1,
	                      IsValid(M_WhileAliveDrop) && M_WhileAliveDrop->GetIsAirDropRunning(),
	                      M_WhileAliveFinishedBroadcasts));
	const bool bWaiting = IsValid(M_WhenDestroyedDrop) && M_WhenDestroyedDrop->GetStartedDropCount() == 0
		&& M_WhenDestroyedDrop->GetIsAirDropRunning();
	Check(bWaiting, TEXT("start_when_destroyed_waits_while_alive"),
	      FString::Printf(TEXT("started=%d running=%d"),
	                      IsValid(M_WhenDestroyedDrop) ? M_WhenDestroyedDrop->GetStartedDropCount() : -1,
	                      IsValid(M_WhenDestroyedDrop) && M_WhenDestroyedDrop->GetIsAirDropRunning()));
}

void UEnemyAirDropTestSubsystem::CheckWhenDestroyedStarted()
{
	Check(M_WhenDestroyedStartedBroadcasts == 1, TEXT("start_when_destroyed_drops_once_after_destruction"),
	      FString::Printf(TEXT("started=%d"), M_WhenDestroyedStartedBroadcasts));
}

void UEnemyAirDropTestSubsystem::CheckParaDrops()
{
	using namespace EnemyAirDropTestPrivate;
	const int32 Started = IsValid(M_ParaDrop) ? M_ParaDrop->GetStartedDropCount() : -1;
	const int32 Finished = IsValid(M_ParaDrop) ? M_ParaDrop->GetFinishedDropCount() : -1;
	Check(Started == ParaDropCount && Finished == ParaDropCount && M_ParaFinishedBroadcasts == 1,
	      TEXT("fixed_count_drops_then_finishes_once"),
	      FString::Printf(TEXT("started=%d finished=%d air_drop_finished=%d"), Started, Finished,
	                      M_ParaFinishedBroadcasts));
	const int32* const FirstAbilities = M_ParaAbilitiesPerDrop.Find(1);
	const int32* const SecondAbilities = M_ParaAbilitiesPerDrop.Find(2);
	Check(FirstAbilities && SecondAbilities && *FirstAbilities == 2 && *SecondAbilities == 2,
	      TEXT("mixed_parachute_drop_flies_paratroopers_and_cargo_plane"),
	      FString::Printf(TEXT("abilities drop1=%d drop2=%d"), FirstAbilities ? *FirstAbilities : -1,
	                      SecondAbilities ? *SecondAbilities : -1));
	const bool bWaited = M_ParaDropStartSeconds.Num() == ParaDropCount && not M_ParaDropFinishSeconds.IsEmpty()
		&& M_ParaDropStartSeconds[1] >= M_ParaDropFinishSeconds[0];
	Check(bWaited, TEXT("second_drop_waits_for_first"),
	      FString::Printf(TEXT("starts=%d finishes=%d second_start=%.1f first_finish=%.1f"),
	                      M_ParaDropStartSeconds.Num(), M_ParaDropFinishSeconds.Num(),
	                      M_ParaDropStartSeconds.Num() > 1 ? M_ParaDropStartSeconds[1] : -1.0,
	                      M_ParaDropFinishSeconds.IsEmpty() ? -1.0 : M_ParaDropFinishSeconds[0]));
}

int32 UEnemyAirDropTestSubsystem::CountDeliveredUnits(const bool bVehicle, const uint8 Subtype,
                                                      int32& OutRegistered) const
{
	int32 Count = 0;
	OutRegistered = 0;
	for (const TPair<FObjectKey, FSeenUnit>& Pair : M_SeenUnits)
	{
		if (Pair.Value.bVehicle != bVehicle || Pair.Value.Subtype != Subtype)
		{
			continue;
		}
		Count++;
		OutRegistered += Pair.Value.bWasRegistered ? 1 : 0;
	}
	return Count;
}

void UEnemyAirDropTestSubsystem::CheckDeliveredUnits()
{
	using namespace EnemyAirDropTestPrivate;
	FRTSGameDifficulty GameDifficulty;
	GameDifficulty.DifficultyLevel = M_Difficulty;
	const int32 ExpectedCargoVehicles = GameDifficulty.IsHardOrHigher() ? 1 : 0;
	struct FExpected
	{
		const TCHAR* Name;
		bool bVehicle;
		uint8 Subtype;
		int32 Count;
	};
	const FExpected ExpectedUnits[] = {
		{TEXT("para_squads"), false, static_cast<uint8>(ParaSquad), ParaDropCount},
		{TEXT("para_vehicles_by_cargo_plane"), true, static_cast<uint8>(ParaVehicle), ParaDropCount},
		{TEXT("cargo_squads"), false, static_cast<uint8>(CargoSquad), 1},
		{TEXT("cargo_hard_extra_vehicles"), true, static_cast<uint8>(CargoHardVehicle), ExpectedCargoVehicles},
		{TEXT("start_when_destroyed_squads"), false, static_cast<uint8>(LinkedSquad), 1}
	};
	int32 TotalDelivered = 0;
	int32 TotalRegistered = 0;
	for (const FExpected& Expected : ExpectedUnits)
	{
		int32 Registered = 0;
		const int32 Delivered = CountDeliveredUnits(Expected.bVehicle, Expected.Subtype, Registered);
		TotalDelivered += Delivered;
		TotalRegistered += Registered;
		Check(Delivered == Expected.Count, FString(TEXT("delivered_")) + Expected.Name,
		      FString::Printf(TEXT("%d (expected %d) registered=%d"), Delivered, Expected.Count, Registered));
	}
	Check(TotalDelivered > 0 && TotalRegistered == TotalDelivered, TEXT("landed_units_join_enemy_direct_control"),
	      FString::Printf(TEXT("registered=%d of %d"), TotalRegistered, TotalDelivered));
}

void UEnemyAirDropTestSubsystem::CheckNothingVisibleInGame(const AActor* PlacedActor, const FString& Name)
{
	if (not IsValid(PlacedActor))
	{
		Check(false, TEXT("hidden_in_game_") + Name, TEXT("actor gone"));
		return;
	}
	TArray<UPrimitiveComponent*> Primitives;
	PlacedActor->GetComponents<UPrimitiveComponent>(Primitives);
	int32 VisibleInGame = 0;
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		VisibleInGame += IsValid(Primitive) && Primitive->IsVisible() && not Primitive->bHiddenInGame ? 1 : 0;
	}
	Check(PlacedActor->IsHidden() && VisibleInGame == 0, TEXT("hidden_in_game_") + Name,
	      FString::Printf(TEXT("actor_hidden=%d visible_primitives=%d of %d"), PlacedActor->IsHidden(), VisibleInGame,
	                      Primitives.Num()));
}

bool UEnemyAirDropTestSubsystem::FindNavigableCenter(FVector& OutCenter) const
{
	using namespace EnemyAirDropTestPrivate;
	const ACPPController* const Controller = FRTS_Statics::GetRTSController(GetWorld());
	const FVector CameraLocation = IsValid(Controller) && IsValid(Controller->PlayerCameraManager)
		                               ? Controller->PlayerCameraManager->GetCameraLocation()
		                               : FVector::ZeroVector;
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	FHitResult Hit;
	const FVector Top(CameraLocation.X, CameraLocation.Y, GroundTraceHalfHeight);
	const FVector Bottom(CameraLocation.X, CameraLocation.Y, -GroundTraceHalfHeight);
	const FVector Ground = GetWorld()->LineTraceSingleByObjectType(Hit, Top, Bottom, ObjectQuery)
		                       ? Hit.ImpactPoint
		                       : CameraLocation;
	UNavigationSystemV1* const NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Projected;
	if (not IsValid(NavigationSystem) || not NavigationSystem->ProjectPointToNavigation(Ground, Projected, NavExtent))
	{
		return false;
	}
	OutCenter = Projected.Location;
	return true;
}

#else

void UEnemyAirDropTestSubsystem::TickSteps() {}
void UEnemyAirDropTestSubsystem::AdvanceStep(const EStep) {}
void UEnemyAirDropTestSubsystem::StartTest() {}
void UEnemyAirDropTestSubsystem::OnActorSpawned(AActor*) {}
void UEnemyAirDropTestSubsystem::TrackParaRuns() {}
void UEnemyAirDropTestSubsystem::TrackDeliveredUnits() {}
bool UEnemyAirDropTestSubsystem::GetIsNearLandingPoint(const FVector&) const { return false; }
void UEnemyAirDropTestSubsystem::CheckConfiguredDropHasNoProblem() {}
AEnemyAirDrop* UEnemyAirDropTestSubsystem::SpawnAirDrop(const FVector&, const FName) { return nullptr; }
void UEnemyAirDropTestSubsystem::SpawnParaDrop() {}
void UEnemyAirDropTestSubsystem::SpawnCargoDrop() {}
void UEnemyAirDropTestSubsystem::SpawnLinkedDrops() {}
void UEnemyAirDropTestSubsystem::OnParaDropStarted(AEnemyAirDrop*, int32) {}
void UEnemyAirDropTestSubsystem::OnParaDropFinished(AEnemyAirDrop*, int32) {}
void UEnemyAirDropTestSubsystem::OnParaAirDropFinished(AEnemyAirDrop*) {}
void UEnemyAirDropTestSubsystem::OnCargoAirDropFinished(AEnemyAirDrop*) {}
void UEnemyAirDropTestSubsystem::OnWhileAliveAirDropFinished(AEnemyAirDrop*) {}
void UEnemyAirDropTestSubsystem::OnWhenDestroyedDropStarted(AEnemyAirDrop*, int32) {}
void UEnemyAirDropTestSubsystem::CheckTemplates() {}
void UEnemyAirDropTestSubsystem::CheckLinkedDropsBeforeTrigger() {}
void UEnemyAirDropTestSubsystem::CheckWhenDestroyedStarted() {}
void UEnemyAirDropTestSubsystem::CheckParaDrops() {}
int32 UEnemyAirDropTestSubsystem::CountDeliveredUnits(const bool, const uint8, int32&) const { return 0; }
void UEnemyAirDropTestSubsystem::CheckDeliveredUnits() {}
void UEnemyAirDropTestSubsystem::CheckNothingVisibleInGame(const AActor*, const FString&) {}
bool UEnemyAirDropTestSubsystem::FindNavigableCenter(FVector&) const { return false; }

#endif

void UEnemyAirDropTestSubsystem::Evaluate()
{
	if (M_Step == EStep::Finished)
	{
		return;
	}
	M_Lines.Add(FString::Printf(TEXT("SUMMARY %s failed=%d"), M_Failed == 0 ? TEXT("PASS") : TEXT("FAIL"), M_Failed));
	UE_LOG(LogRTSEnemyAirDropTest, Display, TEXT("RTS_AIRDROP_TEST %s"), *M_Lines.Last());
	const FString ResultsPath = FPaths::ProjectSavedDir() / TEXT("EnemyAirDropTest") / TEXT("results.txt");
	FFileHelper::SaveStringToFile(FString::Join(M_Lines, TEXT("\n")), *ResultsPath);
	M_Step = EStep::Finished;
	if (APlayerController* const Controller = GetWorld()->GetFirstPlayerController())
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UEnemyAirDropTestSubsystem::Check(const bool bCondition, const FString& Name, const FString& Detail)
{
	M_Lines.Add(FString::Printf(TEXT("%s %s %s"), bCondition ? TEXT("PASS") : TEXT("FAIL"), *Name, *Detail));
	UE_LOG(LogRTSEnemyAirDropTest, Display, TEXT("RTS_AIRDROP_TEST %s"), *M_Lines.Last());
	M_Failed += bCondition ? 0 : 1;
}
