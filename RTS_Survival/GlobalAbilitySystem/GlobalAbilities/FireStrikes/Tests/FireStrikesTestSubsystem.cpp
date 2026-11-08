// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FireStrikesTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PostProcessComponent.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Barrage/GA_Barrage.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/FireStrikes/GA_IncendiaryAirStrike.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/FireStrikes/IncendiaryCanister.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireManager/RTSFireManager.h"
#include "RTS_Survival/Subsystems/FireSubsystem/RTSFireSubsystem.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSFireStrikesTest, Log, All);

namespace FireStrikesTestPrivate
{
	constexpr int32 TestPlayer = 1;
	constexpr uint8 EnemyPlayer = 2;
	// The landscape and shaders take several seconds before a screenshot shows the map.
	constexpr float StartDelaySeconds = 5.f;
	constexpr float CameraWarmupSeconds = 8.f;
	constexpr float ScenarioTimeoutSeconds = 150.f;
	constexpr float CameraTraceLength = 500000.f;
	constexpr float CameraDistance = 3600.f;
	constexpr float CameraPitch = -48.f;
	constexpr float CameraYaw = 35.f;
	constexpr float SpawnAboveGroundCm = 120.f;
	// The burn check starts once no new fire has appeared for this long, so blasts are over.
	constexpr float QuietAfterLastFireSeconds = 2.f;
	// The walk-in squad needs a moment to spawn its soldiers before the first sample.
	constexpr float WalkInSettleSeconds = 2.5f;
	constexpr float BurnSampleSpanSeconds = 5.f;
	// The fireball of a burst reads best about a second after it starts.
	constexpr float FireballShotDelaySeconds = 1.f;
	// A fire's flames sit on the landscape: a VFX this far above or below the ground trace is misplaced.
	constexpr float MaxFireHeightAboveGroundCm = 60.f;
	// Tolerance of the drop-point match for the strip ability (canisters burst exactly on their spot).
	constexpr float DropPointToleranceCm = 120.f;
	// The fire subsystem ticks once per second; allow that and a frame or two on top of the duration.
	constexpr float FireExpiryToleranceSeconds = 2.5f;
	constexpr float MinBurnDamage = 20.f;
	// "Lightly": a tank in the burning ground loses at most this share of its health.
	constexpr float MaxVehicleHealthShareLost = 0.25f;
	constexpr float StripLengthCm = 4000.f;
	// The barrage beside the strip in the combined scenario: its shells and fires stay clear of the canister fires.
	constexpr float SecondTargetOffsetYCm = 3200.f;
	// The fire barrage burns 20 s, the Ju 87's canisters 18 s; the shorter one bounds both.
	constexpr float BarrageFireSecondsLowerBound = 18.f;
	// Stand-ins for flame units burning beside the strikes in the combined scenario (flamethrower ground fire:
	// 3 m, 7 s), fed again before they burn out for as long as the strikes' fires burn.
	constexpr int32 FlamerFireCount = 6;
	constexpr float FlamerRadiusCm = 300.f;
	constexpr float FlamerDurationSeconds = 7.f;
	constexpr float FlamerVfxScale = 1.4f;
	constexpr float FlamerDamagePerSecond = 18.f;
	constexpr float FlamerRefeedSeconds = 5.f;
	constexpr float FlamerFeedUntilSeconds = 22.f;
	// Between the strip and the barrage: the test map's landscape ends just past the strip's other side.
	constexpr float FlamerRowOffsetYCm = 1100.f;
	constexpr float FlamerSpacingCm = 700.f;
	constexpr float FlamerSpotToleranceCm = 150.f;
	const TCHAR* const SquadClassPath = TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin.Rus_Squad_Mosin_C");
	const TCHAR* const VehicleClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");
	const TCHAR* const GroundFireVfxMarker = TEXT("GroundFire");
	const TCHAR* const Ju87Marker = TEXT("StukaJu87");
}

bool UFireStrikesTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("FireStrikesValidate"));
}

TStatId UFireStrikesTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFireStrikesTestSubsystem, STATGROUP_Tickables);
}

void UFireStrikesTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case EFireStrikesTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EFireStrikesTestPhase::Running:
		TickRun();
		return;
	case EFireStrikesTestPhase::Finished:
		return;
	}
}

void UFireStrikesTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	const FString Label = M_Scenarios.IsValidIndex(M_ScenarioIndex) ? M_Scenarios[M_ScenarioIndex].Label : TEXT("setup");
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSFireStrikesTest, Display, TEXT("RTS_FIRE_TEST CHECK PASS %s.%s %s"), *Label, *Description,
		       *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSFireStrikesTest, Error, TEXT("RTS_FIRE_TEST CHECK FAIL %s.%s %s"), *Label, *Description, *Details);
}

float UFireStrikesTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

float UFireStrikesTestSubsystem::GetRunSeconds() const
{
	return GetNow() - M_RunStartSeconds;
}

void UFireStrikesTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSFireStrikesTest, Display, TEXT("RTS_FIRE_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = EFireStrikesTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UFireStrikesTestSubsystem::TickWaitingForStart()
{
	using namespace FireStrikesTestPrivate;
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller))
	{
		return;
	}
	// The map boots paused behind the start-game widget; start it as the player's "Press To Start" does.
	UPlayerStartGameControl* StartGameControl = Controller->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	if (Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	if (M_StartSeconds < 0.f)
	{
		M_StartSeconds = GetNow();
		return;
	}
	if (GetNow() - M_StartSeconds < StartDelaySeconds)
	{
		return;
	}
	if (not IsValid(M_Camera))
	{
		if (not FindAnchor(M_Anchor))
		{
			Check(false, TEXT("found_ground_anchor"));
			Finish();
			return;
		}
		BuildScenarios();
		if (UWorld* World = GetWorld())
		{
			M_Camera = World->SpawnActor<ACameraActor>();
		}
		M_Target = M_Anchor;
		M_CameraWarmupStartSeconds = GetNow();
		DisableFogDarkening();
		UE_LOG(LogRTSFireStrikesTest, Display, TEXT("RTS_FIRE_TEST ANCHOR %s"), *M_Anchor.ToCompactString());
	}
	// The view jumps to the anchor first, so the landscape there has streamed in before the first screenshot.
	UpdateCamera();
	if (GetNow() - M_CameraWarmupStartSeconds < CameraWarmupSeconds)
	{
		return;
	}
	StartScenario(0);
}

