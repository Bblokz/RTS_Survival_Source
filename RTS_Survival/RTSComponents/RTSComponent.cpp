// Copyright (C) Bas Blokzijl - All rights reserved.


#include "RTSComponent.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"

#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/MasterObjects/HealthBase/HPActorObjectsMaster.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpPawnMaster.h"
#include "RTS_Survival/UnitTests/TestTurrets/ATestTurretOwner.h"
#include "RTS_Survival/Utils/CollisionSetup/RTSPlayerCollision.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/Turret/StandaloneTurret/StandaloneTurret.h"
#include "TacticalAIComponents/TacticalAISquad/EnemySquadTacticalAI.h"
#include "TacticalAIComponents/TacticalAITank/EnemyTankTacticalAI.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BxpBonuses/PanzerwerferReloadBonus/PanzerwerferReloadBonusSubsystem.h"


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
	if(M_EnemyTatcicalAIComp.IsValid())
	{
		M_EnemyTatcicalAIComp->OnUnitInCombat();
	}
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
	case EAllUnitType::UNType_StandaloneTurret:
		bOutIsValidString = true;
		return StaticEnum<EStandaloneTurretSubtype>()->GetDisplayNameTextByValue(
			static_cast<int64>(GetSubtypeAsStandaloneTurretSubtype())).ToString();
	case EAllUnitType::UNType_GarrisonBuilding:
		bOutIsValidString = true;
		return "Apartment Building";
	}
	bOutIsValidString = false;
	return "None";
}

void URTSComponent::DeregisterFromGameState() const
{
	if (GetWorld() == nullptr)
	{
		return;
	}
	AActor* ParentActor = GetOwner();
	UGameUnitManager* GameUnitManager = FindGameUnitManager();
	if (not IsValid(ParentActor) || not IsValid(GameUnitManager) || not bAddToGameState)
	{
		RTSFunctionLibrary::ReportError("Could not obtain valid"
			"GameUnitManager in URTSComponent::DeregisterFromGameState");
		return;
	}
	RemoveOwnerFromUnitManager(*GameUnitManager, *ParentActor, OwningPlayer);
}

UGameUnitManager* URTSComponent::FindGameUnitManager() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}
	const ACPPGameState* GameState = Cast<ACPPGameState>(World->GetGameState());
	if (not IsValid(GameState))
	{
		return nullptr;
	}
	return GameState->GetGameUnitManager();
}

bool URTSComponent::AddOwnerToUnitManager(UGameUnitManager& GameUnitManager, AActor& ParentActor,
                                          const uint8 Player) const
{
	if (ASquadUnit* SquadUnit = Cast<ASquadUnit>(&ParentActor))
	{
		GameUnitManager.AddSquadUnitForPlayer(SquadUnit, Player);
		return true;
	}
	if (ATankMaster* Tank = Cast<ATankMaster>(&ParentActor))
	{
		GameUnitManager.AddTankForPlayer(Tank, Player);
		return true;
	}
	if (AAircraftMaster* Aircraft = Cast<AAircraftMaster>(&ParentActor))
	{
		GameUnitManager.AddAircraftForPlayer(Aircraft, Player);
		return true;
	}
	if (ABuildingExpansion* Bxp = Cast<ABuildingExpansion>(&ParentActor))
	{
		GameUnitManager.AddBxpForPlayer(Bxp, Player);
		return true;
	}
	// Test turret owners, standalone turrets and every other pawn or actor with health share the generic list.
	if (ParentActor.IsA(ATestTurretOwner::StaticClass())
		|| ParentActor.IsA(AHpPawnMaster::StaticClass())
		|| ParentActor.IsA(AHPActorObjectsMaster::StaticClass()))
	{
		GameUnitManager.AddActorForPlayer(&ParentActor, Player);
		return true;
	}
	return false;
}

void URTSComponent::RemoveOwnerFromUnitManager(UGameUnitManager& GameUnitManager, AActor& ParentActor,
                                               const uint8 Player) const
{
	if (ASquadUnit* SquadUnit = Cast<ASquadUnit>(&ParentActor))
	{
		GameUnitManager.RemoveSquadUnitForPlayer(SquadUnit, Player);
		return;
	}
	if (ATankMaster* Tank = Cast<ATankMaster>(&ParentActor))
	{
		GameUnitManager.RemoveTankForPlayer(Tank, Player);
		return;
	}
	if (AAircraftMaster* Aircraft = Cast<AAircraftMaster>(&ParentActor))
	{
		GameUnitManager.RemoveAircraftForPlayer(Aircraft, Player);
		return;
	}
	if (ABuildingExpansion* Bxp = Cast<ABuildingExpansion>(&ParentActor))
	{
		GameUnitManager.RemoveBxpForPlayer(Bxp, Player);
		return;
	}
	GameUnitManager.RemoveActorForPlayer(&ParentActor, Player);
}

