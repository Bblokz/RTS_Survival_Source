// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ParachutePayloadTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/CargoParachuteDescentComponent.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/ParachuteDescentComponent.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/ParachuteJumperProxy.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeaponController.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSParachutePayloadTest, Log, All);

namespace ParachutePayloadTestPrivate
{
	constexpr int32 TestPlayer = 1;
	constexpr float StartDelaySeconds = 3.f;
	constexpr float StepTimeoutSeconds = 150.f;
	// After the last load landed: the gun crews finish deploying and take their posts.
	constexpr float SettleSeconds = 10.f;
	constexpr float ScreenshotFlushSeconds = 1.5f;
	constexpr float CameraTraceLength = 500000.f;
	constexpr float MidDescentMinAlpha = 0.35f;
	constexpr float MidDescentMaxAlpha = 0.65f;
	// Guns tilt with the slope up to 20 degrees; buildings stand level.
	constexpr float MinGunUpZ = 0.9f;
	constexpr float MinBuildingUpZ = 0.995f;
	constexpr float MaxGroundGapCm = 40.f;
	constexpr float MaxOperatorFromPostCm = 150.f;
	constexpr float CameraYawFromX = 35.f;
}

bool UParachutePayloadTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("ParachutePayloadValidate"));
}

TStatId UParachutePayloadTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UParachutePayloadTestSubsystem, STATGROUP_Tickables);
}

void UParachutePayloadTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case EParachutePayloadTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EParachutePayloadTestPhase::StartingStep:
		StartStep();
		return;
	case EParachutePayloadTestPhase::Dropping:
		TickDropping();
		return;
	case EParachutePayloadTestPhase::Settling:
		TickSettling();
		return;
	case EParachutePayloadTestPhase::Checking:
		CheckStep();
		return;
	case EParachutePayloadTestPhase::Finished:
		return;
	}
}

void UParachutePayloadTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	const FString StepLabel = M_Steps.IsValidIndex(M_StepIndex) ? M_Steps[M_StepIndex].Label : TEXT("setup");
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSParachutePayloadTest, Display, TEXT("RTS_PARACHUTE_PAYLOAD_TEST CHECK PASS %s.%s %s"),
		       *StepLabel, *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSParachutePayloadTest, Error, TEXT("RTS_PARACHUTE_PAYLOAD_TEST CHECK FAIL %s.%s %s"),
	       *StepLabel, *Description, *Details);
}

float UParachutePayloadTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void UParachutePayloadTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSParachutePayloadTest, Display, TEXT("RTS_PARACHUTE_PAYLOAD_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = EParachutePayloadTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UParachutePayloadTestSubsystem::BuildSteps()
{
	M_Steps = {
		{EGlobalAbility::GA_GerParatroopers, TEXT("Paratroopers"), EParachutePayloadKind::Infantry, 1},
		{EGlobalAbility::GA_GerFlak88Drop, TEXT("Flak88Drop"), EParachutePayloadKind::TeamWeapon, 1},
		{EGlobalAbility::GA_GerSFH18Drop, TEXT("SFH18Drop"), EParachutePayloadKind::TeamWeapon, 2},
		{EGlobalAbility::GA_GerPanzerschreckPitDrop, TEXT("PanzerschreckPitDrop"), EParachutePayloadKind::Building, 2},
		{EGlobalAbility::GA_GerPakPitDrop, TEXT("PakPitDrop"), EParachutePayloadKind::Building, 1},
		{EGlobalAbility::GA_GerFieldBunkerDrop, TEXT("FieldBunkerDrop"), EParachutePayloadKind::Building, 1},
	};
	FString Only;
	if (FParse::Value(FCommandLine::Get(), TEXT("ParachutePayloadOnly="), Only) && not Only.IsEmpty())
	{
		M_Steps.RemoveAll([&Only](const FPayloadTestStep& Step)
		{
			return not Step.Label.Contains(Only);
		});
	}
}

void UParachutePayloadTestSubsystem::TickWaitingForStart()
{
	using namespace ParachutePayloadTestPrivate;
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
	BuildSteps();
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
	UE_LOG(LogRTSParachutePayloadTest, Display, TEXT("RTS_PARACHUTE_PAYLOAD_TEST START anchor=%s steps=%d"),
	       *M_Anchor.ToCompactString(), M_Steps.Num());
	M_StepIndex = 0;
	M_Phase = M_Steps.IsEmpty() ? EParachutePayloadTestPhase::Finished : EParachutePayloadTestPhase::StartingStep;
	if (M_Steps.IsEmpty())
	{
		Finish();
	}
}

bool UParachutePayloadTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace ParachutePayloadTestPrivate;
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("ParachutePayloadAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("ParachutePayloadAnchorY="), AnchorY))
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

void UParachutePayloadTestSubsystem::StartStep()
{
	const FPayloadTestStep& Step = M_Steps[M_StepIndex];
	M_ActorsBeforeStep.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		M_ActorsBeforeStep.Add(*It);
	}
	bM_AbilityEnded = false;
	bM_TookDescentShot = false;
	bM_TookLandedShot = false;
	M_CameraFocus = M_Anchor + FVector(0.f, 0.f, 1500.f);
	M_CameraDistance = 7000.f;
	M_CameraPitch = -12.f;

	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	M_Ability = IsValid(AbilitySet) ? AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(Step.AbilityType) : nullptr;
	Check(IsValid(Manager) && IsValid(M_Ability), TEXT("template_exists"),
	      IsValid(M_Ability) ? M_Ability->GetClass()->GetName() : TEXT("none"));
	if (not IsValid(Manager) || not IsValid(M_Ability))
	{
		M_Phase = EParachutePayloadTestPhase::Checking;
		return;
	}
	M_Ability->InitGlobalAbility(ParachutePayloadTestPrivate::TestPlayer, Manager, Controller);
	M_Ability->OnGlobalAbilityEnded.AddUObject(this, &UParachutePayloadTestSubsystem::OnAbilityEnded);
	M_Ability->ExecuteAbilityAtLocation(M_Anchor);
	M_PhaseStartSeconds = GetNow();
	M_Phase = EParachutePayloadTestPhase::Dropping;
	UE_LOG(LogRTSParachutePayloadTest, Display, TEXT("RTS_PARACHUTE_PAYLOAD_TEST STEP %s executed class=%s"),
	       *Step.Label, *M_Ability->GetClass()->GetName());
}

void UParachutePayloadTestSubsystem::OnAbilityEnded(UGlobalAbility* Ability)
{
	if (Ability == M_Ability)
	{
		bM_AbilityEnded = true;
	}
}

void UParachutePayloadTestSubsystem::TickDropping()
{
	using namespace ParachutePayloadTestPrivate;
	FVector LoadLocation;
	if (not bM_TookDescentShot && TryFindMidDescentLoad(LoadLocation))
	{
		bM_TookDescentShot = true;
		M_CameraFocus = LoadLocation;
		M_CameraDistance = 5500.f;
		M_CameraPitch = -8.f;
		UpdateCamera();
		RequestScreenshot(M_Steps[M_StepIndex].Label + TEXT("_descent"));
		UE_LOG(LogRTSParachutePayloadTest, Display, TEXT("RTS_PARACHUTE_PAYLOAD_TEST DESCENT %s load_at=%s"),
		       *M_Steps[M_StepIndex].Label, *LoadLocation.ToCompactString());
	}
	UpdateCamera();
	const bool bTimedOut = GetNow() - M_PhaseStartSeconds >= StepTimeoutSeconds;
	if (bM_AbilityEnded || bTimedOut)
	{
		Check(bM_AbilityEnded, TEXT("ability_finished"),
		      FString::Printf(TEXT("after=%.1fs"), GetNow() - M_PhaseStartSeconds));
		Check(bM_TookDescentShot, TEXT("seen_mid_descent"));
		M_PhaseStartSeconds = GetNow();
		M_CameraFocus = M_Anchor + FVector(0.f, 0.f, 150.f);
		M_CameraDistance = M_Steps[M_StepIndex].Kind == EParachutePayloadKind::Building ? 4200.f : 3200.f;
		M_CameraPitch = -38.f;
		M_Phase = EParachutePayloadTestPhase::Settling;
	}
}

void UParachutePayloadTestSubsystem::TickSettling()
{
	using namespace ParachutePayloadTestPrivate;
	UpdateCamera();
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	if (not bM_TookLandedShot && Elapsed >= SettleSeconds)
	{
		bM_TookLandedShot = true;
		M_LandedShotSeconds = GetNow();
		RequestScreenshot(M_Steps[M_StepIndex].Label + TEXT("_landed"));
		return;
	}
	if (bM_TookLandedShot && GetNow() - M_LandedShotSeconds >= ScreenshotFlushSeconds)
	{
		M_Phase = EParachutePayloadTestPhase::Checking;
	}
}

void UParachutePayloadTestSubsystem::CheckStep()
{
	const FPayloadTestStep& Step = M_Steps[M_StepIndex];
	TArray<AActor*> Proxies = GetNewActorsOfClass(AParachuteJumperProxy::StaticClass());
	Check(Proxies.IsEmpty(), TEXT("no_jumper_proxy_left"), FString::Printf(TEXT("proxies=%d"), Proxies.Num()));
	switch (Step.Kind)
	{
	case EParachutePayloadKind::Infantry:
		CheckInfantryStep(Step);
		break;
	case EParachutePayloadKind::TeamWeapon:
		CheckTeamWeaponStep(Step);
		break;
	case EParachutePayloadKind::Building:
		CheckBuildingStep(Step);
		break;
	}
	CleanUpStep();
	++M_StepIndex;
	if (not M_Steps.IsValidIndex(M_StepIndex))
	{
		Finish();
		return;
	}
	M_Phase = EParachutePayloadTestPhase::StartingStep;
}

void UParachutePayloadTestSubsystem::CheckInfantryStep(const FPayloadTestStep& Step)
{
	using namespace ParachutePayloadTestPrivate;
	TArray<AActor*> Squads = GetNewActorsOfClass(ASquadController::StaticClass());
	Check(Squads.Num() >= Step.ExpectedCount, TEXT("squads_dropped"), FString::Printf(TEXT("squads=%d"), Squads.Num()));
	for (AActor* Actor : Squads)
	{
		ASquadController* Squad = Cast<ASquadController>(Actor);
		Check(not Squad->GetIsInParachuteDrop(), TEXT("squad_released"), Squad->GetName());
		const URTSComponent* RTSComponent = Squad->GetRTSComponent();
		Check(IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == TestPlayer, TEXT("squad_owned_by_player"));
		int32 LandedUnits = 0;
		const TArray<ASquadUnit*> Units = Squad->GetSquadUnitsChecked();
		for (const ASquadUnit* Unit : Units)
		{
			const UCapsuleComponent* Capsule = Unit->GetCapsuleComponent();
			const float FeetGap = GetHeightAboveGround(*Unit) - (IsValid(Capsule) ? Capsule->GetScaledCapsuleHalfHeight() : 0.f);
			const USelectionComponent* Selection = Unit->GetSelectionComponent();
			if (not Unit->IsHidden() && FMath::Abs(FeetGap) <= MaxGroundGapCm && IsValid(Selection) &&
				Selection->GetCanBeSelected())
			{
				++LandedUnits;
			}
		}
		Check(not Units.IsEmpty() && LandedUnits == Units.Num(), TEXT("soldiers_landed_visible_selectable"),
		      FString::Printf(TEXT("%s landed=%d of %d"), *Squad->GetName(), LandedUnits, Units.Num()));
	}
}

void UParachutePayloadTestSubsystem::CheckTeamWeaponStep(const FPayloadTestStep& Step)
{
	using namespace ParachutePayloadTestPrivate;
	TArray<AActor*> Squads = GetNewActorsOfClass(ATeamWeaponController::StaticClass());
	Check(Squads.Num() == Step.ExpectedCount, TEXT("gun_squads_dropped"),
	      FString::Printf(TEXT("squads=%d expected=%d"), Squads.Num(), Step.ExpectedCount));
	TArray<FVector> GunLocations;
	for (AActor* Actor : Squads)
	{
		ATeamWeaponController* Squad = Cast<ATeamWeaponController>(Actor);
		const FString SquadName = Squad->GetName();
		Check(not Squad->GetIsInParachuteDrop(), TEXT("squad_released"), SquadName);
		const URTSComponent* RTSComponent = Squad->GetRTSComponent();
		Check(IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == TestPlayer, TEXT("squad_owned_by_player"),
		      SquadName);
		ATeamWeapon* Gun = Squad->GetControlledTeamWeaponNoReport();
		Check(IsValid(Gun), TEXT("gun_exists"), SquadName);
		if (not IsValid(Gun))
		{
			continue;
		}
		GunLocations.Add(Gun->GetActorLocation());
		Check(not Gun->IsHidden() && Gun->GetActorEnableCollision(), TEXT("gun_visible_with_collision"), Gun->GetName());
		Check(Gun->FindComponentByClass<UCargoParachuteDescentComponent>() == nullptr, TEXT("gun_chute_removed"));
		const float UpZ = Gun->GetActorUpVector().Z;
		Check(UpZ >= MinGunUpZ, TEXT("gun_upright"), FString::Printf(TEXT("up_z=%.3f"), UpZ));
		float GroundZ = 0.f;
		const bool bHasGround = Gun->TryGetGroundHeightAt(Gun->GetActorLocation(), GroundZ);
		const float Gap = bHasGround ? Gun->GetActorLocation().Z - GroundZ : 99999.f;
		Check(bHasGround && FMath::Abs(Gap) <= MaxGroundGapCm, TEXT("gun_on_ground"),
		      FString::Printf(TEXT("gap=%.1f at=%s"), Gap, *Gun->GetActorLocation().ToCompactString()));
		Check(Squad->GetTeamWeaponState() == ETeamWeaponState::Ready_Deployed, TEXT("gun_deployed"),
		      Squad->GetOrderDebugDescription());

		TArray<ASquadUnit*> Operators;
		Squad->GetCrewOperators(Operators);
		Check(Operators.Num() >= Gun->GetRequiredOperators() && Gun->GetRequiredOperators() > 0,
		      TEXT("crew_operators_assigned"),
		      FString::Printf(TEXT("operators=%d required=%d"), Operators.Num(), Gun->GetRequiredOperators()));
		int32 OperatorsAtPost = 0;
		int32 OperatorsCrewing = 0;
		for (int32 OperatorIndex = 0; OperatorIndex < Operators.Num(); ++OperatorIndex)
		{
			const ASquadUnit* Operator = Operators[OperatorIndex];
			FVector PostLocation;
			if (Squad->TryGetOperatorCrewPositionLocation(OperatorIndex, PostLocation) &&
				FVector::Dist2D(Operator->GetActorLocation(), PostLocation) <= MaxOperatorFromPostCm)
			{
				++OperatorsAtPost;
			}
			const USquadUnitAnimInstance* AnimInstance = Operator->GetAnimBP_SquadUnit();
			if (IsValid(AnimInstance) && AnimInstance->GetIsTeamWeaponCrewAnimationActive())
			{
				++OperatorsCrewing;
			}
		}
		Check(OperatorsAtPost == Operators.Num(), TEXT("operators_at_posts"),
		      FString::Printf(TEXT("%d of %d"), OperatorsAtPost, Operators.Num()));
		Check(OperatorsCrewing == Operators.Num(), TEXT("operators_crewing_gun"),
		      FString::Printf(TEXT("%d of %d"), OperatorsCrewing, Operators.Num()));
		int32 VisibleSelectable = 0;
		const TArray<ASquadUnit*> Units = Squad->GetSquadUnitsChecked();
		for (const ASquadUnit* Unit : Units)
		{
			const USelectionComponent* Selection = Unit->GetSelectionComponent();
			if (not Unit->IsHidden() && IsValid(Selection) && Selection->GetCanBeSelected())
			{
				++VisibleSelectable;
			}
		}
		Check(not Units.IsEmpty() && VisibleSelectable == Units.Num(), TEXT("crew_visible_selectable"),
		      FString::Printf(TEXT("%d of %d"), VisibleSelectable, Units.Num()));
	}
	if (GunLocations.Num() == 2)
	{
		const float Spacing = FVector::Dist2D(GunLocations[0], GunLocations[1]);
		Check(Spacing >= 800.f, TEXT("guns_apart"), FString::Printf(TEXT("spacing=%.0f"), Spacing));
	}
}

void UParachutePayloadTestSubsystem::CheckBuildingStep(const FPayloadTestStep& Step)
{
	using namespace ParachutePayloadTestPrivate;
	TArray<AActor*> Buildings = GetNewActorsOfClass(ABuildingExpansion::StaticClass());
	Check(Buildings.Num() == Step.ExpectedCount, TEXT("buildings_dropped"),
	      FString::Printf(TEXT("buildings=%d expected=%d"), Buildings.Num(), Step.ExpectedCount));
	for (AActor* Actor : Buildings)
	{
		ABuildingExpansion* Building = Cast<ABuildingExpansion>(Actor);
		const FString Name = Building->GetClass()->GetName();
		Check(not Building->IsHidden() && Building->GetActorEnableCollision(), TEXT("building_visible_with_collision"),
		      Name);
		Check(Building->FindComponentByClass<UCargoParachuteDescentComponent>() == nullptr,
		      TEXT("building_chute_removed"), Name);
		const float UpZ = Building->GetActorUpVector().Z;
		Check(UpZ >= MinBuildingUpZ, TEXT("building_upright"), FString::Printf(TEXT("%s up_z=%.4f"), *Name, UpZ));
		const float Gap = GetHeightAboveGround(*Building);
		Check(FMath::Abs(Gap) <= MaxGroundGapCm, TEXT("building_on_ground"),
		      FString::Printf(TEXT("%s gap=%.1f at=%s"), *Name, Gap, *Building->GetActorLocation().ToCompactString()));
		const URTSComponent* RTSComponent = Building->GetRTSComponent();
		Check(IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == TestPlayer, TEXT("building_owned_by_player"),
		      FString::Printf(TEXT("%s owner=%d"), *Name, IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : -1));
		Check(Building->GetStatus() == EBuildingExpansionStatus::BXS_Built, TEXT("building_built"),
		      FString::Printf(TEXT("%s status=%s"), *Name, *UEnum::GetValueAsString(Building->GetStatus())));
		Check(Building->IsUnitAlive(), TEXT("building_alive"), Name);
		const USelectionComponent* Selection = Building->GetSelectionComponent();
		Check(IsValid(Selection) && Selection->GetCanBeSelected(), TEXT("building_selectable"), Name);
		const TArray<ACPPTurretsMaster*> Turrets = Building->GetTurrets();
		Check(not Turrets.IsEmpty(), TEXT("building_has_weapons"),
		      FString::Printf(TEXT("%s turrets=%d weapons=%d"), *Name, Turrets.Num(), Building->GetAllWeapons().Num()));
		for (const ACPPTurretsMaster* Turret : Turrets)
		{
			Check(IsValid(Turret) && Turret->GetAttachParentActor() != nullptr, TEXT("turret_attached"), Name);
		}
	}
	if (Buildings.Num() == 2)
	{
		const float Spacing = FVector::Dist2D(Buildings[0]->GetActorLocation(), Buildings[1]->GetActorLocation());
		Check(Spacing >= 500.f, TEXT("buildings_apart"), FString::Printf(TEXT("spacing=%.0f"), Spacing));
	}
}

void UParachutePayloadTestSubsystem::CleanUpStep()
{
	// The next drop lands on the same spot: remove what this one brought.
	TArray<AActor*> NewActors;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (M_ActorsBeforeStep.Contains(Actor) || Actor == M_Camera)
		{
			continue;
		}
		if (Actor->IsA<ASquadController>() || Actor->IsA<ASquadUnit>() || Actor->IsA<ATeamWeapon>() ||
			Actor->IsA<ABuildingExpansion>() || Actor->IsA<AParachuteJumperProxy>())
		{
			NewActors.Add(Actor);
		}
	}
	for (AActor* Actor : NewActors)
	{
		if (IsValid(Actor) && not Actor->IsActorBeingDestroyed())
		{
			Actor->Destroy();
		}
	}
	M_Ability = nullptr;
}

