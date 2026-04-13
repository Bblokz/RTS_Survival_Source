#include "DontLoseCommanderMission.h"

#include "RTS_Survival/Audio/RTSVoiceLines/RTSVoicelines.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Missions/Defeat/RTSDefeatType.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Types/MovePlayerCameraTypes.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace DontLoseCommanderMissionConstants
{
	constexpr int32 RetryFindCommanderDelaySeconds = 2;
	constexpr int32 MaxFindCommanderAttempts = 5;
}

void UDontLoseCommanderMission::OnMissionStart()
{
	Super::OnMissionStart();
	if (not MissionState.bIsTriggered)
	{
		return;
	}

	M_CommandVehicleFindAttempts = 0;
	bM_HasTriggeredCommanderLossFlow = false;
	M_CommandVehicle = nullptr;
	TryFindAndBindPlayerCommandVehicle();
}

void UDontLoseCommanderMission::OnMissionComplete()
{
	ClearMissionTimers();
	Super::OnMissionComplete();
}

void UDontLoseCommanderMission::OnMissionFailed()
{
	ClearMissionTimers();
	Super::OnMissionFailed();
}

void UDontLoseCommanderMission::ClearMissionTimers()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_RetryFindCommanderHandle);
	World->GetTimerManager().ClearTimer(M_DefeatAfterCommanderLostHandle);
}

void UDontLoseCommanderMission::TryFindAndBindPlayerCommandVehicle()
{
	UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		RTSFunctionLibrary::ReportError("DontLoseCommanderMission failed finding command vehicle because GameUnitManager is invalid.");
		return;
	}

	ATankMaster* CommandVehicle = GameUnitManager->GetPlayerCommandVehicle();
	if (not IsValid(CommandVehicle))
	{
		HandleNoCommandVehicleFound();
		return;
	}

	M_CommandVehicle = CommandVehicle;
	M_LastKnownCommanderVehiclePosition = CommandVehicle->GetActorLocation();
	BindToCommandVehicleDeath();
}

void UDontLoseCommanderMission::HandleNoCommandVehicleFound()
{
	M_CommandVehicleFindAttempts += 1;

	if (M_CommandVehicleFindAttempts >= DontLoseCommanderMissionConstants::MaxFindCommanderAttempts)
	{
		RTSFunctionLibrary::ReportError(
			"DontLoseCommanderMission failed to find a player command vehicle after maximum retries.");
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError("DontLoseCommanderMission could not schedule command vehicle retry because world is invalid.");
		return;
	}

	World->GetTimerManager().SetTimer(
		M_RetryFindCommanderHandle,
		this,
		&UDontLoseCommanderMission::HandleRetryFindCommandVehicle,
		DontLoseCommanderMissionConstants::RetryFindCommanderDelaySeconds,
		false);
}

void UDontLoseCommanderMission::BindToCommandVehicleDeath()
{
	if (not GetIsValidCommandVehicle())
	{
		return;
	}

	M_CommandVehicle->OnUnitDies.AddUObject(this, &UDontLoseCommanderMission::HandleCommandVehicleDied);
}

void UDontLoseCommanderMission::HandleCommandVehicleDied()
{
	if (bM_HasTriggeredCommanderLossFlow)
	{
		return;
	}
	bM_HasTriggeredCommanderLossFlow = true;

	ATankMaster* CommandVehicle = M_CommandVehicle.Get();
	if (IsValid(CommandVehicle))
	{
		M_LastKnownCommanderVehiclePosition = CommandVehicle->GetActorLocation();
	}

	StartCommanderLossCameraPan();
	TriggerCommanderLossAnnouncerVoiceLine();
	ScheduleMissionDefeat();
}

void UDontLoseCommanderMission::StartCommanderLossCameraPan()
{
	FMovePlayerCamera CameraMove;
	CameraMove.MoveToLocation = M_LastKnownCommanderVehiclePosition;
	CameraMove.TimeToMove = M_CameraPanTimeToCommanderDeathLocation;
	CameraMove.TimeCameraInputDisabled = M_CameraPanTimeToCommanderDeathLocation;
	MoveCamera(CameraMove);
}

void UDontLoseCommanderMission::TriggerCommanderLossAnnouncerVoiceLine() const
{
	PlayPlayerAnnouncerLine(EAnnouncerVoiceLineType::DefeatLostCommander);
}

void UDontLoseCommanderMission::ScheduleMissionDefeat()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError("DontLoseCommanderMission could not schedule defeat because world is invalid.");
		return;
	}

	const float DefeatDelaySeconds = FMath::Max(0.0f, M_DefeatDelayAfterCommanderLost);
	World->GetTimerManager().SetTimer(
		M_DefeatAfterCommanderLostHandle,
		this,
		&UDontLoseCommanderMission::HandleTriggerDefeatDelayed,
		DefeatDelaySeconds,
		false);
}

void UDontLoseCommanderMission::HandleRetryFindCommandVehicle()
{
	TryFindAndBindPlayerCommandVehicle();
}

void UDontLoseCommanderMission::HandleTriggerDefeatDelayed()
{
	AMissionManager* MissionManager = GetMissionManagerChecked();
	if (not IsValid(MissionManager))
	{
		RTSFunctionLibrary::ReportError("DontLoseCommanderMission failed triggering defeat because mission manager is invalid.");
		return;
	}

	MissionManager->TriggerDefeat(ERTSDefeatType::LostCommandVehicle);
}

bool UDontLoseCommanderMission::GetIsValidCommandVehicle() const
{
	if (M_CommandVehicle.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_CommandVehicle",
		"UDontLoseCommanderMission::GetIsValidCommandVehicle",
		this);

	return false;
}
