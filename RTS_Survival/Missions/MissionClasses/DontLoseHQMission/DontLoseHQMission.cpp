#include "DontLoseHQMission.h"

#include "RTS_Survival/Audio/RTSVoiceLines/RTSVoicelines.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Missions/Defeat/RTSDefeatType.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Types/MovePlayerCameraTypes.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace DontLoseHQMissionConstants
{
	constexpr int32 RetryFindHQDelaySeconds = 2;
	constexpr int32 MaxFindHQAttempts = 5;
}

void UDontLoseHQMission::OnMissionStart()
{
	if (GetIsMissionComplete())
	{
		return;
	}

	Super::OnMissionStart();
	if (not MissionState.bIsTriggered)
	{
		return;
	}

	M_HQFindAttempts = 0;
	bM_HasTriggeredHQLossFlow = false;
	M_PlayerHQ = nullptr;
	TryFindAndBindPlayerHQ();
}

void UDontLoseHQMission::OnMissionComplete()
{
	bM_HasTriggeredHQLossFlow = true;
	ClearMissionTimers();
	Super::OnMissionComplete();
}

void UDontLoseHQMission::OnMissionFailed()
{
	bM_HasTriggeredHQLossFlow = true;
	ClearMissionTimers();
	Super::OnMissionFailed();
}

void UDontLoseHQMission::ClearMissionTimers()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_RetryFindHQHandle);
	World->GetTimerManager().ClearTimer(M_DefeatAfterHQLostHandle);
}

void UDontLoseHQMission::TryFindAndBindPlayerHQ()
{
	UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		RTSFunctionLibrary::ReportError("DontLoseHQMission failed finding the player HQ because GameUnitManager is invalid.");
		return;
	}

	ANomadicVehicle* PlayerHQ = GameUnitManager->GetPlayerHQ();
	if (not IsValid(PlayerHQ))
	{
		HandleNoPlayerHQFound();
		return;
	}

	M_PlayerHQ = PlayerHQ;
	M_LastKnownHQPosition = PlayerHQ->GetActorLocation();
	BindToPlayerHQDeath();
}

void UDontLoseHQMission::HandleNoPlayerHQFound()
{
	M_HQFindAttempts += 1;
	if (M_HQFindAttempts >= DontLoseHQMissionConstants::MaxFindHQAttempts)
	{
		RTSFunctionLibrary::ReportError("DontLoseHQMission failed to find the player HQ after maximum retries.");
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError("DontLoseHQMission could not schedule the player HQ retry because world is invalid.");
		return;
	}

	World->GetTimerManager().SetTimer(
		M_RetryFindHQHandle,
		this,
		&UDontLoseHQMission::HandleRetryFindPlayerHQ,
		DontLoseHQMissionConstants::RetryFindHQDelaySeconds,
		false);
}

void UDontLoseHQMission::BindToPlayerHQDeath()
{
	if (not GetIsValidPlayerHQ())
	{
		return;
	}

	M_PlayerHQ->OnUnitDies.AddUObject(this, &UDontLoseHQMission::HandlePlayerHQDied);
}

void UDontLoseHQMission::HandlePlayerHQDied()
{
	if (GetIsMissionComplete() || bM_HasTriggeredHQLossFlow)
	{
		return;
	}
	bM_HasTriggeredHQLossFlow = true;

	ANomadicVehicle* PlayerHQ = M_PlayerHQ.Get();
	if (IsValid(PlayerHQ))
	{
		M_LastKnownHQPosition = PlayerHQ->GetActorLocation();
	}

	StartHQLossCameraPan();
	TriggerHQLossAnnouncerVoiceLine();
	ScheduleMissionDefeat();
}

void UDontLoseHQMission::StartHQLossCameraPan()
{
	FMovePlayerCamera CameraMove;
	CameraMove.MoveToLocation = M_LastKnownHQPosition;
	CameraMove.TimeToMove = M_CameraPanTimeToHQDeathLocation;
	CameraMove.TimeCameraInputDisabled = M_CameraPanTimeToHQDeathLocation;
	MoveCamera(CameraMove);
}

void UDontLoseHQMission::TriggerHQLossAnnouncerVoiceLine() const
{
	PlayPlayerAnnouncerLine(EAnnouncerVoiceLineType::DefeatLostHQ);
}

void UDontLoseHQMission::ScheduleMissionDefeat()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError("DontLoseHQMission could not schedule defeat because world is invalid.");
		return;
	}

	const float DefeatDelaySeconds = FMath::Max(0.0f, M_DefeatDelayAfterHQLost);
	if (DefeatDelaySeconds <= KINDA_SMALL_NUMBER)
	{
		HandleTriggerDefeatDelayed();
		return;
	}

	World->GetTimerManager().SetTimer(
		M_DefeatAfterHQLostHandle,
		this,
		&UDontLoseHQMission::HandleTriggerDefeatDelayed,
		DefeatDelaySeconds,
		false);
}

void UDontLoseHQMission::HandleRetryFindPlayerHQ()
{
	if (GetIsMissionComplete())
	{
		return;
	}

	TryFindAndBindPlayerHQ();
}

void UDontLoseHQMission::HandleTriggerDefeatDelayed()
{
	if (GetIsMissionComplete())
	{
		return;
	}

	AMissionManager* MissionManager = GetMissionManagerChecked();
	if (not IsValid(MissionManager))
	{
		RTSFunctionLibrary::ReportError("DontLoseHQMission failed triggering defeat because mission manager is invalid.");
		return;
	}

	MissionManager->TriggerDefeat(ERTSDefeatType::LostHQ);
}

bool UDontLoseHQMission::GetIsValidPlayerHQ() const
{
	if (M_PlayerHQ.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_PlayerHQ",
		"UDontLoseHQMission::GetIsValidPlayerHQ",
		this);

	return false;
}
