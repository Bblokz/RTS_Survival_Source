// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SquadReinforcementComponent.h"

#include "ReinforcementPoint.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/Pooling/WorldSubSystem/RTSTimedProgressBarWorldSubsystem.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/UnitData/SquadData.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingMenuManager.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"

namespace
{
	constexpr float ReinforcementProgressOffsetZ = 500.0f;
	constexpr float ReinforcementProgressScale = 1.0f;
	constexpr int32 ReinforcementCostRoundingIncrement = 5;
}

USquadReinforcementComponent::USquadReinforcementComponent(): bM_IsActivated(false), M_MaxSquadUnits(0),
                                                              bM_HasMissingSquadMembers(false),
                                                              bM_ReinforcementAbilityAdded(false)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void USquadReinforcementComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_InitSquadDataSnapshot();
}

void USquadReinforcementComponent::ActivateReinforcements(const bool bActivate)
{
	bM_IsActivated = bActivate;
	if (not bM_IsActivated)
	{
		RemoveReinforcementAbility();
		return;
	}

	UpdateMissingSquadMemberState();
	RefreshReinforcementAbility();
}

void USquadReinforcementComponent::NotifySquadMembershipChanged()
{
	UpdateMissingSquadMemberState();
	RefreshReinforcementAbility();
}

void USquadReinforcementComponent::Reinforce(UReinforcementPoint* ReinforcementPoint)
{
	TArray<TSubclassOf<ASquadUnit>> MissingUnitClasses;
	FVector ReinforcementLocation = FVector::ZeroVector;
	if (not CanProcessReinforcement(ReinforcementPoint, MissingUnitClasses, ReinforcementLocation))
	{
		return;
	}

	const int32 MissingUnitCount = MissingUnitClasses.Num();
	TMap<ERTSResourceType, int32> ReinforcementCost;
	if (not TryResolveReinforcementCost(MissingUnitCount, ReinforcementCost))
	{
		return;
	}

	if (not TryPayReinforcementCost(ReinforcementCost))
	{
		return;
	}

	const float ReinforcementTime = CalculateReinforcementTime(MissingUnitCount);
	ScheduleReinforcement(ReinforcementTime, ReinforcementLocation, MissingUnitClasses);
}

void USquadReinforcementComponent::BeginPlay_InitSquadDataSnapshot()
{
	M_SquadController = Cast<ASquadController>(GetOwner());
	if (not GetIsValidSquadController())
	{
		return;
	}

	for (const TSoftClassPtr<ASquadUnit>& SoftClass : M_SquadController->SquadUnitClasses)
	{
		if (SoftClass.IsNull())
		{
			continue;
		}
		M_InitialSquadUnitClasses.Add(SoftClass.LoadSynchronous());
	}
	M_MaxSquadUnits = M_InitialSquadUnitClasses.Num();
	UpdateMissingSquadMemberState();
}

bool USquadReinforcementComponent::GetIsValidSquadController()
{
	if (M_SquadController.IsValid())
	{
		return true;
	}
	const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : FString("InvalidOwner");
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_SquadController",
	                                                      "GetIsValidSquadController" + OwnerName);
	return false;
}

