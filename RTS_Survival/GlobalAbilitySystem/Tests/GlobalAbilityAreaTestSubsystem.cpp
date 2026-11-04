// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GlobalAbilityAreaTestSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/CommanderSettings.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/ReconFlight/GlobalAbilityVisionBubble.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/RadixiteNuke/RadixiteNuke.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/UI/GlobalAbilityPanel/W_GlobalAbilityPanel.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerAimAbilitiy/PlayerAimAbility.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/FlareAimAbility/FlareProjectile.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/FieldConstructionAbilityComponent/FieldConstructionActor/FieldMine.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSGlobalAbilityAreaTest, Log, All);

#if RTS_WITH_MAP_TESTS
struct FGlobalAbilityTestAccess
{
	static void MakeFree(UGlobalAbility& Ability)
	{
		Ability.M_AbilityCosts.Costs = FUnitCost();
		Ability.M_AbilityCosts.CoolDownRemaining = 0;
		Ability.M_AbilityRequirements = FGlobalAbilityRequirements();
	}
};
#endif

namespace GlobalAbilityAreaTestConstants
{
	constexpr float StartDelaySeconds = 3.f;
	constexpr float AimHoldSeconds = 2.f;
	constexpr float StripLengthCm = 5000.f;
	constexpr float FlightTimeoutSeconds = 60.f;
	constexpr float NukeTimeoutSeconds = 60.f;
	// The nuke radiates for 30 s before the V2 can use the same spot.
	constexpr float V2RocketTimeoutSeconds = 90.f;
	// Without radiation the warhead is done once the flash and shockwave have faded.
	constexpr float V2SpentAfterBlastSeconds = 6.f;
	// The arc starts 300 m out and peaks around 40 m: far from the side, under the camera.
	constexpr float V2MinApproachDistanceCm = 15000.f;
	constexpr float V2MaxArcHeightCm = 7000.f;
	constexpr float FlareSettleSeconds = 4.f;
	constexpr float MineLandingSeconds = 5.f;
	constexpr float HalfwayFraction = 0.4f;
	constexpr int32 EnemyPlayer = 2;
	constexpr int32 HumanPlayer = 1;
	constexpr float CaptureDistanceCm = 7500.f;
	constexpr float CapturePitchDegrees = 60.f;
	// The spies' targets stand behind the anchor, away from the strips the flights revealed.
	constexpr int32 SpiesTargetCount = 8;
	// UT_Nomadics: 7000 + 3 x 1400 put the last tank past the fog grid at the map edge, where nothing is ever seen.
	constexpr float SpiesTargetDistanceCm = 5500.f;
	constexpr float SpiesTargetSpacingCm = 1000.f;
	// Spawned pawns join the fog TimeRemainingStartAsyncLoading + 2 s after spawning.
	constexpr float SpiesFogSettleSeconds = 8.f;
	constexpr float SpiesRevealSettleSeconds = 2.f;
	constexpr int32 SpiesUnitsToReveal = 6;
	const TCHAR* const PanelClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GameUI/GlobalAbilities/W_BP_GlobalAbilityPanel.W_BP_GlobalAbilityPanel_C");
	const TCHAR* const VictimClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");
}

bool UGlobalAbilityAreaTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if RTS_WITH_MAP_TESTS
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("GlobalAbilityAreaValidate"));
#else
	return false;
#endif
}

TStatId UGlobalAbilityAreaTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGlobalAbilityAreaTestSubsystem, STATGROUP_Tickables);
}

void UGlobalAbilityAreaTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Step)
	{
	case EGlobalAbilityAreaTestStep::WaitingForStart:
		TickWaitingForStart();
		return;
	case EGlobalAbilityAreaTestStep::ReconClicked:
		TickRecon();
		return;
	case EGlobalAbilityAreaTestStep::ReconShotDown:
		TickReconShotDown();
		return;
	case EGlobalAbilityAreaTestStep::FlareCurtain:
		TickFlareCurtain();
		return;
	case EGlobalAbilityAreaTestStep::PlayerMinefield:
		TickPlayerMinefield();
		return;
	case EGlobalAbilityAreaTestStep::EnemyMinefield:
		TickEnemyMinefield();
		return;
	case EGlobalAbilityAreaTestStep::EnemyRecon:
		TickEnemyRecon();
		return;
	case EGlobalAbilityAreaTestStep::SpiesInfiltration:
		TickSpiesInfiltration();
		return;
	case EGlobalAbilityAreaTestStep::Nuke:
		TickNuke();
		return;
	case EGlobalAbilityAreaTestStep::V2Rocket:
		TickV2Rocket();
		return;
	case EGlobalAbilityAreaTestStep::Finished:
		return;
	}
}

void UGlobalAbilityAreaTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedChecks;
		UE_LOG(LogRTSGlobalAbilityAreaTest, Display, TEXT("RTS_ABILITY_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedChecks;
	UE_LOG(LogRTSGlobalAbilityAreaTest, Error, TEXT("RTS_ABILITY_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float UGlobalAbilityAreaTestSubsystem::GetStepSeconds() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() - M_StepStartSeconds : 0.f;
}

void UGlobalAbilityAreaTestSubsystem::EnterStep(const EGlobalAbilityAreaTestStep NewStep)
{
	M_Step = NewStep;
	M_StepStartSeconds = GetWorld()->GetTimeSeconds();
	M_NextCaptureSeconds = 0.f;
	M_CaptureIndex = 0;
	bM_HasKilledAircraft = false;
	UE_LOG(LogRTSGlobalAbilityAreaTest, Display, TEXT("RTS_ABILITY_TEST STEP %s"), *UEnum::GetValueAsString(NewStep));
}

void UGlobalAbilityAreaTestSubsystem::Finish()
{
	const bool bPassed = M_FailedChecks == 0 && M_PassedChecks > 0;
	UE_LOG(LogRTSGlobalAbilityAreaTest, Display, TEXT("RTS_ABILITY_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedChecks, M_FailedChecks);
	M_Step = EGlobalAbilityAreaTestStep::Finished;
	if (ACPPController* Controller = GetController())
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

UGlobalAbility* UGlobalAbilityAreaTestSubsystem::MakeAbility(const EGlobalAbility AbilityType,
                                                             const int32 OwningPlayer)
{
	ACPPController* Controller = GetController();
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	UGlobalAbility* Ability = IsValid(AbilitySet)
		                          ? AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(AbilityType)
		                          : nullptr;
	Check(IsValid(Ability) && IsValid(Manager), TEXT("ability_template"), UEnum::GetValueAsString(AbilityType));
	if (not IsValid(Ability) || not IsValid(Manager))
	{
		return nullptr;
	}
#if RTS_WITH_MAP_TESTS
	FGlobalAbilityTestAccess::MakeFree(*Ability);
#endif
	Ability->InitGlobalAbility(OwningPlayer, Manager, Controller);
	M_Abilities.Add(Ability);
	M_ActiveAbility = Ability;
	return Ability;
}

ACPPController* UGlobalAbilityAreaTestSubsystem::GetController() const
{
	return FRTS_Statics::GetRTSController(GetWorld());
}

APlayerAimAbility* UGlobalAbilityAreaTestSubsystem::FindAimActor() const
{
	for (TActorIterator<APlayerAimAbility> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

bool UGlobalAbilityAreaTestSubsystem::FindGround(const FVector2D& Location, FVector& OutGround) const
{
	return GlobalAbilityHelpers::TryFindGroundLocation(this, FVector(Location.X, Location.Y, 0.f), OutGround);
}

void UGlobalAbilityAreaTestSubsystem::StartAim(UGlobalAbility* Ability, const FString& Label)
{
	ACPPController* Controller = GetController();
	APlayerAimAbility* AimActor = FindAimActor();
	Check(IsValid(Ability) && IsValid(Controller) && IsValid(AimActor), TEXT("aim_can_start"), Label);
	if (not IsValid(Ability) || not IsValid(Controller) || not IsValid(AimActor))
	{
		return;
	}
	Controller->OnGlobaAbilityActivated(Ability->GetAimSettings(), FGlobalAbilitySoundSettings(), Ability);
	Check(AimActor->IsAreaAimActive(), TEXT("area_aim_shown"), Label);
	M_AimLabel = Label;
	M_AimStage = EGlobalAbilityAreaAimTestStage::Hovering;
	bM_IsAimingStrip = Ability->GetAimSettings().AreaAim.Shape == EGlobalAbilityAimShape::Strip;
	M_StepStartSeconds = GetWorld()->GetTimeSeconds();
	FinishPendingShaders();
}

bool UGlobalAbilityAreaTestSubsystem::TickAim()
{
	using namespace GlobalAbilityAreaTestConstants;
	if (M_AimStage == EGlobalAbilityAreaAimTestStage::None)
	{
		return false;
	}
	APlayerAimAbility* AimActor = FindAimActor();
	const bool bIsHovering = M_AimStage == EGlobalAbilityAreaAimTestStage::Hovering;
	if (IsValid(AimActor))
	{
		// This runs after the controller, which moved the aim to wherever the offscreen cursor happens to be.
		AimActor->UpdateGlobalAbilityArea(bIsHovering ? M_LineStart : M_LineEnd);
	}
	if (GetStepSeconds() < AimHoldSeconds)
	{
		return true;
	}
	FinishPendingShaders();
	const FVector Focus = bM_IsAimingStrip ? (M_LineStart + M_LineEnd) * 0.5f : M_LineStart;
	Capture(Focus, M_AimLabel + (bIsHovering ? TEXT("_BeforeClick") : TEXT("_Drawing")));
	if (bIsHovering && bM_IsAimingStrip)
	{
		ClickStripStart();
		return true;
	}
	ClickFinalLocation();
	return false;
}

void UGlobalAbilityAreaTestSubsystem::ClickStripStart()
{
	ACPPController* Controller = GetController();
	APlayerAimAbility* AimActor = FindAimActor();
	if (not IsValid(Controller) || not IsValid(AimActor))
	{
		M_AimStage = EGlobalAbilityAreaAimTestStage::None;
		return;
	}
	Check(Controller->TryHandleActiveGlobalAbilityPrimaryClick(M_LineStart), TEXT("first_click_handled"), M_AimLabel);
	FVector PreviewStart;
	FVector PreviewEnd;
	Check(AimActor->GetStripLine(M_LineEnd, PreviewStart, PreviewEnd), TEXT("first_click_anchored_strip"), M_AimLabel);
	Check(AimActor->IsPlayerAimActive(), TEXT("aim_stays_after_first_click"), M_AimLabel);
	TArray<FVector> DropPoints;
	if (IsValid(M_ActiveAbility))
	{
		M_ActiveAbility->GetStripDropPoints(PreviewStart, PreviewEnd, DropPoints);
	}
	M_ExpectedMines = DropPoints.Num();
	M_AimStage = EGlobalAbilityAreaAimTestStage::Drawing;
	M_StepStartSeconds = GetWorld()->GetTimeSeconds();
}

void UGlobalAbilityAreaTestSubsystem::ClickFinalLocation()
{
	ACPPController* Controller = GetController();
	APlayerAimAbility* AimActor = FindAimActor();
	const FVector ClickLocation = bM_IsAimingStrip ? M_LineEnd : M_LineStart;
	M_AimStage = EGlobalAbilityAreaAimTestStage::None;
	M_StepStartSeconds = GetWorld()->GetTimeSeconds();
	if (not IsValid(Controller) || not IsValid(AimActor))
	{
		return;
	}
	Check(Controller->TryHandleActiveGlobalAbilityPrimaryClick(ClickLocation), TEXT("final_click_handled"), M_AimLabel);
	Check(not AimActor->IsPlayerAimActive(), TEXT("aim_gone_after_final_click"), M_AimLabel);
}

void UGlobalAbilityAreaTestSubsystem::FinishPendingShaders() const
{
#if WITH_EDITOR
	if (GShaderCompilingManager != nullptr)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
#endif
}

void UGlobalAbilityAreaTestSubsystem::Capture(const FVector& Focus, const FString& Label) const
{
	// The capture rig is test code; without map tests this subsystem is never created.
#if RTS_WITH_MAP_TESTS
	using namespace GlobalAbilityAreaTestConstants;
	UWorld* World = GetWorld();
	if (not M_CaptureRig.GetIsEnabled() || not IsValid(World))
	{
		return;
	}
	const FRotator ViewRotation(-CapturePitchDegrees, 0.f, 0.f);
	FB4HowitzerCameraView View;
	View.Name = TEXT("Game");
	View.Location = Focus - ViewRotation.Vector() * CaptureDistanceCm;
	View.LookAt = Focus;
	M_CaptureRig.CaptureViews(*World, FTransform::Identity, {View}, Label);
#endif
}

void UGlobalAbilityAreaTestSubsystem::CaptureEvery(const float IntervalSeconds, const FVector& Focus,
                                                   const FString& Label)
{
	if (GetStepSeconds() < M_NextCaptureSeconds)
	{
		return;
	}
	M_NextCaptureSeconds = GetStepSeconds() + IntervalSeconds;
	Capture(Focus, FString::Printf(TEXT("%s_%02d"), *Label, M_CaptureIndex++));
}

void UGlobalAbilityAreaTestSubsystem::TickWaitingForStart()
{
	using namespace GlobalAbilityAreaTestConstants;
	ACPPController* Controller = GetController();
	if (not IsValid(Controller))
	{
		return;
	}
	// The map boots paused behind the start-game widget.
	if (Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	if (M_StepStartSeconds < 0.f)
	{
		M_StepStartSeconds = GetWorld()->GetTimeSeconds();
		return;
	}
	if (GetStepSeconds() < StartDelaySeconds)
	{
		return;
	}

	FString AnchorText;
	FVector2D AnchorXY = FVector2D::ZeroVector;
	if (FParse::Value(FCommandLine::Get(), TEXT("GlobalAbilityAreaAnchor="), AnchorText))
	{
		FString X;
		FString Y;
		AnchorText.Split(TEXT(","), &X, &Y);
		AnchorXY = FVector2D(FCString::Atof(*X), FCString::Atof(*Y));
	}
	else if (IsValid(Controller->PlayerCameraManager))
	{
		const FVector CameraLocation = Controller->PlayerCameraManager->GetCameraLocation();
		AnchorXY = FVector2D(CameraLocation.X, CameraLocation.Y);
	}
	if (not FindGround(AnchorXY, M_Anchor))
	{
		Check(false, TEXT("found_ground_anchor"));
		Finish();
		return;
	}
	FVector2D StripEndXY = AnchorXY + FVector2D(StripLengthCm, 0.f);
	FindGround(AnchorXY, M_LineStart);
	FindGround(StripEndXY, M_LineEnd);

	for (TActorIterator<AAircraftMaster> It(GetWorld()); It; ++It)
	{
		M_AircraftOfTheMap.Add(*It);
	}
#if RTS_WITH_MAP_TESTS
	FString CaptureDirectory;
	if (FParse::Value(FCommandLine::Get(), TEXT("GlobalAbilityAreaCapture="), CaptureDirectory))
	{
		M_CaptureRig.Init(*GetWorld(), CaptureDirectory);
	}
#endif
	StartRecon();
}

void UGlobalAbilityAreaTestSubsystem::StartRecon()
{
	EnterStep(EGlobalAbilityAreaTestStep::ReconClicked);
	StartAim(MakeAbility(EGlobalAbility::GA_GerReconFlight, GlobalAbilityAreaTestConstants::HumanPlayer),
	         TEXT("Recon"));
}

void UGlobalAbilityAreaTestSubsystem::TickRecon()
{
	using namespace GlobalAbilityAreaTestConstants;
	if (TickAim())
	{
		return;
	}
	CaptureEvery(1.5f, (M_LineStart + M_LineEnd) * 0.5f, TEXT("ReconFlight"));
	const bool bFlightGone = GetStepSeconds() > 2.f && not IsValid(FindRunAircraft());
	if (not bFlightGone && GetStepSeconds() < FlightTimeoutSeconds)
	{
		return;
	}
	Check(bFlightGone, TEXT("recon_flight_finished"));
	const int32 Bubbles = CountBubbles();
	Check(Bubbles >= 4, TEXT("recon_lifted_the_fog_along_the_strip"), FString::Printf(TEXT("bubbles=%d"), Bubbles));
	StartReconShotDown();
}

void UGlobalAbilityAreaTestSubsystem::StartReconShotDown()
{
	EnterStep(EGlobalAbilityAreaTestStep::ReconShotDown);
	UGlobalAbility* Ability = MakeAbility(EGlobalAbility::GA_GerReconFlight,
	                                      GlobalAbilityAreaTestConstants::HumanPlayer);
	if (IsValid(Ability))
	{
		Ability->ExecuteAbilityAlongLine(M_LineStart, M_LineEnd);
	}
}

void UGlobalAbilityAreaTestSubsystem::TickReconShotDown()
{
	using namespace GlobalAbilityAreaTestConstants;
	AAircraftMaster* Aircraft = FindRunAircraft();
	if (not bM_HasKilledAircraft && IsValid(Aircraft))
	{
		const float Flown = FVector::DotProduct(Aircraft->GetActorLocation() - M_LineStart, FVector::ForwardVector);
		if (Flown >= StripLengthCm * HalfwayFraction)
		{
			bM_HasKilledAircraft = true;
			M_BubblesAtKill = CountBubbles();
			Aircraft->TakeFatalDamage();
		}
	}
	if (bM_HasKilledAircraft && GetStepSeconds() > 12.f)
	{
		// Bubbles of the first flight expire on their own, so the count can only go down.
		const int32 Bubbles = CountBubbles();
		Check(Bubbles <= M_BubblesAtKill, TEXT("reconnaissance_stops_when_shot_down"),
		      FString::Printf(TEXT("at_kill=%d now=%d"), M_BubblesAtKill, Bubbles));
		StartFlareCurtain();
		return;
	}
	if (GetStepSeconds() > FlightTimeoutSeconds)
	{
		Check(false, TEXT("recon_aircraft_reached_halfway"));
		StartFlareCurtain();
	}
}

void UGlobalAbilityAreaTestSubsystem::StartFlareCurtain()
{
	EnterStep(EGlobalAbilityAreaTestStep::FlareCurtain);
	StartAim(MakeAbility(EGlobalAbility::GA_GerFlareCurtain, GlobalAbilityAreaTestConstants::HumanPlayer),
	         TEXT("Flare"));
}

void UGlobalAbilityAreaTestSubsystem::TickFlareCurtain()
{
	using namespace GlobalAbilityAreaTestConstants;
	if (TickAim())
	{
		return;
	}
	CaptureEvery(2.f, (M_LineStart + M_LineEnd) * 0.5f, TEXT("FlareFlight"));
	const bool bFlightGone = GetStepSeconds() > 2.f && not IsValid(FindRunAircraft());
	if (not bFlightGone && GetStepSeconds() < FlightTimeoutSeconds)
	{
		return;
	}
	if (GetStepSeconds() < FlareSettleSeconds)
	{
		return;
	}
	int32 Flares = 0;
	int32 Burning = 0;
	for (TActorIterator<AFlareProjectile> It(GetWorld()); It; ++It)
	{
		++Flares;
		Burning += It->GetPhase() == EFlareProjectilePhase::Burning ? 1 : 0;
	}
	const int32 Expected = IsValid(M_ActiveAbility) ? M_ActiveAbility->GetAimSettings().AreaAim.ShaderMarkerCount : 0;
	Check(Expected > 0 && Flares == Expected, TEXT("flare_curtain_dropped_every_flare"),
	      FString::Printf(TEXT("flares=%d expected=%d"), Flares, Expected));
	Check(Burning > 0, TEXT("dropped_flares_burn"), FString::Printf(TEXT("burning=%d"), Burning));
	StartPlayerMinefield();
}

void UGlobalAbilityAreaTestSubsystem::StartPlayerMinefield()
{
	EnterStep(EGlobalAbilityAreaTestStep::PlayerMinefield);
	StartAim(MakeAbility(EGlobalAbility::GA_GerAirMinefield, GlobalAbilityAreaTestConstants::HumanPlayer),
	         TEXT("Minefield"));
}

void UGlobalAbilityAreaTestSubsystem::TickPlayerMinefield()
{
	using namespace GlobalAbilityAreaTestConstants;
	if (TickAim())
	{
		return;
	}
	CaptureEvery(1.f, (M_LineStart + M_LineEnd) * 0.5f, TEXT("MinefieldFlight"));
	const bool bFlightGone = GetStepSeconds() > 2.f && not IsValid(FindRunAircraft());
	if ((not bFlightGone || GetStepSeconds() < MineLandingSeconds) && GetStepSeconds() < FlightTimeoutSeconds)
	{
		return;
	}
	int32 Mines = 0;
	bool bAllOwned = true;
	for (TActorIterator<AFieldMine> It(GetWorld()); It; ++It)
	{
		++Mines;
		const URTSComponent* RTSComponent = It->FindComponentByClass<URTSComponent>();
		bAllOwned &= IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == HumanPlayer;
	}
	Check(M_ExpectedMines > 0 && Mines == M_ExpectedMines, TEXT("mines_landed_on_previewed_spots"),
	      FString::Printf(TEXT("mines=%d previewed=%d"), Mines, M_ExpectedMines));
	Check(bAllOwned, TEXT("player_mines_belong_to_player"));
	StartEnemyMinefield();
}

void UGlobalAbilityAreaTestSubsystem::StartEnemyMinefield()
{
	using namespace GlobalAbilityAreaTestConstants;
	EnterStep(EGlobalAbilityAreaTestStep::EnemyMinefield);
	UGlobalAbility* Ability = MakeAbility(EGlobalAbility::GA_SovietAirMinefield, EnemyPlayer);
	if (IsValid(Ability))
	{
		// The AI gives a point; the field is laid somewhere else than the player's.
		Ability->ExecuteAbilityAtLocation(M_Anchor + FVector(0.f, 4000.f, 0.f));
	}
}

void UGlobalAbilityAreaTestSubsystem::TickEnemyMinefield()
{
	using namespace GlobalAbilityAreaTestConstants;
	const bool bFlightGone = GetStepSeconds() > 2.f && not IsValid(FindRunAircraft());
	if ((not bFlightGone || GetStepSeconds() < MineLandingSeconds) && GetStepSeconds() < FlightTimeoutSeconds)
	{
		return;
	}
	int32 EnemyMines = 0;
	int32 ConcealedMines = 0;
	for (TActorIterator<AFieldMine> It(GetWorld()); It; ++It)
	{
		const URTSComponent* RTSComponent = It->FindComponentByClass<URTSComponent>();
		if (not IsValid(RTSComponent) || RTSComponent->GetOwningPlayer() != EnemyPlayer)
		{
			continue;
		}
		++EnemyMines;
		const UStaticMeshComponent* Mesh = It->FindComponentByClass<UStaticMeshComponent>();
		ConcealedMines += IsValid(Mesh) && Mesh->bHiddenInGame ? 1 : 0;
	}
	Check(EnemyMines > 0, TEXT("enemy_minefield_laid_from_a_point"), FString::Printf(TEXT("mines=%d"), EnemyMines));
	Check(ConcealedMines == EnemyMines, TEXT("enemy_mines_are_concealed"),
	      FString::Printf(TEXT("concealed=%d of %d"), ConcealedMines, EnemyMines));
	StartEnemyRecon();
}

void UGlobalAbilityAreaTestSubsystem::StartEnemyRecon()
{
	using namespace GlobalAbilityAreaTestConstants;
	EnterStep(EGlobalAbilityAreaTestStep::EnemyRecon);
	UGlobalAbility* Ability = MakeAbility(EGlobalAbility::GA_SovietReconFlight, EnemyPlayer);
	if (IsValid(Ability))
	{
		Ability->ExecuteAbilityAtLocation(M_Anchor);
	}
}

void UGlobalAbilityAreaTestSubsystem::TickEnemyRecon()
{
	using namespace GlobalAbilityAreaTestConstants;
	const AAircraftMaster* Aircraft = FindRunAircraft();
	if (IsValid(Aircraft))
	{
		const FString ClassName = Aircraft->GetClass()->GetName();
		bM_SawSovietRecon |= ClassName.Contains(TEXT("PE2"));
		CaptureEvery(1.f, Aircraft->GetActorLocation(), TEXT("SovietRecon"));
	}
	const bool bFlightGone = GetStepSeconds() > 2.f && not IsValid(Aircraft);
	if (not bFlightGone && GetStepSeconds() < FlightTimeoutSeconds)
	{
		return;
	}
	Check(bFlightGone, TEXT("soviet_recon_flight_finished"));
	Check(bM_SawSovietRecon, TEXT("soviet_recon_is_the_pe2"));
	CheckPlayerLoadoutFitsPanel();
	CheckSovietNukeTemplate();
	StartSpiesInfiltration();
}

void UGlobalAbilityAreaTestSubsystem::StartSpiesInfiltration()
{
	using namespace GlobalAbilityAreaTestConstants;
	EnterStep(EGlobalAbilityAreaTestStep::SpiesInfiltration);
	UClass* TargetClass = LoadClass<AActor>(nullptr, VictimClassPath);
	for (int32 Index = 0; Index < SpiesTargetCount; ++Index)
	{
		const FVector2D Offset(-SpiesTargetDistanceCm - (Index / 2) * SpiesTargetSpacingCm,
		                       (Index % 2 == 0 ? -1.f : 1.f) * SpiesTargetSpacingCm);
		FVector Ground;
		FindGround(FVector2D(M_Anchor.X, M_Anchor.Y) + Offset, Ground);
		M_SpiesTargets.Add(FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
			*GetWorld(), TargetClass, FTransform(Ground + FVector(0.f, 0.f, 120.f)), EnemyPlayer));
	}
}

void UGlobalAbilityAreaTestSubsystem::TickSpiesInfiltration()
{
	using namespace GlobalAbilityAreaTestConstants;
	if (not bM_HasSentSpies)
	{
		if (GetStepSeconds() >= SpiesFogSettleSeconds)
		{
			SendSpies();
		}
		return;
	}
	if (GetStepSeconds() < SpiesFogSettleSeconds + SpiesRevealSettleSeconds)
	{
		return;
	}
	CheckSpiesReveal();
	for (const TWeakObjectPtr<AActor>& Target : M_SpiesTargets)
	{
		if (Target.IsValid())
		{
			Target->Destroy();
		}
	}
	StartNuke();
}

TArray<AActor*> UGlobalAbilityAreaTestSubsystem::GetHiddenEnemyUnits() const
{
	using namespace GlobalAbilityAreaTestConstants;
	TArray<AActor*> Hidden;
	UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		return Hidden;
	}
	for (ATankMaster* Tank : GameUnitManager->GetPlayerTanks(EnemyPlayer))
	{
		if (IsValid(Tank) && Tank->IsHidden())
		{
			Hidden.Add(Tank);
		}
	}
	for (ASquadUnit* SquadUnit : GameUnitManager->GetSquadUnitsOfPlayer(EnemyPlayer))
	{
		if (IsValid(SquadUnit) && SquadUnit->IsHidden())
		{
			Hidden.Add(SquadUnit);
		}
	}
	return Hidden;
}

void UGlobalAbilityAreaTestSubsystem::SendSpies()
{
	using namespace GlobalAbilityAreaTestConstants;
	bM_HasSentSpies = true;
	int32 HiddenTargets = 0;
	for (const TWeakObjectPtr<AActor>& Target : M_SpiesTargets)
	{
		HiddenTargets += Target.IsValid() && Target->IsHidden() ? 1 : 0;
	}
	Check(HiddenTargets > SpiesUnitsToReveal, TEXT("spies_targets_start_in_the_fog"),
	      FString::Printf(TEXT("hidden=%d of %d"), HiddenTargets, M_SpiesTargets.Num()));
	for (AActor* Unit : GetHiddenEnemyUnits())
	{
		M_HiddenBeforeSpies.Add(Unit);
	}

	UGlobalAbility* Ability = MakeAbility(EGlobalAbility::GA_GerSpiesInfiltration, HumanPlayer);
	if (not IsValid(Ability))
	{
		return;
	}
	Check(Ability->GetAbilityTier() == EGlobalAbilityTier::TierI, TEXT("spies_are_tier_one"));
	// The panel button path: no aim, the ability runs and starts its cooldown at once.
	Ability->OnClickedAbilityButton();
	const APlayerAimAbility* AimActor = FindAimActor();
	Check(not IsValid(AimActor) || not AimActor->IsPlayerAimActive(), TEXT("spies_need_no_aim"));
	Check(Ability->GetAbilityCosts().CoolDownRemaining == 180, TEXT("spies_cooldown_is_180_seconds"),
	      FString::Printf(TEXT("cooldown=%d"), Ability->GetAbilityCosts().CoolDownRemaining));

	int32 Bubbles = 0;
	int32 OnHiddenEnemies = 0;
	TSet<const AActor*> DistinctUnits;
	for (TActorIterator<AGlobalAbilityVisionBubble> It(GetWorld()); It; ++It)
	{
		AActor* Unit = It->GetAttachParentActor();
		if (not IsValid(Unit))
		{
			// The flights' bubbles stand on the ground.
			continue;
		}
		++Bubbles;
		DistinctUnits.Add(Unit);
		OnHiddenEnemies += M_HiddenBeforeSpies.Contains(Unit) ? 1 : 0;
		M_RevealedBySpies.Add(Unit);
	}
	const int32 Expected = FMath::Min(SpiesUnitsToReveal, M_HiddenBeforeSpies.Num());
	Check(Bubbles == Expected, TEXT("spies_reveal_six_units"),
	      FString::Printf(TEXT("bubbles=%d expected=%d hidden_enemies=%d"), Bubbles, Expected,
	                      M_HiddenBeforeSpies.Num()));
	Check(DistinctUnits.Num() == Bubbles, TEXT("spies_reveal_different_units"));
	Check(OnHiddenEnemies == Bubbles, TEXT("spies_only_pick_hidden_enemies"),
	      FString::Printf(TEXT("on_hidden=%d bubbles=%d"), OnHiddenEnemies, Bubbles));
}

void UGlobalAbilityAreaTestSubsystem::CheckSpiesReveal()
{
	int32 Visible = 0;
	for (const TWeakObjectPtr<AActor>& Unit : M_RevealedBySpies)
	{
		const bool bVisible = Unit.IsValid() && not Unit->IsHidden();
		Visible += bVisible ? 1 : 0;
		UE_LOG(LogRTSGlobalAbilityAreaTest, Display, TEXT("RTS_ABILITY_TEST SPIED %s visible=%d at %s"),
		       Unit.IsValid() ? *Unit->GetName() : TEXT("gone"), bVisible ? 1 : 0,
		       Unit.IsValid() ? *Unit->GetActorLocation().ToCompactString() : TEXT("-"));
	}
	Check(M_RevealedBySpies.Num() > 0 && Visible == M_RevealedBySpies.Num(), TEXT("spied_units_show_through_the_fog"),
	      FString::Printf(TEXT("visible=%d of %d"), Visible, M_RevealedBySpies.Num()));
}

void UGlobalAbilityAreaTestSubsystem::CheckPlayerLoadoutFitsPanel()
{
	const UCommanderSettings* CommanderSettings = UCommanderSettings::Get();
	UClass* PanelClass = LoadClass<UW_GlobalAbilityPanel>(nullptr, GlobalAbilityAreaTestConstants::PanelClassPath);
	UW_GlobalAbilityPanel* Panel = IsValid(PanelClass)
		                               ? CreateWidget<UW_GlobalAbilityPanel>(GetWorld(), PanelClass)
		                               : nullptr;
	if (not IsValid(CommanderSettings) || not IsValid(Panel))
	{
		Check(false, TEXT("panel_and_commander_exist"));
		return;
	}
	const int32 PanelSlots = Panel->GetAbilityItems().Num();
	TSet<EGlobalAbility> AbilitiesInAnyKit;
	for (const TPair<ERTSCommander, FRTSCommanderSettings>& CommanderKit : CommanderSettings->CommanderSettingsByType)
	{
		const TArray<EGlobalAbility>& Loadout = CommanderKit.Value.GlobalAbilities;
		const FString CommanderName = UEnum::GetValueAsString(CommanderKit.Key);
		Check(Loadout.Num() == RTSCommanderConstants::AbilitySlotsPerCommander, TEXT("commander_kit_has_all_slots"),
		      FString::Printf(TEXT("%s abilities=%d"), *CommanderName, Loadout.Num()));
		Check(PanelSlots >= Loadout.Num(), TEXT("player_loadout_fits_the_panel"),
		      FString::Printf(TEXT("%s slots=%d abilities=%d"), *CommanderName, PanelSlots, Loadout.Num()));
		AbilitiesInAnyKit.Append(Loadout);
	}
	// The area abilities under test must be reachable by the player through at least one commander.
	for (const EGlobalAbility Ability : {
		     EGlobalAbility::GA_GerReconFlight, EGlobalAbility::GA_GerFlareCurtain,
		     EGlobalAbility::GA_GerAirMinefield, EGlobalAbility::GA_GerRadixiteNuke,
		     EGlobalAbility::GA_GerSpiesInfiltration
	     })
	{
		Check(AbilitiesInAnyKit.Contains(Ability), TEXT("ability_in_player_loadout"), UEnum::GetValueAsString(Ability));
	}
}

void UGlobalAbilityAreaTestSubsystem::CheckSovietNukeTemplate()
{
	const UGlobalAbility* Ability = MakeAbility(EGlobalAbility::GA_SovietRadixiteNuke,
	                                            GlobalAbilityAreaTestConstants::EnemyPlayer);
	Check(IsValid(Ability) && Ability->GetAimSettings().AreaAim.AreaRadius > 1.f, TEXT("soviet_nuke_template_has_area"));
}

void UGlobalAbilityAreaTestSubsystem::StartNuke()
{
	using namespace GlobalAbilityAreaTestConstants;
	EnterStep(EGlobalAbilityAreaTestStep::Nuke);
	UGlobalAbility* Ability = MakeAbility(EGlobalAbility::GA_GerRadixiteNuke, HumanPlayer);
	if (not IsValid(Ability))
	{
		Finish();
		return;
	}
	const FGlobalAbilityAreaAimSettings& AreaAim = Ability->GetAimSettings().AreaAim;
	M_CoreVictim = SpawnVictimNearAnchor(FVector2D(AreaAim.InnerRadius * 0.3f, 0.f), EnemyPlayer);
	M_EdgeVictim = SpawnVictimNearAnchor(FVector2D(0.f, AreaAim.AreaRadius * 0.9f), EnemyPlayer);
	M_FarBystander = SpawnVictimNearAnchor(FVector2D(0.f, AreaAim.AreaRadius * 2.f), EnemyPlayer);

	M_LineStart = M_Anchor;
	StartAim(Ability, TEXT("Nuke"));
}

AActor* UGlobalAbilityAreaTestSubsystem::SpawnVictimNearAnchor(const FVector2D& Offset, const uint8 OwningPlayer) const
{
	constexpr float SpawnAboveGroundCm = 120.f;
	UClass* VictimClass = LoadClass<AActor>(nullptr, GlobalAbilityAreaTestConstants::VictimClassPath);
	FVector Ground;
	FindGround(FVector2D(M_Anchor.X, M_Anchor.Y) + Offset, Ground);
	return FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*GetWorld(), VictimClass,
	                                                 FTransform(Ground + FVector(0.f, 0.f, SpawnAboveGroundCm)),
	                                                 OwningPlayer);
}

void UGlobalAbilityAreaTestSubsystem::TickNuke()
{
	using namespace GlobalAbilityAreaTestConstants;
	if (TickAim())
	{
		return;
	}
	ARadixiteNuke* Nuke = nullptr;
	for (TActorIterator<ARadixiteNuke> It(GetWorld()); It; ++It)
	{
		Nuke = *It;
	}
	CaptureEvery(0.75f, M_Anchor, TEXT("NukeBlast"));
	const bool bPastBlast = IsValid(Nuke) && (Nuke->GetPhase() == ERadixiteNukePhase::Radiating
		|| Nuke->GetPhase() == ERadixiteNukePhase::Spent);
	if (bPastBlast && M_CaptureIndex > 0 && GetStepSeconds() > 14.f)
	{
		CheckNukeResult();
		StartV2Rocket();
		return;
	}
	if (GetStepSeconds() > NukeTimeoutSeconds)
	{
		Check(false, TEXT("nuke_detonated"), IsValid(Nuke) ? UEnum::GetValueAsString(Nuke->GetPhase()) : TEXT("none"));
		Finish();
	}
}

void UGlobalAbilityAreaTestSubsystem::CheckNukeResult()
{
	AActor* EdgeVictim = M_EdgeVictim.Get();
	AActor* FarBystander = M_FarBystander.Get();
	Check(not M_CoreVictim.IsValid() || not M_CoreVictim->IsUnitAlive(), TEXT("nuke_core_kills"));
	Check(IsValid(FarBystander) && FarBystander->IsUnitAlive(), TEXT("nuke_spares_outside_the_blast"));
	UE_LOG(LogRTSGlobalAbilityAreaTest, Display, TEXT("RTS_ABILITY_TEST NUKE edge_victim_alive=%d"),
	       IsValid(EdgeVictim) && EdgeVictim->IsUnitAlive() ? 1 : 0);
}

void UGlobalAbilityAreaTestSubsystem::StartV2Rocket()
{
	EnterStep(EGlobalAbilityAreaTestStep::V2Rocket);
	M_V2Rocket.Reset();
	bM_HasAimedV2Rocket = false;
	M_V2MaxApproachDistance = 0.f;
	M_V2MaxHeight = 0.f;
	M_V2BlastSeconds = -1.f;
}

void UGlobalAbilityAreaTestSubsystem::AimV2Rocket()
{
	using namespace GlobalAbilityAreaTestConstants;
	bM_HasAimedV2Rocket = true;
	UGlobalAbility* Ability = MakeAbility(EGlobalAbility::GA_GerV2Rocket, HumanPlayer);
	if (not IsValid(Ability))
	{
		Finish();
		return;
	}
	const FGlobalAbilityAreaAimSettings& AreaAim = Ability->GetAimSettings().AreaAim;
	Check(AreaAim.AreaRadius > 1.f && AreaAim.InnerRadius > 1.f, TEXT("v2_template_has_blast_and_core"));
	M_CoreVictim = SpawnVictimNearAnchor(FVector2D(AreaAim.InnerRadius * 0.3f, 0.f), EnemyPlayer);
	M_EdgeVictim = SpawnVictimNearAnchor(FVector2D(-AreaAim.AreaRadius * 0.9f, 0.f), EnemyPlayer);
	M_FarBystander = SpawnVictimNearAnchor(FVector2D(0.f, -AreaAim.AreaRadius * 2.f), EnemyPlayer);
	M_LineStart = M_Anchor;
	StartAim(Ability, TEXT("V2Rocket"));
}

void UGlobalAbilityAreaTestSubsystem::TickV2Rocket()
{
	using namespace GlobalAbilityAreaTestConstants;
	if (GetStepSeconds() > V2RocketTimeoutSeconds)
	{
		Check(false, TEXT("v2_detonated"), M_V2Rocket.IsValid() ? UEnum::GetValueAsString(M_V2Rocket->GetPhase())
			                                       : TEXT("none"));
		Finish();
		return;
	}
	if (not bM_HasAimedV2Rocket)
	{
		if (not IsAnyNukeActive())
		{
			AimV2Rocket();
		}
		return;
	}
	if (TickAim())
	{
		return;
	}
	TrackV2Flight();
	if (not M_V2Rocket.IsValid())
	{
		return;
	}
	// The pictures follow the rocket while it flies, then look at the blast.
	const bool bIsFlying = M_V2Rocket->GetPhase() == ERadixiteNukePhase::Falling;
	CaptureEvery(0.5f, bIsFlying ? M_V2Rocket->GetWarheadLocation() : M_Anchor, TEXT("V2Rocket"));
	if (M_V2Rocket->GetPhase() == ERadixiteNukePhase::Spent && M_CaptureIndex > 0)
	{
		CheckV2Result();
		Finish();
	}
}

void UGlobalAbilityAreaTestSubsystem::TrackV2Flight()
{
	if (not M_V2Rocket.IsValid())
	{
		for (TActorIterator<ARadixiteNuke> It(GetWorld()); It; ++It)
		{
			if (It->GetPhase() != ERadixiteNukePhase::Spent)
			{
				M_V2Rocket = *It;
			}
		}
		return;
	}
	const ERadixiteNukePhase Phase = M_V2Rocket->GetPhase();
	if (Phase == ERadixiteNukePhase::Falling)
	{
		const FVector Offset = M_V2Rocket->GetWarheadLocation() - M_V2Rocket->GetActorLocation();
		M_V2MaxApproachDistance = FMath::Max(M_V2MaxApproachDistance, Offset.Size2D());
		M_V2MaxHeight = FMath::Max(M_V2MaxHeight, Offset.Z);
		return;
	}
	if (Phase != ERadixiteNukePhase::Warning && M_V2BlastSeconds < 0.f)
	{
		M_V2BlastSeconds = GetWorld()->GetTimeSeconds();
	}
}

void UGlobalAbilityAreaTestSubsystem::CheckV2Result()
{
	using namespace GlobalAbilityAreaTestConstants;
	const float SpentAfterBlastSeconds = GetWorld()->GetTimeSeconds() - M_V2BlastSeconds;
	Check(M_V2MaxApproachDistance > V2MinApproachDistanceCm, TEXT("v2_flies_in_from_afar"),
	      FString::Printf(TEXT("approach=%.0f"), M_V2MaxApproachDistance));
	Check(M_V2MaxHeight > 0.f && M_V2MaxHeight < V2MaxArcHeightCm, TEXT("v2_flies_a_low_arc"),
	      FString::Printf(TEXT("max_height=%.0f"), M_V2MaxHeight));
	Check(M_V2BlastSeconds > 0.f && SpentAfterBlastSeconds < V2SpentAfterBlastSeconds, TEXT("v2_leaves_no_radiation"),
	      FString::Printf(TEXT("spent_after=%.1f"), SpentAfterBlastSeconds));
	Check(not M_CoreVictim.IsValid() || not M_CoreVictim->IsUnitAlive(), TEXT("v2_core_kills"));
	Check(M_FarBystander.IsValid() && M_FarBystander->IsUnitAlive(), TEXT("v2_spares_outside_the_blast"));
	UE_LOG(LogRTSGlobalAbilityAreaTest, Display, TEXT("RTS_ABILITY_TEST V2 edge_victim_alive=%d"),
	       M_EdgeVictim.IsValid() && M_EdgeVictim->IsUnitAlive() ? 1 : 0);
}

bool UGlobalAbilityAreaTestSubsystem::IsAnyNukeActive() const
{
	for (TActorIterator<ARadixiteNuke> It(GetWorld()); It; ++It)
	{
		if (It->GetPhase() != ERadixiteNukePhase::Spent)
		{
			return true;
		}
	}
	return false;
}

int32 UGlobalAbilityAreaTestSubsystem::CountBubbles() const
{
	int32 Bubbles = 0;
	for (TActorIterator<AGlobalAbilityVisionBubble> It(GetWorld()); It; ++It)
	{
		++Bubbles;
	}
	return Bubbles;
}

AAircraftMaster* UGlobalAbilityAreaTestSubsystem::FindRunAircraft() const
{
	for (TActorIterator<AAircraftMaster> It(GetWorld()); It; ++It)
	{
		if (It->IsUnitAlive() && not M_AircraftOfTheMap.Contains(*It))
		{
			return *It;
		}
	}
	return nullptr;
}
