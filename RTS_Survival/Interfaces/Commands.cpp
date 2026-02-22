// Copyright (C) Bas Blokzijl - All rights reserved.


#include "Commands.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/CaptureMechanic/CaptureMechanicHelpers.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/PickupItems/Items/ItemsMaster.h"
#include "RTS_Survival/Resources/Resource.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/Units/Squads/Reinforcement/ReinforcementPoint.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Units/Aircraft/AirBase/AircraftOwnerComp/AircraftOwnerComp.h"
#include "RTS_Survival/Units/Squads/Reinforcement/SquadReinforcementComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedWeaponAbilityComponent/AttachedWeaponAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/TurretSwapComponent/TurretSwapComp.h"


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
	StopAbilityCooldownTimer();
	ClearCommands();
	UObject::BeginDestroy();
}


void UCommandData::SetAbilities(const TArray<FUnitAbilityEntry>& Abilities)
{
	TArray<FUnitAbilityEntry> ValidAbilities = Abilities;
	if (Abilities.Num() > DeveloperSettings::GamePlay::ActionUI::MaxAbilitiesForActionUI)
	{
		RTSFunctionLibrary::ReportError("The number of abilities exceeds the maximum allowed for the Action UI."
			"UCommandData::SetAbilities");
		ValidAbilities.SetNum(DeveloperSettings::GamePlay::ActionUI::MaxAbilitiesForActionUI);
	}
	M_Abilities = ValidAbilities;
}

TArray<EAbilityID> UCommandData::GetAbilityIds(const bool bExcludeNoAbility) const
{
	return FAbilityHelpers::ExtractAbilityIdsFromEntries(M_Abilities, bExcludeNoAbility);
}

bool UCommandData::SwapAbility(const EAbilityID OldAbility, const EAbilityID NewAbility)
{
	return SwapAbility(OldAbility, FAbilityHelpers::CreateAbilityEntryFromId(NewAbility));
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
	return AddAbility(FAbilityHelpers::CreateAbilityEntryFromId(NewAbility), AtIndex);
}

bool UCommandData::AddAbility(const FUnitAbilityEntry& NewAbility, const int32 AtIndex)
{
	if (NewAbility.AbilityId == EAbilityID::IdNoAbility)
	{
		return false;
	}
	if (GetAbilityIndexById(NewAbility.AbilityId) != INDEX_NONE)
	{
		if (FUnitAbilityEntry* Entry = GetAbilityEntry(NewAbility.AbilityId); Entry && Entry->CustomType == NewAbility.
			CustomType)
		{
			const FString CustomTypeAsBehaviourAbilityString =
				UEnum::GetValueAsString(static_cast<EBehaviourAbilityType>(NewAbility.CustomType));
			RTSFunctionLibrary::ReportError(
				TEXT("Attempted to add an ability that already exists: ") + Global_GetAbilityIDAsString(
					NewAbility.AbilityId) +
				TEXT(" in UCommandData::AddAbility") +
				"Custom type: " + FString::FromInt(NewAbility.CustomType)
				+ "\n As behaviour custom type: " + CustomTypeAsBehaviourAbilityString);
			return false;
		}
	}

	if (AtIndex == INDEX_NONE || AtIndex < 0)
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
			const FString OwnerName = M_Owner ? M_Owner->GetOwnerName() : "Unknown Owner";
			RTSFunctionLibrary::ReportError(
				TEXT("Attempted to add ability at index that is not empty: ") + FString::FromInt(AtIndex) +
				TEXT(" in UCommandData::AddAbility")
				+ "\n Owner: " + OwnerName);
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

FUnitAbilityEntry* UCommandData::GetAbilityEntry(const EAbilityID AbilityId)
{
	return M_Abilities.FindByPredicate([AbilityId](const FUnitAbilityEntry& AbilityEntry)
	{
		return AbilityEntry.AbilityId == AbilityId;
	});
}

FUnitAbilityEntry* UCommandData::GetAbilityEntryOfCustomType(const EAbilityID AbilityID, int32 CustomType)
{
	return M_Abilities.FindByPredicate([AbilityID, CustomType](const FUnitAbilityEntry& AbilityEntry)
	{
		return AbilityEntry.AbilityId == AbilityID && AbilityEntry.CustomType == CustomType;
	});
}

const FUnitAbilityEntry* UCommandData::GetAbilityEntry(const EAbilityID AbilityId) const
{
	return M_Abilities.FindByPredicate([AbilityId](const FUnitAbilityEntry& AbilityEntry)
	{
		return AbilityEntry.AbilityId == AbilityId;
	});
}

bool UCommandData::HasAbilityOnCooldown() const
{
	for (const FUnitAbilityEntry& AbilityEntry : M_Abilities)
	{
		if (AbilityEntry.CooldownRemaining > 0)
		{
			return true;
		}
	}

	return false;
}

void UCommandData::StartAbilityCooldownTimer()
{
	if (not HasAbilityOnCooldown())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (TimerManager.IsTimerActive(M_AbilityCooldownTimerHandle))
	{
		return;
	}
	UpdateActionUI();

	constexpr float AbilityCooldownTickRate = 1.0f;
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &UCommandData::AbilityCoolDownTick);
	TimerManager.SetTimer(M_AbilityCooldownTimerHandle, TimerDelegate, AbilityCooldownTickRate, true);
}

void UCommandData::StopAbilityCooldownTimer()
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_AbilityCooldownTimerHandle);
}

void UCommandData::AbilityCoolDownTick()
{
	bool bHasAnyCooldown = false;

	for (FUnitAbilityEntry& AbilityEntry : M_Abilities)
	{
		if (AbilityEntry.CooldownRemaining <= 0)
		{
			continue;
		}

		AbilityEntry.CooldownRemaining = FMath::Max(AbilityEntry.CooldownRemaining - 1, 0);

		if (AbilityEntry.CooldownRemaining > 0)
		{
			bHasAnyCooldown = true;
		}
	}

	if (not bHasAnyCooldown)
	{
		StopAbilityCooldownTimer();
	}
	// Notify UI manager to update ability UI for primary selected unit.
	UpdateActionUI();
}

