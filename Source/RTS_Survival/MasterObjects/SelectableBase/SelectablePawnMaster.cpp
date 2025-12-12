// Copyright (C) Bas Blokzijl - All rights reserved.
#include "SelectablePawnMaster.h"

#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowType.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"


ASelectablePawnMaster::ASelectablePawnMaster(const FObjectInitializer& ObjectInitializer)
	: AHpPawnMaster(ObjectInitializer)
{
}

void ASelectablePawnMaster::SetUnitSelected(const bool bIsSelected) const
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

FVector ASelectablePawnMaster::GetOwnerLocation() const
{
	return GetActorLocation();
}

FString ASelectablePawnMaster::GetOwnerName() const
{
	return GetName();
}

AActor* ASelectablePawnMaster::GetOwnerActor()
{
	return this;
}

float ASelectablePawnMaster::GetUnitRepairRadius()
{
	if (not GetIsValidRTSComponent())
	{
		return 100.f;
	}
	return RTSComponent->GetFormationUnitInnerRadius() / 2;
}


void ASelectablePawnMaster::BeginPlay()
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
				                                          "ASelectablePawnMaster::BeginPlay");
			}
		}
		else
		{
			RTSFunctionLibrary::ReportNullErrorInitialisation(this, "PlayerController",
			                                                  "ASelectablePawnMaster::BeginPlay");
		}
	}
}

void ASelectablePawnMaster::PostInitProperties()
{
	Super::PostInitProperties();
}

void ASelectablePawnMaster::PostInitializeComponents()
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
			"\n ASelectablePawnMaster::PostInitializeComponents");
	}


	// Initialize the Selection component
	SelectionComponent = FindComponentByClass<USelectionComponent>();
	if (!IsValid(SelectionComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			this,
			"SelectionComponent",
			"ASelectablePawnMaster::PostInitializeComponents");
	}
	PostInitComp_InitFowBehaviour(RTSComponent);
}


void ASelectablePawnMaster::SetUnitToIdleSpecificLogic()
{
}

UCommandData* ASelectablePawnMaster::GetIsValidCommandData()
{
	if (UnitCommandData)
	{
		return UnitCommandData;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "UnitCommandData",
	                                                      "ASelectablePawnMaster::GetIsValidCommandData");
	return nullptr;
}


void ASelectablePawnMaster::DoneExecutingCommand(EAbilityID AbilityFinished)
{
	ICommands::DoneExecutingCommand(AbilityFinished);
}

void ASelectablePawnMaster::StopBehaviourTree()
{
}

void ASelectablePawnMaster::StopMovement()
{
}


void ASelectablePawnMaster::UnitDies(const ERTSDeathType DeathType)
{
	// Set death flag.
	Super::UnitDies(DeathType);
	// Deselect the unit if it is selected.
	if (GetIsValidPlayerControler() && GetIsSelected())
	{
		PlayerController->RemovePawnFromSelectionAndUpdateUI(this);
	}
}

void ASelectablePawnMaster::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	// Deselect the unit if it is selected.
	if (IsValid(PlayerController) && IsValid(SelectionComponent))
	{
		// No error report as selection comp is valid.
		if (not GetIsSelected())
		{
			return;
		}
		PlayerController->RemovePawnFromSelectionAndUpdateUI(this);
	}
}

bool ASelectablePawnMaster::GetIsValidPlayerControler() const
{
	if (IsValid(PlayerController))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid player controller on pawn:" + GetName());
	return false;
}

bool ASelectablePawnMaster::GetIsValidSelectionComponent() const
{
	if (IsValid(SelectionComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid selection component on pawn:" + GetName());
	return false;
}

bool ASelectablePawnMaster::GetIsSelected() const
{
	if (not GetIsValidSelectionComponent())
	{
		return false;
	}
	return SelectionComponent->GetIsSelected();
}

void ASelectablePawnMaster::PostInitComp_InitFowBehaviour(URTSComponent* MyRTSComponent)
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
		// Note that pawns of the enemy should always have a vision radius and hence are passiveEnemyVision.
		const EFowBehaviour FowBehaviour = MyRTSComponent->GetOwningPlayer() == 1
			                                   ? EFowBehaviour::Fow_Active
			                                   : EFowBehaviour::Fow_PassiveEnemyVision;
		// No start immediately as this pawn could have been async loaded and does not participate yet.
		FowComponent->StartFow(FowBehaviour, false);
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "FowComponent", "ATankMaster::BeginPlay");
	}
}