void URTSComponent::MoveOwnerInGameState(const uint8 OldOwningPlayer, const uint8 NewOwningPlayer) const
{
	AActor* ParentActor = GetOwner();
	UGameUnitManager* GameUnitManager = FindGameUnitManager();
	// Silent: an owner that is not kept in the game state has nothing to move.
	if (not bAddToGameState || not IsValid(ParentActor) || not IsValid(GameUnitManager))
	{
		return;
	}
	// Moves the unit between the lists of the two players (target search, tech counts, base clusters).
	RemoveOwnerFromUnitManager(*GameUnitManager, *ParentActor, OldOwningPlayer);
	AddOwnerToUnitManager(*GameUnitManager, *ParentActor, NewOwningPlayer);
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
	RecountSubtypeInGameState();
	OnSubTypeInitialized.Broadcast();
}

void URTSComponent::RecountSubtypeInGameState() const
{
	// Before BeginPlay the unit has not registered yet; it is counted under this subtype when it does.
	if (not HasBegunPlay() || not bAddToGameState || not IsValid(GetOwner()))
	{
		return;
	}
	UGameUnitManager* const GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		return;
	}
	GameUnitManager->RecountUnitSubtype(GetOwner());
}

void URTSComponent::SetStandaloneTurretSubtype(const EStandaloneTurretSubtype NewStandaloneTurretSubtype)
{
	if (NewStandaloneTurretSubtype == EStandaloneTurretSubtype::StandaloneTurret_None)
	{
		RTSFunctionLibrary::ReportError(
			"No standalone turret subtype set for unit: " + GetOwner()->GetName());
		return;
	}

	M_UnitSubType = static_cast<uint8>(NewStandaloneTurretSubtype);
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

EStandaloneTurretSubtype URTSComponent::GetSubtypeAsStandaloneTurretSubtype() const
{
	return static_cast<EStandaloneTurretSubtype>(M_UnitSubType);
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
		if (OwningPlayer != 0)
		{
			(void)AddUnitToGameState();
		}
	}
	// Marks the bodies of the unit with its owner so the shots of that player and its allies pass through them.
	// Neutral things carry no mark.
	if (not FRTSPlayers::IsNeutralOwner(OwningPlayer))
	{
		FRTSPlayerCollision::ApplyOwnerMaskToActor(GetOwner(), OwningPlayer);
	}
	DetermineCreateTacticalAI();
}

void URTSComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_UnitInCombatTimerHandle);
	}
}


void URTSComponent::SetOwningPlayerRuntime(const uint8 NewOwningPlayer)
{
	ChangeOwningPlayer(NewOwningPlayer);
}


void URTSComponent::ChangeOwningPlayer(const uint8 NewOwningPlayer)
{
	if (FRTSPlayers::IsNeutralOwner(NewOwningPlayer) || OwningPlayer == NewOwningPlayer)
	{
		return;
	}
	const uint8 OldOwningPlayer = OwningPlayer;
	OwningPlayer = NewOwningPlayer;
	// Before BeginPlay nothing is set up for the old owner yet: BeginPlay registers the unit, marks its collision
	// and creates its tactical AI, all for the owner it has by then.
	if (not HasBegunPlay())
	{
		return;
	}
	MoveOwnerInGameState(OldOwningPlayer, NewOwningPlayer);
	UpdateTacticalAIForOwner();
	// Between the human and an AI player the collision side changes too; between AI players only the owner bit.
	FRTSPlayerCollision::ApplySideChangeToActor(GetOwner(), OldOwningPlayer, NewOwningPlayer);
	FRTSPlayerCollision::ApplyOwnerMaskToActor(GetOwner(), NewOwningPlayer);
	// Weapons that hold this unit as a target check again whether it is still hostile.
	FRTSPlayers::NotifyUnitOwnerChanged();
	OnOwningPlayerChanged.Broadcast(OldOwningPlayer, NewOwningPlayer);
}

