// Copyright Bas Blokzijl - All rights reserved.

#include "RTSAsyncSpawner.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "RTS_Survival/GameUI/TrainingUI/Interface/Trainer.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptionLibrary/TrainingOptionLibrary.h"
#include "RTS_Survival/Interfaces/RTSInterface/RTSUnit.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

ARTSAsyncSpawner::ARTSAsyncSpawner(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), M_PlayerController(nullptr)
{
	PrimaryActorTick.bCanEverTick = false;
}

bool ARTSAsyncSpawner::CancelActiveLoadRequest(EAsyncRequestType RequestType)
{
	switch (RequestType)
	{
	case EAsyncRequestType::AReq_Bxp:
		{
			if (M_CurrentBxpLoadingHandle.IsValid() && !M_CurrentBxpLoadingHandle->HasLoadCompleted())
			{
				M_CurrentBxpLoadingHandle->CancelHandle();
				M_CurrentBxpLoadingHandle.Reset();
				return true;
			}
		}
		break;
	case EAsyncRequestType::AReq_Training:
		{
			if (M_CurrentTrainingOptionLoadingHandle.IsValid() && !M_CurrentTrainingOptionLoadingHandle->
				HasLoadCompleted())
			{
				M_CurrentTrainingOptionLoadingHandle->CancelHandle();
				M_CurrentTrainingOptionLoadingHandle.Reset();
				return true;
			}
		}
	default:
		break;
	}
	return false;
}

void ARTSAsyncSpawner::BeginPlay()
{
	Super::BeginPlay();
}

void ARTSAsyncSpawner::PostInitializeComponents()
{
	M_TrainingOptionMap.Empty();
	SetupTrainingOptionsFromConfig();

	Super::PostInitializeComponents();
}

void ARTSAsyncSpawner::SetupTrainingOptionsFromConfig()
{
	// Tanks.
	AddTrainingOptionsForTankArray(SetupLightRusVehicles);
	AddTrainingOptionsForTankArray(SetupLightGerVehicles);
	AddTrainingOptionsForTankArray(SetupMediumRusVehicles);
	AddTrainingOptionsForTankArray(SetupMediumGerVehicles);
	AddTrainingOptionsForTankArray(SetupHeavyRusVehicles);
	AddTrainingOptionsForTankArray(SetupHeavyGerVehicles);

	// Squads.
	AddTrainingOptionsForSquadArray(SetupRusSquads);
	AddTrainingOptionsForSquadArray(SetupGerSquads);

	// Nomadics.
	AddTrainingOptionsForNomadicArray(SetupRusNomadics);
	AddTrainingOptionsForNomadicArray(SetupGerNomadics);

	// Aircraft.
	AddTrainingOptionsForAircraftArray(SetupRusAircraft);
	AddTrainingOptionsForAircraftArray(SetupGerAircraft);
}

void ARTSAsyncSpawner::AddTrainingOptionsForTankArray(const TArray<FTankTrainingOptionSetup>& SetupArray)
{
	for (const FTankTrainingOptionSetup& EachEntry : SetupArray)
	{
		if (EachEntry.TankSubtype == ETankSubtype::Tank_None)
		{
			continue;
		}

		const FTrainingOption TrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(EachEntry.TankSubtype));

		AddTrainingOptionMapping(TrainingOption, EachEntry.UnitClass);
	}
}

void ARTSAsyncSpawner::AddTrainingOptionsForSquadArray(const TArray<FSquadTrainingOptionSetup>& SetupArray)
{
	for (const FSquadTrainingOptionSetup& EachEntry : SetupArray)
	{
		if (EachEntry.SquadSubtype == ESquadSubtype::Squad_None)
		{
			continue;
		}

		const FTrainingOption TrainingOption(
			EAllUnitType::UNType_Squad,
			static_cast<uint8>(EachEntry.SquadSubtype));

		AddTrainingOptionMapping(TrainingOption, EachEntry.UnitClass);
	}
}

