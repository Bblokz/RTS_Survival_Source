// Copyright (C) Bas Blokzijl - All rights reserved.


#include "Commands.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/CaptureMechanic/CaptureMechanicHelpers.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/PickupItems/Items/ItemsMaster.h"
#include "RTS_Survival/Resources/Resource.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Units/Aircraft/AirBase/AircraftOwnerComp/AircraftOwnerComp.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


UCommandData::UCommandData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	  , bM_IsCommandQueueEnabled(false)
	  , bM_IsSpawning(false)
	  , NumCommands(0)
	  , CurrentIndex(-1)
	  , M_Owner(nullptr)
{
	// Initialize array
	for (int32 i = 0; i < MAX_COMMANDS; i++)
	{
		M_TCommands[i] = FQueueCommand();
	}
	M_FinalRotation = {FRotator::ZeroRotator, false};
}


void UCommandData::BeginDestroy()
{
	ClearCommands();
	UObject::BeginDestroy();
}


void UCommandData::SetAbilities(const TArray<FUnitAbilityEntry>& Abilities)
{
	TArray<FUnitAbilityEntry> ValidAbilities = Abilities;
	if (Abilities.Num() > DeveloperSettings::GamePlay::ActionUI::MaxAbilitiesForActionUI)
	{
		RTSFunctionLibrary::ReportError("The number of abilities exceeds the maximum allowed for the Action UI.",
				"UCommandData::SetAbilities");
		ValidAbilities.SetNum(DeveloperSettings::GamePlay::ActionUI::MaxAbilitiesForActionUI);
	}
	M_Abilities = ValidAbilities;
}

TArray<EAbilityID> UCommandData::GetAbilityIds(const bool bExcludeNoAbility) const
{
        return ExtractAbilityIdsFromEntries(M_Abilities, bExcludeNoAbility);
}

bool UCommandData::SwapAbility(const EAbilityID OldAbility, const EAbilityID NewAbility)
{
        return SwapAbility(OldAbility, CreateAbilityEntryFromId(NewAbility));
}

bool UCommandData::SwapAbility(const EAbilityID OldAbility, const FUnitAbilityEntry& NewAbility)
{
        const int32 Index = GetAbilityIndexById(OldAbility);
        if (Index != INDEX_NONE)
        {
                M_Abilities[Index] = NewAbility;
                return true;
        }

        return false;
}

bool UCommandData::AddAbility(const EAbilityID NewAbility, const int32 AtIndex)
{
        return AddAbility(CreateAbilityEntryFromId(NewAbility), AtIndex);
}

bool UCommandData::AddAbility(const FUnitAbilityEntry& NewAbility, const int32 AtIndex)
{
        if (NewAbility.AbilityId == EAbilityID::IdNoAbility)
        {
                return false;
        }
        if (GetAbilityIndexById(NewAbility.AbilityId) != INDEX_NONE)
        {
                RTSFunctionLibrary::ReportError(
                        TEXT("Attempted to add an ability that already exists: ") + Global_GetAbilityIDAsString(NewAbility.AbilityId) +
                        TEXT(" in UCommandData::AddAbility"));
                return false;
        }

        if (AtIndex == INDEX_NONE)
	{
		for (int32 i = 0; i < M_Abilities.Num(); i++)
		{
                        if (M_Abilities[i].AbilityId == EAbilityID::IdNoAbility)
                        {
                                M_Abilities[i] = NewAbility;
                                return true;
                        }
                }
                return false;
        }
        if (AtIndex >= 0 && AtIndex < M_Abilities.Num())
        {
                if (M_Abilities[AtIndex].AbilityId != EAbilityID::IdNoAbility)
                {
                        RTSFunctionLibrary::ReportError(
                                TEXT("Attempted to add ability at index that is not empty: ") + FString::FromInt(AtIndex) +
                                TEXT(" in UCommandData::AddAbility"));
                        return false;
                }
                M_Abilities[AtIndex] = NewAbility;
                return true;
        }
        RTSFunctionLibrary::ReportError(
                TEXT("Attempted to add ability at invalid index: ") + FString::FromInt(AtIndex) +
		TEXT(" in UCommandData::AddAbility"));
	return false;
}

bool UCommandData::RemoveAbility(const EAbilityID AbilityToRemove)
{
	if (AbilityToRemove == EAbilityID::IdNoAbility)
	{
		return false;
	}

	const int32 Index = GetAbilityIndexById(AbilityToRemove);
	if (Index != INDEX_NONE)
	{
		M_Abilities[Index] = FUnitAbilityEntry();
		return true;
	}

	RTSFunctionLibrary::ReportError(
		TEXT("Attempted to remove an ability that does not exist: ") + Global_GetAbilityIDAsString(AbilityToRemove) +
		TEXT(" in UCommandData::RemoveAbility"));
	return false;
}

int32 UCommandData::GetAbilityIndexById(const EAbilityID AbilityId) const
{
	return M_Abilities.IndexOfByPredicate([AbilityId](const FUnitAbilityEntry& AbilityEntry)
	{
		return AbilityEntry.AbilityId == AbilityId;
	});
}

