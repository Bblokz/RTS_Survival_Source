// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "NetSpikePlayerController.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformTime.h"
#include "Net/UnrealNetwork.h"

#include "NetSpikeShared.h"
#include "RTS_Survival/Interfaces/Commands.h"

namespace NetSpikeControllerPrivate
{
	// Client rungs; the runner logs rungs 1 and 2 (connecting, map loaded).
	constexpr int32 RungControllerReceived = 3;
	constexpr int32 RungUnitReceived = 4;
	constexpr int32 RungOrderSent = 5;
	constexpr int32 RungSawMovement = 6;
	constexpr int32 RungFinalSent = 7;
	// Host rung of the order; the runner logs the other host rungs.
	constexpr int32 HostRungOrderReceived = 7;

	FString DescribeUnitOnClient(const AActor& Unit)
	{
		const UPrimitiveComponent* const RootPrimitive = Cast<UPrimitiveComponent>(Unit.GetRootComponent());
		const APawn* const UnitPawn = Cast<APawn>(&Unit);
		return FString::Printf(
			TEXT("class=%s local_role=%d root=%s root_simulates_physics=%d has_controller=%d"),
			*Unit.GetClass()->GetName(), static_cast<int32>(Unit.GetLocalRole()),
			RootPrimitive != nullptr ? *RootPrimitive->GetName() : TEXT("none"),
			RootPrimitive != nullptr && RootPrimitive->IsSimulatingPhysics() ? 1 : 0,
			UnitPawn != nullptr && UnitPawn->GetController() != nullptr ? 1 : 0);
	}
}

ANetSpikePlayerController::ANetSpikePlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ANetSpikePlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ANetSpikePlayerController, M_SpikeUnit);
	DOREPLIFETIME(ANetSpikePlayerController, M_MoveOffset);
}

void ANetSpikePlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_Client || not IsLocalController())
	{
		return;
	}
	using namespace NetSpikeControllerPrivate;
	NetSpike::LogRung(NetSpike::ClientSide, RungControllerReceived,
	                  FString::Printf(TEXT("controller_received class=%s"), *GetClass()->GetName()));
}

void ANetSpikePlayerController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() != NM_Client || not IsLocalController())
	{
		return;
	}
	TickClient();
}

void ANetSpikePlayerController::AssignSpikeUnit(AActor* Unit, const FVector& MoveOffset)
{
	if (not HasAuthority())
	{
		return;
	}
	M_SpikeUnit = Unit;
	M_MoveOffset = MoveOffset;
	ForceNetUpdate();
}

// ---------------------------------------------------------------------------------------------------------------------
// Host side: calls from the client
// ---------------------------------------------------------------------------------------------------------------------

void ANetSpikePlayerController::ServerOrderMove_Implementation(AActor* Unit, const FVector_NetQuantize10 Target)
{
	using namespace NetSpikeControllerPrivate;
	M_ServerRecord.bHasOrder = true;
	M_ServerRecord.OrderTarget = Target;
	M_ServerRecord.OrderRealSeconds = FPlatformTime::Seconds();
	// The spike's whole permission check: only the unit the host handed out. See the class note.
	if (not IsValid(Unit) || Unit != M_SpikeUnit)
	{
		NetSpike::LogRung(NetSpike::HostSide, HostRungOrderReceived, TEXT("order_received rejected=not_the_spike_unit"));
		return;
	}
	M_ServerRecord.UnitLocationAtOrder = Unit->GetActorLocation();
	M_ServerRecord.UnitYawAtOrder = Unit->GetActorRotation().Yaw;
	ICommands* const Commands = Cast<ICommands>(Unit);
	if (Commands == nullptr)
	{
		NetSpike::LogRung(NetSpike::HostSide, HostRungOrderReceived, TEXT("order_received rejected=unit_takes_no_orders"));
		return;
	}
	// The same entry point the player's own controller and the enemy AI use for a move order.
	const ECommandQueueError Result = Commands->MoveToLocation(Target, true, FRotator::ZeroRotator);
	M_ServerRecord.OrderResult = static_cast<int32>(Result);
	NetSpike::LogRung(NetSpike::HostSide, HostRungOrderReceived, FString::Printf(
		                  TEXT("order_received result_code=%d unit_at=%s target=%s"), M_ServerRecord.OrderResult,
		                  *M_ServerRecord.UnitLocationAtOrder.ToCompactString(),
		                  *M_ServerRecord.OrderTarget.ToCompactString()));
}

