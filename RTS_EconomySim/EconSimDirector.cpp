// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "EconSimDirector.h"

#include "AIController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyWaves/EnemyWaveController.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/StrategicTrainingState/StrategicTrainingState.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/GameState/GameResourceManager/GameResourceManager.h"
#include "RTS_Survival/Resources/ResourceDropOff/ResourceDropOff.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicAIBlackboard.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/GameUI/BuildingExpansion/WidgetBxpOptionState/BxpOptionData.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainingQueueItem/QueueItem.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingMenuManager.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/ConstructionPreview/CPPConstructionPreview.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/ResourceConverter/ResourceConverterComponent.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/Resources/Resource.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

DEFINE_LOG_CATEGORY_STATIC(LogEconSim, Log, All);

namespace EconSimPrivate
{
	constexpr uint8 HumanPlayer = 1;
	constexpr uint8 EnemyPlayer = 2;
	constexpr double HudBuildRealSeconds = 5.0;
	constexpr double HarvesterSampleSeconds = 0.25;
	constexpr double ProtectSeconds = 1.0;
	constexpr double SinkSeconds = 2.0;
	// A deploy order first drives the truck to its site; after the real conversion time it is finished by force,
	// because the unattended process never finishes the building montage.
	constexpr double DeployAnimationGraceSeconds = 12.0;
	constexpr double StepTimeoutSeconds = 900.0;
	constexpr double RallyMoveSeconds = 4.0;
	constexpr double DeployRetrySeconds = 8.0;
	constexpr double HarvestOrderCheckSeconds = 5.0;
	constexpr double HarvesterStateLogSeconds = 30.0;
	constexpr float WaveSpawnPointRadius = 2500.f;
	constexpr float GroundTraceHalfHeight = 20000.f;
	constexpr int32 SinkRadixiteFloor = 400;
	constexpr int32 SinkMetalFloor = 100;
	constexpr float SinkStorageFraction = 0.5f;
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	const FName HarvesterRole(TEXT("Harvester"));
	const FName HqRole(TEXT("HQ"));
	const FName RefineryRole(TEXT("Refinery"));
	const FName VaultRole(TEXT("Vault"));

	const TCHAR* HqClass = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/SKDFZ-7/Ger_HQ_Truck.Ger_HQ_Truck_C");
	const TCHAR* ResearchCentreClass = TEXT(
		"/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_ResearchCenter."
		"Rus_Building_ResearchCenter_C");
	const TCHAR* RadixiteFolder = TEXT("/Game/RTS_Survival/Blueprints/Resources/ChildBps/Radixite/");
	const TCHAR* MetalFolder = TEXT("/Game/RTS_Survival/Blueprints/Resources/ChildBps/Metal/");

	FString ResourceClassPath(const TCHAR* Folder, const FString& Name)
	{
		return FString(Folder) + Name + TEXT(".") + Name + TEXT("_C");
	}

	FName BxpRole(const EBuildingExpansionType BxpType)
	{
		return FName(*FString::Printf(TEXT("Bxp_%s"), *Global_GetBxpTypeEnumAsString(BxpType)));
	}

	FString ResourceShortName(const ERTSResourceType Type)
	{
		switch (Type)
		{
		case ERTSResourceType::Resource_Radixite:
			return TEXT("R");
		case ERTSResourceType::Resource_Metal:
			return TEXT("M");
		case ERTSResourceType::Resource_VehicleParts:
			return TEXT("VP");
		default:
			return Global_GetResourceTypeDisplayString(Type);
		}
	}

	FString ExportProperty(const UObject* Object, const TCHAR* PropertyName)
	{
		if (Object == nullptr)
		{
			return TEXT("<null>");
		}
		const FProperty* const Property = Object->GetClass()->FindPropertyByName(PropertyName);
		if (Property == nullptr)
		{
			return TEXT("<missing>");
		}
		FString Text;
		Property->ExportTextItem_Direct(Text, Property->ContainerPtrToValuePtr<void>(Object), nullptr, nullptr, PPF_None);
		return Text;
	}

	bool ReadBoolProperty(const UObject* Object, const TCHAR* PropertyName, bool& bOutValue)
	{
		const FBoolProperty* const Property = Object != nullptr
			                                      ? FindFProperty<FBoolProperty>(Object->GetClass(), PropertyName)
			                                      : nullptr;
		if (Property == nullptr)
		{
			return false;
		}
		bOutValue = Property->GetPropertyValue_InContainer(Object);
		return true;
	}

	bool WriteObjectProperty(UObject* Object, const TCHAR* PropertyName, UObject* Value)
	{
		FObjectPropertyBase* const Property = Object != nullptr
			                                      ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName)
			                                      : nullptr;
		if (Property == nullptr)
		{
			return false;
		}
		Property->SetObjectPropertyValue_InContainer(Object, Value);
		return true;
	}

	uint8 GetOwningPlayer(const AActor* Actor)
	{
		const URTSComponent* const RTSComponent = IsValid(Actor) ? Actor->FindComponentByClass<URTSComponent>() : nullptr;
		return IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : 0;
	}

	FString DescribeUnit(const AActor* Actor)
	{
		const URTSComponent* const RTSComponent = Actor->FindComponentByClass<URTSComponent>();
		if (not IsValid(RTSComponent))
		{
			return TEXT("none,none");
		}
		FString SubType = FString::FromInt(RTSComponent->GetUnitSubType());
		if (Actor->IsA<ATankMaster>())
		{
			SubType = UEnum::GetValueAsString(RTSComponent->GetSubtypeAsTankSubtype());
		}
		else if (Actor->IsA<ASquadController>())
		{
			SubType = UEnum::GetValueAsString(RTSComponent->GetSubtypeAsSquadSubtype());
		}
		return UEnum::GetValueAsString(RTSComponent->GetUnitType()) + TEXT(",") + SubType;
	}

	bool ParseVector2D(const TCHAR* Key, FVector2D& OutValue)
	{
		FString Text;
		if (not FParse::Value(FCommandLine::Get(), Key, Text, /*bShouldStopOnSeparator=*/ false))
		{
			return false;
		}
		FString Left;
		FString Right;
		if (not Text.Split(TEXT(","), &Left, &Right))
		{
			return false;
		}
		OutValue = FVector2D(FCString::Atof(*Left), FCString::Atof(*Right));
		return true;
	}

	// -EconSimConverter=In,Out,Seconds (e.g. 15,5,6.667): input per tick, vehicle parts per tick, tick interval.
	FEconSimConverterOverride ParseConverterOverride(const TCHAR* Key)
	{
		FEconSimConverterOverride Override;
		FString Text;
		if (not FParse::Value(FCommandLine::Get(), Key, Text, /*bShouldStopOnSeparator=*/ false))
		{
			return Override;
		}
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() != 3)
		{
			return Override;
		}
		Override.InputPerTick = FCString::Atoi(*Parts[0]);
		Override.OutputPerTick = FCString::Atoi(*Parts[1]);
		Override.TickSeconds = FCString::Atof(*Parts[2]);
		Override.bActive = Override.InputPerTick > 0 && Override.OutputPerTick > 0 && Override.TickSeconds > 0.f;
		return Override;
	}

	FString TechLevelsAsText(const TMap<EEnemyAITechLevel, bool>& Map)
	{
		FString Text;
		for (const TPair<EEnemyAITechLevel, bool>& Pair : Map)
		{
			if (Pair.Value)
			{
				Text += FString::FromInt(static_cast<int32>(Pair.Key));
			}
		}
		return Text.IsEmpty() ? TEXT("-") : Text;
	}
}

AEconSimDirector::AEconSimDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetActorTickEnabled(true);
}

void AEconSimDirector::BeginPlay()
{
	Super::BeginPlay();
	SetTickableWhenPaused(true);
	M_StartRealSeconds = FPlatformTime::Seconds();
	ReadCommandLine();
	BuildScenario();
	if (UWorld* const World = GetWorld())
	{
		M_ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateUObject(this, &AEconSimDirector::OnActorSpawned));
	}
	UE_LOG(LogEconSim, Display, TEXT("ECONSIM begin minutes=%.1f dilation=%.2f out=%s"), M_SimMinutes, M_TimeDilation,
	       *M_OutputDirectory);
}

void AEconSimDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(M_ActorSpawnedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------------------------------------------
// Configuration

void AEconSimDirector::ReadCommandLine()
{
	const TCHAR* const CommandLine = FCommandLine::Get();
	FParse::Value(CommandLine, TEXT("EconSimMinutes="), M_SimMinutes);
	FParse::Value(CommandLine, TEXT("EconSimSample="), M_SampleSeconds);
	FParse::Value(CommandLine, TEXT("EconSimDilation="), M_TimeDilation);
	FParse::Value(CommandLine, TEXT("EconSimProtectMlt="), M_ProtectDamageMlt);
	bM_KeepPreplacedUnits = FParse::Param(CommandLine, TEXT("EconSimKeepPreplaced"));
	bM_ProtectPlayer = not FParse::Param(CommandLine, TEXT("EconSimNoProtect"));
	bM_EnemyResearchCentre = not FParse::Param(CommandLine, TEXT("EconSimNoResearchCentre"));
	bM_SinkResources = not FParse::Param(CommandLine, TEXT("EconSimNoSink"));
	bM_StarveMissionWaves = not FParse::Param(CommandLine, TEXT("EconSimKeepMissionWaves"));
	FParse::Value(CommandLine, TEXT("EconSimEnemyIncome="), M_EnemyIncomePerMinute);
	FParse::Value(CommandLine, TEXT("EconSimMissionLaneY="), M_MissionLaneY);
	bM_FixEnemyBasePoints = not FParse::Param(CommandLine, TEXT("EconSimRawEnemyBasePoints"));
	M_ConverterOverride = EconSimPrivate::ParseConverterOverride(TEXT("EconSimConverter="));
	M_SmelterOverride = EconSimPrivate::ParseConverterOverride(TEXT("EconSimSmelter="));
	FString StartText;
	if (FParse::Value(CommandLine, TEXT("EconSimStart="), StartText, /*bShouldStopOnSeparator=*/ false))
	{
		TArray<FString> Parts;
		StartText.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() == 3)
		{
			M_StartResources = FIntVector(FCString::Atoi(*Parts[0]), FCString::Atoi(*Parts[1]), FCString::Atoi(*Parts[2]));
		}
	}
	FVector2D BaseOverride;
	if (EconSimPrivate::ParseVector2D(TEXT("EconSimBase="), BaseOverride))
	{
		M_Base = FVector(BaseOverride, 0.f);
		bM_HasBaseOverride = true;
	}
	M_SimMinutes = FMath::Max(1.f, M_SimMinutes);
	M_SampleSeconds = FMath::Max(1.f, M_SampleSeconds);
	M_TimeDilation = FMath::Clamp(M_TimeDilation, 0.25f, 8.f);
	if (not FParse::Value(CommandLine, TEXT("EconSimDir="), M_OutputDirectory))
	{
		M_OutputDirectory = FPaths::ProjectSavedDir() / TEXT("EconomySim") / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	}
	M_OutputDirectory = M_OutputDirectory.TrimQuotes();
}