void UCommandData::InitCommandData(ICommands* Owner)
{
        if (not Owner)
        {
                RTSFunctionLibrary::ReportError(TEXT("Attempted to setup command data with a nullptr owner.")
                        TEXT("at function UCommandData::InitCommandData"));
                return;
        }

	M_Owner = Owner;
	bM_IsCommandQueueEnabled = true;
	NumCommands = 0;
	CurrentIndex = -1;
}

EAbilityID UCommandData::GetCurrentActiveCommand() const
{
	return GetCurrentlyActiveCommandType();
}


/** Return the currently active command's EAbilityID. */
EAbilityID UCommandData::GetCurrentlyActiveCommandType() const
{
	if (CurrentIndex >= 0 && CurrentIndex < NumCommands)
	{
		return M_TCommands[CurrentIndex].CommandType;
	}
	return EAbilityID::IdIdle;
}

bool UCommandData::GetIsQueueFull() const
{
	return NumCommands >= MAX_COMMANDS;
}


void UCommandData::PrintCommands() const
{
        if (not M_Owner)
        {
                return;
        }

	FString DebugString = "Command queue (size=" + FString::FromInt(NumCommands) + "):";

	for (int32 i = 0; i < NumCommands; i++)
	{
		const FQueueCommand& Cmd = M_TCommands[i];
		FString CommandName = Global_GetAbilityIDAsString(Cmd.CommandType);
		FString ActiveStr = (i == CurrentIndex) ? TEXT(" [ACTIVE]") : TEXT("");
		DebugString += FString::Printf(TEXT("\n  Index %d: %s%s"), i, *CommandName, *ActiveStr);
	}

	// Draw debug string 300 units above the unit
	FVector Location = M_Owner->GetOwnerLocation();
	if (UWorld* World = GetWorld())
	{
		DrawDebugString(World, Location + FVector(0, 0, 300), DebugString, nullptr, FColor::White, 5.0f);
	}
}


bool UCommandData::GetHasCommandInQueue(EAbilityID CommandToCheck) const
{
	for (int32 i = 0; i < NumCommands; i++)
	{
		if (M_TCommands[i].CommandType == CommandToCheck)
		{
			return true;
		}
	}
	return false;
}

ECommandQueueError UCommandData::AddAbilityToTCommands(
	const EAbilityID Ability,
	const FVector& Location,
	AActor* TargetActor,
	const FRotator& Rotation)
{
	// Check if we have an active queue and if we are not patrolling.
	// In case we shift click while the queue 
	if (const ECommandQueueError QueueError = GetCanAddAnythingNewToQueue(); QueueError != ECommandQueueError::NoError)
	{
		return QueueError;
	}
	SetRotationFlagForFinalMovementCommand(Ability, Rotation);
	if (bM_IsSpawning)
	{
		// The unit has obtained its first command.
		bM_IsSpawning = false;
	}

	// Fill the struct
	FQueueCommand NewCmd;
	NewCmd.CommandType = Ability;
	NewCmd.TargetLocation = Location;
	NewCmd.TargetActor = TargetActor;
	NewCmd.TargetRotator = Rotation;

	// Insert at the end
	M_TCommands[NumCommands] = NewCmd;
	NumCommands++;


	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Added command: " + FString::FromInt((int32)Ability), FColor::Blue);
		PrintCommands();
	}
	// If this is the very first command, we start executing it immediately
	if (NumCommands == 1)
	{
		// If we had any old leftover index
		if (CurrentIndex >= 0)
		{
			RTSFunctionLibrary::ReportError("Index was not reset while attempting to execute first command in queue!");
		}
		ExecuteCommand(false);
	}
	return ECommandQueueError::NoError;
}


