// Copyright (C) Bas Blokzijl - All rights reserved.


#include "BuildingExpansionOwnerComp.h"

#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


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
	if (not GetIsValidPlayerController())
	{
		return;
	}
	AActor* ComponentImplementingActor = GetOwner();
	M_PlayerController->GetMainMenuUI()->RequestUpdateSpecificBuildingExpansionItem(ComponentImplementingActor,
		NewType, NewStatus, ConstructionRules, IndexBExpansion);
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
	if (GetIsValidPlayerController())
	{
		return;
	}
}

bool UBuildingExpansionOwnerComp::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_PlayerController",
		"GetIsValidPlayerController",
		this);
	return false;
}
