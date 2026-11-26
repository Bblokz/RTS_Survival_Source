// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/PlayerController.h"

#include "NetSpikePlayerController.generated.h"

/** What the host learned from the calls of the joining client. Lives on the host's copy of the controller only. */
struct FNetSpikeServerRecord
{
	bool bHasOrder = false;
	// ECommandQueueError of the move order as an int, or INDEX_NONE when the order never reached a unit.
	int32 OrderResult = INDEX_NONE;
	FVector OrderTarget = FVector::ZeroVector;
	FVector UnitLocationAtOrder = FVector::ZeroVector;
	float UnitYawAtOrder = 0.f;
	double OrderRealSeconds = 0.0;

	// Position reports the client sent while the host's tank was driving.
	int32 MovingReportCount = 0;
	float MaxMovingErrorCm = 0.f;
	double SumMovingErrorCm = 0.0;
	float MaxYawErrorDegrees = 0.f;
	// Position reports the client sent while the host's tank stood still.
	int32 RestingReportCount = 0;
	float MaxRestingErrorCm = 0.f;

	bool bHasFinal = false;
	FVector ClientFinalLocation = FVector::ZeroVector;
	float ClientFinalYawDegrees = 0.f;
	float ClientMaxTurnedDegrees = 0.f;
	float ServerYawAtFinal = 0.f;
	float ClientMovedCm = 0.f;
	float ClientPreOrderDriftCm = 0.f;
	FVector ServerLocationAtFinal = FVector::ZeroVector;
	float ServerSpeedAtFinal = 0.f;
};

/** Progress of the joining client's own test steps. Lives on the client's copy of the controller only. */
struct FNetSpikeClientState
{
	bool bHasUnit = false;
	FVector FirstUnitLocation = FVector::ZeroVector;
	double UnitSeenRealSeconds = 0.0;

	bool bHasSentOrder = false;
	FVector UnitLocationAtOrder = FVector::ZeroVector;
	float UnitYawAtOrder = 0.f;
	double OrderRealSeconds = 0.0;
	float PreOrderDriftCm = 0.f;

	float MaxMovedCm = 0.f;
	float MaxTurnedDegrees = 0.f;
	bool bHasLoggedMovement = false;
	FVector LastRestCheckLocation = FVector::ZeroVector;
	double LastRestCheckRealSeconds = 0.0;
	double LastViewReportRealSeconds = 0.0;
	double LastSampleLogRealSeconds = 0.0;

	bool bHasSentFinal = false;
};

/**
 * @brief Player controller of the joining player in the LAN multiplayer spike. The host keeps its normal
 * ACPPController; only the second player gets this class.
 * The host hands it one replicated unit. The client's copy then waits, orders that unit to move through a server
 * call, and reports where it sees the unit, so the host can compare both views.
 * @note A real multiplayer mode must check that the caller owns the unit it orders. The spike only accepts the one
 * unit the host handed out.
 */
UCLASS()
class ANetSpikePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANetSpikePlayerController();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * @brief Host only: gives the client the unit it may order and how far to send it.
	 * @param Unit Replicated unit on the host.
	 * @param MoveOffset World offset from the unit's position to the move target.
	 */
	void AssignSpikeUnit(AActor* Unit, const FVector& MoveOffset);

	const FNetSpikeServerRecord& GetServerRecord() const { return M_ServerRecord; }

private:
	UFUNCTION(Server, Reliable)
	void ServerOrderMove(AActor* Unit, FVector_NetQuantize10 Target);

	UFUNCTION(Server, Unreliable)
	void ServerReportView(FVector_NetQuantize10 Location, float YawDegrees);

	UFUNCTION(Server, Reliable)
	void ServerReportFinal(FVector_NetQuantize10 Location, float YawDegrees, float MaxTurnedDegrees, float MovedCm,
	                       float PreOrderDriftCm);

	void TickClient();
	void TryAdoptUnit(double NowSeconds);
	void TickClientBeforeOrder(double NowSeconds, AActor& Unit);
	void TickClientAfterOrder(double NowSeconds, AActor& Unit);
	void ReportViewAtInterval(double NowSeconds, const AActor& Unit);
	bool GetHasUnitComeToRest(double NowSeconds, const FVector& UnitLocation);
	void SendFinalAndExit(const AActor& Unit, const TCHAR* Reason);

	// The one unit the host allows this client to order. Null on the client until the unit itself has replicated.
	UPROPERTY(Replicated)
	TObjectPtr<AActor> M_SpikeUnit;

	UPROPERTY(Replicated)
	FVector M_MoveOffset = FVector::ZeroVector;

	FNetSpikeServerRecord M_ServerRecord;
	FNetSpikeClientState M_ClientState;
};