void UCommandData::ExecuteCommand(const bool bExecuteCurrentCommand)
{
	if (!M_Owner)
	{
		RTSFunctionLibrary::ReportError("UCommandData::ExecuteCommand - M_Owner is null");
		return;
	}

	if (!bExecuteCurrentCommand)
	{
		// Move to next
		CurrentIndex++;
		if (CurrentIndex >= NumCommands)
		{
			ClearCommands();
			if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
			{
				RTSFunctionLibrary::PrintString("No valid next command index; all done!", FColor::Blue);
			}
			M_Owner->OnUnitIdleAndNoNewCommands();
			return;
		}
	}

	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		PrintCommands();
	}

	if (CurrentIndex < 0 || CurrentIndex >= NumCommands)
	{
		RTSFunctionLibrary::ReportError("ExecuteCommand index is out of bounds!");
		return;
	}

	FQueueCommand& Cmd = M_TCommands[CurrentIndex];
	// If the command is invalid or something, we skip. 
	if (Cmd.CommandType == EAbilityID::IdNoAbility)
	{
		RTSFunctionLibrary::PrintString("The command is IdNoAbility, skipping...");
		return;
	}

	// Now we call ICommands methods based on the type:
	switch (Cmd.CommandType)
	{
	case EAbilityID::IdMove:
		{
			M_Owner->ExecuteMoveCommand(Cmd.TargetLocation);
		}
		break;
	case EAbilityID::IdPatrol:
		{
			M_Owner->ExecutePatrolCommand(Cmd.TargetLocation);
		}
		break;
	case EAbilityID::IdReverseMove:
		{
			M_Owner->ExecuteReverseCommand(Cmd.TargetLocation);
		}
		break;
	case EAbilityID::IdAttack:
		{
			M_Owner->ExecuteAttackCommand(Cmd.TargetActor.Get());
		}
		break;

	case EAbilityID::IdAttackGround:
		{
			M_Owner->ExecuteAttackGroundCommand(Cmd.TargetLocation);
		}
		break;
	case EAbilityID::IdRotateTowards:
		{
			M_Owner->ExecuteRotateTowardsCommand(Cmd.TargetRotator, true);
		}
		break;
	case EAbilityID::IdCreateBuilding:
		{
			M_Owner->ExecuteCreateBuildingCommand(Cmd.TargetLocation, Cmd.TargetRotator);
		}
		break;
	case EAbilityID::IdConvertToVehicle:
		{
			M_Owner->ExecuteConvertToVehicleCommand();
		}
		break;
	case EAbilityID::IdHarvestResource:
		{
			ACPPResourceMaster* TargetResource = Cmd.TargetActor.IsValid()
				                                     ? Cast<ACPPResourceMaster>(Cmd.TargetActor.Get())
				                                     : nullptr;
			M_Owner->ExecuteHarvestResourceCommand(TargetResource);
		}
		break;
	case EAbilityID::IdReturnCargo:
		{
			M_Owner->ExecuteReturnCargoCommand();
		}
		break;
	case EAbilityID::IdPickupItem:
		{
			AActor* TargetItem = Cmd.TargetActor.Get();
			AItemsMaster* Item = Cast<AItemsMaster>(TargetItem);
			M_Owner->ExecutePickupItemCommand(Item);
		}
		break;
	case EAbilityID::IdScavenge:
		{
			AActor* TargetItem = Cmd.TargetActor.Get();
			M_Owner->ExecuteScavengeObject(TargetItem);
		}
		break;
	case EAbilityID::IdRepair:
		{
			AActor* TargetActor = Cmd.TargetActor.Get();
			M_Owner->ExecuteRepairCommand(TargetActor);
			break;
		}
	// Also switch weapon case if we want:
	case EAbilityID::IdSwitchWeapon:
		{
			M_Owner->ExecuteSwitchWeaponsCommand();
		}
		break;
	case EAbilityID::IdDigIn:
		{
			M_Owner->ExecuteDigIn();
		}
		break;
	case EAbilityID::IdBreakCover:
		{
			M_Owner->ExecuteBreakCover();
		}
		break;
	case EAbilityID::IdFireRockets:
		{
			M_Owner->ExecuteFireRockets();
		}
		break;
	case EAbilityID::IdCancelRocketFire:
		{
			M_Owner->ExecuteCancelFireRockets();
		}
		break;
	case EAbilityID::IdReturnToBase:
		{
			M_Owner->ExecuteReturnToBase();
		}
		break;
	case EAbilityID::IdEnterCargo:
		{
			M_Owner->ExecuteEnterCargoCommand(Cmd.TargetActor.Get());
		}
		break;
	case EAbilityID::IdExitCargo:
		{
			M_Owner->ExecuteExitCargoCommand();
		}
		break;
	case EAbilityID::IdCapture:
		{
			M_Owner->ExecuteCaptureCommand(Cmd.TargetActor.Get());
		}
	default:
		RTSFunctionLibrary::PrintString(
			"Command not implemented in ExecuteCommand: " + Global_GetAbilityIDAsString(Cmd.CommandType)
			+ "\n See ICommands!");
	}
}


void UCommandData::ClearCommands()
{
	if (NumCommands > 0)
	{
		bM_IsCommandQueueEnabled = true;
		for (int32 i = 0; i < NumCommands; i++)
		{
			M_TCommands[i] = FQueueCommand();
		}
	}
	NumCommands = 0;
	CurrentIndex = -1;
}

ECommandQueueError UCommandData::IsQueueActiveAndNoPatrol() const
{
	if (!bM_IsCommandQueueEnabled)
	{
		return ECommandQueueError::QueueInactive;
	}

	return GetHasCommandInQueue(EAbilityID::IdPatrol)
		       ? ECommandQueueError::QueueHasPatrol
		       : ECommandQueueError::NoError;
}


ECommandQueueError UCommandData::GetCanAddAnythingNewToQueue() const
{
	ECommandQueueError QueueError = IsQueueActiveAndNoPatrol();
	if (QueueError != ECommandQueueError::NoError)
	{
		return QueueError;
	}
	if (GetIsQueueFull())
	{
		return ECommandQueueError::QueueFull;
	}
	return ECommandQueueError::NoError;
}


