// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SFH18SquadTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityTier.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/CargoParachuteDescentComponent.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/ParachuteJumperProxy.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSSFH18SquadTest, Log, All);

namespace SFH18SquadTestPrivate
{
	constexpr int32 TestPlayer = 1;
	constexpr float StartDelaySeconds = 3.f;
	constexpr float SpawnTimeoutSeconds = 60.f;
	// After Ready_Deployed: the operators walk to their posts and start the crew loop.
	constexpr float CrewSettleSeconds = 8.f;
	constexpr float FireTimeoutSeconds = 60.f;
	constexpr float FireTargetRangeFraction = 0.55f;
	constexpr float MinFireTargetDistanceCm = 1500.f;
	constexpr float ImpactShotDelaySeconds = 3.f;
	constexpr float DropTimeoutSeconds = 150.f;
	constexpr float DropSettleSeconds = 12.f;
	constexpr float ScreenshotFlushSeconds = 1.5f;
	constexpr float CameraTraceLength = 500000.f;
	constexpr float MidDescentMinAlpha = 0.35f;
	constexpr float MidDescentMaxAlpha = 0.65f;
	constexpr float MinGunUpZ = 0.9f;
	constexpr float MaxGroundGapCm = 40.f;
	constexpr float MaxOperatorFromPostCm = 150.f;
	constexpr int32 ExpectedDroppedGuns = 2;
	const TCHAR* const ExpectedGunMeshName = TEXT("SK_Ger_SFH18Anim");
	const TCHAR* const ScreenshotFolder = TEXT("SFH18SquadTest");

	FString GetGunMeshName(const ATeamWeapon& Gun)
	{
		const USkeletalMeshComponent* Mesh = Gun.FindComponentByClass<USkeletalMeshComponent>();
		const USkeletalMesh* Asset = IsValid(Mesh) ? Mesh->GetSkeletalMeshAsset() : nullptr;
		return IsValid(Asset) ? Asset->GetName() : TEXT("none");
	}

	UWeaponState* GetMainWeapon(ATeamWeapon& Gun)
	{
		TArray<UWeaponState*> Weapons = Gun.GetWeapons();
		return Weapons.Num() > 0 && IsValid(Weapons[0]) ? Weapons[0] : nullptr;
	}
}

bool USFH18SquadTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("SFH18SquadValidate"));
}

TStatId USFH18SquadTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USFH18SquadTestSubsystem, STATGROUP_Tickables);
}

void USFH18SquadTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case ESFH18SquadTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case ESFH18SquadTestPhase::WaitingForSpawnedSquad:
		TickWaitingForSpawnedSquad();
		return;
	case ESFH18SquadTestPhase::SettlingSpawnedSquad:
		TickSettlingSpawnedSquad();
		return;
	case ESFH18SquadTestPhase::Firing:
		TickFiring();
		return;
	case ESFH18SquadTestPhase::AfterFire:
		TickAfterFire();
		return;
	case ESFH18SquadTestPhase::StartingDrop:
		StartDrop();
		return;
	case ESFH18SquadTestPhase::Dropping:
		TickDropping();
		return;
	case ESFH18SquadTestPhase::SettlingDrop:
		TickSettlingDrop();
		return;
	case ESFH18SquadTestPhase::CheckingDrop:
		CheckDrop();
		return;
	case ESFH18SquadTestPhase::FinalShot:
		UpdateCamera();
		if (GetNow() - M_PhaseStartSeconds >= SFH18SquadTestPrivate::ScreenshotFlushSeconds)
		{
			Finish();
		}
		return;
	case ESFH18SquadTestPhase::Finished:
		return;
	}
}

void USFH18SquadTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSSFH18SquadTest, Display, TEXT("RTS_SFH18_TEST CHECK PASS %s.%s %s"), *M_StepLabel, *Description,
		       *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSSFH18SquadTest, Error, TEXT("RTS_SFH18_TEST CHECK FAIL %s.%s %s"), *M_StepLabel, *Description,
	       *Details);
}

