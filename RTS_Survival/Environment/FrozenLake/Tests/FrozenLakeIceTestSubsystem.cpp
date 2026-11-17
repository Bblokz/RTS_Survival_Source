// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FrozenLakeIceTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "ShaderCompiler.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSFrozenLakeIceTest, Log, All);

namespace FrozenLakeIceTestPrivate
{
	const TCHAR* const TestSwitch = TEXT("FrozenLakeIceTest");
	const TCHAR* const DirectorySwitch = TEXT("FrozenLakeIceTestDir=");
	const TCHAR* const HeavyTankClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/TigerII/Ger_Vehicle_TigerII.Ger_Vehicle_TigerII_C");
	const TCHAR* const MediumTankClassPath =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H.Ger_Vehicle_PanzerIV_H_C");
	constexpr uint8 PlayerOwner = 1;

	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 120.0;
	constexpr double MaxShaderCompileSeconds = 1200.0;
	constexpr double MaxNavigationSeconds = 90.0;
	constexpr int32 NavigationQuietFrames = 60;
	// Long enough for the capture's auto exposure to settle.
	constexpr int32 ExposureFrames = 150;
	constexpr int32 SwapSettleFrames = 10;
	constexpr double TankSettleSeconds = 1.0;
	constexpr double MaxObserveSeconds = 40.0;
	// After the heavy is under: wait for the hole's nav box to be built into the navmesh, then look at the path.
	constexpr double NavRebuildSeconds = 4.0;
	constexpr double AfterSunkSeconds = 6.0;

	constexpr float SpotSearchStepCm = 800.f;
	constexpr float SpotClearRadiusCm = 1400.f;
	constexpr int32 SpotRingSamples = 12;
	constexpr float MinSpotSeparationCm = 3500.f;
	constexpr float SpotHeightAboveIceCm = 60.f;
	// The path probe runs this far either side of the heavy's spot, along world X.
	constexpr float PathProbeHalfLengthCm = 2500.f;
	const FVector NavExtent(500.f, 500.f, 1000.f);
	// A path round the hole must keep at least this share of the hole radius from its centre.
	constexpr float HoleClearanceFraction = 0.8f;
	// Mean per-channel difference (0-255) allowed between the authored state mask and the runtime copy.
	constexpr float MaxStateMaskDifference = 3.f;
	constexpr float CaptureFieldOfView = 70.f;
}

bool UFrozenLakeIceTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), FrozenLakeIceTestPrivate::TestSwitch);
}

void UFrozenLakeIceTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	M_OutputDirectory = FPaths::ProjectSavedDir() / TEXT("FrozenLakeIceTest");
	FParse::Value(FCommandLine::Get(), FrozenLakeIceTestPrivate::DirectorySwitch, M_OutputDirectory);
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	EnterPhase(EFrozenLakeIceTestPhase::WaitingForGameStart);
	UE_LOG(LogRTSFrozenLakeIceTest, Display, TEXT("RTS_FL_ICE_TEST started, output %s"), *M_OutputDirectory);
}

void UFrozenLakeIceTestSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

void UFrozenLakeIceTestSubsystem::Tick(const float DeltaTime)
{
	if (M_Phase == EFrozenLakeIceTestPhase::Inactive || M_Phase == EFrozenLakeIceTestPhase::Done ||
		not IsValid(GetWorld()))
	{
		return;
	}
	if (M_Phase != EFrozenLakeIceTestPhase::WaitingForGameStart)
	{
		FGrenadeTestHelpers::KeepGameRunning(*GetWorld(), M_PlayerController.Get());
		if (UGameplayStatics::IsGamePaused(GetWorld()))
		{
			UGameplayStatics::SetGamePaused(GetWorld(), false);
		}
	}
	++M_FramesInPhase;
	LogHeartbeat();
	switch (M_Phase)
	{
	case EFrozenLakeIceTestPhase::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case EFrozenLakeIceTestPhase::WaitingForNavigation:
		TickWaitingForNavigation();
		break;
	case EFrozenLakeIceTestPhase::ComparingStateMask:
		TickComparingStateMask();
		break;
	case EFrozenLakeIceTestPhase::WaitingForTanks:
		TickWaitingForTanks();
		break;
	case EFrozenLakeIceTestPhase::Observing:
		TickObserving();
		break;
	default:
		break;
	}
}