void UCommandData::SetRotationFlagForFinalMovementCommand(const EAbilityID NewFinalCommand, const FRotator& Rotation)
{
	if (NewFinalCommand == EAbilityID::IdMove)
	{
		M_FinalRotation.Rotation = Rotation;
		M_FinalRotation.bIsSet = true;
		return;
	}
	M_FinalRotation.bIsSet = false;
}

void ICommands::SetPrimarySelected(UActionUIManager* ActionUIManager)
{
	if (UCommandData* UnitCommandData = GetIsValidCommandData())
	{
		UnitCommandData->M_ActionUIManager = ActionUIManager;
	}
}

void ICommands::SetIsSpawning(const bool bIsSpawning)
{
	if (UCommandData* UnitCommandData = GetIsValidCommandData())
	{
		UnitCommandData->bM_IsSpawning = bIsSpawning;
	}
}

void ICommands::InitAbilityArray(const TArray<FUnitAbilityEntry>& Abilities)
{
	if (UCommandData* UnitCommandData = GetIsValidCommandData())
	{
		UnitCommandData->SetAbilities(Abilities);
	}
}

void ICommands::InitAbilityArray(const TArray<EAbilityID>& Abilities)
{
	InitAbilityArray(ConvertAbilityIdsToEntries(Abilities));
}

TArray<FUnitAbilityEntry> ICommands::GetUnitAbilityEntries()
{
	if (const UCommandData* UnitCommandData = GetIsValidCommandData())
	{
		return UnitCommandData->GetAbilities();
	}
	return TArray<FUnitAbilityEntry>();
}

TArray<EAbilityID> ICommands::GetUnitAbilities()
{
	if (const UCommandData* UnitCommandData = GetIsValidCommandData())
	{
		return UnitCommandData->GetAbilityIds();
	}
	return TArray<EAbilityID>();
}

void ICommands::SetUnitAbilitiesRunTime(const TArray<FUnitAbilityEntry>& Abilities)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not UnitCommandData)
	{
		return;
	}
	UnitCommandData->SetAbilities(Abilities);
	if (UnitCommandData->GetIsPrimarySelected())
	{
		UnitCommandData->M_ActionUIManager->RequestUpdateAbilityUIForPrimary(this);
	}
}

void ICommands::SetUnitAbilitiesRunTime(const TArray<EAbilityID>& Abilities)
{
        SetUnitAbilitiesRunTime(ConvertAbilityIdsToEntries(Abilities));
}

bool ICommands::AddAbility(const EAbilityID NewAbility, const int32 AtIndex)
{
        UCommandData* UnitCommandData = GetIsValidCommandData();
        if (not UnitCommandData)
        {
                return false;
        }
        const bool bAdded = UnitCommandData->AddAbility(NewAbility, AtIndex);
        if (UnitCommandData->GetIsPrimarySelected())
        {
                UnitCommandData->M_ActionUIManager->RequestUpdateAbilityUIForPrimary(this);
        }
        return bAdded;
}

bool ICommands::AddAbility(const FUnitAbilityEntry& NewAbility, const int32 AtIndex)
{
        UCommandData* UnitCommandData = GetIsValidCommandData();
        if (not UnitCommandData)
        {
                return false;
        }
        const bool bAdded = UnitCommandData->AddAbility(NewAbility, AtIndex);
        if (UnitCommandData->GetIsPrimarySelected())
        {
                UnitCommandData->M_ActionUIManager->RequestUpdateAbilityUIForPrimary(this);
        }
        return bAdded;
}

bool ICommands::RemoveAbility(const EAbilityID AbilityToRemove)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not UnitCommandData)
	{
		return false;
	}
	const bool bAbilityRemoved = UnitCommandData->RemoveAbility(AbilityToRemove);
	if (UnitCommandData->GetIsPrimarySelected())
	{
		UnitCommandData->M_ActionUIManager->RequestUpdateAbilityUIForPrimary(this);
	}
	return bAbilityRemoved;
}

bool ICommands::SwapAbility(const EAbilityID OldAbility, const EAbilityID NewAbility)
{
        UCommandData* UnitCommandData = GetIsValidCommandData();
        if (not UnitCommandData)
        {
                return false;
        }
        const bool bIsAbilitySwapped = UnitCommandData->SwapAbility(OldAbility, NewAbility);
        if (UnitCommandData->GetIsPrimarySelected())
        {
                UnitCommandData->M_ActionUIManager->RequestUpdateAbilityUIForPrimary(this);
        }
        return bIsAbilitySwapped;
}

bool ICommands::SwapAbility(const EAbilityID OldAbility, const FUnitAbilityEntry& NewAbility)
{
        UCommandData* UnitCommandData = GetIsValidCommandData();
        if (not UnitCommandData)
        {
                return false;
        }
        const bool bIsAbilitySwapped = UnitCommandData->SwapAbility(OldAbility, NewAbility);
        if (UnitCommandData->GetIsPrimarySelected())
        {
                UnitCommandData->M_ActionUIManager->RequestUpdateAbilityUIForPrimary(this);
        }
        return bIsAbilitySwapped;
}