void AEconSimDirector::BuildScenario()
{
	using namespace EconSimPrivate;
	// Offsets from the base (the HQ) in cm. Everything that deploys stays inside the HQ build radius (45 m).
	M_SiteOffsets.Add(TEXT("RefinerySite"), FVector2D(3800.f, -1800.f));
	M_SiteOffsets.Add(TEXT("VaultSite"), FVector2D(1800.f, -2600.f));

	FEconSimField FieldA;
	FieldA.Name = TEXT("FieldA");
	FieldA.ResourceType = ERTSResourceType::Resource_Radixite;
	FieldA.Centre = FVector(3200.f, 2600.f, 0.f);
	FieldA.NodeClasses = {
		TEXT("Resource_Radixite_Crystal_Large@2800,1800"), TEXT("Resource_Radixite_Crystal_Large@2800,2900"),
		TEXT("Resource_Radixite_Crystal@3700,1800"), TEXT("Resource_Radixite_Crystal@3700,2900"),
		TEXT("Resource_Radixite_Crystal@3250,3700")
	};
	M_Fields.Add(FieldA);

	FEconSimField FieldB;
	FieldB.Name = TEXT("FieldB");
	FieldB.ResourceType = ERTSResourceType::Resource_Radixite;
	FieldB.Centre = FVector(5900.f, -1800.f, 0.f);
	FieldB.NodeClasses = {
		TEXT("Resource_Radixite_Crystal_Large@5600,-1100"),
		TEXT("Resource_Radixite_Crystal_Large@5600,-2500"),
		TEXT("Resource_Radixite_Crystal_Large@6500,-1800")
	};
	M_Fields.Add(FieldB);

	FEconSimField MetalField;
	MetalField.Name = TEXT("Metal");
	MetalField.ResourceType = ERTSResourceType::Resource_Metal;
	MetalField.Centre = FVector(2100.f, -4400.f, 0.f);
	MetalField.NodeClasses = {TEXT("Resource_MetalTower@1400,-4300"), TEXT("Resource_Metal_Ruin1@2900,-4700")};
	M_Fields.Add(MetalField);

	const FTrainingOption Harvester(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_PzI_Harvester));
	const FTrainingOption Refinery(EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerRefinery));
	const FTrainingOption Vault(EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerMetalVault));
	auto Train = [this](const FTrainingOption& Option, const FName StepRole, const FName Target)
	{
		FEconSimStep Step;
		Step.Type = EEconSimStepType::Train;
		Step.Option = Option;
		Step.Role = StepRole;
		Step.Target = Target;
		M_BuildOrder.Add(Step);
	};
	auto Act = [this](const EEconSimStepType Type, const FName StepRole, const FName Target, const EBuildingExpansionType Bxp)
	{
		FEconSimStep Step;
		Step.Type = Type;
		Step.Role = StepRole;
		Step.Target = Target;
		Step.BxpType = Bxp;
		M_BuildOrder.Add(Step);
	};
	const EBuildingExpansionType NoBxp = EBuildingExpansionType::BXT_Invalid;
	// A plain "radixite + metal + conversion" opening: no scavenging, no units bought.
	Train(Harvester, HarvesterRole, TEXT("FieldA"));
	Train(Harvester, HarvesterRole, TEXT("FieldA"));
	Train(Refinery, RefineryRole, NAME_None);
	Act(EEconSimStepType::Deploy, RefineryRole, TEXT("RefinerySite"), NoBxp);
	Act(EEconSimStepType::Expand, RefineryRole, NAME_None, EBuildingExpansionType::BTX_RefConverter);
	Act(EEconSimStepType::EnableConversion, BxpRole(EBuildingExpansionType::BTX_RefConverter), NAME_None, NoBxp);
	Train(Harvester, HarvesterRole, TEXT("FieldB"));
	Train(Harvester, HarvesterRole, TEXT("Metal"));
	Train(Vault, VaultRole, NAME_None);
	Act(EEconSimStepType::Deploy, VaultRole, TEXT("VaultSite"), NoBxp);
	Act(EEconSimStepType::Expand, VaultRole, NAME_None, EBuildingExpansionType::BTX_GerMetalVaultSmelter);
	Act(EEconSimStepType::EnableConversion, BxpRole(EBuildingExpansionType::BTX_GerMetalVaultSmelter), NAME_None, NoBxp);
	Train(Harvester, HarvesterRole, TEXT("FieldB"));
	Train(Harvester, HarvesterRole, TEXT("Metal"));
	Act(EEconSimStepType::Expand, RefineryRole, NAME_None, EBuildingExpansionType::BTX_RefKaskade);
}

// ---------------------------------------------------------------------------------------------------------------
// Tick

void AEconSimDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (M_Phase == EEconSimPhase::Done || not IsValid(GetWorld()))
	{
		return;
	}
	const double RealSeconds = FPlatformTime::Seconds() - M_StartRealSeconds;
	const double MaxRealSeconds = 900.0 + 3.0 * M_SimMinutes * 60.0 / M_TimeDilation;
	if (RealSeconds > MaxRealSeconds)
	{
		Finish(TEXT("real_time_cap"));
		return;
	}
	KeepGameRunning();
	ProcessNewActors();
	switch (M_Phase)
	{
	case EEconSimPhase::WaitForGame:
		if (RealSeconds >= EconSimPrivate::HudBuildRealSeconds && TryStartGame())
		{
			SetupScenario();
		}
		break;
	case EEconSimPhase::WaitForHq:
		TickWaitForHq();
		break;
	case EEconSimPhase::Running:
		{
			const double Now = GetGameSeconds();
			TickBuildOrder();
			if (Now - M_LastHarvesterSampleSeconds >= EconSimPrivate::HarvesterSampleSeconds)
			{
				M_LastHarvesterSampleSeconds = Now;
				SampleHarvesters();
			}
			if (Now - M_LastHarvestOrderCheckSeconds >= EconSimPrivate::HarvestOrderCheckSeconds)
			{
				M_LastHarvestOrderCheckSeconds = Now;
				KeepHarvestersOnTheirFields();
			}
			if (Now - M_LastHarvesterStateLogSeconds >= EconSimPrivate::HarvesterStateLogSeconds)
			{
				M_LastHarvesterStateLogSeconds = Now;
				LogHarvesterStates();
			}
			StarveMissionWaves();
			if (Now - M_LastProtectSeconds >= EconSimPrivate::ProtectSeconds)
			{
				M_LastProtectSeconds = Now;
				ProtectPlayerActors();
			}
			if (Now - M_LastSinkSeconds >= EconSimPrivate::SinkSeconds)
			{
				M_LastSinkSeconds = Now;
				RunResourceSink();
				UpdateEnemyPressure();
				if (bM_StarveMissionWaves)
				{
					SilenceMissions();
				}
			}
			if (Now - M_LastSampleSeconds >= M_SampleSeconds)
			{
				M_LastSampleSeconds = Now;
				WriteTimelineRow();
			}
			if (Now - M_SimStartGameSeconds >= M_SimMinutes * 60.0)
			{
				Finish(TEXT("done"));
			}
			break;
		}
	default:
		break;
	}
}

bool AEconSimDirector::TryStartGame()
{
	M_PlayerController = Cast<ACPPController>(GetWorld()->GetFirstPlayerController());
	if (not M_PlayerController.IsValid())
	{
		return false;
	}
	UPlayerStartGameControl* const StartGameControl = M_PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	return true;
}

void AEconSimDirector::KeepGameRunning() const
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(Picker->WidgetTree->FindWidget(EconSimPrivate::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	ACPPController* const PlayerController = M_PlayerController.Get();
	if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
	{
		PlayerController->PauseAndLockGame(false);
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Scenario setup

void AEconSimDirector::SetupScenario()
{
	if (not bM_HasBaseOverride)
	{
		// UT_EconomySim's RTS_PLayerStart; the actor itself is gone by the time the director joins a -game world.
		M_Base = FVector(-21595.f, -15029.f, 0.f);
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (It->GetClass()->GetName().StartsWith(TEXT("RTS_PLayerStart")))
			{
				M_Base = It->GetActorLocation();
				break;
			}
		}
	}
	M_Base = GroundAt(M_Base);
	if (not FMath::IsNearlyEqual(M_TimeDilation, 1.f))
	{
		AWorldSettings* const WorldSettings = GetWorld()->GetWorldSettings();
		WorldSettings->MaxGlobalTimeDilation = FMath::Max(WorldSettings->MaxGlobalTimeDilation, M_TimeDilation);
		WorldSettings->TimeDilation = M_TimeDilation;
	}
	OpenOutputFiles();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName().StartsWith(TEXT("BP_Mission_SpawnPoint")))
		{
			M_MissionSpawnPoints.Add(It->GetActorLocation());
		}
	}
	if (URTSGameInstance* const GameInstance = GetWorld()->GetGameInstance<URTSGameInstance>())
	{
		if (not GameInstance->GetHasPlayerFaction())
		{
			// The game falls back to the Strike Division without a pick; make that explicit and stop the error spam.
			GameInstance->SetPlayerFaction(ERTSFaction::GerStrikeDivision);
		}
	}
	LogEvent(TEXT("setup"), FString::Printf(TEXT("base=%s wave_spawn_points=%d"), *M_Base.ToCompactString(),
	                                        M_MissionSpawnPoints.Num()));
	Setup_ClearPreplacedUnits();
	Setup_SpawnEnemyResearchCentre();
	Setup_SpawnResourceFields();
	Setup_SpawnHq();
	M_Phase = EEconSimPhase::WaitForHq;
	M_StepStartSeconds = GetGameSeconds();
}