TStatId UFrozenLakeIceTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFrozenLakeIceTestSubsystem, STATGROUP_Tickables);
}

void UFrozenLakeIceTestSubsystem::LogHeartbeat()
{
	constexpr double HeartbeatSeconds = 15.0;
	const double Now = FPlatformTime::Seconds();
	if (Now - M_LastHeartbeatSeconds < HeartbeatSeconds)
	{
		return;
	}
	M_LastHeartbeatSeconds = Now;
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	UE_LOG(LogRTSFrozenLakeIceTest, Display,
	       TEXT("RTS_FL_ICE_TEST heartbeat phase=%s in_phase=%.0f s shader_jobs=%d nav_building=%s"),
	       *UEnum::GetValueAsString(M_Phase), GetSecondsInPhase(),
	       GShaderCompilingManager != nullptr ? GShaderCompilingManager->GetNumRemainingJobs() : -1,
	       IsValid(NavigationSystem) && NavigationSystem->IsNavigationBuildInProgress() ? TEXT("yes") : TEXT("no"));
}

UFrozenLakeIceSubsystem* UFrozenLakeIceTestSubsystem::GetIceSubsystem() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetSubsystem<UFrozenLakeIceSubsystem>() : nullptr;
}

void UFrozenLakeIceTestSubsystem::EnterPhase(const EFrozenLakeIceTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = FPlatformTime::Seconds();
	M_FramesInPhase = 0;
}

double UFrozenLakeIceTestSubsystem::GetSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartSeconds;
}

double UFrozenLakeIceTestSubsystem::GetObservedSeconds() const
{
	return FPlatformTime::Seconds() - M_ObserveStartSeconds;
}

void UFrozenLakeIceTestSubsystem::Finish()
{
	M_Phase = EFrozenLakeIceTestPhase::Done;
	for (const FString& Failure : M_Failures)
	{
		UE_LOG(LogRTSFrozenLakeIceTest, Display, TEXT("RTS_FL_ICE_TEST FAIL_REASON %s"), *Failure);
	}
	UE_LOG(LogRTSFrozenLakeIceTest, Display, TEXT("RTS_FL_ICE_TEST RESULT %s (%d failures) output %s"),
	       M_Failures.IsEmpty() ? TEXT("PASS") : TEXT("FAIL"), M_Failures.Num(), *M_OutputDirectory);
	FPlatformMisc::RequestExit(false, TEXT("FrozenLakeIceTest"));
}

void UFrozenLakeIceTestSubsystem::TickWaitingForGameStart()
{
	using namespace FrozenLakeIceTestPrivate;
	if (GetSecondsInPhase() > MaxStartSeconds)
	{
		M_Failures.Add(TEXT("the game did not start"));
		Finish();
		return;
	}
	if (GetSecondsInPhase() < HudBuildSeconds || not FGrenadeTestHelpers::TryStartGame(*GetWorld(), M_PlayerController))
	{
		return;
	}
	const UFrozenLakeIceSubsystem* Ice = GetIceSubsystem();
	if (Ice == nullptr || not Ice->GetIsActive())
	{
		M_Failures.Add(TEXT("the frozen lake ice subsystem is not active on this map"));
		Finish();
		return;
	}
	EnterPhase(EFrozenLakeIceTestPhase::WaitingForNavigation);
}