void ARTSAsyncSpawner::AddTrainingOptionsForNomadicArray(const TArray<FNomadicTrainingOptionSetup>& SetupArray)
{
	for (const FNomadicTrainingOptionSetup& EachEntry : SetupArray)
	{
		if (EachEntry.NomadicSubtype == ENomadicSubtype::Nomadic_None)
		{
			continue;
		}

		const FTrainingOption TrainingOption(
			EAllUnitType::UNType_Nomadic,
			static_cast<uint8>(EachEntry.NomadicSubtype));

		AddTrainingOptionMapping(TrainingOption, EachEntry.UnitClass);
	}
}

void ARTSAsyncSpawner::AddTrainingOptionsForAircraftArray(const TArray<FAircraftTrainingOptionSetup>& SetupArray)
{
	for (const FAircraftTrainingOptionSetup& EachEntry : SetupArray)
	{
		if (EachEntry.AircraftSubtype == EAircraftSubtype::Aircarft_None)
		{
			continue;
		}

		const FTrainingOption TrainingOption(
			EAllUnitType::UNType_Aircraft,
			static_cast<uint8>(EachEntry.AircraftSubtype));

		AddTrainingOptionMapping(TrainingOption, EachEntry.UnitClass);
	}
}

void ARTSAsyncSpawner::AddTrainingOptionMapping(
	const FTrainingOption& TrainingOption,
	const TSoftClassPtr<AActor>& UnitClass)
{
	if (TrainingOption.IsNone())
	{
		RTSFunctionLibrary::ReportError(
			"ARTSAsyncSpawner::AddTrainingOptionMapping - Attempted to add Train_None training option.");
		return;
	}

	if (UnitClass.ToSoftObjectPath().IsNull())
	{
		RTSFunctionLibrary::ReportError(
			"ARTSAsyncSpawner::AddTrainingOptionMapping - UnitClass soft pointer is null for option: "
			+ TrainingOption.GetTrainingName());
		return;
	}

	if (M_TrainingOptionMap.Contains(TrainingOption))
	{
		RTSFunctionLibrary::ReportError(
			"ARTSAsyncSpawner::AddTrainingOptionMapping - Duplicate training option mapping for option: "
			+ TrainingOption.GetTrainingName());
		return;
	}

	M_TrainingOptionMap.Add(TrainingOption, UnitClass);
}

void ARTSAsyncSpawner::InitRTSAsyncSpawner(ACPPController* PlayerController)
{
	M_PlayerController = PlayerController;
	// Error check.
	(void)EnsurePlayerControllerIsValid();
}

UStaticMesh* ARTSAsyncSpawner::SyncGetBuildingExpansionPreviewMesh(EBuildingExpansionType BuildingExpansionType)
{
	if (BxpPreviewMeshMap.Contains(BuildingExpansionType))
	{
		return BxpPreviewMeshMap[BuildingExpansionType];
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"Building expansion type not found in the map! \n At function SyncGetBuildingExpansionPreviewMesh in RTSAsyncSpawner.cpp"
			"number of BuildingExpansionType: " + FString::FromInt(static_cast<int32>(BuildingExpansionType)));
		return nullptr;
	}
}

void ARTSAsyncSpawner::CancelLoadRequestForTrainer(const TWeakObjectPtr<UTrainerComponent> TrainerComponent)
{
	if (TrainerComponent.IsValid())
	{
		FAsyncOptionQueueItem QueueItem;
		bool bTrainerFound = false;
		while (M_TrainingQueue.Dequeue(QueueItem))
		{
			if (QueueItem.Trainer != TrainerComponent)
			{
				M_TrainingQueue.Enqueue(QueueItem);
			}
			else
			{
				bTrainerFound = true;
				break;
			}
		}
		if (!bTrainerFound)
		{
			CancelActiveLoadRequest(EAsyncRequestType::AReq_Training);
			RTSFunctionLibrary::ReportError(
				"we cancel the current load request as the trainer was not found in the queue."
				"\n at function CancelLoadRequestForTrainer in RTSAsyncSpawner.cpp"
				"\n Trainer name: " + TrainerComponent->GetName());
		}
	}
}


bool ARTSAsyncSpawner::EnsurePlayerControllerIsValid() const
{
	if (not IsValid(M_PlayerController))
	{
		RTSFunctionLibrary::ReportError(
			"PlayerController is nullptr in EnsurePlayerControllerIsValid. \n At function EnsurePlayerControllerIsValid in RTSAsyncSpawner.cpp"
			"Class name: ARTSAsyncSpawner. \n failed to set PlayerController reference on init?.");
		return false;
	}
	return true;
}