void AEconSimDirector::Setup_ClearPreplacedUnits()
{
	using namespace EconSimPrivate;
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* const Actor = *It;
		if (Actor->IsA<ACPPResourceMaster>())
		{
			ToDestroy.Add(Actor);
			continue;
		}
		const bool bIsUnit = Actor->IsA<ATankMaster>() || Actor->IsA<ASquadController>() || Actor->IsA<ASquadUnit>();
		if (not bIsUnit)
		{
			continue;
		}
		const uint8 OwnerPlayer = GetOwningPlayer(Actor);
		if (OwnerPlayer == EnemyPlayer)
		{
			M_EnemyTanksPreplaced += Actor->IsA<ATankMaster>() ? 1 : 0;
			M_EnemySquadsPreplaced += Actor->IsA<ASquadController>() ? 1 : 0;
			AppendLine(TEXT("enemy_units.csv"), FString::Printf(TEXT("0.0,preplaced,%s,%s"), *Actor->GetClass()->GetName(),
			                                                    *DescribeUnit(Actor)));
			if (bM_KeepPreplacedUnits)
			{
				M_SeenActors.Add(Actor);
				M_EnemyUnits.Add(Actor);
				continue;
			}
		}
		if (OwnerPlayer == HumanPlayer || OwnerPlayer == EnemyPlayer)
		{
			ToDestroy.Add(Actor);
		}
	}
	UPlayerResourceManager* const Resources = GetResources();
	for (AActor* const Actor : ToDestroy)
	{
		M_SeenActors.Add(Actor);
		// Destroy() skips the death path, which is what takes a truck's drop off (storage) away from the player.
		// (Also taking it out of UGameResourceManager's registry broke all harvesting in run 6; left alone.)
		const ANomadicVehicle* const Truck = Cast<ANomadicVehicle>(Actor);
		if (IsValid(Truck) && IsValid(Resources) && IsValid(Truck->GetResourceDropOff())
			&& GetOwningPlayer(Actor) == HumanPlayer)
		{
			Resources->RegisterDropOff(Truck->GetResourceDropOff(), false);
		}
		Actor->Destroy();
	}
	LogEvent(TEXT("cleared"), FString::Printf(TEXT("destroyed=%d enemy_preplaced_tanks=%d squads=%d kept=%d"),
	                                          ToDestroy.Num(), M_EnemyTanksPreplaced, M_EnemySquadsPreplaced,
	                                          bM_KeepPreplacedUnits ? 1 : 0));
}

void AEconSimDirector::Setup_SpawnEnemyResearchCentre()
{
	using namespace EconSimPrivate;
	if (not bM_EnemyResearchCentre)
	{
		return;
	}
	FVector2D At(12900.f, -2400.f);
	ParseVector2D(TEXT("EconSimResearchAt="), At);
	UClass* const Class = LoadClass<AActor>(nullptr, ResearchCentreClass);
	if (not IsValid(Class))
	{
		LogEvent(TEXT("warning"), TEXT("research centre class not found"));
		return;
	}
	const FTransform Transform(FRotator::ZeroRotator, GroundAt(FVector(At, 0.f)));
	AActor* const Building = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*GetWorld(), Class, Transform, EnemyPlayer);
	LogEvent(TEXT("enemy_research_centre"), FString::Printf(TEXT("spawned=%d at=%s"), IsValid(Building) ? 1 : 0,
	                                                         *Transform.GetLocation().ToCompactString()));
}

void AEconSimDirector::Setup_SpawnResourceFields()
{
	using namespace EconSimPrivate;
	for (FEconSimField& Field : M_Fields)
	{
		const TCHAR* const Folder = Field.ResourceType == ERTSResourceType::Resource_Metal ? MetalFolder : RadixiteFolder;
		for (const FString& Entry : Field.NodeClasses)
		{
			FString ClassName;
			FString OffsetText;
			Entry.Split(TEXT("@"), &ClassName, &OffsetText);
			FString XText;
			FString YText;
			OffsetText.Split(TEXT(","), &XText, &YText);
			UClass* const Class = LoadClass<AActor>(nullptr, *ResourceClassPath(Folder, ClassName));
			if (not IsValid(Class))
			{
				LogEvent(TEXT("warning"), TEXT("resource class not found ") + ClassName);
				continue;
			}
			const FVector Location = GroundAt(BaseOffset(FVector2D(FCString::Atof(*XText), FCString::Atof(*YText))));
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* const Node = GetWorld()->SpawnActor<AActor>(Class, FTransform(Location), Parameters);
			if (not IsValid(Node))
			{
				continue;
			}
			M_SeenActors.Add(Node);
			Field.Nodes.Add(Node);
		}
	}
}

void AEconSimDirector::Setup_SpawnHq()
{
	using namespace EconSimPrivate;
	UClass* const Class = LoadClass<AActor>(nullptr, HqClass);
	if (not IsValid(Class))
	{
		Finish(TEXT("hq_class_missing"));
		return;
	}
	const FVector Location = NavPointNear(M_Base) + FVector(0.f, 0.f, 100.f);
	AActor* const Unit = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*GetWorld(), Class, FTransform(Location), HumanPlayer);
	M_Hq = Cast<ANomadicVehicle>(Unit);
	if (not M_Hq.IsValid())
	{
		Finish(TEXT("hq_spawn_failed"));
		return;
	}
	M_SeenActors.Add(Unit);
	RegisterPlayerActor(Unit);
	M_Roles.Add(HqRole, Unit);
	const ECommandQueueError Error = M_Hq->CreateBuildingAtLocation(M_Base, FRotator::ZeroRotator, true);
	LogEvent(TEXT("hq_deploy_ordered"), FString::Printf(TEXT("error=%d"), static_cast<int32>(Error)));
}

void AEconSimDirector::TickWaitForHq()
{
	ANomadicVehicle* const Hq = M_Hq.Get();
	if (not IsValid(Hq))
	{
		Finish(TEXT("hq_lost"));
		return;
	}
	if (not TickForceFinishDeploy(Hq))
	{
		if (GetGameSeconds() - M_StepStartSeconds > EconSimPrivate::StepTimeoutSeconds)
		{
			Finish(TEXT("hq_deploy_timeout"));
		}
		return;
	}
	UPlayerResourceManager* const Resources = GetResources();
	if (not IsValid(Resources))
	{
		return;
	}
	LogEvent(TEXT("hq_built"), FString::Printf(TEXT("before_grant R=%d/%d M=%d/%d VP=%d/%d"),
	                                           Resources->GetResourceAmount(ERTSResourceType::Resource_Radixite),
	                                           Resources->GetResourceStorage(ERTSResourceType::Resource_Radixite),
	                                           Resources->GetResourceAmount(ERTSResourceType::Resource_Metal),
	                                           Resources->GetResourceStorage(ERTSResourceType::Resource_Metal),
	                                           Resources->GetResourceAmount(ERTSResourceType::Resource_VehicleParts),
	                                           Resources->GetResourceStorage(ERTSResourceType::Resource_VehicleParts)));
	GrantStartResources();
	M_SimStartGameSeconds = GetGameSeconds();
	M_StepStartSeconds = M_SimStartGameSeconds;
	M_Phase = EEconSimPhase::Running;
	WriteContextFile();
	WriteTimelineRow();
}