float USFH18SquadTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void USFH18SquadTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSSFH18SquadTest, Display, TEXT("RTS_SFH18_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = ESFH18SquadTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void USFH18SquadTestSubsystem::TickWaitingForStart()
{
	using namespace SFH18SquadTestPrivate;
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
	if (not FindAnchor(M_Anchor))
	{
		Check(false, TEXT("found_ground_anchor"));
		Finish();
		return;
	}
	if (UWorld* World = GetWorld())
	{
		M_Camera = World->SpawnActor<ACameraActor>();
	}
	// Face the map centre so the fire target lies on the landscape.
	M_SpawnYaw = FVector(-M_Anchor.X, -M_Anchor.Y, 0.f).GetSafeNormal().Rotation().Yaw;
	UE_LOG(LogRTSSFH18SquadTest, Display, TEXT("RTS_SFH18_TEST START anchor=%s yaw=%.0f"),
	       *M_Anchor.ToCompactString(), M_SpawnYaw);
	StartSpawn();
}

bool USFH18SquadTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace SFH18SquadTestPrivate;
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("SFH18AnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("SFH18AnchorY="), AnchorY))
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

void USFH18SquadTestSubsystem::StartSpawn()
{
	M_StepLabel = TEXT("SpawnedSquad");
	M_Phase = ESFH18SquadTestPhase::WaitingForSpawnedSquad;
	M_PhaseStartSeconds = GetNow();
	ARTSAsyncSpawner* Spawner = FRTS_Statics::GetAsyncSpawner(this);
	Check(IsValid(Spawner), TEXT("async_spawner_found"));
	if (not IsValid(Spawner))
	{
		Finish();
		return;
	}
	TWeakObjectPtr<USFH18SquadTestSubsystem> WeakThis(this);
	const bool bRequested = Spawner->AsyncSpawnOptionAtLocation(
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Ger_SFH18_150mm)),
		M_Anchor,
		this,
		0,
		[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnSquadSpawned(SpawnedActor);
			}
		},
		FRotator(0.f, M_SpawnYaw, 0.f));
	Check(bRequested, TEXT("spawner_accepted_sfh18_option"));
	SetCamera(M_Anchor + FVector(0.f, 0.f, 120.f), 1700.f, -22.f, M_SpawnYaw + 60.f);
	if (not bRequested)
	{
		Finish();
	}
}

void USFH18SquadTestSubsystem::OnSquadSpawned(AActor* SpawnedActor)
{
	M_SpawnedSquad = Cast<ASquadController>(SpawnedActor);
	UE_LOG(LogRTSSFH18SquadTest, Display, TEXT("RTS_SFH18_TEST SPAWNED %s"),
	       IsValid(SpawnedActor) ? *SpawnedActor->GetClass()->GetName() : TEXT("none"));
}

void USFH18SquadTestSubsystem::TickWaitingForSpawnedSquad()
{
	using namespace SFH18SquadTestPrivate;
	UpdateCamera();
	ATeamWeaponController* Squad = Cast<ATeamWeaponController>(M_SpawnedSquad.Get());
	ATeamWeapon* Gun = IsValid(Squad) ? Squad->GetControlledTeamWeaponNoReport() : nullptr;
	const bool bDeployed = IsValid(Gun) && Squad->GetTeamWeaponState() == ETeamWeaponState::Ready_Deployed;
	if (not bDeployed)
	{
		if (GetNow() - M_PhaseStartSeconds >= SpawnTimeoutSeconds)
		{
			Check(IsValid(M_SpawnedSquad.Get()), TEXT("squad_spawned"));
			Check(IsValid(Squad), TEXT("squad_is_team_weapon_controller"));
			Check(IsValid(Gun), TEXT("gun_spawned"));
			Check(false, TEXT("gun_deployed"), IsValid(Squad) ? Squad->GetOrderDebugDescription() : TEXT("no squad"));
			M_Phase = ESFH18SquadTestPhase::StartingDrop;
		}
		return;
	}
	Check(true, TEXT("gun_deployed_after_spawn"), FString::Printf(TEXT("after=%.1fs"), GetNow() - M_PhaseStartSeconds));
	SetCamera(Gun->GetActorLocation() + FVector(0.f, 0.f, 120.f), 1700.f, -22.f, Gun->GetActorRotation().Yaw + 60.f);
	M_PhaseStartSeconds = GetNow();
	M_Phase = ESFH18SquadTestPhase::SettlingSpawnedSquad;
}

