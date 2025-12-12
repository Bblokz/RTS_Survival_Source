// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "PlayerProfileLoader.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/CardSystem/PlayerProfile/PlayerProfileLoader/FRTS_Profile.h"
#include "RTS_Survival/CardSystem/PlayerProfile/SavePlayerProfile/USavePlayerProfile.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptionLibrary/TrainingOptionLibrary.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/UnitData/NomadicVehicleData.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


// Sets default values for this component's properties
UPlayerProfileLoader::UPlayerProfileLoader()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerProfileLoader::LoadProfile(TObjectPtr<ARTSAsyncSpawner> AsyncSpawner, const FVector& SpawnCenter,
                                       ACPPController* PlayerController, const bool bDoNotLoadPlayerUnits)
{
	if (not IsValid(PlayerController))
	{
		RTSFunctionLibrary::ReportError("Could not load profile as the player controller is not valid");
		return;
	}
	M_PlayerController = MakeWeakObjectPtr(PlayerController);
	if (not IsValid(AsyncSpawner))
	{
		RTSFunctionLibrary::ReportError("Could not load profile as the async spawner is not valid");
		return;
	}
	USavePlayerProfile* PlayerProfile = FRTS_Profile::LoadPlayerProfile();
	if (not IsValid(PlayerProfile))
	{
		RTSFunctionLibrary::ReportError("Could not load profile as the player profile is not valid");
		return;
	}
	FRTS_Profile::DebugPrintProfile(PlayerProfile);

	UpdateNomadicUnit(ExtractTrainingOptionsOfType(PlayerProfile->M_SelectedCards, ECardType::BarracksTrain),
	                  ENomadicSubtype::Nomadic_GerBarracks);
	UpdateNomadicUnit(ExtractTrainingOptionsOfType(PlayerProfile->M_SelectedCards, ECardType::ForgeTrain),
	                  ENomadicSubtype::Nomadic_GerLightSteelForge);
	UpdateNomadicUnit(ExtractTrainingOptionsOfType(PlayerProfile->M_SelectedCards, ECardType::MechanicalDepotTrain),
	                  ENomadicSubtype::Nomadic_GerMechanizedDepot);
	ApplyResourceCards(ExtractResources(PlayerProfile->M_SelectedCards));
	ApplyTechnologies(ExtractTechnologies(PlayerProfile->M_SelectedCards));
	if(bDoNotLoadPlayerUnits)
	{
		return;
	}
	LoadInUnits(AsyncSpawner, ExtractUnitsToCreate(PlayerProfile->M_SelectedCards), SpawnCenter);
}


void UPlayerProfileLoader::BeginPlay()
{
	Super::BeginPlay();
}