void ANetSpikePlayerController::ServerReportView_Implementation(const FVector_NetQuantize10 Location,
                                                                const float YawDegrees)
{
	const AActor* const Unit = M_SpikeUnit;
	if (not IsValid(Unit))
	{
		return;
	}
	const float ErrorCm = FVector::Dist(Unit->GetActorLocation(), Location);
	const float YawErrorDegrees = FMath::Abs(FMath::FindDeltaAngleDegrees(Unit->GetActorRotation().Yaw, YawDegrees));
	const bool bIsServerUnitDriving = Unit->GetVelocity().Size() > NetSpike::MovingSpeedCmPerSecond;
	if (not bIsServerUnitDriving)
	{
		++M_ServerRecord.RestingReportCount;
		M_ServerRecord.MaxRestingErrorCm = FMath::Max(M_ServerRecord.MaxRestingErrorCm, ErrorCm);
		return;
	}
	++M_ServerRecord.MovingReportCount;
	M_ServerRecord.SumMovingErrorCm += ErrorCm;
	M_ServerRecord.MaxMovingErrorCm = FMath::Max(M_ServerRecord.MaxMovingErrorCm, ErrorCm);
	M_ServerRecord.MaxYawErrorDegrees = FMath::Max(M_ServerRecord.MaxYawErrorDegrees, YawErrorDegrees);
}