void USFH18SquadTestSubsystem::TickSettlingSpawnedSquad()
{
	using namespace SFH18SquadTestPrivate;
	UpdateCamera();
	if (GetNow() - M_PhaseStartSeconds < CrewSettleSeconds)
	{
		return;
	}
	ATeamWeaponController* Squad = Cast<ATeamWeaponController>(M_SpawnedSquad.Get());
	ATeamWeapon* Gun = IsValid(Squad) ? Squad->GetControlledTeamWeaponNoReport() : nullptr;
	if (not IsValid(Gun))
	{
		Check(false, TEXT("gun_still_valid"));
		M_Phase = ESFH18SquadTestPhase::StartingDrop;
		return;
	}
	CheckSquadIdentity(*Squad, TEXT("spawned"));
	CheckSquadCrewed(*Squad, TEXT("spawned"));
	RequestScreenshot(TEXT("spawned_deployed_rear"));
	StartFiring(*Squad, *Gun);
}

void USFH18SquadTestSubsystem::StartFiring(ATeamWeaponController& Squad, ATeamWeapon& Gun)
{
	using namespace SFH18SquadTestPrivate;
	M_StepLabel = TEXT("Fire");
	bM_SawFire = false;
	bM_TookImpactShot = false;
	const float Range = Squad.GetTeamWeaponRange();
	const float TargetDistance = FMath::Max(Range * FireTargetRangeFraction, MinFireTargetDistanceCm);
	const FVector Direction = Gun.GetActorForwardVector().GetSafeNormal2D();
	const FVector RawTarget = Gun.GetActorLocation() + Direction * TargetDistance;
	FVector Target = RawTarget;
	const bool bTargetOnGround = GlobalAbilityHelpers::TryFindGroundLocation(this, RawTarget, Target);
	Check(bTargetOnGround, TEXT("fire_target_on_ground"),
	      FString::Printf(TEXT("range=%.0f distance=%.0f target=%s"), Range, TargetDistance,
	                      *Target.ToCompactString()));
	M_FireTarget = Target;
	const UWeaponState* Weapon = GetMainWeapon(Gun);
	UE_LOG(LogRTSSFH18SquadTest, Display, TEXT("RTS_SFH18_TEST FIRE_ORDER target=%s loaded=%d"),
	       *Target.ToCompactString(), IsValid(Weapon) && Weapon->IsWeaponFullyLoaded() ? 1 : 0);
	const ECommandQueueError Error = Squad.AttackGround(Target, true);
	Check(Error == ECommandQueueError::NoError, TEXT("attack_ground_accepted"),
	      FString::Printf(TEXT("error=%d"), static_cast<int32>(Error)));
	SetCamera(Gun.GetActorLocation() + FVector(0.f, 0.f, 150.f), 1900.f, -14.f, Gun.GetActorRotation().Yaw + 90.f);
	M_PhaseStartSeconds = GetNow();
	M_Phase = ESFH18SquadTestPhase::Firing;
}

