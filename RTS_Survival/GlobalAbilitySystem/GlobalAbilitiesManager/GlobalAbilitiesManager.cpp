// Copyright (C) Bas Blokzijl - All rights reserved.


#include "GlobalAbilitiesManager.h"

#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/UI/W_GA_Description/W_GA_Description.h"
#include "RTS_Survival/GlobalAbilitySystem/UI/W_GA_Item/W_GA_Item.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/GlobalAbilitySystem/UI/GlobalAbilityPanel/W_GlobalAbilityPanel.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


// Sets default values for this component's properties
UGlobalAbilitiesManager::UGlobalAbilitiesManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UGlobalAbilitiesManager::InitGlobalAbilitiesManager(const int32 OwningPlayer,
                                                         const TArray<EGlobalAbility>& GlobalAbilities,
                                                         UW_GlobalAbilityPanel* W_GlobalAbilityPanel,
                                                         TObjectPtr<UPlayerResourceManager> PlayerResourceMgr,
                                                         TObjectPtr<UGameUnitManager> GameUnitMgr,
                                                         TObjectPtr<UPlayerTechManager> PlayerTechMgr,
                                                         const FVector& PlayerStartLocation,
                                                         const float AircraftHeight)
{
	M_OwningPlayer = OwningPlayer;
	// Player and enemy ability managers both need unit requirement checks.
	M_GameUnitManager = GameUnitMgr;
	if (IsPlayerAbilityManager())
	{
		M_GlobalAbilityPanel = W_GlobalAbilityPanel;
		if (EnsureIsValidGlobalAbilityPanel())
		{
			M_GA_Description = M_GlobalAbilityPanel.Get()->GetDescription();
			if (EnsureIsValidGlobalAbilityDescription())
			{
				M_GA_Description.Get()->HideDescription();
			}
		}

		M_PlayerResourceManager = PlayerResourceMgr;
		M_PlayerTechManager = PlayerTechMgr;
		(void)EnsureIsValidPlayerResourceManager();
		(void)EnsureIsValidPlayerTechManager();
		(void)EnsureIsValidGameUnitManager();
	}
	LoadGlobalAbilities(GlobalAbilities);
	SetupSpawnSettings(PlayerStartLocation, AircraftHeight);
}

void UGlobalAbilitiesManager::SetupSpawnSettings(const FVector& PlayerStart,
                                                 const float AircraftHeightStart)
{
	M_SpawnSetings.PlayerStartLocation = PlayerStart;
	M_SpawnSetings.AircraftStartLocation = PlayerStart;
	M_SpawnSetings.AircratHeightStart = AircraftHeightStart;

	AFowManager* const FowManager = FRTS_Statics::GetFowManager(this);
	if (not IsValid(FowManager))
	{
		return;
	}

	M_SpawnSetings.MapExtent = FowManager->GetMapExtent();
	M_SpawnSetings.Fow_ManagerPosition = FowManager->GetActorLocation();

	const FVector DirectionToPlayerStart =
		(PlayerStart - M_SpawnSetings.Fow_ManagerPosition).GetSafeNormal();

	M_SpawnSetings.AircraftStartLocation =
		M_SpawnSetings.Fow_ManagerPosition + DirectionToPlayerStart * M_SpawnSetings.MapExtent;
}

bool UGlobalAbilitiesManager::TryGenerateLocationOnAircraftEdgeLine(
	const FVector& AircraftEdgeCenterLocation,
	const FVector& FowManagerPosition,
	const FVector& PlayerStart,
	const float SpawnLineHalfLength,
	const float AircraftHeightStart,
	FVector& OutSpawnLocation) const
{
	FVector DirectionFromFowToPlayer = PlayerStart - FowManagerPosition;
	DirectionFromFowToPlayer.Z = 0.0f;

	if (DirectionFromFowToPlayer.IsNearlyZero())
	{
		return false;
	}

	const FVector DirectionFromFowToPlayerNormalized = DirectionFromFowToPlayer.GetSafeNormal();

	const FVector PerpendicularDirection = FVector(
		-DirectionFromFowToPlayerNormalized.Y,
		DirectionFromFowToPlayerNormalized.X,
		0.0f
	);

	const float RandomDistanceAlongEdge = FMath::FRandRange(
		-SpawnLineHalfLength,
		SpawnLineHalfLength
	);

	OutSpawnLocation =
		AircraftEdgeCenterLocation + PerpendicularDirection * RandomDistanceAlongEdge;

	OutSpawnLocation.Z = AircraftHeightStart;

	return true;
}


void UGlobalAbilitiesManager::BeginPlay()
{
	Super::BeginPlay();
}

void UGlobalAbilitiesManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearPlayerRequirementTimer();
	ClearCooldownTimer();
	ReleaseLoadedAbilities();
	Super::EndPlay(EndPlayReason);
}

void UGlobalAbilitiesManager::SetMaxAllowedAbilityTier(const EGlobalAbilityTier MaxAllowedTier)
{
	M_MaxAllowedAbilityTier = MaxAllowedTier;
}