void UCommandData::StartCooldownForCommand(const FQueueCommand& Command)
{
	FUnitAbilityEntry* AbilityEntry = nullptr;

	if (GetDoesQueuedCommandRequireSubtypeEntry(Command.CommandType))
	{
		AbilityEntry = GetAbilityEntryForQueuedCommandSubtype(Command);
	}
	else
	{
		AbilityEntry = GetAbilityEntry(Command.CommandType);
	}

	if (not AbilityEntry)
	{
		return;
	}

	if (AbilityEntry->CooldownDuration <= 0)
	{
		return;
	}

	AbilityEntry->CooldownRemaining = AbilityEntry->CooldownDuration;
	StartAbilityCooldownTimer();
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

int32 UCommandData::GetQueuedFieldConstructionCommandCount(const EFieldConstructionType ConstructionType) const
{
	int32 CommandCount = 0;
	for (int32 CommandIndex = 0; CommandIndex < NumCommands; CommandIndex++)
	{
		const FQueueCommand& QueuedCommand = M_TCommands[CommandIndex];
		if (QueuedCommand.CommandType != EAbilityID::IdFieldConstruction)
		{
			continue;
		}

		if (QueuedCommand.GetFieldConstructionSubtype() != ConstructionType)
		{
			continue;
		}

		CommandCount++;
	}

	return CommandCount;
}

void UCommandData::UpdateActionUI()
{
	if (GetIsPrimarySelected())
	{
		M_ActionUIManager->RequestUpdateAbilityUIForPrimary(M_Owner);
	}
}

bool UCommandData::StartCooldownOnAbility(const EAbilityID AbilityID, const int32 CustomType)
{
	for (auto& AbilityEntry : M_Abilities)
	{
		if (AbilityEntry.AbilityId == AbilityID && (CustomType == INDEX_NONE || AbilityEntry.CustomType == CustomType))
		{
			if (AbilityEntry.CooldownDuration <= 0)
			{
				return false;
			}

			AbilityEntry.CooldownRemaining = AbilityEntry.CooldownDuration;
			StartAbilityCooldownTimer();
			return true;
		}
	}
	return false;
}

const FQueueCommand* UCommandData::GetCurrentQueuedCommand() const
{
	if (CurrentIndex >= 0 && CurrentIndex < NumCommands)
	{
		return &M_TCommands[CurrentIndex];
	}
	return nullptr;
}


bool UCommandData::GetIsQueuedCommandStillAllowed(const FQueueCommand& QueuedCommand)
{
	const EAbilityID CommandType = QueuedCommand.CommandType;

	if (not IsAbilityRequiredOnCommandCard(CommandType))
	{
		// cannot use cooldown as it has no ability entry and is therefore always allowed.
		return true;
	}
	if (GetDoesQueuedCommandRequireSubtypeEntry(CommandType))
	{
		// Make sure for behaviours or the mode abilty that the specific subtype is still in the ability array
		// Multiple different abilities of these are possible and so we need to check for the specific one.
		const FUnitAbilityEntry* const SubtypeAbilityEntry = GetAbilityEntryForQueuedCommandSubtype(QueuedCommand);
		if (SubtypeAbilityEntry == nullptr)
		{
			RTSFunctionLibrary::ReportError(
				"Queued subtype ability command is no longer allowed: "
				+ Global_GetAbilityIDAsString(CommandType)
				+ GetQueuedCommandSubtypeSuffix(QueuedCommand));

			return false;
		}

		if (GetIsAbilityEntryOnCooldown(CommandType, SubtypeAbilityEntry,
		                                GetQueuedCommandSubtypeSuffix(QueuedCommand)))
		{
			return false;
		}

		return true;
	}

	// Check whether the queued ability is still in the ability array.
	if (not GetIsQueuedCommandAbilityIdStillOnUnit(CommandType))
	{
		return false;
	}

	const FUnitAbilityEntry* const AbilityEntry = GetAbilityEntry(CommandType);
	if (GetIsAbilityEntryOnCooldown(CommandType, AbilityEntry, FString{}))
	{
		return false;
	}

	return true;
}

bool UCommandData::GetIsQueuedCommandAbilityIdStillOnUnit(const EAbilityID AbilityId) const
{
	const TArray<EAbilityID>& UnitAbilities = GetAbilityIds();
	if (UnitAbilities.Contains(AbilityId))
	{
		return true;
	}

	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(
			"Queued command is no longer allowed: " + Global_GetAbilityIDAsString(AbilityId),
			FColor::Red);
	}

	return false;
}

bool UCommandData::GetDoesQueuedCommandRequireSubtypeEntry(const EAbilityID AbilityId) const
{
	return (AbilityId == EAbilityID::IdApplyBehaviour)
		|| (AbilityId == EAbilityID::IdActivateMode)
		|| (AbilityId == EAbilityID::IdDisableMode)
		|| (AbilityId == EAbilityID::IdFieldConstruction)
		|| (AbilityId == EAbilityID::IdThrowGrenade)
		|| (AbilityId == EAbilityID::IdCancelThrowGrenade)
		|| (AbilityId == EAbilityID::IdAimAbility)
		|| (AbilityId == EAbilityID::IdCancelAimAbility)
		|| (AbilityId == EAbilityID::IdAttachedWeapon)
		|| (AbilityId == EAbilityID::IdSwapTurret);
}

FUnitAbilityEntry* UCommandData::GetAbilityEntryForQueuedCommandSubtype(const FQueueCommand& QueuedCommand)
{
	return GetAbilityEntryOfCustomType(QueuedCommand.CommandType, QueuedCommand.CustomType);
}