bool UFireStrikesTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace FireStrikesTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("FireStrikesAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("FireStrikesAnchorY="), AnchorY))
	{
		return GlobalAbilityHelpers::TryFindGroundLocation(this, FVector(AnchorX, AnchorY, 0.f), OutAnchor);
	}
	const UWorld* World = GetWorld();
	const APlayerController* Controller = FRTS_Statics::GetRTSController(World);
	if (not IsValid(Controller) || not IsValid(Controller->PlayerCameraManager))
	{
		return false;
	}
	const FVector CameraLocation = Controller->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = Controller->PlayerCameraManager->GetCameraRotation().Vector();
	FHitResult GroundHit;
	if (not World->LineTraceSingleByChannel(GroundHit, CameraLocation, CameraLocation + CameraForward * CameraTraceLength,
	                                        ECC_Visibility))
	{
		return false;
	}
	return GlobalAbilityHelpers::TryFindGroundLocation(this, GroundHit.Location, OutAnchor);
}

void UFireStrikesTestSubsystem::DisableFogDarkening() const
{
	// The test has no player units nearby, so the fog post process would black out the pictures of the target.
	for (TActorIterator<AFowManager> It(GetWorld()); It; ++It)
	{
		TArray<UPostProcessComponent*> PostProcesses;
		It->GetComponents<UPostProcessComponent>(PostProcesses);
		for (UPostProcessComponent* PostProcess : PostProcesses)
		{
			if (IsValid(PostProcess))
			{
				PostProcess->bEnabled = false;
			}
		}
	}
}

void UFireStrikesTestSubsystem::BuildScenarios()
{
	FScenario Ju87;
	Ju87.AbilityType = EGlobalAbility::GA_GerIncendiaryJu87;
	Ju87.Label = TEXT("IncendiaryJu87");
	Ju87.bIsStripAbility = true;
	Ju87.MaxFireDistanceCm = FireStrikesTestPrivate::DropPointToleranceCm;
	Ju87.MinFires = 1;
	M_Scenarios.Add(Ju87);

	FScenario Barrage;
	Barrage.AbilityType = EGlobalAbility::GA_GerFireBarrage;
	Barrage.Label = TEXT("FireBarrage");
	Barrage.TargetOffset = FVector2D(-2500.f, 2500.f);
	Barrage.MinFires = 3;
	M_Scenarios.Add(Barrage);

	// Both at once beside each other: 6 canister fires + up to 8 shell fires must all burn to their own end (the
	// ground fire cap used to be 8 and put the oldest out early).
	FScenario Both = Ju87;
	Both.Label = TEXT("BothAtOnce");
	Both.bAlsoFireBarrage = true;
	Both.TargetOffset = FVector2D(0.f, -3000.f);
	Both.MinFires = 9;
	M_Scenarios.Add(Both);
}

void UFireStrikesTestSubsystem::ResetScenarioState()
{
	M_PlannedPoints.Reset();
	M_BaselineFireCount = 0;
	M_PeakFireCount = 0;
	M_SeenFires.Reset();
	M_LastNewFireSeconds = -1.f;
	M_FiresOutSeconds = -1.f;
	M_EndedCount = 0;
	M_EndedSeconds = -1.f;
	M_FireDurationSeconds = 0.f;
	M_FireRadiusCm = 0.f;
	M_PreExistingAircraft.Reset();
	M_StrikeAircraft.Reset();
	M_Ju87Seen = 0;
	M_EnemySquads.Reset();
	M_VictimSquad.Reset();
	M_WalkInSquad.Reset();
	M_Vehicle.Reset();
	M_VehicleStartHealth = -1.f;
	M_VehicleMaxHealth = -1.f;
	M_VehicleHealthAfterBlasts = -1.f;
	M_WalkInVehicle.Reset();
	M_WalkInVehicleEarlyHealth = -1.f;
	bM_TookFireballShot = false;
	M_VictimSquadStartHealth = -1.f;
	M_WalkInFireLocation = FVector::ZeroVector;
	M_WalkInSpawnSeconds = -1.f;
	M_EarlySamples.Reset();
	M_EarlySampleSeconds = -1.f;
	M_EarlyHealthSum = -1.f;
	M_LateHealthSum = -1.f;
	bM_TookStrikeShot = false;
	bM_TookBurningShot = false;
	bM_TookLateShot = false;
	M_Ability = nullptr;
	M_SecondAbility = nullptr;
	M_SecondTarget = FVector::ZeroVector;
	M_SecondTargetReachCm = 0.f;
	M_FlamerSpots.Reset();
	M_NextFlamerFeedSeconds = 0.f;
}

void UFireStrikesTestSubsystem::StartScenario(const int32 ScenarioIndex)
{
	ResetScenarioState();
	M_ScenarioIndex = ScenarioIndex;
	const FScenario& Scenario = M_Scenarios[ScenarioIndex];
	M_Phase = EFireStrikesTestPhase::Running;
	M_RunStartSeconds = GetNow();
	if (not CreateAbility(Scenario))
	{
		FinishScenario();
		return;
	}
	ReadFireTuning(Scenario);
	PlanTarget(Scenario);
	UE_LOG(LogRTSFireStrikesTest, Display, TEXT("RTS_FIRE_TEST SCENARIO %s target=%s fire_radius=%.0f duration=%.1f"),
	       *Scenario.Label, *M_Target.ToCompactString(), M_FireRadiusCm, M_FireDurationSeconds);
	const ARTSFireManager* FireManager = FindFireManager();
	Check(IsValid(FireManager), TEXT("fire_manager_exists"));
	M_BaselineFireCount = IsValid(FireManager) ? FireManager->GetActiveGroundFireCount() : 0;
	M_PeakFireCount = M_BaselineFireCount;
	for (TActorIterator<AAircraftMaster> It(GetWorld()); It; ++It)
	{
		M_PreExistingAircraft.Add(*It);
	}
	if (not Scenario.bAlsoFireBarrage)
	{
		SpawnVictims(Scenario);
	}
	UpdateCamera();
	ExecuteAbility(Scenario);
	RequestScreenshot(Scenario.Label + TEXT("_Executed"));
}