bool UGlobalAbilitiesManager::QueryRequirementForAbility(const UGlobalAbility* Ability) const
{
	if (not IsValid(Ability))
	{
		return false;
	}
#if RTS_WITH_ALL_GLOBAL_ABILITIES
	// Ability test target: no tier cap, technology or required unit blocks the player.
	if (IsPlayerAbilityManager())
	{
		return true;
	}
#endif

	return QueryTierAllowedForAbility(Ability) && QueryTechnologyRequirementForAbility(Ability)
		&& QueryUnitRequirementForAbility(Ability);
}

bool UGlobalAbilitiesManager::QueryTierAllowedForAbility(const UGlobalAbility* Ability) const
{
	if (not IsValid(Ability))
	{
		return false;
	}
	if (IsEnemyAbilityManager())
	{
		return true;
	}

	return static_cast<uint8>(Ability->GetAbilityTier()) <= static_cast<uint8>(M_MaxAllowedAbilityTier);
}

bool UGlobalAbilitiesManager::QueryUnitRequirementForAbility(const UGlobalAbility* Ability) const
{
	if (not IsValid(Ability))
	{
		return false;
	}

	const FTrainingOption& RequiredUnit = Ability->M_AbilityRequirements.RequiredUnit;
	if (RequiredUnit.IsNone())
	{
		return true;
	}

	if (IsEnemyAbilityManager())
	{
		return QueryEnemyUnitRequirementForAbility(RequiredUnit);
	}

	return QueryPlayerUnitRequirementForAbility(RequiredUnit);
}

bool UGlobalAbilitiesManager::QueryPlayerUnitRequirementForAbility(const FTrainingOption& RequiredUnit) const
{
	if (not EnsureIsValidGameUnitManager())
	{
		return false;
	}

	return CheckDoesPlayerHaveUnit(RequiredUnit);
}

bool UGlobalAbilitiesManager::QueryEnemyUnitRequirementForAbility(const FTrainingOption& RequiredUnit) const
{
	if (not EnsureIsValidGameUnitManager())
	{
		return false;
	}

	return CheckDoesEnemyHaveUnit(RequiredUnit);
}

bool UGlobalAbilitiesManager::QueryTechnologyRequirementForAbility(const UGlobalAbility* Ability) const
{
	if (not IsValid(Ability))
	{
		return false;
	}

	if (IsEnemyAbilityManager())
	{
		return true;
	}

	if (Ability->M_AbilityRequirements.RequiredTechnology == ETechnology::Tech_NONE)
	{
		return true;
	}

	if (not EnsureIsValidPlayerTechManager())
	{
		return false;
	}

	return M_PlayerTechManager.Get()->HasTechResearched(Ability->M_AbilityRequirements.RequiredTechnology);
}

bool UGlobalAbilitiesManager::QueryCostsForAbility(const UGlobalAbility* Ability) const
{
	if (not IsValid(Ability))
	{
		return false;
	}

	if (IsEnemyAbilityManager())
	{
		return true;
	}
#if RTS_WITH_ALL_GLOBAL_ABILITIES
	// Ability test target: the player's abilities are free (TryPayForAbility takes nothing).
	return true;
#else
	if (not EnsureIsValidPlayerResourceManager())
	{
		return false;
	}

	const EPlayerError Error = M_PlayerResourceManager.Get()->GetCanPayForCost(Ability->M_AbilityCosts.Costs.ResourceCosts);
	return Error == EPlayerError::Error_None;
#endif
}

bool UGlobalAbilitiesManager::QueryCooldownForAbility(const UGlobalAbility* Ability) const
{
	return IsValid(Ability) && Ability->M_AbilityCosts.CoolDownRemaining <= 0;
}

void UGlobalAbilitiesManager::OnHoveredAbilityButton(UGlobalAbility* HoveredAbility, const bool bIsHover)
{
	if (not IsPlayerAbilityManager() || not EnsureIsValidGlobalAbilityDescription())
	{
		return;
	}
	if (not bIsHover || GetHasActiveAbility() || not IsValid(HoveredAbility))
	{
		M_GA_Description.Get()->HideDescription();
		return;
	}
	M_GA_Description.Get()->ShowDescription(
		HoveredAbility->M_UISettings,
		HoveredAbility->M_AbilityCosts,
		GetHoverDescriptionForAbility(HoveredAbility));
}

void UGlobalAbilitiesManager::OnClickedAbilityButton(UGlobalAbility* ClickedAbility)
{
	if (not IsValid(ClickedAbility) || GetHasActiveAbility())
	{
		return;
	}
	ClickedAbility->OnClickedAbilityButton();
	if (EnsureIsValidGlobalAbilityDescription())
	{
		M_GA_Description.Get()->HideDescription();
	}
}


FVector UGlobalAbilitiesManager::GetAircraftBombingSpawnLocation(const UObject* Requester,
                                                                 const FVector& TargetLocation) const
{
	(void)Requester;
	(void)TargetLocation;

	FVector SpawnLocation;
	if (TryGenerateLocationOnAircraftEdgeLine(
		M_SpawnSetings.AircraftStartLocation,
		M_SpawnSetings.Fow_ManagerPosition,
		M_SpawnSetings.PlayerStartLocation,
		M_SpawnSetings.MapExtent / 2,
		M_SpawnSetings.AircratHeightStart,
		SpawnLocation))
	{
		return SpawnLocation;
	}

	return M_SpawnSetings.AircraftStartLocation;
}