void USFH18SquadTestSubsystem::TickFiring()
{
	using namespace SFH18SquadTestPrivate;
	UpdateCamera();
	ATeamWeaponController* Squad = Cast<ATeamWeaponController>(M_SpawnedSquad.Get());
	ATeamWeapon* Gun = IsValid(Squad) ? Squad->GetControlledTeamWeaponNoReport() : nullptr;
	UWeaponState* Weapon = IsValid(Gun) ? GetMainWeapon(*Gun) : nullptr;
	// One round in the magazine: the shot starts the reload, so a gun that is no longer fully loaded has fired.
	if (IsValid(Weapon) && not Weapon->IsWeaponFullyLoaded())
	{
		bM_SawFire = true;
		Check(true, TEXT("gun_fired_at_ground_target"),
		      FString::Printf(TEXT("after=%.1fs distance=%.0f reload_progress=%.2f"), GetNow() - M_PhaseStartSeconds,
		                      FVector::Dist2D(Gun->GetActorLocation(), M_FireTarget), Weapon->GetReloadProgress()));
		RequestScreenshot(TEXT("spawned_firing_side"));
		M_PhaseStartSeconds = GetNow();
		M_Phase = ESFH18SquadTestPhase::AfterFire;
		return;
	}
	if (GetNow() - M_PhaseStartSeconds >= FireTimeoutSeconds)
	{
		Check(false, TEXT("gun_fired_at_ground_target"),
		      IsValid(Squad) ? Squad->GetOrderDebugDescription() : TEXT("no squad"));
		M_PhaseStartSeconds = GetNow();
		M_Phase = ESFH18SquadTestPhase::AfterFire;
	}
}

void USFH18SquadTestSubsystem::TickAfterFire()
{
	using namespace SFH18SquadTestPrivate;
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	if (bM_SawFire && not bM_TookImpactShot && Elapsed >= ScreenshotFlushSeconds)
	{
		// Look past the gun at the target area for the shell's impact.
		SetCamera(M_FireTarget + FVector(0.f, 0.f, 100.f), 3500.f, -20.f, M_SpawnYaw + 30.f);
	}
	UpdateCamera();
	if (bM_SawFire && not bM_TookImpactShot && Elapsed >= ImpactShotDelaySeconds)
	{
		bM_TookImpactShot = true;
		RequestScreenshot(TEXT("spawned_impact"));
		return;
	}
	if (Elapsed < ImpactShotDelaySeconds + ScreenshotFlushSeconds)
	{
		return;
	}
	ATeamWeaponController* Squad = Cast<ATeamWeaponController>(M_SpawnedSquad.Get());
	if (IsValid(Squad))
	{
		Squad->StopCommand();
		Check(IsValid(Squad->GetControlledTeamWeaponNoReport()), TEXT("gun_alive_after_firing"));
	}
	CleanUpSpawnedSquad();
	M_Phase = ESFH18SquadTestPhase::StartingDrop;
}

void USFH18SquadTestSubsystem::CleanUpSpawnedSquad()
{
	// The drop lands on the same spot: remove the spawned squad, its gun and its crew.
	ASquadController* Squad = M_SpawnedSquad.Get();
	if (not IsValid(Squad))
	{
		return;
	}
	TArray<AActor*> ToDestroy;
	if (ATeamWeaponController* TeamWeaponSquad = Cast<ATeamWeaponController>(Squad))
	{
		ToDestroy.Add(TeamWeaponSquad->GetControlledTeamWeaponNoReport());
	}
	for (ASquadUnit* Unit : Squad->GetSquadUnitsChecked())
	{
		ToDestroy.Add(Unit);
	}
	ToDestroy.Add(Squad);
	for (AActor* Actor : ToDestroy)
	{
		if (IsValid(Actor) && not Actor->IsActorBeingDestroyed())
		{
			Actor->Destroy();
		}
	}
	M_SpawnedSquad.Reset();
}

