// Copyright (C) Bas Blokzijl - All rights reserved.



#include "ActorObjectsMaster.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


AActorObjectsMaster::AActorObjectsMaster(const FObjectInitializer& ObjectInitializer)
 : Super(ObjectInitializer)	
{
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AActorObjectsMaster::BeginPlay()
{
	Super::BeginPlay();

	if(AGameStateBase* GameState =  UGameplayStatics::GetGameState(this))
	{
		const ACPPGameState* CPPGameState = Cast<ACPPGameState>(GameState);
		if(IsValid(CPPGameState))
		{
			M_GameUnitManager = CPPGameState->GetGameUnitManager();
			if(!IsValid(M_GameUnitManager))
			{
				RTSFunctionLibrary::ReportError("GameUnitManager not set in GameState"
					"\n actor cannot set reference, actor: " + GetName());
			}
		}
		else
		{
			RTSFunctionLibrary::ReportFailedCastError("GameState",
				"CPPGameState", "CPPTurretMaster::BeginPlay");
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError("GameState not found in ActorObjectsMaster::BeginPlay");
	
	}
	
}

bool AActorObjectsMaster::GetIsValidGameUnitManger() const
{
	if(IsValid(M_GameUnitManager))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("GameUnitManager not set in ActorObjectsMaster"
		"\n  actor: " + GetName());
	return false;
}

// Called every frame
void AActorObjectsMaster::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