void AEconSimDirector::GrantStartResources() const
{
	UPlayerResourceManager* const Resources = GetResources();
	const TPair<ERTSResourceType, int32> Targets[] = {
		{ERTSResourceType::Resource_Radixite, M_StartResources.X},
		{ERTSResourceType::Resource_Metal, M_StartResources.Y},
		{ERTSResourceType::Resource_VehicleParts, M_StartResources.Z}
	};
	for (const TPair<ERTSResourceType, int32>& Target : Targets)
	{
		const int32 Missing = Target.Value - Resources->GetResourceAmount(Target.Key);
		if (Missing > 0)
		{
			Resources->AddResource(Target.Key, Missing);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Build order

void AEconSimDirector::TickBuildOrder()
{
	if (not M_BuildOrder.IsValidIndex(M_StepIndex))
	{
		return;
	}
	const FEconSimStep& Step = M_BuildOrder[M_StepIndex];
	bool bDone = false;
	switch (Step.Type)
	{
	case EEconSimStepType::Train:
		bDone = TickStep_Train(Step);
		break;
	case EEconSimStepType::Deploy:
		bDone = TickStep_Deploy(Step);
		break;
	case EEconSimStepType::Expand:
		bDone = TickStep_Expand(Step);
		break;
	case EEconSimStepType::EnableConversion:
		bDone = TickStep_EnableConversion(Step);
		break;
	}
	if (bDone)
	{
		return;
	}
	if (GetGameSeconds() - M_StepStartSeconds > EconSimPrivate::StepTimeoutSeconds)
	{
		AdvanceStep(TEXT("timeout"));
	}
}

void AEconSimDirector::AdvanceStep(const FString& Note)
{
	const FEconSimStep& Step = M_BuildOrder[M_StepIndex];
	const double Took = GetGameSeconds() - M_StepStartSeconds;
	LogEvent(TEXT("step_done"), FString::Printf(TEXT("index=%d type=%d role=%s target=%s bxp=%s took=%.1f note=%s"),
	                                            M_StepIndex, static_cast<int32>(Step.Type), *Step.Role.ToString(),
	                                            *Step.Target.ToString(), *Global_GetBxpTypeEnumAsString(Step.BxpType),
	                                            Took, *Note));
	++M_StepIndex;
	bM_StepQueued = false;
	M_DeployConversionStartSeconds = -1.0;
	M_LastDeployOrderSeconds = -1000.0;
	M_PendingExpansion = FEconSimPendingExpansion();
	M_StepStartSeconds = GetGameSeconds();
	if (not M_BuildOrder.IsValidIndex(M_StepIndex))
	{
		M_BuildOrderDoneSeconds = GetGameSeconds();
		LogEvent(TEXT("build_order_done"), FString::Printf(TEXT("t=%.1f"), M_BuildOrderDoneSeconds - M_SimStartGameSeconds));
	}
}

bool AEconSimDirector::TickStep_Train(const FEconSimStep& Step)
{
	if (not bM_StepQueued)
	{
		bM_StepQueued = QueueOnHqTrainer(Step.Option);
		return false;
	}
	if (not TryClaimTrainedUnit(Step))
	{
		return false;
	}
	AdvanceStep(TEXT("trained"));
	return true;
}

bool AEconSimDirector::QueueOnHqTrainer(const FTrainingOption& Option)
{
	UTrainerComponent* const Trainer = GetHqTrainer();
	if (not IsValid(Trainer) || not Trainer->GetIsAbleToTrain())
	{
		return false;
	}
	FTrainingQueueItem QueueItem;
	QueueItem.ItemID = Option;
	const ACPPController* const Controller = GetController();
	const UMainGameUI* const MainGameUI = IsValid(Controller) ? Controller->GetMainMenuUI() : nullptr;
	const UTrainingMenuManager* const MenuManager = IsValid(MainGameUI) ? MainGameUI->GetTrainingMenuManager() : nullptr;
	if (IsValid(MenuManager))
	{
		QueueItem.TotalTrainingTime = MenuManager->GetTrainingOptionState(Option).TrainingTime;
		QueueItem.RemainingTrainingTime = QueueItem.TotalTrainingTime;
	}
	QueueItem.WidgetIndex = Trainer->GetTrainingOptions().IndexOfByKey(Option);
	bool bIsValidCosts = false;
	QueueItem.TotalResourceCosts = FRTS_Statics::GetResourceCostsOfTrainingOption(Option, bIsValidCosts, this);
	QueueItem.RemainingResourceCosts = QueueItem.TotalResourceCosts;
	FString CostText;
	for (const TPair<ERTSResourceType, int32>& Cost : QueueItem.TotalResourceCosts)
	{
		CostText += FString::Printf(TEXT("%s%d "), *EconSimPrivate::ResourceShortName(Cost.Key), Cost.Value);
	}
	Trainer->AddToQueue(QueueItem);
	LogEvent(TEXT("queued"), FString::Printf(TEXT("%s time=%d cost=%s valid=%d menu=%d"), *(UEnum::GetValueAsString(Option.UnitType) + TEXT(":") + FString::FromInt(Option.SubtypeValue)),
	                                         QueueItem.TotalTrainingTime, *CostText, bIsValidCosts ? 1 : 0,
	                                         IsValid(MenuManager) ? 1 : 0));
	return true;
}

bool AEconSimDirector::TryClaimTrainedUnit(const FEconSimStep& Step)
{
	using namespace EconSimPrivate;
	const bool bWantsHarvester = Step.Role == HarvesterRole;
	for (FEconSimProtectedActor& Entry : M_ProtectedActors)
	{
		AActor* const Actor = Entry.Actor.Get();
		if (not IsValid(Actor) || not Actor->IsA<ATankMaster>())
		{
			continue;
		}
		bool bClaimed = false;
		for (const TPair<FName, TWeakObjectPtr<AActor>>& RoleEntry : M_Roles)
		{
			bClaimed |= RoleEntry.Value.Get() == Actor;
		}
		for (const FEconSimHarvester& Track : M_Harvesters)
		{
			bClaimed |= Track.Unit.Get() == Actor;
		}
		if (bClaimed)
		{
			continue;
		}
		UHarvester* const Harvester = Actor->FindComponentByClass<UHarvester>();
		const URTSComponent* const RTSComponent = Actor->FindComponentByClass<URTSComponent>();
		const bool bIsMatchingNomadic = Actor->IsA<ANomadicVehicle>() && IsValid(RTSComponent)
			&& RTSComponent->GetUnitSubType() == Step.Option.SubtypeValue;
		const bool bMatches = bWantsHarvester ? IsValid(Harvester) && not Actor->IsA<ANomadicVehicle>() : bIsMatchingNomadic;
		if (not bMatches)
		{
			continue;
		}
		const double Waited = GetGameSeconds() - M_StepStartSeconds;
		if (bWantsHarvester)
		{
			FEconSimHarvester& Track = M_Harvesters.AddDefaulted_GetRef();
			Track.Unit = Actor;
			Track.Harvester = Harvester;
			Track.Field = Step.Target;
			Track.Index = M_Harvesters.Num() - 1;
			Track.SpawnSeconds = GetGameSeconds();
		}
		else
		{
			M_Roles.Add(Step.Role, Actor);
		}
		LogEvent(TEXT("trained"), FString::Printf(TEXT("%s role=%s target=%s waited=%.1f"), *Actor->GetName(),
		                                          *Step.Role.ToString(), *Step.Target.ToString(), Waited));
		return true;
	}
	return false;
}

AActor* AEconSimDirector::FindBestNode(const FName FieldName) const
{
	const FEconSimField* const Field = M_Fields.FindByPredicate([FieldName](const FEconSimField& Candidate)
	{
		return Candidate.Name == FieldName;
	});
	if (Field == nullptr)
	{
		return nullptr;
	}
	int32 AlreadyOnField = 0;
	for (const FEconSimHarvester& Track : M_Harvesters)
	{
		AlreadyOnField += Track.Field == FieldName ? 1 : 0;
	}
	TArray<AActor*> Live;
	for (const TWeakObjectPtr<AActor>& Node : Field->Nodes)
	{
		const UResourceComponent* const Resource = Node.IsValid() ? Node->FindComponentByClass<UResourceComponent>() : nullptr;
		if (IsValid(Resource) && Resource->GetTotalAmount() > 0)
		{
			Live.Add(Node.Get());
		}
	}
	return Live.IsEmpty() ? nullptr : Live[AlreadyOnField % Live.Num()];
}

void AEconSimDirector::OrderHarvest(AActor* Unit, const FName FieldName) const
{
	ICommands* const Commands = Cast<ICommands>(Unit);
	AActor* const Node = FindBestNode(FieldName);
	if (Commands == nullptr || Node == nullptr)
	{
		UE_LOG(LogEconSim, Warning, TEXT("ECONSIM cannot order harvest on %s"), *FieldName.ToString());
		return;
	}
	const ECommandQueueError Error = Commands->HarvestResource(Node, true);
	UE_LOG(LogEconSim, Display, TEXT("ECONSIM harvest %s -> %s error=%d"), *Unit->GetName(), *Node->GetName(),
	       static_cast<int32>(Error));
}

bool AEconSimDirector::TickStep_Deploy(const FEconSimStep& Step)
{
	ANomadicVehicle* const Truck = GetBuilding(Step.Role);
	if (not IsValid(Truck))
	{
		return false;
	}
	const double Now = GetGameSeconds();
	// A freshly trained truck first takes the trainer's rally move, which would cancel an earlier deploy order.
	const bool bRallyMoveOver = Now - M_StepStartSeconds >= EconSimPrivate::RallyMoveSeconds;
	const bool bIdleTruck = Truck->GetIsInTruckMode() && Truck->GetIsUnitIdle();
	if (bRallyMoveOver && bIdleTruck && Now - M_LastDeployOrderSeconds >= EconSimPrivate::DeployRetrySeconds)
	{
		const FVector2D* const Offset = M_SiteOffsets.Find(Step.Target);
		const FVector Site = NavPointNear(BaseOffset(Offset != nullptr ? *Offset : FVector2D::ZeroVector));
		const ECommandQueueError Error = Truck->CreateBuildingAtLocation(Site, FRotator::ZeroRotator, true);
		LogEvent(TEXT("deploy_ordered"), FString::Printf(TEXT("%s at=%s error=%d retry=%d"), *Step.Role.ToString(),
		                                                 *Site.ToCompactString(), static_cast<int32>(Error),
		                                                 bM_StepQueued ? 1 : 0));
		M_LastDeployOrderSeconds = Now;
		bM_StepQueued = true;
		return false;
	}
	if (not bM_StepQueued)
	{
		return false;
	}
	if (not TickForceFinishDeploy(Truck))
	{
		return false;
	}
	AdvanceStep(TEXT("deployed"));
	return true;
}

bool AEconSimDirector::TickForceFinishDeploy(ANomadicVehicle* Truck)
{
	const ENomadStatus Status = Truck->GetNomadicStatus();
	if (Status == ENomadStatus::Building)
	{
		return true;
	}
	if (Status == ENomadStatus::Truck)
	{
		return false;
	}
	const double Now = GetGameSeconds();
	if (M_DeployConversionStartSeconds < 0.0)
	{
		M_DeployConversionStartSeconds = Now;
	}
	if (Now - M_DeployConversionStartSeconds < EconSimPrivate::DeployAnimationGraceSeconds)
	{
		return false;
	}
	const bool bFinished = Truck->ShippingTest_ForceFinishConversionToBuilding();
	if (bFinished)
	{
		M_DeployConversionStartSeconds = -1.0;
	}
	return bFinished;
}

bool AEconSimDirector::TickStep_Expand(const FEconSimStep& Step)
{
	ANomadicVehicle* const Building = GetBuilding(Step.Role);
	UPlayerResourceManager* const Resources = GetResources();
	if (not IsValid(Building) || not IsValid(Resources) || Building->GetNomadicStatus() != ENomadStatus::Building)
	{
		return false;
	}
	if (not M_PendingExpansion.bPreviewStarted)
	{
		if (Building->IsBuildingAbleToExpand() && Resources->GetCanPayForBxp(Step.BxpType))
		{
			TryStartExpansionPreview(Building, Step.BxpType);
		}
		return false;
	}
	if (not M_PendingExpansion.bPlaced)
	{
		TryPlaceExpansion();
		return false;
	}
	ABuildingExpansion* const Expansion = M_PendingExpansion.Expansion.Get();
	if (not IsValid(Expansion))
	{
		M_PendingExpansion.Expansion = Building->GetBuildingExpansionAtIndex(M_PendingExpansion.SlotIndex);
		return false;
	}
	if (Expansion->GetBuildingExpansionStatus() != EBuildingExpansionStatus::BXS_Built)
	{
		return false;
	}
	M_Roles.Add(EconSimPrivate::BxpRole(Step.BxpType), Expansion);
	RegisterPlayerActor(Expansion);
	AdvanceStep(TEXT("built ") + Expansion->GetName());
	return true;
}

bool AEconSimDirector::TryStartExpansionPreview(ANomadicVehicle* Building, const EBuildingExpansionType BxpType)
{
	ACPPController* const Controller = GetController();
	if (not IsValid(Controller))
	{
		return false;
	}
	const TArray<FBxpOptionData> Options = Building->GetUnlockedBuildingExpansionTypes();
	const FBxpOptionData* const Option = Options.FindByPredicate([BxpType](const FBxpOptionData& Candidate)
	{
		return Candidate.ExpansionType == BxpType;
	});
	if (Option == nullptr)
	{
		FString Offered;
		for (const FBxpOptionData& Candidate : Options)
		{
			Offered += Global_GetBxpTypeEnumAsString(Candidate.ExpansionType) + TEXT(" ");
		}
		LogEvent(TEXT("warning"), FString::Printf(TEXT("%s not offered by %s (offers %s)"),
		                                          *Global_GetBxpTypeEnumAsString(BxpType), *Building->GetName(), *Offered));
		AdvanceStep(TEXT("not_offered"));
		return false;
	}
	const TArray<FBuildingExpansionItem>& Slots = Building->GetBuildingExpansions();
	const int32 Slot = Slots.IndexOfByPredicate([](const FBuildingExpansionItem& Item)
	{
		return Item.Expansion == nullptr && Item.ExpansionStatus == EBuildingExpansionStatus::BXS_UnlockedButNotBuild;
	});
	if (Slot == INDEX_NONE)
	{
		LogEvent(TEXT("warning"), TEXT("no free expansion slot on ") + Building->GetName());
		AdvanceStep(TEXT("no_slot"));
		return false;
	}
	TScriptInterface<IBuildingExpansionOwner> BxpOwner;
	BxpOwner.SetObject(Building);
	BxpOwner.SetInterface(static_cast<IBuildingExpansionOwner*>(Building));
	Controller->ExpandBuildingWithBxpAndStartPreview(*Option, BxpOwner, Slot, false);
	M_PendingExpansion.Building = Building;
	M_PendingExpansion.SlotIndex = Slot;
	M_PendingExpansion.bPreviewStarted = true;
	LogEvent(TEXT("bxp_preview"), FString::Printf(TEXT("%s slot=%d rules=%d"), *Global_GetBxpTypeEnumAsString(BxpType),
	                                              Slot, static_cast<int32>(Option->BxpConstructionRules.ConstructionType)));
	return true;
}

bool AEconSimDirector::TryPlaceExpansion()
{
	ACPPController* const Controller = GetController();
	ANomadicVehicle* const Building = M_PendingExpansion.Building.Get();
	if (not IsValid(Controller) || not IsValid(Building) || not Controller->MapTest_GetIsBxpWaitingForPlacement())
	{
		return false;
	}
	const FEconSimStep& Step = M_BuildOrder[M_StepIndex];
	if (not AimPreviewAtSlot(Building, Step.BxpType))
	{
		return false;
	}
	FVector Click = Controller->CPPConstructionPreviewRef->GetActorLocation();
	const bool bPlaced = Controller->MapTest_PlaceLoadedBxpAtLocation(Click);
	M_PendingExpansion.bPlaced = bPlaced;
	M_PendingExpansion.Expansion = Building->GetBuildingExpansionAtIndex(M_PendingExpansion.SlotIndex);
	LogEvent(TEXT("bxp_placed"), FString::Printf(TEXT("%s placed=%d at=%s"), *Global_GetBxpTypeEnumAsString(Step.BxpType),
	                                             bPlaced ? 1 : 0, *Click.ToCompactString()));
	if (not bPlaced)
	{
		// Start over: the next tick opens the preview again once the building can pay and expand.
		M_PendingExpansion = FEconSimPendingExpansion();
	}
	return bPlaced;
}

bool AEconSimDirector::AimPreviewAtSlot(ANomadicVehicle* Building, const EBuildingExpansionType BxpType) const
{
	// Headless there is no cursor, so the preview never snaps by itself: put it where the snap would put it.
	ACPPController* const Controller = GetController();
	ACPPConstructionPreview* const Preview = IsValid(Controller) ? Controller->CPPConstructionPreviewRef : nullptr;
	UStaticMeshComponent* const AttachMesh = Building->GetAttachToMeshComponent();
	if (not IsValid(Preview) || not IsValid(AttachMesh))
	{
		return false;
	}
	const TArray<FBxpOptionData> Options = Building->GetUnlockedBuildingExpansionTypes();
	const FBxpOptionData* const Option = Options.FindByPredicate([BxpType](const FBxpOptionData& Candidate)
	{
		return Candidate.ExpansionType == BxpType;
	});
	const EBxpConstructionType Type = Option != nullptr ? Option->BxpConstructionRules.ConstructionType
		                                  : EBxpConstructionType::None;
	if (Type == EBxpConstructionType::Socket)
	{
		const TArray<UStaticMeshSocket*> FreeSockets = Building->GetFreeSocketList(M_PendingExpansion.SlotIndex);
		if (FreeSockets.IsEmpty() || FreeSockets[0] == nullptr)
		{
			UE_LOG(LogEconSim, Warning, TEXT("ECONSIM no free socket on %s"), *Building->GetName());
			return false;
		}
		const FName SocketName = FreeSockets[0]->SocketName;
		Preview->SetActorLocationAndRotation(AttachMesh->GetSocketLocation(SocketName),
		                                     AttachMesh->GetSocketRotation(SocketName));
		return EconSimPrivate::WriteObjectProperty(Preview, TEXT("M_SocketToSnapTo"), FreeSockets[0]);
	}
	if (Type == EBxpConstructionType::Free)
	{
		const FVector Spot = GroundAt(Building->GetActorLocation() + Building->GetActorRightVector() * 1500.f);
		Preview->SetActorLocation(Spot);
		return true;
	}
	Preview->SetActorLocationAndRotation(AttachMesh->GetComponentLocation(), AttachMesh->GetComponentRotation());
	return true;
}

bool AEconSimDirector::TickStep_EnableConversion(const FEconSimStep& Step)
{
	const TWeakObjectPtr<AActor>* const Found = M_Roles.Find(Step.Role);
	AActor* const Expansion = Found != nullptr ? Found->Get() : nullptr;
	if (not IsValid(Expansion))
	{
		AdvanceStep(TEXT("no_expansion"));
		return true;
	}
	UResourceConverterComponent* const Converter = Expansion->FindComponentByClass<UResourceConverterComponent>();
	if (not bM_StepQueued)
	{
		if (ICommands* const Commands = Cast<ICommands>(Expansion))
		{
			Commands->NoQueue_SetResourceConversionEnabled(true);
		}
		bM_StepQueued = true;
		return false;
	}
	bool bEnabled = false;
	EconSimPrivate::ReadBoolProperty(Converter, TEXT("bM_Enabled"), bEnabled);
	if (not bEnabled && GetGameSeconds() - M_StepStartSeconds > 2.0 && IsValid(Converter))
	{
		// The command path did not switch it on: press the component's own toggle.
		Converter->SetResourceConversionEnabled(true);
		EconSimPrivate::ReadBoolProperty(Converter, TEXT("bM_Enabled"), bEnabled);
		AdvanceStep(FString::Printf(TEXT("fallback_toggle enabled=%d"), bEnabled ? 1 : 0));
		return true;
	}
	if (not bEnabled)
	{
		return false;
	}
	ApplyConverterOverride(Converter);
	AdvanceStep(TEXT("conversion_enabled ") + EconSimPrivate::ExportProperty(Converter, TEXT("M_Settings")));
	return true;
}

void AEconSimDirector::ApplyConverterOverride(UResourceConverterComponent* Converter)
{
	// Test-only what-if: rewrite the live converter's tick settings (Blueprint data is untouched) and restart its
	// timer, so a rebalancing option can be measured before anyone edits the Blueprint.
	const FStructProperty* const SettingsProperty = Converter != nullptr
		                                                ? FindFProperty<FStructProperty>(
			                                                UResourceConverterComponent::StaticClass(),
			                                                TEXT("M_Settings"))
		                                                : nullptr;
	if (SettingsProperty == nullptr || SettingsProperty->Struct != FResourceConverterSettings::StaticStruct())
	{
		return;
	}
	FResourceConverterSettings* const Settings = SettingsProperty->ContainerPtrToValuePtr<FResourceConverterSettings>(
		Converter);
	const int32* const RadixiteDelta = Settings->Tick.ResourceDeltas.Find(ERTSResourceType::Resource_Radixite);
	const bool bIsRadixiteConverter = RadixiteDelta != nullptr && *RadixiteDelta < 0;
	const FEconSimConverterOverride& Override = bIsRadixiteConverter ? M_ConverterOverride : M_SmelterOverride;
	if (not Override.bActive)
	{
		return;
	}
	const ERTSResourceType Input = bIsRadixiteConverter ? ERTSResourceType::Resource_Radixite
		                               : ERTSResourceType::Resource_Metal;
	Settings->Tick.ResourceDeltas.Add(Input, -Override.InputPerTick);
	Settings->Tick.ResourceDeltas.Add(ERTSResourceType::Resource_VehicleParts, Override.OutputPerTick);
	Settings->Tick.TickRate = Override.TickSeconds;
	// Off and on again: StartResourceTimer reads the new TickRate.
	Converter->SetResourceConversionEnabled(false);
	Converter->SetResourceConversionEnabled(true);
	LogEvent(TEXT("converter_override"), FString::Printf(TEXT("%s %s -%d -> VP +%d every %.2f s"),
	                                                     *Converter->GetOwner()->GetName(),
	                                                     *EconSimPrivate::ResourceShortName(Input),
	                                                     Override.InputPerTick, Override.OutputPerTick,
	                                                     Override.TickSeconds));
}

// ---------------------------------------------------------------------------------------------------------------
// Measurement

void AEconSimDirector::OnActorSpawned(AActor* SpawnedActor)
{
	if (IsValid(SpawnedActor) && SpawnedActor != this)
	{
		M_NewActors.Add(SpawnedActor);
	}
}

void AEconSimDirector::ProcessNewActors()
{
	using namespace EconSimPrivate;
	for (int32 Index = M_NewActors.Num() - 1; Index >= 0; --Index)
	{
		AActor* const Actor = M_NewActors[Index].Get();
		if (not IsValid(Actor))
		{
			M_NewActors.RemoveAt(Index);
			continue;
		}
		if (not Actor->HasActorBegunPlay())
		{
			continue;
		}
		M_NewActors.RemoveAt(Index);
		if (M_SeenActors.Contains(Actor))
		{
			continue;
		}
		M_SeenActors.Add(Actor);
		const uint8 OwnerPlayer = GetOwningPlayer(Actor);
		if (OwnerPlayer == HumanPlayer)
		{
			RegisterPlayerActor(Actor);
		}
		else if (OwnerPlayer == EnemyPlayer && M_Phase == EEconSimPhase::Running)
		{
			RegisterEnemyUnit(Actor);
		}
	}
}

void AEconSimDirector::RegisterPlayerActor(AActor* Actor)
{
	UHealthComponent* const Health = Actor->FindComponentByClass<UHealthComponent>();
	if (not IsValid(Health))
	{
		return;
	}
	for (const FEconSimProtectedActor& Entry : M_ProtectedActors)
	{
		if (Entry.Actor.Get() == Actor)
		{
			return;
		}
	}
	FEconSimProtectedActor& Entry = M_ProtectedActors.AddDefaulted_GetRef();
	Entry.Actor = Actor;
	Entry.Health = Health;
	if (bM_ProtectPlayer)
	{
		Health->SetDamageReductionMultiplier(Health->GetDamageReductionMultiplier() * M_ProtectDamageMlt);
	}
}

void AEconSimDirector::RegisterEnemyUnit(AActor* Actor)
{
	const bool bIsTank = Actor->IsA<ATankMaster>();
	const bool bIsSquad = Actor->IsA<ASquadController>();
	if (not bIsTank && not bIsSquad)
	{
		return;
	}
	M_EnemyTanksSpawned += bIsTank ? 1 : 0;
	M_EnemySquadsSpawned += bIsSquad ? 1 : 0;
	bool bFromWave = false;
	for (const FVector& SpawnPoint : M_MissionSpawnPoints)
	{
		bFromWave |= FVector::Dist2D(SpawnPoint, Actor->GetActorLocation()) < EconSimPrivate::WaveSpawnPointRadius;
	}
	// UT_EnemyAI's enemy controller carries a test GA_SovietT34Drop that brings a T-34 trio along the map's south
	// edge every 12 s; no wave supply or wave timer holds it back. No Soviet building or base point is near that lane:
	// what appears there is the drop's (or a wave's), and it is removed so only the strategic AI trains.
	const bool bInMissionLane = Actor->GetActorLocation().Y < M_MissionLaneY;
	bFromWave |= bInMissionLane;
	if (not (bFromWave && bM_StarveMissionWaves))
	{
		M_EnemyUnits.Add(Actor);
	}
	AppendLine(TEXT("enemy_units.csv"), FString::Printf(TEXT("%.1f,%s,%s,%s,%.0f,%.0f"),
	                                                    GetGameSeconds() - M_SimStartGameSeconds,
	                                                    bFromWave ? TEXT("wave") : TEXT("trained"),
	                                                    *Actor->GetClass()->GetName(),
	                                                    *EconSimPrivate::DescribeUnit(Actor),
	                                                    Actor->GetActorLocation().X, Actor->GetActorLocation().Y));
	if (bFromWave && bM_StarveMissionWaves)
	{
		++M_MissionWaveUnitsRemoved;
		M_EnemyTanksSpawned -= bIsTank ? 1 : 0;
		M_EnemySquadsSpawned -= bIsSquad ? 1 : 0;
		Actor->Destroy();
	}
}

void AEconSimDirector::SampleHarvesters()
{
	for (FEconSimHarvester& Track : M_Harvesters)
	{
		SampleOneHarvester(Track);
	}
}

void AEconSimDirector::SampleOneHarvester(FEconSimHarvester& Track)
{
	UHarvester* const Harvester = Track.Harvester.Get();
	if (not IsValid(Harvester))
	{
		return;
	}
	int32 Cargo = 0;
	ERTSResourceType Resource = ERTSResourceType::Resource_None;
	const int32 Capacity = Harvester->GetMaxCapacityForTargetResource(Cargo, Resource);
	if (Resource == ERTSResourceType::Resource_None)
	{
		Resource = Track.LastResource;
	}
	Track.Capacity = FMath::Max(Track.Capacity, Capacity);
	if (Cargo < Track.LastCargo)
	{
		const int32 Delivered = Track.LastCargo - Cargo;
		const ERTSResourceType DeliveredType = Track.LastResource;
		Track.DeliveredRadixite += DeliveredType == ERTSResourceType::Resource_Radixite ? Delivered : 0;
		Track.DeliveredMetal += DeliveredType == ERTSResourceType::Resource_Metal ? Delivered : 0;
		Track.DeliveredOther += DeliveredType != ERTSResourceType::Resource_Radixite
		                        && DeliveredType != ERTSResourceType::Resource_Metal
			                        ? Delivered
			                        : 0;
		const double Now = GetGameSeconds();
		double Cycle = -1.0;
		if (Cargo == 0)
		{
			++Track.Drops;
			if (Track.LastEmptiedSeconds >= 0.0)
			{
				Cycle = Now - Track.LastEmptiedSeconds;
				Track.CycleSecondsSum += Cycle;
				++Track.Cycles;
			}
			Track.LastEmptiedSeconds = Now;
		}
		AppendLine(TEXT("harvest_drops.csv"), FString::Printf(TEXT("%.2f,%d,%s,%s,%d,%.2f"), Now - M_SimStartGameSeconds,
		                                                     Track.Index, *Track.Field.ToString(),
		                                                     *EconSimPrivate::ResourceShortName(DeliveredType), Delivered,
		                                                     Cycle));
	}
	Track.LastCargo = Cargo;
	Track.LastResource = Resource;
}

void AEconSimDirector::KeepHarvestersOnTheirFields()
{
	// The trainer's rally move and the harvester's own idle search can send it to another node: put it back.
	for (FEconSimHarvester& Track : M_Harvesters)
	{
		const UHarvester* const Harvester = Track.Harvester.Get();
		AActor* const Unit = Track.Unit.Get();
		if (not IsValid(Harvester) || not IsValid(Unit) || GetGameSeconds() - Track.SpawnSeconds < EconSimPrivate::RallyMoveSeconds)
		{
			continue;
		}
		const FEconSimField* const Field = M_Fields.FindByPredicate([&Track](const FEconSimField& Candidate)
		{
			return Candidate.Name == Track.Field;
		});
		// TargetResource is protected (Blueprint read-only); read it by reflection.
		const FWeakObjectProperty* const TargetProperty = FindFProperty<FWeakObjectProperty>(UHarvester::StaticClass(),
			TEXT("TargetResource"));
		const UObject* const TargetComponent = TargetProperty != nullptr
			                                       ? TargetProperty->GetObjectPropertyValue_InContainer(Harvester)
			                                       : nullptr;
		const UActorComponent* const TargetResource = Cast<UActorComponent>(TargetComponent);
		const AActor* const Target = IsValid(TargetResource) ? TargetResource->GetOwner() : nullptr;
		const bool bOnField = Field != nullptr && Target != nullptr
			&& Field->Nodes.ContainsByPredicate([Target](const TWeakObjectPtr<AActor>& Node)
			{
				return Node.Get() == Target;
			});
		if (bOnField || FindBestNode(Track.Field) == nullptr)
		{
			continue;
		}
		// Do not cut a loaded harvester's way to the drop off short.
		if (Track.LastCargo > 0 && Target == nullptr)
		{
			continue;
		}
		OrderHarvest(Unit, Track.Field);
		++M_HarvestReorders;
	}
}

void AEconSimDirector::LogHarvesterStates() const
{
	const FWeakObjectProperty* const TargetProperty = FindFProperty<FWeakObjectProperty>(UHarvester::StaticClass(),
		TEXT("TargetResource"));
	const double Now = GetGameSeconds() - M_SimStartGameSeconds;
	for (const FEconSimHarvester& Track : M_Harvesters)
	{
		const UHarvester* const Harvester = Track.Harvester.Get();
		const APawn* const Pawn = Cast<APawn>(Track.Unit.Get());
		if (not IsValid(Harvester) || not IsValid(Pawn))
		{
			continue;
		}
		const UObject* const TargetComponent = TargetProperty != nullptr
			                                       ? TargetProperty->GetObjectPropertyValue_InContainer(Harvester)
			                                       : nullptr;
		const UActorComponent* const TargetResource = Cast<UActorComponent>(TargetComponent);
		const AActor* const Target = IsValid(TargetResource) ? TargetResource->GetOwner() : nullptr;
		const AAIController* const AIController = Cast<AAIController>(Pawn->GetController());
		const FString MoveStatus = IsValid(AIController)
			                           ? UEnum::GetValueAsString(AIController->GetMoveStatus())
			                           : FString(TEXT("NoAIController"));
		const FVector Location = Pawn->GetActorLocation();
		AppendLine(TEXT("harvester_state.csv"), FString::Printf(TEXT("%.1f,%d,%s,%.0f,%.0f,%.0f,%d,%d,%s,%s"), Now,
		                                                       Track.Index, *Track.Field.ToString(), Location.X,
		                                                       Location.Y, Pawn->GetVelocity().Size(), Track.LastCargo,
		                                                       Track.Drops,
		                                                       IsValid(Target) ? *Target->GetName() : TEXT("None"),
		                                                       *MoveStatus));
	}
}

void AEconSimDirector::SilenceMissions() const
{
	// A mission Blueprint can spawn units straight through UMissionBase::AsyncSpawnActor, which no wave supply limits:
	// stop every timer, latent Delay and scheduler callback of the mission system so only the strategic AI trains.
	UWorld* const World = GetWorld();
	AMissionManager* MissionManager = nullptr;
	for (TActorIterator<AMissionManager> It(World); It; ++It)
	{
		MissionManager = *It;
		World->GetTimerManager().ClearAllTimersForObject(MissionManager);
		World->GetLatentActionManager().RemoveActionsForObject(MissionManager);
	}
	for (TObjectIterator<UMissionBase> It; It; ++It)
	{
		UMissionBase* const Mission = *It;
		if (not IsValid(Mission) || Mission->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			continue;
		}
		World->GetTimerManager().ClearAllTimersForObject(Mission);
		World->GetLatentActionManager().RemoveActionsForObject(Mission);
		if (IsValid(MissionManager))
		{
			MissionManager->CancelAllCallbacksForObject(Mission);
		}
	}
}

void AEconSimDirector::StarveMissionWaves() const
{
	if (not bM_StarveMissionWaves)
	{
		return;
	}
	AEnemyController* const Enemy = FRTS_Statics::GetEnemyController(this);
	if (not IsValid(Enemy))
	{
		return;
	}
	if (Enemy->GetEnemyWaveSupply() > 0)
	{
		Enemy->AddToWaveSupply(-Enemy->GetEnemyWaveSupply());
	}
	// The test mission tops the supply up in the same frame as its wave fires; the wave iterations run on the wave
	// controller's timers, so clearing those stops every mission wave. Strategic training lives on its own component.
	if (UEnemyWaveController* const WaveController = Enemy->FindComponentByClass<UEnemyWaveController>())
	{
		GetWorld()->GetTimerManager().ClearAllTimersForObject(WaveController);
	}
}

void AEconSimDirector::ProtectPlayerActors()
{
	if (not bM_ProtectPlayer)
	{
		return;
	}
	for (int32 Index = M_ProtectedActors.Num() - 1; Index >= 0; --Index)
	{
		UHealthComponent* const Health = M_ProtectedActors[Index].Health.Get();
		if (not IsValid(Health) || not M_ProtectedActors[Index].Actor.IsValid())
		{
			M_ProtectedActors.RemoveAt(Index);
			continue;
		}
		const float Missing = Health->GetMaxHealth() - Health->GetCurrentHealth();
		if (Missing <= 0.f || Health->GetCurrentHealth() <= 0.f)
		{
			continue;
		}
		M_DamageAbsorbed += Missing / FMath::Max(M_ProtectDamageMlt, KINDA_SMALL_NUMBER);
		Health->SetCurrentHealth(Health->GetMaxHealth());
	}
}

void AEconSimDirector::RunResourceSink()
{
	UPlayerResourceManager* const Resources = GetResources();
	if (not bM_SinkResources || not IsValid(Resources))
	{
		return;
	}
	// Vehicle parts are never spent by the build order: bank all of them, so their storage never clips conversion.
	const int32 VehicleParts = Resources->GetResourceAmount(ERTSResourceType::Resource_VehicleParts);
	if (VehicleParts > 0)
	{
		Resources->SubtractResource(ERTSResourceType::Resource_VehicleParts, VehicleParts);
		M_BankVehicleParts += VehicleParts;
	}
	if (M_BuildOrderDoneSeconds < 0.0)
	{
		return;
	}
	using namespace EconSimPrivate;
	const int32 Radixite = Resources->GetResourceAmount(ERTSResourceType::Resource_Radixite);
	const int32 RadixiteStorage = Resources->GetResourceStorage(ERTSResourceType::Resource_Radixite);
	if (Radixite > FMath::Max(SinkRadixiteFloor, FMath::RoundToInt(RadixiteStorage * SinkStorageFraction)))
	{
		Resources->SubtractResource(ERTSResourceType::Resource_Radixite, Radixite - SinkRadixiteFloor);
		M_BankRadixite += Radixite - SinkRadixiteFloor;
	}
	const int32 Metal = Resources->GetResourceAmount(ERTSResourceType::Resource_Metal);
	const int32 MetalStorage = Resources->GetResourceStorage(ERTSResourceType::Resource_Metal);
	if (Metal > FMath::Max(SinkMetalFloor, FMath::RoundToInt(MetalStorage * SinkStorageFraction)))
	{
		Resources->SubtractResource(ERTSResourceType::Resource_Metal, Metal - SinkMetalFloor);
		M_BankMetal += Metal - SinkMetalFloor;
	}
}

float AEconSimDirector::GetEnemyIncomeForMinute(const float Minute) const
{
	if (M_EnemyIncomePerMinute >= 0.f)
	{
		return M_EnemyIncomePerMinute;
	}
	// SurvivalWaveDefaults::BaseStrategicTrainingPointsPerMinute (Normal): the authored strategic AI income ramp.
	const FVector2D Keys[] = {{0.f, 4.f}, {10.f, 8.f}, {20.f, 12.f}, {40.f, 18.f}, {60.f, 24.f}};
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(Keys); ++Index)
	{
		if (Minute <= Keys[Index].X)
		{
			const float Alpha = (Minute - Keys[Index - 1].X) / (Keys[Index].X - Keys[Index - 1].X);
			return FMath::Lerp(Keys[Index - 1].Y, Keys[Index].Y, Alpha);
		}
	}
	return Keys[UE_ARRAY_COUNT(Keys) - 1].Y;
}

void AEconSimDirector::UpdateEnemyPressure()
{
	AEnemyController* const Enemy = FRTS_Statics::GetEnemyController(this);
	if (not IsValid(Enemy))
	{
		return;
	}
	UEnemyStrategicAIComponent* const Strategic = Enemy->GetEnemyStrategicAIComponent();
	if (not IsValid(Strategic))
	{
		return;
	}
	if (bM_FixEnemyBasePoints)
	{
		ProjectEnemyBasePointsToNavmesh(*Strategic);
	}
	if (FMath::IsNearlyZero(M_EnemyIncomePerMinute))
	{
		return;
	}
	const float Minute = static_cast<float>((GetGameSeconds() - M_SimStartGameSeconds) / 60.0);
	Strategic->GetEditableEnemyTrainingState().EnemyLevelTraining.TrainingPointsPerMinute =
		FMath::RoundToInt(GetEnemyIncomeForMinute(Minute));
}

void AEconSimDirector::ProjectEnemyBasePointsToNavmesh(UEnemyStrategicAIComponent& Strategic)
{
	// Training spawns at a base cluster's centre after projecting it onto the navmesh with a 2 cm extent
	// (StochasticHelpers::CanProjectNavigable_TrainingLocation); a centroid between buildings is almost never that close,
	// so the paid batch silently spawns nothing. Test-only: move each centre to the nearest navigable point.
	UNavigationSystemV1* const NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSystem == nullptr)
	{
		return;
	}
	for (FEnemyBasePointCoreBuildings& BasePoint : Strategic.GetEditableStrategicAIBlackboard().EnemyBasePoints)
	{
		FNavLocation Projected;
		if (NavSystem->ProjectPointToNavigation(BasePoint.BaseLocation, Projected, FVector(3000.f, 3000.f, 2000.f))
			&& not BasePoint.BaseLocation.Equals(Projected.Location, 1.f))
		{
			BasePoint.BaseLocation = Projected.Location;
			++M_BasePointsProjected;
		}
	}
}

int32 AEconSimDirector::CountEnabledConverters() const
{
	int32 Enabled = 0;
	for (const FEconSimProtectedActor& Entry : M_ProtectedActors)
	{
		const AActor* const Actor = Entry.Actor.Get();
		const UResourceConverterComponent* const Converter = IsValid(Actor)
			                                                     ? Actor->FindComponentByClass<UResourceConverterComponent>()
			                                                     : nullptr;
		bool bEnabled = false;
		if (EconSimPrivate::ReadBoolProperty(Converter, TEXT("bM_Enabled"), bEnabled) && bEnabled)
		{
			++Enabled;
		}
	}
	return Enabled;
}

void AEconSimDirector::WriteTimelineRow()
{
	UPlayerResourceManager* const Resources = GetResources();
	if (not IsValid(Resources))
	{
		return;
	}
	int32 HarvestedRadixite = 0;
	int32 HarvestedMetal = 0;
	for (const FEconSimHarvester& Track : M_Harvesters)
	{
		HarvestedRadixite += Track.DeliveredRadixite;
		HarvestedMetal += Track.DeliveredMetal;
	}
	int32 AliveTanks = 0;
	int32 AliveSquads = 0;
	for (int32 Index = M_EnemyUnits.Num() - 1; Index >= 0; --Index)
	{
		const AActor* const Unit = M_EnemyUnits[Index].Get();
		if (not IsValid(Unit))
		{
			M_EnemyUnits.RemoveAt(Index);
			continue;
		}
		AliveTanks += Unit->IsA<ATankMaster>() ? 1 : 0;
		AliveSquads += Unit->IsA<ASquadController>() ? 1 : 0;
	}
	int32 TrainingPoints = -1;
	int32 EnemyIncome = -1;
	int32 WaveSupply = -1;
	FString TechLevels = TEXT("-");
	if (AEnemyController* const Enemy = FRTS_Statics::GetEnemyController(this))
	{
		WaveSupply = Enemy->GetEnemyWaveSupply();
		if (UEnemyStrategicAIComponent* const Strategic = Enemy->GetEnemyStrategicAIComponent())
		{
			const FEnemyStrategicTrainingState& State = Strategic->GetEditableEnemyTrainingState();
			TrainingPoints = State.TrainingPoints;
			EnemyIncome = State.EnemyLevelTraining.TrainingPointsPerMinute;
			TechLevels = EconSimPrivate::TechLevelsAsText(State.TechLevelUnlockedMap);
		}
	}
	const double Seconds = M_SimStartGameSeconds >= 0.0 ? GetGameSeconds() - M_SimStartGameSeconds : 0.0;
	AppendLine(TEXT("timeline.csv"), FString::Printf(
		TEXT("%.1f,%.1f,%d,%d,%d,%d,%d,%d,%lld,%lld,%lld,%d,%d,%d,%d,%.0f,%d,%d,%d,%d,%d,%d,%s,%d,%d"),
		Seconds, FPlatformTime::Seconds() - M_StartRealSeconds,
		Resources->GetResourceAmount(ERTSResourceType::Resource_Radixite),
		Resources->GetResourceStorage(ERTSResourceType::Resource_Radixite),
		Resources->GetResourceAmount(ERTSResourceType::Resource_Metal),
		Resources->GetResourceStorage(ERTSResourceType::Resource_Metal),
		Resources->GetResourceAmount(ERTSResourceType::Resource_VehicleParts),
		Resources->GetResourceStorage(ERTSResourceType::Resource_VehicleParts),
		M_BankRadixite, M_BankMetal, M_BankVehicleParts, HarvestedRadixite, HarvestedMetal, M_Harvesters.Num(),
		CountEnabledConverters(), M_DamageAbsorbed, TrainingPoints, AliveTanks, AliveSquads, M_EnemyTanksSpawned,
		M_EnemySquadsSpawned, WaveSupply, *TechLevels, M_StepIndex, EnemyIncome));
}

void AEconSimDirector::LogEvent(const FString& Kind, const FString& Detail)
{
	const double Seconds = M_SimStartGameSeconds >= 0.0 ? GetGameSeconds() - M_SimStartGameSeconds : 0.0;
	UE_LOG(LogEconSim, Display, TEXT("ECONSIM %.1f %s %s"), Seconds, *Kind, *Detail);
	AppendLine(TEXT("events.csv"), FString::Printf(TEXT("%.1f,%s,\"%s\""), Seconds, *Kind, *Detail.Replace(TEXT("\""), TEXT("'"))));
}

// ---------------------------------------------------------------------------------------------------------------
// Output

void AEconSimDirector::OpenOutputFiles()
{
	IFileManager::Get().MakeDirectory(*M_OutputDirectory, true);
	AppendLine(TEXT("timeline.csv"), TEXT(
		"t,real_s,R,R_cap,M,M_cap,VP,VP_cap,bank_R,bank_M,bank_VP,harvested_R,harvested_M,harvesters,converters_on,"
		"damage_absorbed,enemy_points,enemy_tanks_alive,enemy_squads_alive,enemy_tanks_spawned,enemy_squads_spawned,"
		"enemy_wave_supply,enemy_tech,step,enemy_income"));
	AppendLine(TEXT("events.csv"), TEXT("t,kind,detail"));
	AppendLine(TEXT("enemy_units.csv"), TEXT("t,origin,class,unit_type,subtype,x,y"));
	AppendLine(TEXT("harvest_drops.csv"), TEXT("t,harvester,field,resource,amount,cycle_s"));
	AppendLine(TEXT("harvester_state.csv"), TEXT("t,harvester,field,x,y,speed,cargo,drops,target,move_status"));
}

void AEconSimDirector::AppendLine(const FString& FileName, const FString& Line) const
{
	FFileHelper::SaveStringToFile(Line + LINE_TERMINATOR, *(M_OutputDirectory / FileName),
	                              FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(),
	                              FILEWRITE_Append);
}

void AEconSimDirector::WriteContextFile() const
{
	FString Text = TEXT("# Economy sim context\n\n");
	const URTSGameInstance* const GameInstance = GetWorld()->GetGameInstance<URTSGameInstance>();
	if (IsValid(GameInstance))
	{
		Text += FString::Printf(TEXT("- faction: %s\n- difficulty: %s (%d%%)\n- vehicle balance: %s\n"),
		                        *UEnum::GetValueAsString(GameInstance->GetPlayerFaction()),
		                        *UEnum::GetValueAsString(GameInstance->GetSelectedGameDifficulty().DifficultyLevel),
		                        GameInstance->GetSelectedGameDifficulty().DifficultyPercentage,
		                        *UEnum::GetValueAsString(GameInstance->GetVehicleBalance()));
	}
	Text += FString::Printf(TEXT("- map: %s\n- base: %s\n- time dilation: %.2f\n- protect player: %d (damage x%.3f)\n"),
	                        *GetWorld()->GetMapName(), *M_Base.ToCompactString(), M_TimeDilation,
	                        bM_ProtectPlayer ? 1 : 0, M_ProtectDamageMlt);
	Text += FString::Printf(TEXT("- converter override: %s\n- smelter override: %s\n"),
	                        M_ConverterOverride.bActive
		                        ? *FString::Printf(TEXT("R -%d -> VP +%d every %.2f s"), M_ConverterOverride.InputPerTick,
		                                           M_ConverterOverride.OutputPerTick, M_ConverterOverride.TickSeconds)
		                        : TEXT("none (Blueprint values)"),
	                        M_SmelterOverride.bActive
		                        ? *FString::Printf(TEXT("M -%d -> VP +%d every %.2f s"), M_SmelterOverride.InputPerTick,
		                                           M_SmelterOverride.OutputPerTick, M_SmelterOverride.TickSeconds)
		                        : TEXT("none (Blueprint values)"));
	Text += FString::Printf(TEXT("- start grant: R %d, M %d, VP %d\n- resource sink: %d\n\n## Resource nodes\n\n"),
	                        M_StartResources.X, M_StartResources.Y, M_StartResources.Z, bM_SinkResources ? 1 : 0);
	Text += TEXT("| field | node | total | per harvest | distance to HQ (m) |\n|---|---|---|---|---|\n");
	for (const FEconSimField& Field : M_Fields)
	{
		for (const TWeakObjectPtr<AActor>& Node : Field.Nodes)
		{
			const UResourceComponent* const Resource = Node.IsValid() ? Node->FindComponentByClass<UResourceComponent>() : nullptr;
			if (not IsValid(Resource))
			{
				continue;
			}
			// AmountPerHarvest is protected (Blueprint data); read it by reflection.
			Text += FString::Printf(TEXT("| %s | %s | %d | %s | %.1f |\n"), *Field.Name.ToString(), *Node->GetClass()->GetName(),
			                        Resource->GetTotalAmount(),
			                        *EconSimPrivate::ExportProperty(Resource, TEXT("AmountPerHarvest")),
			                        FVector::Dist2D(Node->GetActorLocation(), M_Base) / 100.f);
		}
	}
	Text += TEXT("\n## Enemy\n\n") + DescribeEnemyTraining();
	FFileHelper::SaveStringToFile(Text, *(M_OutputDirectory / TEXT("context.md")));
}

FString AEconSimDirector::DescribeEnemyTraining() const
{
	AEnemyController* const Enemy = FRTS_Statics::GetEnemyController(this);
	if (not IsValid(Enemy))
	{
		return TEXT("no enemy controller\n");
	}
	FString Text = FString::Printf(TEXT("- controller: %s, wave supply %d\n"), *Enemy->GetName(), Enemy->GetEnemyWaveSupply());
	if (UEnemyStrategicAIComponent* const Strategic = Enemy->GetEnemyStrategicAIComponent())
	{
		const FEnemyStrategicTrainingState& State = Strategic->GetEditableEnemyTrainingState();
		Text += FString::Printf(TEXT("- training points now %d, start %d, per minute %d, tech %s\n"), State.TrainingPoints,
		                        State.EnemyLevelTraining.TrainingPointsStart,
		                        State.EnemyLevelTraining.TrainingPointsPerMinute,
		                        *EconSimPrivate::TechLevelsAsText(State.TechLevelUnlockedMap));
	}
	Text += TEXT("\nM_EnemyAIMissionSettings:\n\n```\n") + EconSimPrivate::ExportProperty(Enemy, TEXT("M_EnemyAIMissionSettings"))
		+ TEXT("\n```\n\nM_EnemyLevelTraining:\n\n```\n") + EconSimPrivate::ExportProperty(Enemy, TEXT("M_EnemyLevelTraining"))
		+ TEXT("\n```\n");
	return Text;
}

FString AEconSimDirector::DescribeConverters() const
{
	FString Text;
	for (const FEconSimProtectedActor& Entry : M_ProtectedActors)
	{
		const AActor* const Actor = Entry.Actor.Get();
		const UResourceConverterComponent* const Converter = IsValid(Actor)
			                                                     ? Actor->FindComponentByClass<UResourceConverterComponent>()
			                                                     : nullptr;
		if (not IsValid(Converter))
		{
			continue;
		}
		Text += FString::Printf(TEXT("- %s: enabled=%s\n\n```\n%s\n```\n"), *Actor->GetClass()->GetName(),
		                        *EconSimPrivate::ExportProperty(Converter, TEXT("bM_Enabled")),
		                        *EconSimPrivate::ExportProperty(Converter, TEXT("M_Settings")));
	}
	return Text.IsEmpty() ? TEXT("none\n") : Text;
}

void AEconSimDirector::WriteSummary() const
{
	const double Seconds = M_SimStartGameSeconds >= 0.0 ? GetGameSeconds() - M_SimStartGameSeconds : 0.0;
	FString Text = FString::Printf(TEXT("# Economy sim summary\n\n- simulated: %.1f game s in %.1f real s\n"), Seconds,
	                               FPlatformTime::Seconds() - M_StartRealSeconds);
	Text += FString::Printf(TEXT("- build order: %d/%d steps, done at %.1f s\n"), M_StepIndex, M_BuildOrder.Num(),
	                        M_BuildOrderDoneSeconds >= 0.0 ? M_BuildOrderDoneSeconds - M_SimStartGameSeconds : -1.0);
	Text += FString::Printf(TEXT("- bank: R %lld, M %lld, VP %lld\n- damage absorbed by protection: %.0f\n"),
	                        M_BankRadixite, M_BankMetal, M_BankVehicleParts, M_DamageAbsorbed);
	Text += FString::Printf(TEXT("- harvest re-orders: %d\n- mission wave units removed: %d\n"), M_HarvestReorders,
	                        M_MissionWaveUnitsRemoved);
	Text += FString::Printf(TEXT("- enemy base points moved onto the navmesh: %d\n"), M_BasePointsProjected);
	Text += FString::Printf(TEXT("- enemy: %d tanks + %d squads trained (preplaced %d + %d)\n\n"), M_EnemyTanksSpawned,
	                        M_EnemySquadsSpawned, M_EnemyTanksPreplaced, M_EnemySquadsPreplaced);
	Text += TEXT("## Harvesters\n\n| # | field | capacity | drops | avg cycle (s) | R | M | per minute since spawn |\n");
	Text += TEXT("|---|---|---|---|---|---|---|---|\n");
	for (const FEconSimHarvester& Track : M_Harvesters)
	{
		const double Alive = FMath::Max(1.0, GetGameSeconds() - Track.SpawnSeconds);
		Text += FString::Printf(TEXT("| %d | %s | %d | %d | %.1f | %d | %d | %.1f |\n"), Track.Index, *Track.Field.ToString(),
		                        Track.Capacity, Track.Drops, Track.Cycles > 0 ? Track.CycleSecondsSum / Track.Cycles : -1.0,
		                        Track.DeliveredRadixite, Track.DeliveredMetal,
		                        (Track.DeliveredRadixite + Track.DeliveredMetal) * 60.0 / Alive);
	}
	Text += TEXT("\n## Resource nodes left\n\n");
	for (const FEconSimField& Field : M_Fields)
	{
		for (const TWeakObjectPtr<AActor>& Node : Field.Nodes)
		{
			const UResourceComponent* const Resource = Node.IsValid() ? Node->FindComponentByClass<UResourceComponent>() : nullptr;
			Text += FString::Printf(TEXT("- %s %s: %d\n"), *Field.Name.ToString(),
			                        Node.IsValid() ? *Node->GetClass()->GetName() : TEXT("(gone)"),
			                        IsValid(Resource) ? Resource->GetTotalAmount() : 0);
		}
	}
	Text += TEXT("\n## Converters (live settings)\n\n") + DescribeConverters();
	Text += TEXT("\n## Enemy at the end\n\n") + DescribeEnemyTraining();
	FFileHelper::SaveStringToFile(Text, *(M_OutputDirectory / TEXT("summary.md")));
}

void AEconSimDirector::Finish(const FString& Reason)
{
	if (M_Phase == EEconSimPhase::Done)
	{
		return;
	}
	if (not M_OutputDirectory.IsEmpty() && M_SimStartGameSeconds >= 0.0)
	{
		WriteTimelineRow();
	}
	LogEvent(TEXT("finish"), Reason);
	M_Phase = EEconSimPhase::Done;
	WriteSummary();
	UE_LOG(LogEconSim, Display, TEXT("ECONSIM RESULT %s steps=%d/%d dir=%s"), *Reason, M_StepIndex, M_BuildOrder.Num(),
	       *M_OutputDirectory);
	FPlatformMisc::RequestExit(false, TEXT("EconSim finished"));
}

// ---------------------------------------------------------------------------------------------------------------
// Helpers

double AEconSimDirector::GetGameSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0;
}