FString UCommandData::GetQueuedCommandSubtypeSuffix(const FQueueCommand& QueuedCommand) const
{
	if (QueuedCommand.CommandType == EAbilityID::IdApplyBehaviour)
	{
		return " with behaviour type: " + UEnum::GetValueAsString(QueuedCommand.GetBehaviourAbilitySubtype());
	}

	if ((QueuedCommand.CommandType == EAbilityID::IdActivateMode) || (QueuedCommand.CommandType ==
		EAbilityID::IdDisableMode))
	{
		return " with mode type: " + UEnum::GetValueAsString(QueuedCommand.GetModeAbilitySubtype());
	}

	if (QueuedCommand.CommandType == EAbilityID::IdFieldConstruction)
	{
		return " with field construction type: " + UEnum::GetValueAsString(
			QueuedCommand.GetFieldConstructionSubtype());
	}

	if ((QueuedCommand.CommandType == EAbilityID::IdThrowGrenade)
		|| (QueuedCommand.CommandType == EAbilityID::IdCancelThrowGrenade))
	{
		return " with grenade type: " + UEnum::GetValueAsString(QueuedCommand.GetGrenadeAbilitySubtype());
	}

	if ((QueuedCommand.CommandType == EAbilityID::IdAimAbility)
		|| (QueuedCommand.CommandType == EAbilityID::IdCancelAimAbility))
	{
		return " with aim ability type: " + UEnum::GetValueAsString(QueuedCommand.GetAimAbilitySubtype());
	}

	if (QueuedCommand.CommandType == EAbilityID::IdAttachedWeapon)
	{
		return " with attached weapon type: " + UEnum::GetValueAsString(
			QueuedCommand.GetAttachedWeaponAbilitySubtype());
	}

	if (QueuedCommand.CommandType == EAbilityID::IdSwapTurret)
	{
		return " with turret swap type: " + UEnum::GetValueAsString(QueuedCommand.GetTurretSwapAbilitySubtype());
	}

	return FString{};
}

bool UCommandData::GetIsAbilityEntryOnCooldown(
	const EAbilityID AbilityId,
	const FUnitAbilityEntry* const AbilityEntry,
	const FString& SubtypeSuffix) const
{
	if (AbilityEntry == nullptr)
	{
		return false;
	}

	if (AbilityEntry->CooldownRemaining <= 0)
	{
		return false;
	}

	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		const FString DebugText =
			"Queued command is on cooldown: "
			+ Global_GetAbilityIDAsString(AbilityId)
			+ SubtypeSuffix;

		RTSFunctionLibrary::PrintString(DebugText, FColor::Red);
	}

	return true;
}

void UCommandData::OnCommandInQueueCancelled(const FQueueCommand& CancelledCommand)
{
	if (CancelledCommand.CommandType == EAbilityID::IdFieldConstruction)
	{
		// If we had a field construction command cancelled, we need to remove any static preview meshes related to it.
		AActor* StaticPreviewActor = CancelledCommand.TargetActor.IsValid()
			                             ? CancelledCommand.TargetActor.Get()
			                             : nullptr;
		if (StaticPreviewActor)
		{
			StaticPreviewActor->Destroy();
		}
	}
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
	const FRotator& Rotation,
	const int32 CustomType)
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
	NewCmd.CustomType = CustomType;

	// Insert at the end
	M_TCommands[NumCommands] = NewCmd;
	NumCommands++;


	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
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
	if (not M_Owner)
	{
		RTSFunctionLibrary::ReportError("UCommandData::ExecuteCommand - M_Owner is null");
		return;
	}

	if (not bExecuteCurrentCommand)
	{
		// Move to next
		CurrentIndex++;
		if (CurrentIndex >= NumCommands)
		{
			ClearCommands();
			if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
			{
				RTSFunctionLibrary::PrintString("No valid next command index; all done!", FColor::Blue);
			}
			M_Owner->OnUnitIdleAndNoNewCommands();
			return;
		}
	}

	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
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
		ExecuteCommand(false);
		return;
	}

	if (not GetIsQueuedCommandStillAllowed(Cmd))
	{
		OnCommandInQueueCancelled(Cmd);
		// This ability is now either on cooldown or entirely removed from the unit; skip it.
		ExecuteCommand(false);
		return;
	}

	StartCooldownForCommand(Cmd);

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
	case EAbilityID::IdReinforceSquad:
		{
			M_Owner->ExecuteReinforceCommand(Cmd.TargetActor.Get());
		}
		break;
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
	case EAbilityID::IdThrowGrenade:
		{
			M_Owner->ExecuteThrowGrenadeCommand(Cmd.TargetLocation, Cmd.GetGrenadeAbilitySubtype());
		}
		break;
	case EAbilityID::IdCancelThrowGrenade:
		{
			M_Owner->ExecuteCancelThrowGrenadeCommand(Cmd.GetGrenadeAbilitySubtype());
		}
		break;
	case EAbilityID::IdAimAbility:
		{
			M_Owner->ExecuteAimAbilityCommand(Cmd.TargetLocation, Cmd.GetAimAbilitySubtype());
		}
		break;
	case EAbilityID::IdAttachedWeapon:
		{
			M_Owner->ExecuteAttachedWeaponAbilityCommand(Cmd.TargetLocation, Cmd.GetAttachedWeaponAbilitySubtype());
		}
		break;
	case EAbilityID::IdSwapTurret:
		{
			M_Owner->ExecuteTurretSwapCommand(Cmd.GetTurretSwapAbilitySubtype());
		}
		break;
	case EAbilityID::IdCancelAimAbility:
		{
			M_Owner->ExecuteCancelAimAbilityCommand(Cmd.GetAimAbilitySubtype());
		}
		break;
	case EAbilityID::IdReturnToBase:
		{
			M_Owner->ExecuteReturnToBase();
		}
		break;
	case EAbilityID::IdRetreat:
		{
			M_Owner->ExecuteRetreatCommand(Cmd.TargetLocation);
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
		break;
	case EAbilityID::IdApplyBehaviour:
		{
			ExecuteBehaviourAbility(Cmd.GetBehaviourAbilitySubtype(), false);
		}
		break;
	case EAbilityID::IdActivateMode:
		{
			ExecuteModeAbility(Cmd.GetModeAbilitySubtype());
		}
		break;
	case EAbilityID::IdDisableMode:
		{
			ExecuteDisableModeAbility(Cmd.GetModeAbilitySubtype());
		}
		break;
	case EAbilityID::IdFieldConstruction:
		{
			AActor* StaticPreviewActor = Cmd.TargetActor.IsValid() ? Cmd.TargetActor.Get() : nullptr;
			M_Owner->ExecuteFieldConstructionCommand(Cmd.GetFieldConstructionSubtype(), Cmd.TargetLocation,
			                                         Cmd.TargetRotator, StaticPreviewActor);
		}
		break;
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
			const bool bIsFieldConstructionCommand = M_TCommands[i].CommandType == EAbilityID::IdFieldConstruction;
			if (bIsFieldConstructionCommand)
			{
				AActor* const TargetActor = M_TCommands[i].TargetActor.IsValid()
					                            ? M_TCommands[i].TargetActor.Get()
					                            : nullptr;
				if (TargetActor)
				{
					TargetActor->Destroy();
				}
			}
			M_TCommands[i] = FQueueCommand();
		}
	}
	NumCommands = 0;
	CurrentIndex = -1;
}

