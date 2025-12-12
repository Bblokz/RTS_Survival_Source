// Copyright (C) Bas Blokzijl - All rights reserved.


#include "RTSComponent.h"

#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/UnitTests/TestTurrets/ATestTurretOwner.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"


URTSComponent::URTSComponent()
	: OwningPlayer(1),
	  UnitType(EAllUnitType::UNType_Squad),
	  bAddToGameState(false), M_UnitSubType(0)

{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URTSComponent::SetUnitInCombat(const bool bInCombat)
{
	if (not bInCombat)
	{
		// Unit not in combat anymore, instant reset.
		bM_IsUnitInCombat = false;
		return;
	}
	if (bM_IsUnitInCombat)
	{
		// Unit already in combat, no need to set it again.
		return;
	}
	bM_IsUnitInCombat = true;
	TWeakObjectPtr<URTSComponent> WeakThis(this);
	auto OnOutOfCombat = [WeakThis]()-> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->bM_IsUnitInCombat = false;
	};
	if (UWorld* World = GetWorld())
	{
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindLambda(OnOutOfCombat);
		World->GetTimerManager().SetTimer(M_UnitInCombatTimerHandle,
		                                  OnOutOfCombat, M_UnitInCombatTimeElapse, false);
	}
}

FString URTSComponent::GetDisplayName(bool& bOutIsValidString) const
{
	switch (UnitType)
	{
	case EAllUnitType::UNType_None:
		bOutIsValidString = false;
		return "";
	case EAllUnitType::UNType_Squad:
		bOutIsValidString = true;
		return Global_GetSquadDisplayName(GetSubtypeAsSquadSubtype());
	case EAllUnitType::UNType_Harvester:
		bOutIsValidString = true;
		return "Harvester";
	case EAllUnitType::UNType_Tank:
		bOutIsValidString = true;
		return Global_GetTankDisplayName(GetSubtypeAsTankSubtype());
	case EAllUnitType::UNType_Nomadic:
		bOutIsValidString = true;
		return Global_GetNomadicDisplayName(GetSubtypeAsNomadicSubtype());
	case EAllUnitType::UNType_BuildingExpansion:
		bOutIsValidString = true;
		return "Building Expansion";
	case EAllUnitType::UNType_Aircraft:
		bOutIsValidString = true;
		return Global_GetAircraftDisplayName(GetSubtypeAsAircraftSubtype());
	}
	bOutIsValidString = false;
	return "None";
}

void URTSComponent::DeregisterFromGameState() const
{
	if (const UWorld* World = GetWorld())
	{
		AActor* ParentActor = GetOwner();
		ACPPGameState* GameState = Cast<ACPPGameState>(World->GetGameState());
		UGameUnitManager* GameUnitManager = NULL;
		if (IsValid(GameState))
		{
			GameUnitManager = GameState->GetGameUnitManager();
		}
		if (IsValid(ParentActor) && IsValid(GameUnitManager) && bAddToGameState)
		{
			if (ParentActor->IsA(ASquadUnit::StaticClass()))
			{
				ASquadUnit* SquadUnit = Cast<ASquadUnit>(ParentActor);
				GameUnitManager->RemoveSquadUnitForPlayer(SquadUnit, OwningPlayer);
			}
			else if (ParentActor->IsA(ATankMaster::StaticClass()))
			{
				ATankMaster* Tank = Cast<ATankMaster>(ParentActor);
				GameUnitManager->RemoveTankForPlayer(Tank, OwningPlayer);
			}
			else if(ParentActor->IsA(AAircraftMaster::StaticClass()))
			{
				AAircraftMaster* Aircraft =  Cast<AAircraftMaster>(ParentActor);
				GameUnitManager->RemoveAircraftForPlayer(Aircraft, OwningPlayer);
			}
			else if (ParentActor->IsA(ABuildingExpansion::StaticClass()))
			{
				ABuildingExpansion* Bxp = Cast<ABuildingExpansion>(ParentActor);
				GameUnitManager->RemoveBxpForPlayer(Bxp, OwningPlayer);
			}
			else
			{
				GameUnitManager->RemoveActorForPlayer(ParentActor, OwningPlayer);
			}
		}
		else
		{
			RTSFunctionLibrary::ReportError("Could not obtain valid"
				"GameUnitManager in URTSComponent::DeregisterFromGameState");
		}
	}
}