bool ARTSAsyncSpawner::IsBxpLoadRequestValid(
	const EBuildingExpansionType BuildingExpansionType,
	const TScriptInterface<IBuildingExpansionOwner>& BuildingExpansionOwner) const
{
	if (!BuildingExpansionOwner)
	{
		RTSFunctionLibrary::ReportError("Cannot spawn building async for null owner!"
			"\n at function AsyncSpawnBuildingExpansion in RTSAsyncSpawner.cpp"
			"Spawn request will be ignored.");
		return false;
	}
	if (BuildingExpansionType == EBuildingExpansionType::BXT_Invalid)
	{
		RTSFunctionLibrary::ReportError(
			"Attempt to spawn invalid building expansion type! \n At function AsyncSpawnBuildingExpansion in RTSAsyncSpawner.cpp"
			"\n the function will return without spawning the expansion.");
		return false;
	}
	return true;
}

bool ARTSAsyncSpawner::AsyncSpawnBuildingExpansion(
	const FBxpOptionData& BuildingExpansionTypeData,
	TScriptInterface<IBuildingExpansionOwner> BuildingExpansionOwner,
	const int ExpansionSlotIndex,
	const EAsyncBxpLoadingType BxpLoadingType)
{
	if (!IsBxpLoadRequestValid(BuildingExpansionTypeData.ExpansionType, BuildingExpansionOwner))
	{
		return false;
	}

	if (not BuildingExpansionMap.Contains(BuildingExpansionTypeData.ExpansionType))
	{
		const FString BxpName = Global_GetBxpTypeEnumAsString(BuildingExpansionTypeData.ExpansionType);
		RTSFunctionLibrary::ReportError(
			"Building expansion type not found in the map! \n At function AsyncSpawnBuildingExpansion in RTSAsyncSpawner.cpp"
			"number of BuildingExpansionType: " + BxpName);
		return false;
	}

	const TSoftClassPtr<ABuildingExpansion> AssetClass = BuildingExpansionMap[BuildingExpansionTypeData.ExpansionType];

	// Cancel any previous loading request
	CancelActiveLoadRequest(EAsyncRequestType::AReq_Bxp);

	// If the asset is already loaded, handle it immediately
	if (AssetClass.IsValid())
	{
		OnAsyncBxpLoadComplete(
			AssetClass.ToSoftObjectPath(),
			BuildingExpansionTypeData,
			BuildingExpansionOwner,
			ExpansionSlotIndex,
			BxpLoadingType);
	}
	else
	{
		// Start a new async loading request
		M_CurrentBxpLoadingHandle = M_StreamableManager.RequestAsyncLoad(
			AssetClass.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(
				this,
				&ARTSAsyncSpawner::OnAsyncBxpLoadComplete,
				AssetClass.ToSoftObjectPath(),
				BuildingExpansionTypeData,
				BuildingExpansionOwner,
				ExpansionSlotIndex,
				BxpLoadingType));
	}
	return true;
}


void ARTSAsyncSpawner::OnAsyncBxpLoadComplete(
	FSoftObjectPath AssetPath,
	const FBxpOptionData BuildingExpansionTypeData,
	TScriptInterface<IBuildingExpansionOwner> BuildingExpansionOwner,
	const int ExpansionSlotIndex,
	const EAsyncBxpLoadingType BxpLoadingType)
{
	// Reset the handle since the loading is complete
	if (M_CurrentBxpLoadingHandle.IsValid())
	{
		M_CurrentBxpLoadingHandle.Reset();
	}

	// Resolve the loaded asset, attempts to find the object in memory
	UObject* LoadedAsset = AssetPath.ResolveObject();
	// Cast the loaded asset to a UClass to obtain actor class
	UClass* AssetClass = nullptr;
	if (LoadedAsset)
	{
		AssetClass = Cast<UClass>(LoadedAsset);
	}
	if (not LoadedAsset || not AssetClass)
	{
		OnFailedAssetOrClassLoadBxp(LoadedAsset, AssetClass, BuildingExpansionTypeData.ExpansionType);
		OnBuildingExpansionSpawned(nullptr,
		                           BuildingExpansionOwner,
		                           BuildingExpansionTypeData,
		                           ExpansionSlotIndex,
		                           BxpLoadingType);
		return;
	}
	// Spawn the building expansion actor at the current location of this spawner
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(
		AssetClass, GetActorLocation(), FRotator::ZeroRotator, SpawnParams);

	// If the actor was spawned successfully, notify the player controller
	if (SpawnedActor)
	{
		OnBuildingExpansionSpawned(
			SpawnedActor, BuildingExpansionOwner, BuildingExpansionTypeData, ExpansionSlotIndex,
			BxpLoadingType);
		return;
	}
	RTSFunctionLibrary::ReportError("Async Asset Spawner: building expansion class loading succeeded but"
		"failed to spawn actor for building expansion: " + Global_GetBxpTypeEnumAsString(
			BuildingExpansionTypeData.ExpansionType));
}