void UFrozenLakeIceTestSubsystem::TickWaitingForNavigation()
{
	using namespace FrozenLakeIceTestPrivate;
	const int32 RemainingShaderJobs = GShaderCompilingManager != nullptr
		                                  ? GShaderCompilingManager->GetNumRemainingJobs()
		                                  : 0;
	if (RemainingShaderJobs > 0 && GetSecondsInPhase() < MaxShaderCompileSeconds)
	{
		M_FramesInPhase = 0;
		return;
	}
	UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const bool bIsBuilding = IsValid(NavigationSystem) && NavigationSystem->IsNavigationBuildInProgress();
	if (bIsBuilding && GetSecondsInPhase() < MaxNavigationSeconds)
	{
		M_FramesInPhase = 0;
		return;
	}
	if (not bIsBuilding && M_FramesInPhase < NavigationQuietFrames)
	{
		return;
	}
	if (not FindSpots())
	{
		Finish();
		return;
	}
	ASceneCapture2D* Capture = nullptr;
	UTextureRenderTarget2D* CaptureTarget = nullptr;
	if (FGrenadeTestHelpers::CreateCaptureCamera(*GetWorld(), CaptureFieldOfView, Capture, CaptureTarget))
	{
		M_Capture = Capture;
		M_CaptureTarget = CaptureTarget;
		FGrenadeTestHelpers::AimLikeGameCamera(*Capture, M_GreySpot);
	}
	EnterPhase(EFrozenLakeIceTestPhase::ComparingStateMask);
}

void UFrozenLakeIceTestSubsystem::TickComparingStateMask()
{
	using namespace FrozenLakeIceTestPrivate;
	UFrozenLakeIceSubsystem* Ice = GetIceSubsystem();
	if (Ice == nullptr || not IsValid(M_CaptureTarget))
	{
		M_Failures.Add(TEXT("no capture for the state mask comparison"));
		Finish();
		return;
	}
	// Authored texture first (exposure settles on it), then the runtime copy the holes are drawn into: they must match.
	if (M_FramesInPhase == 1)
	{
		Ice->SetShowAuthoredStateMask(true);
		return;
	}
	if (M_FramesInPhase == ExposureFrames)
	{
		M_CaptureTarget->GameThread_GetRenderTargetResource()->ReadPixels(M_AuthoredPixels);
		SavePicture(TEXT("00_state_authored.png"));
		Ice->SetShowAuthoredStateMask(false);
		return;
	}
	if (M_FramesInPhase < ExposureFrames + SwapSettleFrames)
	{
		return;
	}
	SavePicture(TEXT("01_state_runtime.png"));
	const float Difference = CompareWithAuthoredPixels();
	UE_LOG(LogRTSFrozenLakeIceTest, Display, TEXT("RTS_FL_ICE_TEST state_mask_difference mean=%.2f (of 255)"),
	       Difference);
	if (Difference < 0.f || Difference > MaxStateMaskDifference)
	{
		M_Failures.Add(FString::Printf(TEXT("runtime state mask differs from the authored one by %.2f"), Difference));
	}
	SpawnTanks();
	EnterPhase(EFrozenLakeIceTestPhase::WaitingForTanks);
}