void ANetSpikePlayerController::ServerReportFinal_Implementation(const FVector_NetQuantize10 Location,
                                                                 const float YawDegrees,
                                                                 const float MaxTurnedDegrees, const float MovedCm,
                                                                 const float PreOrderDriftCm)
{
	M_ServerRecord.ClientFinalLocation = Location;
	M_ServerRecord.ClientFinalYawDegrees = YawDegrees;
	M_ServerRecord.ClientMaxTurnedDegrees = MaxTurnedDegrees;
	M_ServerRecord.ClientMovedCm = MovedCm;
	M_ServerRecord.ClientPreOrderDriftCm = PreOrderDriftCm;
	const AActor* const Unit = M_SpikeUnit;
	if (IsValid(Unit))
	{
		M_ServerRecord.ServerLocationAtFinal = Unit->GetActorLocation();
		M_ServerRecord.ServerYawAtFinal = Unit->GetActorRotation().Yaw;
		M_ServerRecord.ServerSpeedAtFinal = Unit->GetVelocity().Size();
	}
	M_ServerRecord.bHasFinal = true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Client side: the test steps
// ---------------------------------------------------------------------------------------------------------------------

void ANetSpikePlayerController::TickClient()
{
	if (M_ClientState.bHasSentFinal)
	{
		return;
	}
	const double NowSeconds = FPlatformTime::Seconds();
	if (not M_ClientState.bHasUnit)
	{
		TryAdoptUnit(NowSeconds);
		return;
	}
	AActor* const Unit = M_SpikeUnit;
	if (not IsValid(Unit))
	{
		return;
	}
	if (not M_ClientState.bHasSentOrder)
	{
		TickClientBeforeOrder(NowSeconds, *Unit);
		return;
	}
	TickClientAfterOrder(NowSeconds, *Unit);
}

void ANetSpikePlayerController::TryAdoptUnit(const double NowSeconds)
{
	const AActor* const Unit = M_SpikeUnit;
	if (not IsValid(Unit))
	{
		return;
	}
	using namespace NetSpikeControllerPrivate;
	M_ClientState.bHasUnit = true;
	M_ClientState.FirstUnitLocation = Unit->GetActorLocation();
	M_ClientState.UnitSeenRealSeconds = NowSeconds;
	NetSpike::LogRung(NetSpike::ClientSide, RungUnitReceived, FString::Printf(
		                  TEXT("unit_received at=%s %s"), *M_ClientState.FirstUnitLocation.ToCompactString(),
		                  *DescribeUnitOnClient(*Unit)));
}

void ANetSpikePlayerController::TickClientBeforeOrder(const double NowSeconds, AActor& Unit)
{
	if (NowSeconds - M_ClientState.UnitSeenRealSeconds < NetSpike::ClientStillSeconds)
	{
		return;
	}
	using namespace NetSpikeControllerPrivate;
	const FVector UnitLocation = Unit.GetActorLocation();
	const FVector Target = UnitLocation + M_MoveOffset;
	M_ClientState.bHasSentOrder = true;
	M_ClientState.UnitLocationAtOrder = UnitLocation;
	M_ClientState.UnitYawAtOrder = Unit.GetActorRotation().Yaw;
	M_ClientState.OrderRealSeconds = NowSeconds;
	M_ClientState.PreOrderDriftCm = FVector::Dist(UnitLocation, M_ClientState.FirstUnitLocation);
	M_ClientState.LastRestCheckLocation = UnitLocation;
	M_ClientState.LastRestCheckRealSeconds = NowSeconds;
	ServerOrderMove(&Unit, Target);
	NetSpike::LogRung(NetSpike::ClientSide, RungOrderSent, FString::Printf(
		                  TEXT("order_sent unit_at=%s target=%s drift_before_order_cm=%.1f"),
		                  *UnitLocation.ToCompactString(), *Target.ToCompactString(),
		                  M_ClientState.PreOrderDriftCm));
}

void ANetSpikePlayerController::TickClientAfterOrder(const double NowSeconds, AActor& Unit)
{
	using namespace NetSpikeControllerPrivate;
	const FVector UnitLocation = Unit.GetActorLocation();
	const float MovedCm = FVector::Dist2D(UnitLocation, M_ClientState.UnitLocationAtOrder);
	M_ClientState.MaxMovedCm = FMath::Max(M_ClientState.MaxMovedCm, MovedCm);
	const float TurnedDegrees = FMath::Abs(
		FMath::FindDeltaAngleDegrees(M_ClientState.UnitYawAtOrder, Unit.GetActorRotation().Yaw));
	M_ClientState.MaxTurnedDegrees = FMath::Max(M_ClientState.MaxTurnedDegrees, TurnedDegrees);
	const double SecondsSinceOrder = NowSeconds - M_ClientState.OrderRealSeconds;
	if (not M_ClientState.bHasLoggedMovement && MovedCm > NetSpike::MovementSeenCm)
	{
		M_ClientState.bHasLoggedMovement = true;
		NetSpike::LogRung(NetSpike::ClientSide, RungSawMovement, FString::Printf(
			                  TEXT("saw_movement seconds_after_order=%.2f"), SecondsSinceOrder));
	}
	if (NowSeconds - M_ClientState.LastSampleLogRealSeconds >= NetSpike::SampleLogIntervalSeconds)
	{
		M_ClientState.LastSampleLogRealSeconds = NowSeconds;
		UE_LOG(LogRTSNetSpike, Display, TEXT("RTS_NETSPIKE client sample t=%.1f at=%s moved_cm=%.0f yaw=%.1f"),
		       SecondsSinceOrder, *UnitLocation.ToCompactString(), MovedCm, Unit.GetActorRotation().Yaw);
	}
	ReportViewAtInterval(NowSeconds, Unit);
	if (GetHasUnitComeToRest(NowSeconds, UnitLocation))
	{
		SendFinalAndExit(Unit, TEXT("unit_came_to_rest"));
		return;
	}
	if (SecondsSinceOrder > NetSpike::ClientMaxObserveSeconds)
	{
		SendFinalAndExit(Unit, TEXT("observe_time_over"));
	}
}

void ANetSpikePlayerController::ReportViewAtInterval(const double NowSeconds, const AActor& Unit)
{
	if (NowSeconds - M_ClientState.LastViewReportRealSeconds < NetSpike::ClientViewReportIntervalSeconds)
	{
		return;
	}
	M_ClientState.LastViewReportRealSeconds = NowSeconds;
	ServerReportView(Unit.GetActorLocation(), Unit.GetActorRotation().Yaw);
}

bool ANetSpikePlayerController::GetHasUnitComeToRest(const double NowSeconds, const FVector& UnitLocation)
{
	if (NowSeconds - M_ClientState.LastRestCheckRealSeconds < NetSpike::ClientRestCheckIntervalSeconds)
	{
		return false;
	}
	const float MovedSinceCheckCm = FVector::Dist(UnitLocation, M_ClientState.LastRestCheckLocation);
	M_ClientState.LastRestCheckLocation = UnitLocation;
	M_ClientState.LastRestCheckRealSeconds = NowSeconds;
	// A tank that has not left yet is also standing still: only a tank that travelled counts as arrived.
	return MovedSinceCheckCm < NetSpike::RestToleranceCm && M_ClientState.MaxMovedCm > NetSpike::MinTravelCm;
}

void ANetSpikePlayerController::SendFinalAndExit(const AActor& Unit, const TCHAR* Reason)
{
	using namespace NetSpikeControllerPrivate;
	const FVector UnitLocation = Unit.GetActorLocation();
	const float MovedCm = FVector::Dist2D(UnitLocation, M_ClientState.UnitLocationAtOrder);
	const float YawDegrees = Unit.GetActorRotation().Yaw;
	M_ClientState.bHasSentFinal = true;
	ServerReportFinal(UnitLocation, YawDegrees, M_ClientState.MaxTurnedDegrees, MovedCm,
	                  M_ClientState.PreOrderDriftCm);
	NetSpike::LogRung(NetSpike::ClientSide, RungFinalSent, FString::Printf(
		                  TEXT("final_sent reason=%s at=%s yaw=%.1f max_turned_deg=%.1f moved_cm=%.0f"), Reason,
		                  *UnitLocation.ToCompactString(), YawDegrees, M_ClientState.MaxTurnedDegrees, MovedCm));
	// From here on the host ends the session; the runner must not read that as a failed connection.
	NetSpike::MarkClientWorkDone();
	NetSpike::RequestProcessExit(NetSpike::ExitCodePass, NetSpike::ClientExitDelaySeconds);
}