void ARTSAsyncSpawner::OnFailedAssetOrClassLoadBxp(
	const UObject* LoadedAsset,
	const UClass* AssetClass,
	const EBuildingExpansionType ExpansionType) const
{
	const FString IsLoadedAssetValid = LoadedAsset ? TEXT("Valid Loaded Asset") : TEXT("Invalid Loaded Asset");
	const FString IsAssetClassCastValid = AssetClass ? "Valid Asset class casting" : "Invalid Asset class casting";
	const FString ExpansionName = Global_GetBxpTypeEnumAsString(ExpansionType);
	RTSFunctionLibrary::ReportError(
		"Async assets spawner failed to spanw building expansion of type " + ExpansionName +
		"With " + IsLoadedAssetValid + " and " + IsAssetClassCastValid);
}


void ARTSAsyncSpawner::OnBuildingExpansionSpawned(
	AActor* SpawnedActor,
	TScriptInterface<IBuildingExpansionOwner> BuildingExpansionOwner,
	const FBxpOptionData& BxpConstructionRulesAndType,
	const int ExpansionSlotIndex,
	const EAsyncBxpLoadingType BxpLoadingType
) const
{
	if (not EnsurePlayerControllerIsValid())
	{
		return;
	}
	if (not SpawnedActor)
	{
		M_PlayerController->OnBxpSpawnedAsync(nullptr,
		                                      BuildingExpansionOwner,
		                                      BxpConstructionRulesAndType,
		                                      ExpansionSlotIndex,
		                                      BxpLoadingType);
		return;
	}
	if (ABuildingExpansion* BuildingExpansion = Cast<ABuildingExpansion>(SpawnedActor))
	{
		M_PlayerController->OnBxpSpawnedAsync(BuildingExpansion,
		                                      BuildingExpansionOwner,
		                                      BxpConstructionRulesAndType,
		                                      ExpansionSlotIndex, BxpLoadingType);
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"Builing expansion cast failed! \n At function OnBuildingExpansionSpawned in RTSAsyncSpawner.cpp"
			"Class name: ARTSAsyncSpawner. \n Check the BuildingExpansion reference."
			"\n name of spawned actor: " + SpawnedActor->GetName());

		M_PlayerController->OnBxpSpawnedAsync(nullptr,
		                                      BuildingExpansionOwner,
		                                      BxpConstructionRulesAndType,
		                                      ExpansionSlotIndex,
		                                      BxpLoadingType);
	}
}