void UFrozenLakeIceTestSubsystem::TickWaitingForTanks()
{
	using namespace FrozenLakeIceTestPrivate;
	if (GetSecondsInPhase() < TankSettleSeconds)
	{
		return;
	}
	const UFrozenLakeIceSubsystem* Ice = GetIceSubsystem();
	for (FFrozenLakeIceTestTank& TestTank : M_Tanks)
	{
		ATankMaster* Tank = TestTank.Tank.Get();
		if (not IsValid(Tank))
		{
			M_Failures.Add(FString::Printf(TEXT("%s did not spawn"), *TestTank.Label));
			continue;
		}
		TestTank.Agent = Tank->GetRTSNavAgentType();
		const bool bTooWeak = Ice != nullptr && Ice->GetIsIceTooWeakFor(TestTank.Agent, Tank->GetActorLocation());
		UE_LOG(LogRTSFrozenLakeIceTest, Display,
		       TEXT("RTS_FL_ICE_TEST tank %s class=%s agent=%s at=%s ice_too_weak=%s should_break=%s"),
		       *TestTank.Label, *Tank->GetClass()->GetName(), *Global_GetRTSNavAgentName(TestTank.Agent),
		       *Tank->GetActorLocation().ToCompactString(), bTooWeak ? TEXT("yes") : TEXT("no"),
		       TestTank.bShouldBreak ? TEXT("yes") : TEXT("no"));
		if (bTooWeak != TestTank.bShouldBreak)
		{
			M_Failures.Add(FString::Printf(TEXT("%s stands on the wrong ice (too weak: %d)"), *TestTank.Label,
			                               bTooWeak));
		}
	}
	LogPathsAcrossGreyIce();
	float ClosestToSpot = 0.f;
	M_PathBeforeLength = MeasurePathAcrossSpot(ClosestToSpot);
	UE_LOG(LogRTSFrozenLakeIceTest, Display,
	       TEXT("RTS_FL_ICE_TEST path_before medium length=%.0f closest_to_spot=%.0f"), M_PathBeforeLength,
	       ClosestToSpot);
	M_ObserveStartSeconds = FPlatformTime::Seconds();
	QueuePicture(0.2, TEXT("02_spawned.png"));
	EnterPhase(EFrozenLakeIceTestPhase::Observing);
}

void UFrozenLakeIceTestSubsystem::TickObserving()
{
	using namespace FrozenLakeIceTestPrivate;
	TakeDuePictures();
	const FFrozenLakeIceTestTank* Heavy = M_Tanks.IsValidIndex(0) ? &M_Tanks[0] : nullptr;
	const bool bHeavySunk = Heavy != nullptr && Heavy->SunkSeconds >= 0.0;
	if (bHeavySunk && not bM_HasCheckedHolePath && GetObservedSeconds() > Heavy->SunkSeconds + NavRebuildSeconds)
	{
		CheckHolePath();
	}
	const bool bIsDone = bHeavySunk && GetObservedSeconds() > Heavy->SunkSeconds + AfterSunkSeconds &&
		M_PendingPictures.IsEmpty();
	if (not bIsDone && GetObservedSeconds() < MaxObserveSeconds)
	{
		return;
	}
	CheckResults();
	Finish();
}

bool UFrozenLakeIceTestSubsystem::FindIceSpot(const bool bWantGreyIce, const FVector& AwayFrom,
                                              FVector& OutSpot) const
{
	using namespace FrozenLakeIceTestPrivate;
	const UFrozenLakeIceSubsystem* Ice = GetIceSubsystem();
	const UStaticMeshComponent* Surface = Ice != nullptr ? Ice->GetIceSurface() : nullptr;
	if (Surface == nullptr)
	{
		return false;
	}
	const FBox Bounds = Surface->Bounds.GetBox();
	const FVector Centre = Bounds.GetCenter();
	// Closest qualifying spot to the lake centre: inside the camera-friendly middle, away from the shore.
	float BestDistance = TNumericLimits<float>::Max();
	for (float X = Bounds.Min.X; X <= Bounds.Max.X; X += SpotSearchStepCm)
	{
		for (float Y = Bounds.Min.Y; Y <= Bounds.Max.Y; Y += SpotSearchStepCm)
		{
			const FVector Spot(X, Y, Ice->GetIceSurfaceZ() + SpotHeightAboveIceCm);
			const bool bTooClose = not AwayFrom.IsZero() && FVector::Dist2D(Spot, AwayFrom) < MinSpotSeparationCm;
			const float Distance = FVector::Dist2D(Spot, Centre);
			if (bTooClose || Distance >= BestDistance || not GetIsSpotOfKind(*Ice, Spot, bWantGreyIce))
			{
				continue;
			}
			BestDistance = Distance;
			OutSpot = Spot;
		}
	}
	return BestDistance < TNumericLimits<float>::Max();
}