void USFH18SquadTestSubsystem::StartDrop()
{
	M_StepLabel = TEXT("SFH18Drop");
	M_ActorsBeforeDrop.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		M_ActorsBeforeDrop.Add(*It);
	}
	bM_AbilityEnded = false;
	bM_TookDescentShot = false;
	bM_TookLandedShot = false;
	SetCamera(M_Anchor + FVector(0.f, 0.f, 1500.f), 7000.f, -12.f, 35.f);

	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	M_Ability = IsValid(AbilitySet)
		            ? AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(EGlobalAbility::GA_GerSFH18Drop)
		            : nullptr;
	Check(IsValid(Manager) && IsValid(M_Ability), TEXT("template_exists"),
	      IsValid(M_Ability) ? M_Ability->GetClass()->GetName() : TEXT("none"));
	if (not IsValid(Manager) || not IsValid(M_Ability))
	{
		Finish();
		return;
	}
	const TMap<ERTSResourceType, int32>& Costs = M_Ability->GetAbilityCosts().Costs.ResourceCosts;
	const int32* Radixite = Costs.Find(ERTSResourceType::Resource_Radixite);
	Check(Costs.Num() == 1 && Radixite != nullptr && *Radixite > 0, TEXT("template_costs_radixite_only"),
	      FString::Printf(TEXT("resources=%d radixite=%d cooldown=%d tier=%s"), Costs.Num(),
	                      Radixite != nullptr ? *Radixite : -1, M_Ability->GetAbilityCosts().CoolDownTime,
	                      *UEnum::GetValueAsString(M_Ability->GetAbilityTier())));
	Check(M_Ability->GetAbilityTier() == EGlobalAbilityTier::TierII, TEXT("template_tier_ii"));
	M_Ability->InitGlobalAbility(SFH18SquadTestPrivate::TestPlayer, Manager, Controller);
	M_Ability->OnGlobalAbilityEnded.AddUObject(this, &USFH18SquadTestSubsystem::OnAbilityEnded);
	M_Ability->ExecuteAbilityAtLocation(M_Anchor);
	M_PhaseStartSeconds = GetNow();
	M_Phase = ESFH18SquadTestPhase::Dropping;
	UE_LOG(LogRTSSFH18SquadTest, Display, TEXT("RTS_SFH18_TEST DROP executed class=%s"),
	       *M_Ability->GetClass()->GetName());
}

void USFH18SquadTestSubsystem::OnAbilityEnded(UGlobalAbility* Ability)
{
	if (Ability == M_Ability)
	{
		bM_AbilityEnded = true;
	}
}

void USFH18SquadTestSubsystem::TickDropping()
{
	using namespace SFH18SquadTestPrivate;
	FVector LoadLocation;
	if (not bM_TookDescentShot && TryFindMidDescentGun(LoadLocation))
	{
		bM_TookDescentShot = true;
		SetCamera(LoadLocation, 4000.f, -6.f, 35.f);
		RequestScreenshot(TEXT("drop_descent"));
	}
	UpdateCamera();
	const bool bTimedOut = GetNow() - M_PhaseStartSeconds >= DropTimeoutSeconds;
	if (bM_AbilityEnded || bTimedOut)
	{
		Check(bM_AbilityEnded, TEXT("ability_finished"),
		      FString::Printf(TEXT("after=%.1fs"), GetNow() - M_PhaseStartSeconds));
		Check(bM_TookDescentShot, TEXT("seen_gun_mid_descent"));
		M_PhaseStartSeconds = GetNow();
		SetCamera(M_Anchor + FVector(0.f, 0.f, 150.f), 2800.f, -32.f, 35.f);
		M_Phase = ESFH18SquadTestPhase::SettlingDrop;
	}
}

void USFH18SquadTestSubsystem::TickSettlingDrop()
{
	using namespace SFH18SquadTestPrivate;
	UpdateCamera();
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	if (not bM_TookLandedShot && Elapsed >= DropSettleSeconds)
	{
		bM_TookLandedShot = true;
		M_LandedShotSeconds = GetNow();
		RequestScreenshot(TEXT("drop_landed"));
		return;
	}
	if (bM_TookLandedShot && GetNow() - M_LandedShotSeconds >= ScreenshotFlushSeconds)
	{
		M_Phase = ESFH18SquadTestPhase::CheckingDrop;
	}
}