bool ICommands::HasAbility(const EAbilityID AbilityToCheck)
{
        if (UCommandData* UnitCommandData = GetIsValidCommandData())
        {
                return UnitCommandData->GetAbilityIds().Contains(AbilityToCheck);
        }
        return false;
}


bool ICommands::GetIsSpawning()
{
	if (UCommandData* UnitCommandData = GetIsValidCommandData())
	{
		return UnitCommandData->bM_IsSpawning;
	}
	return false;
}

void ICommands::NoQueue_SetResourceConversionEnabled(const bool bEnabled)
{
	NoQueue_ExecuteSetResourceConversionEnabled(bEnabled);
}

ECommandQueueError ICommands::MoveToLocation(
	const FVector& Location,
	const bool bSetUnitToIdle,
	const FRotator& FinishedMovementRotation, const bool bForceFinalRotationRegardlessOfReverse)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}

	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdMove))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	// By default this is false which means that the vehicle reversing to the location will completely disregard the
	// final rotation set. This is set to true when the player uses a rotation arrow to determine the final rotation,
	// in which case the final rotation overrides the reversing logic.
	UnitCommandData->M_FinalRotation.bForceFinalRotationRegardlessOfReverse = bForceFinalRotationRegardlessOfReverse;

	// We add the final movement rotation as the rotation parameter here.
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(EAbilityID::IdMove, Location, nullptr,
	                                                                        FinishedMovementRotation);
	return Error;
}

void ICommands::SetForceFinalRotationRegardlessOfReverse(const bool ForceUseFinalRotation)
{
	if (UCommandData* CommandData = GetIsValidCommandData())
	{
		CommandData->M_FinalRotation.bForceFinalRotationRegardlessOfReverse = ForceUseFinalRotation;
	}
}


bool ICommands::GetForceFinalRotationRegardlessOfReverse()
{
	if (const UCommandData* CommandData = GetIsValidCommandData())
	{
		return CommandData->M_FinalRotation.bForceFinalRotationRegardlessOfReverse;
	}
	// Safe default is the legacy behavior.
	return true;
}


ECommandQueueError ICommands::StopCommand()
{
	SetUnitToIdle();
	ExecuteStopCommand();
	return ECommandQueueError::NoError;
}


ECommandQueueError ICommands::AttackActor(
	AActor* Target,
	const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdAttack))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdAttack, FVector::ZeroVector, Target, FRotator::ZeroRotator);
	return Error;
}

ECommandQueueError ICommands::CaptureActor(AActor* CaptureTarget, const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	if(not FCaptureMechanicHelpers::GetValidCaptureInterface(CaptureTarget) || not GetCanCaptureUnits())
	{
	return ECommandQueueError::AbilityNotAllowed;
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdCapture, FVector::ZeroVector, CaptureTarget, FRotator::ZeroRotator);
	return Error;
}

ECommandQueueError ICommands::AttackGround(const FVector& Location, const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (!IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	// Check for regular attack ability; attack ground is not on the command card.
	if (!GetIsAbilityAllowedForUnit(EAbilityID::IdAttack))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	return UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdAttackGround,
		Location,
		/*TargetActor=*/nullptr,
		/*Rotation=*/FRotator::ZeroRotator);
}

ECommandQueueError ICommands::PatrolToLocation(const FVector& Location, const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();

	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdPatrol))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdPatrol, Location, nullptr, FRotator::ZeroRotator);
	return Error;
}


ECommandQueueError ICommands::ReverseUnitToLocation(
	const FVector& Location,
	const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdReverseMove))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdReverseMove, Location, nullptr, FRotator::ZeroRotator);
	return Error;
}


ECommandQueueError ICommands::RotateTowards(
	const FRotator& WorldRotation,
	const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();

	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}

	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdRotateTowards))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdRotateTowards, FVector::ZeroVector, nullptr,
		WorldRotation);
	return Error;
}


ECommandQueueError ICommands::CreateBuildingAtLocation(const FVector Location, const FRotator WorldRotation,
                                                       const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	// No ability allowed check as this ability is not part of the command card.
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdCreateBuilding, Location, nullptr, WorldRotation);
	return Error;
}


ECommandQueueError ICommands::HarvestResource(
	AActor* TargetResource,
	const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();

	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdHarvestResource))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(EAbilityID::IdHarvestResource,
	                                                                  FVector::ZeroVector, TargetResource,
	                                                                  FRotator::ZeroRotator);
	return Error;
}

ECommandQueueError ICommands::ReturnCargo(const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();

	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdHarvestResource))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(EAbilityID::IdReturnCargo,
	                                                                  FVector::ZeroVector, nullptr,
	                                                                  FRotator::ZeroRotator);
	return Error;
}


ECommandQueueError ICommands::PickupItem(AItemsMaster* TargetItem, const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();

	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdPickupItem))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(EAbilityID::IdPickupItem, FVector::ZeroVector,
	                                                                  TargetItem,
	                                                                  FRotator::ZeroRotator);
	return Error;
}

