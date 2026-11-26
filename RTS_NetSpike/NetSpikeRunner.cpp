// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "NetSpikeRunner.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

#include "NetSpikePlayerController.h"
#include "NetSpikeShared.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

namespace NetSpikeRunnerPrivate
{
	// Host rungs; rung 7 (order received) is logged by the controller on the host.
	constexpr int32 HostRungMapLoaded = 1;
	constexpr int32 HostRungMatchStarted = 2;
	constexpr int32 HostRungTankSpawned = 3;
	constexpr int32 HostRungListening = 4;
	constexpr int32 HostRungClientConnected = 5;
	constexpr int32 HostRungTankHanded = 6;
	constexpr int32 HostRungServerMoved = 8;
	// Client rungs of the runner; the controller logs the later ones.
	constexpr int32 ClientRungConnecting = 1;
	constexpr int32 ClientRungMapLoaded = 2;

	const TCHAR* const DefaultClientAddress = TEXT("127.0.0.1:7777");
	// Reflected: protected on AGameModeBase, and the spike adds no subclass of the game's own game mode.
	const FName StartAsSpectatorsPropertyName(TEXT("bStartPlayersAsSpectators"));
	// Button of UW_GameDifficultyPicker the skirmish harness also presses.
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));

	FString DescribeUnitOnHost(const AActor& Unit)
	{
		const UPrimitiveComponent* const RootPrimitive = Cast<UPrimitiveComponent>(Unit.GetRootComponent());
		return FString::Printf(
			TEXT("class=%s root=%s root_simulates_physics=%d replicates=%d replicates_movement=%d"),
			*Unit.GetClass()->GetName(), RootPrimitive != nullptr ? *RootPrimitive->GetName() : TEXT("none"),
			RootPrimitive != nullptr && RootPrimitive->IsSimulatingPhysics() ? 1 : 0, Unit.GetIsReplicated() ? 1 : 0,
			Unit.IsReplicatingMovement() ? 1 : 0);
	}
}

void UNetSpikeRunner::Start(const ENetSpikeRole Role)
{
	M_Role = Role;
	M_StartRealSeconds = FPlatformTime::Seconds();
	FParse::Value(FCommandLine::Get(), TEXT("NetSpikeAddress="), M_ClientAddress);
	if (M_ClientAddress.IsEmpty())
	{
		M_ClientAddress = NetSpikeRunnerPrivate::DefaultClientAddress;
	}
	// Editor builds show a dialog for every reported error; this process has nobody to click it.
	RTSFunctionLibrary::PushModalDialogSuppression();
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UNetSpikeRunner::OnPostLoadMap);
	GEngine->OnNetworkFailure().AddUObject(this, &UNetSpikeRunner::OnNetworkFailure);
	GEngine->OnTravelFailure().AddUObject(this, &UNetSpikeRunner::OnTravelFailure);
	M_TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UNetSpikeRunner::Tick));
	UE_LOG(LogRTSNetSpike, Display, TEXT("RTS_NETSPIKE %s runner_started"), GetSideName());
}