ECommandQueueError UCommandData::IsQueueActiveAndNoPatrol() const
{
	if (not bM_IsCommandQueueEnabled)
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

void UCommandData::ExecuteBehaviourAbility(const EBehaviourAbilityType BehaviourAbility, const bool bByPassQueue) const
{
	const UApplyBehaviourAbilityComponent* Comp = FAbilityHelpers::GetBehaviourAbilityCompOfType(
		BehaviourAbility, M_Owner->GetOwnerActor());
	if (not IsValid(Comp))
	{
		if (bByPassQueue)
		{
			return;
		}
		M_Owner->DoneExecutingCommand(EAbilityID::IdApplyBehaviour);
		return;
	}
	Comp->ExecuteBehaviourAbility();
	if (bByPassQueue)
	{
		return;
	}
	M_Owner->DoneExecutingCommand(EAbilityID::IdApplyBehaviour);
}

void UCommandData::ExecuteModeAbility(const EModeAbilityType ModeAbility) const
{
	if (not M_Owner)
	{
		return;
	}

	UModeAbilityComponent* Comp = FAbilityHelpers::GetModeAbilityCompOfType(
		ModeAbility, M_Owner->GetOwnerActor());
	if (not IsValid(Comp))
	{
		M_Owner->DoneExecutingCommand(EAbilityID::IdActivateMode);
		return;
	}

	Comp->ActivateMode();
}

void UCommandData::ExecuteDisableModeAbility(const EModeAbilityType ModeAbility) const
{
	if (not M_Owner)
	{
		return;
	}

	UModeAbilityComponent* Comp = FAbilityHelpers::GetModeAbilityCompOfType(
		ModeAbility, M_Owner->GetOwnerActor());
	if (not IsValid(Comp))
	{
		M_Owner->DoneExecutingCommand(EAbilityID::IdDisableMode);
		return;
	}

	Comp->DeactivateMode();
}

bool UCommandData::IsAbilityRequiredOnCommandCard(const EAbilityID CommandType) const
{
	switch (CommandType)
	{
	// Explicitly not on the command card (or gated via other rules).
	case EAbilityID::IdCreateBuilding:
	case EAbilityID::IdConvertToVehicle:
	case EAbilityID::IdAttackGround:
	case EAbilityID::IdReturnCargo:
	case EAbilityID::IdEnterCargo:
	case EAbilityID::IdCapture:
		return false;

	default:
		return true;
	}
}

void UCommandData::TerminateFieldConstructionCommand()
{
	const FQueueCommand* const CurrentCmd = GetCurrentQueuedCommand();
	if (CurrentCmd == nullptr)
	{
		return;
	}

	if (CurrentCmd->CommandType != EAbilityID::IdFieldConstruction)
	{
		return;
	}

	AActor* const StaticPreviewActor = CurrentCmd->TargetActor.Get();
	const EFieldConstructionType ConstructionType = CurrentCmd->GetFieldConstructionSubtype();

	M_Owner->TerminateFieldConstructionCommand(ConstructionType, StaticPreviewActor);
}

void ICommands::SetPrimarySelected(UActionUIManager* ActionUIManager)
{
	if (UCommandData* UnitCommandData = GetIsValidCommandData())
	{
		UnitCommandData->M_ActionUIManager = ActionUIManager;
	}
}

int32 ICommands::GetConstructionAbilityCount()
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not UnitCommandData)
	{
		return 0;
	}
	int32 Count = 0;
	for (const FUnitAbilityEntry& AbilityEntry : UnitCommandData->GetAbilities())
	{
		if (AbilityEntry.AbilityId == EAbilityID::IdFieldConstruction)
		{
			Count++;
		}
	}
	return Count;
}

int32 ICommands::GetQueuedFieldConstructionCommandCount(const EFieldConstructionType ConstructionType)
{
	const UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not UnitCommandData)
	{
		return 0;
	}
	return UnitCommandData->GetQueuedFieldConstructionCommandCount(ConstructionType);
}

void ICommands::UnstuckSquadMoveUp(const float ZOffset)
{
	return;
}

void ICommands::SetRTSOverlapEvasionEnabled(const bool bEnabled)
{
	
}

bool ICommands::GetIsUnitInCombat() const
{
	return false;
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
	InitAbilityArray(FAbilityHelpers::ConvertAbilityIdsToEntries(Abilities));
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
	UnitCommandData->UpdateActionUI();
}