void UPlayerProfileLoader::LoadInUnits(TObjectPtr<ARTSAsyncSpawner> AsyncSpawner,
                                       const TArray<FTrainingOption>& InUnits, const FVector& SpawnCenter)
{
	if (!IsValid(AsyncSpawner))
	{
		RTSFunctionLibrary::ReportError("Cannot load in units as the player camera pawn or async spawner is not valid");
		return;
	}
	// Sanity check.
	M_IsProfileUnitSpawned.Empty();
	SetupWaitForUnitsToSpawn(InUnits);

	constexpr float GridEdgeLength = DeveloperSettings::GamePlay::Formation::SpawnFormationEdgeLenght;

	// Spawn Hq first.
	RequestToSpawnHQ(AsyncSpawner, SpawnCenter);

	// Prepare the units to spawn 
	TArray<FTrainingOption> UnitsToSpawn = InUnits;

	const int32 NumUnits = UnitsToSpawn.Num();
	if (NumUnits == 0)
	{
		// No units to spawn besides the HQ
		return;
	}

	// Calculate the grid size (number of units per side)
	const int32 GridSize = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(NumUnits)));

	// Handle edge cases where GridSize is less than 1
	if (GridSize < 1)
	{
		RTSFunctionLibrary::ReportError("Invalid grid size calculated for unit placement");
		return;
	}

	// Calculate the spacing between units
	float GridSpacing = 0.0f;
	if (GridSize > 1)
	{
		GridSpacing = GridEdgeLength / (GridSize - 1);
	}

	// Determine the starting positions for X and Y
	const float StartX = SpawnCenter.X - (GridEdgeLength / 2.0f);
	const float StartY = SpawnCenter.Y - (GridEdgeLength / 2.0f);
	const float SpawnZ = SpawnCenter.Z;

	int32 UnitIndex = 0;
	for (int32 Row = 0; Row < GridSize && UnitIndex < NumUnits; ++Row)
	{
		for (int32 Col = 0; Col < GridSize && UnitIndex < NumUnits; ++Col)
		{
			// Calculate the spawn location for the unit
			FVector UnitLocation;
			UnitLocation.X = StartX + Col * GridSpacing;
			UnitLocation.Y = StartY + Row * GridSpacing;
			UnitLocation.Z = SpawnZ;

			// Skip the center position where the HQ is placed
			if (UnitLocation.Equals(SpawnCenter, KINDA_SMALL_NUMBER))
			{
				continue; // Adjust if necessary to avoid overlapping with the HQ
			}

			const FTrainingOption UnitOption = UnitsToSpawn[UnitIndex];

			// Spawn the unit asynchronously at the calculated location
			if (!AsyncSpawner->AsyncSpawnOptionAtLocation(
				UnitOption,
				UnitLocation,
				this,
				0,
				[this](const FTrainingOption& Option, AActor* SpawnedActor,  const int32 ID)
				{
					this->OnOptionSpawned(Option);
				}))
			{
				const FString ErrorMessage = FString::Printf(
					TEXT("Failed to spawn unit '%s' at location (%f, %f, %f)"),
					*UnitOption.GetTrainingName(),
					UnitLocation.X, UnitLocation.Y, UnitLocation.Z);
				RTSFunctionLibrary::ReportError(ErrorMessage);
			}

			++UnitIndex;
		}
	}
}

void UPlayerProfileLoader::UpdateNomadicUnit(const TArray<FTrainingOption>& NomadicTrainingOptions,
                                             const ENomadicSubtype NomadicType) const
{
	if (NomadicTrainingOptions.Num() == 0)
	{
		RTSFunctionLibrary::PrintToLog(
			"No " + UTrainingOptionLibrary::GetEnumValueName(EAllUnitType::UNType_Nomadic,
			                                                 static_cast<uint8>(NomadicType)) +
			" availabe, will not affect training cards");
		return;
	}
	ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	if (not IsValid(GameState))
	{
		RTSFunctionLibrary::ReportError(
			"Could not update " + UTrainingOptionLibrary::GetEnumValueName(
				EAllUnitType::UNType_Nomadic, static_cast<uint8>(NomadicType)) +
			" as the game state is not valid");
		return;
	}
	FNomadicData NomadicData = GameState->GetNomadicDataOfPlayer(1, NomadicType);
	NomadicData.TrainingOptions.Empty();
	NomadicData.TrainingOptions = NomadicTrainingOptions;
	GameState->UpgradeNomadicDataForPlayer(1, NomadicType, NomadicData);
}

void UPlayerProfileLoader::ApplyTechnologies(const TArray<ETechnology>& Technologies) const
{
	UPlayerTechManager* TechManager = FRTS_Statics::GetPlayerTechManager(this);
	if (TechManager)
	{
		for (auto EachTech : Technologies)
		{
			TechManager->OnTechResearched(EachTech);
		}
	}
}

void UPlayerProfileLoader::ApplyResourceCards(const TArray<FUnitCost>& ResourceBonuses) const
{
	UPlayerResourceManager* ResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (ResourceManager)
	{
		ResourceManager->ApplyResourceCards(ResourceBonuses);
	}
}


TArray<FTrainingOption> UPlayerProfileLoader::ExtractUnitsToCreate(const TArray<FCardSaveData>& SelectedCards)
{
	TArray<FTrainingOption> Units;
	for (auto EachData : SelectedCards)
	{
		if (EachData.CardType == ECardType::StartingUnit)
		{
			Units.Add(EachData.UnitToCreate);
		}
	}
	return Units;
}