bool UFireStrikesTestSubsystem::CreateAbility(const FScenario& Scenario)
{
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	Check(IsValid(Manager) && IsValid(AbilitySet), TEXT("ability_manager_and_set"));
	if (not IsValid(Manager) || not IsValid(AbilitySet))
	{
		return false;
	}
	// A map started directly has no commander, so the ability is built from its template as the manager does.
	M_Ability = AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(Scenario.AbilityType);
	const bool bExpectedClass = Scenario.bIsStripAbility
		                            ? IsValid(Cast<UGA_IncendiaryAirStrike>(M_Ability))
		                            : IsValid(Cast<UGA_Barrage>(M_Ability));
	Check(bExpectedClass, TEXT("template_class"), IsValid(M_Ability) ? M_Ability->GetClass()->GetName() : TEXT("none"));
	if (not bExpectedClass)
	{
		return false;
	}
	Check(M_Ability->GetAbilityTier() == EGlobalAbilityTier::TierIII, TEXT("tier_iii"));
	Check(M_Ability->GetAbilityCosts().CoolDownTime >= 180, TEXT("cooldown_in_tier_iii_band"),
	      FString::Printf(TEXT("cooldown=%d"), M_Ability->GetAbilityCosts().CoolDownTime));
	M_Ability->InitGlobalAbility(FireStrikesTestPrivate::TestPlayer, Manager, Controller);
	M_Ability->OnGlobalAbilityEnded.AddUObject(this, &UFireStrikesTestSubsystem::OnAbilityEnded);
	return true;
}

void UFireStrikesTestSubsystem::ReadFireTuning(const FScenario& Scenario)
{
	FRTSGroundFireSettings GroundFire;
	if (const UGA_IncendiaryAirStrike* AirStrike = Cast<UGA_IncendiaryAirStrike>(M_Ability))
	{
		GroundFire = AirStrike->GetIncendiarySettings().CanisterSettings.GroundFire;
	}
	else
	{
		// The barrage keeps its weapon data private; the burn rides on its shells' FWeaponData::GroundFire.
		const FStructProperty* WeaponDataProperty = FindFProperty<FStructProperty>(
			M_Ability->GetClass(), TEXT("M_WeaponData"));
		if (WeaponDataProperty != nullptr && WeaponDataProperty->Struct == FWeaponData::StaticStruct())
		{
			GroundFire = WeaponDataProperty->ContainerPtrToValuePtr<FWeaponData>(M_Ability.Get())->GroundFire;
		}
	}
	Check(GroundFire.GetIsEnabled(), TEXT("ground_fire_enabled"),
	      FString::Printf(TEXT("radius=%.0f duration=%.1f dps=%.1f armor=%.0f/%.1f/%.0f vfx=%.2f"), GroundFire.RadiusCm,
	                      GroundFire.DurationSeconds, GroundFire.DamagePerSecond, GroundFire.FullArmorPen,
	                      GroundFire.ArmorPenFallOff, GroundFire.MaxArmorPen, GroundFire.VfxScale));
	M_FireDurationSeconds = GroundFire.DurationSeconds;
	M_FireRadiusCm = GroundFire.RadiusCm;
	if (not Scenario.bIsStripAbility)
	{
		// Shells land anywhere in the aimed radius; their fires reach one fire radius further.
		M_Scenarios[M_ScenarioIndex].MaxFireDistanceCm = M_Ability->GetAimSettings().AreaAim.AreaRadius + M_FireRadiusCm;
	}
}

void UFireStrikesTestSubsystem::PlanTarget(const FScenario& Scenario)
{
	using namespace FireStrikesTestPrivate;
	M_Target = M_Anchor;
	GlobalAbilityHelpers::TryFindGroundLocation(
		this, M_Anchor + FVector(Scenario.TargetOffset.X, Scenario.TargetOffset.Y, 0.f), M_Target);
	if (not Scenario.bIsStripAbility)
	{
		return;
	}
	const FVector HalfStrip(StripLengthCm * 0.5f, 0.f, 0.f);
	M_LineStart = M_Target - HalfStrip;
	M_LineEnd = M_Target + HalfStrip;
	M_Ability->GetStripDropPoints(M_LineStart, M_LineEnd, M_PlannedPoints);
	const UGA_IncendiaryAirStrike* AirStrike = Cast<UGA_IncendiaryAirStrike>(M_Ability);
	const int32 CanisterCount = IsValid(AirStrike) ? AirStrike->GetIncendiarySettings().CanisterCount : 0;
	Check(M_PlannedPoints.Num() == CanisterCount && CanisterCount > 0, TEXT("drop_points_planned"),
	      FString::Printf(TEXT("points=%d canisters=%d"), M_PlannedPoints.Num(), CanisterCount));
	if (not Scenario.bAlsoFireBarrage)
	{
		M_Scenarios[M_ScenarioIndex].ExactFires = M_PlannedPoints.Num();
	}
	for (int32 PointIndex = 0; PointIndex < M_PlannedPoints.Num(); ++PointIndex)
	{
		UE_LOG(LogRTSFireStrikesTest, Display, TEXT("RTS_FIRE_TEST PLANNED %d %s"), PointIndex,
		       *M_PlannedPoints[PointIndex].ToCompactString());
	}
}

void UFireStrikesTestSubsystem::SpawnVictims(const FScenario& Scenario)
{
	const FVector SquadSpot = Scenario.bIsStripAbility && M_PlannedPoints.Num() > 2 ? M_PlannedPoints[2] : M_Target;
	FVector VehicleSpot = M_Target + FVector(600.f, 0.f, 0.f);
	if (Scenario.bIsStripAbility && M_PlannedPoints.Num() > 4)
	{
		VehicleSpot = M_PlannedPoints[4];
	}
	GlobalAbilityHelpers::TryFindGroundLocation(this, VehicleSpot, VehicleSpot);
	M_VictimSquad = SpawnEnemySquad(SquadSpot);
	M_Vehicle = SpawnEnemyVehicle(VehicleSpot);
	Check(M_VictimSquad.IsValid() && M_Vehicle.IsValid(), TEXT("victims_spawned"),
	      FString::Printf(TEXT("squad_at=%s vehicle_at=%s"), *SquadSpot.ToCompactString(),
	                      *VehicleSpot.ToCompactString()));
}

AActor* UFireStrikesTestSubsystem::SpawnEnemyVehicle(const FVector& GroundLocation) const
{
	using namespace FireStrikesTestPrivate;
	UWorld* World = GetWorld();
	UClass* VehicleClass = LoadClass<AActor>(nullptr, VehicleClassPath);
	if (not IsValid(World) || not IsValid(VehicleClass))
	{
		return nullptr;
	}
	return FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*World, VehicleClass, FTransform(GroundLocation + FVector(0.f, 0.f, SpawnAboveGroundCm)), EnemyPlayer);
}