void ICommands::SetUnitAbilitiesRunTime(const TArray<EAbilityID>& Abilities)
{
	SetUnitAbilitiesRunTime(FAbilityHelpers::ConvertAbilityIdsToEntries(Abilities));
}

bool ICommands::AddAbility(const EAbilityID NewAbility, const int32 AtIndex)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not UnitCommandData)
	{
		return false;
	}
	const bool bAdded = UnitCommandData->AddAbility(NewAbility, AtIndex);
	UnitCommandData->UpdateActionUI();
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
	UnitCommandData->UpdateActionUI();
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
	UnitCommandData->UpdateActionUI();
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
	UnitCommandData->UpdateActionUI();
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
	UnitCommandData->UpdateActionUI();
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

bool ICommands::StartCooldownOnAbility(const EAbilityID AbilityID, const int32 CustomType)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not UnitCommandData)
	{
		return false;
	}
	const bool bDidStartCooldown = UnitCommandData->StartCooldownOnAbility(AbilityID, CustomType);
	UnitCommandData->UpdateActionUI();
	return bDidStartCooldown;
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

	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdMove);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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

ECommandQueueError ICommands::Reinforce(const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	USquadReinforcementComponent* ReinforcementComp = FAbilityHelpers::GetReinforcementAbilityComp(GetOwnerActor());

	if (not IsValid(ReinforcementComp))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	UReinforcementPoint* ActivePoint = ReinforcementComp->GetActiveReinforcementPoint();
	if (not IsValid(ActivePoint))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}

	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdReinforceSquad);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdReinforceSquad, FVector::ZeroVector, ActivePoint->GetOwner(), FRotator::ZeroRotator);
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
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdAttack);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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
	if (not FCaptureMechanicHelpers::GetValidCaptureInterface(CaptureTarget) || not GetCanCaptureUnits())
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdCapture, FVector::ZeroVector, CaptureTarget, FRotator::ZeroRotator);
	return Error;
}

ECommandQueueError ICommands::ActivateBehaviourAbility(const EBehaviourAbilityType BehaviourAbility,
                                                       const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	FUnitAbilityEntry BehaviourAbilityEntry;
	if (not FAbilityHelpers::GetHasBehaviourAbility(UnitCommandData->GetAbilities(), BehaviourAbility,
	                                                BehaviourAbilityEntry))
	{
	}
	if (BehaviourAbilityEntry.CooldownRemaining > 0)
	{
		return ECommandQueueError::AbilityOnCooldown;
	}
	// If the flag was not set then the unit has added the ability with shift meaning we want it to happen at the end of the queue.
	// else: the ability should fire immediately without stopping the current queue ability.
	if (not bSetUnitToIdle)
	{
		return UnitCommandData->AddAbilityToTCommands(
			EAbilityID::IdApplyBehaviour,
			FVector::ZeroVector,
			/*TargetActor=*/nullptr,
			/*Rotation=*/FRotator::ZeroRotator,
			static_cast<int32>(BehaviourAbility));
	}

	// no queuing but do start the cooldown if needed to prevent spam.
	if (BehaviourAbilityEntry.CooldownDuration > 0)
	{
		if (not UnitCommandData->StartCooldownOnAbility(EAbilityID::IdApplyBehaviour,
		                                                static_cast<int32>(BehaviourAbility)))
		{
			RTSFunctionLibrary::ReportError(
				"Failed to start cooldown on behaviour ability when activating without queueing"
				"\n eventhough the behaviour has a non zero cooldown: " + UEnum::GetValueAsString(BehaviourAbility) +
				" with: " + FString::FromInt(BehaviourAbilityEntry.CooldownDuration));
		}
	}
	UnitCommandData->ExecuteBehaviourAbility(BehaviourAbility, true);
	return ECommandQueueError::NoError;
}

ECommandQueueError ICommands::FieldConstruction(const EFieldConstructionType FieldConstruction,
                                                const bool bSetUnitToIdle, const FVector& ConstructionLocation,
                                                const FRotator& ConstructionRotation, AActor*
                                                StaticPreview)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	FUnitAbilityEntry OutFieldConstructionAbilityEntry;
	if (not FAbilityHelpers::GetHasFieldConstructionAbility(UnitCommandData->GetAbilities(), FieldConstruction,
	                                                        OutFieldConstructionAbilityEntry))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	// Field construction abilities may have a cooldown.
	if (OutFieldConstructionAbilityEntry.CooldownRemaining > 0)
	{
		return ECommandQueueError::AbilityOnCooldown;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	return UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdFieldConstruction,
		ConstructionLocation,
		/*TargetActor=*/StaticPreview,
		ConstructionRotation,
		static_cast<int32>(FieldConstruction));
}

ECommandQueueError ICommands::ActivateModeAbility(const EModeAbilityType ModeAbilityType, const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}

	FUnitAbilityEntry ModeAbilityEntry;
	if (not FAbilityHelpers::GetHasModeAbility(UnitCommandData->GetAbilities(), ModeAbilityType, ModeAbilityEntry))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}

	// Not allowed because the unit can only deactivate the mode now.
	if (ModeAbilityEntry.AbilityId != EAbilityID::IdActivateMode)
	{
		return ECommandQueueError::AbilityNotAllowed;
	}

	if (ModeAbilityEntry.CooldownRemaining > 0)
	{
		return ECommandQueueError::AbilityOnCooldown;
	}

	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}

	return UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdActivateMode,
		FVector::ZeroVector,
		/*TargetActor=*/nullptr,
		/*Rotation=*/FRotator::ZeroRotator,
		static_cast<int32>(ModeAbilityType));
}