TArray<AActor*> UParachutePayloadTestSubsystem::GetNewActorsOfClass(UClass* ActorClass) const
{
	TArray<AActor*> Actors;
	for (TActorIterator<AActor> It(GetWorld(), ActorClass); It; ++It)
	{
		if (not M_ActorsBeforeStep.Contains(*It) && not It->IsActorBeingDestroyed())
		{
			Actors.Add(*It);
		}
	}
	return Actors;
}

float UParachutePayloadTestSubsystem::GetHeightAboveGround(const AActor& Actor) const
{
	FVector GroundLocation;
	if (not GlobalAbilityHelpers::TryFindGroundLocation(this, Actor.GetActorLocation(), GroundLocation))
	{
		return 99999.f;
	}
	return Actor.GetActorLocation().Z - GroundLocation.Z;
}

bool UParachutePayloadTestSubsystem::TryFindMidDescentLoad(FVector& OutLoadLocation) const
{
	using namespace ParachutePayloadTestPrivate;
	const bool bInfantry = M_Steps[M_StepIndex].Kind == EParachutePayloadKind::Infantry;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (bInfantry)
		{
			if (It->FindComponentByClass<UParachuteDescentComponent>() != nullptr &&
				GetHeightAboveGround(**It) < 1800.f)
			{
				OutLoadLocation = It->GetActorLocation();
				return true;
			}
			continue;
		}
		if (It->IsA<AParachuteJumperProxy>())
		{
			continue;
		}
		const UCargoParachuteDescentComponent* Descent = It->FindComponentByClass<UCargoParachuteDescentComponent>();
		if (IsValid(Descent) && Descent->GetIsDescending() && Descent->GetDescentAlpha() >= MidDescentMinAlpha &&
			Descent->GetDescentAlpha() <= MidDescentMaxAlpha)
		{
			OutLoadLocation = It->GetActorLocation();
			UE_LOG(LogRTSParachutePayloadTest, Display,
			       TEXT("RTS_PARACHUTE_PAYLOAD_TEST CANOPIES load=%s canopies=%d alpha=%.2f"),
			       *It->GetName(), Descent->GetCanopyCount(), Descent->GetDescentAlpha());
			return true;
		}
	}
	return false;
}

void UParachutePayloadTestSubsystem::UpdateCamera() const
{
	using namespace ParachutePayloadTestPrivate;
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	const FRotator ViewRotation(M_CameraPitch, CameraYawFromX, 0.f);
	M_Camera->SetActorLocationAndRotation(M_CameraFocus - ViewRotation.Vector() * M_CameraDistance, ViewRotation);
	// The game's own camera takes the view back; it is handed to the test camera again every frame.
	Controller->SetViewTarget(M_Camera);
}

void UParachutePayloadTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("ParachutePayloadsTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
	UE_LOG(LogRTSParachutePayloadTest, Display, TEXT("RTS_PARACHUTE_PAYLOAD_TEST SCREENSHOT %s"), *FileName);
}

#else

bool UParachutePayloadTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UParachutePayloadTestSubsystem::Tick(float DeltaTime) {}
TStatId UParachutePayloadTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UParachutePayloadTestSubsystem, STATGROUP_Tickables);
}

#endif