void ARTSAsyncSpawner::OnAsyncSpawnOptionAtLocationComplete(
	const FSoftObjectPath& AssetPath,
	const FTrainingOption TrainingOption,
	const FVector& Location, const int32 ID)
{
	// Resolve the loaded asset
	UObject* LoadedAsset = AssetPath.ResolveObject();
	if (!LoadedAsset)
	{
		RTSFunctionLibrary::ReportError(
			"Failed to resolve loaded asset for TrainingOption: " + TrainingOption.GetTrainingName() +
			"\nAt function OnAsyncSpawnOptionAtLocationComplete in ARTSAsyncSpawner.cpp");
		return;
	}

	// Cast the loaded asset to UClass
	UClass* AssetClass = Cast<UClass>(LoadedAsset);
	if (!AssetClass)
	{
		RTSFunctionLibrary::ReportError(
			"Failed to cast loaded asset to UClass for TrainingOption: " + TrainingOption.GetTrainingName()
			+
			"\nAt function OnAsyncSpawnOptionAtLocationComplete in ARTSAsyncSpawner.cpp");
		return;
	}

	// Spawn the actor at the provided location
	FActorSpawnParameters SpawnParams;
	if (not GetWorld())
	{
		return;
	}
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(
		AssetClass, Location, FRotator::ZeroRotator, SpawnParams);

	if (!IsValid(SpawnedActor))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to spawn actor for TrainingOption: " + TrainingOption.GetTrainingName() +
			"\nAt function OnAsyncSpawnOptionAtLocationComplete in ARTSAsyncSpawner.cpp");
		return;
	}
	if (APawn* SpawnedPawn = Cast<APawn>(SpawnedActor))
	{
		SpawnedPawn->SpawnDefaultController();
	}


	// Perform additional setup on the spawned actor if necessary
	if (IRTSUnit* RTSUnit = Cast<IRTSUnit>(SpawnedActor))
	{
		RTSUnit->OnRTSUnitSpawned(true);
		RTSUnit->OnRTSUnitSpawned(false, 1.f, Location);
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"Spawned actor does not implement IRTSUnit for TrainingOption: " + TrainingOption.GetTrainingName() +
			"\nAt function OnAsyncSpawnOptionAtLocationComplete in ARTSAsyncSpawner.cpp");
	}
	if (M_SpawnCallbacks.Contains(AssetPath))
	{
		const auto& PairData = M_SpawnCallbacks[AssetPath];
		if (PairData.Key.IsValid() && PairData.Value)
		{
			PairData.Value(TrainingOption, SpawnedActor, ID);
		}
		M_SpawnCallbacks.Remove(AssetPath);
	}
}


bool ARTSAsyncSpawner::IsTrainerNotInQueue(const TWeakObjectPtr<UTrainerComponent> TrainerComponent)
{
	TArray<FAsyncOptionQueueItem> TempQueue;
	bool bTrainerFound = false;

	// Dequeue all items into TempQueue and check if the trainer is present
	FAsyncOptionQueueItem QueueItem;
	while (M_TrainingQueue.Dequeue(QueueItem))
	{
		if (QueueItem.Trainer == TrainerComponent)
		{
			bTrainerFound = true;
			RTSFunctionLibrary::ReportError("Trainer is not unique in async queue!"
				"\n at function AsyncSpawnTrainingOption in RTSAsyncSpawner.cpp"
				"\n Spawn request will be ignored."
				"\n Trainer name: " + TrainerComponent->GetName());
		}
		TempQueue.Add(QueueItem);
	}

	for (const FAsyncOptionQueueItem& Item : TempQueue)
	{
		M_TrainingQueue.Enqueue(Item);
	}
	return !bTrainerFound;
}

bool ARTSAsyncSpawner::AsyncSpawnTrainingOption(
	FTrainingOption TrainingOption,
	const TWeakObjectPtr<UTrainerComponent> TrainerComponent)
{
	if (TrainerComponent.IsValid() && IsTrainerNotInQueue(TrainerComponent))
	{
		FAsyncOptionQueueItem QueueItem;
		QueueItem.TrainingOption = TrainingOption;
		QueueItem.Trainer = TrainerComponent;

		const bool bIsFirstItem = M_TrainingQueue.IsEmpty();
		M_TrainingQueue.Enqueue(QueueItem);
		if (bIsFirstItem)
		{
			LoadNextTrainingOptionInQueue();
		}
		return true;
	}
	return false;
}

