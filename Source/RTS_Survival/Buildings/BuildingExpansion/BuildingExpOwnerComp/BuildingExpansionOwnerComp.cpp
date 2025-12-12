// Copyright (C) Bas Blokzijl - All rights reserved.


#include "BuildingExpansionOwnerComp.h"

#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Player/CPPController.h"


UBuildingExpansionOwnerComp::UBuildingExpansionOwnerComp(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer),
	  M_PlayerController(nullptr)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBuildingExpansionOwnerComp::BeginPlay()
{
	Super::BeginPlay();
}


void UBuildingExpansionOwnerComp::UpdateMainGameUIWithStatusChanges(const int IndexBExpansion,
                                                                    const EBuildingExpansionStatus NewStatus,
                                                                    const EBuildingExpansionType NewType,
                                                                    const FBxpConstructionRules& ConstructionRules) const
{
	if (M_PlayerController.IsValid())
	{
		AActor* ComponentImplementingActor = GetOwner();
		M_PlayerController->GetMainMenuUI()->RequestUpdateSpecificBuildingExpansionItem(ComponentImplementingActor,
			NewType, NewStatus, ConstructionRules, IndexBExpansion);
	}
	else
	{
		RTSFunctionLibrary::ReportError("Player controller is null! "
			"\n at function UpdateMainGameUIWithStatusChanges in BuildingExpansionOwnerComp.cpp"
			"\n Actor: " + GetOwner()->GetName());
	}
}

void UBuildingExpansionOwnerComp::InitBuildingExpansionComp(ACPPController* NewPlayerController)
{
	if (IsValid(NewPlayerController))
	{
		M_PlayerController = NewPlayerController;
		return;
	}
	RTSFunctionLibrary::ReportError("NewPlayerController is null "
		"\n at function InitBuildingExpansionComp in BuildingExpansionOwnerComp.cpp."
		"\n attempting auto-repair...");
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	M_PlayerController = Cast<ACPPController>(World->GetFirstPlayerController());
	if (M_PlayerController.IsValid())
	{
		return;
	}
	RTSFunctionLibrary::ReportError("auto-repair failed!");
}