bool UNetSpikeRunner::Tick(const float DeltaSeconds)
{
	if (bM_IsFinished)
	{
		return false;
	}
	++M_TickCount;
	const double NowSeconds = FPlatformTime::Seconds();
	const bool bIsHost = M_Role == ENetSpikeRole::Host;
	const double DeadlineSeconds = bIsHost ? NetSpike::HostDeadlineSeconds : NetSpike::ClientDeadlineSeconds;
	if (NowSeconds - M_StartRealSeconds > DeadlineSeconds)
	{
		Finish(false, FString::Printf(TEXT("timeout_at_host_step_%d"), static_cast<int32>(M_HostStep)));
		return false;
	}
	if (bIsHost)
	{
		TickHost(NowSeconds);
	}
	else
	{
		TickClient();
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Host
// ---------------------------------------------------------------------------------------------------------------------

void UNetSpikeRunner::TickHost(const double NowSeconds)
{
	if (M_HostStep > ENetSpikeHostStep::StartMatch)
	{
		KeepGameRunning();
	}
	switch (M_HostStep)
	{
	case ENetSpikeHostStep::Travel:
		TickHost_Travel();
		break;
	case ENetSpikeHostStep::WaitForMap:
		TickHost_WaitForMap(NowSeconds);
		break;
	case ENetSpikeHostStep::StartMatch:
		TickHost_StartMatch(NowSeconds);
		break;
	case ENetSpikeHostStep::SpawnTank:
		TickHost_SpawnTank(NowSeconds);
		break;
	case ENetSpikeHostStep::SettleTank:
		TickHost_SettleTank(NowSeconds);
		break;
	case ENetSpikeHostStep::WaitForClient:
		TickHost_WaitForClient();
		break;
	case ENetSpikeHostStep::WaitForOrder:
		TickHost_WaitForOrder();
		break;
	case ENetSpikeHostStep::WaitForFinal:
		TickHost_WaitForFinal(NowSeconds);
		break;
	case ENetSpikeHostStep::Done:
		break;
	}
}

void UNetSpikeRunner::TickHost_Travel()
{
	UWorld* const World = NetSpike::GetGameWorld();
	if (World == nullptr || M_TickCount < NetSpike::TicksBeforeTravel)
	{
		return;
	}
	// "listen" opens the net driver; the map keeps its own game mode, so the host plays the normal game.
	const FString TravelUrl = FString::Printf(TEXT("%s?listen"), NetSpike::MapPath);
	GEngine->SetClientTravel(World, *TravelUrl, TRAVEL_Absolute);
	M_HostStep = ENetSpikeHostStep::WaitForMap;
}

void UNetSpikeRunner::TickHost_WaitForMap(const double NowSeconds)
{
	using namespace NetSpikeRunnerPrivate;
	UWorld* const World = NetSpike::GetGameWorld();
	if (World == nullptr || not World->GetMapName().Contains(NetSpike::MapShortName) || not World->HasBegunPlay())
	{
		return;
	}
	AGameModeBase* const GameMode = World->GetAuthGameMode();
	if (not IsValid(GameMode))
	{
		return;
	}
	SetJoiningPlayersToSpikeClasses(*GameMode);
	M_MapReadyRealSeconds = NowSeconds;
	M_HostStep = ENetSpikeHostStep::StartMatch;
	NetSpike::LogRung(NetSpike::HostSide, HostRungMapLoaded, FString::Printf(
		                  TEXT("map_loaded map=%s game_mode=%s net_mode=%d"), *World->GetMapName(),
		                  *GameMode->GetClass()->GetName(), static_cast<int32>(World->GetNetMode())));
}

void UNetSpikeRunner::SetJoiningPlayersToSpikeClasses(AGameModeBase& GameMode) const
{
	GameMode.PlayerControllerClass = ANetSpikePlayerController::StaticClass();
	GameMode.HUDClass = AHUD::StaticClass();
	const FBoolProperty* const SpectatorsProperty = FindFProperty<FBoolProperty>(
		AGameModeBase::StaticClass(), NetSpikeRunnerPrivate::StartAsSpectatorsPropertyName);
	if (SpectatorsProperty != nullptr)
	{
		SpectatorsProperty->SetPropertyValue_InContainer(&GameMode, true);
		return;
	}
	// Without the spectator switch the joining player would get the game's camera pawn, which expects the game's
	// own controller.
	GameMode.DefaultPawnClass = nullptr;
	UE_LOG(LogRTSNetSpike, Warning, TEXT("RTS_NETSPIKE host spectator switch not found, cleared the pawn class"));
}

void UNetSpikeRunner::TickHost_StartMatch(const double NowSeconds)
{
	if (NowSeconds - M_MapReadyRealSeconds < NetSpike::HudBuildSeconds || not TryStartMatch())
	{
		return;
	}
	M_HostStep = ENetSpikeHostStep::SpawnTank;
	NetSpike::LogRung(NetSpike::HostSide, NetSpikeRunnerPrivate::HostRungMatchStarted, FString::Printf(
		                  TEXT("match_started host_controller=%s"), *M_HostController->GetClass()->GetName()));
}

bool UNetSpikeRunner::TryStartMatch()
{
	const UWorld* const World = NetSpike::GetGameWorld();
	if (World == nullptr)
	{
		return false;
	}
	ACPPController* const HostController = Cast<ACPPController>(World->GetFirstPlayerController());
	if (not IsValid(HostController))
	{
		return false;
	}
	M_HostController = HostController;
	UPlayerStartGameControl* const StartGameControl = HostController->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	return true;
}

void UNetSpikeRunner::KeepGameRunning() const
{
	UWorld* const World = NetSpike::GetGameWorld();
	if (World == nullptr)
	{
		return;
	}
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(
			Picker->WidgetTree->FindWidget(NetSpikeRunnerPrivate::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	ACPPController* const HostController = M_HostController.Get();
	if (IsValid(HostController) && HostController->GetIsGameOnPause())
	{
		HostController->PauseAndLockGame(false);
		HostController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

void UNetSpikeRunner::TickHost_SpawnTank(const double NowSeconds)
{
	UWorld* const World = NetSpike::GetGameWorld();
	if (World == nullptr)
	{
		return;
	}
	UClass* const TankClass = LoadClass<AActor>(nullptr, NetSpike::TankClassPath);
	if (not IsValid(TankClass))
	{
		Finish(false, TEXT("tank_class_not_found"));
		return;
	}
	FVector SpawnLocation = NetSpike::ArenaLocation;
	float GroundZ = SpawnLocation.Z;
	const bool bFoundGround = TryGetGroundZ(*World, SpawnLocation, GroundZ);
	SpawnLocation.Z = GroundZ + NetSpike::TankSpawnLiftCm;
	AActor* const Tank = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*World, TankClass, FTransform(FRotator::ZeroRotator, SpawnLocation),
		static_cast<uint8>(RTSPlayers::HumanPlayer));
	if (not IsValid(Tank))
	{
		Finish(false, TEXT("tank_spawn_failed"));
		return;
	}
	// The whole networking change to the game's tank: three flags on this one spawned actor, nothing on its class.
	// Always relevant, because the joining player has no camera near the tank for the distance check.
	Tank->bAlwaysRelevant = true;
	Tank->SetReplicates(true);
	Tank->SetReplicateMovement(true);
	M_Tank = Tank;
	M_TankSpawnRealSeconds = NowSeconds;
	M_HostStep = ENetSpikeHostStep::SettleTank;
	NetSpike::LogRung(NetSpike::HostSide, NetSpikeRunnerPrivate::HostRungTankSpawned, FString::Printf(
		                  TEXT("tank_spawned at=%s found_ground=%d %s"), *SpawnLocation.ToCompactString(),
		                  bFoundGround ? 1 : 0, *NetSpikeRunnerPrivate::DescribeUnitOnHost(*Tank)));
}

bool UNetSpikeRunner::TryGetGroundZ(const UWorld& World, const FVector& Location, float& OutGroundZ) const
{
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(NetSpikeGround), false);
	FHitResult GroundHit;
	const FVector TraceStart(Location.X, Location.Y, Location.Z + NetSpike::GroundTraceUpCm);
	const FVector TraceEnd(Location.X, Location.Y, Location.Z - NetSpike::GroundTraceDownCm);
	if (not World.LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE, QueryParams))
	{
		return false;
	}
	OutGroundZ = GroundHit.ImpactPoint.Z;
	return true;
}

void UNetSpikeRunner::TickHost_SettleTank(const double NowSeconds)
{
	const AActor* const Tank = M_Tank.Get();
	const UWorld* const World = NetSpike::GetGameWorld();
	if (not IsValid(Tank) || World == nullptr)
	{
		Finish(false, TEXT("tank_lost_while_settling"));
		return;
	}
	if (NowSeconds - M_TankSpawnRealSeconds < NetSpike::TankSettleSeconds)
	{
		return;
	}
	const int32 ListenPort = GetListenPort(*World);
	if (ListenPort == INDEX_NONE)
	{
		Finish(false, TEXT("host_is_not_listening"));
		return;
	}
	M_SettledLocation = Tank->GetActorLocation();
	M_HostStep = ENetSpikeHostStep::WaitForClient;
	// The launch script starts the client when it reads this line.
	NetSpike::LogRung(NetSpike::HostSide, NetSpikeRunnerPrivate::HostRungListening, FString::Printf(
		                  TEXT("listening port=%d tank_settled_at=%s %s"), ListenPort,
		                  *M_SettledLocation.ToCompactString(), *NetSpikeRunnerPrivate::DescribeUnitOnHost(*Tank)));
}

int32 UNetSpikeRunner::GetListenPort(const UWorld& World) const
{
	UNetDriver* const NetDriver = World.GetNetDriver();
	if (not IsValid(NetDriver) || World.GetNetMode() != NM_ListenServer)
	{
		return INDEX_NONE;
	}
	const TSharedPtr<const FInternetAddr> LocalAddress = NetDriver->GetLocalAddr();
	return LocalAddress.IsValid() ? LocalAddress->GetPort() : World.URL.Port;
}

void UNetSpikeRunner::TickHost_WaitForClient()
{
	using namespace NetSpikeRunnerPrivate;
	UWorld* const World = NetSpike::GetGameWorld();
	AActor* const Tank = M_Tank.Get();
	if (World == nullptr || not IsValid(Tank))
	{
		Finish(false, TEXT("tank_lost_before_client_joined"));
		return;
	}
	ANetSpikePlayerController* ClientController = nullptr;
	for (TActorIterator<ANetSpikePlayerController> Iterator(World); Iterator; ++Iterator)
	{
		ClientController = *Iterator;
		break;
	}
	if (not IsValid(ClientController))
	{
		return;
	}
	M_ClientController = ClientController;
	NetSpike::LogRung(NetSpike::HostSide, HostRungClientConnected, FString::Printf(
		                  TEXT("client_connected controller=%s player_controllers=%d first_is_host=%d"),
		                  *ClientController->GetName(), World->GetNumPlayerControllers(),
		                  World->GetFirstPlayerController() == M_HostController.Get() ? 1 : 0));
	ClientController->AssignSpikeUnit(Tank, NetSpike::MoveOffset);
	M_HostStep = ENetSpikeHostStep::WaitForOrder;
	NetSpike::LogRung(NetSpike::HostSide, HostRungTankHanded, FString::Printf(
		                  TEXT("tank_handed at=%s drift_since_settled_cm=%.1f"),
		                  *Tank->GetActorLocation().ToCompactString(),
		                  FVector::Dist(Tank->GetActorLocation(), M_SettledLocation)));
}

void UNetSpikeRunner::TickHost_WaitForOrder()
{
	const ANetSpikePlayerController* const ClientController = M_ClientController.Get();
	if (not IsValid(ClientController))
	{
		Finish(false, TEXT("client_left_before_order"));
		return;
	}
	if (ClientController->GetServerRecord().bHasOrder)
	{
		M_HostStep = ENetSpikeHostStep::WaitForFinal;
	}
}

void UNetSpikeRunner::TickHost_WaitForFinal(const double NowSeconds)
{
	const ANetSpikePlayerController* const ClientController = M_ClientController.Get();
	const AActor* const Tank = M_Tank.Get();
	if (not IsValid(ClientController) || not IsValid(Tank))
	{
		Finish(false, TEXT("client_or_tank_lost_after_order"));
		return;
	}
	const FNetSpikeServerRecord& Record = ClientController->GetServerRecord();
	const float MovedCm = FVector::Dist2D(Tank->GetActorLocation(), Record.UnitLocationAtOrder);
	M_MaxServerMovedCm = FMath::Max(M_MaxServerMovedCm, MovedCm);
	const float TurnedDegrees = FMath::Abs(
		FMath::FindDeltaAngleDegrees(Record.UnitYawAtOrder, Tank->GetActorRotation().Yaw));
	M_MaxServerTurnedDegrees = FMath::Max(M_MaxServerTurnedDegrees, TurnedDegrees);
	if (not bM_HasLoggedServerMovement && MovedCm > NetSpike::MovementSeenCm)
	{
		bM_HasLoggedServerMovement = true;
		NetSpike::LogRung(NetSpike::HostSide, NetSpikeRunnerPrivate::HostRungServerMoved, FString::Printf(
			                  TEXT("server_moved seconds_after_order=%.2f"), NowSeconds - Record.OrderRealSeconds));
	}
	LogHostSample(NowSeconds, *Tank);
	if (Record.bHasFinal)
	{
		JudgeRun();
	}
}

void UNetSpikeRunner::LogHostSample(const double NowSeconds, const AActor& Tank)
{
	if (NowSeconds - M_LastSampleLogRealSeconds < NetSpike::SampleLogIntervalSeconds)
	{
		return;
	}
	M_LastSampleLogRealSeconds = NowSeconds;
	const ANetSpikePlayerController* const ClientController = M_ClientController.Get();
	const double OrderSeconds = IsValid(ClientController) ? ClientController->GetServerRecord().OrderRealSeconds : 0.0;
	UE_LOG(LogRTSNetSpike, Display, TEXT("RTS_NETSPIKE host sample t=%.1f at=%s speed=%.0f yaw=%.1f"),
	       NowSeconds - OrderSeconds, *Tank.GetActorLocation().ToCompactString(), Tank.GetVelocity().Size(),
	       Tank.GetActorRotation().Yaw);
}

void UNetSpikeRunner::JudgeRun()
{
	const ANetSpikePlayerController* const ClientController = M_ClientController.Get();
	if (not IsValid(ClientController))
	{
		Finish(false, TEXT("client_lost_before_judging"));
		return;
	}
	const FNetSpikeServerRecord& Record = ClientController->GetServerRecord();
	const float ServerPreOrderDriftCm = FVector::Dist(Record.UnitLocationAtOrder, M_SettledLocation);
	const float FinalErrorCm = FVector::Dist(Record.ClientFinalLocation, Record.ServerLocationAtFinal);
	const float DistanceToTargetCm = FVector::Dist2D(Record.ServerLocationAtFinal, Record.OrderTarget);
	const float FinalYawErrorDegrees = FMath::Abs(
		FMath::FindDeltaAngleDegrees(Record.ServerYawAtFinal, Record.ClientFinalYawDegrees));
	const double AverageMovingErrorCm = Record.MovingReportCount > 0
		                                    ? Record.SumMovingErrorCm / Record.MovingReportCount
		                                    : 0.0;
	UE_LOG(LogRTSNetSpike, Display,
	       TEXT("RTS_NETSPIKE metrics order_result_code=%d server_moved_cm=%.0f client_moved_cm=%.0f "
		       "final_error_cm=%.1f final_yaw_error_deg=%.1f server_max_turned_deg=%.1f client_max_turned_deg=%.1f "
		       "server_final_yaw=%.1f server_distance_to_target_cm=%.0f server_speed_at_final=%.0f "
		       "moving_reports=%d moving_error_avg_cm=%.1f moving_error_max_cm=%.1f moving_yaw_error_max_deg=%.1f "
		       "resting_reports=%d resting_error_max_cm=%.1f server_drift_before_order_cm=%.1f "
		       "client_drift_before_order_cm=%.1f"),
	       Record.OrderResult, M_MaxServerMovedCm, Record.ClientMovedCm, FinalErrorCm, FinalYawErrorDegrees,
	       M_MaxServerTurnedDegrees, Record.ClientMaxTurnedDegrees, Record.ServerYawAtFinal, DistanceToTargetCm,
	       Record.ServerSpeedAtFinal, Record.MovingReportCount, AverageMovingErrorCm, Record.MaxMovingErrorCm,
	       Record.MaxYawErrorDegrees, Record.RestingReportCount, Record.MaxRestingErrorCm, ServerPreOrderDriftCm,
	       Record.ClientPreOrderDriftCm);

	TArray<FString> Reasons;
	if (Record.OrderResult != static_cast<int32>(ECommandQueueError::NoError))
	{
		Reasons.Add(FString::Printf(TEXT("order_refused_code_%d"), Record.OrderResult));
	}
	if (ServerPreOrderDriftCm > NetSpike::StillToleranceCm)
	{
		Reasons.Add(TEXT("server_tank_moved_before_order"));
	}
	if (Record.ClientPreOrderDriftCm > NetSpike::StillToleranceCm)
	{
		Reasons.Add(TEXT("client_tank_moved_before_order"));
	}
	if (M_MaxServerMovedCm < NetSpike::MinTravelCm)
	{
		Reasons.Add(TEXT("server_tank_did_not_move"));
	}
	if (Record.ClientMovedCm < NetSpike::MinTravelCm)
	{
		Reasons.Add(TEXT("client_did_not_see_the_move"));
	}
	if (FinalErrorCm > NetSpike::FinalToleranceCm)
	{
		Reasons.Add(TEXT("final_positions_differ"));
	}
	if (M_MaxServerTurnedDegrees < NetSpike::MinTurnDegrees)
	{
		Reasons.Add(TEXT("server_tank_did_not_turn"));
	}
	if (Record.ClientMaxTurnedDegrees < NetSpike::MinTurnDegrees)
	{
		Reasons.Add(TEXT("client_did_not_see_the_turn"));
	}
	if (FinalYawErrorDegrees > NetSpike::FinalYawToleranceDegrees)
	{
		Reasons.Add(TEXT("final_facings_differ"));
	}
	Finish(Reasons.IsEmpty(), FString::Join(Reasons, TEXT(",")));
}

// ---------------------------------------------------------------------------------------------------------------------
// Client
// ---------------------------------------------------------------------------------------------------------------------

void UNetSpikeRunner::TickClient()
{
	if (bM_HasClientConnectStarted || M_TickCount < NetSpike::TicksBeforeTravel)
	{
		return;
	}
	UWorld* const World = NetSpike::GetGameWorld();
	if (World == nullptr)
	{
		return;
	}
	bM_HasClientConnectStarted = true;
	NetSpike::LogRung(NetSpike::ClientSide, NetSpikeRunnerPrivate::ClientRungConnecting,
	                  FString::Printf(TEXT("connecting address=%s"), *M_ClientAddress));
	GEngine->SetClientTravel(World, *M_ClientAddress, TRAVEL_Absolute);
}

// ---------------------------------------------------------------------------------------------------------------------
// Both sides
// ---------------------------------------------------------------------------------------------------------------------

void UNetSpikeRunner::OnPostLoadMap(UWorld* LoadedWorld)
{
	if (M_Role != ENetSpikeRole::Client || not bM_HasClientConnectStarted || LoadedWorld == nullptr
		|| NetSpike::GetIsClientWorkDone())
	{
		return;
	}
	NetSpike::LogRung(NetSpike::ClientSide, NetSpikeRunnerPrivate::ClientRungMapLoaded, FString::Printf(
		                  TEXT("map_loaded map=%s net_mode=%d"), *LoadedWorld->GetMapName(),
		                  static_cast<int32>(LoadedWorld->GetNetMode())));
}

void UNetSpikeRunner::OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, const ENetworkFailure::Type FailureType,
                                       const FString& ErrorString)
{
	if (M_Role == ENetSpikeRole::Client && NetSpike::GetIsClientWorkDone())
	{
		UE_LOG(LogRTSNetSpike, Display, TEXT("RTS_NETSPIKE client session_ended_by_host"));
		return;
	}
	UE_LOG(LogRTSNetSpike, Error, TEXT("RTS_NETSPIKE %s network_failure type=%s error=%s"), GetSideName(),
	       ENetworkFailure::ToString(FailureType), *ErrorString);
	if (M_Role == ENetSpikeRole::Client)
	{
		Finish(false, TEXT("network_failure"));
	}
}

void UNetSpikeRunner::OnTravelFailure(UWorld* World, const ETravelFailure::Type FailureType,
                                      const FString& ErrorString)
{
	if (M_Role == ENetSpikeRole::Client && NetSpike::GetIsClientWorkDone())
	{
		return;
	}
	UE_LOG(LogRTSNetSpike, Error, TEXT("RTS_NETSPIKE %s travel_failure type=%s error=%s"), GetSideName(),
	       ETravelFailure::ToString(FailureType), *ErrorString);
	Finish(false, TEXT("travel_failure"));
}

void UNetSpikeRunner::Finish(const bool bPassed, const FString& Reasons)
{
	if (bM_IsFinished)
	{
		return;
	}
	bM_IsFinished = true;
	M_HostStep = ENetSpikeHostStep::Done;
	UE_LOG(LogRTSNetSpike, Display, TEXT("RTS_NETSPIKE %s RESULT %s reasons=%s"), GetSideName(),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), Reasons.IsEmpty() ? TEXT("none") : *Reasons);
	NetSpike::RequestProcessExit(bPassed ? NetSpike::ExitCodePass : NetSpike::ExitCodeFail, NetSpike::ExitDelaySeconds);
}

const TCHAR* UNetSpikeRunner::GetSideName() const
{
	return M_Role == ENetSpikeRole::Host ? NetSpike::HostSide : NetSpike::ClientSide;
}