bool UFrozenLakeIceTestSubsystem::GetIsSpotOfKind(const UFrozenLakeIceSubsystem& Ice, const FVector& Spot,
                                                  const bool bWantGreyIce) const
{
	using namespace FrozenLakeIceTestPrivate;
	const UWorld* World = GetWorld();
	for (int32 Sample = 0; Sample <= SpotRingSamples; ++Sample)
	{
		const float Angle = 2.f * PI * Sample / SpotRingSamples;
		const float Reach = Sample == SpotRingSamples ? 0.f : SpotClearRadiusCm;
		const FVector Probe = Spot + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Reach;
		const bool bHeavyHeldOut = Ice.GetIsIceTooWeakFor(ERTSNavAgents::HeavyTank, Probe);
		const bool bMediumHeldOut = Ice.GetIsIceTooWeakFor(ERTSNavAgents::MediumTank, Probe);
		const bool bIsKind = bWantGreyIce ? bHeavyHeldOut && not bMediumHeldOut : not bHeavyHeldOut;
		if (not bIsKind)
		{
			return false;
		}
		// Ice under the probe, not shore: the first hit downwards is the ice surface.
		FHitResult Hit;
		const FVector Top = Probe + FVector(0.f, 0.f, SpotHeightAboveIceCm * 10.f);
		const FVector Bottom = Probe - FVector(0.f, 0.f, SpotHeightAboveIceCm * 10.f);
		if (not IsValid(World) || not World->LineTraceSingleByChannel(Hit, Top, Bottom, ECC_Visibility) ||
			Hit.GetComponent() != Ice.GetIceSurface())
		{
			return false;
		}
	}
	return true;
}

bool UFrozenLakeIceTestSubsystem::FindSpots()
{
	if (not FindIceSpot(true, FVector::ZeroVector, M_GreySpot))
	{
		M_Failures.Add(TEXT("no grey-ice spot found"));
		return false;
	}
	if (not FindIceSpot(true, M_GreySpot, M_GreySpotControl))
	{
		M_Failures.Add(TEXT("no second grey-ice spot found"));
		return false;
	}
	if (not FindIceSpot(false, M_GreySpot, M_HeavySpot))
	{
		M_Failures.Add(TEXT("no heavy-ice spot found"));
		return false;
	}
	UE_LOG(LogRTSFrozenLakeIceTest, Display, TEXT("RTS_FL_ICE_TEST spots grey=%s grey_control=%s heavy=%s"),
	       *M_GreySpot.ToCompactString(), *M_GreySpotControl.ToCompactString(), *M_HeavySpot.ToCompactString());
	return true;
}

void UFrozenLakeIceTestSubsystem::SpawnTanks()
{
	using namespace FrozenLakeIceTestPrivate;
	if (UFrozenLakeIceSubsystem* Ice = GetIceSubsystem())
	{
		Ice->OnVehicleStateChanged().AddUObject(this, &UFrozenLakeIceTestSubsystem::OnVehicleStateChanged);
	}
	// The heavy is placed on grey ice: orders never path a heavy there (the nav keeps it out).
	SpawnTestTank(TEXT("heavy_on_grey"), HeavyTankClassPath, M_GreySpot, true);
	SpawnTestTank(TEXT("medium_on_grey"), MediumTankClassPath, M_GreySpotControl, false);
	SpawnTestTank(TEXT("heavy_on_heavy"), HeavyTankClassPath, M_HeavySpot, false);
}

void UFrozenLakeIceTestSubsystem::SpawnTestTank(const FString& Label, const TCHAR* ClassPath, const FVector& Spot,
                                                const bool bShouldBreak)
{
	using namespace FrozenLakeIceTestPrivate;
	FFrozenLakeIceTestTank& TestTank = M_Tanks.AddDefaulted_GetRef();
	TestTank.Label = Label;
	TestTank.bShouldBreak = bShouldBreak;
	AActor* Spawned = FGrenadeTestHelpers::SpawnUnitForPlayer(*GetWorld(), ClassPath,
	                                                           FTransform(FRotator(0.f, 30.f, 0.f), Spot), PlayerOwner);
	TestTank.Tank = Cast<ATankMaster>(Spawned);
}