bool URTSComponent::AddUnitToGameState() const
{
	if (GetWorld() == nullptr)
	{
		return false;
	}
	UGameUnitManager* GameUnitManager = FindGameUnitManager();
	AActor* ParentActor = GetOwner();
	if (not IsValid(GameUnitManager) || not IsValid(ParentActor))
	{
		RTSFunctionLibrary::ReportError("Could not obtain valid"
			"GameUnitManager in URTSComponent::AddUnitToGameState");
		return false;
	}
	return AddOwnerToUnitManager(*GameUnitManager, *ParentActor, OwningPlayer);
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

void URTSComponent::UpdateTacticalAIForOwner()
{
	const bool bOwnerNeedsTacticalAI = not FRTSPlayers::IsHumanPlayer(OwningPlayer);
	if (bOwnerNeedsTacticalAI == GetHasTacticalAI())
	{
		return;
	}
	if (bOwnerNeedsTacticalAI)
	{
		DetermineCreateTacticalAI();
		return;
	}
	M_EnemyTatcicalAIComp->DestroyComponent();
	M_EnemyTatcicalAIComp = nullptr;
}

bool URTSComponent::GetHasTacticalAI() const
{
	return M_EnemyTatcicalAIComp.IsValid();
}

void URTSComponent::DetermineCreateTacticalAI()
{
	if (FRTSPlayers::IsHumanPlayer(OwningPlayer))
	{
		return;
	}
	if(UnitType == EAllUnitType::UNType_Tank)
	{
		CreateTankTacticalAIAtOwner();
		return;
	}
	// Make sure to only create at the squad controller and not the squad units.
	const ASquadController* OwnerSquad = Cast<ASquadController>(GetOwner());
	if(UnitType == EAllUnitType::UNType_Squad && IsValid(OwnerSquad))
	{
		CreateSquadTacticalAIAtOwner();
	}
}

void URTSComponent::CreateTankTacticalAIAtOwner()
{
	UEnemyTankTacticalAI* TankTacticalAI = NewObject<UEnemyTankTacticalAI>(GetOwner(), UEnemyTankTacticalAI::StaticClass());
	if (IsValid(TankTacticalAI))
	{
		TankTacticalAI->RegisterComponent();
		M_EnemyTatcicalAIComp = TankTacticalAI;
	}
	else
	{
		RTSFunctionLibrary::ReportError("Failed to create Tank Tactical AI Component at owner.");
	}
}

void URTSComponent::CreateSquadTacticalAIAtOwner()
{
	UEnemySquadTacticalAI* SquadTacticalAI = NewObject<UEnemySquadTacticalAI>(GetOwner(), UEnemySquadTacticalAI::StaticClass());
	if (IsValid(SquadTacticalAI))
	{
		SquadTacticalAI->RegisterComponent();
		M_EnemyTatcicalAIComp = SquadTacticalAI;
	}
	else
	{
		RTSFunctionLibrary::ReportError("Failed to create Squad Tactical AI Component at owner.");
	}
}


bool URTSComponent::HasTechnologyApplied(const ETechnology Technology) const
{
	return M_TechAppliedToUnit.Contains(Technology);
}

void URTSComponent::AddAppliedTechnology(const ETechnology Technology)
{
	M_TechAppliedToUnit.AddUnique(Technology);
}

void URTSComponent::OnTankInitializedLookingForUpgrades(ATankMaster* Tank)
{
	// Expansion bonuses come from the owner's standing expansions, for every owner (not only researched player techs).
	if (UWorld* World = GetWorld())
	{
		if (UPanzerwerferReloadBonusSubsystem* ReloadBonusSubsystem =
			World->GetSubsystem<UPanzerwerferReloadBonusSubsystem>())
		{
			ReloadBonusSubsystem->OnTankReady(Tank);
		}
	}
	if (OwningPlayer != UPlayerTechManager::PlayerTechOwnerIndex || not IsValid(Tank))
	{
		return;
	}

	if (UPlayerTechManager* PlayerTechManager = FRTS_Statics::GetPlayerTechManager(this))
	{
		PlayerTechManager->CheckTechsToApplyToTank(Tank);
	}
}

void URTSComponent::OnNomadicInitializedLookingForUpgrades(ANomadicVehicle* Nomadic)
{
	if (OwningPlayer != UPlayerTechManager::PlayerTechOwnerIndex || not IsValid(Nomadic))
	{
		return;
	}

	if (UPlayerTechManager* PlayerTechManager = FRTS_Statics::GetPlayerTechManager(this))
	{
		PlayerTechManager->CheckTechsToApplyToNomadic(Nomadic);
	}
}

void URTSComponent::OnSquadInitializedLookingForUpgrades(ASquadController* Squad)
{
	if (OwningPlayer != UPlayerTechManager::PlayerTechOwnerIndex || not IsValid(Squad))
	{
		return;
	}

	if (UPlayerTechManager* PlayerTechManager = FRTS_Statics::GetPlayerTechManager(this))
	{
		PlayerTechManager->CheckTechsToApplyToSquad(Squad);
	}
}

void URTSComponent::OnBuildingExpansionInitializedLookingForUpgrades(ABuildingExpansion* BuildingExpansion)
{
	if (OwningPlayer != UPlayerTechManager::PlayerTechOwnerIndex || not IsValid(BuildingExpansion))
	{
		return;
	}

	if (UPlayerTechManager* PlayerTechManager = FRTS_Statics::GetPlayerTechManager(this))
	{
		PlayerTechManager->CheckTechsToApplyToBuildingExpansion(BuildingExpansion);
	}
}

void URTSComponent::OnAircraftInitializedLookingForUpgrades(AAircraftMaster* Aircraft)
{
	if (OwningPlayer != UPlayerTechManager::PlayerTechOwnerIndex || not IsValid(Aircraft))
	{
		return;
	}

	if (UPlayerTechManager* PlayerTechManager = FRTS_Statics::GetPlayerTechManager(this))
	{
		PlayerTechManager->CheckTechsToApplyToAircraft(Aircraft);
	}
}