ECommandQueueError ICommands::DigIn(const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();

	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdDigIn))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdDigIn, FVector::ZeroVector, nullptr,
		FRotator::ZeroRotator);
	return Error;
}

ECommandQueueError ICommands::BreakCover(const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();

	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdBreakCover))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdBreakCover, FVector::ZeroVector, nullptr,
		FRotator::ZeroRotator);
	return Error;
}


ECommandQueueError ICommands::SwitchWeapons(const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();

	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdSwitchWeapon))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdSwitchWeapon, FVector::ZeroVector, nullptr,
		FRotator::ZeroRotator);
	return Error;
}

ECommandQueueError ICommands::FireRockets(const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();

	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdFireRockets))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdFireRockets, FVector::ZeroVector, nullptr,
		FRotator::ZeroRotator);
	return Error;
}

ECommandQueueError ICommands::CancelFireRockets(const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdCancelRocketFire))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdCancelRocketFire, FVector::ZeroVector, nullptr,
		FRotator::ZeroRotator);
	return Error;
}

UHarvester* ICommands::GetIsHarvester()
{
	return nullptr;
	// Find if the unit has a harvester component.
	// const AActor* Owner = Cast<AActor>(this);
	// if (IsValid(Owner))
	// {
	// 	UActorComponent* HarvesterComp = Owner->GetComponentByClass(UHarvester::StaticClass());
	// 	if (IsValid(HarvesterComp))
	// 	{
	// 		UHarvester* Harvester = Cast<UHarvester>(HarvesterComp);
	// 		if (IsValid(Harvester))
	// 		{
	// 			return Harvester;
	// 		}
	// 		RTSFunctionLibrary::ReportFailedCastError(
	// 			"HarvesterComp",
	// 			"UHarvester", "ICommands::GetIsHarvester");
	// 	}
	// }
	// else
	// {
	// 	RTSFunctionLibrary::ReportFailedCastError("Owner",
	// 	                                          "AActor", "ICommands::GetIsHarvester");
	// }
	// return nullptr;
}


bool ICommands::GetHasCommandInQueue(const EAbilityID CommandToCheck)
{
	if (UCommandData* UnitCommandData = GetIsValidCommandData())
	{
		return UnitCommandData->GetHasCommandInQueue(CommandToCheck);
	}
	return false;
}

ECommandQueueError ICommands::ConvertToVehicle(const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();

	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	// No ability allowed check as this ability is not part of the command card.
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(EAbilityID::IdConvertToVehicle,
	                                                                        FVector::ZeroVector, nullptr,
	                                                                        FRotator::ZeroRotator);
	return Error;
}


ECommandQueueError ICommands::ScavengeObject(AActor* TargetObject, const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();

	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdScavenge))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(EAbilityID::IdScavenge, FVector::ZeroVector,
	                                                                        TargetObject,
	                                                                        FRotator::ZeroRotator);
	return Error;
}

ECommandQueueError ICommands::RepairActor(AActor* TargetObject, const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();

	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdRepair))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(EAbilityID::IdRepair, FVector::ZeroVector,
	                                                                        TargetObject,
	                                                                        FRotator::ZeroRotator);
	return Error;
}

ECommandQueueError ICommands::ReturnToBase(const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();

	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	if (not GetIsAbilityAllowedForUnit(EAbilityID::IdReturnToBase))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdReturnToBase, FVector::ZeroVector,
		nullptr,
		FRotator::ZeroRotator);
	return Error;
}


EAbilityID ICommands::GetActiveCommandID()
{
	// Get the current command of the unit
	if (const UCommandData* CommandData = GetIsValidCommandData())
	{
		return CommandData->GetCurrentlyActiveCommandType();
	}
	return EAbilityID::IdIdle;
}

bool ICommands::GetIsUnitIdle()
{
	return GetActiveCommandID() == EAbilityID::IdIdle;
}


void ICommands::ExecuteMoveCommand(const FVector MoveToLocation)
{
	// non-pure so no overwrite needed in base-classes that don't use this.
}

void ICommands::TerminateMoveCommand()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Terminated move command! (in ICommands)");
	}
}

void ICommands::ExecuteStopCommand()
{
}

void ICommands::ExecuteAttackCommand(AActor* TargetActor)
{
	// non-pure so no overwrite needed in base-classes that don't use this.
}

void ICommands::TerminateAttackCommand()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished attack command! (in ICommands)");
	}
}

void ICommands::ExecuteAttackGroundCommand(const FVector GroundLocation)
{
}

void ICommands::TerminateAttackGroundCommand()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished attack ground command! (in ICommands)");
	}
}

void ICommands::ExecutePatrolCommand(const FVector PatrolToLocation)
{
	// non-pure so no overwrite needed in base-classes that don't use this.
}

void ICommands::TerminatePatrolCommand()
{
	// non-pure so no overwrite needed in base-classes that don't use this.
}

void ICommands::ExecuteReverseCommand(const FVector ReverseToLocation)
{
	// non-pure so no overwrite needed in base-classes that don't use this.
}

void ICommands::TerminateRotateTowardsCommand()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished Rotate to command! (in ICommands)");
	}
}