void UFrozenLakeIceTestSubsystem::OnVehicleStateChanged(ATankMaster* Tank, const EFrozenLakeIceVehicleState NewState)
{
	for (FFrozenLakeIceTestTank& TestTank : M_Tanks)
	{
		if (TestTank.Tank.Get() != Tank)
		{
			continue;
		}
		TestTank.LastState = NewState;
		const double Now = GetObservedSeconds();
		UE_LOG(LogRTSFrozenLakeIceTest, Display, TEXT("RTS_FL_ICE_TEST state %s -> %s at %.2f s"), *TestTank.Label,
		       *UEnum::GetValueAsString(NewState), Now);
		if (NewState == EFrozenLakeIceVehicleState::Warning)
		{
			TestTank.WarningSeconds = Now;
			QueuePicture(Now + 1.2, FString::Printf(TEXT("03_%s_warning.png"), *TestTank.Label));
		}
		else if (NewState == EFrozenLakeIceVehicleState::Sinking)
		{
			TestTank.SinkingSeconds = Now;
			QueuePicture(Now + 0.4, FString::Printf(TEXT("04_%s_break.png"), *TestTank.Label));
			QueuePicture(Now + 3.0, FString::Printf(TEXT("05_%s_sinking.png"), *TestTank.Label));
		}
		else if (NewState == EFrozenLakeIceVehicleState::Sunk)
		{
			TestTank.SunkSeconds = Now;
			QueuePicture(Now + 2.5, FString::Printf(TEXT("06_%s_hole.png"), *TestTank.Label));
		}
	}
}

void UFrozenLakeIceTestSubsystem::QueuePicture(const double AfterSeconds, const FString& FileName)
{
	M_PendingPictures.Add(TPair<double, FString>(AfterSeconds, FileName));
}

void UFrozenLakeIceTestSubsystem::TakeDuePictures()
{
	const double Now = GetObservedSeconds();
	for (int32 Index = M_PendingPictures.Num() - 1; Index >= 0; --Index)
	{
		if (M_PendingPictures[Index].Key > Now)
		{
			continue;
		}
		SavePicture(M_PendingPictures[Index].Value);
		M_PendingPictures.RemoveAt(Index);
	}
}

void UFrozenLakeIceTestSubsystem::SavePicture(const FString& FileName) const
{
	if (not IsValid(M_CaptureTarget) || not IsValid(GetWorld()))
	{
		return;
	}
	FGrenadeTestHelpers::SaveCapture(*GetWorld(), *M_CaptureTarget, M_OutputDirectory, FileName);
	UE_LOG(LogRTSFrozenLakeIceTest, Display, TEXT("RTS_FL_ICE_TEST picture %s"), *FileName);
}

float UFrozenLakeIceTestSubsystem::CompareWithAuthoredPixels() const
{
	if (not IsValid(M_CaptureTarget) || M_AuthoredPixels.IsEmpty())
	{
		return -1.f;
	}
	TArray<FColor> RuntimePixels;
	M_CaptureTarget->GameThread_GetRenderTargetResource()->ReadPixels(RuntimePixels);
	if (RuntimePixels.Num() != M_AuthoredPixels.Num())
	{
		return -1.f;
	}
	double Sum = 0.0;
	for (int32 Index = 0; Index < RuntimePixels.Num(); ++Index)
	{
		const FColor& A = RuntimePixels[Index];
		const FColor& B = M_AuthoredPixels[Index];
		Sum += FMath::Abs(A.R - B.R) + FMath::Abs(A.G - B.G) + FMath::Abs(A.B - B.B);
	}
	return static_cast<float>(Sum / (3.0 * RuntimePixels.Num()));
}

