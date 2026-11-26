// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/Object.h"

#include "NetSpikeRunner.generated.h"

class ACPPController;
class AGameModeBase;
class ANetSpikePlayerController;
class UNetDriver;
class UWorld;

enum class ENetSpikeRole : uint8
{
	Host,
	Client
};

enum class ENetSpikeHostStep : uint8
{
	Travel,
	WaitForMap,
	StartMatch,
	SpawnTank,
	SettleTank,
	WaitForClient,
	WaitForOrder,
	WaitForFinal,
	Done
};

/**
 * @brief Drives one process of the LAN multiplayer spike. Created and rooted by the module when
 * -NetSpikeRole=Host or -NetSpikeRole=Client is on the command line.
 * The host opens the test map as a listen server with the map's normal game mode, starts the match, spawns one
 * tank, switches replication on for that one tank and judges the run. The client only connects; its controller
 * (ANetSpikePlayerController) does the rest.
 * @note The host's own player keeps the normal game classes. Only players that join later get the spike's
 * controller, a plain HUD and no camera pawn, set on the running game mode after the host has logged in.
 */
UCLASS()
class UNetSpikeRunner : public UObject
{
	GENERATED_BODY()

public:
	void Start(ENetSpikeRole Role);

private:
	bool Tick(float DeltaSeconds);

	void TickHost(double NowSeconds);
	void TickHost_Travel();
	void TickHost_WaitForMap(double NowSeconds);
	void TickHost_StartMatch(double NowSeconds);
	void TickHost_SpawnTank(double NowSeconds);
	void TickHost_SettleTank(double NowSeconds);
	void TickHost_WaitForClient();
	void TickHost_WaitForOrder();
	void TickHost_WaitForFinal(double NowSeconds);

	void TickClient();

	/** @brief Makes players that join from now on spike players; the host's own player is already in. */
	void SetJoiningPlayersToSpikeClasses(AGameModeBase& GameMode) const;
	bool TryStartMatch();
	/** @brief The start-game gate, the difficulty picker and info takeovers pause the game: keeps it running. */
	void KeepGameRunning() const;
	bool TryGetGroundZ(const UWorld& World, const FVector& Location, float& OutGroundZ) const;
	int32 GetListenPort(const UWorld& World) const;
	void LogHostSample(double NowSeconds, const AActor& Tank);
	void JudgeRun();
	void Finish(bool bPassed, const FString& Reasons);

	void OnPostLoadMap(UWorld* LoadedWorld);
	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType,
	                      const FString& ErrorString);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

	const TCHAR* GetSideName() const;

	ENetSpikeRole M_Role = ENetSpikeRole::Host;
	ENetSpikeHostStep M_HostStep = ENetSpikeHostStep::Travel;
	bool bM_HasClientConnectStarted = false;
	bool bM_IsFinished = false;
	int32 M_TickCount = 0;
	double M_StartRealSeconds = 0.0;
	double M_MapReadyRealSeconds = 0.0;
	double M_TankSpawnRealSeconds = 0.0;
	double M_LastSampleLogRealSeconds = 0.0;
	FString M_ClientAddress;

	// Where the tank stood after it settled, before any client joined.
	FVector M_SettledLocation = FVector::ZeroVector;
	float M_MaxServerMovedCm = 0.f;
	float M_MaxServerTurnedDegrees = 0.f;
	bool bM_HasLoggedServerMovement = false;

	TWeakObjectPtr<ACPPController> M_HostController;
	TWeakObjectPtr<AActor> M_Tank;
	TWeakObjectPtr<ANetSpikePlayerController> M_ClientController;

	FTSTicker::FDelegateHandle M_TickerHandle;
};