ASquadController* UFireStrikesTestSubsystem::SpawnEnemySquad(const FVector& GroundLocation)
{
	using namespace FireStrikesTestPrivate;
	UWorld* World = GetWorld();
	UClass* SquadClass = LoadClass<AActor>(nullptr, SquadClassPath);
	if (not IsValid(World) || not IsValid(SquadClass))
	{
		return nullptr;
	}
	ASquadController* Squad = Cast<ASquadController>(FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*World, SquadClass, FTransform(GroundLocation + FVector(0.f, 0.f, SpawnAboveGroundCm)), EnemyPlayer));
	if (IsValid(Squad))
	{
		M_EnemySquads.Add(Squad);
	}
	return Squad;
}

void UFireStrikesTestSubsystem::ExecuteAbility(const FScenario& Scenario)
{
	if (Scenario.bAlsoFireBarrage)
	{
		ExecuteSecondBarrage();
		PlanFlamerFires();
		FeedFlamerFires();
	}
	if (Scenario.bIsStripAbility)
	{
		M_Ability->ExecuteAbilityAlongLine(M_LineStart, M_LineEnd);
		return;
	}
	M_Ability->ExecuteAbilityAtLocation(M_Target);
}

void UFireStrikesTestSubsystem::PlanFlamerFires()
{
	using namespace FireStrikesTestPrivate;
	for (int32 FlamerIndex = 0; FlamerIndex < FlamerFireCount; ++FlamerIndex)
	{
		const float AlongX = (FlamerIndex - (FlamerFireCount - 1) * 0.5f) * FlamerSpacingCm;
		FVector Spot;
		GlobalAbilityHelpers::TryFindGroundLocation(this, M_Target + FVector(AlongX, FlamerRowOffsetYCm, 0.f), Spot);
		M_FlamerSpots.Add(Spot);
	}
}

void UFireStrikesTestSubsystem::FeedFlamerFires()
{
	using namespace FireStrikesTestPrivate;
	URTSFireSubsystem* FireSubsystem = GetWorld()->GetSubsystem<URTSFireSubsystem>();
	if (not IsValid(FireSubsystem) || M_FlamerSpots.IsEmpty())
	{
		return;
	}
	FRTSGroundFireSettings Flamer;
	Flamer.RadiusCm = FlamerRadiusCm;
	Flamer.DurationSeconds = FlamerDurationSeconds;
	Flamer.DamagePerSecond = FlamerDamagePerSecond;
	Flamer.VfxScale = FlamerVfxScale;
	for (const FVector& Spot : M_FlamerSpots)
	{
		// The same spot again merges into its burning area and keeps it alive, like a flamer still firing.
		FireSubsystem->RequestGroundFire(Flamer, Spot, TestPlayer);
	}
	M_NextFlamerFeedSeconds = GetRunSeconds() + FlamerRefeedSeconds;
}

bool UFireStrikesTestSubsystem::GetIsFlamerFire(const FVector& Location) const
{
	return M_FlamerSpots.ContainsByPredicate([&Location](const FVector& Spot)
	{
		return FVector::Dist2D(Spot, Location) <= FireStrikesTestPrivate::FlamerSpotToleranceCm;
	});
}

void UFireStrikesTestSubsystem::ExecuteSecondBarrage()
{
	using namespace FireStrikesTestPrivate;
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	M_SecondAbility = IsValid(AbilitySet)
		                  ? AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(EGlobalAbility::GA_GerFireBarrage)
		                  : nullptr;
	Check(IsValid(M_SecondAbility) && IsValid(Manager), TEXT("second_ability_barrage"));
	if (not IsValid(M_SecondAbility) || not IsValid(Manager))
	{
		return;
	}
	GlobalAbilityHelpers::TryFindGroundLocation(this, M_Target + FVector(0.f, SecondTargetOffsetYCm, 0.f),
	                                            M_SecondTarget);
	M_SecondTargetReachCm = M_SecondAbility->GetAimSettings().AreaAim.AreaRadius + M_FireRadiusCm;
	M_SecondAbility->InitGlobalAbility(TestPlayer, Manager, Controller);
	M_SecondAbility->OnGlobalAbilityEnded.AddUObject(this, &UFireStrikesTestSubsystem::OnAbilityEnded);
	M_SecondAbility->ExecuteAbilityAtLocation(M_SecondTarget);
	UE_LOG(LogRTSFireStrikesTest, Display, TEXT("RTS_FIRE_TEST SECOND_ABILITY barrage at=%s"),
	       *M_SecondTarget.ToCompactString());
}

void UFireStrikesTestSubsystem::OnAbilityEnded(UGlobalAbility* Ability)
{
	if (Ability != M_Ability && Ability != M_SecondAbility)
	{
		return;
	}
	++M_EndedCount;
	if (M_EndedSeconds < 0.f)
	{
		M_EndedSeconds = GetRunSeconds();
	}
	UE_LOG(LogRTSFireStrikesTest, Display, TEXT("RTS_FIRE_TEST ABILITY_ENDED count=%d at=%.1fs"), M_EndedCount,
	       GetRunSeconds());
}

void UFireStrikesTestSubsystem::TickRun()
{
	using namespace FireStrikesTestPrivate;
	if (not M_Scenarios.IsValidIndex(M_ScenarioIndex))
	{
		return;
	}
	UpdateCamera();
	MatchEnemySoldiersToSquadOwner();
	TrackAircraft();
	TrackFires();
	TrackWalkInSquad();
	if (not M_FlamerSpots.IsEmpty() && GetRunSeconds() >= M_NextFlamerFeedSeconds
		&& GetRunSeconds() < FlamerFeedUntilSeconds)
	{
		FeedFlamerFires();
	}
	if (M_VehicleStartHealth < 0.f && M_Vehicle.IsValid() && GetUnitMaxHealth(M_Vehicle.Get()) > 0.f)
	{
		M_VehicleStartHealth = GetUnitHealth(M_Vehicle.Get());
		M_VehicleMaxHealth = GetUnitMaxHealth(M_Vehicle.Get());
		M_VictimSquadStartHealth = GetSquadHealth(M_VictimSquad.Get());
	}
	if (GetIsScenarioDone() || GetRunSeconds() > ScenarioTimeoutSeconds)
	{
		FinishScenario();
	}
}

void UFireStrikesTestSubsystem::MatchEnemySoldiersToSquadOwner()
{
	for (const TWeakObjectPtr<ASquadController>& Squad : M_EnemySquads)
	{
		if (not Squad.IsValid())
		{
			continue;
		}
		const TArray<ASquadUnit*> Soldiers = Squad->GetSquadUnitsChecked();
		for (ASquadUnit* Soldier : Soldiers)
		{
			FRTSUnitOwnerSpawning::MatchSoldierToSquadOwner(Soldier, FireStrikesTestPrivate::EnemyPlayer);
		}
	}
}