void ICommands::ExecuteCreateBuildingCommand(const FVector BuildingLocation, const FRotator BuildingRotation)
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("start create building commands! (in ICommands)");
	}
}

void ICommands::TerminateCreateBuildingCommand()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished create building commands! (in ICommands)");
	}
}

void ICommands::ExecuteConvertToVehicleCommand()
{
}

void ICommands::TerminateConvertToVehicleCommand()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Terminate convert to vehicle command! (in ICommands)");
	}
}

void ICommands::ExecuteHarvestResourceCommand(ACPPResourceMaster* TargetResource)
{
}

void ICommands::ExecuteReturnCargoCommand()
{
}

void ICommands::TerminateReturnCargoCommand()
{
}

void ICommands::TerminateHarvestResourceCommand()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished harvest resource command! (in ICommands)");
	}
}

void ICommands::ExecutePickupItemCommand(AItemsMaster* TargetItem)
{
}

void ICommands::TerminatePickupItemCommand()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished pickup item command! (in ICommands)");
	}
}

void ICommands::ExecuteSwitchWeaponsCommand()
{
}

void ICommands::TerminateSwitchWeaponsCommand()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished Switch weapon command! (In ICommands)");
	}
}

void ICommands::ExecuteScavengeObject(AActor* TargetObject)
{
}

void ICommands::TerminateScavengeObject()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished Scavenge object command! (In ICommands)");
	}
}

void ICommands::ExecuteDigIn()
{
}

void ICommands::TerminateDigIn()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished DigIn command! (in ICommands)");
	}
}

void ICommands::ExecuteBreakCover()
{
}

void ICommands::TerminateBreakCover()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished BreakCover Command! (in ICommands)");
	}
}

void ICommands::ExecuteFireRockets()
{
	// Uses component, implement in childs.
}

void ICommands::TerminateFireRockets()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished FireRockets Command! (in ICommands)");
	}
}

void ICommands::ExecuteCancelFireRockets()
{
}

void ICommands::TerminateCancelFireRockets()
{
	// nothing to do, instant ability.
}

void ICommands::ExecuteRepairCommand(AActor* TargetActor)
{
}

void ICommands::TerminateRepairCommand()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished Repair Command! (in ICommands)");
	}
}

void ICommands::ExecuteReturnToBase()
{
}

void ICommands::TerminateReturnToBase()
{
}

void ICommands::ExecuteEnterCargoCommand(AActor* CarrierActor)
{
	// Non-pure; override in concrete controllers. Typically:
	// - validate CarrierActor
	// - start pathing/boarding logic
	// - when boarded, call DoneExecutingCommand(EAbilityID::IdEnterCargo)
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("ExecuteEnterCargoCommand (base)", FColor::Blue);
	}
}

void ICommands::TerminateEnterCargoCommand()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("TerminateEnterCargoCommand (base)", FColor::Blue);
	}
}

void ICommands::ExecuteExitCargoCommand()
{
	// Non-pure; override in concrete controllers. Typically:
	// - start disembark logic (pick egress, unattach, enable movement)
	// - when done, call DoneExecutingCommand(EAbilityID::IdExitCargo)
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("ExecuteExitCargoCommand (base)", FColor::Blue);
	}
}

void ICommands::TerminateExitCargoCommand()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("TerminateExitCargoCommand (base)", FColor::Blue);
	}
}

void ICommands::ExecuteCaptureCommand(AActor* CaptureTarget)
{
}

void ICommands::TerminateCaptureCommand()
{
}

void ICommands::NoQueue_ExecuteSetResourceConversionEnabled(const bool bEnabled)
{
}


void ICommands::ExecuteRotateTowardsCommand(const FRotator RotateToRotator, const bool IsQueueCommand)
{
	// non-pure so no overwrite needed in base-classes that don't use this.
}

void ICommands::TerminateReverseCommand()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished turn reverse command! (in ICommands)");
	}
}

void ICommands::SetUnitToIdle()
{
	if (UCommandData* UnitCommandData = GetIsValidCommandData())
	{
		// Terminate logic for the last active command type.
		const EAbilityID LastActiveCommand = UnitCommandData->GetCurrentActiveCommand();
		TerminateCommand(LastActiveCommand);
		// Clear command queue.
		UnitCommandData->ClearCommands();
		// Custom reset logic that is unit-specific.
		SetUnitToIdleSpecificLogic();
	}
}

void ICommands::OnUnitIdleAndNoNewCommands()
{
	if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Unit Idle and no commands left! (in ICommands)");
	}
	if (UCommandData* UnitCommandData = GetIsValidCommandData())
	{
		if (UnitCommandData->M_FinalRotation.bFinishedCommandQueueWithFinalRotation)
		{
			// Reset guard flag; the final rotation was just completed.
			UnitCommandData->M_FinalRotation.bFinishedCommandQueueWithFinalRotation = false;
		}
		else
		{
			OnUnitIdleAndNoNewCommandsDelegate.Broadcast();
		}
	}
}