FVector UGlobalAbilitiesManager::GetAircraftBombingRetreatLocation(const UObject* Requester,
                                                                   const FVector& TargetLocation) const
{
	FVector SpawnLocation; 
	if (TryGenerateLocationOnAircraftEdgeLine(
		M_SpawnSetings.AircraftStartLocation,
		M_SpawnSetings.Fow_ManagerPosition,
		M_SpawnSetings.PlayerStartLocation,
		M_SpawnSetings.MapExtent / 2,
		M_SpawnSetings.AircratHeightStart, 
		SpawnLocation))
	{
		return SpawnLocation;
	}
	return M_SpawnSetings.AircraftStartLocation;
}

FVector UGlobalAbilitiesManager::GetStraightFlightSpawnLocation(const FVector& LineStart,
                                                                const FVector& FlightDirection,
                                                                const float MaxApproachDistance) const
{
	constexpr float MarginOutsideMap = 2000.0f;
	float DistanceToEdge = MaxApproachDistance;
	if (M_SpawnSetings.MapExtent > 0.0f)
	{
		const FVector FromMapCentre = LineStart - M_SpawnSetings.Fow_ManagerPosition;
		const auto GetDistanceOnAxis = [this](const float Position, const float BackwardDirection)
		{
			if (FMath::IsNearlyZero(BackwardDirection))
			{
				return TNumericLimits<float>::Max();
			}
			const float Edge = BackwardDirection > 0.0f ? M_SpawnSetings.MapExtent : -M_SpawnSetings.MapExtent;
			return FMath::Max(0.0f, (Edge - Position) / BackwardDirection);
		};
		DistanceToEdge = FMath::Min(
			GetDistanceOnAxis(FromMapCentre.X, -FlightDirection.X),
			GetDistanceOnAxis(FromMapCentre.Y, -FlightDirection.Y)) + MarginOutsideMap;
	}

	FVector SpawnLocation = LineStart - FlightDirection * FMath::Min(DistanceToEdge, MaxApproachDistance);
	SpawnLocation.Z = M_SpawnSetings.AircratHeightStart;
	return SpawnLocation;
}

bool UGlobalAbilitiesManager::IsPlayerAbilityManager() const
{
	return M_OwningPlayer == 1;
}

bool UGlobalAbilitiesManager::IsEnemyAbilityManager() const
{
	return not IsPlayerAbilityManager();
}

bool UGlobalAbilitiesManager::EnsureIsValidGlobalAbilityPanel() const
{
	if (M_GlobalAbilityPanel.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_GlobalAbilityPanel"),
		TEXT("UGlobalAbilitiesManager::EnsureIsValidGlobalAbilityPanel"),
		this
	);
	return false;
}

bool UGlobalAbilitiesManager::EnsureIsValidGlobalAbilityDescription() const
{
	if (M_GA_Description.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_GA_Description"),
		TEXT("UGlobalAbilitiesManager::EnsureIsValidGlobalAbilityDescription"),
		this
	);
	return false;
}

bool UGlobalAbilitiesManager::EnsureIsValidPlayerTechManager() const
{
	if (M_PlayerTechManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PlayerTechManager"),
		TEXT("UGlobalAbilitiesManager::EnsureIsValidPlayerTechManager"),
		this
	);
	return false;
}

bool UGlobalAbilitiesManager::EnsureIsValidPlayerResourceManager() const
{
	if (M_PlayerResourceManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_PlayerResourceManager"),
		TEXT("UGlobalAbilitiesManager::EnsureIsValidPlayerResourceManager"),
		this
	);
	return false;
}

bool UGlobalAbilitiesManager::EnsureIsValidGameUnitManager() const
{
	if (M_GameUnitManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_GameUnitManager"),
		TEXT("UGlobalAbilitiesManager::EnsureIsValidGameUnitManager"),
		this
	);
	return false;
}

void UGlobalAbilitiesManager::LoadGlobalAbilities(TArray<EGlobalAbility> AbilityEnums)
{
	const URTSGlobalAbilitySettings* GlobalAbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	if (not IsValid(GlobalAbilitySettings))
	{
		return;
	}
	UGlobalAbilitySetDataAsset* AbilitySetDataAsset = GlobalAbilitySettings->GlobalAbilityDataAsset.LoadSynchronous();
	if (not IsValid(AbilitySetDataAsset))
	{
		RTSFunctionLibrary::ReportError(TEXT("Global ability data asset is not valid in global ability settings."));
		return;
	}

	TArray<TObjectPtr<UGlobalAbility>> AbilitiesLoadedCopyUObjects;
	for (const EGlobalAbility AbilityEnum : AbilityEnums)
	{
		UGlobalAbility* AbilityCopy = AbilitySetDataAsset->GetGLobalAbilityForTypeAsCopyFromTemplate(AbilityEnum);
		if (not IsValid(AbilityCopy))
		{
			continue;
		}
		// The enum key the template is registered under is authoritative for the loaded ability's type.
		// Stamping it here keeps FindLoadedAbilityByType and per-ability loadout overrides (e.g. enemy
		// cooldown overrides) correct even when a template's own M_AbilityType was authored inconsistently.
		AbilityCopy->M_AbilityType = AbilityEnum;
		AbilityCopy->InitGlobalAbility(M_OwningPlayer, this, Cast<ACPPController>(GetOwner()));
		AbilitiesLoadedCopyUObjects.Add(AbilityCopy);
	}
	OnLoadedAbilities(AbilitiesLoadedCopyUObjects);
}