void USFH18SquadTestSubsystem::CheckDrop()
{
	using namespace SFH18SquadTestPrivate;
	const TArray<AActor*> Proxies = GetNewActorsOfClass(AParachuteJumperProxy::StaticClass());
	Check(Proxies.IsEmpty(), TEXT("no_jumper_proxy_left"), FString::Printf(TEXT("proxies=%d"), Proxies.Num()));
	const TArray<AActor*> Squads = GetNewActorsOfClass(ATeamWeaponController::StaticClass());
	Check(Squads.Num() == ExpectedDroppedGuns, TEXT("gun_squads_dropped"),
	      FString::Printf(TEXT("squads=%d expected=%d"), Squads.Num(), ExpectedDroppedGuns));
	TArray<FVector> GunLocations;
	for (AActor* Actor : Squads)
	{
		ATeamWeaponController* Squad = Cast<ATeamWeaponController>(Actor);
		Check(not Squad->GetIsInParachuteDrop(), TEXT("squad_released"), Squad->GetName());
		CheckSquadIdentity(*Squad, TEXT("dropped"));
		ATeamWeapon* Gun = Squad->GetControlledTeamWeaponNoReport();
		if (not IsValid(Gun))
		{
			continue;
		}
		GunLocations.Add(Gun->GetActorLocation());
		Check(Gun->FindComponentByClass<UCargoParachuteDescentComponent>() == nullptr, TEXT("gun_chute_removed"),
		      Gun->GetName());
		CheckSquadCrewed(*Squad, TEXT("dropped"));
	}
	if (GunLocations.Num() == 2)
	{
		const float Spacing = FVector::Dist2D(GunLocations[0], GunLocations[1]);
		Check(Spacing >= 800.f, TEXT("guns_apart"), FString::Printf(TEXT("spacing=%.0f"), Spacing));
	}
	// A close look at one dropped gun and its crew.
	if (GunLocations.Num() > 0)
	{
		SetCamera(GunLocations[0] + FVector(0.f, 0.f, 120.f), 1800.f, -24.f, M_CameraYaw);
		RequestScreenshot(TEXT("drop_gun_close"));
	}
	M_Ability = nullptr;
	M_PhaseStartSeconds = GetNow();
	M_Phase = ESFH18SquadTestPhase::FinalShot;
}

void USFH18SquadTestSubsystem::CheckSquadIdentity(ATeamWeaponController& Squad, const FString& Label)
{
	using namespace SFH18SquadTestPrivate;
	const FString SquadName = Squad.GetName();
	const URTSComponent* RTSComponent = Squad.GetRTSComponent();
	Check(IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == TestPlayer, Label + TEXT("_owned_by_player"),
	      FString::Printf(TEXT("%s owner=%d"), *SquadName, IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : -1));
	Check(IsValid(RTSComponent) && RTSComponent->GetSubtypeAsSquadSubtype() == ESquadSubtype::Squad_Ger_SFH18_150mm,
	      Label + TEXT("_squad_subtype_sfh18"),
	      FString::Printf(TEXT("%s class=%s subtype=%s"), *SquadName, *Squad.GetClass()->GetName(),
	                      IsValid(RTSComponent)
		                      ? *UEnum::GetValueAsString(RTSComponent->GetSubtypeAsSquadSubtype())
		                      : TEXT("none")));
	ATeamWeapon* Gun = Squad.GetControlledTeamWeaponNoReport();
	Check(IsValid(Gun), Label + TEXT("_gun_exists"), SquadName);
	if (not IsValid(Gun))
	{
		return;
	}
	const FString MeshName = GetGunMeshName(*Gun);
	Check(MeshName == ExpectedGunMeshName, Label + TEXT("_gun_is_sfh18_mesh"),
	      FString::Printf(TEXT("class=%s mesh=%s"), *Gun->GetClass()->GetName(), *MeshName));
	const UWeaponState* Weapon = GetMainWeapon(*Gun);
	const EWeaponName WeaponName = IsValid(Weapon)
		                               ? Weapon->GetWeaponDataAdjustedForShellType().WeaponName
		                               : static_cast<EWeaponName>(0);
	Check(IsValid(Weapon) && WeaponName == EWeaponName::sFH18_150MM_TW, Label + TEXT("_weapon_is_sfh18"),
	      FString::Printf(TEXT("weapon=%s calibre=%d range=%.0f"), *UEnum::GetValueAsString(WeaponName),
	                      IsValid(Weapon) ? Weapon->GetWeaponDataAdjustedForShellType().WeaponCalibre : 0,
	                      Squad.GetTeamWeaponRange()));
	const TArray<ASquadUnit*> Units = Squad.GetSquadUnitsChecked();
	UE_LOG(LogRTSSFH18SquadTest, Display,
	       TEXT("RTS_SFH18_TEST CONFIG %s squad=%s gun=%s soldiers=%d operators_required=%d"),
	       *Label, *Squad.GetClass()->GetName(), *Gun->GetClass()->GetName(), Units.Num(), Gun->GetRequiredOperators());
}