void ICommands::DoneExecutingCommand(const EAbilityID AbilityFinished)
{
	if (UCommandData* CommandData = GetIsValidCommandData())
	{
		if (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Unit done executing command!", FColor::Red);
		}
		TerminateCommand(AbilityFinished);
		CommandData->ExecuteCommand(false);
	}
}

ECommandQueueError ICommands::EnterCargo(AActor* CarrierActor, const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (!IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}

	// Typically not on the command card; skip the card gate like CreateBuilding/ConvertToVehicle.
	// If you *do* want to lock it behind abilities, uncomment the check below.
	// if (!GetIsAbilityAllowedForUnit(EAbilityID::IdEnterCargo)) { return ECommandQueueError::AbilityNotAllowed; }

	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}

	return UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdEnterCargo,
		FVector::ZeroVector,
		CarrierActor,
		FRotator::ZeroRotator);
}

ECommandQueueError ICommands::ExitCargo(const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (!IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}

	if (!GetIsAbilityAllowedForUnit(EAbilityID::IdExitCargo))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}

	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}

	return UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdExitCargo,
		FVector::ZeroVector,
		nullptr,
		FRotator::ZeroRotator);
}

void ICommands::SetCommandQueueEnabled(const bool bSetEnabled)
{
	if (UCommandData* CommandData = GetIsValidCommandData())
	{
		if (bSetEnabled)
		{
			SetUnitToIdle();
		}
		CommandData->bM_IsCommandQueueEnabled = bSetEnabled;
	}
}

void ICommands::RotateTowardsFinalMovementRotation()
{
	if (UCommandData* CommandData = GetIsValidCommandData())
	{
		if (CommandData->M_FinalRotation.bIsSet)
		{
			// To guard the broadcast on unit idle, so we don't get a double call of finishing the rotation after
			// we finished a command queue and execute the final rotation.
			CommandData->M_FinalRotation.bFinishedCommandQueueWithFinalRotation = true;
			RotateTowards(CommandData->M_FinalRotation.Rotation, true);
			CommandData->M_FinalRotation.bIsSet = false;
		}
	}
}

void ICommands::ResetRotateTowardsFinalMovementRotation()
{
	if (UCommandData* CommandData = GetIsValidCommandData())
	{
		// Reset the flag, the rotation will be repopulated when the set flag is set too.
		CommandData->M_FinalRotation.bIsSet = false;
	}
}

void ICommands::TerminateCommand(const EAbilityID AbilityToKill)
{
	switch (AbilityToKill)
	{
	case EAbilityID::IdMove:
		TerminateMoveCommand();
		break;
	case EAbilityID::IdPatrol:
		TerminatePatrolCommand();
		break;
	case EAbilityID::IdAttack:
		TerminateAttackCommand();
		break;
	case EAbilityID::IdAttackGround:
		TerminateAttackGroundCommand();
		break;
	case EAbilityID::IdReverseMove:
		TerminateReverseCommand();
		break;
	case EAbilityID::IdRotateTowards:
		TerminateRotateTowardsCommand();
		break;
	case EAbilityID::IdIdle:
		break;
	case EAbilityID::IdCreateBuilding:
		TerminateCreateBuildingCommand();
		break;
	case EAbilityID::IdConvertToVehicle:
		TerminateConvertToVehicleCommand();
		break;
	case EAbilityID::IdHarvestResource:
		TerminateHarvestResourceCommand();
		break;
	case EAbilityID::IdReturnCargo:
		TerminateReturnCargoCommand();
		break;
	case EAbilityID::IdPickupItem:
		TerminatePickupItemCommand();
		break;
	case EAbilityID::IdSwitchWeapon:
		TerminateSwitchWeaponsCommand();
		break;
	case EAbilityID::IdScavenge:
		TerminateScavengeObject();
		break;
	case EAbilityID::IdDigIn:
		TerminateDigIn();
		break;
	case EAbilityID::IdBreakCover:
		TerminateBreakCover();
		break;
	case EAbilityID::IdFireRockets:
		TerminateFireRockets();
		break;
	case EAbilityID::IdCancelRocketFire:
		TerminateCancelFireRockets();
		break;
	case EAbilityID::IdRepair:
		TerminateRepairCommand();
		break;
	case EAbilityID::IdReturnToBase:
		TerminateReturnToBase();
		break;
	case EAbilityID::IdEnterCargo:
		TerminateEnterCargoCommand();
		break;
	case EAbilityID::IdExitCargo:
		TerminateExitCargoCommand();
		break;
	case EAbilityID::IdCapture:
		TerminateCaptureCommand();
		break;
	default:
		RTSFunctionLibrary::PrintString(
			"Command has no termination logic implemented."
			"\n at function TerminateCommand"
			"\n for command: " + Global_GetAbilityIDAsString(AbilityToKill));
		break;
	}
}

bool ICommands::GetIsAbilityAllowedForUnit(const EAbilityID AbilityToCheck)
{
	return GetUnitAbilities().Contains(AbilityToCheck);
}