void UGlobalAbilitiesManager::OnLoadedAbilities(TArray<TObjectPtr<UGlobalAbility>> AbilitiesLoadedCopyUObjects)
{
	M_GlobalAbilities = AbilitiesLoadedCopyUObjects;
	M_BaseCooldownTimes.Reset();
	if (M_AppliedCooldownMlt < 1.f)
	{
		// A cooldown bonus registered before the abilities finished loading.
		for (UGlobalAbility* Ability : M_GlobalAbilities)
		{
			if (IsValid(Ability))
			{
				ApplyCooldownMltToAbility(Ability, M_AppliedCooldownMlt);
			}
		}
	}
	StartCooldownTimer();
#if RTS_WITH_ALL_GLOBAL_ABILITIES
	// Ability test target: a template that starts on cooldown does not block the player either.
	if (IsPlayerAbilityManager())
	{
		for (UGlobalAbility* Ability : M_GlobalAbilities)
		{
			if (IsValid(Ability))
			{
				Ability->M_AbilityCosts.CoolDownRemaining = 0;
			}
		}
	}
#endif
	if (IsPlayerAbilityManager())
	{
		InitAbilityPanel(AbilitiesLoadedCopyUObjects);
		CheckRequirements();
		StartPlayerRequirementTimer();
	}
}

void UGlobalAbilitiesManager::InitAbilityPanel(TArray<TObjectPtr<UGlobalAbility>> LoadedAbilities)
{
	if (not IsPlayerAbilityManager() || not EnsureIsValidGlobalAbilityPanel())
	{
		return;
	}
#if RTS_WITH_ALL_GLOBAL_ABILITIES
	// More abilities than panel slots: show the first page; RTS.GlobalAbilities.Page shows the others.
	if (LoadedAbilities.Num() > RTS::AllGlobalAbilities::PanelPageSize)
	{
		ShowAbilityPanelPage(0);
		return;
	}
#endif
	M_GlobalAbilityPanel.Get()->InitWithLoadedAbilities(LoadedAbilities, this);
}

#if RTS_WITH_ALL_GLOBAL_ABILITIES
namespace
{
	// One player ability manager per world; the shown page of its panel.
	int32 GAllAbilitiesPanelPage = 0;
}

void UGlobalAbilitiesManager::ShowAbilityPanelPage(const int32 PageIndex)
{
	if (not IsPlayerAbilityManager() || not EnsureIsValidGlobalAbilityPanel())
	{
		return;
	}
	const int32 PageCount = GetAbilityPanelPageCount();
	GAllAbilitiesPanelPage = ((PageIndex % PageCount) + PageCount) % PageCount;
	TArray<TObjectPtr<UGlobalAbility>> PageAbilities;
	const int32 FirstIndex = GAllAbilitiesPanelPage * RTS::AllGlobalAbilities::PanelPageSize;
	for (int32 Index = FirstIndex;
	     Index < FirstIndex + RTS::AllGlobalAbilities::PanelPageSize && M_GlobalAbilities.IsValidIndex(Index); ++Index)
	{
		PageAbilities.Add(M_GlobalAbilities[Index]);
	}
	M_GlobalAbilityPanel.Get()->InitWithLoadedAbilities(PageAbilities, this);
	CheckRequirements();
	UE_LOG(LogTemp, Display, TEXT("RTS_ALL_GLOBAL_ABILITIES panel page %d/%d (abilities %d..%d of %d)"),
	       GAllAbilitiesPanelPage + 1, PageCount, FirstIndex + 1, FirstIndex + PageAbilities.Num(),
	       M_GlobalAbilities.Num());
}

int32 UGlobalAbilitiesManager::GetAbilityPanelPage() const
{
	return GAllAbilitiesPanelPage;
}

int32 UGlobalAbilitiesManager::GetAbilityPanelPageCount() const
{
	return FMath::Max(1, FMath::DivideAndRoundUp(M_GlobalAbilities.Num(), RTS::AllGlobalAbilities::PanelPageSize));
}
#endif

void UGlobalAbilitiesManager::StartPlayerRequirementTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	constexpr float RequirementCheckInterval = 1.f;
	World->GetTimerManager().SetTimer(
		M_CheckRequirementsTimerHandle,
		this,
		&UGlobalAbilitiesManager::CheckRequirements,
		RequirementCheckInterval,
		true);
}

void UGlobalAbilitiesManager::ClearPlayerRequirementTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		M_CheckRequirementsTimerHandle.Invalidate();
		return;
	}

	World->GetTimerManager().ClearTimer(M_CheckRequirementsTimerHandle);
	M_CheckRequirementsTimerHandle.Invalidate();
}