void URTSComponent::SetUnitSubtype(
	ENomadicSubtype NewNomadicSubtype,
	ETankSubtype NewTankSubtype, ESquadSubtype NewSquadSubtype,
	EBuildingExpansionType NewBxpSubType,
	EAircraftSubtype NewAircraftSubtype)
{
	if (NewNomadicSubtype != ENomadicSubtype::Nomadic_None)
	{
		M_UnitSubType = static_cast<uint8>(NewNomadicSubtype);
		if (NewTankSubtype != ETankSubtype::Tank_None)
		{
			RTSFunctionLibrary::ReportError(
				"Ambiguous Unit Subtype, both Nomadic and Tank Subtypes are set in SetUnitSubtype"
				"\n name of unit: " + GetOwner()->GetName());
		}
	}
	else if (NewTankSubtype != ETankSubtype::Tank_None)
	{
		M_UnitSubType = static_cast<uint8>(NewTankSubtype);
	}
	else if (NewSquadSubtype != ESquadSubtype::Squad_None)
	{
		M_UnitSubType = static_cast<uint8>(NewSquadSubtype);
	}
	else if (NewBxpSubType != EBuildingExpansionType::BXT_Invalid)
	{
		M_UnitSubType = static_cast<uint8>(NewBxpSubType);
	}
	else if(NewAircraftSubtype != EAircraftSubtype::Aircarft_None)
	{
		M_UnitSubType = static_cast<uint8>(NewAircraftSubtype);
	}
	else
	{
		RTSFunctionLibrary::ReportError("No Unit Subtype set in SetUnitSubtype"
			"\n name of unit: " + GetOwner()->GetName());
	}
	OnSubTypeInitialized.Broadcast();
}

ETankSubtype URTSComponent::GetSubtypeAsTankSubtype() const
{
	// interpret the stored integer value as a tank subtype
	return static_cast<ETankSubtype>(M_UnitSubType);
}

ENomadicSubtype URTSComponent::GetSubtypeAsNomadicSubtype() const
{
	return static_cast<ENomadicSubtype>(M_UnitSubType);
}

ESquadSubtype URTSComponent::GetSubtypeAsSquadSubtype() const
{
	return static_cast<ESquadSubtype>(M_UnitSubType);
}

EBuildingExpansionType URTSComponent::GetSubtypeAsBxpSubtype() const
{
	return static_cast<EBuildingExpansionType>(M_UnitSubType);
}

EAircraftSubtype URTSComponent::GetSubtypeAsAircraftSubtype() const
{
	return static_cast<EAircraftSubtype>(M_UnitSubType);
}

FTrainingOption URTSComponent::GetUnitTrainingOption() const
{
	FTrainingOption TrainingOption;
	TrainingOption.UnitType = UnitType;
	TrainingOption.SubtypeValue = M_UnitSubType;
	return TrainingOption;
}

void URTSComponent::SetSelectionPriority(const int32 NewSelectionPriority)
{
	if (NewSelectionPriority >= 0)
	{
		SelectionPriority = NewSelectionPriority;
	}
}

void URTSComponent::BeginPlay()
{
	Super::BeginPlay();

	CheckExposedUVariables();

	if (bAddToGameState)
	{
		if (!OwningPlayer == 0)
		{
			(void)AddUnitToGameState();
		}
	}
}

void URTSComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_UnitInCombatTimerHandle);
	}
}