bool ARTSAsyncSpawner::AsyncSpawnOptionAtLocation(const FTrainingOption TrainingOption,
                                                  const FVector& Location,
                                                  TWeakObjectPtr<UObject> CallbackOwner,
                                                  const int32 SpawnID, TFunction<void(
	                                                  const FTrainingOption&, AActor* SpawnedActor,
	                                                  const int32 ID)> OnSpawnedCallback)
{
	// Validate the TrainingOption
	if (TrainingOption.IsNone())
	{
		RTSFunctionLibrary::ReportError(
			"Attempted to spawn Train_None type!\n"
			"At function AsyncSpawnOptionAtLocation in ARTSAsyncSpawner.cpp\n"
			"The function will return without spawning the option.");
		return false;
	}

	// Check if TrainingOption exists in TrainingOptionMap
	if (!M_TrainingOptionMap.Contains(TrainingOption))
	{
		RTSFunctionLibrary::ReportError(
			"Training option type not found in the map!\n"
			"At function AsyncSpawnOptionAtLocation in ARTSAsyncSpawner.cpp\n"
			"Training option name: " + TrainingOption.GetTrainingName());
		return false;
	}


	const TSoftClassPtr<AActor> AssetClass = M_TrainingOptionMap[TrainingOption];

	// Store the call back for this asset.
	M_SpawnCallbacks.Add(AssetClass.ToSoftObjectPath(),
	                     TPair<TWeakObjectPtr<UObject>,
	                           TFunction<void(const FTrainingOption&, AActor* SpawnedActor, const int32 ID)>>(
		                     CallbackOwner, OnSpawnedCallback));

	// If the asset is already loaded, handle it immediately
	if (AssetClass.IsValid())
	{
		OnAsyncSpawnOptionAtLocationComplete(
			AssetClass.ToSoftObjectPath(),
			TrainingOption,
			Location, SpawnID);
	}
	else
	{
		// Start a new async loading request
		FSoftObjectPath AssetPath = AssetClass.ToSoftObjectPath();

		FStreamableDelegate Delegate = FStreamableDelegate::CreateLambda(
			[this, AssetPath, TrainingOption, Location, SpawnID]()
			{
				OnAsyncSpawnOptionAtLocationComplete(AssetPath, TrainingOption, Location, SpawnID);
			});

		TSharedPtr<FStreamableHandle> LoadingHandle = M_StreamableManager.RequestAsyncLoad(
			AssetPath,
			Delegate);

		if (!LoadingHandle.IsValid())
		{
			RTSFunctionLibrary::ReportError(
				"Failed to request async load for TrainingOption: " + TrainingOption.GetTrainingName() +
				"\nAt function AsyncSpawnOptionAtLocation in ARTSAsyncSpawner.cpp");
			return false;
		}
	}

	return true;
}


void ARTSAsyncSpawner::LoadNextTrainingOptionInQueue()
{
	if (M_TrainingQueue.IsEmpty())
	{
		return;
	}

	const auto [TrainingOption, TrainerComponent] = *M_TrainingQueue.Peek();
	M_TrainingQueue.Pop();

	if (!IsSpawnOptionValid(TrainingOption, TrainerComponent))
	{
		LoadNextTrainingOptionInQueue();
	}

	if (M_TrainingOptionMap.Contains(TrainingOption))
	{
		const TSoftClassPtr<AActor> AssetClass = M_TrainingOptionMap[TrainingOption];

		// Cancel any previous loading request
		if (CancelActiveLoadRequest(EAsyncRequestType::AReq_Training))
		{
			RTSFunctionLibrary::ReportError(
				"A previous Async option load request was cancelled when the next one was loaded."
				"\n at function LoadNextTrainingOptionInQueue in RTSAsyncSpawner.cpp"
				"\n New (to load) Training option name: " + TrainingOption.GetTrainingName());
		}

		// If the asset is already loaded, handle it immediately
		if (AssetClass.IsValid())
		{
			HandleAsyncTrainingLoadComplete(
				AssetClass.ToSoftObjectPath(),
				TrainingOption,
				TrainerComponent);
		}
		else
		{
			// Start a new async loading request
			M_CurrentTrainingOptionLoadingHandle = M_StreamableManager.RequestAsyncLoad(
				AssetClass.ToSoftObjectPath(),
				FStreamableDelegate::CreateUObject(
					this,
					&ARTSAsyncSpawner::HandleAsyncTrainingLoadComplete,
					AssetClass.ToSoftObjectPath(),
					TrainingOption,
					TrainerComponent));
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"Training option type not found in the map! \n At function SpawnNextTrainingOptionInQueue in RTSAsyncSpawner.cpp"
			"\n Training option name: " + TrainingOption.GetTrainingName());
		// Notify the trainer that the training option failed to spawn.
		OnTrainingOptionSpawned(nullptr, TrainerComponent, TrainingOption);
	}
}