void UGlobalAbilitiesManager::StartCooldownTimer()
{
	if (not IsEnemyAbilityManager())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	constexpr float CooldownTickInterval = 1.f;
	World->GetTimerManager().SetTimer(
		M_CooldownTimerHandle,
		this,
		&UGlobalAbilitiesManager::TickGlobalAbilityCooldowns,
		CooldownTickInterval,
		true);
}

void UGlobalAbilitiesManager::ClearCooldownTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		M_CooldownTimerHandle.Invalidate();
		return;
	}

	World->GetTimerManager().ClearTimer(M_CooldownTimerHandle);
	M_CooldownTimerHandle.Invalidate();
}

void UGlobalAbilitiesManager::ReleaseLoadedAbilities()
{
	for (UGlobalAbility* Ability : M_GlobalAbilities)
	{
		if (IsValid(Ability))
		{
			Ability->CancelAbilityActivation();
		}
	}

	M_GlobalAbilities.Reset();
	M_BaseCooldownTimes.Reset();
	M_GlobalAbilityPanel.Reset();
	M_GA_Description.Reset();
	M_PlayerTechManager.Reset();
	M_PlayerResourceManager.Reset();
	M_GameUnitManager.Reset();
}

void UGlobalAbilitiesManager::CheckRequirements()
{
	for (UGlobalAbility* Ability : M_GlobalAbilities)
	{
		UpdateAbilityAvailability(Ability);
	}
}

void UGlobalAbilitiesManager::UpdateAbilityAvailability(UGlobalAbility* Ability)
{
	if (not IsValid(Ability))
	{
		return;
	}
	if (Ability->M_AbilityCosts.CoolDownRemaining > 0)
	{
		Ability->M_AbilityCosts.CoolDownRemaining = FMath::Max(
			0,
			Ability->M_AbilityCosts.CoolDownRemaining - 1);
	}
	const bool bMeetsRequirements = QueryRequirementForAbility(Ability);
	const bool bCanPayCost = QueryCostsForAbility(Ability);
	const bool bIsOnCooldown = not QueryCooldownForAbility(Ability);
	UpdateAbilityItemAvailability(
		Ability,
		bMeetsRequirements && bCanPayCost && not bIsOnCooldown,
		not bMeetsRequirements);
}

void UGlobalAbilitiesManager::UpdateAbilityItemAvailability(const UGlobalAbility* Ability, const bool bIsEnabled,
                                                            const bool bUseGreyTint) const
{
	if (not EnsureIsValidGlobalAbilityPanel())
	{
		return;
	}
	for (UW_GA_Item* AbilityItem : M_GlobalAbilityPanel.Get()->GetAbilityItems())
	{
		if (IsValid(AbilityItem) && AbilityItem->GetLoadedAbility() == Ability)
		{
			AbilityItem->RefreshCooldownFromLoadedAbility();
			AbilityItem->SetAbilityAvailable(bIsEnabled, bUseGreyTint);
			return;
		}
	}
}

FText UGlobalAbilitiesManager::GetHoverDescriptionForAbility(const UGlobalAbility* Ability) const
{
	if (not IsValid(Ability))
	{
		return FText::GetEmpty();
	}
#if RTS_WITH_ALL_GLOBAL_ABILITIES
	// Ability test target: nothing blocks the player, so the tooltip shows the ability itself.
	if (IsPlayerAbilityManager())
	{
		return Ability->M_UISettings.Description;
	}
#endif
	if (not QueryTierAllowedForAbility(Ability))
	{
		return FText::FromString(FRTSRichTextConverter::MakeRTSRich(
			FString::Printf(TEXT("Not available in this operation: %s ability (allowed up to %s)"),
			                *Global_GetGlobalAbilityTierDisplayName(Ability->GetAbilityTier()),
			                *Global_GetGlobalAbilityTierDisplayName(M_MaxAllowedAbilityTier)),
			ERTSRichText::Text_Bad14));
	}
	if (Ability->M_AbilityRequirements.RequiredTechnology != ETechnology::Tech_NONE
		&& not EnsureIsValidPlayerTechManager())
	{
		return FText::GetEmpty();
	}

	if (Ability->M_AbilityRequirements.RequiredTechnology != ETechnology::Tech_NONE
		&& not M_PlayerTechManager.Get()->HasTechResearched(Ability->M_AbilityRequirements.RequiredTechnology))
	{
		return FText::FromString(FRTSRichTextConverter::MakeRTSRich(
			FString(TEXT("Required Tech: ")) + Global_GetTechDisplayName(
				Ability->M_AbilityRequirements.RequiredTechnology),
			ERTSRichText::Text_Bad14));
	}
	if (not Ability->M_AbilityRequirements.RequiredUnit.IsNone()
		&& not CheckDoesPlayerHaveUnit(Ability->M_AbilityRequirements.RequiredUnit))
	{
		return FText::FromString(FRTSRichTextConverter::MakeRTSRich(
			FString(TEXT("Required Unit: ")) + GetUnitRequirementDisplayName(
				Ability->M_AbilityRequirements.RequiredUnit),
			ERTSRichText::Text_Bad14));
	}
	if (Ability->M_AbilityCosts.CoolDownRemaining > 0)
	{
		return FText::FromString(FRTSRichTextConverter::MakeRTSRich(
			FString::Printf(TEXT("On Cooldown: %d"), Ability->M_AbilityCosts.CoolDownRemaining),
			ERTSRichText::Text_Bad14));
	}
	return Ability->M_UISettings.Description;
}