void USFH18SquadTestSubsystem::CheckSquadCrewed(ATeamWeaponController& Squad, const FString& Label)
{
	using namespace SFH18SquadTestPrivate;
	ATeamWeapon* Gun = Squad.GetControlledTeamWeaponNoReport();
	if (not IsValid(Gun))
	{
		return;
	}
	Check(not Gun->IsHidden() && Gun->GetActorEnableCollision(), Label + TEXT("_gun_visible_with_collision"),
	      Gun->GetName());
	const float UpZ = Gun->GetActorUpVector().Z;
	Check(UpZ >= MinGunUpZ, Label + TEXT("_gun_upright"), FString::Printf(TEXT("up_z=%.3f"), UpZ));
	float GroundZ = 0.f;
	const bool bHasGround = Gun->TryGetGroundHeightAt(Gun->GetActorLocation(), GroundZ);
	const float Gap = bHasGround ? Gun->GetActorLocation().Z - GroundZ : 99999.f;
	Check(bHasGround && FMath::Abs(Gap) <= MaxGroundGapCm, Label + TEXT("_gun_on_ground"),
	      FString::Printf(TEXT("gap=%.1f at=%s"), Gap, *Gun->GetActorLocation().ToCompactString()));
	Check(Squad.GetTeamWeaponState() == ETeamWeaponState::Ready_Deployed, Label + TEXT("_gun_deployed"),
	      Squad.GetOrderDebugDescription());

	TArray<ASquadUnit*> Operators;
	Squad.GetCrewOperators(Operators);
	Check(Operators.Num() >= Gun->GetRequiredOperators() && Gun->GetRequiredOperators() > 0,
	      Label + TEXT("_crew_operators_assigned"),
	      FString::Printf(TEXT("operators=%d required=%d"), Operators.Num(), Gun->GetRequiredOperators()));
	int32 OperatorsAtPost = 0;
	int32 OperatorsCrewing = 0;
	for (int32 OperatorIndex = 0; OperatorIndex < Operators.Num(); ++OperatorIndex)
	{
		const ASquadUnit* Operator = Operators[OperatorIndex];
		FVector PostLocation;
		const bool bHasPost = Squad.TryGetOperatorCrewPositionLocation(OperatorIndex, PostLocation);
		const float FromPost = bHasPost ? FVector::Dist2D(Operator->GetActorLocation(), PostLocation) : -1.f;
		if (bHasPost && FromPost <= MaxOperatorFromPostCm)
		{
			++OperatorsAtPost;
		}
		const USquadUnitAnimInstance* AnimInstance = Operator->GetAnimBP_SquadUnit();
		const bool bCrewing = IsValid(AnimInstance) && AnimInstance->GetIsTeamWeaponCrewAnimationActive();
		OperatorsCrewing += bCrewing ? 1 : 0;
		// Post in the gun's frame, to place the crew positions against the mesh.
		const FVector LocalPost = bHasPost
			                          ? Gun->GetActorTransform().InverseTransformPosition(PostLocation)
			                          : FVector::ZeroVector;
		UE_LOG(LogRTSSFH18SquadTest, Display,
		       TEXT("RTS_SFH18_TEST OPERATOR %s index=%d from_post=%.0f local_post=%s crewing=%d"),
		       *Label, OperatorIndex, FromPost, *LocalPost.ToCompactString(), bCrewing ? 1 : 0);
	}
	Check(OperatorsAtPost == Operators.Num(), Label + TEXT("_operators_at_posts"),
	      FString::Printf(TEXT("%d of %d"), OperatorsAtPost, Operators.Num()));
	Check(OperatorsCrewing == Operators.Num(), Label + TEXT("_operators_playing_crew_animation"),
	      FString::Printf(TEXT("%d of %d"), OperatorsCrewing, Operators.Num()));
	int32 VisibleSelectable = 0;
	const TArray<ASquadUnit*> Units = Squad.GetSquadUnitsChecked();
	for (const ASquadUnit* Unit : Units)
	{
		const USelectionComponent* Selection = Unit->GetSelectionComponent();
		if (not Unit->IsHidden() && IsValid(Selection) && Selection->GetCanBeSelected())
		{
			++VisibleSelectable;
		}
	}
	Check(not Units.IsEmpty() && VisibleSelectable == Units.Num(), Label + TEXT("_crew_visible_selectable"),
	      FString::Printf(TEXT("%d of %d"), VisibleSelectable, Units.Num()));
}