FVector AEconSimDirector::BaseOffset(const FVector2D& Offset) const
{
	return M_Base + FVector(Offset.X, Offset.Y, 0.f);
}

FVector AEconSimDirector::GroundAt(const FVector& Location) const
{
	FHitResult Hit;
	const FVector Start(Location.X, Location.Y, Location.Z + EconSimPrivate::GroundTraceHalfHeight);
	const FVector End(Location.X, Location.Y, Location.Z - EconSimPrivate::GroundTraceHalfHeight);
	FCollisionQueryParams Params(TEXT("EconSimGround"), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return Hit.Location;
	}
	return Location;
}

FVector AEconSimDirector::NavPointNear(const FVector& Location) const
{
	UNavigationSystemV1* const NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Projected;
	if (NavSystem != nullptr && NavSystem->ProjectPointToNavigation(GroundAt(Location), Projected, FVector(600.f, 600.f, 2000.f)))
	{
		return Projected.Location;
	}
	return GroundAt(Location);
}

ACPPController* AEconSimDirector::GetController() const
{
	return M_PlayerController.Get();
}

UPlayerResourceManager* AEconSimDirector::GetResources() const
{
	const ACPPController* const Controller = GetController();
	return IsValid(Controller) ? Controller->GetPlayerResourceManager() : nullptr;
}

UTrainerComponent* AEconSimDirector::GetHqTrainer() const
{
	const ANomadicVehicle* const Hq = M_Hq.Get();
	return IsValid(Hq) ? Hq->FindComponentByClass<UTrainerComponent>() : nullptr;
}

ANomadicVehicle* AEconSimDirector::GetBuilding(const FName RoleName) const
{
	const TWeakObjectPtr<AActor>* const Found = M_Roles.Find(RoleName);
	return Found != nullptr ? Cast<ANomadicVehicle>(Found->Get()) : nullptr;
}