TArray<FTrainingOption> UPlayerProfileLoader::ExtractTrainingOptionsOfType(
	const TArray<FCardSaveData>& SelectedCards, const ECardType TypeToExtract)
{
	TArray<FTrainingOption> TrainingOptions;
	for (auto EachData : SelectedCards)
	{
		if (EachData.CardType == TypeToExtract)
		{
			TrainingOptions.Add(EachData.TrainingOption);
		}
	}
	RTSFunctionLibrary::PrintToLog(
		"Amount of training options of " + Global_GetCardTypeAsString(TypeToExtract) + " is " + FString::FromInt(
			TrainingOptions.Num()));
	return TrainingOptions;
}

TArray<FUnitCost> UPlayerProfileLoader::ExtractResources(const TArray<FCardSaveData>& SelectedCards)
{
	TArray<FUnitCost> ResourceBonuses;
	for (auto EachData : SelectedCards)
	{
		if (EachData.CardType == ECardType::Resource)
		{
			ResourceBonuses.Add(EachData.Resources);
		}
	}
	return ResourceBonuses;
}

TArray<ETechnology> UPlayerProfileLoader::ExtractTechnologies(const TArray<FCardSaveData>& SelectedCards)
{
	TArray<ETechnology> Technologies;
	for (auto EachData : SelectedCards)
	{
		if (EachData.CardType == ECardType::Technology)
		{
			Technologies.Add(EachData.Technology);
		}
	}
	return Technologies;
}

void UPlayerProfileLoader::OnOptionSpawned(const FTrainingOption& Option)
{
	if (not IsWaitingForUnitToSpawn(Option))
	{
		return;
	}
	M_IsProfileUnitSpawned[Option] = true;
	if (DeveloperSettings::Debugging::GCardSystem_Compile_DebugSymbols)
	{
		DebugOnProfileUnitSpawned(Option);
	}
	// Check if all profile units have been spawned
	for (const auto& EachPair : M_IsProfileUnitSpawned)
	{
		if (not EachPair.Value)
		{
			return;
		}
	}
	if (M_PlayerController.IsValid())
	{
		M_PlayerController->OnPlayerProfileLoadComplete();
		return;
	}
	RTSFunctionLibrary::ReportError("Invalid player controller in UPlayerProfileLoader::OnOptionSpawned"
		"\n Cannot finish PlayerProfileLoad on CPPController!");
}

void UPlayerProfileLoader::SetupWaitForUnitsToSpawn(const TArray<FTrainingOption>& UnitsToSpawn)
{
	for (auto eachOption : UnitsToSpawn)
	{
		M_IsProfileUnitSpawned.Add(eachOption, false);
	}
}

bool UPlayerProfileLoader::IsWaitingForUnitToSpawn(const FTrainingOption& Option)
{
	if (not M_IsProfileUnitSpawned.Contains(Option))
	{
		RTSFunctionLibrary::ReportError("Profile Unit Spawned is not in the list of profile units!!"
			"\n Unit spawned: " + Option.GetTrainingName());
		return false;
	}
	return true;
}

void UPlayerProfileLoader::RequestToSpawnHQ(TObjectPtr<ARTSAsyncSpawner> AsyncSpawner, const FVector& SpawnCenter)
{
	// Spawn the HQ at the camera location
	const FTrainingOption HQOption = FTrainingOption(EAllUnitType::UNType_Nomadic,
	                                                 static_cast<uint8>(ENomadicSubtype::Nomadic_GerHq));
	M_IsProfileUnitSpawned.Add(HQOption, false);

	if (!AsyncSpawner->AsyncSpawnOptionAtLocation(
		HQOption,
		SpawnCenter,
		this,
		0,
		[this](const FTrainingOption& Option, AActor* SpawnedActor, const int32 ID)
		{
			this->OnOptionSpawned(Option);
		}))
	{
		RTSFunctionLibrary::ReportError("Failed to spawn HQ at the camera location");
	}
}

void UPlayerProfileLoader::DebugOnProfileUnitSpawned(const FTrainingOption& Option)
{
	FString UnitName = Option.GetTrainingName();
	RTSFunctionLibrary::PrintString("Unit spawned: " + UnitName +
		"\n still waiting for units:");
	for (auto eachPair : M_IsProfileUnitSpawned)
	{
		if (not eachPair.Value)
		{
			RTSFunctionLibrary::PrintString(eachPair.Key.GetTrainingName());
		}
	}
}