ANavigationData* UFrozenLakeIceTestSubsystem::FindNavDataForAgent(const ERTSNavAgents Agent) const
{
	const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (not IsValid(NavigationSystem))
	{
		return nullptr;
	}
	const FString AgentName = Global_GetRTSNavAgentName(Agent);
	for (ANavigationData* NavData : NavigationSystem->NavDataSet)
	{
		if (IsValid(NavData) && NavData->GetConfig().Name.ToString() == AgentName)
		{
			return NavData;
		}
	}
	return nullptr;
}

void UFrozenLakeIceTestSubsystem::LogPathsAcrossGreyIce() const
{
	using namespace FrozenLakeIceTestPrivate;
	UWorld* World = GetWorld();
	// From the heavy-ice spot into the grey ice: light and medium must get there in game, heavy must not.
	for (const ERTSNavAgents Agent : {ERTSNavAgents::Character, ERTSNavAgents::LightTank, ERTSNavAgents::MediumTank,
	                                 ERTSNavAgents::HeavyTank})
	{
		ANavigationData* NavData = FindNavDataForAgent(Agent);
		FVector Start = M_HeavySpot;
		FVector End = M_GreySpot;
		FGrenadeTestHelpers::ProjectOntoNavMesh(*World, Start, NavExtent);
		const UNavigationPath* Path = NavData != nullptr
			                              ? UNavigationSystemV1::FindPathToLocationSynchronously(World, Start, End,
				                              NavData)
			                              : nullptr;
		const bool bHasPath = IsValid(Path) && Path->PathPoints.Num() > 1;
		const float EndMiss = bHasPath ? FVector::Dist2D(Path->PathPoints.Last(), End) : -1.f;
		UE_LOG(LogRTSFrozenLakeIceTest, Display,
		       TEXT("RTS_FL_ICE_TEST nav_into_grey agent=%s navdata=%s path=%s partial=%s end_miss=%.0f"),
		       *Global_GetRTSNavAgentName(Agent), NavData != nullptr ? *NavData->GetName() : TEXT("none"),
		       bHasPath ? TEXT("yes") : TEXT("no"), bHasPath && Path->IsPartial() ? TEXT("yes") : TEXT("no"),
		       EndMiss);
	}
}

float UFrozenLakeIceTestSubsystem::MeasurePathAcrossSpot(float& OutClosestToSpot) const
{
	using namespace FrozenLakeIceTestPrivate;
	OutClosestToSpot = -1.f;
	ANavigationData* NavData = FindNavDataForAgent(ERTSNavAgents::MediumTank);
	UWorld* World = GetWorld();
	if (NavData == nullptr || not IsValid(World))
	{
		return -1.f;
	}
	FVector Start = M_GreySpot - FVector(PathProbeHalfLengthCm, 0.f, 0.f);
	FVector End = M_GreySpot + FVector(PathProbeHalfLengthCm, 0.f, 0.f);
	FGrenadeTestHelpers::ProjectOntoNavMesh(*World, Start, NavExtent);
	FGrenadeTestHelpers::ProjectOntoNavMesh(*World, End, NavExtent);
	const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Start, End, NavData);
	if (not IsValid(Path) || Path->PathPoints.Num() < 2 || Path->IsPartial())
	{
		return -1.f;
	}
	float Length = 0.f;
	OutClosestToSpot = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index + 1 < Path->PathPoints.Num(); ++Index)
	{
		const FVector A = Path->PathPoints[Index];
		const FVector B = Path->PathPoints[Index + 1];
		Length += FVector::Dist2D(A, B);
		const FVector2D Spot2D(M_GreySpot);
		const FVector2D Closest = FMath::ClosestPointOnSegment2D(Spot2D, FVector2D(A), FVector2D(B));
		OutClosestToSpot = FMath::Min(OutClosestToSpot, static_cast<float>(FVector2D::Distance(Closest, Spot2D)));
	}
	return Length;
}