void UFireStrikesTestSubsystem::TrackAircraft()
{
	for (TActorIterator<AAircraftMaster> It(GetWorld()); It; ++It)
	{
		const TWeakObjectPtr<AActor> Aircraft(*It);
		if (M_PreExistingAircraft.Contains(Aircraft) || M_StrikeAircraft.Contains(Aircraft))
		{
			continue;
		}
		M_StrikeAircraft.Add(Aircraft);
		M_Ju87Seen += It->GetClass()->GetName().Contains(FireStrikesTestPrivate::Ju87Marker) ? 1 : 0;
		UE_LOG(LogRTSFireStrikesTest, Display, TEXT("RTS_FIRE_TEST AIRCRAFT %s at=%.1fs loc=%s"),
		       *It->GetClass()->GetName(), GetRunSeconds(), *It->GetActorLocation().ToCompactString());
	}
	if (not bM_TookStrikeShot && CountCanisters() > 0)
	{
		bM_TookStrikeShot = true;
		RequestScreenshot(M_Scenarios[M_ScenarioIndex].Label + TEXT("_Drop"));
	}
}

void UFireStrikesTestSubsystem::TrackFires()
{
	const ARTSFireManager* FireManager = FindFireManager();
	if (not IsValid(FireManager))
	{
		return;
	}
	const int32 ActiveFires = FireManager->GetActiveGroundFireCount();
	M_PeakFireCount = FMath::Max(M_PeakFireCount, ActiveFires);
	TArray<UNiagaraComponent*> ActiveVfx;
	GatherActiveGroundFireVfx(ActiveVfx);
	TrackFireEnds(ActiveVfx);
	for (UNiagaraComponent* Vfx : ActiveVfx)
	{
		// A pooled component is a new fire again once its previous fire was seen ending.
		const bool bKnown = M_SeenFires.ContainsByPredicate([Vfx](const FSeenFire& Seen)
		{
			return Seen.Component.Get() == Vfx && Seen.EndSeconds < 0.f;
		});
		if (bKnown)
		{
			continue;
		}
		FSeenFire Seen;
		Seen.Component = Vfx;
		Seen.Location = Vfx->GetComponentLocation();
		Seen.FirstSeenSeconds = GetRunSeconds();
		M_SeenFires.Add(Seen);
		M_LastNewFireSeconds = GetRunSeconds();
		UE_LOG(LogRTSFireStrikesTest, Display, TEXT("RTS_FIRE_TEST FIRE_STARTED %d at=%.1fs loc=%s active_areas=%d"),
		       M_SeenFires.Num(), Seen.FirstSeenSeconds, *Seen.Location.ToCompactString(), ActiveFires);
		if (not bM_TookStrikeShot)
		{
			bM_TookStrikeShot = true;
			RequestScreenshot(M_Scenarios[M_ScenarioIndex].Label + TEXT("_Impact"));
		}
	}
	if (not bM_TookFireballShot && not M_SeenFires.IsEmpty()
		&& GetRunSeconds() - M_SeenFires[0].FirstSeenSeconds >= FireStrikesTestPrivate::FireballShotDelaySeconds)
	{
		bM_TookFireballShot = true;
		RequestScreenshot(M_Scenarios[M_ScenarioIndex].Label + TEXT("_Fireball"));
	}
	if (M_FiresOutSeconds < 0.f && not M_SeenFires.IsEmpty() && ActiveFires <= M_BaselineFireCount)
	{
		M_FiresOutSeconds = GetRunSeconds();
		UE_LOG(LogRTSFireStrikesTest, Display, TEXT("RTS_FIRE_TEST FIRES_OUT at=%.1fs"), M_FiresOutSeconds);
		RequestScreenshot(M_Scenarios[M_ScenarioIndex].Label + TEXT("_FiresOut"));
	}
}

void UFireStrikesTestSubsystem::TrackFireEnds(const TArray<UNiagaraComponent*>& ActiveVfx)
{
	constexpr float MovedToleranceCm = 10.f;
	for (FSeenFire& Seen : M_SeenFires)
	{
		if (Seen.EndSeconds >= 0.f)
		{
			continue;
		}
		UNiagaraComponent* Component = Seen.Component.Get();
		const bool bStillBurning = IsValid(Component) && ActiveVfx.Contains(Component)
			&& FVector::Dist(Component->GetComponentLocation(), Seen.Location) <= MovedToleranceCm;
		if (not bStillBurning)
		{
			Seen.EndSeconds = GetRunSeconds();
		}
	}
}

void UFireStrikesTestSubsystem::TrackWalkInSquad()
{
	using namespace FireStrikesTestPrivate;
	if (M_Scenarios[M_ScenarioIndex].bAlsoFireBarrage)
	{
		return;
	}
	const bool bFiresQuiet = M_LastNewFireSeconds >= 0.f
		&& GetRunSeconds() - M_LastNewFireSeconds >= QuietAfterLastFireSeconds && M_FiresOutSeconds < 0.f;
	if (M_WalkInSpawnSeconds < 0.f && bFiresQuiet)
	{
		// A fresh squad straight into the newest fire: no blast hits it, so all its losses are the burn.
		const FSeenFire& NewestFire = M_SeenFires.Last();
		M_WalkInFireLocation = NewestFire.Location;
		M_WalkInSquad = SpawnEnemySquad(M_WalkInFireLocation);
		// A fresh tank too, in the oldest fire: shells that hit the victim tank say nothing about the burn.
		M_WalkInVehicle = SpawnEnemyVehicle(M_SeenFires[0].Location);
		M_WalkInSpawnSeconds = GetRunSeconds();
		UE_LOG(LogRTSFireStrikesTest, Display, TEXT("RTS_FIRE_TEST WALK_IN_SQUAD %s at=%s t=%.1fs"),
		       M_WalkInSquad.IsValid() ? *M_WalkInSquad->GetName() : TEXT("none"),
		       *M_WalkInFireLocation.ToCompactString(), M_WalkInSpawnSeconds);
		return;
	}
	if (M_WalkInSpawnSeconds < 0.f)
	{
		return;
	}
	if (M_EarlySampleSeconds < 0.f && GetRunSeconds() - M_WalkInSpawnSeconds >= WalkInSettleSeconds)
	{
		TakeEarlySamples();
		return;
	}
	if (M_EarlySampleSeconds >= 0.f && M_LateHealthSum < 0.f
		&& GetRunSeconds() - M_EarlySampleSeconds >= BurnSampleSpanSeconds)
	{
		TakeLateSamples();
	}
}

