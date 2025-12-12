// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "MoveCameraMission.generated.h"

class UPlayerCameraController;
struct FMovePlayerCamera;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UMoveCameraMission : public UMissionBase
{
	GENERATED_BODY()

protected:
	virtual void OnMissionStart() override;


private:


	
};