TArray<AActor*> USFH18SquadTestSubsystem::GetNewActorsOfClass(UClass* ActorClass) const
{
	TArray<AActor*> Actors;
	for (TActorIterator<AActor> It(GetWorld(), ActorClass); It; ++It)
	{
		if (not M_ActorsBeforeDrop.Contains(*It) && not It->IsActorBeingDestroyed())
		{
			Actors.Add(*It);
		}
	}
	return Actors;
}

bool USFH18SquadTestSubsystem::TryFindMidDescentGun(FVector& OutLoadLocation) const
{
	using namespace SFH18SquadTestPrivate;
	for (TActorIterator<ATeamWeapon> It(GetWorld()); It; ++It)
	{
		const UCargoParachuteDescentComponent* Descent = It->FindComponentByClass<UCargoParachuteDescentComponent>();
		if (IsValid(Descent) && Descent->GetIsDescending() && Descent->GetDescentAlpha() >= MidDescentMinAlpha &&
			Descent->GetDescentAlpha() <= MidDescentMaxAlpha)
		{
			OutLoadLocation = It->GetActorLocation();
			UE_LOG(LogRTSSFH18SquadTest, Display, TEXT("RTS_SFH18_TEST DESCENT gun=%s mesh=%s canopies=%d alpha=%.2f"),
			       *It->GetName(), *GetGunMeshName(**It), Descent->GetCanopyCount(), Descent->GetDescentAlpha());
			return true;
		}
	}
	return false;
}

void USFH18SquadTestSubsystem::SetCamera(const FVector& Focus, const float Distance, const float Pitch, const float Yaw)
{
	M_CameraFocus = Focus;
	M_CameraDistance = Distance;
	M_CameraPitch = Pitch;
	M_CameraYaw = Yaw;
	UpdateCamera();
}

void USFH18SquadTestSubsystem::UpdateCamera() const
{
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	const FRotator ViewRotation(M_CameraPitch, M_CameraYaw, 0.f);
	M_Camera->SetActorLocationAndRotation(M_CameraFocus - ViewRotation.Vector() * M_CameraDistance, ViewRotation);
	// The game's own camera takes the view back; it is handed to the test camera again every frame.
	Controller->SetViewTarget(M_Camera);
}

void USFH18SquadTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / SFH18SquadTestPrivate::ScreenshotFolder /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
	UE_LOG(LogRTSSFH18SquadTest, Display, TEXT("RTS_SFH18_TEST SCREENSHOT %s"), *FileName);
}

#else

bool USFH18SquadTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void USFH18SquadTestSubsystem::Tick(float DeltaTime) {}
TStatId USFH18SquadTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USFH18SquadTestSubsystem, STATGROUP_Tickables);
}

#endif