FString UGlobalAbilitiesManager::GetUnitRequirementDisplayName(const FTrainingOption& UnitId) const
{
	switch (UnitId.UnitType)
	{
	case EAllUnitType::UNType_Squad:
		return Global_GetSquadDisplayName(static_cast<ESquadSubtype>(UnitId.SubtypeValue));
	case EAllUnitType::UNType_Tank:
		return Global_GetTankDisplayName(static_cast<ETankSubtype>(UnitId.SubtypeValue));
	case EAllUnitType::UNType_Nomadic:
		return Global_GetNomadicDisplayName(static_cast<ENomadicSubtype>(UnitId.SubtypeValue));
	case EAllUnitType::UNType_BuildingExpansion:
		return Global_GetBxpDisplayString(static_cast<EBuildingExpansionType>(UnitId.SubtypeValue));
	case EAllUnitType::UNType_Aircraft:
		return Global_GetAircraftDisplayName(static_cast<EAircraftSubtype>(UnitId.SubtypeValue));
	case EAllUnitType::UNType_StandaloneTurret:
		return TEXT("Standalone Turret");
	case EAllUnitType::UNType_Harvester:
		return TEXT("Harvester");
	default:
		return TEXT("Unit");
	}
}

void UGlobalAbilitiesManager::OnAbilityFinishedExecuting(UGlobalAbility* Ability)
{
	if (not IsValid(Ability))
	{
		return;
	}
	Ability->M_AbilityCosts.CoolDownRemaining = Ability->M_AbilityCosts.CoolDownTime;
#if RTS_WITH_ALL_GLOBAL_ABILITIES
	// Ability test target: no cooldown blocks the player; the button stays available.
	if (IsPlayerAbilityManager())
	{
		Ability->M_AbilityCosts.CoolDownRemaining = 0;
		UpdateAbilityItemAvailability(Ability, true, false);
		return;
	}
#endif
	if (IsPlayerAbilityManager())
	{
		UpdateAbilityItemAvailability(Ability, false, false);
	}
}

void UGlobalAbilitiesManager::RegisterCooldownMlt(const AActor* Source, const float CooldownMlt)
{
	if (not IsValid(Source))
	{
		RTSFunctionLibrary::ReportError("Tried to register a global ability cooldown bonus without a source."
			"\n At function UGlobalAbilitiesManager::RegisterCooldownMlt");
		return;
	}
	M_CooldownSources.RemoveAll([](const FGlobalAbilityCooldownSource& EachSource)
	{
		return not EachSource.Source.IsValid();
	});
	// A zero multiplier would make every ability free to spam.
	constexpr float MinCooldownMlt = 0.05f;
	const float ClampedCooldownMlt = FMath::Clamp(CooldownMlt, MinCooldownMlt, 1.f);
	bool bIsAlreadyRegistered = false;
	for (FGlobalAbilityCooldownSource& EachSource : M_CooldownSources)
	{
		if (EachSource.Source.Get() == Source)
		{
			EachSource.CooldownMlt = ClampedCooldownMlt;
			bIsAlreadyRegistered = true;
		}
	}
	if (not bIsAlreadyRegistered)
	{
		FGlobalAbilityCooldownSource NewSource;
		NewSource.Source = Source;
		NewSource.CooldownMlt = ClampedCooldownMlt;
		M_CooldownSources.Add(NewSource);
	}
	OnCooldownMltChanged();
}

void UGlobalAbilitiesManager::UnregisterCooldownMlt(const AActor* Source)
{
	// A source that is not registered (never built, or already taken back) is not an error.
	M_CooldownSources.RemoveAll([Source](const FGlobalAbilityCooldownSource& EachSource)
	{
		return EachSource.Source.Get() == Source || not EachSource.Source.IsValid();
	});
	OnCooldownMltChanged();
}

float UGlobalAbilitiesManager::GetCooldownMlt() const
{
	float LowestCooldownMlt = 1.f;
	for (const FGlobalAbilityCooldownSource& EachSource : M_CooldownSources)
	{
		if (EachSource.Source.IsValid())
		{
			LowestCooldownMlt = FMath::Min(LowestCooldownMlt, EachSource.CooldownMlt);
		}
	}
	return LowestCooldownMlt;
}

void UGlobalAbilitiesManager::OnCooldownMltChanged()
{
	const float NewCooldownMlt = GetCooldownMlt();
	if (FMath::IsNearlyEqual(NewCooldownMlt, M_AppliedCooldownMlt) || M_AppliedCooldownMlt <= 0.f)
	{
		return;
	}
	// A cooldown that is running keeps its progress: what is left shrinks (or grows) with the new multiplier.
	const float Ratio = NewCooldownMlt / M_AppliedCooldownMlt;
	for (UGlobalAbility* Ability : M_GlobalAbilities)
	{
		if (not IsValid(Ability))
		{
			continue;
		}
		ApplyCooldownMltToAbility(Ability, NewCooldownMlt);
		if (Ability->M_AbilityCosts.CoolDownRemaining > 0)
		{
			Ability->M_AbilityCosts.CoolDownRemaining = FMath::Max(
				1, FMath::CeilToInt32(Ability->M_AbilityCosts.CoolDownRemaining * Ratio));
		}
	}
	M_AppliedCooldownMlt = NewCooldownMlt;
}