ECommandQueueError ICommands::DisableModeAbility(const EModeAbilityType ModeAbilityType, const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}

	FUnitAbilityEntry ModeAbilityEntry;
	if (not FAbilityHelpers::GetHasModeAbility(UnitCommandData->GetAbilities(), ModeAbilityType, ModeAbilityEntry))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}

	if (ModeAbilityEntry.AbilityId != EAbilityID::IdDisableMode)
	{
		return ECommandQueueError::AbilityNotAllowed;
	}

	if (ModeAbilityEntry.CooldownRemaining > 0)
	{
		return ECommandQueueError::AbilityOnCooldown;
	}

	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}

	return UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdDisableMode,
		FVector::ZeroVector,
		/*TargetActor=*/nullptr,
		/*Rotation=*/FRotator::ZeroRotator,
		static_cast<int32>(ModeAbilityType));
}

ECommandQueueError ICommands::AttackGround(const FVector& Location, const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	// Check for regular attack ability; attack ground is not on the command card.
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdAttack);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdPatrol);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdReverseMove);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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

	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdRotateTowards);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdHarvestResource);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdHarvestResource);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdPickupItem);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdDigIn);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdBreakCover);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdSwitchWeapon);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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

ECommandQueueError ICommands::ThrowGrenade(const FVector& Location, const bool bSetUnitToIdle,
                                           const EGrenadeAbilityType GrenadeAbilityType)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	FUnitAbilityEntry GrenadeAbilityEntry;
	if (not FAbilityHelpers::GetHasGrenadeAbility(UnitCommandData->GetAbilities(), EAbilityID::IdThrowGrenade,
	                                              GrenadeAbilityType, GrenadeAbilityEntry))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (GrenadeAbilityEntry.CooldownRemaining > 0)
	{
		return ECommandQueueError::AbilityOnCooldown;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdThrowGrenade, Location, nullptr,
		FRotator::ZeroRotator, static_cast<int32>(GrenadeAbilityType));
	return Error;
}

ECommandQueueError ICommands::CancelThrowingGrenade(const bool bSetUnitToIdle,
                                                    const EGrenadeAbilityType GrenadeAbilityType)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	FUnitAbilityEntry GrenadeAbilityEntry;
	if (not FAbilityHelpers::GetHasGrenadeAbility(UnitCommandData->GetAbilities(), EAbilityID::IdCancelThrowGrenade,
	                                              GrenadeAbilityType, GrenadeAbilityEntry))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (GrenadeAbilityEntry.CooldownRemaining > 0)
	{
		return ECommandQueueError::AbilityOnCooldown;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdCancelThrowGrenade, FVector::ZeroVector, nullptr,
		FRotator::ZeroRotator, static_cast<int32>(GrenadeAbilityType));
	return Error;
}

ECommandQueueError ICommands::AimAbility(const FVector& Location, const bool bSetUnitToIdle,
                                         const EAimAbilityType AimAbilityType)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	FUnitAbilityEntry AimAbilityEntry;
	if (not FAbilityHelpers::GetHasAimAbility(UnitCommandData->GetAbilities(), EAbilityID::IdAimAbility,
	                                          AimAbilityType, AimAbilityEntry))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (AimAbilityEntry.CooldownRemaining > 0)
	{
		return ECommandQueueError::AbilityOnCooldown;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdAimAbility, Location, nullptr, FRotator::ZeroRotator, static_cast<int32>(AimAbilityType));
	return Error;
}

ECommandQueueError ICommands::CancelAimAbility(const bool bSetUnitToIdle, const EAimAbilityType AimAbilityType)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	FUnitAbilityEntry AimAbilityEntry;
	if (not FAbilityHelpers::GetHasAimAbility(UnitCommandData->GetAbilities(), EAbilityID::IdCancelAimAbility,
	                                          AimAbilityType, AimAbilityEntry))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	if (AimAbilityEntry.CooldownRemaining > 0)
	{
		return ECommandQueueError::AbilityOnCooldown;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdCancelAimAbility, FVector::ZeroVector, nullptr, FRotator::ZeroRotator,
		static_cast<int32>(AimAbilityType));
	return Error;
}

ECommandQueueError ICommands::FireAttachedWeaponAbility(const FVector& TargetLocation, const bool bSetUnitToIdle,
                                                        const EAttachWeaponAbilitySubType AttachedWeaponAbilityType)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}

	FUnitAbilityEntry AbilityEntry;
	if (not FAbilityHelpers::GetHasAttachedWeaponAbility(UnitCommandData->GetAbilities(), EAbilityID::IdAttachedWeapon,
	                                                     AttachedWeaponAbilityType, AbilityEntry))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}

	if (AbilityEntry.CooldownRemaining > 0)
	{
		return ECommandQueueError::AbilityOnCooldown;
	}

	AActor* OwnerActor = GetOwnerActor();
	UAttachedWeaponAbilityComponent* AbilityComponent = FAbilityHelpers::GetAttachedWeaponAbilityComponent(
		AttachedWeaponAbilityType, OwnerActor);
	if (not IsValid(AbilityComponent))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}

	const float AbilityRange = AbilityComponent->GetAttachedWeaponAbilityRange();
	const FVector OwnerLocation = GetOwnerLocation();
	const float DistanceSquared = FVector::DistSquared(OwnerLocation, TargetLocation);
	if (AbilityRange <= 0.0f || DistanceSquared > FMath::Square(AbilityRange))
	{
		return ECommandQueueError::AbilityNotInCastRange;
	}

	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}

	const ECommandQueueError Error = UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdAttachedWeapon,
		TargetLocation,
		nullptr,
		FRotator::ZeroRotator,
		static_cast<int32>(AttachedWeaponAbilityType));
	return Error;
}

ECommandQueueError ICommands::SwapTurret(const bool bSetUnitToIdle, const ETurretSwapAbility TurretSwapAbilityType)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}

	FUnitAbilityEntry TurretSwapAbilityEntry;
	if (not FAbilityHelpers::GetHasTurretSwapAbility(UnitCommandData->GetAbilities(), TurretSwapAbilityType,
	                                                 TurretSwapAbilityEntry))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}

	if (TurretSwapAbilityEntry.CooldownRemaining > 0)
	{
		return ECommandQueueError::AbilityOnCooldown;
	}

	UTurretSwapComp* SwapComp = FAbilityHelpers::GetTurretSwapAbilityComponent(TurretSwapAbilityType, GetOwnerActor());
	if (not IsValid(SwapComp))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}

	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}

	return UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdSwapTurret,
		FVector::ZeroVector,
		nullptr,
		FRotator::ZeroRotator,
		static_cast<int32>(TurretSwapAbilityType));
}