void UFrozenLakeIceTestSubsystem::CheckHolePath()
{
	using namespace FrozenLakeIceTestPrivate;
	bM_HasCheckedHolePath = true;
	const UFrozenLakeIceSubsystem* Ice = GetIceSubsystem();
	const float HoleRadius = Ice != nullptr && not Ice->GetHoles().IsEmpty() ? Ice->GetHoles()[0].Radius : 0.f;
	const FVector2D HoleCentre = Ice != nullptr && not Ice->GetHoles().IsEmpty()
		                             ? Ice->GetHoles()[0].Centre
		                             : FVector2D(M_GreySpot);
	M_GreySpot = FVector(HoleCentre, M_GreySpot.Z);
	float ClosestToHole = 0.f;
	const float Length = MeasurePathAcrossSpot(ClosestToHole);
	UE_LOG(LogRTSFrozenLakeIceTest, Display,
	       TEXT("RTS_FL_ICE_TEST path_after medium length=%.0f (before %.0f) closest_to_hole=%.0f hole_radius=%.0f"),
	       Length, M_PathBeforeLength, ClosestToHole, HoleRadius);
	if (HoleRadius <= 0.f)
	{
		M_Failures.Add(TEXT("no hole was made"));
		return;
	}
	if (Length >= 0.f && ClosestToHole < HoleRadius * HoleClearanceFraction)
	{
		M_Failures.Add(FString::Printf(TEXT("the medium path still crosses the hole (closest %.0f, radius %.0f)"),
		                               ClosestToHole, HoleRadius));
	}
}

void UFrozenLakeIceTestSubsystem::CheckResults()
{
	for (const FFrozenLakeIceTestTank& TestTank : M_Tanks)
	{
		ATankMaster* Tank = TestTank.Tank.Get();
		const bool bAlive = IsValid(Tank) && Tank->IsUnitAlive();
		const bool bBroke = TestTank.SinkingSeconds >= 0.0;
		const bool bSunk = TestTank.SunkSeconds >= 0.0;
		UE_LOG(LogRTSFrozenLakeIceTest, Display,
		       TEXT("RTS_FL_ICE_TEST result %s agent=%s warning=%.2f break=%.2f sunk=%.2f alive=%s last_state=%s"),
		       *TestTank.Label, *Global_GetRTSNavAgentName(TestTank.Agent), TestTank.WarningSeconds,
		       TestTank.SinkingSeconds, TestTank.SunkSeconds, bAlive ? TEXT("yes") : TEXT("no"),
		       *UEnum::GetValueAsString(TestTank.LastState));
		if (TestTank.bShouldBreak && (not bSunk || bAlive))
		{
			M_Failures.Add(FString::Printf(TEXT("%s did not go through the ice and die"), *TestTank.Label));
		}
		if (not TestTank.bShouldBreak && (bBroke || not bAlive))
		{
			M_Failures.Add(FString::Printf(TEXT("%s broke the ice or died but should not"), *TestTank.Label));
		}
		const bool bVisibleAboveIce = IsValid(Tank) && not Tank->IsHidden() && Tank->GetActorLocation().Z > 0.f;
		if (TestTank.bShouldBreak && bVisibleAboveIce)
		{
			M_Failures.Add(FString::Printf(TEXT("%s is still visible above the ice"), *TestTank.Label));
		}
	}
	if (not bM_HasCheckedHolePath)
	{
		M_Failures.Add(TEXT("the path round the hole was never checked"));
	}
}

#else

bool UFrozenLakeIceTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UFrozenLakeIceTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UFrozenLakeIceTestSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UFrozenLakeIceTestSubsystem::Tick(float DeltaTime)
{
}

TStatId UFrozenLakeIceTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFrozenLakeIceTestSubsystem, STATGROUP_Tickables);
}

#endif