bool ARTSAsyncSpawner::IsSpawnOptionValid(
	const FTrainingOption TrainingOption,
	const TWeakObjectPtr<UTrainerComponent> TrainerComponent) const
{
	if (!TrainerComponent.IsValid())
	{
		RTSFunctionLibrary::ReportError("Cannot spawn training option async for null owner!"
			"\n at function SpawnNextTrainingOption in RTSAsyncSpawner.cpp"
			"Spawn request will be ignored."
			"\n Training option name: " + TrainingOption.GetTrainingName());
		return false;
	}
	if (TrainingOption.IsNone())
	{
		RTSFunctionLibrary::ReportError(
			"Attempted to spawn Train_None type! \n At function SpawnNextTrainingOptionInQueue in RTSAsyncSpawner.cpp"
			"\n the function will return without spawning the option.");
		return false;
	}
	return true;
}


void ARTSAsyncSpawner::HandleAsyncTrainingLoadComplete(
	const FSoftObjectPath AssetPath,
	FTrainingOption TrainingOption,
	TWeakObjectPtr<UTrainerComponent> TrainerComponent)
{
	// Reset the handle since the loading is complete
	if (M_CurrentTrainingOptionLoadingHandle.IsValid())
	{
		M_CurrentTrainingOptionLoadingHandle.Reset();
	}
	if (TrainerComponent.IsValid())
	{
		if (AActor* TrainedUnit = AttemptSpawnAsset(AssetPath))
		{
			OnTrainingOptionSpawned(TrainedUnit, TrainerComponent, TrainingOption);
		}
		else
		{
			RTSFunctionLibrary::ReportError("Failed to spawn actor for trainer"
				"Trainer name: " + TrainerComponent->GetName()
				+ "TrainingOption: " + TrainingOption.GetTrainingName());
			OnTrainingOptionSpawned(nullptr, TrainerComponent, TrainingOption);
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError("Could not spawn training option for null trainer!"
			"\n at function HandleAsyncTrainingLoadComplete in RTSAsyncSpawner.cpp"
			"Spawn request will be ignored.");
	}
}

void ARTSAsyncSpawner::OnTrainingOptionSpawned(
	AActor* SpawnedActor,
	TWeakObjectPtr<UTrainerComponent> TrainerComponent,
	const FTrainingOption& TrainingOption) const
{
	if (IsValid(SpawnedActor))
	{
		if (IRTSUnit* RTSUnit = Cast<IRTSUnit>(SpawnedActor))
		{
			RTSUnit->OnRTSUnitSpawned(true);
		}
		else
		{
			RTSFunctionLibrary::ReportError("Could not cast spawned actor to RTS UNIT!"
				"\n No Onspawn disable logic is set."
				"Spawned actor: " + SpawnedActor->GetName()
				+ "Trainer component: " + TrainerComponent->GetName());
		}
	}
	// Teleports the spawned actor to the training socket.
	TrainerComponent->OnOptionSpawned(SpawnedActor, TrainingOption);
}

AActor* ARTSAsyncSpawner::AttemptSpawnAsset(const FSoftObjectPath& AssetPath) const
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return nullptr;
	}
	// Resolve the loaded asset, attempts to find the object in memory
	UObject* LoadedAsset = AssetPath.ResolveObject();
	if (not LoadedAsset)
	{
		return nullptr;
	}
	// Cast the loaded asset to a UClass to obtain actor class
	UClass* AssetClass = Cast<UClass>(LoadedAsset);
	if (not AssetClass)
	{
		return nullptr;
	}
	// Spawn the trained actor at the current location of this spawner
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* SpawnedActor = World->SpawnActor<AActor>(
		AssetClass, GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
	if (IsValid(SpawnedActor))
	{
		// Explicitly call BeginPlay if necessary
		if (!SpawnedActor->HasActorBegunPlay())
		{
			SpawnedActor->DispatchBeginPlay();
		}
		if (APawn* SpawnedPawn = Cast<APawn>(SpawnedActor))
		{
			SpawnedPawn->SpawnDefaultController();
		}
		return SpawnedActor;
	}
	return nullptr;
}

