// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Mission_BerlinStayUndetected.h"

#include "RTS_Survival/Enemy/EnemyWaves/WaveSpawner/EnemyWaveSpawner.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace BerlinStayUndetectedConstants
{
	// The human player's number in the unit manager.
	constexpr uint8 HumanPlayer = 1;
}

void UMission_BerlinStayUndetected::OnTriggerAreaCallback(AActor* OverlappingActor, const int32 TriggerID,
                                                         ATriggerArea* TriggerVolume)
{
	// Recorded first: the Blueprint may raise the alarm (OnMissionFailed) from inside this callback.
	RecordAlarmCandidate(OverlappingActor, TriggerID);
	Super::OnTriggerAreaCallback(OverlappingActor, TriggerID, TriggerVolume);
}

void UMission_BerlinStayUndetected::RecordAlarmCandidate(const AActor* OverlappingActor, const int32 TriggerID)
{
	if (bM_HasRespondedToAlarm || not IsValid(OverlappingActor))
	{
		return;
	}
	if (TriggerID >= AlarmFirstZoneTriggerId)
	{
		M_AlarmCause = EBerlinAlarmCause::Spotted;
		M_AlarmLocation = OverlappingActor->GetActorLocation();
		return;
	}
	if (TriggerID == AlarmRelayBunkerTriggerId)
	{
		M_AlarmCause = EBerlinAlarmCause::RelayBunker;
		M_AlarmLocation = OverlappingActor->GetActorLocation();
		return;
	}
	if (TriggerID == AlarmForecourtTriggerId)
	{
		// The runner is a Soviet guard; the patrols go for the player units it ran from.
		M_AlarmCause = EBerlinAlarmCause::RunnerArrived;
		M_AlarmLocation = GetClosestPlayerUnitLocation(OverlappingActor->GetActorLocation());
	}
}

void UMission_BerlinStayUndetected::OnMissionFailed()
{
	RespondToAlarm();
	Super::OnMissionFailed();
}

void UMission_BerlinStayUndetected::RespondToAlarm()
{
	if (bM_HasRespondedToAlarm || M_AlarmCause == EBerlinAlarmCause::None)
	{
		return;
	}
	bM_HasRespondedToAlarm = true;
	// Played before Super fails the stage, so it queues ahead of the advisor's alarm line (ResponseForce).
	USoundBase* const Line = GetLineForCause(M_AlarmCause);
	if (IsValid(Line))
	{
		PlayPortrait(AlarmLines.Portrait, Line);
	}
	AEnemyWaveSpawner::ConvergeAllPatrolsOnAlarm(this, GetPatrolTargetLocation(), PatrolCombatSecondsBeforeReturn,
	                                             PatrolCombatCheckDelaySeconds);
}

FVector UMission_BerlinStayUndetected::GetPatrolTargetLocation() const
{
	if (not bPatrolsAttackPlayerHQ)
	{
		return M_AlarmLocation;
	}
	const ACPPController* const PlayerController = FRTS_Statics::GetRTSController(this);
	ANomadicVehicle* const HQ = IsValid(PlayerController) ? PlayerController->GetPlayerHq() : nullptr;
	if (RTSFunctionLibrary::RTSIsValid(HQ))
	{
		return HQ->GetActorLocation();
	}
	// The controller may not know the HQ (map without profile units): look for a deployed or moving one.
	for (ANomadicVehicle* const Nomadic : GetPlayerNomadicsOfType(ENomadicSubtype::Nomadic_GerHq))
	{
		if (RTSFunctionLibrary::RTSIsValid(Nomadic))
		{
			return Nomadic->GetActorLocation();
		}
	}
	return M_AlarmLocation;
}

USoundBase* UMission_BerlinStayUndetected::GetLineForCause(const EBerlinAlarmCause Cause) const
{
	switch (Cause)
	{
	case EBerlinAlarmCause::Spotted:
		return AlarmLines.SpottedLine;
	case EBerlinAlarmCause::RunnerArrived:
		return AlarmLines.RunnerArrivedLine;
	case EBerlinAlarmCause::RelayBunker:
		return AlarmLines.RelayBunkerLine;
	default:
		return nullptr;
	}
}

FVector UMission_BerlinStayUndetected::GetClosestPlayerUnitLocation(const FVector& From) const
{
	const UGameUnitManager* const UnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(UnitManager))
	{
		return From;
	}
	TArray<AActor*> PlayerUnits;
	for (ASquadUnit* const SquadUnit : UnitManager->GetSquadUnitsOfPlayer(BerlinStayUndetectedConstants::HumanPlayer))
	{
		PlayerUnits.Add(SquadUnit);
	}
	for (ATankMaster* const Tank : UnitManager->GetPlayerTanks(BerlinStayUndetectedConstants::HumanPlayer))
	{
		PlayerUnits.Add(Tank);
	}
	FVector ClosestLocation = From;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	for (const AActor* const Unit : PlayerUnits)
	{
		if (not IsValid(Unit))
		{
			continue;
		}
		const float DistanceSquared = FVector::DistSquared2D(Unit->GetActorLocation(), From);
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestLocation = Unit->GetActorLocation();
		}
	}
	return ClosestLocation;
}