void UFireStrikesTestSubsystem::TakeEarlySamples()
{
	M_EarlySampleSeconds = GetRunSeconds();
	M_EarlyHealthSum = 0.f;
	// The blasts are over by now: from here on the vehicle only loses health to the burn.
	M_WalkInVehicleEarlyHealth = M_WalkInVehicle.IsValid() ? GetUnitHealth(M_WalkInVehicle.Get()) : -1.f;
	M_VehicleHealthAfterBlasts = M_Vehicle.IsValid() ? GetUnitHealth(M_Vehicle.Get()) : 0.f;
	if (M_WalkInSquad.IsValid())
	{
		const TArray<ASquadUnit*> Soldiers = M_WalkInSquad->GetSquadUnitsChecked();
		for (ASquadUnit* Soldier : Soldiers)
		{
			if (not IsValid(Soldier) || not GetIsInsideAnyFire(Soldier->GetActorLocation()))
			{
				continue;
			}
			FSoldierSample Sample;
			Sample.Soldier = Soldier;
			Sample.Health = GetUnitHealth(Soldier);
			M_EarlySamples.Add(Sample);
			M_EarlyHealthSum += Sample.Health;
			const URTSComponent* SoldierRTS = Soldier->FindComponentByClass<URTSComponent>();
			UE_LOG(LogRTSFireStrikesTest, Display, TEXT("RTS_FIRE_TEST EARLY soldier=%s owner=%d health=%.0f"),
			       *Soldier->GetName(), IsValid(SoldierRTS) ? SoldierRTS->GetOwningPlayer() : -1, Sample.Health);
		}
	}
	if (not bM_TookBurningShot)
	{
		bM_TookBurningShot = true;
		RequestScreenshot(M_Scenarios[M_ScenarioIndex].Label + TEXT("_Burning"));
	}
}

void UFireStrikesTestSubsystem::TakeLateSamples()
{
	M_LateHealthSum = 0.f;
	for (const FSoldierSample& Sample : M_EarlySamples)
	{
		// A soldier burnt to death (or removed) counts as zero health.
		const float Health = Sample.Soldier.IsValid() ? GetUnitHealth(Sample.Soldier.Get()) : 0.f;
		M_LateHealthSum += Health;
		UE_LOG(LogRTSFireStrikesTest, Display, TEXT("RTS_FIRE_TEST LATE soldier=%s health=%.0f (was %.0f)"),
		       Sample.Soldier.IsValid() ? *Sample.Soldier->GetName() : TEXT("gone"), Health, Sample.Health);
	}
	if (not bM_TookLateShot)
	{
		bM_TookLateShot = true;
		RequestScreenshot(M_Scenarios[M_ScenarioIndex].Label + TEXT("_BurningLate"));
	}
}

bool UFireStrikesTestSubsystem::GetIsScenarioDone() const
{
	using namespace FireStrikesTestPrivate;
	const bool bCombined = M_Scenarios[M_ScenarioIndex].bAlsoFireBarrage;
	const int32 ExpectedEnds = bCombined ? 2 : 1;
	if (M_EndedCount < ExpectedEnds || M_FiresOutSeconds < 0.f || (not bCombined && M_LateHealthSum < 0.f))
	{
		return false;
	}
	// One more second so a late second end broadcast would still be counted.
	return CountLivingStrikeAircraft() == 0 && GetRunSeconds() - M_FiresOutSeconds > 1.f;
}

void UFireStrikesTestSubsystem::FinishScenario()
{
	if (M_Scenarios.IsValidIndex(M_ScenarioIndex) && IsValid(M_Ability))
	{
		const FScenario& Scenario = M_Scenarios[M_ScenarioIndex];
		CheckFirePlacement(Scenario);
		CheckFireLifetimes(Scenario);
		if (not Scenario.bAlsoFireBarrage)
		{
			CheckBurnDamage();
			CheckVehicle();
		}
		CheckCleanEnd();
		M_Ability->OnGlobalAbilityEnded.RemoveAll(this);
		if (IsValid(M_SecondAbility))
		{
			M_SecondAbility->OnGlobalAbilityEnded.RemoveAll(this);
		}
	}
	for (const TWeakObjectPtr<ASquadController>& Squad : M_EnemySquads)
	{
		if (Squad.IsValid())
		{
			Squad->Destroy();
		}
	}
	if (M_WalkInVehicle.IsValid())
	{
		M_WalkInVehicle->Destroy();
	}
	if (M_Vehicle.IsValid())
	{
		M_Vehicle->Destroy();
	}
	const int32 NextScenario = M_ScenarioIndex + 1;
	if (M_Scenarios.IsValidIndex(NextScenario))
	{
		StartScenario(NextScenario);
		return;
	}
	Finish();
}