ECommandQueueError ICommands::FireRockets(const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();

	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdFireRockets);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdCancelRocketFire);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdScavenge);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdRepair);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdReturnToBase);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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

ECommandQueueError ICommands::RetreatToLocation(const FVector& RetreatLocation, const bool bSetUnitToIdle)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();

	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}
	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdRetreat);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
	}
	if (bSetUnitToIdle)
	{
		SetUnitToIdle();
	}
	if (UnitCommandData->GetHasCommandInQueue(EAbilityID::IdRetreat))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}
	return UnitCommandData->AddAbilityToTCommands(
		EAbilityID::IdRetreat, RetreatLocation,
		nullptr,
		FRotator::ZeroRotator);
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
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Terminated move command! (in ICommands)");
	}
}

void ICommands::ExecuteReinforceCommand(AActor* ReinforcementTarget)
{
}

void ICommands::TerminateReinforceCommand()
{
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
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished attack command! (in ICommands)");
	}
}

void ICommands::ExecuteAttackGroundCommand(const FVector GroundLocation)
{
}

void ICommands::TerminateAttackGroundCommand()
{
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
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
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished Rotate to command! (in ICommands)");
	}
}

void ICommands::ExecuteCreateBuildingCommand(const FVector BuildingLocation, const FRotator BuildingRotation)
{
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("start create building commands! (in ICommands)");
	}
}

void ICommands::TerminateCreateBuildingCommand()
{
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished create building commands! (in ICommands)");
	}
}

void ICommands::ExecuteConvertToVehicleCommand()
{
}

void ICommands::TerminateConvertToVehicleCommand()
{
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
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
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished harvest resource command! (in ICommands)");
	}
}

void ICommands::ExecutePickupItemCommand(AItemsMaster* TargetItem)
{
}

void ICommands::TerminatePickupItemCommand()
{
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished pickup item command! (in ICommands)");
	}
}

void ICommands::ExecuteSwitchWeaponsCommand()
{
}

void ICommands::TerminateSwitchWeaponsCommand()
{
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished Switch weapon command! (In ICommands)");
	}
}

void ICommands::ExecuteScavengeObject(AActor* TargetObject)
{
}

void ICommands::TerminateScavengeObject()
{
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished Scavenge object command! (In ICommands)");
	}
}

void ICommands::ExecuteDigIn()
{
}

void ICommands::TerminateDigIn()
{
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished DigIn command! (in ICommands)");
	}
}

void ICommands::ExecuteBreakCover()
{
}

void ICommands::TerminateBreakCover()
{
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
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
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
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


void ICommands::ExecuteThrowGrenadeCommand(const FVector TargetLocation,
                                           const EGrenadeAbilityType GrenadeAbilityType)
{
}

void ICommands::TerminateThrowGrenadeCommand(const EGrenadeAbilityType GrenadeAbilityType)
{
}

void ICommands::ExecuteCancelThrowGrenadeCommand(const EGrenadeAbilityType GrenadeAbilityType)
{
}

void ICommands::TerminateCancelThrowGrenadeCommand(const EGrenadeAbilityType GrenadeAbilityType)
{
}

void ICommands::ExecuteAimAbilityCommand(const FVector TargetLocation, const EAimAbilityType AimAbilityType)
{
}

void ICommands::TerminateAimAbilityCommand(const EAimAbilityType AimAbilityType)
{
}

void ICommands::ExecuteAttachedWeaponAbilityCommand(const FVector TargetLocation,
                                                    const EAttachWeaponAbilitySubType AttachedWeaponAbilityType)
{
}

void ICommands::TerminateAttachedWeaponAbilityCommand(const EAttachWeaponAbilitySubType AttachedWeaponAbilityType)
{
}

void ICommands::ExecuteTurretSwapCommand(const ETurretSwapAbility TurretSwapAbilityType)
{
}

void ICommands::TerminateTurretSwapCommand(const ETurretSwapAbility TurretSwapAbilityType)
{
}

void ICommands::ExecuteCancelAimAbilityCommand(const EAimAbilityType AimAbilityType)
{
}

void ICommands::TerminateCancelAimAbilityCommand(const EAimAbilityType AimAbilityType)
{
}

void ICommands::ExecuteRepairCommand(AActor* TargetActor)
{
}

void ICommands::TerminateRepairCommand()
{
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
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

void ICommands::ExecuteRetreatCommand(const FVector RetreatLocation)
{
	static_cast<void>(RetreatLocation);
}

void ICommands::TerminateRetreatCommand()
{
}

void ICommands::ExecuteEnterCargoCommand(AActor* CarrierActor)
{
	// Non-pure; override in concrete controllers. Typically:
	// - validate CarrierActor
	// - start pathing/boarding logic
	// - when boarded, call DoneExecutingCommand(EAbilityID::IdEnterCargo)
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("ExecuteEnterCargoCommand (base)", FColor::Blue);
	}
}

void ICommands::TerminateEnterCargoCommand()
{
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("TerminateEnterCargoCommand (base)", FColor::Blue);
	}
}

void ICommands::ExecuteExitCargoCommand()
{
	// Non-pure; override in concrete controllers. Typically:
	// - start disembark logic (pick egress, unattach, enable movement)
	// - when done, call DoneExecutingCommand(EAbilityID::IdExitCargo)
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("ExecuteExitCargoCommand (base)", FColor::Blue);
	}
}