bool USquadReinforcementComponent::GetIsValidRTSComponent()
{
	if (not GetIsValidSquadController())
	{
		return false;
	}
	if (IsValid(M_SquadController->RTSComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportNullErrorComponent(this, "RTSComponent",
	                                             "USquadReinforcementComponent::GetIsValidRTSComponent");
	return false;
}

bool USquadReinforcementComponent::EnsureAbilityArraySized()
{
	if (not GetIsValidSquadController())
	{
		return false;
	}

	TArray<EAbilityID> Abilities = M_SquadController->GetUnitAbilities();
	const int32 DesiredNum = ReinforcementAbilityIndex + 1;
	if (Abilities.Num() >= DesiredNum)
	{
		return true;
	}

	const int32 MaxAbilities = DeveloperSettings::GamePlay::ActionUI::MaxAbilitiesForActionUI;
	if (DesiredNum > MaxAbilities)
	{
		RTSFunctionLibrary::ReportError("Reinforcement ability index exceeds max abilities."
			"\nUSquadReinforcementComponent::EnsureAbilityArraySized");
		return false;
	}

	const int32 OriginalNum = Abilities.Num();
	const int32 NewSize = FMath::Min(FMath::Max(DesiredNum, OriginalNum), MaxAbilities);
	Abilities.SetNum(NewSize);
	for (int32 Index = OriginalNum; Index < Abilities.Num(); Index++)
	{
		Abilities[Index] = EAbilityID::IdNoAbility;
	}

	M_SquadController->SetUnitAbilitiesRunTime(Abilities);
	return true;
}

void USquadReinforcementComponent::AddReinforcementAbility()
{
	if (bM_ReinforcementAbilityAdded)
	{
		return;
	}
	if (not EnsureAbilityArraySized())
	{
		return;
	}

	const TArray<EAbilityID> CurrentAbilities = M_SquadController->GetUnitAbilities();
	if (CurrentAbilities.IsValidIndex(ReinforcementAbilityIndex) &&
		CurrentAbilities[ReinforcementAbilityIndex] == EAbilityID::IdReinforceSquad)
	{
		bM_ReinforcementAbilityAdded = true;
		return;
	}
	if (CurrentAbilities.IsValidIndex(ReinforcementAbilityIndex) &&
		CurrentAbilities[ReinforcementAbilityIndex] != EAbilityID::IdNoAbility)
	{
		RTSFunctionLibrary::ReportError("Reinforcement ability slot already occupied."
			"\nUSquadReinforcementComponent::AddReinforcementAbility");
		return;
	}
	if (not M_SquadController->AddAbility(EAbilityID::IdReinforceSquad, ReinforcementAbilityIndex))
	{
		return;
	}
	bM_ReinforcementAbilityAdded = true;
}

void USquadReinforcementComponent::RemoveReinforcementAbility()
{
	if (not GetIsValidSquadController())
	{
		return;
	}
	const TArray<EAbilityID> CurrentAbilities = M_SquadController->GetUnitAbilities();
	if (CurrentAbilities.IsValidIndex(ReinforcementAbilityIndex) &&
		CurrentAbilities[ReinforcementAbilityIndex] != EAbilityID::IdReinforceSquad)
	{
		bM_ReinforcementAbilityAdded = false;
		return;
	}
	if (not M_SquadController->RemoveAbility(EAbilityID::IdReinforceSquad))
	{
		return;
	}
	bM_ReinforcementAbilityAdded = false;
}

void USquadReinforcementComponent::RefreshReinforcementAbility()
{
	if (not bM_IsActivated)
	{
		RemoveReinforcementAbility();
		return;
	}

	if (not bM_HasMissingSquadMembers)
	{
		RemoveReinforcementAbility();
		return;
	}

	AddReinforcementAbility();
}

void USquadReinforcementComponent::UpdateMissingSquadMemberState()
{
	bM_HasMissingSquadMembers = DoesSquadNeedReinforcement();
}

bool USquadReinforcementComponent::DoesSquadNeedReinforcement() const
{
	if (not M_SquadController.IsValid())
	{
		return false;
	}
	if (M_MaxSquadUnits <= 0)
	{
		return false;
	}

	return M_SquadController->GetSquadUnitsCount() < M_MaxSquadUnits;
}

bool USquadReinforcementComponent::CanProcessReinforcement(UReinforcementPoint* ReinforcementPoint,
                                                           TArray<TSubclassOf<ASquadUnit>>& OutMissingUnitClasses,
                                                           FVector& OutReinforcementLocation)
{
	if (not bM_IsActivated)
	{
		return false;
	}
	if (M_ReinforcementRequestState.bM_IsInProgress)
	{
		return false;
	}
	if (not GetIsReinforcementAllowed(ReinforcementPoint))
	{
		return false;
	}
	if (not GetMissingUnitClasses(OutMissingUnitClasses))
	{
		return false;
	}

	bool bHasValidLocation = false;
	OutReinforcementLocation = ReinforcementPoint->GetReinforcementLocation(bHasValidLocation);
	return bHasValidLocation;
}

bool USquadReinforcementComponent::GetIsReinforcementAllowed(UReinforcementPoint* ReinforcementPoint)
{
	if (not GetIsValidSquadController())
	{
		return false;
	}
	if (not IsValid(ReinforcementPoint))
	{
		RTSFunctionLibrary::ReportError("Reinforcement point is null in Reinforce call."
			"\nUSquadReinforcementComponent::GetIsReinforcementAllowed");
		return false;
	}
	if (M_MaxSquadUnits <= 0)
	{
		RTSFunctionLibrary::ReportError("Max squad units not initialised for reinforcement."
			"\nUSquadReinforcementComponent::GetIsReinforcementAllowed");
		return false;
	}
	if (M_SquadController->GetSquadUnitsCount() >= M_MaxSquadUnits)
	{
		return false;
	}
	return true;
}

bool USquadReinforcementComponent::TryResolveReinforcementCost(const int32 MissingUnits,
                                                               TMap<ERTSResourceType, int32>& OutCost)
{
	if (MissingUnits <= 0)
	{
		return false;
	}
	if (not GetIsValidRTSComponent())
	{
		return false;
	}
	OutCost.Reset();
	const int32 ClampedMaxSquadUnits = FMath::Max(M_MaxSquadUnits, 1);
	const float MissingRatio = static_cast<float>(MissingUnits) / static_cast<float>(ClampedMaxSquadUnits);
	bool bIsValidData = false;
	const FSquadData SquadData = FRTS_Statics::BP_GetSquadDataOfPlayer(
		M_SquadController->RTSComponent->GetOwningPlayer(),
		M_SquadController->RTSComponent->GetSubtypeAsSquadSubtype(),
		this,
		bIsValidData);
	if (not bIsValidData)
	{
		RTSFunctionLibrary::ReportError("Unable to retrieve squad data for reinforcement cost."
			"\nUSquadReinforcementComponent::TryResolveReinforcementCost");
		return false;
	}

	for (const auto& CostPair : SquadData.Cost.ResourceCosts)
	{
		const float ScaledCost = static_cast<float>(CostPair.Value) * MissingRatio;
		const int32 RoundedCost = FMath::CeilToInt(ScaledCost / ReinforcementCostRoundingIncrement)
			* ReinforcementCostRoundingIncrement;
		if (RoundedCost > 0)
		{
			OutCost.Add(CostPair.Key, RoundedCost);
		}
	}
	return OutCost.Num() > 0;
}

bool USquadReinforcementComponent::TryPayReinforcementCost(const TMap<ERTSResourceType, int32>& ReinforcementCost) const
{
	UPlayerResourceManager* ResourceManager = nullptr;
	if (not TryGetResourceManager(ResourceManager))
	{
		return false;
	}

	const EPlayerError PaymentError = ResourceManager->GetCanPayForCost(ReinforcementCost);
	if (PaymentError != EPlayerError::Error_None)
	{
		if (ACPPController* PlayerController = FRTS_Statics::GetRTSController(this))
		{
			PlayerController->DisplayErrorMessage(PaymentError);
		}
		return false;
	}

	const bool bPaid = ResourceManager->PayForCosts(ReinforcementCost);
	if (bPaid)
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Failed to pay reinforcement costs despite validation."
		"\nUSquadReinforcementComponent::TryPayReinforcementCost");
	return false;
}

bool USquadReinforcementComponent::TryGetResourceManager(UPlayerResourceManager*& OutManager) const
{
	OutManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (IsValid(OutManager))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("PlayerResourceManager not valid for reinforcement payment."
		"\nUSquadReinforcementComponent::TryGetResourceManager");
	return false;
}

bool USquadReinforcementComponent::TryGetTrainingMenuManager(UTrainingMenuManager*& OutMenuManager) const
{
	OutMenuManager = FRTS_Statics::GetTrainingMenuManager(this);
	if (IsValid(OutMenuManager))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("TrainingMenuManager not valid while calculating reinforcement time."
		"\nUSquadReinforcementComponent::TryGetTrainingMenuManager");
	return false;
}

float USquadReinforcementComponent::CalculateReinforcementTime(const int32 MissingUnits) const
{
	UTrainingMenuManager* TrainingMenuManager = nullptr;
	if (not TryGetTrainingMenuManager(TrainingMenuManager))
	{
		return 1.0f;
	}
	if (not M_SquadController.IsValid() || not IsValid(M_SquadController->RTSComponent))
	{
		return 1.0f;
	}

	const FTrainingOption TrainingOption(EAllUnitType::UNType_Squad,
	                                     static_cast<uint8>(M_SquadController->RTSComponent->
	                                                                           GetSubtypeAsSquadSubtype()));
	const FTrainingOptionState OptionState = TrainingMenuManager->GetTrainingOptionState(TrainingOption);
	if (OptionState.TrainingTime <= 0)
	{
		return 1.0f;
	}

	const int32 ClampedMaxUnits = FMath::Max(M_MaxSquadUnits, 1);
	const float MissingRatio = static_cast<float>(MissingUnits) / static_cast<float>(ClampedMaxUnits);
	const float ReinforcementTime = OptionState.TrainingTime * MissingRatio;
	return FMath::Max(ReinforcementTime, 0.1f);
}

bool USquadReinforcementComponent::GetMissingUnitClasses(TArray<TSubclassOf<ASquadUnit>>& OutMissingUnitClasses) const
{
	if (not M_SquadController.IsValid())
	{
		return false;
	}

	TMap<TSubclassOf<ASquadUnit>, int32> ExpectedCounts;
	for (const TSubclassOf<ASquadUnit> UnitClass : M_InitialSquadUnitClasses)
	{
		if (not UnitClass)
		{
			continue;
		}
		ExpectedCounts.FindOrAdd(UnitClass)++;
	}

	TMap<TSubclassOf<ASquadUnit>, int32> AliveCounts;
	const TArray<ASquadUnit*> CurrentUnits = M_SquadController->GetSquadUnitsChecked();
	for (ASquadUnit* SquadUnit : CurrentUnits)
	{
		if (not IsValid(SquadUnit))
		{
			continue;
		}
		AliveCounts.FindOrAdd(SquadUnit->GetClass())++;
	}

	for (const auto& Expected : ExpectedCounts)
	{
		const int32 AliveCount = AliveCounts.FindRef(Expected.Key);
		const int32 MissingCount = Expected.Value - AliveCount;
		for (int32 Index = 0; Index < MissingCount; Index++)
		{
			OutMissingUnitClasses.Add(Expected.Key);
		}
	}
	return OutMissingUnitClasses.Num() > 0;
}

void USquadReinforcementComponent::ScheduleReinforcement(const float ReinforcementTime, const FVector& SpawnLocation,
                                                         const TArray<TSubclassOf<ASquadUnit>>& MissingUnitClasses)
{
	M_ReinforcementRequestState.bM_IsInProgress = true;
	M_ReinforcementRequestState.M_CachedLocation = SpawnLocation;
	M_ReinforcementRequestState.M_PendingClasses = MissingUnitClasses;

	if (UWorld* World = GetWorld())
	{
		FTimerDelegate ReinforcementDelegate;
		ReinforcementDelegate.BindUObject(this, &USquadReinforcementComponent::SpawnMissingUnits);
		World->GetTimerManager().SetTimer(M_ReinforcementRequestState.M_TimerHandle,
		                                  ReinforcementDelegate, ReinforcementTime, false);

		URTSTimedProgressBarWorldSubsystem* ProgressSubsystem =
			World->GetSubsystem<URTSTimedProgressBarWorldSubsystem>();
		if (IsValid(ProgressSubsystem) && GetIsValidSquadController())
		{
			ProgressSubsystem->ActivateTimedProgressBarAnchored(
				M_SquadController->GetRootComponent(),
				FVector(0.0f, 0.0f, ReinforcementProgressOffsetZ),
				0.0f,
				ReinforcementTime,
				true,
				ERTSProgressBarType::Default,
				false,
				TEXT(""),
				ReinforcementProgressScale);
		}
		return;
	}

	M_ReinforcementRequestState.bM_IsInProgress = false;
	M_ReinforcementRequestState.M_PendingClasses.Reset();
}

void USquadReinforcementComponent::SpawnMissingUnits()
{
	M_ReinforcementRequestState.bM_IsInProgress = false;
	if (not GetIsValidSquadController())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError(TEXT("World is invalid while spawning reinforcements.")
			TEXT("\nUSquadReinforcementComponent::SpawnMissingUnits"));
		return;
	}

	World->GetTimerManager().ClearTimer(M_ReinforcementRequestState.M_TimerHandle);

	M_SquadController->BeginReinforcementSpawnGrid(M_SquadController->GetActorLocation());

	TArray<ASquadUnit*> SpawnedUnits;
	for (const TSubclassOf<ASquadUnit> UnitClass : M_ReinforcementRequestState.M_PendingClasses)
	{
		if (not UnitClass)
		{
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ASquadUnit* SpawnedUnit = World->SpawnActor<ASquadUnit>(UnitClass, M_ReinforcementRequestState.M_CachedLocation,
		                                                        FRotator::ZeroRotator, SpawnParameters);
		if (not IsValid(SpawnedUnit))
		{
			RTSFunctionLibrary::ReportError("Failed to spawn squad unit during reinforcement."
				"\nUSquadReinforcementComponent::SpawnMissingUnits");
			continue;
		}

		SpawnedUnit->SetSquadController(M_SquadController.Get());
		SpawnedUnit->OnSquadSpawned(false, 0.0f, M_ReinforcementRequestState.M_CachedLocation);
		SpawnedUnits.Add(SpawnedUnit);
		M_SquadController->RegisterReinforcedUnit(SpawnedUnit);
	}

	MoveSpawnedUnitsToController(SpawnedUnits);
	M_ReinforcementRequestState.M_PendingClasses.Reset();
}

void USquadReinforcementComponent::MoveSpawnedUnitsToController(const TArray<ASquadUnit*>& SpawnedUnits) const
{
	if (not M_SquadController.IsValid())
	{
		return;
	}

	for (ASquadUnit* SpawnedUnit : SpawnedUnits)
	{
		if (not IsValid(SpawnedUnit))
		{
			continue;
		}
		const FVector TargetLocation = M_SquadController->GetNextReinforcementSpawnLocation();
		SpawnedUnit->ExecuteMoveToSelfPathFinding(TargetLocation, EAbilityID::IdMove);
	}
}