void UFireStrikesTestSubsystem::CheckFirePlacement(const FScenario& Scenario)
{
	using namespace FireStrikesTestPrivate;
	const int32 NewAreas = M_PeakFireCount - M_BaselineFireCount;
	Check(M_SeenFires.Num() >= Scenario.MinFires, TEXT("fires_started"),
	      FString::Printf(TEXT("fires=%d peak_new_areas=%d"), M_SeenFires.Num(), NewAreas));
	if (Scenario.ExactFires != INDEX_NONE)
	{
		Check(M_SeenFires.Num() == Scenario.ExactFires && NewAreas == Scenario.ExactFires, TEXT("one_fire_per_canister"),
		      FString::Printf(TEXT("fires=%d areas=%d expected=%d"), M_SeenFires.Num(), NewAreas,
		                      Scenario.ExactFires));
	}
	int32 OnGround = 0;
	int32 WhereIntended = 0;
	for (const FSeenFire& Seen : M_SeenFires)
	{
		FVector Ground;
		const bool bHasGround = GlobalAbilityHelpers::TryFindGroundLocation(this, Seen.Location, Ground);
		OnGround += bHasGround && FMath::Abs(Seen.Location.Z - Ground.Z) <= MaxFireHeightAboveGroundCm ? 1 : 0;
		float NearestDistance = FVector::Dist2D(Seen.Location, M_Target);
		for (const FVector& Planned : M_PlannedPoints)
		{
			NearestDistance = FMath::Min(NearestDistance, FVector::Dist2D(Seen.Location, Planned));
		}
		const bool bNearSecondTarget = Scenario.bAlsoFireBarrage
			&& FVector::Dist2D(Seen.Location, M_SecondTarget) <= M_SecondTargetReachCm;
		WhereIntended += NearestDistance <= Scenario.MaxFireDistanceCm || bNearSecondTarget
		                 || GetIsFlamerFire(Seen.Location)
			                 ? 1
			                 : 0;
		UE_LOG(LogRTSFireStrikesTest, Display,
		       TEXT("RTS_FIRE_TEST FIRE loc=%s ground_z=%.0f nearest_intended=%.0f started=%.1fs"),
		       *Seen.Location.ToCompactString(), Ground.Z, NearestDistance, Seen.FirstSeenSeconds);
	}
	Check(not M_SeenFires.IsEmpty() && OnGround == M_SeenFires.Num(), TEXT("fires_on_ground"),
	      FString::Printf(TEXT("%d of %d"), OnGround, M_SeenFires.Num()));
	Check(not M_SeenFires.IsEmpty() && WhereIntended == M_SeenFires.Num(), TEXT("fires_where_intended"),
	      FString::Printf(TEXT("%d of %d within %.0f cm"), WhereIntended, M_SeenFires.Num(),
	                      Scenario.MaxFireDistanceCm));
	if (Scenario.bIsStripAbility)
	{
		int32 PlannedWithFire = 0;
		for (const FVector& Planned : M_PlannedPoints)
		{
			PlannedWithFire += M_SeenFires.ContainsByPredicate([&Planned](const FSeenFire& Seen)
			{
				return FVector::Dist2D(Seen.Location, Planned) <= DropPointToleranceCm;
			})
				                   ? 1
				                   : 0;
		}
		Check(PlannedWithFire == M_PlannedPoints.Num(), TEXT("every_drop_point_burns"),
		      FString::Printf(TEXT("%d of %d"), PlannedWithFire, M_PlannedPoints.Num()));
	}
}

void UFireStrikesTestSubsystem::CheckFireLifetimes(const FScenario& Scenario)
{
	using namespace FireStrikesTestPrivate;
	// Every fire burns its own duration: none is put out early to make room under the ground fire cap.
	const float ShortestDuration = Scenario.bAlsoFireBarrage
		                               ? FMath::Min(M_FireDurationSeconds, BarrageFireSecondsLowerBound)
		                               : M_FireDurationSeconds;
	int32 EarlyOuts = 0;
	float ShortestLife = TNumericLimits<float>::Max();
	int32 FlamerFires = 0;
	for (const FSeenFire& Seen : M_SeenFires)
	{
		if (GetIsFlamerFire(Seen.Location))
		{
			// Fed flamer fires burn as long as they are fed; only the strikes' fires have a fixed duration.
			++FlamerFires;
			continue;
		}
		const float Life = (Seen.EndSeconds >= 0.f ? Seen.EndSeconds : GetRunSeconds()) - Seen.FirstSeenSeconds;
		ShortestLife = FMath::Min(ShortestLife, Life);
		EarlyOuts += Life < ShortestDuration - FireExpiryToleranceSeconds ? 1 : 0;
	}
	Check(not M_SeenFires.IsEmpty() && EarlyOuts == 0, TEXT("no_fire_put_out_early"),
	      FString::Printf(TEXT("strike fires=%d early=%d shortest=%.1fs duration>=%.1fs (+%d flamer fires)"),
	                      M_SeenFires.Num() - FlamerFires, EarlyOuts, ShortestLife, ShortestDuration, FlamerFires));
	if (Scenario.bAlsoFireBarrage)
	{
		Check(FlamerFires == FlamerFireCount, TEXT("flamer_fires_burned_beside"),
		      FString::Printf(TEXT("flamer fires=%d expected=%d"), FlamerFires, FlamerFireCount));
	}
	const ARTSFireManager* FireManager = FindFireManager();
	const int32 NewAreas = M_PeakFireCount - M_BaselineFireCount;
	Check(NewAreas == M_SeenFires.Num(), TEXT("all_fires_burned_together"),
	      FString::Printf(TEXT("peak areas=%d fires=%d cap=%d"), NewAreas, M_SeenFires.Num(),
	                      IsValid(FireManager) ? FireManager->GetMaxActiveGroundFires() : -1));
}

void UFireStrikesTestSubsystem::CheckBurnDamage()
{
	using namespace FireStrikesTestPrivate;
	Check(not M_EarlySamples.IsEmpty(), TEXT("walk_in_soldiers_in_fire"),
	      FString::Printf(TEXT("soldiers=%d"), M_EarlySamples.Num()));
	Check(M_LateHealthSum >= 0.f && M_EarlyHealthSum - M_LateHealthSum >= MinBurnDamage,
	      TEXT("infantry_burn_over_time"),
	      FString::Printf(TEXT("health %.0f -> %.0f over %.0f s"), M_EarlyHealthSum, M_LateHealthSum,
	                      BurnSampleSpanSeconds));
	const float VictimEndHealth = GetSquadHealth(M_VictimSquad.Get());
	UE_LOG(LogRTSFireStrikesTest, Display, TEXT("RTS_FIRE_TEST VICTIM_SQUAD health %.0f -> %.0f"),
	       M_VictimSquadStartHealth, VictimEndHealth);
}

void UFireStrikesTestSubsystem::CheckVehicle()
{
	using namespace FireStrikesTestPrivate;
	// The victim tank stood under the strike; direct shell or canister hits are not the burn, so it is only logged.
	const float VictimEndHealth = M_Vehicle.IsValid() ? GetUnitHealth(M_Vehicle.Get()) : 0.f;
	UE_LOG(LogRTSFireStrikesTest, Display,
	       TEXT("RTS_FIRE_TEST VICTIM_T34 health %.0f at start, %.0f after the blasts, %.0f at the end of %.0f"),
	       M_VehicleStartHealth, M_VehicleHealthAfterBlasts, VictimEndHealth, M_VehicleMaxHealth);
	// The walk-in tank drove into a burning area after the blasts: all its losses are the burn.
	const float WalkInMaxHealth = GetUnitMaxHealth(M_WalkInVehicle.Get());
	const float WalkInEndHealth = GetUnitHealth(M_WalkInVehicle.Get());
	const float BurnShare = WalkInMaxHealth > 0.f && M_WalkInVehicleEarlyHealth >= 0.f
		                        ? (M_WalkInVehicleEarlyHealth - WalkInEndHealth) / WalkInMaxHealth
		                        : 1.f;
	Check(M_WalkInVehicle.IsValid() && BurnShare <= MaxVehicleHealthShareLost, TEXT("vehicle_lightly_burnt"),
	      FString::Printf(TEXT("walk-in T-34 health %.0f -> %.0f of %.0f in the fire (burn %.0f%%)"),
	                      M_WalkInVehicleEarlyHealth, WalkInEndHealth, WalkInMaxHealth, BurnShare * 100.f));
}