void UGlobalAbilitiesManager::ApplyCooldownMltToAbility(UGlobalAbility* Ability, const float CooldownMlt)
{
	// The first time an ability is scaled its current cooldown is its base (no bonus was active before).
	const int32 BaseCooldownTime = M_BaseCooldownTimes.FindOrAdd(Ability, Ability->M_AbilityCosts.CoolDownTime);
	if (BaseCooldownTime <= 0)
	{
		return;
	}
	Ability->M_AbilityCosts.CoolDownTime = FMath::Max(1, FMath::CeilToInt32(BaseCooldownTime * CooldownMlt));
}

bool UGlobalAbilitiesManager::TryPayForAbility(UGlobalAbility* Ability) const
{
	if (not IsValid(Ability) || not EnsureIsValidPlayerResourceManager())
	{
		return false;
	}
#if RTS_WITH_ALL_GLOBAL_ABILITIES
	// Ability test target: the player's abilities cost nothing.
	return true;
#else
	return M_PlayerResourceManager.Get()->PayForCosts(Ability->M_AbilityCosts.Costs.ResourceCosts);
#endif
}

bool UGlobalAbilitiesManager::GetHasActiveAbility() const
{
	for (const UGlobalAbility* Ability : M_GlobalAbilities)
	{
		if (IsValid(Ability) && Ability->M_AbilityState == EGlobalAbilityState::Activated)
		{
			return true;
		}
	}
	return false;
}

bool UGlobalAbilitiesManager::CheckDoesPlayerHaveUnit(const FTrainingOption& UnitId) const
{
	// We assume a valid game unit manager here.
	switch (UnitId.UnitType)
	{
	case EAllUnitType::UNType_None:
	case EAllUnitType::UNType_Harvester:
		return true;
	case EAllUnitType::UNType_Squad:
		return CheckDoesPlayerHaveSquad(UnitId);
	case EAllUnitType::UNType_Tank:
		return CheckDoesPlayerHaveTank(UnitId);
	case EAllUnitType::UNType_Nomadic:
		return CheckDoesPlayerHaveNomadic(UnitId);
	case EAllUnitType::UNType_BuildingExpansion:
		return CheckDoesPlayerHaveBuildingExpansion(UnitId);
	case EAllUnitType::UNType_Aircraft:
		return CheckDoesPlayerHaveAircraft(UnitId);
	case EAllUnitType::UNType_StandaloneTurret:
		return IsValid(M_GameUnitManager.Get()->FindUnitForPlayer(UnitId, UPlayerTechManager::PlayerTechOwnerIndex));
	}
	return true;
}

bool UGlobalAbilitiesManager::CheckDoesPlayerHaveSquad(const FTrainingOption& UnitId) const
{
	const ESquadSubtype SquadSubtype = static_cast<ESquadSubtype>(UnitId.SubtypeValue);
	return M_GameUnitManager.Get()->GetHasPlayerSquadOfType(SquadSubtype);
}

bool UGlobalAbilitiesManager::CheckDoesPlayerHaveTank(const FTrainingOption& UnitId) const
{
	const ETankSubtype TankSubtype = static_cast<ETankSubtype>(UnitId.SubtypeValue);
	return M_GameUnitManager.Get()->GetHasPlayerTankOfType(TankSubtype);
}

bool UGlobalAbilitiesManager::CheckDoesPlayerHaveNomadic(const FTrainingOption& UnitId) const
{
	const ENomadicSubtype NomadicSubtype = static_cast<ENomadicSubtype>(UnitId.SubtypeValue);
	return M_GameUnitManager.Get()->GetHasPlayerNomadicOfType(NomadicSubtype);
}

bool UGlobalAbilitiesManager::CheckDoesPlayerHaveBuildingExpansion(const FTrainingOption& UnitId) const
{
	const EBuildingExpansionType BuildingExpansionSubtype = static_cast<EBuildingExpansionType>(UnitId.SubtypeValue);
	return M_GameUnitManager.Get()->GetHasPlayerBxpOfType(BuildingExpansionSubtype);
}

bool UGlobalAbilitiesManager::CheckDoesPlayerHaveAircraft(const FTrainingOption& UnitId) const
{
	const EAircraftSubtype AircraftSubtype = static_cast<EAircraftSubtype>(UnitId.SubtypeValue);
	return M_GameUnitManager.Get()->GetHasPlayerAircraftOfType(AircraftSubtype);
}

UGlobalAbility* UGlobalAbilitiesManager::FindLoadedAbilityByType(const EGlobalAbility AbilityType) const
{
	for (UGlobalAbility* Ability : M_GlobalAbilities)
	{
		if (IsValid(Ability) && Ability->M_AbilityType == AbilityType)
		{
			return Ability;
		}
	}

	return nullptr;
}

