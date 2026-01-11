// Copyright (C) Bas Blokzijl - All rights reserved.


#include "SelectableActorObjectsMaster.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowType.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


ASelectableActorObjectsMaster::ASelectableActorObjectsMaster(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	  , bIsHidden(false)
	  , SelectionComponent(nullptr)
	  , PlayerController(nullptr)
	  , UnitCommandData(nullptr)
{
}

void ASelectableActorObjectsMaster::SetUnitSelected(const bool bIsSelected) const
{
	if (not GetIsValidSelectionComponent())
	{
		return;
	}
	if (bIsSelected)
	{
		SelectionComponent->SetUnitSelected();
	}
	else
	{
		SelectionComponent->SetUnitDeselected();
	}
}


void ASelectableActorObjectsMaster::BeginPlay()
{
	Super::BeginPlay();
	if (const UWorld* World = GetWorld())
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
		if (IsValid(PC))
		{
			if (ACPPController* RTSController = Cast<ACPPController>(PC))
			{
				PlayerController = RTSController;
			}
			else
			{
				RTSFunctionLibrary::ReportFailedCastError("RTSPlayerController",
				                                          "ACPPController",
				                                          "ASelectableActorObjectsMaster::BeginPlay");
			}
		}
		else
		{
			RTSFunctionLibrary::ReportNullErrorInitialisation(this, "PlayerController",
			                                                  "ASelectableActorObjectsMaster::BeginPlay");
		}
	}
}

void ASelectableActorObjectsMaster::BeginDestroy()
{
	Super::BeginDestroy();
	UnitCommandData = nullptr;
}

void ASelectableActorObjectsMaster::UnitDies(const ERTSDeathType DeathType)
{
	// Set death flag.
	Super::UnitDies(DeathType);
	// Deselect unit if selected.
	if (GetIsValidSelectionComponent() && SelectionComponent->GetIsSelected())
	{
		if (IsValid(PlayerController))
		{
			PlayerController->RemoveActorFromSelectionAndUpdateUI(this);
		}
	}
}

void ASelectableActorObjectsMaster::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	// Deselect unit if selected.
	if (IsValid(SelectionComponent) && SelectionComponent->GetIsSelected())
	{
		if (IsValid(PlayerController))
		{
			PlayerController->RemoveActorFromSelectionAndUpdateUI(this);
		}
	}
}


bool ASelectableActorObjectsMaster::GetIsValidSelectionComponent() const
{
	if (not IsValid(SelectionComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "SelectionComponent",
		                                             "ASelectableActorObjectsMaster::GetIsValidSelectionComponent");
		return false;
	}
	return true;
}

void ASelectableActorObjectsMaster::PostInitializeComponents()
{
	// Important; first init the RTS component!
	Super::PostInitializeComponents();
	UnitCommandData = NewObject<UCommandData>(this);
	// Pass on a reference to self so the CommandData can access the interface.
	UnitCommandData->InitCommandData(this);
	UObject* Outer = UnitCommandData->GetOuter();
	if (Outer != this)
	{
		RTSFunctionLibrary::ReportError("The outer on this CommandData is not set to this object."
			"\n ASelectableActorObjectsMaster::PostInitializeComponents");
	}

	SelectionComponent = FindComponentByClass<USelectionComponent>();
	if (!IsValid(SelectionComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "SelectionComponent",
		                                             "ASelectableActorObjectsMaster::PostInitializeComponents");
	};
	PostInitComp_InitFowBehaviour(RTSComponent);
}

bool ASelectableActorObjectsMaster::GetIsValidFowComponent() const
{
	if(not IsValid(FowComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "FowComponent",
		                                             "ASelectableActorObjectsMaster::GetIsValidFowComponent");
		return false;
	}
	return true;
}

void ASelectableActorObjectsMaster::SetUnitToIdleSpecificLogic()
{
}

UCommandData* ASelectableActorObjectsMaster::GetIsValidCommandData()
{
	if (UnitCommandData)
	{
		return UnitCommandData;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "UnitCommandData",
	                                                      "ASelectableActorObjectsMaster::GetIsValidCommandData");
	return nullptr;
}

void ASelectableActorObjectsMaster::DoneExecutingCommand(EAbilityID AbilityFinished)
{
	ICommands::DoneExecutingCommand(AbilityFinished);
}

void ASelectableActorObjectsMaster::StopBehaviourTree()
{
}

void ASelectableActorObjectsMaster::StopMovement()
{
}

FVector ASelectableActorObjectsMaster::GetOwnerLocation() const
{
	return GetActorLocation();
}

FString ASelectableActorObjectsMaster::GetOwnerName() const
{
	return GetName();
}

AActor* ASelectableActorObjectsMaster::GetOwnerActor()
{
	return this;
}

void ASelectableActorObjectsMaster::PostInitComp_InitFowBehaviour(URTSComponent* MyRTSComponent)
{
	if (not IsValid(MyRTSComponent))
	{
		RTSFunctionLibrary::ReportError("Cannot setup fow behaviour for pawn: + " + GetName() +
			"\n as it has no valid RTSComponent");
		return;
	}
	FowComponent = NewObject<UFowComp>(this, TEXT("FowComponent"));
	if (IsValid(FowComponent))
	{
		FowComponent->RegisterComponent();
		// If the owning player is 1 then the unit is active in fow otherwise it is passive.
		// the unit either has a vision bubble or is hidden/shown depending on their fow position.
		// All selectable actors generate vision either AI- or Player-owned.
		const EFowBehaviour FowBehaviour = MyRTSComponent->GetOwningPlayer() == 1
			                                   ? EFowBehaviour::Fow_Active
			                                   : EFowBehaviour::Fow_PassiveEnemyVision;
		// Start Immediately as unlike pawns these units are not trained
		// todo change this incase we can later train SelectableActors
		FowComponent->StartFow(FowBehaviour, true);
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "FowComponent", "ATankMaster::BeginPlay");
	}
}
