// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GliderDropTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GliderDrop/GA_GliderTankDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSGliderTest, Log, All);

namespace GliderDropTestPrivate
{
	constexpr int32 TestPlayer = 1;
	constexpr float StartDelaySeconds = 3.f;
	constexpr float TimeoutSeconds = 200.f;
	// Matches the fixed strip length of the GA_GerE100Drop template.
	constexpr float SkidLength = 7000.f;
	constexpr float CameraTraceLength = 500000.f;
	constexpr float MidPhaseShotSeconds = 3.f;
	// The tank counts as out once it has driven this far from where it was placed inside the hull.
	constexpr float TankOutDistance = 1500.f;
	constexpr float TankDriveSeconds = 25.f;
	constexpr float CameraPitch = -50.f;
	constexpr float CameraYawFromRun = 145.f;
}

bool UGliderDropTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("GliderDropValidate"));
}

TStatId UGliderDropTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGliderDropTestSubsystem, STATGROUP_Tickables);
}

void UGliderDropTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case EGliderDropTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EGliderDropTestPhase::Running:
		TickRun();
		return;
	case EGliderDropTestPhase::Finished:
		return;
	}
}

void UGliderDropTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSGliderTest, Display, TEXT("RTS_GLIDER_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSGliderTest, Error, TEXT("RTS_GLIDER_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float UGliderDropTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void UGliderDropTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSGliderTest, Display, TEXT("RTS_GLIDER_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = EGliderDropTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UGliderDropTestSubsystem::TickWaitingForStart()
{
	using namespace GliderDropTestPrivate;
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
	StartRun();
}

bool UGliderDropTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace GliderDropTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("GliderDropAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("GliderDropAnchorY="), AnchorY))
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

void UGliderDropTestSubsystem::StartRun()
{
	using namespace GliderDropTestPrivate;
	M_Phase = EGliderDropTestPhase::Running;
	M_RunStartSeconds = GetNow();
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	Check(IsValid(Manager) && IsValid(AbilitySet), TEXT("ability_manager_and_set"));
	if (not IsValid(Manager) || not IsValid(AbilitySet))
	{
		Finish();
		return;
	}
	// A map started directly has no commander, so the ability is built from its template as the manager does.
	M_Ability = AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(EGlobalAbility::GA_GerE100Drop);
	Check(IsValid(Cast<UGA_GliderTankDrop>(M_Ability)), TEXT("e100_drop_is_glider_drop"),
	      IsValid(M_Ability) ? M_Ability->GetClass()->GetName() : TEXT("none"));
	if (not IsValid(Cast<UGA_GliderTankDrop>(M_Ability)))
	{
		Finish();
		return;
	}

	float RunYawDegrees = 0.f;
	FParse::Value(FCommandLine::Get(), TEXT("GliderDropYaw="), RunYawDegrees);
	M_RunDirection = FRotator(0.f, RunYawDegrees, 0.f).Vector();
	M_Ability->InitGlobalAbility(TestPlayer, Manager, Controller);
	M_Ability->ExecuteAbilityAlongLine(M_Anchor - M_RunDirection * SkidLength, M_Anchor);
	if (UWorld* World = GetWorld())
	{
		M_Camera = World->SpawnActor<ACameraActor>();
	}
	UE_LOG(LogRTSGliderTest, Display, TEXT("RTS_GLIDER_TEST EXECUTED anchor=%s yaw=%.0f"),
	       *M_Anchor.ToCompactString(), RunYawDegrees);
}

void UGliderDropTestSubsystem::TickRun()
{
	using namespace GliderDropTestPrivate;
	if (not M_Delivery.IsValid())
	{
		for (TActorIterator<AGliderTankDelivery> It(GetWorld()); It; ++It)
		{
			M_Delivery = *It;
			It->OnTankDelivered.AddUObject(this, &UGliderDropTestSubsystem::OnTankDelivered);
			LogSkidDustParameters(**It);
			break;
		}
	}
	TrackDeliveryPhase();
	UpdateCamera();

	const float Elapsed = GetNow() - M_RunStartSeconds;
	const bool bTankDroveLongEnough = bM_DeliveredBroadcast && GetNow() - M_TankDeliveredSeconds >= TankDriveSeconds;
	if (bTankDroveLongEnough || Elapsed >= TimeoutSeconds)
	{
		FinishRun();
	}
}

void UGliderDropTestSubsystem::TrackDeliveryPhase()
{
	using namespace GliderDropTestPrivate;
	if (not M_Delivery.IsValid())
	{
		return;
	}
	const EGliderDeliveryPhase Phase = M_Delivery->GetDeliveryPhase();
	if (Phase != M_LastSeenPhase)
	{
		M_LastSeenPhase = Phase;
		M_PhaseSeenSeconds = GetNow();
		bM_TookMidPhaseShot = false;
		M_FurthestPhase = static_cast<uint8>(Phase) > static_cast<uint8>(M_FurthestPhase) ? Phase : M_FurthestPhase;
		const FString PhaseName = UEnum::GetValueAsString(Phase);
		UE_LOG(LogRTSGliderTest, Display, TEXT("RTS_GLIDER_TEST PHASE %s at=%.1fs location=%s"), *PhaseName,
		       GetNow() - M_RunStartSeconds, *M_Delivery->GetActorLocation().ToCompactString());
		RequestScreenshot(PhaseName.RightChop(PhaseName.Find(TEXT("::")) + 2) + TEXT("_start"));
		return;
	}
	if (not bM_TookMidPhaseShot && GetNow() - M_PhaseSeenSeconds >= MidPhaseShotSeconds)
	{
		bM_TookMidPhaseShot = true;
		const FString PhaseName = UEnum::GetValueAsString(Phase);
		RequestScreenshot(PhaseName.RightChop(PhaseName.Find(TEXT("::")) + 2) + TEXT("_mid"));
	}
}

void UGliderDropTestSubsystem::LogSkidDustParameters(const AGliderTankDelivery& Delivery) const
{
	const UNiagaraComponent* Dust = Delivery.FindComponentByClass<UNiagaraComponent>();
	const UNiagaraSystem* System = IsValid(Dust) ? Dust->GetAsset() : nullptr;
	if (not IsValid(System))
	{
		return;
	}
	TArray<FNiagaraVariable> UserParameters;
	System->GetExposedParameters().GetUserParameters(UserParameters);
	for (const FNiagaraVariable& Parameter : UserParameters)
	{
		UE_LOG(LogRTSGliderTest, Display, TEXT("RTS_GLIDER_TEST DUST_PARAM %s %s"), *Parameter.GetName().ToString(),
		       *Parameter.GetType().GetName());
	}
}

void UGliderDropTestSubsystem::OnTankDelivered(AGliderTankDelivery* Delivery, ATankMaster* Tank)
{
	bM_DeliveredBroadcast = true;
	M_TankDeliveredSeconds = GetNow();
	M_Tank = Tank;
	if (IsValid(Tank))
	{
		M_TankStartLocation = Tank->GetActorLocation();
	}
	UE_LOG(LogRTSGliderTest, Display, TEXT("RTS_GLIDER_TEST DELIVERED tank=%s at=%.1fs"),
	       IsValid(Tank) ? *Tank->GetClass()->GetName() : TEXT("none"), GetNow() - M_RunStartSeconds);
}

void UGliderDropTestSubsystem::FinishRun()
{
	using namespace GliderDropTestPrivate;
	RequestScreenshot(TEXT("Final"));
	Check(M_Delivery.IsValid() || M_FurthestPhase != EGliderDeliveryPhase::None, TEXT("delivery_spawned"));
	Check(static_cast<uint8>(M_FurthestPhase) >= static_cast<uint8>(EGliderDeliveryPhase::Lingering),
	      TEXT("glider_landed_and_opened"), UEnum::GetValueAsString(M_FurthestPhase));
	Check(bM_DeliveredBroadcast && M_Tank.IsValid(), TEXT("tank_delivered"));
	if (M_Tank.IsValid())
	{
		const ATankMaster* Tank = M_Tank.Get();
		Check(Tank->GetClass()->GetName().Contains(TEXT("E100")), TEXT("tank_is_e100"), Tank->GetClass()->GetName());
		const float Driven = FVector::Dist2D(Tank->GetActorLocation(), M_TankStartLocation);
		Check(Driven >= TankOutDistance, TEXT("tank_drove_out_of_glider"), FString::Printf(TEXT("driven=%.0f"), Driven));
		const float FromStop = FVector::Dist2D(Tank->GetActorLocation(), M_Anchor);
		Check(FromStop <= SkidLength, TEXT("tank_near_stop_point"), FString::Printf(TEXT("from_stop=%.0f"), FromStop));
		const USelectionComponent* Selection = Tank->GetSelectionComponent();
		Check(IsValid(Selection) && Selection->GetCanBeSelected(), TEXT("tank_selectable"));
	}
	Finish();
}

void UGliderDropTestSubsystem::UpdateCamera() const
{
	using namespace GliderDropTestPrivate;
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera) || not M_Delivery.IsValid())
	{
		return;
	}
	const EGliderDeliveryPhase Phase = M_Delivery->GetDeliveryPhase();
	// Far enough back to keep the tug in view while towing; close in on the nose once the glider is down.
	const float Distance = Phase == EGliderDeliveryPhase::TowedApproach ? 9000.f
		                       : Phase == EGliderDeliveryPhase::Gliding ? 7000.f
		                       : 5200.f;
	const FVector Focus = M_Tank.IsValid() && static_cast<uint8>(Phase) >= static_cast<uint8>(EGliderDeliveryPhase::Lingering)
		                      ? (M_Tank->GetActorLocation() + M_Delivery->GetActorLocation()) * 0.5f
		                      : M_Delivery->GetActorLocation() + M_RunDirection * 1500.f;
	const FRotator ViewRotation(CameraPitch, M_RunDirection.Rotation().Yaw + CameraYawFromRun, 0.f);
	M_Camera->SetActorLocationAndRotation(Focus - ViewRotation.Vector() * Distance, ViewRotation);
	// The game's own camera takes the view back; it is handed to the test camera again every frame.
	Controller->SetViewTarget(M_Camera);
}

void UGliderDropTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("GliderDropTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
}

#else

bool UGliderDropTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UGliderDropTestSubsystem::Tick(float DeltaTime) {}
TStatId UGliderDropTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGliderDropTestSubsystem, STATGROUP_Tickables);
}

#endif