void UGlobalAbilitiesManager::TickGlobalAbilityCooldowns()
{
	for (UGlobalAbility* Ability : M_GlobalAbilities)
	{
		if (IsValid(Ability) && Ability->M_AbilityCosts.CoolDownRemaining > 0)
		{
			Ability->M_AbilityCosts.CoolDownRemaining = FMath::Max(0, Ability->M_AbilityCosts.CoolDownRemaining - 1);
		}
	}
}

void UGlobalAbilitiesManager::ApplyCooldownOverride(
	UGlobalAbility* Ability,
	const FEnemyGlobalAbilityLoadoutEntry& LoadoutEntry)
{
	if (not IsValid(Ability))
	{
		return;
	}

	if (LoadoutEntry.CooldownTimeOverride > 0)
	{
		Ability->M_AbilityCosts.CoolDownTime = LoadoutEntry.CooldownTimeOverride;
	}

	if (LoadoutEntry.bStartOnCooldown)
	{
		Ability->M_AbilityCosts.CoolDownRemaining = Ability->M_AbilityCosts.CoolDownTime;
	}
}

TArray<UGlobalAbility*> UGlobalAbilitiesManager::GetAvailableEnemyGlobalAbilities() const
{
	TArray<UGlobalAbility*> AvailableAbilities;
	if (not IsEnemyAbilityManager())
	{
		return AvailableAbilities;
	}

	for (UGlobalAbility* Ability : M_GlobalAbilities)
	{
		if (QueryRequirementForAbility(Ability) && QueryCooldownForAbility(Ability))
		{
			AvailableAbilities.Add(Ability);
		}
	}

	return AvailableAbilities;
}

bool UGlobalAbilitiesManager::TryExecuteAbilityAtLocation(UGlobalAbility* Ability, const FVector& TargetLocation)
{
	if (not IsValid(Ability) || not QueryRequirementForAbility(Ability) || not QueryCooldownForAbility(Ability))
	{
		return false;
	}

	if (IsPlayerAbilityManager() && not TryPayForAbility(Ability))
	{
		return false;
	}

	Ability->ExecuteAbilityAtLocation(TargetLocation);
	OnAbilityFinishedExecuting(Ability);
	return true;
}

bool UGlobalAbilitiesManager::CheckDoesEnemyHaveUnit(const FTrainingOption& UnitId) const
{
	switch (UnitId.UnitType)
	{
	case EAllUnitType::UNType_None:
	case EAllUnitType::UNType_Harvester:
		return true;
	case EAllUnitType::UNType_Squad:
		return CheckDoesEnemyHaveSquad(UnitId);
	case EAllUnitType::UNType_Tank:
		return CheckDoesEnemyHaveTank(UnitId);
	case EAllUnitType::UNType_Nomadic:
		return CheckDoesEnemyHaveNomadic(UnitId);
	case EAllUnitType::UNType_BuildingExpansion:
		return CheckDoesEnemyHaveBuildingExpansion(UnitId);
	case EAllUnitType::UNType_Aircraft:
		return CheckDoesEnemyHaveAircraft(UnitId);
	case EAllUnitType::UNType_StandaloneTurret:
		{
			constexpr int32 EnemyPlayerIndex = 2;
			return IsValid(M_GameUnitManager.Get()->FindUnitForPlayer(UnitId, EnemyPlayerIndex));
		}
	}
	return true;
}

bool UGlobalAbilitiesManager::CheckDoesEnemyHaveSquad(const FTrainingOption& UnitId) const
{
	const ESquadSubtype SquadSubtype = static_cast<ESquadSubtype>(UnitId.SubtypeValue);
	return M_GameUnitManager.Get()->GetHasEnemySquadOfType(SquadSubtype);
}

bool UGlobalAbilitiesManager::CheckDoesEnemyHaveTank(const FTrainingOption& UnitId) const
{
	const ETankSubtype TankSubtype = static_cast<ETankSubtype>(UnitId.SubtypeValue);
	return M_GameUnitManager.Get()->GetHasEnemyTankOfType(TankSubtype);
}

bool UGlobalAbilitiesManager::CheckDoesEnemyHaveNomadic(const FTrainingOption& UnitId) const
{
	const ENomadicSubtype NomadicSubtype = static_cast<ENomadicSubtype>(UnitId.SubtypeValue);
	return M_GameUnitManager.Get()->GetHasEnemyNomadicOfType(NomadicSubtype);
}

bool UGlobalAbilitiesManager::CheckDoesEnemyHaveBuildingExpansion(const FTrainingOption& UnitId) const
{
	const EBuildingExpansionType BuildingExpansionSubtype = static_cast<EBuildingExpansionType>(UnitId.SubtypeValue);
	return M_GameUnitManager.Get()->GetHasEnemyBxpOfType(BuildingExpansionSubtype);
}

bool UGlobalAbilitiesManager::CheckDoesEnemyHaveAircraft(const FTrainingOption& UnitId) const
{
	const EAircraftSubtype AircraftSubtype = static_cast<EAircraftSubtype>(UnitId.SubtypeValue);
	return M_GameUnitManager.Get()->GetHasEnemyAircraftOfType(AircraftSubtype);
}