void ICommands::TerminateExitCargoCommand()
{
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
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

void ICommands::ExecuteFieldConstructionCommand(const EFieldConstructionType FieldConstruction,
                                                const FVector& ConstructionLocation,
                                                const FRotator& ConstructionRotation, AActor* StaticPreviewActor)
{
}

void ICommands::TerminateFieldConstructionCommand(EFieldConstructionType FieldConstructionType,
                                                  AActor* StaticPreviewActor)
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
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
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
	if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
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
		if constexpr (DeveloperSettings::Debugging::GCommands_Compile_DebugSymbols)
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
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}

	// Typically not on the command card; skip the card gate like CreateBuilding/ConvertToVehicle.
	// If you *do* want to lock it behind abilities, uncomment the check below.
	// const ECommandQueueError AbilityError = GetIsAbilityAllowedForUnit(EAbilityID::IdEnterCargo);
	// if (AbilityError != ECommandQueueError::NoError) { return AbilityError; }

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
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}

	const ECommandQueueError AbilityError = GetIsAbilityOnCommandCardAndNotOnCooldown(EAbilityID::IdExitCargo);
	if (AbilityError != ECommandQueueError::NoError)
	{
		return AbilityError;
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
	case EAbilityID::IdThrowGrenade:
		{
			UCommandData* CommandData = GetIsValidCommandData();
			if (not CommandData)
			{
				return;
			}

			const FQueueCommand* CurrentCommand = CommandData->GetCurrentQueuedCommand();
			if (CurrentCommand == nullptr)
			{
				return;
			}

			TerminateThrowGrenadeCommand(CurrentCommand->GetGrenadeAbilitySubtype());
		}
		break;
	case EAbilityID::IdCancelThrowGrenade:
		{
			UCommandData* CommandData = GetIsValidCommandData();
			if (not CommandData)
			{
				return;
			}

			const FQueueCommand* CurrentCommand = CommandData->GetCurrentQueuedCommand();
			if (CurrentCommand == nullptr)
			{
				return;
			}

			TerminateCancelThrowGrenadeCommand(CurrentCommand->GetGrenadeAbilitySubtype());
		}
		break;
	case EAbilityID::IdAimAbility:
		{
			UCommandData* CommandData = GetIsValidCommandData();
			if (not CommandData)
			{
				return;
			}

			const FQueueCommand* CurrentCommand = CommandData->GetCurrentQueuedCommand();
			if (CurrentCommand == nullptr)
			{
				return;
			}

			TerminateAimAbilityCommand(CurrentCommand->GetAimAbilitySubtype());
		}
		break;
	case EAbilityID::IdAttachedWeapon:
		{
			UCommandData* CommandData = GetIsValidCommandData();
			if (not CommandData)
			{
				return;
			}

			const FQueueCommand* CurrentCommand = CommandData->GetCurrentQueuedCommand();
			if (CurrentCommand == nullptr)
			{
				return;
			}

			TerminateAttachedWeaponAbilityCommand(CurrentCommand->GetAttachedWeaponAbilitySubtype());
		}
		break;
	case EAbilityID::IdSwapTurret:
		{
			UCommandData* CommandData = GetIsValidCommandData();
			if (not CommandData)
			{
				return;
			}

			const FQueueCommand* CurrentCommand = CommandData->GetCurrentQueuedCommand();
			if (CurrentCommand == nullptr)
			{
				return;
			}

			TerminateTurretSwapCommand(CurrentCommand->GetTurretSwapAbilitySubtype());
		}
		break;
	case EAbilityID::IdCancelAimAbility:
		{
			UCommandData* CommandData = GetIsValidCommandData();
			if (not CommandData)
			{
				return;
			}

			const FQueueCommand* CurrentCommand = CommandData->GetCurrentQueuedCommand();
			if (CurrentCommand == nullptr)
			{
				return;
			}

			TerminateCancelAimAbilityCommand(CurrentCommand->GetAimAbilitySubtype());
		}
		break;
	case EAbilityID::IdRepair:
		TerminateRepairCommand();
		break;
	case EAbilityID::IdReturnToBase:
		TerminateReturnToBase();
		break;
	case EAbilityID::IdRetreat:
		TerminateRetreatCommand();
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
	case EAbilityID::IdApplyBehaviour:
		// No terminate; behaviour auto expires.
		break;
	case EAbilityID::IdActivateMode:
		break;
	case EAbilityID::IdDisableMode:
		break;
	case EAbilityID::IdReinforceSquad:
		TerminateReinforceCommand();
		break;
	case EAbilityID::IdFieldConstruction:
		{
			UCommandData* CommandData = GetIsValidCommandData();
			if (not CommandData)
			{
				return;
			}
			// Will call back the terminate logic on the Owner but with the preview actor and the subtype
			// so we know which ability was cancelled.
			CommandData->TerminateFieldConstructionCommand();
		}
		break;
	default:
		RTSFunctionLibrary::PrintString(
			"Command has no termination logic implemented."
			"\n at function TerminateCommand"
			"\n for command: " + Global_GetAbilityIDAsString(AbilityToKill));
		break;
	}
}

ECommandQueueError ICommands::GetIsAbilityOnCommandCardAndNotOnCooldown(const EAbilityID AbilityToCheck)
{
	UCommandData* UnitCommandData = GetIsValidCommandData();
	if (not IsValid(UnitCommandData))
	{
		return ECommandQueueError::CommandDataInvalid;
	}

	const TArray<EAbilityID> UnitAbilities = UnitCommandData->GetAbilityIds();
	if (not UnitAbilities.Contains(AbilityToCheck))
	{
		return ECommandQueueError::AbilityNotAllowed;
	}

	const FUnitAbilityEntry* AbilityEntry = UnitCommandData->GetAbilityEntry(AbilityToCheck);
	if (AbilityEntry && AbilityEntry->CooldownRemaining > 0)
	{
		return ECommandQueueError::AbilityOnCooldown;
	}

	return ECommandQueueError::NoError;
}