void URTSComponent::ChangeOwningPlayer(const uint8 NewOwningPlayer)
{
	if (NewOwningPlayer > 0 && OwningPlayer != NewOwningPlayer)
	{
		if (const UWorld* World = GetWorld())
		{
			ACPPGameState* GameState = Cast<ACPPGameState>(World->GetGameState());
			UGameUnitManager* GameUnitManager = NULL;
			if (IsValid(GameState))
			{
				GameUnitManager = GameState->GetGameUnitManager();
			}
			AActor* ParentActor = GetOwner();
			if (IsValid(ParentActor) && IsValid(GameUnitManager) && bAddToGameState)
			{
				if (ParentActor->IsA(ASquadUnit::StaticClass()))
				{
					ASquadUnit* SquadUnit = Cast<ASquadUnit>(ParentActor);
					GameUnitManager->RemoveSquadUnitForPlayer(SquadUnit, OwningPlayer);
					GameUnitManager->AddSquadUnitForPlayer(SquadUnit, NewOwningPlayer);
				}
				else if (ParentActor->IsA(ATankMaster::StaticClass()))
				{
					ATankMaster* Tank = Cast<ATankMaster>(ParentActor);
					GameUnitManager->RemoveTankForPlayer(Tank, OwningPlayer);
					GameUnitManager->AddTankForPlayer(Tank, NewOwningPlayer);
				}
			}
		}
		OwningPlayer = NewOwningPlayer;
	}
}

bool URTSComponent::AddUnitToGameState() const
{
	if (const UWorld* World = GetWorld())
	{
		ACPPGameState* GameState = Cast<ACPPGameState>(World->GetGameState());
		UGameUnitManager* GameUnitManager = nullptr;
		if (IsValid(GameState))
		{
			GameUnitManager = GameState->GetGameUnitManager();
		}
		AActor* ParentActor = GetOwner();
		if (IsValid(GameUnitManager) && IsValid(ParentActor))
		{
			if (ParentActor->IsA(ASquadUnit::StaticClass()))
			{
				ASquadUnit* SquadUnit = Cast<ASquadUnit>(ParentActor);
				GameUnitManager->AddSquadUnitForPlayer(SquadUnit, OwningPlayer);
				return true;
			}
			if (ParentActor->IsA(ATankMaster::StaticClass()))
			{
				ATankMaster* Tank = Cast<ATankMaster>(ParentActor);
				GameUnitManager->AddTankForPlayer(Tank, OwningPlayer);
				return true;
			}
			if (ParentActor->IsA(AAircraftMaster::StaticClass()))
			{
				AAircraftMaster* Aircraft = Cast<AAircraftMaster>(ParentActor);
				GameUnitManager->AddAircraftForPlayer(Aircraft, OwningPlayer);
				return true;
			}
			if(ParentActor->IsA(ABuildingExpansion::StaticClass()))
			{
				ABuildingExpansion* Bxp = Cast<ABuildingExpansion>(ParentActor);
				GameUnitManager->AddBxpForPlayer(Bxp, OwningPlayer);
				return true;
			}
			if (ParentActor->IsA(ATestTurretOwner::StaticClass()))
			{
				ATestTurretOwner* TurretOwner = Cast<ATestTurretOwner>(ParentActor);
				GameUnitManager->AddActorForPlayer(TurretOwner, OwningPlayer);
				return true;
			}
			if (ParentActor->IsA(AHpPawnMaster::StaticClass()))
			{
				AHpPawnMaster* HpPawnMaster = Cast<AHpPawnMaster>(ParentActor);
				GameUnitManager->AddActorForPlayer(HpPawnMaster, OwningPlayer);
			}
			if (ParentActor->IsA(AHPActorObjectsMaster::StaticClass()))
			{
				AHPActorObjectsMaster* HpActorObjectsMaster = Cast<AHPActorObjectsMaster>(ParentActor);
				GameUnitManager->AddActorForPlayer(HpActorObjectsMaster, OwningPlayer);
			}
		}
		else
		{
			RTSFunctionLibrary::ReportError("Could not obtain valid"
				"GameUnitManager in URTSComponent::AddUnitToGameState");
		}
	} // Game Has Started
	return false;
}

void URTSComponent::CheckExposedUVariables()
{
	if (OwningPlayer == 222)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"OwningPlayer (set to default 222	)",
			"URTSComponent::CheckExposedUVariables",
			GetOwner());
	}

	if (UnitType == EAllUnitType::UNType_None)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"UnitType",
			"URTSComponent::CheckExposedUVariables",
			GetOwner());
	}
	if (FormationUnitInnerRadius == -1)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"FormationUnitInnerRadius",
			"URTSComponent::CheckExposedUVariables",
			GetOwner());
	}
}