void ARTSAsyncSpawner::AsyncBatchLoadInstantPlaceExpansions(
	TArray<EBuildingExpansionType> TypesToLoad,
	TArray<int32> BxpItemIndices, const TScriptInterface<IBuildingExpansionOwner>& BuildingExpansionOwner,
	TArray<FBxpOptionData> BxpTypesAndConstructionRules)
{
	// Assign a stable ID for this batch
	int32 RequestId = M_NextInstantPlacementRequestId++;

	// Create and store the batch
	FInstantPlacementBxpBatch& Batch = M_InstantPlacementRequests.Add(RequestId);
	Batch.TypesToLoad = MoveTemp(TypesToLoad);
	Batch.Owner = BuildingExpansionOwner;
	Batch.BxpItemIndices = MoveTemp(BxpItemIndices);
	Batch.BxpTypesAndConstructionRules = MoveTemp(BxpTypesAndConstructionRules);

	// Build up the list of asset paths we actually need to load
	TArray<FSoftObjectPath> AssetPaths;
	AssetPaths.Reserve(Batch.TypesToLoad.Num());
	for (EBuildingExpansionType Type : Batch.TypesToLoad)
	{
		if (BuildingExpansionMap.Contains(Type))
		{
			AssetPaths.Add(BuildingExpansionMap[Type].ToSoftObjectPath());
		}
	}

	// Kick off the single, batched async load
	Batch.Handle = M_StreamableManager.RequestAsyncLoad(
		AssetPaths,
		FStreamableDelegate::CreateUObject(
			this,
			&ARTSAsyncSpawner::OnBatchBxpLoadComplete,
			RequestId));
}

void ARTSAsyncSpawner::OnBatchBxpLoadComplete(const int32 RequestId)
{
	// pull out & remove
	auto Batch = ExtractAndRemoveBatch(RequestId);

	if (!Batch.Owner)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Instant‐placement batch canceled: owner no longer valid."));
		return;
	}

	// init results
	TArray<ABuildingExpansion*> SpawnedBxps;
	SpawnedBxps.SetNum(Batch.TypesToLoad.Num());

	// one‐liner per index
	for (int32 i = 0; i < Batch.TypesToLoad.Num(); ++i)
	{
		SpawnedBxps[i] = BatchBxp_TryResolveAndSpawnBxp(Batch.TypesToLoad[i]);
	}

	// final callback
	if (Batch.Owner)
	{
		Batch.Owner->BatchBxp_OnInstantPlacementBxpsSpawned(SpawnedBxps,
		                                                    Batch.BxpItemIndices,
		                                                    Batch.BxpTypesAndConstructionRules,
		                                                    M_PlayerController, Batch.Owner);
	}
}


FInstantPlacementBxpBatch ARTSAsyncSpawner::ExtractAndRemoveBatch(int32 RequestId)
{
	FInstantPlacementBxpBatch Batch = M_InstantPlacementRequests.FindChecked(RequestId);
	M_InstantPlacementRequests.Remove(RequestId);
	return Batch;
}

UClass* ARTSAsyncSpawner::ResolveExpansionClass(EBuildingExpansionType Type) const
{
	if (!BuildingExpansionMap.Contains(Type))
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("No mapping for expansion type %d"), int(Type)));
		return nullptr;
	}
	auto Path = BuildingExpansionMap[Type].ToSoftObjectPath();
	auto* Loaded = Path.ResolveObject();
	return Loaded ? Cast<UClass>(Loaded) : nullptr;
}

ABuildingExpansion* ARTSAsyncSpawner::BatchBxp_SpawnExpansionActor(UClass* Cls) const
{
	if (!Cls) return nullptr;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (auto* A = GetWorld()->SpawnActor<AActor>(Cls, GetActorLocation(), FRotator::ZeroRotator, P))
		return Cast<ABuildingExpansion>(A);
	return nullptr;
}

ABuildingExpansion* ARTSAsyncSpawner::BatchBxp_TryResolveAndSpawnBxp(EBuildingExpansionType Type) const
{
	UClass* Cls = ResolveExpansionClass(Type);
	if (!Cls)
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("Failed to resolve class for type %d"), int(Type)));
		return nullptr;
	}

	auto* Bxp = BatchBxp_SpawnExpansionActor(Cls);
	if (!Bxp)
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("Failed to spawn Bxp for type %d"), int(Type)));
	}
	return Bxp;
}