void UFireStrikesTestSubsystem::CheckCleanEnd()
{
	using namespace FireStrikesTestPrivate;
	const int32 ExpectedEnds = M_Scenarios[M_ScenarioIndex].bAlsoFireBarrage ? 2 : 1;
	Check(M_EndedCount == ExpectedEnds, TEXT("each_ability_ended_once"),
	      FString::Printf(TEXT("ended=%d at=%.1fs"), M_EndedCount, M_EndedSeconds));
	Check(M_FiresOutSeconds >= 0.f, TEXT("fires_went_out"), FString::Printf(TEXT("out at %.1fs"), M_FiresOutSeconds));
	if (M_FiresOutSeconds >= 0.f && M_LastNewFireSeconds >= 0.f && not M_Scenarios[M_ScenarioIndex].bAlsoFireBarrage)
	{
		const float LastFireBurnt = M_FiresOutSeconds - M_LastNewFireSeconds;
		Check(LastFireBurnt <= M_FireDurationSeconds + FireExpiryToleranceSeconds
		      && LastFireBurnt >= M_FireDurationSeconds - FireExpiryToleranceSeconds, TEXT("fires_burn_their_duration"),
		      FString::Printf(TEXT("last fire burnt %.1fs, duration %.1fs"), LastFireBurnt, M_FireDurationSeconds));
	}
	TArray<UNiagaraComponent*> ActiveVfx;
	GatherActiveGroundFireVfx(ActiveVfx);
	Check(ActiveVfx.IsEmpty(), TEXT("flames_stopped"), FString::Printf(TEXT("active_vfx=%d"), ActiveVfx.Num()));
	Check(CountCanisters() == 0, TEXT("no_canisters_left"));
	const bool bIsStrip = M_Scenarios[M_ScenarioIndex].bIsStripAbility;
	if (bIsStrip)
	{
		Check(M_StrikeAircraft.Num() == 1 && M_Ju87Seen == 1, TEXT("one_ju87"),
		      FString::Printf(TEXT("aircraft=%d ju87=%d"), M_StrikeAircraft.Num(), M_Ju87Seen));
	}
	Check(CountLivingStrikeAircraft() == 0, TEXT("aircraft_gone"));
}

ARTSFireManager* UFireStrikesTestSubsystem::FindFireManager() const
{
	for (TActorIterator<ARTSFireManager> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void UFireStrikesTestSubsystem::GatherActiveGroundFireVfx(TArray<UNiagaraComponent*>& OutComponents) const
{
	const ARTSFireManager* FireManager = FindFireManager();
	if (not IsValid(FireManager))
	{
		return;
	}
	TArray<UNiagaraComponent*> Components;
	FireManager->GetComponents<UNiagaraComponent>(Components);
	for (UNiagaraComponent* Component : Components)
	{
		const UNiagaraSystem* Asset = IsValid(Component) ? Component->GetAsset() : nullptr;
		if (IsValid(Asset) && Component->IsActive()
			&& Asset->GetName().Contains(FireStrikesTestPrivate::GroundFireVfxMarker))
		{
			OutComponents.Add(Component);
		}
	}
}

bool UFireStrikesTestSubsystem::GetIsInsideAnyFire(const FVector& Location) const
{
	TArray<UNiagaraComponent*> ActiveVfx;
	GatherActiveGroundFireVfx(ActiveVfx);
	for (const UNiagaraComponent* Vfx : ActiveVfx)
	{
		if (FVector::Dist2D(Vfx->GetComponentLocation(), Location) <= M_FireRadiusCm)
		{
			return true;
		}
	}
	return false;
}

float UFireStrikesTestSubsystem::GetUnitHealth(const AActor* Unit) const
{
	const UHealthComponent* Health = IsValid(Unit) ? Unit->FindComponentByClass<UHealthComponent>() : nullptr;
	return IsValid(Health) ? Health->GetCurrentHealth() : 0.f;
}

float UFireStrikesTestSubsystem::GetUnitMaxHealth(const AActor* Unit) const
{
	const UHealthComponent* Health = IsValid(Unit) ? Unit->FindComponentByClass<UHealthComponent>() : nullptr;
	return IsValid(Health) ? Health->GetMaxHealth() : 0.f;
}

float UFireStrikesTestSubsystem::GetSquadHealth(ASquadController* Squad) const
{
	if (not IsValid(Squad))
	{
		return 0.f;
	}
	float TotalHealth = 0.f;
	const TArray<ASquadUnit*> Soldiers = Squad->GetSquadUnitsChecked();
	for (const ASquadUnit* Soldier : Soldiers)
	{
		TotalHealth += GetUnitHealth(Soldier);
	}
	return TotalHealth;
}

int32 UFireStrikesTestSubsystem::CountLivingStrikeAircraft() const
{
	int32 Living = 0;
	for (const TWeakObjectPtr<AActor>& Aircraft : M_StrikeAircraft)
	{
		Living += Aircraft.IsValid() && not Aircraft->IsActorBeingDestroyed() ? 1 : 0;
	}
	return Living;
}

int32 UFireStrikesTestSubsystem::CountCanisters() const
{
	int32 Count = 0;
	for (TActorIterator<AIncendiaryCanister> It(GetWorld()); It; ++It)
	{
		Count += It->IsHidden() ? 0 : 1;
	}
	return Count;
}

void UFireStrikesTestSubsystem::UpdateCamera() const
{
	using namespace FireStrikesTestPrivate;
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	const FRotator ViewRotation(CameraPitch, CameraYaw, 0.f);
	M_Camera->SetActorLocationAndRotation(M_Target - ViewRotation.Vector() * CameraDistance, ViewRotation);
	// The game's own camera takes the view back; it is handed to the test camera again every frame.
	Controller->SetViewTarget(M_Camera);
}

void UFireStrikesTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("FireStrikesTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
	UE_LOG(LogRTSFireStrikesTest, Display, TEXT("RTS_FIRE_TEST SCREENSHOT %s at=%.1fs"), *FileName, GetRunSeconds());
}

#else

bool UFireStrikesTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UFireStrikesTestSubsystem::Tick(float DeltaTime) {}
TStatId UFireStrikesTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFireStrikesTestSubsystem, STATGROUP_Tickables);
}

#endif
