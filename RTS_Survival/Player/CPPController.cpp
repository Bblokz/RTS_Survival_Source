// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CPPController.h"

#include "Abilities.h"
#include "AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "Camera/CameraPawn.h"
#include "Camera/CameraController/PlayerCameraController.h"
#include "CommandTypeDecoder/PlayerCommandTypeDecoder.h"
#include "ConstructionPreview/NomadicPreviewAttachments/FNomadicPreviewAttachments.h"
#include "HUD/CPPHUD.h"
#include "Kismet/GameplayStatics.h"
#include "PauseGame/PauseGameOptions.h"
#include "PlayerAudioController/PlayerAudioController.h"
#include "PlayerBuildRadiusManager/PlayerBuildRadiusManager.h"
#include "PlayerControlGroupManager/PlayerControlGroupManager.h"
#include "PlayerError/PlayerError.h"
#include "PlayerResourceManager/PlayerResourceManager.h"
#include "PlayerTechManager/PlayerTechManager.h"
#include "PortraitManager/PortraitManager.h"
#include "RTSCheatManager/RTSCheatManager.h"
#include "RTSPrimaryClickContext/RTSPrimaryClickContext.h"
#include "RTS_Survival/Audio/RTSVoiceLineHelpers/RTS_VoiceLineHelpers.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/CaptureMechanic/CaptureMechanicHelpers.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Game/GameUpdateComponent/RTSGameSettingsHandler.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/MouseHovering/UW_HoveringActor.h"
#include "RTS_Survival/LandscapeDeformSystem/LandscapeDeformManager/LandscapeDeformManager.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpCharacterObjectsMaster.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectableActorObjectsMaster.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/Procedural/LandscapeDivider/RTSLandscapeDivider.h"
#include "RTS_Survival/Resources/Resource.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/RTSComponents/RadiusComp/RadiusComp.h"
#include "RTS_Survival/RTSComponents/RepairComponent/RepairHelpers/RepairHelpers.h"
#include "RTS_Survival/StartLocation/PlayerStartLocationMngr/PlayerStartLocationManager.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/BuildRadiusComp/BuildRadiusComp.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/Navigator/RTSNavigator.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "SelectionHelpers/PlayerSelectionHelpers.h"
#include "StartGameProfileManager/PlayerProfileLoader.h"


class RTS_SURVIVAL_API AStaticPreviewMesh;
/**
 * 
 */
ACPPController::ACPPController()
	: CameraRef(nullptr)
	  , SpringarmRef(nullptr)
	  , CPPConstructionPreviewRef(nullptr)
	  , HUDRef(nullptr)
	  , CPPGameStateRef(nullptr)
	  , EmptyTransfrom(FTransform::Identity)
	  , ConstructionRotationRate(0.f)
	  , IsExtendingGameUI(false)
	  , DeltaSeconds(0.f)
	  , TimeHoldLeftMarquee(0.f)
	  , bIsHoldingShift(false)
	  , bIsHoldingControl(false)
	  , bM_IsInTechTree(false)
	  , bM_IsInArchive(false)
	  , M_PrimaryClickContext()
	  , bM_IsActionButtonActive(false)
	  , M_ActiveAbility(EAbilityID::IdNoAbility)
	  , M_NomadicVehicleSelectedForBuilding(nullptr)
	  , M_GameUIController(nullptr)
	  , M_FormationController(nullptr)
	  , M_IsBuildingPreviewModeActive(EPlayerBuildingPreviewMode::BuildingPreviewModeOFF)
	  , M_MainGameUI(nullptr)
	  , M_RTSNavigator(nullptr)
	  , M_RTSAsyncSpawner(nullptr)
	  , M_PlayerResourceManager(nullptr)
	  , M_PlayerAudioController(nullptr)
	  , M_PlayerTechManager(nullptr)
	  , M_PlayerProfileLoader(nullptr)
	  , M_PlayerCameraController(nullptr)
	  , M_CommandTypeDecoder(nullptr)
	  , M_PlayerBuildRadiusManager(nullptr)
	  , M_PlayerControlGroupManager(nullptr)
	  , M_PlayerStartLocationManager(nullptr)
	  , M_PlayerHQ(nullptr)
	  , M_FowManager(nullptr)
	  , M_HoveringActorWidgetClass(nullptr)
	  , M_HoveringActorWidget(nullptr)
	  , M_LastFrameMousePosition(FVector2D::ZeroVector)
	  , M_TimeWithoutMouseMovement(0.f)
	  , LastHoveredActor(nullptr)
	  , M_CurrentHoveredActor(nullptr)
	  , M_PlayerProfileLoadingStatus()
	  , M_RTSGameSettingsHandler(nullptr)
	  , M_PauseGameState()

{
	M_PrimaryClickContext = ERTSPrimaryClickContext::RegularPrimaryClick;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bShouldPerformFullTickWhenPaused = true;

	// Is actor component.
	M_PlayerResourceManager = CreateDefaultSubobject<UPlayerResourceManager>(TEXT("PlayerResourceManager"));
	M_PlayerCameraController = CreateDefaultSubobject<UPlayerCameraController>(TEXT("PlayerCameraController"));
	M_CommandTypeDecoder = CreateDefaultSubobject<UPlayerCommandTypeDecoder>(TEXT("PlayerCommandTypeDecoder"));
	M_PlayerBuildRadiusManager = CreateDefaultSubobject<UPlayerBuildRadiusManager>(TEXT("PlayerBuildRadiusManager"));
	M_PlayerControlGroupManager = CreateDefaultSubobject<UPlayerControlGroupManager>(TEXT("PlayerControlGroupManager"));

	M_PlayerTechManager = CreateDefaultSubobject<UPlayerTechManager>(TEXT("PlayerTechManager"));
	M_FormationController = CreateDefaultSubobject<UFormationController>(TEXT("FormationController"));


	M_PlayerProfileLoader = CreateDefaultSubobject<UPlayerProfileLoader>(TEXT("PlayerProfileLoader"));
	M_PlayerAudioController = CreateDefaultSubobject<UPlayerAudioController>(TEXT("PlayerAudioController"));
	M_PlayerPortraitManager = CreateDefaultSubobject<UPlayerPortraitManager>(TEXT("PlayerPortraitManager"));


	CheatClass = URTSCheatManager::StaticClass();

	TSelectedActorsMasters.Empty();
	TSelectedPawnMasters.Empty();
	TSelectedSquadControllers.Empty();

	M_PlayerStartLocationManager = CreateDefaultSubobject<UPlayerStartLocationManager>(
		TEXT("PlayerStartLocationManager"));
	M_PlayerStartLocationManager->SetPlayerController(this);
	OnMainMenuCallbacks.SetPlayerController(this);
} //Constructor

void ACPPController::InitPortrait(UW_Portrait* PortraitWidget) const
{
	if (not GetIsValidPlayerPortraitManager())
	{
		return;
	}
	M_PlayerPortraitManager->InitPortraitManager(PortraitWidget, GetPlayerAudioController());
}


void ACPPController::PauseGame(const ERTSPauseGameOptions PauseOption)
{
	if (PauseGame_GetIsGameLocked())
	{
		return;
	}
	const bool AttemptUnPause = PauseOption == ERTSPauseGameOptions::ForceUnpause || PauseOption ==
		ERTSPauseGameOptions::FlipFlop;
	if (bM_IsInTechTree && AttemptUnPause)
	{
		RTSFunctionLibrary::DisplayNotification(FText::FromString("in tech tree: cannot unpause!"));
		return;
	}
	if (bM_IsInArchive && AttemptUnPause)
	{
		RTSFunctionLibrary::DisplayNotification(FText::FromString("in archive: cannot unpause!"));
		return;
	}
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	const bool PauseByFlipFlop = PauseOption == ERTSPauseGameOptions::FlipFlop && not M_PauseGameState.bM_IsGamePaused;
	if (PauseOption == ERTSPauseGameOptions::FlipFlop && not M_PauseGameState.bM_IsGamePaused)
	{
		RTSFunctionLibrary::PrintString(" FlipFLop but not paused!!");
	}
	if (PauseByFlipFlop || PauseOption == ERTSPauseGameOptions::ForcePause)
	{
		if (M_PauseGameState.bM_IsGamePaused)
		{
			return;
		}
		World->bIsCameraMoveableWhenPaused = true;
		M_PauseGameState.bM_IsGamePaused = true;
		UGameplayStatics::SetGamePaused(World, true);
		return;
	}
	const bool UnPauseByFliFlop = PauseOption == ERTSPauseGameOptions::FlipFlop && M_PauseGameState.bM_IsGamePaused;
	if (UnPauseByFliFlop || PauseOption == ERTSPauseGameOptions::ForceUnpause)
	{
		if (not M_PauseGameState.bM_IsGamePaused)
		{
			return;
		}
		World->bIsCameraMoveableWhenPaused = false;
		M_PauseGameState.bM_IsGamePaused = false;
		UGameplayStatics::SetGamePaused(World, false);
		return;
	}
	RTSFunctionLibrary::ReportError("Invalid pause state!");
}

void ACPPController::SetGameUIVisibility(const bool bVisible)
{
	UWorld* const World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	int32 ToggledCount = 0;

	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* const Obj = *It;
		if (!IsValid(Obj)) { continue; }
		if (Obj->IsTemplate()) { continue; } // skip CDOs/archetypes
		if (Obj->GetWorld() != World) { continue; }
		if (!Obj->GetClass()->ImplementsInterface(URTSUIElement::StaticClass()))
		{
			continue;
		}

		// C++-only interface call
		if (IRTSUIElement* const UIElem = Cast<IRTSUIElement>(Obj))
		{
			UIElem->OnHideAllGameUI(not bVisible);
			++ToggledCount;
		}
	}
}

void ACPPController::PauseAndLockGame(const bool bLock)
{
	if (M_PauseGameState.bM_IsGameLocked && bLock)
	{
		RTSFunctionLibrary::ReportError("attempted to lock already locked game!"
			"\n See PauseAndLockGame in cpp controller");
		return;
	}
	if (not M_PauseGameState.bM_IsGameLocked && !bLock)
	{
		RTSFunctionLibrary::ReportError("attempted to unlock already unlocked game!"
			"\n See PauseAndLockGame in cpp controller");
		return;
	}
	ERTSPauseGameOptions PauseResult = bLock ? ERTSPauseGameOptions::ForcePause : ERTSPauseGameOptions::ForceUnpause;
	if (bLock)
	{
		// Pause first.
		PauseGame(PauseResult);
	}
	M_PauseGameState.bM_IsGameLocked = bLock;
	if (not bLock)
	{
		// Unpause after unlocking.
		PauseGame(PauseResult);
	}
	const bool bLockCamera = bLock;
	if (GetIsValidPlayerCameraController())
	{
		M_PlayerCameraController->SetCameraMovementDisabled(bLockCamera);
	}
}

UPlayerPortraitManager* ACPPController::GetPlayerPortraitManager() const
{
	return M_PlayerPortraitManager;
}

void ACPPController::TakeScreenShot(const bool bIncludeUI, const float InnerPercent /*inner fraction [0..1]*/)
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("TakeScreenShot: World is null."));
		return;
	}

	const bool bQueued = M_ViewportScreenshotTask.Request(World, bIncludeUI, InnerPercent);
	if (not bQueued)
	{
		UE_LOG(LogTemp, Warning, TEXT("TakeScreenShot: failed to queue screenshot request."));
	}
}

bool ACPPController::OnCinematicTakeOver(const bool bStartCinematic)
{
	UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	const bool bIsValidGameUnitManager = IsValid(GameUnitManager);
	if (not GetIsValidMainMenu() || not GetIsValidPlayerCameraController() || not bIsValidGameUnitManager)
	{
		RTSFunctionLibrary::ReportError("Cinematic take over failed!");
		return false;
	}
	const bool bMakeGameUIVisible = !bStartCinematic;
	const bool bLockCamera = bStartCinematic;
	// When we start a cinematic we ensure no optimization is happening, so we can show units outside the player FOV.
	const bool bOptimizeAllUnitsInGame = !bStartCinematic;
	GameUnitManager->SetAllUnitOptimizationEnabled(bOptimizeAllUnitsInGame);
	M_MainGameUI->SetMainMenuVisiblity(bMakeGameUIVisible);
	M_PlayerCameraController->SetCameraMovementDisabled(bLockCamera);
	return true;
}


void ACPPController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	DeltaSeconds = DeltaTime;

	// Get current mouse position in screen space.
	FVector2D MouseScreenPosition;
	GetMousePosition(MouseScreenPosition.X, MouseScreenPosition.Y);

	// Get the current projected mouse location on the landscape.
	FHitResult HitResult;
	const bool bHit = GetHitResultUnderCursor(ECC_Visibility, false, HitResult);

	// --------------------------------------------------------
	// ----- Tick player component updates
	// --------------------------------------------------------
	PlayerRotationArrow.TickArrowRotation(MouseScreenPosition, HitResult.Location);
	UpdateHoveringActorInfo(DeltaTime, MouseScreenPosition, HitResult, bHit);


	M_LastFrameMousePosition = MouseScreenPosition;
}

void ACPPController::SetupInputComponent()
{
	Super::SetupInputComponent();
}

void ACPPController::DisplayErrorMessage(const EPlayerError Error)
{
	switch (Error)
	{
	case EPlayerError::Error_None:
		break;
	case EPlayerError::Error_NotEnoughRadixite:
		DisplayErrorMessageCpp(FText::FromString("Not Enough Radixite"));
		break;
	case EPlayerError::Error_NotEnoughMetal:
		DisplayErrorMessageCpp(FText::FromString("Not Enough Metal"));
		break;
	case EPlayerError::Error_NotEnoughEnergy:
		DisplayErrorMessageCpp(FText::FromString("Not Enough Energy"));
		break;
	case EPlayerError::Error_NotEnoughVehicleParts:
		DisplayErrorMessageCpp(FText::FromString("Not Enough Vehicle Parts"));
		break;
	case EPlayerError::Error_NotEnoughtFuel:
		DisplayErrorMessageCpp(FText::FromString("Not Enough Fuel"));
		break;
	case EPlayerError::Error_NotENoughAmmo:
		DisplayErrorMessageCpp(FText::FromString("Not Enough Ammo"));
		break;
	case EPlayerError::Error_LocationNotReachable:
		DisplayErrorMessageCpp(FText::FromString("Location Not Reachable"));
		break;
	case EPlayerError::Error_NotEnoughWeaponBlueprints:
		break;
	case EPlayerError::Error_NotEnoughBuildingBlueprints:
		break;
	case EPlayerError::Error_NotEnoughEnergyBlueprints:
		break;
	case EPlayerError::Error_NotEnoughVehicleBlueprints:
		break;
	case EPlayerError::Error_NotEnoughConstructionBlueprints:
		break;
	}
}


void ACPPController::PlayVoiceLine(const AActor* Unit, const ERTSVoiceLine VoiceLine, const bool bForcePlay,
                                   const bool bQueueIfNotPlayed) const
{
	if (not GetIsValidPlayerAudioController())
	{
		return;
	}
	M_PlayerAudioController->PlayVoiceLine(Unit, VoiceLine, bForcePlay, bQueueIfNotPlayed);
}

void ACPPController::PlayAnnouncerVoiceLine(const EAnnouncerVoiceLineType VoiceLineType, const bool bQueueIfNotPlayed,
                                            const bool bInterruptRegularVoiceLines) const
{
	if (not GetIsValidPlayerAudioController())
	{
		return;
	}
	M_PlayerAudioController->PlayAnnouncerVoiceLine(VoiceLineType, bQueueIfNotPlayed, bInterruptRegularVoiceLines);
}

UAudioComponent* ACPPController::PlaySpatialVoiceLine(const AActor* PrimarySelectedUnit,
                                                      const ERTSVoiceLine VoiceLineType,
                                                      const FVector& Location, const bool bIgnorePlayerCooldown) const
{
	if (not GetIsValidPlayerAudioController())
	{
		return nullptr;
	}
	return M_PlayerAudioController->PlaySpatialVoiceLine(PrimarySelectedUnit, VoiceLineType, Location,
	                                                     bIgnorePlayerCooldown);
}

void ACPPController::MissionOnHqSpawned(const FUnitCost BonusPerResource)
{
	if (not GetIsValidPlayerResourceManager())
	{
		return;
	}
	TArray<TPair<ERTSResourceType, int32>> ExtraResourceBonuses;
	for (auto Each : BonusPerResource.ResourceCosts)
	{
		ExtraResourceBonuses.Add(TPair<ERTSResourceType, int32>(Each.Key, Each.Value));
	}
	M_PlayerResourceManager->AddCustomResourceBonuses(ExtraResourceBonuses);
	OnPlayerProfileLoadComplete();
}

void ACPPController::OnRTSGameSettingsLoaded(URTSGameSettingsHandler* RTSGameSettings)
{
	if (not IsValid(RTSGameSettings))
	{
		RTSFunctionLibrary::ReportError("called OnRtsGameSettingsLoaded with invalid RTSGameSettings!");
	}
	M_RTSGameSettingsHandler = RTSGameSettings;
	if (M_PlayerProfileLoadingStatus.bHasLoadedPlayerProfile)
	{
		M_PlayerResourceManager->InitializeResourcesSettings(RTSGameSettings);
	}
	else
	{
		// The player profile has not been loaded yet, make sure to Initialize the resources settings once it does.
		M_PlayerProfileLoadingStatus.bInitializeResourcesSettingsOnLoad = true;
	}
}

ALandscapedeformManager* ACPPController::GetLandscapeDeformManager()
{
	if (IsValid(M_LdfManager))
	{
		return M_LdfManager;
	}
	if (const auto Manager = GetLandscapeDeformManagerFromWorld())
	{
		M_LdfManager = Manager;
		return M_LdfManager;
	}
	if (DeveloperSettings::Debugging::GLandscapeDeformManager_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::ReportError(
			"Could not find LandscapeDeformManager in GetLandscapeDeformManager in CPPController.cpp");
	}
	return nullptr;
}

void ACPPController::SetCameraMovementDisabled(const bool bDisable) const
{
	if (not GetIsValidPlayerCameraController())
	{
		return;
	}
	M_PlayerCameraController->SetCameraMovementDisabled(bDisable);
}

void ACPPController::TabThroughActionUIHierarchy()
{
	if (GetIsValidGameUIController())
	{
		// Calls UpdateActionUI in the PlayerController blueprint if needed.
		M_GameUIController->TabThroughGameUIHierarchy(&TSelectedPawnMasters, &TSelectedSquadControllers,
		                                              &TSelectedActorsMasters);
	}
}


/*--------------------------------- MARQUEE ---------------------------------*/

void ACPPController::CPPMarqueeSelectionStart()
{
	if (M_PrimaryClickContext == ERTSPrimaryClickContext::SecondaryActive ||
		M_PrimaryClickContext == ERTSPrimaryClickContext::FormationTypePrimaryClick)
	{
		// User is holding secondary click.
		return;
	}
	if (HUDRef != nullptr)
	{
		HUDRef->StartSelection();
	}
} //MarqueeSelectionStart

void ACPPController::CPPMarqueeSelectionEnd()
{
	if (HUDRef != nullptr)
	{
		HUDRef->StopSelection();
	}
}

void ACPPController::SelectUnitsFromMarquee(
	const TArray<ASquadUnit*>& MarqueeSquadUnits,
	const TArray<ASelectableActorObjectsMaster*>& MarqueeSelectableActors,
	const TArray<ASelectablePawnMaster*>& MarqueeSelectablePawns)
{
	if (not GetIsValidGameUIController() || not GetIsValidCommandTypeDecoder())
	{
		return;
	}
	const bool bFullSelectionReset = !bIsHoldingShift;
	ESelectionChangeAction SelectionAction;

	if (bFullSelectionReset)
	{
		Internal_MarqueeFullSelectionReset_UpdateArraysAndDecals(
			MarqueeSquadUnits, MarqueeSelectableActors, MarqueeSelectablePawns);
		// Complete rebuild.
		SelectionAction = ESelectionChangeAction::UnitAdded_RebuildUIHierarchy;
	}
	else
	{
		if (Internal_IsMarqueeOnlyAlreadySelectedUnits(MarqueeSquadUnits, MarqueeSelectableActors,
		                                               MarqueeSelectablePawns))
		{
			SelectionAction = Internal_RemoveMarqueeFromSelection(MarqueeSquadUnits, MarqueeSelectableActors,
			                                                      MarqueeSelectablePawns);
		}
		else
		{
			SelectionAction = Internal_MarqueeAddValidUniqueUnitsToSelection_UpdateArraysAndDecals(
				MarqueeSquadUnits, MarqueeSelectableActors, MarqueeSelectablePawns);
		}
	}
	UpdateUIForSelectionAction(SelectionAction, nullptr);
}

void ACPPController::ResetSelectionArrays()
{
	EnsureSelectionsAreRTSValid();
	for (const auto EachSquad : TSelectedSquadControllers)
	{
		if (not IsValid(EachSquad))
		{
			continue;
		}
		EachSquad->SetSquadSelected(false);
	}
	TSelectedSquadControllers.Empty();
	for (const auto EachSelectableActor : TSelectedActorsMasters)
	{
		if (not IsValid(EachSelectableActor))
		{
			continue;
		}
		EachSelectableActor->SetUnitSelected(false);
	}
	TSelectedActorsMasters.Empty();
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		if (not IsValid(EachPawn))
		{
			continue;
		}
		EachPawn->SetUnitSelected(false);
	}
	TSelectedPawnMasters.Empty();
}


/*--------------------------------- LEFT CLICK COMMANDS ---------------------------------*/

FVector ACPPController::GetCursorWorldPosition(
	const float SightDistance,
	bool& bOutIsValidCalculation) const
{
	FVector WorldLocation, WorldDirection;
	FHitResult HitResult;
	DeprojectMousePositionToWorld(WorldLocation, WorldDirection);
	// End point of ray calculation.
	WorldDirection *= SightDistance;
	WorldDirection += WorldLocation;
	if (const UWorld* World = GetWorld())
	{
		World->LineTraceSingleByChannel(HitResult, WorldLocation, WorldDirection, ECC_GameTraceChannel2);
		if (HitResult.GetActor() != nullptr)
		{
			bOutIsValidCalculation = true;
			return HitResult.Location;
		}
	}
	bOutIsValidCalculation = false;
	return FVector::Zero();
}

void ACPPController::SetIsPlayerInTechTree(const bool bIsInTechTree)
{
	if (IsValid(M_PlayerCameraController))
	{
		M_PlayerCameraController->SetIsPlayerInTechTreeOrArchive(bIsInTechTree);
	}
	// Important; needs to be set before changing pause so the pause function can check techtree-status.
	this->bM_IsInTechTree = bIsInTechTree;
	if (bIsInTechTree)
	{
		if (not M_PauseGameState.bM_IsGamePaused)
		{
			PauseGame(ERTSPauseGameOptions::ForcePause);
		}
	}
	// game can never be unpaused from tech tree so we don't need to check for that.
	else
	{
		PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

void ACPPController::SetIsPlayerInArchive(const bool bIsInArchive)
{
	if (IsValid(M_PlayerCameraController))
	{
		M_PlayerCameraController->SetIsPlayerInTechTreeOrArchive(bIsInArchive);
	}

	// Important; needs to be set before changing pause so the pause function can check archive-status.
	this->bM_IsInArchive = bIsInArchive;
	if (bIsInArchive)
	{
		if (not M_PauseGameState.bM_IsGamePaused)
		{
			PauseGame(ERTSPauseGameOptions::ForcePause);
		}
	}
	// game can never be unpaused from tech tree so we don't need to check for that.
	else
	{
		PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}


void ACPPController::CancelBuilding(AActor* RequestingActor)
{
	if (RequestingActor && RequestingActor->IsA(ANomadicVehicle::StaticClass()))
	{
		if (ANomadicVehicle* NomadicVehicle = Cast<ANomadicVehicle>(RequestingActor))
		{
			NomadicVehicle->SetUnitToIdle();
			if (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
			{
				RTSFunctionLibrary::PrintString("CancelBuilding: " + NomadicVehicle->GetName());
			}
			if (GetIsPreviewBuildingActive())
			{
				StopBuildingPreviewMode();
			}
		}
		else
		{
			RTSFunctionLibrary::PrintString("CancelBuilding: actor is not of nomadic base class!.");
		}
	}
	else
	{
		RTSFunctionLibrary::PrintString("CancelBuilding: No valid actor provided for canceling building.");
	}
}

// Called from W_BottomCenterUI upon clicking the convert to building button.
void ACPPController::ConstructBuilding(AActor* RequestingActor)
{
	if (RequestingActor && RequestingActor->IsA(ANomadicVehicle::StaticClass()))
	{
		if (ANomadicVehicle* NomadicVehicle = Cast<ANomadicVehicle>(RequestingActor))
		{
			NomadicVehicle->SetUnitToIdle();
			if (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
			{
				RTSFunctionLibrary::PrintString("ConstructBuilding: " + NomadicVehicle->GetName());
			}
			M_NomadicVehicleSelectedForBuilding = NomadicVehicle;
			// If it is NOT the HQ converting to building THEN we need a build radius.
			const bool bUseBuildRadius = NomadicVehicle != M_PlayerHQ;
			StartNomadicBuildingPreview(NomadicVehicle->GetPreviewMesh(),
			                            EPlayerBuildingPreviewMode::NomadicPreviewMode,
			                            bUseBuildRadius,
			                            GetNomadicPreviewAttachments(NomadicVehicle));
			M_ActiveAbility = EAbilityID::IdCreateBuilding;
			// Note that we do not call the MainGameUI to show the cancel button as this is already
			// completed by MainGameUI itself since this function is called from the MainGameUI.
		}
	}
}

void ACPPController::ConvertBackToVehicle(AActor* RequestingActor)
{
	if (RequestingActor)
	{
		if (ANomadicVehicle* NomadicVehicle = Cast<ANomadicVehicle>(RequestingActor))
		{
			NomadicVehicle->ConvertToVehicle(true);
		}
	}
}

void ACPPController::CancelVehicleConversion(AActor* RequestingActor)
{
	if (RequestingActor)
	{
		if (ANomadicVehicle* NomadicVehicle = Cast<ANomadicVehicle>(RequestingActor))
		{
			NomadicVehicle->SetUnitToIdle();
		}
	}
}

void ACPPController::TruckConverted(
	ANomadicVehicle* ConvertedTruck,
	const bool bConvertedToBuilding) const
{
	if (IsValid(M_MainGameUI))
	{
		M_MainGameUI->OnTruckConverted(ConvertedTruck, bConvertedToBuilding);
	}
}


void ACPPController::ExpandBuildingWithBxpAndStartPreview(
	const FBxpOptionData& BuildingExpansionTypeData,
	TScriptInterface<IBuildingExpansionOwner> BuildingExpansionOwner,
	const int ExpansionSlotIndex,
	const bool bIsUnpacked_Preview_Expansion)
{
	if (!GetIsValidAsyncSpawner() || not BuildingExpansionOwner)
	{
		return;
	}
	// New building expansions always use the preview for placement first.
	// Note that there is also a specific enum value for unpacking Bxps that have instant placement.
	// Like origin and socket placements for those see RebuildPackedExpansion.
	// An origin or socket placement loaded for the first time will always use the preview.
	const EAsyncBxpLoadingType BxpLoadingType = bIsUnpacked_Preview_Expansion
		                                            ? EAsyncBxpLoadingType::AsyncLoadingPreviewPackedBxp
		                                            : EAsyncBxpLoadingType::AsyncLoadingNewBxp;
	// Do we need to reset any previous async request?
	if (M_AsyncBxpRequestState.M_Status != EAsyncBxpStatus::Async_NoRequest)
	{
		// Will stop the previous load request if the previously requested bxp was not loaded yet.
		// Also resets the previous request and building mode.
		StopBxpPreviewPlacementResetAsyncRequest(M_AsyncBxpRequestState.M_BuildingExpansionOwner,
		                                         M_AsyncBxpRequestState.M_BxpLoadingType);
		RTSFunctionLibrary::ReportError(
			"Attempted to expand building while another expansion is in progress. Cancelling previous request.");
	}
	if (UStaticMesh* PreviewMesh = M_RTSAsyncSpawner->SyncGetBuildingExpansionPreviewMesh(
		BuildingExpansionTypeData.ExpansionType))
	{
		// Save the request.
		M_AsyncBxpRequestState.InitSuccessfulRequest(EAsyncBxpStatus::Async_SpawnedPreview_WaitForBuildingMesh,
		                                             ExpansionSlotIndex,
		                                             BuildingExpansionOwner,
		                                             BxpLoadingType);

		// Calls back to OnBxpSpawnedAsync when the loading is complete.
		M_RTSAsyncSpawner->AsyncSpawnBuildingExpansion(BuildingExpansionTypeData, BuildingExpansionOwner,
		                                               ExpansionSlotIndex,
		                                               BxpLoadingType);

		UStaticMeshComponent* AttachToMesh = BuildingExpansionOwner->GetAttachToMeshComponent();
		FBxpConstructionData BxpConstructionData;
		// Performs a validity check on the provided construction rules for this bxp.
		BxpConstructionData.Init(
			BuildingExpansionTypeData.BxpConstructionRules,
			AttachToMesh,
			BuildingExpansionOwner->GetBxpExpansionRange(),
			BuildingExpansionOwner,
			ExpansionSlotIndex);;

		const FVector HostLocation = BuildingExpansionOwner->GetBxpOwnerLocation();

		// Only use build radius if we want to perform a free bxp placement.
		const bool bUseBuildRadius =
			BuildingExpansionTypeData.BxpConstructionRules.ConstructionType == EBxpConstructionType::Free;
		StartBxpBuildingPreview(
			PreviewMesh,
			bUseBuildRadius,
			HostLocation,
			BxpConstructionData);
	}
	else
	{
		const FString BxpName = Global_GetBxpTypeEnumAsString(BuildingExpansionTypeData.ExpansionType);
		RTSFunctionLibrary::ReportError(
			"Could not load preview mesh for building expansion type: " + BxpName
			+ "\n See function ExpandBuildingWithType in CPPController.cpp"
			"\n Forgot to add mapping in AsyncRTSAssetsSpawner?");
	}
}

void ACPPController::OnClickedCancelBxpPlacement(
	const TScriptInterface<IBuildingExpansionOwner>& BuildingExpansionOwner,
	const bool bIsPackedExpansion,
	const EBuildingExpansionType BxpType)
{
	if (bIsPackedExpansion && M_AsyncBxpRequestState.M_BxpLoadingType == EAsyncBxpLoadingType::AsyncLoadingNewBxp)
	{
		const FString Expected = Global_GetAsyncBxpLoadingTypeEnumAsString(
				EAsyncBxpLoadingType::AsyncLoadingPreviewPackedBxp) +
			"Or" + Global_GetAsyncBxpLoadingTypeEnumAsString(
				EAsyncBxpLoadingType::AsyncLoadingPackedBxpInstantPlacement);
		RTSFunctionLibrary::ReportError("Clicked to cancel a packed bxp but the async loading state is set to Loading"
			"new Bxp!!"
			"\n Expected : " + Expected);
	}
	StopBxpPreviewPlacementResetAsyncRequest(BuildingExpansionOwner, M_AsyncBxpRequestState.M_BxpLoadingType);
}

void ACPPController::RebuildPackedExpansion(const FBxpOptionData& BuildingExpansionTypeData,
                                            const TScriptInterface<IBuildingExpansionOwner>& BuildingExpansionOwner,
                                            const int ExpansionSlotIndex)
{
	if (!GetIsValidAsyncSpawner() || not BuildingExpansionOwner)
	{
		return;
	}
	const bool bIsInstantPlacePackedExpansion =
		BuildingExpansionTypeData.BxpConstructionRules.ConstructionType == EBxpConstructionType::Socket
		|| BuildingExpansionTypeData.BxpConstructionRules.ConstructionType == EBxpConstructionType::AtBuildingOrigin;
	if (not bIsInstantPlacePackedExpansion)
	{
		// Start a packed expansion placement preview.
		ExpandBuildingWithBxpAndStartPreview(BuildingExpansionTypeData, BuildingExpansionOwner,
		                                     ExpansionSlotIndex, true);
		return;
	}

	// Do we need to reset any previous async request?
	if (M_AsyncBxpRequestState.M_Status != EAsyncBxpStatus::Async_NoRequest)
	{
		// Will stop the previous load request if the previously requested bxp was not loaded yet.
		// Also resets the previous request and building mode.
		StopBxpPreviewPlacementResetAsyncRequest(M_AsyncBxpRequestState.M_BuildingExpansionOwner,
		                                         M_AsyncBxpRequestState.M_BxpLoadingType);
		RTSFunctionLibrary::ReportError(
			"Attempted to rebuild packed expansion while previous async request was active which is now cancelled.");
	}
	constexpr EAsyncBxpLoadingType BxpLoadingType = EAsyncBxpLoadingType::AsyncLoadingPackedBxpInstantPlacement;
	// Save the request.
	M_AsyncBxpRequestState.InitSuccessfulRequest(EAsyncBxpStatus::Async_SpawnedPreview_WaitForBuildingMesh,
	                                             ExpansionSlotIndex,
	                                             BuildingExpansionOwner,
	                                             BxpLoadingType);

	// Calls back to OnBxpSpawnedAsync when the loading is complete.
	M_RTSAsyncSpawner->AsyncSpawnBuildingExpansion(BuildingExpansionTypeData, BuildingExpansionOwner,
	                                               ExpansionSlotIndex,
	                                               BxpLoadingType);
	if (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::DisplayNotification(
			FText::FromString("start async loading of PackedBxp instant placement!"));
	}
}

void ACPPController::OnBxpSpawnedAsync(
	ABuildingExpansion* SpawnedBxp,
	TScriptInterface<IBuildingExpansionOwner> BxpOwner,
	const FBxpOptionData& BxpConstructionRulesAndType,
	const int ExpansionSlotIndex,
	const EAsyncBxpLoadingType BxpLoadingType)
{
	if (not SpawnedBxp)
	{
		RTSFunctionLibrary::ReportError("Something went wrong when async spawning the bxp"
			"\n Building mode and preview mode will be cancelled.");

		StopBxpPreviewPlacementResetAsyncRequest(BxpOwner, BxpLoadingType);
		return;
	}
	if (BxpOwner)
	{
		if (M_AsyncBxpRequestState.GetIsInstantPlacementRequest())
		{
			OnBxpSpawnedAsync_CheckInstantPlacement_ResetAsyncRequest(SpawnedBxp, BxpOwner,
			                                                          BxpConstructionRulesAndType,
			                                                          ExpansionSlotIndex);
			return;
		}
		const bool bIsPackedExpansion = M_AsyncBxpRequestState.GetIsPackedExpansionRequest();
		BxpOwner->OnBuildingExpansionCreated(SpawnedBxp, ExpansionSlotIndex, BxpConstructionRulesAndType,
		                                     bIsPackedExpansion, BxpOwner);
		// Loading is complete but request still there as we need to wait till the building is placed.
		M_AsyncBxpRequestState.M_Status = EAsyncBxpStatus::Async_BxpIsSpawned;
		M_AsyncBxpRequestState.M_Expansion = SpawnedBxp;
		if (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
		{
			const FString BxpLoadingTypeString = Global_GetAsyncBxpLoadingTypeEnumAsString(BxpLoadingType);
		}
		return;
	}
	RTSFunctionLibrary::ReportError("Bxp owner is null!"
		"\n At function ACPPController::ONBxpSpawnedAsync in CPPController.cpp"
		"\n SpawnedBxp : " + SpawnedBxp->GetName() + " will be destroyed.");
	SpawnedBxp->Destroy();
	StopBxpPreviewPlacementResetAsyncRequest(BxpOwner, BxpLoadingType);
}

void ACPPController::OnBxpSpawnedAsync_CheckInstantPlacement_ResetAsyncRequest(
	ABuildingExpansion* SpawnedBxp, const TScriptInterface<IBuildingExpansionOwner>& BxpOwner,
	const FBxpOptionData& BxpConstructionRulesAndType, const int ExpansionSlotIndex)
{
	const bool bSuccess = OnBxpSpawnedAsync_InstantPlacement(
		SpawnedBxp, BxpOwner, BxpConstructionRulesAndType, ExpansionSlotIndex);
	if (not bSuccess)
	{
		RTSFunctionLibrary::ReportError("Failed instant place bxp after loading: will destroy the bxp!"
			"Not resetting any state on the Bxp Owner as the entry was never initialized.");
		if (SpawnedBxp)
		{
			SpawnedBxp->Destroy();
		}
	}
	// Reset the async request state.
	M_AsyncBxpRequestState.Reset();
}


bool ACPPController::OnBxpSpawnedAsync_InstantPlacement(
	ABuildingExpansion* SpawnedBxp,
	const TScriptInterface<IBuildingExpansionOwner>& BxpOwner,
	const FBxpOptionData& BxpConstructionRulesAndType,
	const int ExpansionSlotIndex) const
{
	if (not BxpOwner || not IsValid(BxpOwner->GetAttachToMeshComponent()) || not IsValid(SpawnedBxp))
	{
		RTSFunctionLibrary::ReportError("Attempted to instant place a packed bxp after loading (socket or origin) but"
			"the bxp, owner or attach to mesh was invalid!"
			"\n See OnBxpSpawnedAsync_InstantPlacement in CPPController.cpp");
		return false;
	}
	const EBxpConstructionType ConstructionType = BxpConstructionRulesAndType.BxpConstructionRules.ConstructionType;
	if (ConstructionType == EBxpConstructionType::Free || ConstructionType == EBxpConstructionType::None)
	{
		const FString BxpConstString = Global_GetBxpConstructionTypeEnumAsString(ConstructionType);
		RTSFunctionLibrary::ReportError(
			"Attempted to instant place packed bxp after async loading but the bxp construction type"
			"is not correct!"
			"\n Expected Socket or AtBuildingOrigin but got : " + BxpConstString +
			"\n See OnBxpSpawnedAsync_InstantPlacement in CPPController.cpp");
		return false;
	}
	FRotator BxpRotation = FRotator::ZeroRotator;
	FVector BxpLocation = FVector::ZeroVector;
	const FName SocketName = BxpConstructionRulesAndType.BxpConstructionRules.SocketName;
	if (BxpConstructionRulesAndType.BxpConstructionRules.ConstructionType == EBxpConstructionType::Socket)
	{
		if (SocketName == NAME_None)
		{
			RTSFunctionLibrary::ReportError("After loading an instant place bxp of type Socket attempted to be placed"
				"but the socket was not valid! Did this date not get propagated correctly from the BXP item?"
				"\n See OnBxpSpawnedAsync_InstantPlacement in CPPController.cpp");
			return false;
		}
		BxpLocation = BxpOwner->GetAttachToMeshComponent()->GetSocketLocation(SocketName);
		BxpRotation = BxpOwner->GetAttachToMeshComponent()->GetSocketRotation(SocketName);
		if (BxpLocation.IsNearlyZero())
		{
			RTSFunctionLibrary::ReportError(
				"The obtained socket location on the bxp owner's attach to mesh was nearly zero"
				"\n While the instant placement bxp was loaded with a correct socket name! Incorrect socket saved?"
				"\n See OnBxpSpawnedAsync_InstantPlacement in CPPController.cpp");
			return false;
		}
	}
	else
	{
		BxpRotation = FRotator::ZeroRotator;
		BxpLocation = BxpOwner->GetAttachToMeshComponent()->GetComponentLocation();
		if (BxpLocation.IsNearlyZero())
		{
			RTSFunctionLibrary::ReportError(
				"The obtained origin location on the bxp owner's attach to mesh was nearly zero"
				"\n See OnBxpSpawnedAsync_InstantPlacement");
			return false;
		}
	}

	// First init the entry.
	BxpOwner->OnBuildingExpansionCreated(SpawnedBxp, ExpansionSlotIndex, BxpConstructionRulesAndType,
	                                     true, BxpOwner);
	// Start the construction instantly.
	SpawnedBxp->StartExpansionConstructionAtLocation(BxpLocation, BxpRotation,
	                                                 SocketName);
	return true;
}


void ACPPController::InitPlayerController(
	UMainGameUI* NewMainGameUI,
	ARTSAsyncSpawner* NewRTSAsyncSpawner,
	const FVector& SpawnCenter, const TMap<ETechnology,
	                                       TSoftClassPtr<UTechnologyEffect>> Technologies,
	TSubclassOf<UW_HoveringActor> HoverWidgetClass,
	const bool bDoNotLoadPlayerProfileUnits,
	const EMiniMapStartDirection StartDirection)
{
	M_MainGameUI = NewMainGameUI;
	OnMainMenuCallbacks.OnMainMenuUIReady.Broadcast();
	// Needs the fow manager which is set in beginplay before the blueprint beginplay calls this init function.
	InitMiniMapWithFowDataOnMainMenu(StartDirection);
	M_TechnologyEffectsMap = Technologies;
	if (GetIsValidPlayerTechManager())
	{
		M_PlayerTechManager->InitTechsInManager(this);
	}
	if (NewRTSAsyncSpawner)
	{
		M_RTSAsyncSpawner = NewRTSAsyncSpawner;
		InitPlayerStartLocation(SpawnCenter, bDoNotLoadPlayerProfileUnits);
	}
	else
	{
		RTSFunctionLibrary::ReportError("Async spawner is null! see InitPlayerController in CPPController.cpp");
	}
	if (IsValid(HoverWidgetClass))
	{
		M_HoveringActorWidgetClass = HoverWidgetClass;
		M_HoveringActorWidget = CreateWidget<UW_HoveringActor>(this, M_HoveringActorWidgetClass);
		if (GetIsValidHoverWidget())
		{
			M_HoveringActorWidget->AddToViewport();
			M_HoveringActorWidget->HideWidget();
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"No valid hoverwidget class provided in InitPlayerController in CPPController.cpp");
	}
}

void ACPPController::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_SetupRotationArrow();
}

void ACPPController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (M_FormationController)
	{
		const TSharedPtr<FPlayerFormationPositionEffects> FormationEffects = MakeShared<
			FPlayerFormationPositionEffects>(PlayerFormationEffects);
		M_FormationController->InitFormationController(FormationEffects);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.bNoFail = true;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FTransform DefaultTransform = FTransform::Identity;

	M_RTSNavigator = Cast<ARTSNavigator>(
		GetWorld()->SpawnActor(ARTSNavigator::StaticClass(), &DefaultTransform, SpawnParams));

	if (M_PlayerCameraController)
	{
		M_PlayerCameraController->SetAutoActivate(true);
		M_PlayerCameraController->Activate();
	}

	// create aactionuicontroller on map using spawn actor.
	M_GameUIController = GetWorld()->SpawnActor<AGameUIController>(AGameUIController::StaticClass());
	if (IsValid(M_GameUIController))
	{
		M_GameUIController->SetPlayerController(this);
	}
	else
	{
		RTSFunctionLibrary::ReportError("Cannot set playercontroller on ActionUI");
	}
	// Attempt to find the Fow Manager on the map.
	InitFowManager();
}

void ACPPController::BeginDestroy()
{
	Super::BeginDestroy();
}

void ACPPController::UseControlGroup(const int32 GroupIndex)
{
	if (not GetIsValidPlayerControlGroupManager())
	{
		return;
	}
	AActor* NewActorTypeAddedByGroup = nullptr;
	const ESelectionChangeAction Action = M_PlayerControlGroupManager->UseControlGroup(
		GroupIndex, bIsHoldingShift, bIsHoldingControl, NewActorTypeAddedByGroup);
	UpdateUIForSelectionAction(Action, NewActorTypeAddedByGroup);
}

void ACPPController::MoveCameraToControlGroup(const int32 GroupIndex)
{
	if (not GetIsValidPlayerControlGroupManager())
	{
		return;
	}
	bool bIsValidLocation;
	const FVector GroupLocation = M_PlayerControlGroupManager->GetControlGroupLocation(GroupIndex, bIsValidLocation);
	if (bIsValidLocation)
	{
		FocusCameraOnLocation(GroupLocation);
	}
}

void ACPPController::OnActionButtonShortCut(const int32 Index)
{
	if (not GetIsValidGameUIController())
	{
		return;
	}
	ActivateActionButton(Index);
}

void ACPPController::SetModifierCameraMovementSpeed(const float NewSpeed)
{
	DeveloperSettings::UIUX::ModifierCameraMovementSpeed = NewSpeed;
}

float ACPPController::GetCameraPanSpeed()
{
	return DeveloperSettings::UIUX::CameraPanSpeed;
}

float ACPPController::GetCameraPitchLimit()
{
	return DeveloperSettings::UIUX::CameraPitchLimit;
}

void ACPPController::SecondaryClickStart()
{
	// This captures the primary click for formation widget creation.
	M_PrimaryClickContext = ERTSPrimaryClickContext::SecondaryActive;
	// Get current mouse position in screen space.
	FVector2D MouseScreenPosition;
	GetMousePosition(MouseScreenPosition.X, MouseScreenPosition.Y);

	// Get the current projected mouse location on the landscape.
	FHitResult HitResult;
	if (GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
	{
		M_SecondaryStartMouseProjectedLocation = HitResult.Location;
		M_SecondaryStartClickedActor = HitResult.GetActor();
		PlayerRotationArrow.InitRotationArrowAction(MouseScreenPosition, HitResult.Location);
	}
	else
	{
		M_SecondaryStartMouseProjectedLocation = FVector::Zero();
		M_SecondaryStartClickedActor = nullptr;
	}
}

void ACPPController::SecondaryClick()
{
	// Sets primary click back to regular if no formation widget was started.
	OnSecondaryClickFinished_CheckPrimaryClickContext();
	EnsureSelectionsAreRTSValid();
	PlayerRotationArrow.StopRotationArrow();
	if (GetIsPreviewBuildingActive())
	{
		// as building mode was active there still wasn't a valid building location provided for the vehicle.
		// there is also no preview set on the vehicle so we only need to terminate the construction preview model.
		StopBuildingPreviewMode();
		return;
	}
	DeactivateActionButton();
	bool bHitGameObj = false;
	M_InitMousePosGameLeftClick = GetCursorWorldPosition(DeveloperSettings::UIUX::SightDistanceMouse, bHitGameObj);
	FVector MouseProjectedLocation;
	AActor* ClickedActor;
	const bool bHit = GetSecondaryClickHitActorAndLocation(ClickedActor, MouseProjectedLocation);
	ESelectionChangeAction SelectionAction = ESelectionChangeAction::SelectionInvariant;
	if (not bHit || not ClickedActor)
	{
		// Only reset if nothing was hit with a regular or control-click.
		SelectionAction = bIsHoldingShift
			                  ? ESelectionChangeAction::SelectionInvariant
			                  : ESelectionChangeAction::FullResetSelection;
		UpdateUIForSelectionAction(SelectionAction, ClickedActor);
		return;
	}
	ChangeClickedActorIfIsChildActor(ClickedActor);
	// If we do not hold any of these buttons and the clicked actor is selectable then we want to move to the selectable
	// actor instead.
	const bool bIsAddingOrRemovingFromSelection = bIsHoldingControl || bIsHoldingShift;
	if (bIsAddingOrRemovingFromSelection && GetIsClickedUnitSelectable(ClickedActor))
	{
		// We play the voice line of the new actor; not the primary.
		UpdateUIForSelectionAction(ClickUnit_GetSelectionAction(ClickedActor), ClickedActor);
		return;
	}
	if (IsAnyUnitSelected())
	{
		ExecuteCommandsForEachUnitSelected(ClickedActor, MouseProjectedLocation);
	}
}


void ACPPController::PrimaryClick()
{
	if (GetIsValidMainMenu())
	{
		if (M_MainGameUI->CLoseOptionUIOnLeftClick())
		{
			// click consumed by closing.
			return;
		}
	}
	switch (M_PrimaryClickContext)
	{
	case ERTSPrimaryClickContext::None:
		RTSFunctionLibrary::ReportError("NONE Primary click state on cpp controller");
		break;
	case ERTSPrimaryClickContext::RegularPrimaryClick:
		RegularPrimaryClick();
		break;
	case ERTSPrimaryClickContext::SecondaryActive:
		PrimaryClickWhileSecondaryActive();
		break;
	case ERTSPrimaryClickContext::FormationTypePrimaryClick:
		PrimaryClickFormationWidget();
		break;
	}
}


void ACPPController::RotateLeft()
{
	if (CPPConstructionPreviewRef)
	{
		CPPConstructionPreviewRef->RotatePreviewCounterclockwise();
	}
}

void ACPPController::RotateRight()
{
	if (CPPConstructionPreviewRef)
	{
		CPPConstructionPreviewRef->RotatePreviewClockwise();
	}
}

void ACPPController::RegularPrimaryClick()
{
	FHitResult HitUnderCursor;
	if (not GetHitResultUnderCursor(ECC_Visibility, false, HitUnderCursor))
	{
		return;
	}

	EnsureSelectionsAreRTSValid();
	AActor* ClickedActor = HitUnderCursor.GetActor();
	ChangeClickedActorIfIsChildActor(ClickedActor);
	FVector ClickedLocation = HitUnderCursor.Location;

	if (not IsAnyUnitSelected())
	{
		// Possibly add allied unit to selection.
		UpdateUIForSelectionAction(ClickUnit_GetSelectionAction(ClickedActor), ClickedActor);
		return;
	}

	if (GetIsPreviewBuildingActive())
	{
		if (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("In building mode trying to place building!", FColor::Blue);
		}

		if (TryPlaceBuilding(ClickedLocation))
		{
			StopPreviewAndBuildingMode(true);
			if (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
			{
				RTSFunctionLibrary::PrintString("Finished Buildingmode!", FColor::Red);
			}
		}
		return;
	}
	if (bM_IsActionButtonActive)
	{
		// Execute specific ActionButton logic depending on active ability.
		bM_IsActionButtonActive = !ExecuteActionButtonSecondClick(ClickedActor, ClickedLocation);
		if (!bM_IsActionButtonActive)
		{
			DeactivateActionButton();
		}
	}
	else
	{
		// Possibly add allied unit to selection.
		UpdateUIForSelectionAction(ClickUnit_GetSelectionAction(ClickedActor), ClickedActor);
	}
	if (DeveloperSettings::Debugging::GPlayerClickAndAction_Compile_DebugSymbols)
	{
		if (IsValid(ClickedActor))
		{
			RTSFunctionLibrary::PrintString("clicked actor: " + ClickedActor->GetName(), FColor::Blue);
		}
		else
		{
			RTSFunctionLibrary::PrintString("clicked actor is not valid at PrimaryClick()", FColor::Red);
		}
	}
}

void ACPPController::PrimaryClickWhileSecondaryActive()
{
	if (not GetIsValidFormationController())
	{
		return;
	}
	FVector2D MouseScreenPosition;
	if (not GetMousePosition(MouseScreenPosition.X, MouseScreenPosition.Y))
	{
		RTSFunctionLibrary::ReportError("Failed to get mouse position to start formation picker.");
		return;
	}
	if (M_FormationController->ActivateFormationPicker(MouseScreenPosition))
	{
		// The next click will be consumed by the formation widget.
		M_PrimaryClickContext = ERTSPrimaryClickContext::FormationTypePrimaryClick;
		return;
	}
	RTSFunctionLibrary::DisplayNotification(
		FText::FromString("Failed to activate formation widget, primary click context is unaltered."));
}

void ACPPController::PrimaryClickFormationWidget()
{
	if (not GetIsValidFormationController())
	{
		// Reset to prevent consuming clicks forever.
		M_PrimaryClickContext = ERTSPrimaryClickContext::RegularPrimaryClick;
		M_PrimaryClickContext = ERTSPrimaryClickContext::RegularPrimaryClick;
		RTSFunctionLibrary::DisplayNotification(FText::FromString("The primary click for the formation picker failed"
			"and the context was reset to regular primary click. "));
		return;
	}
	FVector2D MouseScreenPosition;
	if (not GetMousePosition(MouseScreenPosition.X, MouseScreenPosition.Y))
	{
		RTSFunctionLibrary::ReportError("Failed to get mouse position to pick formation.");
		return;
	}
	M_FormationController->PrimaryClickPickFormation(MouseScreenPosition);
	// Reset.
	M_PrimaryClickContext = ERTSPrimaryClickContext::RegularPrimaryClick;
}

void ACPPController::OnSecondaryClickFinished_CheckPrimaryClickContext()
{
	if (M_PrimaryClickContext == ERTSPrimaryClickContext::SecondaryActive)
	{
		// Secondary was active but not used to start formation widget; resume regular primary click context.
		M_PrimaryClickContext = ERTSPrimaryClickContext::RegularPrimaryClick;
	}
}

void ACPPController::RebuildGameUIHierarchy()
{
	if (not GetIsValidGameUIController() || not GetIsValidCommandTypeDecoder())
	{
		return;
	}
	M_GameUIController->RebuildGameUIHierarchy(&TSelectedPawnMasters, &TSelectedSquadControllers,
	                                           &TSelectedActorsMasters);

	M_CommandTypeDecoder->ResetAllPlacementEffects();
	M_CommandTypeDecoder->SpawnPlacementEffectsForPrimarySelected(
		M_GameUIController->GetPrimarySelectedUnitType(),
		M_GameUIController->GetPrimarySelectedUnit());
}

void ACPPController::Internal_MarqueeFullSelectionReset_UpdateArraysAndDecals(
	const TArray<ASquadUnit*>& MarqueeSquadUnits,
	const TArray<ASelectableActorObjectsMaster*>& MarqueeSelectableActors,
	const TArray<ASelectablePawnMaster*>& MarqueeSelectablePawns)
{
	// empty & deselect
	ResetSelectionArrays();
	TSelectedActorsMasters = MarqueeSelectableActors;
	TSelectedPawnMasters = MarqueeSelectablePawns;
	for (const auto EachSquadUnit : MarqueeSquadUnits)
	{
		if (IsValid(EachSquadUnit))
		{
			TSelectedSquadControllers.AddUnique(EachSquadUnit->GetSquadControllerChecked());
		}
	}
	// Ensure all the saved pointers are valid.
	EnsureSelectionsAreRTSValid();
	for (const auto EachSquad : TSelectedSquadControllers)
	{
		EachSquad->SetSquadSelected(true);
	}
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		EachPawn->SetUnitSelected(true);
	}
	for (const auto EachActor : TSelectedActorsMasters)
	{
		EachActor->SetUnitSelected(true);
	}
}

ESelectionChangeAction ACPPController::Internal_MarqueeAddValidUniqueUnitsToSelection_UpdateArraysAndDecals(
	const TArray<ASquadUnit*>& MarqueeSquadUnits,
	const TArray<ASelectableActorObjectsMaster*>& MarqueeSelectableActors,
	const TArray<ASelectablePawnMaster*>& MarqueeSelectablePawns)
{
	const TSet<FTrainingOption> TypesBefore = FPlayerSelectionHelpers::BuildTrainingOptionSet(TSelectedPawnMasters,
		TSelectedSquadControllers, TSelectedActorsMasters);
	for (const auto EachSquadUnit : MarqueeSquadUnits)
	{
		if (not RTSFunctionLibrary::RTSIsValid(EachSquadUnit))
		{
			continue;
		}
		if (EachSquadUnit->GetSquadControllerChecked() && not TSelectedSquadControllers.Contains(
			EachSquadUnit->GetSquadControllerChecked()))
		{
			TSelectedSquadControllers.Add(EachSquadUnit->GetSquadControllerChecked());
			EachSquadUnit->GetSquadControllerChecked()->SetSquadSelected(true);
		}
	}
	for (auto EachSelectableActor : MarqueeSelectableActors)
	{
		if (not RTSFunctionLibrary::RTSIsValid(EachSelectableActor))
		{
			continue;
		}
		if (not TSelectedActorsMasters.Contains(EachSelectableActor))
		{
			TSelectedActorsMasters.Add(EachSelectableActor);
			EachSelectableActor->SetUnitSelected(true);
		}
	}
	for (auto EachSelectablePawn : MarqueeSelectablePawns)
	{
		if (not RTSFunctionLibrary::RTSIsValid(EachSelectablePawn))
		{
			continue;
		}
		if (not TSelectedPawnMasters.Contains(EachSelectablePawn))
		{
			TSelectedPawnMasters.Add(EachSelectablePawn);
			EachSelectablePawn->SetUnitSelected(true);
		}
	}
	TSet<FTrainingOption> TypesAfter = FPlayerSelectionHelpers::BuildTrainingOptionSet(TSelectedPawnMasters,
		TSelectedSquadControllers, TSelectedActorsMasters);
	if (FPlayerSelectionHelpers::AreThereTypeDifferencesBetweenSets(TypesBefore, TypesAfter))
	{
		// Type differences; we need to rebuild the action UI hierarchy.
		return ESelectionChangeAction::UnitAdded_RebuildUIHierarchy;
	}
	return ESelectionChangeAction::UnitAdded_HierarchyInvariant;
}

bool ACPPController::Internal_IsMarqueeOnlyAlreadySelectedUnits(const TArray<ASquadUnit*>& MarqueeSquadUnits,
                                                                const TArray<ASelectableActorObjectsMaster*>&
                                                                MarqueeSelectableActors,
                                                                const TArray<ASelectablePawnMaster*>&
                                                                MarqueeSelectablePawn) const
{
	for (auto EachSquadUnit : MarqueeSquadUnits)
	{
		if (not RTSFunctionLibrary::RTSIsValid(EachSquadUnit))
		{
			continue;
		}
		if (not FPlayerSelectionHelpers::IsSquadUnitSelected(EachSquadUnit))
		{
			return false;
		}
	}
	for (auto EachSelectableActor : MarqueeSelectableActors)
	{
		if (not RTSFunctionLibrary::RTSIsValid(EachSelectableActor))
		{
			continue;
		}
		if (not FPlayerSelectionHelpers::IsActorSelected(EachSelectableActor))
		{
			return false;
		}
	}
	for (auto EachSelectablePawn : MarqueeSelectablePawn)
	{
		if (not RTSFunctionLibrary::RTSIsValid(EachSelectablePawn))
		{
			continue;
		}
		if (not FPlayerSelectionHelpers::IsPawnSelected(EachSelectablePawn))
		{
			return false;
		}
	}
	return true;
}

ESelectionChangeAction ACPPController::Internal_RemoveMarqueeFromSelection(const TArray<ASquadUnit*>& MarqueeSquadUnits,
                                                                           const TArray<ASelectableActorObjectsMaster*>&
                                                                           MarqueeSelectableActors,
                                                                           const TArray<ASelectablePawnMaster*>&
                                                                           MarqueeSelectablePawn)
{
	TSet<FTrainingOption> TypesBefore = FPlayerSelectionHelpers::BuildTrainingOptionSet(TSelectedPawnMasters,
		TSelectedSquadControllers, TSelectedActorsMasters);
	for (auto EachSquadUnit : MarqueeSquadUnits)
	{
		if (not RTSFunctionLibrary::RTSIsValid(EachSquadUnit))
		{
			continue;
		}
		if (EachSquadUnit->GetSquadControllerChecked() && TSelectedSquadControllers.Contains(
			EachSquadUnit->GetSquadControllerChecked()))
		{
			TSelectedSquadControllers.Remove(EachSquadUnit->GetSquadControllerChecked());
			EachSquadUnit->GetSquadControllerChecked()->SetSquadSelected(false);
		}
	}
	for (auto EachSelectableActor : MarqueeSelectableActors)
	{
		if (not RTSFunctionLibrary::RTSIsValid(EachSelectableActor))
		{
			continue;
		}
		if (TSelectedActorsMasters.Contains(EachSelectableActor))
		{
			TSelectedActorsMasters.Remove(EachSelectableActor);
			EachSelectableActor->SetUnitSelected(false);
		}
	}
	for (auto EachSelectablePawn : MarqueeSelectablePawn)
	{
		if (not RTSFunctionLibrary::RTSIsValid(EachSelectablePawn))
		{
			continue;
		}
		if (TSelectedPawnMasters.Contains(EachSelectablePawn))
		{
			TSelectedPawnMasters.Remove(EachSelectablePawn);
			EachSelectablePawn->SetUnitSelected(false);
		}
	}
	TSet<FTrainingOption> TypesAfter = FPlayerSelectionHelpers::BuildTrainingOptionSet(TSelectedPawnMasters,
		TSelectedSquadControllers, TSelectedActorsMasters);
	const bool bDidTypeHierarchyChange = FPlayerSelectionHelpers::AreThereTypeDifferencesBetweenSets(
		TypesBefore, TypesAfter);
	if (bDidTypeHierarchyChange)
	{
		return ESelectionChangeAction::UnitRemoved_RebuildUIHierarchy;
	}
	return ESelectionChangeAction::UnitRemoved_HierarchyInvariant;
}

void ACPPController::UpdateUIForSelectionAction(const ESelectionChangeAction Action,
                                                AActor* AddSelectedActorToPlayVoiceLine)
{
	if (not GetIsValidGameUIController())
	{
		return;
	}
	bool bRebuildActionUIHierarchy = false;
	bool bRebuildSelectionUIOnly = false;
	bool bPlayPrimaryVoiceLine_UnitAddedNotDetermined = false;
	switch (Action)
	{
	case ESelectionChangeAction::None:
		break;
	case ESelectionChangeAction::FullResetSelection:
		{
			ResetSelectionArrays();
			bRebuildActionUIHierarchy = true;
		}
		break;
	case ESelectionChangeAction::SelectionInvariant:
		break;
	case ESelectionChangeAction::UnitAdded_RebuildUIHierarchy:
		bRebuildActionUIHierarchy = true;
		if (not AddSelectedActorToPlayVoiceLine)
		{
			// There were units added and the hierarchy needs to be rebuilt but the process that added the units
			// could not determine which unit needs to use the voice line, and so we play the primary selected voice line
			// after rebuilding the hierarchy instead.
			bPlayPrimaryVoiceLine_UnitAddedNotDetermined = true;
			break;
		}
		PlayActorSelectionVoiceLine(AddSelectedActorToPlayVoiceLine);
		break;
	case ESelectionChangeAction::UnitRemoved_RebuildUIHierarchy:
		bRebuildActionUIHierarchy = true;
		break;
	case ESelectionChangeAction::UnitRemoved_HierarchyInvariant:
		bRebuildSelectionUIOnly = true;
		break;
	case ESelectionChangeAction::UnitAdded_HierarchyInvariant:
		bRebuildSelectionUIOnly = true;
		break;
	}
	if (bRebuildActionUIHierarchy)
	{
		RebuildGameUIHierarchy();
		if (bPlayPrimaryVoiceLine_UnitAddedNotDetermined)
		{
			PlayVoiceLineForPrimarySelected(ERTSVoiceLine::Select, true);
		}
		return;
	}
	if (bRebuildSelectionUIOnly)
	{
		M_GameUIController->OnlyRebuildSelectionUI(&TSelectedPawnMasters, &TSelectedSquadControllers,
		                                           &TSelectedActorsMasters);
	}
}

void ACPPController::PlayActorSelectionVoiceLine(const AActor* SelectedActor) const
{
	if (not IsValid(SelectedActor) || not GetIsValidPlayerAudioController())
	{
		return;
	}
	M_PlayerAudioController->PlayVoiceLine(SelectedActor, ERTSVoiceLine::Select, false);
}

void ACPPController::ChangeClickedActorIfIsChildActor(AActor*& ClickedActor) const
{
	if (not RTSFunctionLibrary::RTSIsValid(ClickedActor))
	{
		return;
	}
	const bool bIsParentActorValid = IsValid(ClickedActor->GetParentActor());
	if (not bIsParentActorValid)
	{
		return;
	}
	if (ClickedActor->IsA(AInfantryWeaponMaster::StaticClass()) || ClickedActor->IsA(ACPPTurretsMaster::StaticClass()))
	{
		ClickedActor = ClickedActor->GetParentActor();
	}
}

bool ACPPController::GetIsClickedUnitSelectable(AActor* ClickedActor) const
{
	if (not RTSFunctionLibrary::RTSIsValid(ClickedActor))
	{
		return false;
	}
	if (ClickedActor->IsA(ASquadUnit::StaticClass()))
	{
		ASquadUnit* SquadUnit = Cast<ASquadUnit>(ClickedActor);
		if (not SquadUnit || not SquadUnit->GetRTSComponent())
		{
			return false;
		}
		return SquadUnit->GetRTSComponent()->GetOwningPlayer() == 1;
	}
	if (ClickedActor->IsA(ASelectablePawnMaster::StaticClass()))
	{
		ASelectablePawnMaster* SelectablePawn = Cast<ASelectablePawnMaster>(ClickedActor);
		if (not SelectablePawn || not SelectablePawn->GetRTSComponent())
		{
			return false;
		}
		return SelectablePawn->GetRTSComponent()->GetOwningPlayer() == 1;
	}
	if (ClickedActor->IsA(ASelectableActorObjectsMaster::StaticClass()))
	{
		ASelectableActorObjectsMaster* SelectableActor = Cast<ASelectableActorObjectsMaster>(ClickedActor);
		if (not SelectableActor || not SelectableActor->GetRTSComponent())
		{
			return false;
		}
		return SelectableActor->GetRTSComponent()->GetOwningPlayer() == 1;
	}
	return false;
}

bool ACPPController::IsAnyUnitSelected() const
{
	if (TSelectedSquadControllers.IsEmpty() && TSelectedActorsMasters.IsEmpty() && TSelectedPawnMasters.IsEmpty())
	{
		return false;
	}
	return true;
}

ESelectionChangeAction ACPPController::ClickUnit_GetSelectionAction(AActor* NotNullActor)
{
	if (NotNullActor->IsA(ASquadUnit::StaticClass()))
	{
		return Click_OnSquadUnit(Cast<ASquadUnit>(NotNullActor));
	}
	if (NotNullActor->IsA(ASelectablePawnMaster::StaticClass()))
	{
		return Click_OnSelectablePawn(Cast<ASelectablePawnMaster>(NotNullActor));
	}
	if (NotNullActor->IsA(ASelectableActorObjectsMaster::StaticClass()))
	{
		return Click_OnSelectableActor(Cast<ASelectableActorObjectsMaster>(NotNullActor));
	}
	// todo add a setting for this? a player can choose to opt out of deselecting when not clicking on a selectable unit.
	// Do not change selection.
	return ESelectionChangeAction::SelectionInvariant;
}

ESelectionChangeAction ACPPController::Click_OnSquadUnit(ASquadUnit* SquadUnit)
{
	if (not RTSFunctionLibrary::RTSIsValid(SquadUnit) || not SquadUnit->GetRTSComponent()
		|| SquadUnit->GetRTSComponent()->GetOwningPlayer() != 1)
	{
		// Only reset the selection if a regular or control click was on this invalid unit.
		return bIsHoldingShift
			       ? ESelectionChangeAction::SelectionInvariant
			       : ESelectionChangeAction::FullResetSelection;
	}
	if (bIsHoldingShift)
	{
		if (FPlayerSelectionHelpers::IsSquadUnitSelected(SquadUnit))
		{
			DebugPlayerSelection("Removed Squad unit with click");
			return Internal_RemoveSquadUnitFromSelection_UpdateDecals(SquadUnit);
		}
		DebugPlayerSelection("Added Squad unit with click");
		return Internal_AddSquadFromUnitToSelection_UpdateDecals(SquadUnit);
	}
	if (bIsHoldingControl)
	{
		DebugPlayerSelection("Removed Squad unit with click");
		return Internal_RemoveSquadUnitFromSelection_UpdateDecals(SquadUnit);
	}
	// select single unit
	ResetSelectionArrays();
	DebugPlayerSelection("SINGLE Selected Squad unit with click");
	return Internal_AddSquadFromUnitToSelection_UpdateDecals(SquadUnit);
}

ESelectionChangeAction ACPPController::Click_OnSelectablePawn(ASelectablePawnMaster* SelectablePawn)
{
	if (not RTSFunctionLibrary::RTSIsValid(SelectablePawn) || not SelectablePawn->GetRTSComponent()
		|| SelectablePawn->GetRTSComponent()->GetOwningPlayer() != 1)
	{
		// Only reset the selection if a regular or control click was on this invalid unit.
		return bIsHoldingShift
			       ? ESelectionChangeAction::SelectionInvariant
			       : ESelectionChangeAction::FullResetSelection;
	}
	if (bIsHoldingShift)
	{
		if (FPlayerSelectionHelpers::IsPawnSelected(SelectablePawn))
		{
			DebugPlayerSelection("Removed Pawn with click");
			return Internal_RemovePawnFromSelection_UpdateDecals(SelectablePawn);
		}
		DebugPlayerSelection("Added Pawn with click");
		return Internal_AddPawnToSelection_UpdateDecals(SelectablePawn);
	}
	if (bIsHoldingControl)
	{
		DebugPlayerSelection("Removed Pawn with click");
		return Internal_RemovePawnFromSelection_UpdateDecals(SelectablePawn);
	}
	// select single unit
	ResetSelectionArrays();
	DebugPlayerSelection("SINGLE Selected Actor with click");
	return Internal_AddPawnToSelection_UpdateDecals(SelectablePawn);
}

ESelectionChangeAction ACPPController::Click_OnSelectableActor(ASelectableActorObjectsMaster* SelectableActor)
{
	if (not RTSFunctionLibrary::RTSIsValid(SelectableActor) || not SelectableActor->GetRTSComponent()
		|| SelectableActor->GetRTSComponent()->GetOwningPlayer() != 1)
	{
		// Only reset the selection if a regular or control click was on this invalid unit.
		return bIsHoldingShift
			       ? ESelectionChangeAction::SelectionInvariant
			       : ESelectionChangeAction::FullResetSelection;
	}
	if (bIsHoldingShift)
	{
		if (FPlayerSelectionHelpers::IsActorSelected(SelectableActor))
		{
			DebugPlayerSelection("Removed actor with click");
			return Internal_RemoveActorFromSelection_UpdateDecals(SelectableActor);
		}
		DebugPlayerSelection("Added actor with click");
		return Internal_AddActorToSelection_UpdateDecals(SelectableActor);
	}
	if (bIsHoldingControl)
	{
		DebugPlayerSelection("Removed actor with click");
		return Internal_RemoveActorFromSelection_UpdateDecals(SelectableActor);
	}
	// select single unit
	ResetSelectionArrays();
	DebugPlayerSelection("SINGLE Selected Actor with click");
	return Internal_AddActorToSelection_UpdateDecals(SelectableActor);
}


ESelectionChangeAction ACPPController::Internal_AddSquadFromUnitToSelection_UpdateDecals(const ASquadUnit* SquadUnit)
{
	if (not IsValid(SquadUnit))
	{
		return ESelectionChangeAction::SelectionInvariant;
	}
	if (ASquadController* NewSquad = SquadUnit->GetSquadControllerChecked())
	{
		if (TSelectedSquadControllers.Contains(NewSquad))
		{
			DebugPlayerSelection("Squad already selected", FColor::Green);
			return ESelectionChangeAction::SelectionInvariant;
		}
		// Check if we add a new type of unit to the selection if so we need to rebuild the UI hierarchy.
		// Otherwise only update the selection UI.
		const ESelectionChangeAction SelectionChange = FPlayerSelectionHelpers::OnAddingSquad_GetSelectionChange(
			&TSelectedSquadControllers, NewSquad);
		NewSquad->SetSquadSelected(true);
		TSelectedSquadControllers.Add(NewSquad);
		return SelectionChange;
	}
	return ESelectionChangeAction::SelectionInvariant;
}

ESelectionChangeAction ACPPController::Internal_RemoveSquadUnitFromSelection_UpdateDecals(
	const ASquadUnit* SquadUnitToRemove)
{
	const bool bHasValidSquadController = IsValid(SquadUnitToRemove)
		                                      ? IsValid(SquadUnitToRemove->GetSquadControllerChecked())
		                                      : false;

	ASquadController* SquadControllerToRemove = bHasValidSquadController
		                                            ? SquadUnitToRemove->GetSquadControllerChecked()
		                                            : nullptr;
	if (not bHasValidSquadController || not TSelectedSquadControllers.Contains(SquadControllerToRemove))
	{
		// No squad controller to remove.
		return ESelectionChangeAction::SelectionInvariant;
	}
	// If the rts component is broken we remove and rebuild to be safe.
	if (not IsValid(SquadUnitToRemove->GetRTSComponent()))
	{
		TSelectedSquadControllers.Remove(SquadControllerToRemove);
		SquadControllerToRemove->SetSquadSelected(false);
		return ESelectionChangeAction::UnitRemoved_RebuildUIHierarchy;
	}
	// Check if this squad is primary selected.
	const bool bIsPrimarySelected = M_GameUIController->GetPrimarySelectedUnit() == SquadControllerToRemove;
	const FTrainingOption RemovedType = SquadControllerToRemove->GetRTSComponent()->GetUnitTrainingOption();
	// Check if the type of the squad is still selected.
	bool bIsTypeStillSelected = false;
	TSelectedSquadControllers.Remove(SquadControllerToRemove);
	SquadControllerToRemove->SetSquadSelected(false);
	for (auto EachSquad : TSelectedSquadControllers)
	{
		if (not RTSFunctionLibrary::RTSIsValid(EachSquad))
		{
			continue;
		}
		if (EachSquad->GetRTSComponent()->GetUnitTrainingOption() == RemovedType)
		{
			bIsTypeStillSelected = true;
			break;
		}
	}
	if (!bIsTypeStillSelected || bIsPrimarySelected)
	{
		DebugPlayerSelection("Removed last type of squad or primary squad: REBUILD UI", FColor::Red);
		// This type is no longer in the UI; need to rebuild hierarchy.
		// Or the type is available still but the primary selected actor was removed!
		return ESelectionChangeAction::UnitRemoved_RebuildUIHierarchy;
	}
	DebugPlayerSelection("Removed Squad with no effects on UI hierarchy; only affect selection UI.", FColor::Red);
	// Not primary and there are still types of this unit type available; no need to rebuild the UI hierarchy
	return ESelectionChangeAction::UnitRemoved_HierarchyInvariant;
}

ESelectionChangeAction ACPPController::Internal_RemovePawnFromSelection_UpdateDecals(
	ASelectablePawnMaster* SelectablePawnToRemove)
{
	if (not IsValid(SelectablePawnToRemove) || not TSelectedPawnMasters.Contains(SelectablePawnToRemove))
	{
		// no pawn to remove.
		return ESelectionChangeAction::SelectionInvariant;
	}

	// If the rts component is broken we remove and rebuild to be safe.
	if (not IsValid(SelectablePawnToRemove->GetRTSComponent()))
	{
		TSelectedPawnMasters.Remove(SelectablePawnToRemove);
		SelectablePawnToRemove->SetUnitSelected(false);
		return ESelectionChangeAction::UnitRemoved_RebuildUIHierarchy;
	}

	// Check if this pawn is primary selected.
	const bool bIsPrimarySelected = M_GameUIController->GetPrimarySelectedUnit() == SelectablePawnToRemove;
	const FTrainingOption RemovedType = SelectablePawnToRemove->GetRTSComponent()->GetUnitTrainingOption();
	TSelectedPawnMasters.Remove(SelectablePawnToRemove);
	SelectablePawnToRemove->SetUnitSelected(false);
	// Check if the type is still selected.
	bool bIsTypeStillSelected = false;
	for (const auto EachSelectedPawn : TSelectedPawnMasters)
	{
		if (not RTSFunctionLibrary::RTSIsValid(EachSelectedPawn))
		{
			continue;
		}
		if (EachSelectedPawn->GetRTSComponent()->GetUnitTrainingOption() == RemovedType)
		{
			bIsTypeStillSelected = true;
			break;
		}
	}
	if (not bIsTypeStillSelected || bIsPrimarySelected)
	{
		DebugPlayerSelection("Removed last type of pawn or primary Pawn: REBUILD UI", FColor::Red);
		// This type is no longer in the UI; need to rebuild hierarchy.
		// Or the type is available still but the primary selected actor was removed!
		return ESelectionChangeAction::UnitRemoved_RebuildUIHierarchy;
	}
	DebugPlayerSelection("Removed Pawn with no effects on UI hierarchy; only rebuild selection UI.", FColor::Red);
	// Not primary and there are still types of this unit type available; no need to rebuild the UI
	return ESelectionChangeAction::UnitRemoved_HierarchyInvariant;
}

ESelectionChangeAction ACPPController::Internal_RemoveActorFromSelection_UpdateDecals(
	ASelectableActorObjectsMaster* SelectableActorToRemove)
{
	if (not IsValid(SelectableActorToRemove) || not TSelectedActorsMasters.Contains(SelectableActorToRemove))
	{
		// no actor to remove.
		return ESelectionChangeAction::SelectionInvariant;
	}

	// If the rts component is broken we remove and rebuild to be safe.
	if (not IsValid(SelectableActorToRemove->GetRTSComponent()))
	{
		TSelectedActorsMasters.Remove(SelectableActorToRemove);
		SelectableActorToRemove->SetUnitSelected(false);
		return ESelectionChangeAction::UnitRemoved_RebuildUIHierarchy;
	}

	// Check if this pawn is primary selected.
	const bool bIsPrimarySelected = M_GameUIController->GetPrimarySelectedUnit() == SelectableActorToRemove;
	const FTrainingOption RemovedType = SelectableActorToRemove->GetRTSComponent()->GetUnitTrainingOption();
	TSelectedActorsMasters.Remove(SelectableActorToRemove);
	SelectableActorToRemove->SetUnitSelected(false);
	// Check if the type is still selected.
	bool bIsTypeStillSelected = false;
	for (const auto EachSelectableActor : TSelectedActorsMasters)
	{
		if (not RTSFunctionLibrary::RTSIsValid(EachSelectableActor))
		{
			continue;
		}
		if (EachSelectableActor->GetRTSComponent()->GetUnitTrainingOption() == RemovedType)
		{
			bIsTypeStillSelected = true;
			break;
		}
	}
	if (not bIsTypeStillSelected || bIsPrimarySelected)
	{
		DebugPlayerSelection("Removed last type of Actor or primary Actor: REBUILD UI", FColor::Red);
		// This type is no longer in the UI; need to rebuild hierarchy.
		// Or the type is available still but the primary selected actor was removed!
		return ESelectionChangeAction::UnitRemoved_RebuildUIHierarchy;
	}
	DebugPlayerSelection("Removed actor with no effects on UI hierarchy.", FColor::Red);
	// Not primary and there are still types of this unit type available; no need to rebuild the UI
	// rebuild selection UI only
	return ESelectionChangeAction::UnitRemoved_HierarchyInvariant;
}


ESelectionChangeAction ACPPController::Internal_AddPawnToSelection_UpdateDecals(ASelectablePawnMaster* SelectablePawn)
{
	if (not IsValid(SelectablePawn))
	{
		return ESelectionChangeAction::SelectionInvariant;
	}
	if (TSelectedPawnMasters.Contains(SelectablePawn))
	{
		DebugPlayerSelection("Pawn already selected", FColor::Green);
		return ESelectionChangeAction::SelectionInvariant;
	}
	// Determine if we added a new type in which case we rebuild the action UI hierarchy if not we only update the SelectionUI.
	const ESelectionChangeAction SelectionChange = FPlayerSelectionHelpers::OnAddingPawn_GetSelectionChange(
		&TSelectedPawnMasters, SelectablePawn);
	SelectablePawn->SetUnitSelected(true);
	TSelectedPawnMasters.Add(SelectablePawn);
	return SelectionChange;
}

ESelectionChangeAction ACPPController::Internal_AddActorToSelection_UpdateDecals(
	ASelectableActorObjectsMaster* SelectableActor)
{
	if (not IsValid(SelectableActor))
	{
		return ESelectionChangeAction::SelectionInvariant;
	}
	if (TSelectedActorsMasters.Contains(SelectableActor))
	{
		DebugPlayerSelection("Actor already selected", FColor::Green);
		return ESelectionChangeAction::SelectionInvariant;
	}
	if (SelectableActor->GetSelectionComponent())
	{
		// Determine if we added a new type in which case we rebuild the action UI hierarchy if not we only update the SelectionUI.
		const ESelectionChangeAction SelectionChange = FPlayerSelectionHelpers::OnAddingActor_GetSelectionChange(
			&TSelectedActorsMasters, SelectableActor);
		SelectableActor->GetSelectionComponent()->SetUnitSelected();
		TSelectedActorsMasters.Add(SelectableActor);
		return SelectionChange;
	}
	return ESelectionChangeAction::SelectionInvariant;
}


UPlayerCameraController* ACPPController::GetValidPlayerCameraController()
{
	if (IsValid(M_PlayerCameraController))
	{
		return M_PlayerCameraController;
	}
	M_PlayerCameraController = FindComponentByClass<UPlayerCameraController>();
	if (not IsValid(M_PlayerCameraController))
	{
		RTSFunctionLibrary::ReportError("could not find player camera controller!!");
	}
	return M_PlayerCameraController;
}

ALandscapedeformManager* ACPPController::GetLandscapeDeformManagerFromWorld() const
{
	if (const UWorld* World = GetWorld())
	{
		if (ALandscapedeformManager* LandscapedeformManager = Cast<ALandscapedeformManager>(
			UGameplayStatics::GetActorOfClass(World, ALandscapedeformManager::StaticClass())))
		{
			return LandscapedeformManager;
		}
	}
	return nullptr;
}

bool ACPPController::GetSecondaryClickHitActorAndLocation(AActor*& OutClickedActor, FVector& OutHitLocation) const
{
	if (not M_SecondaryStartClickedActor.IsValid() || M_SecondaryStartMouseProjectedLocation.IsNearlyZero())
	{
		FHitResult HitResult;
		const bool bHit = GetHitResultUnderCursor(ECC_Visibility, false, HitResult);
		OutClickedActor = HitResult.GetActor();
		OutHitLocation = HitResult.Location;
		return bHit;
	}
	OutClickedActor = M_SecondaryStartClickedActor.Get();
	OutHitLocation = M_SecondaryStartMouseProjectedLocation;
	return true;
}

void ACPPController::BeginPlay_SetupRotationArrow()
{
	if (not PlayerRotationArrow.RotationArrowClass)
	{
		RTSFunctionLibrary::ReportError("No valid rotation arrow class found on rotation arrow struct"
			" in player controller at bp");
		return;
	}
	if (not GetIsValidFormationController())
	{
		return;
	}
	PlayerRotationArrow.PlayerFormationController = M_FormationController;
	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PlayerRotationArrow.RotationArrowActor = World->SpawnActor<AActor>(
			PlayerRotationArrow.RotationArrowClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		if (IsValid(PlayerRotationArrow.RotationArrowActor))
		{
			PlayerRotationArrow.RotationArrowActor->SetActorHiddenInGame(true);
			PlayerRotationArrow.RotationArrowActor->SetActorEnableCollision(false);
		}
		else
		{
			RTSFunctionLibrary::ReportError("Could not spawn rotation arrow actor in player controller at bp");
		}
	}
}


/*--------------------------------- SHIFT WITH NO ACTIONBUTTON ---------------------------------*/

void ACPPController::RemoveSquadOfSquadUnitFromSelectionAndUpdateUI(const ASquadUnit* SquadUnitToRemove)
{
	if (!IsValid(SquadUnitToRemove)) { return; }
	if (ASquadController* Squad = SquadUnitToRemove->GetSquadControllerChecked())
	{
		RemoveSquadFromSelectionAndUpdateUI(Squad);
	}
}


void ACPPController::RemoveSquadFromSelectionAndUpdateUI(ASquadController* SquadControllerToRemove)
{
	if (!IsValid(SquadControllerToRemove) || !GetIsValidGameUIController())
	{
		return;
	}
	EnsureSelectionsAreRTSValid();
	SquadControllerToRemove->SetSquadSelected(false);
	// Check if this squad is primary selected.
	const bool bRemovedPrimarySelected = M_GameUIController->GetPrimarySelectedUnit() == SquadControllerToRemove;
	const FTrainingOption RemovedType = SquadControllerToRemove->GetRTSComponent()->GetUnitTrainingOption();
	// Check if the type of the squad is still selected.
	bool bIsTypeStillSelected = false;
	TSelectedSquadControllers.Remove(SquadControllerToRemove);
	for (auto EachSquad : TSelectedSquadControllers)
	{
		if (EachSquad->GetRTSComponent()->GetUnitTrainingOption() == RemovedType)
		{
			bIsTypeStillSelected = true;
			break;
		}
	}
	ESelectionChangeAction SelectionAction;
	if (!bIsTypeStillSelected || bRemovedPrimarySelected)
	{
		SelectionAction = ESelectionChangeAction::UnitRemoved_RebuildUIHierarchy;
	}
	else
	{
		SelectionAction = ESelectionChangeAction::UnitRemoved_HierarchyInvariant;
	}
	UpdateUIForSelectionAction(SelectionAction, nullptr);
}

void ACPPController::RemoveActorFromSelectionAndUpdateUI(ASelectableActorObjectsMaster* ActorToRemove)
{
	if (!IsValid(ActorToRemove) || !GetIsValidGameUIController())
	{
		return;
	}
	const ESelectionChangeAction Action = Internal_RemoveActorFromSelection_UpdateDecals(ActorToRemove);
	UpdateUIForSelectionAction(Action, nullptr);
}

void ACPPController::RemovePawnFromSelectionAndUpdateUI(ASelectablePawnMaster* PawnToRemove)
{
	if (not IsValid(PawnToRemove) || !GetIsValidGameUIController())
	{
		return;
	}
	const ESelectionChangeAction Action = Internal_RemovePawnFromSelection_UpdateDecals(PawnToRemove);
	UpdateUIForSelectionAction(Action, nullptr);
}

// Keep only units of UnitID.UnitType across all selection arrays.
// If selection already contains only that type, UI remains unchanged.
// If the type is not present in any selection array, report an error.
void ACPPController::OnlySelectUnitsOfType(const FTrainingOption& UnitID)
{
	EnsureSelectionsAreRTSValid();
	const TSet<FTrainingOption> TypesBefore =
		FPlayerSelectionHelpers::BuildTrainingOptionSet(TSelectedPawnMasters, TSelectedSquadControllers,
		                                                TSelectedActorsMasters);

	if (!TypesBefore.Contains(UnitID))
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("OnlySelectUnitsOfType: no units of type %s in current selection."),
			                *UnitID.GetDisplayName()));
		return;
	}

	// Already only this type? leave UI invariant.
	if (TypesBefore.Num() == 1 && TypesBefore.Contains(UnitID))
	{
		return;
	}

	// Filter each selection array to the requested type (preserve per-array order).
	FPlayerSelectionHelpers::DeselectPawnsNotMatchingID(TSelectedPawnMasters, UnitID);
	FPlayerSelectionHelpers::DeselectSquadsNotMatchingID(TSelectedSquadControllers, UnitID);
	FPlayerSelectionHelpers::DeselectActorsNotMatchingID(TSelectedActorsMasters, UnitID);

	// Rebuild the hierarchy & selection panel to reflect the new single-type selection.
	UpdateUIForSelectionAction(ESelectionChangeAction::UnitAdded_RebuildUIHierarchy, nullptr);
}


void ACPPController::RemoveUnitAtIndex(const FTrainingOption& UnitID, int32 SelectionArrayIndex)
{
	if (!GetIsValidGameUIController())
	{
		return;
	}
	EnsureSelectionsAreRTSValid();

	// Snapshot *training options* (type + subtype) before removal
	const TSet<FTrainingOption> OptionsBefore =
		FPlayerSelectionHelpers::BuildTrainingOptionSet(TSelectedPawnMasters, TSelectedSquadControllers,
		                                                TSelectedActorsMasters);

	bool bRemoved = false;

	// Try in the pawns array
	if (!bRemoved)
	{
		bRemoved = FPlayerSelectionHelpers::RemovePawnAtIndexIfMatches(
			TSelectedPawnMasters, SelectionArrayIndex, UnitID);
	}

	// Try in the squads array
	if (!bRemoved)
	{
		bRemoved = FPlayerSelectionHelpers::RemoveSquadAtIndexIfMatches(
			TSelectedSquadControllers, SelectionArrayIndex, UnitID);
	}

	// Try in the actors array
	if (!bRemoved)
	{
		bRemoved = FPlayerSelectionHelpers::RemoveActorAtIndexIfMatches(
			TSelectedActorsMasters, SelectionArrayIndex, UnitID);
	}

	if (!bRemoved)
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("RemoveUnitAtIndex failed: no matching unit at index %d for %s (subtype %d)."),
			                SelectionArrayIndex,
			                *Global_GetUnitTypeString(UnitID.UnitType),
			                UnitID.SubtypeValue));
		return;
	}

	// Decide whether the *set of training options* changed
	const TSet<FTrainingOption> OptionsAfter =
		FPlayerSelectionHelpers::BuildTrainingOptionSet(TSelectedPawnMasters, TSelectedSquadControllers,
		                                                TSelectedActorsMasters);

	const bool bOptionsChanged =
		FPlayerSelectionHelpers::AreThereTypeDifferencesBetweenSets(OptionsBefore, OptionsAfter);

	// If options changed, rebuild the hierarchy; otherwise keep hierarchy and just refresh selection UI
	ESelectionChangeAction SelectionAction = bOptionsChanged
		                                         ? ESelectionChangeAction::UnitRemoved_RebuildUIHierarchy
		                                         : ESelectionChangeAction::UnitRemoved_HierarchyInvariant;

	UpdateUIForSelectionAction(SelectionAction, nullptr);
}

// Cycle the UI hierarchy until the primary selected type equals UnitID.UnitType (if present).
// Returns true on success; reports an error if the type is not found.
bool ACPPController::TryAdvancePrimaryToUnitType(const FTrainingOption& UnitID, const int32 SelectionArrayIndex)
{
	if (!IsValid(M_GameUIController))
	{
		RTSFunctionLibrary::ReportError(TEXT("TrySetActiveSelectionType: GameUIController is not valid."));
		return false;
	}

	EnsureSelectionsAreRTSValid();
	const TSet<FTrainingOption> TypesInSelection =
		FPlayerSelectionHelpers::BuildTrainingOptionSet(TSelectedPawnMasters, TSelectedSquadControllers,
		                                                TSelectedActorsMasters);

	if (!TypesInSelection.Contains(UnitID))
	{
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("TrySetActiveSelectionType: type %s is not present in current selection."),
			                *UnitID.GetDisplayName()));
		return false;
	}

	AActor* NewPrimarySelectedActor = nullptr;
	switch (UnitID.UnitType)
	{
	case EAllUnitType::UNType_None:
		return false;
	case EAllUnitType::UNType_Squad:
		NewPrimarySelectedActor = FPlayerSelectionHelpers::GetSquadAtIndexIfMatches(TSelectedSquadControllers,
			SelectionArrayIndex, UnitID);
		break;
	// Falls through.
	case EAllUnitType::UNType_Harvester:
	// Falls through.
	case EAllUnitType::UNType_Tank:
	// Falls through.
	case EAllUnitType::UNType_Nomadic:
		NewPrimarySelectedActor = FPlayerSelectionHelpers::GetPawnAtIndexIfMatches(TSelectedPawnMasters,
			SelectionArrayIndex, UnitID);
		break;
	case EAllUnitType::UNType_BuildingExpansion:
		NewPrimarySelectedActor = FPlayerSelectionHelpers::GetActorAtIndexIfMatches(TSelectedActorsMasters,
			SelectionArrayIndex, UnitID);
		break;
	case EAllUnitType::UNType_Aircraft:
		NewPrimarySelectedActor = FPlayerSelectionHelpers::GetPawnAtIndexIfMatches(TSelectedPawnMasters,
			SelectionArrayIndex, UnitID);
		break;
	}
	if (not IsValid(NewPrimarySelectedActor))
	{
		return false;
	}
	return M_GameUIController->TryAdvancePrimaryToUnitType(
		UnitID.UnitType, &TSelectedPawnMasters, &TSelectedSquadControllers,

		&TSelectedActorsMasters, NewPrimarySelectedActor);
}

void ACPPController::SelectOnScreenUnitsOfSameTypeAs(AActor* BasisActor)
{
	if (!RTSFunctionLibrary::RTSIsValid(BasisActor))
	{
		return;
	}

	// Resolve the target training option (type + subtype) from the basis actor.
	FTrainingOption TargetID{};
	const URTSComponent* BasisRTS = BasisActor->FindComponentByClass<URTSComponent>();

	// If this is a squad unit, prefer its controller's RTS (consistent with selection arrays),
	// but fall back to the unit's own RTS if needed.
	if (ASquadUnit* AsSquadUnit = Cast<ASquadUnit>(BasisActor))
	{
		if (ASquadController* C = AsSquadUnit->GetSquadControllerChecked())
		{
			if (const URTSComponent* CtrlRTS = C->FindComponentByClass<URTSComponent>())
			{
				TargetID = CtrlRTS->GetUnitTrainingOption();
			}
		}
		if (TargetID.IsNone() && BasisRTS)
		{
			TargetID = BasisRTS->GetUnitTrainingOption();
		}
	}
	else if (BasisRTS)
	{
		TargetID = BasisRTS->GetUnitTrainingOption();
	}

	if (TargetID.IsNone())
	{
		RTSFunctionLibrary::ReportError(
			TEXT("SelectOnScreenUnitsOfSameTypeAs: BasisActor has no valid RTS training option."));
		return;
	}

	// Screen bounds helper.
	int32 ViewX = 0, ViewY = 0;
	GetViewportSize(ViewX, ViewY);
	if (ViewX <= 0 || ViewY <= 0)
	{
		return;
	}

	auto IsOnScreen = [&](const FVector& WorldLocation) -> bool
	{
		FVector2D ScreenPos;
		if (!ProjectWorldLocationToScreen(WorldLocation, ScreenPos, true))
		{
			return false;
		}
		const bool bInBounds = (ScreenPos.X >= 0.f && ScreenPos.X <= ViewX && ScreenPos.Y >= 0.f && ScreenPos.Y <=
			ViewY);
		if (!bInBounds)
		{
			return false;
		}
		// Basic "in front of camera" check
		const FVector CamLoc = PlayerCameraManager ? PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
		const FVector CamFwd = PlayerCameraManager
			                       ? PlayerCameraManager->GetCameraRotation().Vector()
			                       : FVector::ForwardVector;
		const FVector Dir = (WorldLocation - CamLoc).GetSafeNormal();
		return FVector::DotProduct(CamFwd, Dir) > 0.f;
	};

	// Gather on-screen matches by category; we reuse the marquee-style internal helpers to minimize duplication.
	TArray<ASquadUnit*> MarqueeSquadUnits;
	TArray<ASelectableActorObjectsMaster*> MarqueeActors;
	TArray<ASelectablePawnMaster*> MarqueePawns;

	constexpr int32 PlayerIndex = 1; // local player ownership check used elsewhere in controller

	switch (TargetID.UnitType)
	{
	case EAllUnitType::UNType_Squad:
		for (TActorIterator<ASquadUnit> It(GetWorld()); It; ++It)
		{
			ASquadUnit* Unit = *It;
			if (!RTSFunctionLibrary::RTSIsValid(Unit)) { continue; }
			if (!IsOnScreen(Unit->GetActorLocation())) { continue; }

			// Ownership and type match
			const URTSComponent* UnitRTS = Unit->GetRTSComponent();
			ASquadController* C = Unit->GetSquadControllerChecked();
			const URTSComponent* CtrlRTS = C ? C->FindComponentByClass<URTSComponent>() : nullptr;

			const bool bOwned = UnitRTS && UnitRTS->GetOwningPlayer() == PlayerIndex;
			const FTrainingOption UnitTypeID = CtrlRTS
				                                   ? CtrlRTS->GetUnitTrainingOption()
				                                   : (UnitRTS ? UnitRTS->GetUnitTrainingOption() : FTrainingOption{});
			if (bOwned && UnitTypeID == TargetID)
			{
				MarqueeSquadUnits.Add(Unit);
			}
		}
		break;

	case EAllUnitType::UNType_Harvester:
	case EAllUnitType::UNType_Tank:
	case EAllUnitType::UNType_Nomadic:
	case EAllUnitType::UNType_Aircraft:
		for (TActorIterator<ASelectablePawnMaster> It(GetWorld()); It; ++It)
		{
			ASelectablePawnMaster* PawnMaster = *It;
			if (!RTSFunctionLibrary::RTSIsValid(PawnMaster)) { continue; }
			if (!IsOnScreen(PawnMaster->GetActorLocation())) { continue; }

			const URTSComponent* RTS = PawnMaster->GetRTSComponent();
			if (!RTS || RTS->GetOwningPlayer() != PlayerIndex) { continue; }
			if (RTS->GetUnitTrainingOption() == TargetID)
			{
				MarqueePawns.Add(PawnMaster);
			}
		}
		break;

	case EAllUnitType::UNType_BuildingExpansion:
		for (TActorIterator<ASelectableActorObjectsMaster> It(GetWorld()); It; ++It)
		{
			ASelectableActorObjectsMaster* Act = *It;
			if (!RTSFunctionLibrary::RTSIsValid(Act)) { continue; }
			if (!IsOnScreen(Act->GetActorLocation())) { continue; }

			const URTSComponent* RTS = Act->GetRTSComponent();
			if (!RTS || RTS->GetOwningPlayer() != PlayerIndex) { continue; }
			if (RTS->GetUnitTrainingOption() == TargetID)
			{
				MarqueeActors.Add(Act);
			}
		}
		break;

	default:
		// Unknown / none: nothing to select.
		return;
	}

	// Nothing visible of that type.
	if (MarqueeSquadUnits.Num() == 0 && MarqueeActors.Num() == 0 && MarqueePawns.Num() == 0)
	{
		return;
	}

	// Decide how to mutate selection using existing marquee helpers.
	ESelectionChangeAction ActionToApply = ESelectionChangeAction::SelectionInvariant;

	// SHIFT: toggle-add, but if all targets are already selected -> remove them.
	if (bIsHoldingShift)
	{
		const bool bOnlyAlreadySelected = Internal_IsMarqueeOnlyAlreadySelectedUnits(
			MarqueeSquadUnits, MarqueeActors, MarqueePawns);

		if (bOnlyAlreadySelected)
		{
			ActionToApply = Internal_RemoveMarqueeFromSelection(MarqueeSquadUnits, MarqueeActors, MarqueePawns);
		}
		else
		{
			ActionToApply = Internal_MarqueeAddValidUniqueUnitsToSelection_UpdateArraysAndDecals(
				MarqueeSquadUnits, MarqueeActors, MarqueePawns);
		}
	}
	else
	{
		// CTRL or no modifier: replace current selection with the on-screen matches.
		const TSet<FTrainingOption> Before = FPlayerSelectionHelpers::BuildTrainingOptionSet(
			TSelectedPawnMasters, TSelectedSquadControllers, TSelectedActorsMasters);

		Internal_MarqueeFullSelectionReset_UpdateArraysAndDecals(
			MarqueeSquadUnits, MarqueeActors, MarqueePawns);

		const TSet<FTrainingOption> After = FPlayerSelectionHelpers::BuildTrainingOptionSet(
			TSelectedPawnMasters, TSelectedSquadControllers, TSelectedActorsMasters);

		const bool bChanged = FPlayerSelectionHelpers::AreThereTypeDifferencesBetweenSets(Before, After);
		ActionToApply = bChanged
			                ? ESelectionChangeAction::UnitAdded_RebuildUIHierarchy
			                : ESelectionChangeAction::UnitAdded_HierarchyInvariant;
	}

	UpdateUIForSelectionAction(ActionToApply, nullptr);
}


void ACPPController::ExecuteCommandsForEachUnitSelected(
	AActor* ClickedActor,
	FVector& ClickedLocation)
{
	ECommandType CommandType = ECommandType::Movement;
	FTargetUnion TargetUnion;

	// Deduce order (Enum) depending on clicked actor.
	if (GetIsValidCommandTypeDecoder())
	{
		CommandType = M_CommandTypeDecoder->DecodeTargetedActor(ClickedActor, TargetUnion);
	}
	// If the primary selected unit is a trainer that can train then change movement commands to move rally point.
	CheckChangeCommandToMoveRallyPoint(CommandType);
	EAbilityID AbilityActivated;
	const uint32 CommandsExe = IssueCommandToSelectedUnits(CommandType, AbilityActivated, TargetUnion, ClickedLocation,
	                                                       ClickedActor);
	const bool bResetAllPlacementEffects = !bIsHoldingShift;
	constexpr bool bForcePlayVoiceLine = false;
	CreateVfxAndVoiceLineForIssuedCommand(
		bResetAllPlacementEffects, CommandsExe, ClickedLocation, CommandType, AbilityActivated, bForcePlayVoiceLine);
	if (DeveloperSettings::Debugging::GPlayerClickAndAction_Compile_DebugSymbols)
	{
		DebugActorAndCommand(ClickedActor, CommandType);
	}
}

void ACPPController::CheckChangeCommandToMoveRallyPoint(ECommandType& OutCommand) const
{
	if (not GetIsValidGameUIController() || OutCommand != ECommandType::Movement)
	{
		return;
	}
	if (M_GameUIController->GetIsPrimarySelectedEnabledTrainer())
	{
		OutCommand = ECommandType::MoveRallyPoint;
	}
}

uint32 ACPPController::IssueCommandToSelectedUnits(
	ECommandType& OutCommand,
	EAbilityID& OutAbilityActivated,
	const FTargetUnion& Target,
	FVector& ClickedLocation, const AActor* ClickedActor)
{
	bool bProjectionSuccess = false;
	uint32 AmountCommandsExe = 0;
	switch (OutCommand)
	{
	case ECommandType::EnterCargo:
		AmountCommandsExe += IssueOrderEnterCargo(Target.TargetActor, OutAbilityActivated);
		break;
	case ECommandType::EnemyCharacter:
		OutAbilityActivated = EAbilityID::IdAttack;
		AmountCommandsExe += OrderUnitsToAttackActor(Target.TargetCharacter);
		break;
	case ECommandType::AttackGround:
		OutAbilityActivated = EAbilityID::IdAttack;
		OrderUnitsAttackGround(ClickedLocation);
		break;
	// Falls Through.
	case ECommandType::EnemyActor:
	case ECommandType::DestroyDestructableEnvActor:
		OutAbilityActivated = EAbilityID::IdAttack;
		AmountCommandsExe += OrderUnitsToAttackActor(Target.TargetActor);
		break;
	case ECommandType::AlliedActor:
		AmountCommandsExe += IssueOrderRepairAlly(Target.TargetActor, OutAbilityActivated,
		                                          ClickedLocation);
		break;
	case ECommandType::AlliedAirBase:
		AmountCommandsExe += IssueOrderAlliedAirfield(Target.TargetActor, OutAbilityActivated, ClickedLocation);
		break;
	case ECommandType::AlliedBuilding:
		AmountCommandsExe += IssueOrderRepairAlly(Target.TargetBxp, OutAbilityActivated,
		                                          ClickedLocation);
		break;
	case ECommandType::AlliedCharacter:
		AmountCommandsExe += IssueOrderRepairAlly(Target.TargetCharacter, OutAbilityActivated, ClickedLocation);
		break;
	case ECommandType::Movement:
		ClickedLocation = RTSFunctionLibrary::GetLocationProjected(this, ClickedLocation, true,
		                                                           bProjectionSuccess, 5);
		OutAbilityActivated = EAbilityID::IdMove;
		AmountCommandsExe = MoveUnitsToLocation(ClickedLocation);
		break;
	case ECommandType::HarvestResource:
		OutAbilityActivated = EAbilityID::IdHarvestResource;
		AmountCommandsExe += IssueOrderHarvestResource(
			Target.TargetResource, OutAbilityActivated, OutCommand, ClickedLocation);
		break;
	case ECommandType::PickupItem:
		OutAbilityActivated = EAbilityID::IdPickupItem;
		AmountCommandsExe += IssueOrderPickUpItem(Target.PickupItem, OutAbilityActivated,
		                                          OutCommand, ClickedLocation);
		break;
	case ECommandType::ScavengeObject:
		OutAbilityActivated = EAbilityID::IdScavenge;
		AmountCommandsExe += IssueOrderScavengeItem(Target.ScavengeObject, OutAbilityActivated, OutCommand,
		                                            ClickedLocation);
		break;
	case ECommandType::DestroyScavengableObject:
		OutAbilityActivated = EAbilityID::IdAttack;
		AmountCommandsExe += OrderUnitsToAttackActor(Target.ScavengeObject);
		break;
	case ECommandType::MoveRallyPoint:
		OutCommand = ECommandType::MoveRallyPoint;
		OutAbilityActivated = EAbilityID::IdGeneral_Confirm;
		OrderUnitsToMoveRallyPoint(ClickedLocation);

	case ECommandType::CaptureActor:
		{
			// TargetUnion comes from the decoder; it should contain the capture target actor.
			AActor* const CaptureTargetActor = Target.TargetActor;

			// Only squads will actually execute the capture logic.
			AmountCommandsExe += OrderUnitsCaptureActor(CaptureTargetActor);

			if (AmountCommandsExe > 0)
			{
				// We successfully ordered at least one squad to capture.
				OutAbilityActivated = EAbilityID::IdCapture;
			}
			else
			{
				// No squads were able to capture (e.g., none selected).
				// Fallback: treat this as a normal move command to the click location.
				bool bSuccessful = false;
				const FVector NewClickedLocation = RTSFunctionLibrary::GetLocationProjected(
					this,
					NewClickedLocation,
					true,
					bSuccessful,
					5);

				AmountCommandsExe = MoveUnitsToLocation(NewClickedLocation);
				OutAbilityActivated = EAbilityID::IdMove;
			}
			break;
		}
	default:
		OutCommand = ECommandType::Movement;
		OutAbilityActivated = EAbilityID::IdMove;
		ClickedLocation = RTSFunctionLibrary::GetLocationProjected(this, ClickedLocation, true,
		                                                           bProjectionSuccess, 5);
		AmountCommandsExe = MoveUnitsToLocation(ClickedLocation);
	} // switch
	return AmountCommandsExe;
}

uint32 ACPPController::IssueOrderEnterCargo(AActor* CargoActor, EAbilityID& OutAbilityActivated)
{
	if (not RTSFunctionLibrary::RTSIsValid(CargoActor))
	{
		RTSFunctionLibrary::ReportError("not rts valid cargo actor provided for Issue order enter cargo");
		return 0;
	}
	OutAbilityActivated = EAbilityID::IdEnterCargo;
	uint32 Issued = 0;

	// Iterate selected squads; call their controller ExecuteEnterCargoCommand.
	for (ASquadController* Squad : TSelectedSquadControllers)
	{
		const bool bResetCommandQueueFirst = !bIsHoldingShift;
		Issued += Squad->EnterCargo(CargoActor, bResetCommandQueueFirst) == ECommandQueueError::NoError;
	}
	// If no squads make use of the clicked cargo actor then move there instead.
	if (Issued == 0)
	{
		OutAbilityActivated = EAbilityID::IdMove;
		Issued = MoveUnitsToLocation(CargoActor->GetActorLocation());
	}
	return Issued;
}

uint32 ACPPController::IssueOrderRepairAlly(AActor* AlliedActor, EAbilityID& OutIssuedAbility,
                                            FVector& ClickedLocation)
{
	uint32 AmountCommandsExe = 0;
	bool bSuccessful = false;
	if (FRTSRepairHelpers::GetIsUnitValidForRepairs(AlliedActor))
	{
		OutIssuedAbility = EAbilityID::IdRepair;
		AmountCommandsExe += IssueOrderRepairToAllSelectedUnits(AlliedActor);
		if (AmountCommandsExe <= 0)
		{
			OutIssuedAbility = EAbilityID::IdMove;
			ClickedLocation = RTSFunctionLibrary::GetLocationProjected(this,
			                                                           ClickedLocation, true,
			                                                           bSuccessful,
			                                                           5);
			AmountCommandsExe = MoveUnitsToLocation(ClickedLocation);
		}
		return AmountCommandsExe;
	}
	OutIssuedAbility = EAbilityID::IdMove;
	ClickedLocation = RTSFunctionLibrary::GetLocationProjected(this,
	                                                           ClickedLocation, true,
	                                                           bSuccessful,
	                                                           5);
	AmountCommandsExe = MoveUnitsToLocation(ClickedLocation);
	return AmountCommandsExe;
}

uint32 ACPPController::IssueOrderAlliedAirfield(AActor* AlliedActor, EAbilityID& OutIssuedAbility,
                                                FVector& ClickedLocation)
{
	uint32 AmountCommandsExe = 0;
	bool bSuccessful = false;
	ClickedLocation = RTSFunctionLibrary::GetLocationProjected(this,
	                                                           ClickedLocation, true,
	                                                           bSuccessful,
	                                                           5);
	// Ensures selection validity.
	TArray<AAircraftMaster*> SelectedAircraft = GetSelectedAircraft();
	ANomadicVehicle* AlliedNomadic = Cast<ANomadicVehicle>(AlliedActor);
	if (SelectedAircraft.IsEmpty() || not RTSFunctionLibrary::RTSIsValid(AlliedNomadic))
	{
		// Move to projected location; no aircraft selected.
		AmountCommandsExe = MoveUnitsToLocation(ClickedLocation);
		OutIssuedAbility = EAbilityID::IdMove;
		return AmountCommandsExe;
	}
	UAircraftOwnerComp* AircraftOwnerComp = AlliedNomadic->GetAircraftOwnerComp();
	if (not AircraftOwnerComp)
	{
		// No valid aircraft owner; move units to the structure instead.
		AmountCommandsExe = MoveUnitsToLocation(ClickedLocation);
		OutIssuedAbility = EAbilityID::IdMove;
		return AmountCommandsExe;
	}
	OutIssuedAbility = EAbilityID::IdReturnToBase;
	for (auto EachAircraft : SelectedAircraft)
	{
		if (EachAircraft->GetOwnerAircraft() == AircraftOwnerComp)
		{
			// This aircraft is already assigned at this base and will return there.
			AmountCommandsExe += EachAircraft->ReturnToBase(!bIsHoldingShift) == ECommandQueueError::NoError;
		}
		else
		{
			EachAircraft->ChangeOwnerForAircraft(AircraftOwnerComp);
			// Always executes.
			AmountCommandsExe++;
		}
	}
	return AmountCommandsExe;
}

TArray<AAircraftMaster*> ACPPController::GetSelectedAircraft()
{
	EnsureSelectionsAreRTSValid();
	TArray<AAircraftMaster*> AircraftMasters;
	for (auto EachPawn : TSelectedPawnMasters)
	{
		if (not EachPawn->IsA(AAircraftMaster::StaticClass()))
		{
			continue;
		}
		AAircraftMaster* Aircraft = Cast<AAircraftMaster>(EachPawn);
		if (not RTSFunctionLibrary::RTSIsValid(Aircraft))
		{
			continue;
		}
		AircraftMasters.Add(Aircraft);
	}
	return AircraftMasters;
}

uint32 ACPPController::IssueOrderHarvestResource(ACPPResourceMaster* Resource,
                                                 EAbilityID& OutIssuedAbility,
                                                 ECommandType& OutCommand,
                                                 const FVector& ClickedLocation)
{
	uint32 AmountCommandsExe = 0;
	OutIssuedAbility = EAbilityID::IdHarvestResource;
	OutCommand = ECommandType::HarvestResource;
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		if (EachPawn->GetIsHarvester())
		{
			AmountCommandsExe += EachPawn->HarvestResource(Resource, !bIsHoldingShift) ==
				ECommandQueueError::NoError;
		}
	}
	if (AmountCommandsExe == 0)
	{
		// No harvesters selected, move to resource instead.
		OutCommand = ECommandType::Movement;
		OutIssuedAbility = EAbilityID::IdMove;
		AmountCommandsExe = MoveUnitsToLocation(ClickedLocation);
	}
	return AmountCommandsExe;
}

uint32 ACPPController::IssueOrderPickUpItem(AItemsMaster* PickupItem, EAbilityID& OutIssuedAbility,
                                            ECommandType& OutCommand, const FVector& ClickedLocation)
{
	int32 AmountCommandsExe = 0;
	for (const auto EachSquad : TSelectedSquadControllers)
	{
		if (EachSquad->CanSquadPickupWeapon())
		{
			// only one squad will get the item.
			AmountCommandsExe += EachSquad->PickupItem(PickupItem, !bIsHoldingShift) ==
				ECommandQueueError::NoError;
			break;
		}
	}
	if (AmountCommandsExe == 0)
	{
		// No Squads that can pickup selected, move to item instead.
		OutCommand = ECommandType::Movement;
		OutIssuedAbility = EAbilityID::IdMove;
		AmountCommandsExe = MoveUnitsToLocation(ClickedLocation);
	}
	return AmountCommandsExe;
}

uint32 ACPPController::IssueOrderScavengeItem(AScavengeableObject* ScavObject,
                                              EAbilityID& OutIssuedAbility, ECommandType& OutCommand,
                                              const FVector& ClickedLocation)
{
	uint32 AmountCommandsExe = 0;
	OutIssuedAbility = EAbilityID::IdScavenge;
	for (const auto EachSquad : TSelectedSquadControllers)
	{
		if (EachSquad->GetIsScavenger())
		{
			AmountCommandsExe += EachSquad->ScavengeObject(ScavObject, !bIsHoldingShift) ==
				ECommandQueueError::NoError;
			break;
		}
	}
	if (AmountCommandsExe == 0)
	{
		// No scavengers selected, move to object instead.
		OutCommand = ECommandType::Movement;
		OutIssuedAbility = EAbilityID::IdMove;
		AmountCommandsExe = MoveUnitsToLocation(ClickedLocation);
	}
	return AmountCommandsExe;
}

void ACPPController::CreateVfxAndVoiceLineForIssuedCommand(const bool bResetAllPlacementEffects,
                                                           const uint32 CommandsExecuted,
                                                           const FVector& ClickedLocation,
                                                           const ECommandType CommandTypeIssued,
                                                           const EAbilityID AbilityActivated,
                                                           const bool bForcePlayVoiceLine)
{
	if (CommandsExecuted <= 0 || not GetIsValidCommandTypeDecoder() || not GetIsValidPlayerAudioController())
	{
		return;
	}
	M_CommandTypeDecoder->CreatePlacementEffectIfCommandExecuted(bResetAllPlacementEffects,
	                                                             CommandsExecuted,
	                                                             ClickedLocation,
	                                                             M_CommandTypeDecoder->DecodeCommandTypeIntoEffect(
		                                                             CommandTypeIssued),
	                                                             true);
	PlayVoiceLineForPrimarySelected(FRTS_VoiceLineHelpers::GetVoiceLineFromAbility(AbilityActivated),
	                                bForcePlayVoiceLine);
}


uint32 ACPPController::MoveUnitsToLocation(const FVector& MoveLocation)
{
	if (not GetIsValidFormationController())
	{
		return 0;
	}

	FRotator AfterMovementRotation = FRotator::ZeroRotator;
	uint32 AmountCommandsExe = 0;

	M_FormationController->InitiateMovement(
		MoveLocation, &TSelectedSquadControllers, &TSelectedPawnMasters, &TSelectedActorsMasters);

	const bool bArrowUsed = M_FormationController->IsPlayerRotationOverrideActive();
	// If the arrow was used then force final rotation even if the vehicle got there by reversing.
	const bool bForceFinalRotationRegardlessOfReverse = bArrowUsed;

	for (const auto EachSquad : TSelectedSquadControllers)
	{
		if (not IsValid(EachSquad->GetRTSComponent()))
		{
			RTSFunctionLibrary::ReportErrorVariableNotInitialised(
				EachSquad, "RTSComponent", "MoveUnitsToLocation", EachSquad);
			continue;
		}

		const FVector SlotLoc =
			M_FormationController->UseFormationPosition(
				EachSquad->GetRTSComponent(), AfterMovementRotation, EachSquad);

		AmountCommandsExe +=
		(EachSquad->MoveToLocation(
			SlotLoc,
			/*bSetUnitToIdle*/ !bIsHoldingShift,
			/*FinishedMovementRotation*/ AfterMovementRotation,
			/*bForceFinalRotationRegardlessOfReverse*/
			bForceFinalRotationRegardlessOfReverse) == ECommandQueueError::NoError);
	}

	for (const auto EachPawn : TSelectedPawnMasters)
	{
		if (not IsValid(EachPawn->GetRTSComponent()))
		{
			RTSFunctionLibrary::ReportErrorVariableNotInitialised(
				EachPawn, "RTSComponent", "MoveUnitsToLocation", EachPawn);
			continue;
		}

		const FVector SlotLoc =
			M_FormationController->UseFormationPosition(
				EachPawn->GetRTSComponent(), AfterMovementRotation, EachPawn);

		AmountCommandsExe +=
		(EachPawn->MoveToLocation(
			SlotLoc,
			/*bSetUnitToIdle*/ !bIsHoldingShift,
			/*FinishedMovementRotation*/ AfterMovementRotation,
			/*bForceFinalRotationRegardlessOfReverse*/
			bForceFinalRotationRegardlessOfReverse) == ECommandQueueError::NoError);
	}
	M_FormationController->OnMovementFinished();

	return AmountCommandsExe;
}

uint32 ACPPController::OrderUnitsToAttackActor(AActor* TargetActor)
{
	uint32 AmountCommandsExe = 0;
	const bool bResetCommandQueueFirst = !bIsHoldingShift;
	for (const auto EachSquad : TSelectedSquadControllers)
	{
		AmountCommandsExe += EachSquad->AttackActor(TargetActor, bResetCommandQueueFirst) ==
			ECommandQueueError::NoError;
	}
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		if (EachPawn->GetVehicleType() == OrderTypeVehicle::OTV_Armed)
		{
			AmountCommandsExe += EachPawn->AttackActor(TargetActor, bResetCommandQueueFirst) ==
				ECommandQueueError::NoError;
		}
	}
	for (const auto EachActor : TSelectedActorsMasters)
	{
		AmountCommandsExe += EachActor->AttackActor(TargetActor, bResetCommandQueueFirst) ==
			ECommandQueueError::NoError;
	}
	return AmountCommandsExe;
}

uint32 ACPPController::OrderUnitsCaptureActor(AActor* CaptureTargetActor)
{
	uint32 AmountCommandsExe = 0;

	if (!RTSFunctionLibrary::RTSIsValid(CaptureTargetActor))
	{
		RTSFunctionLibrary::ReportError(
			"OrderUnitsCaptureActor called with invalid CaptureTargetActor in ACPPController.");
		return AmountCommandsExe;
	}

	// Verify that the actor actually supports the capture interface.
	if (FCaptureMechanicHelpers::GetValidCaptureInterface(CaptureTargetActor) == nullptr)
	{
		RTSFunctionLibrary::ReportError(
			"OrderUnitsCaptureActor called on actor that does not implement CaptureInterface.");
		return AmountCommandsExe;
	}

	// Only squads are allowed to perform the capture command.
	EnsureSelectionsAreRTSValid();

	if (TSelectedSquadControllers.IsEmpty())
	{
		// No squads selected: nothing to capture. Caller can decide on fallback (e.g. plain move).
		return AmountCommandsExe;
	}

	for (ASquadController* EachSquad : TSelectedSquadControllers)
	{
		if (!IsValid(EachSquad))
		{
			continue;
		}

		// Capture command does not need to exist in any ability array; we explicitly
		// target squads here and let the squad controller handle the capture logic.
		EachSquad->CaptureActor(CaptureTargetActor, !bIsHoldingShift);
		++AmountCommandsExe;
	}

	return AmountCommandsExe;
}


uint32 ACPPController::OrderUnitsToMoveRallyPoint(const FVector& RallyPointLocation)
{
	if (not GetIsValidGameUIController())
	{
		return 0;
	}
	uint32 AmountCommandsExe = 0;
	TArray<AActor*> PrimarySameTypeUnits;
	M_GameUIController->GetPrimarySameTypeActors(PrimarySameTypeUnits);
	for (auto EachPrimary : PrimarySameTypeUnits)
	{
		if (not IsValid(EachPrimary))
		{
			continue;
		}
		UTrainerComponent* TrainerComp = EachPrimary->FindComponentByClass<UTrainerComponent>();
		if (not IsValid(TrainerComp))
		{
			continue;
		}
		TrainerComp->MoveRallyPointActor(RallyPointLocation);
		AmountCommandsExe++;
	}
	return AmountCommandsExe;
}

uint32 ACPPController::OrderUnitsAttackGround(const FVector& GroundLocation)
{
	uint32 AmountCommandsExe = 0;
	const bool bResetCommandQueueFirst = !bIsHoldingShift;
	for (const auto EachSquad : TSelectedSquadControllers)
	{
		AmountCommandsExe += EachSquad->AttackGround(GroundLocation, bResetCommandQueueFirst) ==
			ECommandQueueError::NoError;
	}
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		AmountCommandsExe += EachPawn->AttackGround(GroundLocation, bResetCommandQueueFirst) ==
			ECommandQueueError::NoError;
	}
	for (const auto EachActor : TSelectedActorsMasters)
	{
		AmountCommandsExe += EachActor->AttackGround(GroundLocation, bResetCommandQueueFirst) ==
			ECommandQueueError::NoError;
	}
	return AmountCommandsExe;
}

void ACPPController::DirectActionButtonConversion(const EAbilityID ConversionAbility)
{
	EnsureSelectionsAreRTSValid();
	uint32 commandsExe = 0;
	bool bEnable = ConversionAbility == EAbilityID::IdEnableResourceConversion;
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		EachPawn->NoQueue_SetResourceConversionEnabled(bEnable);
		commandsExe++;
	}
	for (const auto EachActor : TSelectedActorsMasters)
	{
		EachActor->NoQueue_SetResourceConversionEnabled(bEnable);
		commandsExe++;
	}
	if (commandsExe > 0)
	{
		PlayVoiceLineForPrimarySelected(FRTS_VoiceLineHelpers::GetVoiceLineFromAbility(EAbilityID::IdGeneral_Confirm),
		                                false, false);
	}
}


uint32 ACPPController::RotateUnitsToDirection(
	const FRotator& RotateToDirection)
{
	EnsureSelectionsAreRTSValid();
	uint32 commandsExe = 0;
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		commandsExe += EachPawn->RotateTowards(RotateToDirection, !bIsHoldingShift) == ECommandQueueError::NoError;
	}
	//todo implement rotation for squads.
	return commandsExe;
}

uint32 ACPPController::RotateUnitsToLocation(const FVector& RotateLocation)
{
	EnsureSelectionsAreRTSValid();
	uint32 commandsExe = 0;
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(
			EachPawn->GetActorLocation(), RotateLocation);
		commandsExe += EachPawn->RotateTowards(LookAtRotation, !bIsHoldingShift) ==
			ECommandQueueError::NoError;
	}
	return commandsExe;
}

/*--------------------------------- ACTION BUTTON ---------------------------------*/

void ACPPController::ActivateActionButton(const int32 ActionButtonAbilityIndex)
{
	const FUnitAbilityEntry ActiveAbilityEntry = M_GameUIController->GetActiveAbilityEntry(ActionButtonAbilityIndex);

	if (ActiveAbilityEntry.AbilityId == EAbilityID::IdNoAbility)
	{
		return;
	}
	EnsureSelectionsAreRTSValid();
	M_ActiveAbility = ActiveAbilityEntry.AbilityId;
	// switch to determine if ability needs to be executed immediately
	switch (M_ActiveAbility)
	{
	case EAbilityID::IdStop:
		if (DeveloperSettings::Debugging::GAction_UI_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Stop imm");
		}
		this->DirectActionButtonStop();
		break;
	case EAbilityID::IdProne:
		if (DeveloperSettings::Debugging::GAction_UI_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Prone imm");
		}
		this->DirectActionButtonProne();
		break;
	case EAbilityID::IdSwitchWeapon:
		if (DeveloperSettings::Debugging::GAction_UI_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Switch weapon imm");
		}
		this->DirectActionButtonSwitchWeapon();
		break;
	case EAbilityID::IdSwitchMelee:
		if (DeveloperSettings::Debugging::GAction_UI_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Switch Melee weapon imm");
		}
		this->DirectActionButtonSwitchMelee();
		break;
	case EAbilityID::IdCreateBuilding:
		if (DeveloperSettings::Debugging::GAction_UI_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Create building imm");
		}
		this->DirectActionButtonCreateBuilding();
		break;
	case EAbilityID::IdDigIn:
		if (DeveloperSettings::Debugging::GAction_UI_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Dig in imm");
		}
		this->DirectActionButtonDigIn();
		break;
	case EAbilityID::IdBreakCover:
		if (DeveloperSettings::Debugging::GAction_UI_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Break cover imm");
		}
		this->DirectActionButtonBreakCover();
		break;
	case EAbilityID::IdReturnCargo:
		if (DeveloperSettings::Debugging::GAction_UI_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Return cargo imm");
		}
		this->DirectActionButtonReturnCargo();
		break;
	case EAbilityID::IdFireRockets:
		if (DeveloperSettings::Debugging::GAction_UI_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Fire Rockets");
		}
		this->DirectionActionButtonFireRockets();
		break;
	case EAbilityID::IdCancelRocketFire:
		if (DeveloperSettings::Debugging::GAction_UI_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("cancel fire Rockets");
		}
		this->DirectActionButtonCancelRocketFire();
		break;
	case EAbilityID::IdReturnToBase:
		if (DeveloperSettings::Debugging::GAction_UI_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("return to base");
		}
		this->DirectActionButtonReturnToBase();
		break;
	case EAbilityID::IdApplyBehaviour:
		this->DirectActionButtonBehaviourAbility(static_cast<EBehaviourAbilityType>(ActiveAbilityEntry.CustomType));
		break;
	case EAbilityID::IdExitCargo:
		if (DeveloperSettings::Debugging::GAction_UI_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Exit cargo imm");
		}
		this->DirectActionButtonExitCargo();
		break;
	// Falls through.
	case EAbilityID::IdDisableResourceConversion:
	case EAbilityID::IdEnableResourceConversion:
		this->DirectActionButtonConversion(M_ActiveAbility);
		break;
	default:
		// execute a button on the next click
		bM_IsActionButtonActive = true;
		UpdateCursor();
	}
}

void ACPPController::UpdateCursor()
{
	if (bM_IsActionButtonActive)
	{
		// Ability requires a second click; set cursor to appropriate type
		EMouseCursor::Type CursorType = EMouseCursor::Default;

		switch (M_ActiveAbility)
		{
		case EAbilityID::IdAttack:
			CursorType = EMouseCursor::Crosshairs;
			break;
		case EAbilityID::IdMove:
			CursorType = EMouseCursor::CardinalCross;
			break;
		case EAbilityID::IdRepair:
			CursorType = EMouseCursor::Type::ResizeLeftRight;
			break;
		case EAbilityID::IdHarvestResource:
			CursorType = EMouseCursor::Type::ResizeUpDown;
			break;
		case EAbilityID::IdScavenge:
			CursorType = EMouseCursor::Type::ResizeSouthEast;
			break;
		case EAbilityID::IdPatrol:
			CursorType = EMouseCursor::Type::ResizeSouthWest;
			break;
		case EAbilityID::IdPickupItem:
			CursorType = EMouseCursor::Type::EyeDropper;
			break;
		default:
			break;
		}

		CurrentMouseCursor = CursorType;
	}
	else
	{
		// Reset cursor to default
		CurrentMouseCursor = EMouseCursor::Default;
	}
}


/*--------------------------------- ACTION BUTTON ABILITIES ---------------------------------*/

bool ACPPController::ExecuteActionButtonSecondClick(
	AActor* ClickedActor,
	FVector& ClickedLocation)
{
	// ActiveAbility is set according to the primary selected unit, using ExecuteActionButton
	/* AbilityAttackCPP Calls the AttackUnit Event for InfantryMaster units
	* Shift is handled inside the AbilityCPPs
	*/
	switch (M_ActiveAbility)
	{
	case EAbilityID::IdNoAbility:
		// Nothing to do, button not bound to any ability.
		break;
	case EAbilityID::IdMove:
		this->ActionButtonMove(ClickedLocation);
	case EAbilityID::IdReverseMove:
		this->ActionButtonReverse(ClickedLocation);
		break;
	case EAbilityID::IdAttack:
		this->ActionButtonAttack(ClickedActor, ClickedLocation);
		break;
	case EAbilityID::IdPatrol:
		this->ActionButtonPatrol(ClickedLocation);
		break;
	case EAbilityID::IdRotateTowards:
		this->ActionButtonRotateTowards(ClickedLocation);
		break;
	case EAbilityID::IdHarvestResource:
		RTSFunctionLibrary::DisplayNotification(FText::FromString("TODO harvest button implementation."));
		break;
	case EAbilityID::IdRepair:
		ActionButtonRepair(ClickedActor);
		break;
	default:
		break;
	}
	return true;
}


void ACPPController::ActionButtonAttack(AActor* ClickedActor, FVector& ClickedLocation)
{
	if (not GetIsValidCommandTypeDecoder())
	{
		return;
	}
	FTargetUnion TargetUnion;

	// Set the correct targeted actor in the union.
	ECommandType Command = M_CommandTypeDecoder->DecodeTargetedActor(ClickedActor, TargetUnion);

	if (Command == ECommandType::ScavengeObject)
	{
		// We switch the command to an attack command as the player wants to destroy an env object.
		Command = ECommandType::DestroyScavengableObject;
	}

	if (Command == ECommandType::DestructableEnvActor)
	{
		// We switch the command to an attack command as the player wants to destroy an env object.
		Command = ECommandType::DestroyDestructableEnvActor;
	}
	CheckAttackCommandForAttackGround(Command);
	EAbilityID AbilityActivated;
	const uint32 CommandsExe = IssueCommandToSelectedUnits(Command, AbilityActivated, TargetUnion, ClickedLocation,
	                                                       ClickedActor);

	// Note that we display the attack placement effect but that the targeted actor may very well be an allied
	// unit, in this case IssueCommandToSelectedUnits issues a movement command but the attack effect is displayed
	// as part of the attack button. The voice line will then still be movement as that is the activated ability!
	const bool bResetAllPlacementEffects = !bIsHoldingShift;
	constexpr bool bForcePlayVoiceLine = false;
	CreateVfxAndVoiceLineForIssuedCommand(
		bResetAllPlacementEffects, CommandsExe, ClickedLocation, ECommandType::Attack, AbilityActivated,
		bForcePlayVoiceLine);

	if (DeveloperSettings::Debugging::GPlayerClickAndAction_Compile_DebugSymbols)
	{
		DebugActorAndCommand(ClickedActor, Command);
	}
}

void ACPPController::CheckAttackCommandForAttackGround(ECommandType& OutCommandType) const
{
	switch (OutCommandType)
	{
	// IF we have a regular attack command, do not change anything.
	case ECommandType::Attack:
	case ECommandType::EnemyActor:
	case ECommandType::EnemyCharacter:
	case ECommandType::DestroyScavengableObject:
	case ECommandType::DestroyDestructableEnvActor:
		break;
	default:
		if (bIsHoldingControl)
		{
			// If CTRL is held, we convert the command to an attack ground command.
			OutCommandType = ECommandType::AttackGround;
		}
	}
}


void ACPPController::ActionButtonMove(FVector& MoveLocation)
{
	if (not GetIsValidFormationController())
	{
		return;
	}
	M_FormationController->InitiateMovement(MoveLocation, &TSelectedSquadControllers, &TSelectedPawnMasters,
	                                        &TSelectedActorsMasters);
	uint32 CommandsExe = 0;
	// Fore move to clicked location regardless of clicked actor.
	ECommandType CommandType = ECommandType::Movement;
	EAbilityID AbilityActivated;
	CommandsExe = IssueCommandToSelectedUnits(CommandType, AbilityActivated, FTargetUnion(), MoveLocation, nullptr);

	const bool bResetAllPlacementEffects = !bIsHoldingShift;
	constexpr bool bForcePlayVoiceLine = false;
	CreateVfxAndVoiceLineForIssuedCommand(
		bResetAllPlacementEffects, CommandsExe, MoveLocation, CommandType, AbilityActivated, bForcePlayVoiceLine);


	if (DeveloperSettings::Debugging::GPlayerClickAndAction_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("actionbutton Move!", FColor::Blue);
	}
}

void ACPPController::ActionButtonRotateTowards(const FVector& RotateLocation)
{
	uint32 CommandsExe = 0;

	CommandsExe = RotateUnitsToLocation(RotateLocation);
	const bool bResetAllPlacementEffects = !bIsHoldingShift;
	constexpr bool bForcePlayVoiceLine = false;
	CreateVfxAndVoiceLineForIssuedCommand(
		bResetAllPlacementEffects, CommandsExe, RotateLocation, ECommandType::RotateTowards,
		EAbilityID::IdRotateTowards, bForcePlayVoiceLine);

	if (DeveloperSettings::Debugging::GPlayerClickAndAction_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("actionbutton RotateTowards towards!", FColor::Blue);
	}
}

void ACPPController::ActionButtonRepair(AActor* ClickedActor)
{
	if (!RTSFunctionLibrary::RTSIsValid(ClickedActor))
	{
		return;
	}
	uint32 CommandsExe = IssueOrderRepairToAllSelectedUnits(ClickedActor);
	const bool bResetAllPlacementEffects = !bIsHoldingShift;
	constexpr bool bForcePlayVoiceLine = false;
	CreateVfxAndVoiceLineForIssuedCommand(
		bResetAllPlacementEffects, CommandsExe, ClickedActor->GetActorLocation(), ECommandType::Repair,
		EAbilityID::IdRepair, bForcePlayVoiceLine);
}

int32 ACPPController::IssueOrderRepairToAllSelectedUnits(AActor* ClickedActor)
{
	if (not FRTSRepairHelpers::GetIsUnitValidForRepairs(ClickedActor))
	{
		FRTSRepairHelpers::Debug_Repair("Clicked actor in cpp controler is not valid for repairs");
		return 0;
	}
	EnsureSelectionsAreRTSValid();
	uint32 CommandsExe = 0;
	const bool bResetCommandQueueFirst = !bIsHoldingShift;
	for (const auto EachSquad : TSelectedSquadControllers)
	{
		CommandsExe += EachSquad->RepairActor(ClickedActor, bResetCommandQueueFirst) == ECommandQueueError::NoError;
	}
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		CommandsExe += EachPawn->RepairActor(ClickedActor, bResetCommandQueueFirst) == ECommandQueueError::NoError;
	}
	for (const auto EachActor : TSelectedActorsMasters)
	{
		CommandsExe += EachActor->RepairActor(ClickedActor, bResetCommandQueueFirst) == ECommandQueueError::NoError;
	}
	return CommandsExe;
}

void ACPPController::ActionButtonReverse(const FVector ReverseLocation)
{
	FRotator AfterMovementRotation = FRotator::ZeroRotator;
	if (not GetIsValidFormationController())
	{
		return;
	}
	uint32 CommandsExe = 0;
	M_FormationController->InitiateMovement(ReverseLocation, &TSelectedSquadControllers, &TSelectedPawnMasters,
	                                        &TSelectedActorsMasters);
	if (bIsHoldingShift)
	{
		for (const auto EachPawn : TSelectedPawnMasters)
		{
			if (not IsValid(EachPawn->GetRTSComponent()))
			{
				RTSFunctionLibrary::ReportErrorVariableNotInitialised(EachPawn, "RTSComponent", "ActionButtonReverse",
				                                                      EachPawn);
				continue;
			}
			CommandsExe += EachPawn->ReverseUnitToLocation(
				M_FormationController->UseFormationPosition(EachPawn->GetRTSComponent(), AfterMovementRotation,
				                                            EachPawn), false
			) == ECommandQueueError::NoError;
		}
	}
	else
	{
		for (const auto EachPawn : TSelectedPawnMasters)
		{
			if (not IsValid(EachPawn->GetRTSComponent()))
			{
				RTSFunctionLibrary::ReportErrorVariableNotInitialised(EachPawn, "RTSComponent", "ActionButtonReverse",
				                                                      EachPawn);
				continue;
			}
			CommandsExe += EachPawn->
				ReverseUnitToLocation(
					M_FormationController->UseFormationPosition(EachPawn->GetRTSComponent(), AfterMovementRotation,
					                                            EachPawn),
					true) == ECommandQueueError::NoError;
		}
	}
	const bool bResetAllPlacementEffects = !bIsHoldingShift;
	constexpr bool bForcePlayVoiceLine = false;
	// Command type movement to create movement effect at reverse location.
	// Activated ability is set to reverse for proper Voice Line.
	CreateVfxAndVoiceLineForIssuedCommand(
		bResetAllPlacementEffects, CommandsExe, ReverseLocation, ECommandType::Movement,
		EAbilityID::IdReverseMove, bForcePlayVoiceLine);
	if (DeveloperSettings::Debugging::GPlayerClickAndAction_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("actionbutton Reverse move!", FColor::Blue);
	}
}


void ACPPController::ActionButtonPatrol(const FVector& ClickedLocation)
{
	FRotator AfterMovementRotation = FRotator::ZeroRotator;
	if (not GetIsValidFormationController())
	{
		return;
	}
	uint32 CommandsExe = 0;
	M_FormationController->InitiateMovement(ClickedLocation, &TSelectedSquadControllers, &TSelectedPawnMasters,
	                                        &TSelectedActorsMasters);
	for (const auto EachSquad : TSelectedSquadControllers)
	{
		if (not IsValid(EachSquad->GetRTSComponent()))
		{
			RTSFunctionLibrary::ReportErrorVariableNotInitialised(EachSquad, "RTSComponent", "ActionButtonPatrol",
			                                                      EachSquad);
			continue;
		}
		CommandsExe += EachSquad->PatrolToLocation(
			M_FormationController->UseFormationPosition(EachSquad->GetRTSComponent(), AfterMovementRotation, EachSquad),
			!bIsHoldingShift) == ECommandQueueError::NoError;
	}
	const bool bResetAllPlacementEffects = !bIsHoldingShift;
	constexpr bool bForcePlayVoiceLine = false;
	// Command type movement to create movement effect at Patrol location.
	// Activated ability is set to Movement for proper Voice Line.
	CreateVfxAndVoiceLineForIssuedCommand(
		bResetAllPlacementEffects, CommandsExe, ClickedLocation, ECommandType::Movement,
		EAbilityID::IdMove, bForcePlayVoiceLine);

	if (CommandsExe > 0)
		if (DeveloperSettings::Debugging::GPlayerClickAndAction_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("actionbutton Patrol!", FColor::Blue);
		}
}

/*--------------------------------- Action Button DIRECT ABILITIES---------------------------------*/

void ACPPController::DirectActionButtonStop()
{
	EnsureSelectionsAreRTSValid();
	for (const auto EachSquad : TSelectedSquadControllers)
	{
		EachSquad->StopCommand();
	}
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		EachPawn->StopCommand();
	}
	for (const auto EachActor : TSelectedActorsMasters)
	{
		EachActor->StopCommand();
	}
	PlayVoiceLineForPrimarySelected(FRTS_VoiceLineHelpers::GetVoiceLineFromAbility(EAbilityID::IdStop),
	                                false, true);
}

void ACPPController::DirectActionButtonDigIn()
{
	EnsureSelectionsAreRTSValid();
	int32 CommandsExe = 0;
	for (const auto EachSquad : TSelectedSquadControllers)
	{
		CommandsExe += EachSquad->DigIn(!bIsHoldingShift) == ECommandQueueError::NoError;
	}
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		CommandsExe += EachPawn->DigIn(!bIsHoldingShift) == ECommandQueueError::NoError;
	}
	if (CommandsExe > 0)
	{
		PlayVoiceLineForPrimarySelected(FRTS_VoiceLineHelpers::GetVoiceLineFromAbility(EAbilityID::IdDigIn),
		                                false);
	}
}

void ACPPController::DirectionActionButtonFireRockets()
{
	EnsureSelectionsAreRTSValid();
	int32 CommandsExe = 0;
	for (const auto EachSquad : TSelectedSquadControllers)
	{
		CommandsExe += EachSquad->FireRockets(!bIsHoldingShift) == ECommandQueueError::NoError;
	}
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		CommandsExe += EachPawn->FireRockets(!bIsHoldingShift) == ECommandQueueError::NoError;
	}
	if (CommandsExe > 0)
	{
		PlayVoiceLineForPrimarySelected(FRTS_VoiceLineHelpers::GetVoiceLineFromAbility(EAbilityID::IdFireRockets),
		                                false, true);
	}
}

void ACPPController::DirectActionButtonCancelRocketFire()
{
	EnsureSelectionsAreRTSValid();
	int32 CommandsExe = 0;
	for (const auto EachSquad : TSelectedSquadControllers)
	{
		CommandsExe += EachSquad->CancelFireRockets(!bIsHoldingShift) == ECommandQueueError::NoError;
	}
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		CommandsExe += EachPawn->CancelFireRockets(!bIsHoldingShift) == ECommandQueueError::NoError;
	}
	if (CommandsExe > 0)
	{
		PlayVoiceLineForPrimarySelected(FRTS_VoiceLineHelpers::GetVoiceLineFromAbility(EAbilityID::IdCancelRocketFire),
		                                false);
	}
}

void ACPPController::DirectActionButtonReturnToBase()
{
	EnsureSelectionsAreRTSValid();
	int32 CommandsExe = 0;
	for (auto EachPawn : TSelectedPawnMasters)
	{
		AAircraftMaster* Aircraft = Cast<AAircraftMaster>(EachPawn);
		if (not Aircraft)
		{
			continue;
		}
		CommandsExe += Aircraft->ReturnToBase(!bIsHoldingShift) == ECommandQueueError::NoError;
	}

	if (CommandsExe > 0)
	{
		PlayVoiceLineForPrimarySelected(FRTS_VoiceLineHelpers::GetVoiceLineFromAbility(EAbilityID::IdReturnToBase),
		                                false);
	}
}


void ACPPController::DirectActionButtonExitCargo()
{
	EnsureSelectionsAreRTSValid();

	int32 CommandsExe = 0;
	const bool bResetQueue = !bIsHoldingShift;

	// Exit cargo is a squad-driven action (infantry squads leaving a carrier),
	// so we issue it to selected squads.
	for (ASquadController* EachSquad : TSelectedSquadControllers)
	{
		if (!RTSFunctionLibrary::RTSIsValid(EachSquad)) { continue; }
		CommandsExe += (EachSquad->ExitCargo(bResetQueue) == ECommandQueueError::NoError);
	}

	// Optional VO feedback if something actually executed.
	if (CommandsExe > 0)
	{
		PlayVoiceLineForPrimarySelected(
			FRTS_VoiceLineHelpers::GetVoiceLineFromAbility(EAbilityID::IdExitCargo),
			false /*bForcePlay*/);
	}
}

void ACPPController::DirectActionButtonBreakCover()
{
	EnsureSelectionsAreRTSValid();
	int32 CommandsExe = 0;
	for (const auto EachSquad : TSelectedSquadControllers)
	{
		CommandsExe += EachSquad->BreakCover(!bIsHoldingShift) == ECommandQueueError::NoError;
	}
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		CommandsExe += EachPawn->BreakCover(!bIsHoldingShift) == ECommandQueueError::NoError;
	}
	if (CommandsExe > 0)
	{
		PlayVoiceLineForPrimarySelected(FRTS_VoiceLineHelpers::GetVoiceLineFromAbility(EAbilityID::IdBreakCover),
		                                false);
	}
}

void ACPPController::DirectActionButtonBehaviourAbility(const EBehaviourAbilityType Type)
{
	EnsureSelectionsAreRTSValid();
	int32 CommandsExe = 0;
	for (auto EachSquad : TSelectedSquadControllers)
	{
		CommandsExe += EachSquad->ActivateBehaviourAbility(Type, !bIsHoldingShift) == ECommandQueueError::NoError;
	}
	for (auto EachPawn : TSelectedPawnMasters)
	{
		CommandsExe += EachPawn->ActivateBehaviourAbility(Type, !bIsHoldingShift) == ECommandQueueError::NoError;
	}
	for (auto EachActor : TSelectedActorsMasters)
	{
		CommandsExe += EachActor->ActivateBehaviourAbility(Type, !bIsHoldingShift) == ECommandQueueError::NoError;
	}
	if(CommandsExe > 0)
	{
		PlayVoiceLineForPrimarySelected(FRTS_VoiceLineHelpers::GetVoiceLineFromAbility(EAbilityID::IdGeneral_Confirm),
		                                false);
		
	}
}

void ACPPController::DirectActionButtonReturnCargo()
{
	EnsureSelectionsAreRTSValid();
	int32 CommandsExe = 0;
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		if (EachPawn->GetIsHarvester())
		{
			CommandsExe += EachPawn->ReturnCargo(!bIsHoldingShift) == ECommandQueueError::NoError;
		}
	}
	if (CommandsExe > 0)
	{
		PlayVoiceLineForPrimarySelected(FRTS_VoiceLineHelpers::GetVoiceLineFromAbility(EAbilityID::IdReturnCargo),
		                                false);
	}
}

void ACPPController::DirectActionButtonProne()
{
	// todo.
}

void ACPPController::DirectActionButtonSwitchWeapon()
{
	for (auto EachSquad : TSelectedSquadControllers)
	{
		if (EachSquad->HasSquadSecondaryWeapons())
		{
			EachSquad->SwitchWeapons(!bIsHoldingShift);
		}
	}
}

void ACPPController::DirectActionButtonSwitchMelee()
{
	// if (bIsHoldingShift)
	// {
	// 	for (ACPP_UnitMaster* EachUm : TSelectedUnitMasters)
	// 	{
	// 		if (EachUm->GetRTSComponent()->GetUnitType() == EAllUnitType::UNType_Infantry)
	// 		{
	// 			EachUm->PrepareSwitchMelee(true);
	// 		}
	// 	}
	// }
	// else
	// {
	// 	for (ACPP_UnitMaster* EachUm : TSelectedUnitMasters)
	// 	{
	// 		if (EachUm->GetRTSComponent()->GetUnitType() == EAllUnitType::UNType_Infantry)
	// 		{
	// 			// make this the first ability to execute
	// 			EachUm->ResetUnitForNewCommand(true, true, true, true, true);
	// 			EachUm->PrepareSwitchMelee(true);
	// 		}
	// 	}
	// }
}

void ACPPController::DirectActionButtonSwitchMode()
{
	//todo
	// for (const auto EachUm : TSelectedUnitMasters)
	// {
	// EachUm->SwitchMode();
	// }
}

void ACPPController::DirectActionButtonCreateBuilding()
{
	const EAllUnitType ActiveUnitType = M_GameUIController->GetPrimarySelectedUnitType();
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		// Get PreviewMesh of pawn with EAllUnitType equal to active UnitType.
		if (EachPawn->GetRTSComponent()->GetUnitType() == ActiveUnitType)
		{
			ANomadicVehicle* NomadicVehicle = Cast<ANomadicVehicle>(EachPawn);
			// Note that we dont check if the truck has a building command queued, as if it does have one
			// then we can overwrite it if shift is not held on the second click.
			if (NomadicVehicle && NomadicVehicle->GetIsInTruckMode())
			{
				// If it is NOT the HQ converting to building THEN we need a build radius.
				const bool bUseBuildRadius = NomadicVehicle != M_PlayerHQ;
				StartNomadicBuildingPreview(NomadicVehicle->GetPreviewMesh(),
				                            EPlayerBuildingPreviewMode::NomadicPreviewMode,
				                            bUseBuildRadius,
				                            GetNomadicPreviewAttachments(NomadicVehicle));
				M_MainGameUI->RequestShowCancelBuilding(NomadicVehicle);
				M_NomadicVehicleSelectedForBuilding = NomadicVehicle;
				if (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
				{
					RTSFunctionLibrary::PrintString("Found truck; building mode active", FColor::Magenta);
				}
				break;
			}
		}
	}
}

bool ACPPController::TryPlaceBuilding(FVector& ClickedLocation)
{
	if (CPPConstructionPreviewRef->GetIsBuildingPreviewBlocked())
	{
		// Need a click somewhere else.
		DisplayErrorMessageCpp(FText::FromString("Cannot build here!"));
		return false;
	}
	switch (M_IsBuildingPreviewModeActive)
	{
	case EPlayerBuildingPreviewMode::NomadicPreviewMode:
		NomadicConvertToBuilding(ClickedLocation);
		break;
	case EPlayerBuildingPreviewMode::BxpPreviewMode:
		return PlaceBxpIfAsyncLoaded(ClickedLocation);
	default:
		break;
	}
	return true;
}

bool ACPPController::PlaceBxpIfAsyncLoaded(FVector& InClickedLocation)
{
	if (not RTSFunctionLibrary::RTSIsValid(M_AsyncBxpRequestState.M_Expansion))
	{
		RTSFunctionLibrary::ReportError("Failed to place building as bxp is not valid!"
			"At function TryPlaceBxp in CPPController"
			"async status: " + Global_AsyncBxpStatusToString(M_AsyncBxpRequestState.M_Status));
		return false;
	}
	if (not GetIsValidConstructionPreview() || not GetIsValidPlayerResourceManager())
	{
		return false;
	}

	const bool bIsNotPackedExpansion = not M_AsyncBxpRequestState.GetIsPackedExpansionRequest();
	if (bIsNotPackedExpansion)
	{
		if (!M_PlayerResourceManager->PayForBxp(
			M_AsyncBxpRequestState.M_Expansion->GetBuildingExpansionType()))
		{
			RTSFunctionLibrary::DisplayNotification(FText::FromString("Player cannot pay for (NEW) bxp"
				"and so will not place at PlaceBxpIfAsyncLoaded"));
			return false;
		}
	}
	// Adjust location if the bxp needs to snap to socket or origin.
	InClickedLocation = CPPConstructionPreviewRef->GetBxpPreviewLocation(InClickedLocation);
	const FRotator BxpRotation = CPPConstructionPreviewRef->GetPreviewRotation();
	const FName BxpAttachToSocketName = CPPConstructionPreviewRef->GetBxpSocketName();

	// This function has a special status update that does not only update the owner (and possibly ui) with the bxp status
	// but also with the rotation it was placed with as well as the socket name.
	M_AsyncBxpRequestState.M_Expansion->StartExpansionConstructionAtLocation(
		InClickedLocation,
		BxpRotation,
		BxpAttachToSocketName);
	// Set Bxp to null and reset the state to no async request.
	M_AsyncBxpRequestState.Reset();
	if (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
	{
		const FString PackedExpansionString = bIsNotPackedExpansion ? "WasNewBxp" : "WasUnpackedBxp";
		const FString PayState = bIsNotPackedExpansion
			                         ? "Need to pay for (NEW)"
			                         : "No payment resources check preformed (PACKED)";
		RTSFunctionLibrary::DisplayNotification(FText::FromString("As bxp: " + PackedExpansionString +
			"\n The pay state when trying to place the bpx is:" + PayState));
	}
	return true;
}

// Second click
bool ACPPController::NomadicConvertToBuilding(
	const FVector& BuildingLocation)
{
	bool bFoundTruckForConstruction = false;
	const EAllUnitType ActiveUnitType = M_GameUIController->GetPrimarySelectedUnitType();
	ANomadicVehicle* NomadicVehicle;
	const FRotator BuildingRotation = CPPConstructionPreviewRef->GetPreviewRotation();
	for (const auto EachPawn : TSelectedPawnMasters)
	{
		if (EachPawn->GetRTSComponent()->GetUnitType() == ActiveUnitType)
		{
			if (bIsHoldingShift)
			{
				// The ability is added to the queue so we need to find a truck that doesn't have a building in queue.
				// As building abilities cannot be stacked.
				if (not EachPawn->GetHasCommandInQueue(EAbilityID::IdCreateBuilding))
				{
					NomadicVehicle = Cast<ANomadicVehicle>(EachPawn);
					if (NomadicVehicle && NomadicVehicle->GetIsInTruckMode()
						// Check if the mesh of this vehicle is the same as the one we are building.
						&& NomadicVehicle->GetPreviewMesh() == CPPConstructionPreviewRef->GetPreviewMesh())
					{
						// Order the vehicle to move and build.
						NomadicVehicle->CreateBuildingAtLocation(BuildingLocation, BuildingRotation, false);
						// Create a static preview and set the reference on the vehicle.
						NomadicVehicle->SetStaticPreviewMesh(
							CPPConstructionPreviewRef->CreateStaticMeshActor(BuildingRotation, NomadicVehicle));
						bFoundTruckForConstruction = true;
						break;
					}
				}
			}
			else
			{
				// We don't add the ability to the queue but we must make sure that the selected truck is not already building something.
				NomadicVehicle = Cast<ANomadicVehicle>(EachPawn);
				if (NomadicVehicle && NomadicVehicle->GetIsInTruckMode()
					// Check if the mesh of this vehicle is the same as the one we are building.
					&& NomadicVehicle->GetPreviewMesh() == CPPConstructionPreviewRef->GetPreviewMesh())
				{
					NomadicVehicle->CreateBuildingAtLocation(BuildingLocation, BuildingRotation, true);
					// Create a static preview and set the reference on the vehicle.
					NomadicVehicle->SetStaticPreviewMesh(
						CPPConstructionPreviewRef->CreateStaticMeshActor(BuildingRotation, NomadicVehicle));
					bFoundTruckForConstruction = true;
					break;
				}
			}
		}
	}
	if (!bIsHoldingShift)
	{
		if (GetIsValidCommandTypeDecoder())
		{
			M_CommandTypeDecoder->ResetAllPlacementEffects();
		}
		if (!bFoundTruckForConstruction)
		{
			DisplayErrorMessageCpp(FText::FromString("No free truck available for construction!"));
		}
	}
	else if (!bFoundTruckForConstruction)
	{
		DisplayErrorMessageCpp(FText::FromString("Cannot stack building abilities!"));
	}
	// Either there was a valid location and an available truck was found which now has the building ability queued.
	// Or no truck was available. In Both cases we cancel the preview and deactivate building preview mode
	// as the command is propagated to a truck or cannot be executed.
	return bFoundTruckForConstruction;
}


void ACPPController::StopBuildingPreviewMode()
{
	switch (M_IsBuildingPreviewModeActive)
	{
	case EPlayerBuildingPreviewMode::BxpPreviewMode:
		// If we are previewing a bxp, make sure to cancel it and cache the packed state if we were
		// unpacking a bxp instead of clean building it so we can place the packed bxp somewhere else later.
		// Also resets the previous request and building mode.
		StopBxpPreviewPlacementResetAsyncRequest(M_AsyncBxpRequestState.M_BuildingExpansionOwner,
		                                         M_AsyncBxpRequestState.M_BxpLoadingType);
		break;
	case EPlayerBuildingPreviewMode::NomadicPreviewMode:
		StopNomadicPreviewPlacement();
		break;
	case EPlayerBuildingPreviewMode::BuildingPreviewModeOFF:
		break;
	default:
		break;
	}
}

void ACPPController::StopNomadicPreviewPlacement()
{
	if (M_NomadicVehicleSelectedForBuilding)
	{
		M_MainGameUI->RequestShowConstructBuilding(M_NomadicVehicleSelectedForBuilding);
	}
	M_NomadicVehicleSelectedForBuilding = nullptr;
	StopPreviewAndBuildingMode(false);
}

void ACPPController::StopBxpPreviewPlacementResetAsyncRequest(const TScriptInterface<IBuildingExpansionOwner>& BxpOwner,
                                                              const EAsyncBxpLoadingType BxpLoadingType)
{
	switch (M_AsyncBxpRequestState.M_Status)
	{
	case EAsyncBxpStatus::Async_NoRequest:
		{
			RTSFunctionLibrary::PrintString("ERROR: cannot StopBxpPreviewPlacement as there is no request!");
			break;
		}
	case EAsyncBxpStatus::Async_SpawnedPreview_WaitForBuildingMesh:
		{
			M_RTSAsyncSpawner->CancelActiveLoadRequest(EAsyncRequestType::AReq_Bxp);
			// This is not needed as the bxp entry is only ever initialized after the bxp is spawned.
			// if (BxpOwner)
			// {
			// 	BxpOwner->CancelBxpThatWasNotLoaded(M_AsyncBxpRequestState.ExpansionSlotIndex,
			// 	                                    bIsCancelledPackedExpansion);
			// }
			// else
			// {
			// 	RTSFunctionLibrary::ReportError(
			// 		"At StopBuildingExpansionPreviewPlacement but no valid owner! so we only cancel the load request");
			// }
		}
		break;
	case EAsyncBxpStatus::Async_BxpIsSpawned:
		if (M_AsyncBxpRequestState.M_Expansion && BxpOwner)
		{
			// Was this bxp packed? If so we save the type and set the status to IsPackedUp on the data component of the owner.
			// This makes sure we can unpack it later at a different location.
			const bool bIsPackedExpansion = M_AsyncBxpRequestState.GetIsPackedExpansionRequest();
			BxpOwner->DestroyBuildingExpansion(M_AsyncBxpRequestState.M_Expansion,
			                                   bIsPackedExpansion);
		}
		else
		{
			RTSFunctionLibrary::ReportError("At StopBuildingExpansionPreviewPlacement but no valid bxp or owner! "
				"async status: " + Global_AsyncBxpStatusToString(M_AsyncBxpRequestState.M_Status));
		}
		break;
	}
	M_AsyncBxpRequestState.Reset();
	// Only instant placement does not need to stop preview mode.
	// The rest did start it before requesting the async load and therefore needs to stop the preview mode now.
	const bool UsedPreviewMode = BxpLoadingType != EAsyncBxpLoadingType::AsyncLoadingPackedBxpInstantPlacement;
	if (UsedPreviewMode)
	{
		StopPreviewAndBuildingMode(false);
	}
	if (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
	{
		if (not UsedPreviewMode && M_IsBuildingPreviewModeActive != EPlayerBuildingPreviewMode::BuildingPreviewModeOFF)
		{
			const FString CurrentPreviewMode = Global_GetPlayerBuildingPreviewModeEnumAsString(
				M_IsBuildingPreviewModeActive);
			RTSFunctionLibrary::ReportError("We called StopBxpPreviewPlacementResetAsyncRequest but while the"
				"async BxpLoadingType was NOT using preview mode the preview mode on the player controller"
				"was on anyways (Not PreviewModeOFF) which should not be the case. \n"
				"\n Preview mode actually active: " + CurrentPreviewMode);
		}
		if (not UsedPreviewMode)
		{
			RTSFunctionLibrary::DisplayNotification(FText::FromString(
				"Called stop preview placement on a instant placement"
				"of a packed bxp, so no preview mode is stopped on the controller and Construction preview!"));
		}
	}
}

void ACPPController::CancelBuildingExpansionPreview(TScriptInterface<IBuildingExpansionOwner> BxpOwner,
                                                    const bool bIsPackedBxp)
{
	const bool bAsyncStatusIsPackedBxp = M_AsyncBxpRequestState.M_BxpLoadingType !=
		EAsyncBxpLoadingType::AsyncLoadingNewBxp;
	if (bAsyncStatusIsPackedBxp != bIsPackedBxp)
	{
		const FString MainMenuPackedString = bIsPackedBxp ? "packed" : "unpacked";
		const FString AsyncStatusLoadingString = Global_GetAsyncBxpLoadingTypeEnumAsString(
			M_AsyncBxpRequestState.M_BxpLoadingType);
		const FString AsyncBxpStatusString = Global_AsyncBxpStatusToString(M_AsyncBxpRequestState.M_Status);
		RTSFunctionLibrary::ReportError(
			"The main game UI requested to cancel the building expansion preview however the "
			"PackedBxp state of the main game UI does not match the async request state in the player controller!"
			"\n MainGameUI packed state: " + MainMenuPackedString +
			"\n Async Bxp Loading Type: " + AsyncStatusLoadingString +
			"\n Async Bxp Status: " + AsyncBxpStatusString);
	}
	StopBxpPreviewPlacementResetAsyncRequest(BxpOwner,
	                                         M_AsyncBxpRequestState.M_BxpLoadingType);
}

void ACPPController::CancelBuildingExpansionConstruction(TScriptInterface<IBuildingExpansionOwner> BxpOwner,
                                                         ABuildingExpansion* BuildingExpansion,
                                                         const bool bDestroyPackedBxp) const
{
	if (GetIsValidPlayerResourceManager() && IsValid(BuildingExpansion) && not bDestroyPackedBxp)
	{
		M_PlayerResourceManager->RefundBxp(BuildingExpansion->GetBuildingExpansionType());
	}

	if (IsValid(BuildingExpansion) && BxpOwner)
	{
		BxpOwner->DestroyBuildingExpansion(BuildingExpansion, bDestroyPackedBxp);
	}
}

void ACPPController::FocusCameraOnActor(const AActor* ActorToFocus) const
{
	if (not IsValid(CameraRef))
	{
		return;
	}
	if (IsValid(ActorToFocus))
	{
		CameraRef->SetActorLocation(ActorToFocus->GetActorLocation());
	}
}

void ACPPController::FocusCameraOnLocation(const FVector& LocationToFocus) const
{
	if (not IsValid(CameraRef) || not GetIsValidPlayerCameraController())
	{
		return;
	}
	CameraRef->SetActorLocation(LocationToFocus);
	M_PlayerCameraController->ResetCameraToBaseZoomLevel();
}

void ACPPController::FocusCameraOnStartLocation(const FVector& LocationToFocus) const
{
	if (not GetIsValidPlayerCameraController() || not IsValid(CameraRef))
	{
		return;
	}
	CameraRef->SetActorLocation(LocationToFocus);
	M_PlayerCameraController->SetCustomCameraZoomLevel(DeveloperSettings::UIUX::ZoomForPlayerStartOverview);
}

void ACPPController::InitMiniMapWithFowDataOnMainMenu(const EMiniMapStartDirection StartDirection) const
{
	if (not GetIsValidMainMenu() || not GetIsValidFowManager())
	{
		return;
	}
	M_MainGameUI->InitMiniMap(M_FowManager, StartDirection);
}

bool ACPPController::PauseGame_GetIsGameLocked() const
{
	if (M_PauseGameState.bM_IsGameLocked)
	{
		RTSFunctionLibrary::DisplayNotification(FText::FromString("The game is locked no pause or unpause possible"));
		return true;
	}
	return false;
}

void ACPPController::DebugPlayerSelection(const FString& Message, const FColor& Color) const
{
	if (DeveloperSettings::Debugging::GPlayerSelection_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message, Color);
	}
}

void ACPPController::StopPreviewAndBuildingMode(const bool bIsPlacedSuccessfully)
{
	CPPConstructionPreviewRef->StopBuildingPreview(bIsPlacedSuccessfully);
	M_IsBuildingPreviewModeActive = EPlayerBuildingPreviewMode::BuildingPreviewModeOFF;
	ShowPlayerBuildRadius(false);
	if (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Stopped Building Preview Mode", FColor::Magenta);
	}
}

bool ACPPController::GetIsValidCommandTypeDecoder()
{
	if (IsValid(M_CommandTypeDecoder))
	{
		return true;
	}
	RTSFunctionLibrary::PrintString("Invalid command type decoder.");
	M_CommandTypeDecoder = NewObject<UPlayerCommandTypeDecoder>(this, UPlayerCommandTypeDecoder::StaticClass());
	if (IsValid(M_CommandTypeDecoder))
	{
		RTSFunctionLibrary::PrintString("Repair CommandTypeDecoder in player controller was succesfull!");
		return true;
	}
	return false;
}

bool ACPPController::GetIsValidPlayerResourceManager() const
{
	if (IsValid(M_PlayerResourceManager))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_PlayerResourceManager",
	                                                      "GetIsValidPlayerResourceManager", this);
	return false;
}

bool ACPPController::GetIsValidPlayerAudioController() const
{
	if (IsValid(M_PlayerAudioController))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_PlayerAudioController",
	                                                      "GetIsValidPlayerAudioController", this);
	return false;
}

bool ACPPController::GetIsValidPlayerPortraitManager() const
{
	if (not IsValid(M_PlayerPortraitManager))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_PlayerPortraitManager",
		                                                      "GetIsValidPlayerPortraitManager", this);
		return false;
	}
	return true;
}

void ACPPController::PlayVoiceLineForPrimarySelected(const ERTSVoiceLine VoiceLineType, const bool bForcePlay,
                                                     const bool bQueueIfNotPlayed) const
{
	if (not GetIsValidGameUIController() || not GetIsValidPlayerAudioController())
	{
		RTSFunctionLibrary::ReportError("Cannot play voice lines: " + UEnum::GetValueAsString(VoiceLineType) +
			" as either ActionUIController or PlayerAudioController is not valid"
			"\n see ACPPController::PlayVoiceLine");
		return;
	}

	if (AActor* Primary = M_GameUIController->GetPrimarySelectedUnit(); RTSFunctionLibrary::RTSIsValid(Primary))
	{
		M_PlayerAudioController->PlayVoiceLine(Primary, VoiceLineType, bForcePlay, bQueueIfNotPlayed);
	}
}


bool ACPPController::GetIsValidPlayerCameraController() const
{
	if (IsValid(M_PlayerCameraController))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_PlayerCameraController",
	                                                      "GetIsvalidPlayerCameraController", this);
	return false;
}

bool ACPPController::GetIsValidPlayerBuildRadiusManager() const
{
	if (IsValid(M_PlayerBuildRadiusManager))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_PlayerBuildRadiusManager",
	                                                      "GetIsValidPlayerBuildRadiusManager", this);
	return false;
}

bool ACPPController::GetIsValidPlayerProfileLoader()
{
	if (IsValid(M_PlayerProfileLoader))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_PlayerProfileLoader",
	                                                      "GetIsValidPlayerProfileLoader", this);
	M_PlayerProfileLoader = NewObject<UPlayerProfileLoader>(this, UPlayerProfileLoader::StaticClass());
	return IsValid(M_PlayerProfileLoader);
}

bool ACPPController::GetIsValidGameUIController() const
{
	if (IsValid(M_GameUIController))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_ActionUIController",
	                                                      "GetIsValidActionUIController", this);
	return false;
}

bool ACPPController::GetIsValidAsyncSpawner() const
{
	if (IsValid(M_RTSAsyncSpawner))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_RTSAsyncSpawner",
	                                                      "GetIsValidAsyncSpawner", this);
	return false;
}

bool ACPPController::GetIsValidFormationController()
{
	if (IsValid(M_FormationController))
	{
		return true;
	}
	M_FormationController = NewObject<UFormationController>(this, UFormationController::StaticClass());
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_FormationController",
	                                                      "GetIsValidFormationController", this);
	return IsValid(M_FormationController);
}

bool ACPPController::GetIsValidPlayerControlGroupManager()
{
	if (IsValid(M_PlayerControlGroupManager))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_PlayerControlGroupManager",
	                                                      "GetIsValidPlayerControlGroupManager", this);
	M_PlayerControlGroupManager = NewObject<
		UPlayerControlGroupManager>(this, UPlayerControlGroupManager::StaticClass());
	return IsValid(M_PlayerControlGroupManager);
}

bool ACPPController::GetIsValidPlayerTechManager()
{
	if (IsValid(M_PlayerTechManager))
	{
		return true;
	}
	M_PlayerTechManager = NewObject<UPlayerTechManager>(this, UPlayerTechManager::StaticClass());
	if (IsValid(M_PlayerTechManager))
	{
		M_PlayerTechManager->InitTechsInManager(this);
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_PlayerTechManager",
	                                                      "GetIsValidPlayerTechManager", this);
	return false;
}

FNomadicPreviewAttachments ACPPController::GetNomadicPreviewAttachments(
	const ANomadicVehicle* NomadicVehicleToExpand) const
{
	FNomadicPreviewAttachments NomadicPreviewAttachments = FNomadicPreviewAttachments();
	if (not IsValid(NomadicVehicleToExpand))
	{
		return NomadicPreviewAttachments;
	}
	NomadicPreviewAttachments.BuildingRadius = NomadicVehicleToExpand->GetBuildingRadius();
	NomadicPreviewAttachments.AttachmentOffset = FVector(0, 0, 25);
	return NomadicPreviewAttachments;
}


void ACPPController::StartNomadicBuildingPreview(
	UStaticMesh* BuildingMesh,
	const EPlayerBuildingPreviewMode PreviewMode,
	const bool bUseBuildRadius,
	const FNomadicPreviewAttachments& NomadicPreviewAttachments)
{
	if (not GetIsValidConstructionPreview())
	{
		return;
	}
	if (PreviewMode != EPlayerBuildingPreviewMode::NomadicPreviewMode)
	{
		const FString PreviewModeString = Global_GetPlayerBuildingPreviewModeEnumAsString(PreviewMode);
		RTSFunctionLibrary::ReportError("Called start nomadic building preview with the wrong preview mode!"
			"\n Mode provided: " + PreviewModeString);
		return;
	}
	M_IsBuildingPreviewModeActive = PreviewMode;
	if (bUseBuildRadius)
	{
		ShowPlayerBuildRadius(true);
		// For nomadic buildings we use the player build radii manager.
		const TArray<URadiusComp*> Radii = GetIsValidPlayerBuildRadiusManager()
			                                   ? Get_NOMADIC_BuildRadiiAndMakeThemVisible()
			                                   : TArray<URadiusComp*>();

		CPPConstructionPreviewRef->StartNomadicPreview(BuildingMesh, Radii, true, NomadicPreviewAttachments);
	}
	else
	{
		CPPConstructionPreviewRef->StartNomadicPreview(BuildingMesh, {}, false);
	}
}


TArray<URadiusComp*> ACPPController::Get_NOMADIC_BuildRadiiAndMakeThemVisible() const
{
	ShowPlayerBuildRadius(true);
	TArray<URadiusComp*> Radii;
	if (GetIsValidPlayerBuildRadiusManager())
	{
		TArray<UBuildRadiusComp*> BuildRadii = M_PlayerBuildRadiusManager->GetBuildRadiiForConstruction();
		for (const auto EachRadius : BuildRadii)
		{
			Radii.Add(EachRadius);
		}
	}
	return Radii;
}

void ACPPController::StartBxpBuildingPreview(
	UStaticMesh* PreviewMesh,
	const bool bNeedBxpWithinConstructionRadius,
	const FVector& HostLocation,
	const FBxpConstructionData& BxpConstructionData)
{
	if (not GetIsValidConstructionPreview())
	{
		return;
	}
	M_IsBuildingPreviewModeActive = EPlayerBuildingPreviewMode::BxpPreviewMode;
	CPPConstructionPreviewRef->StartBxpPreview(
		PreviewMesh,
		bNeedBxpWithinConstructionRadius,
		HostLocation,
		BxpConstructionData);
}


void ACPPController::EnsureSelectionsAreRTSValid()
{
	int AmountRemovedUnits = 0;

	AActor* OldPrimary = nullptr;
	if (GetIsValidGameUIController())
	{
		OldPrimary = M_GameUIController->GetPrimarySelectedUnit();
	}

	// -------- Pawns --------
	TArray<ASelectablePawnMaster*> NewPawns;
	NewPawns.Reserve(TSelectedPawnMasters.Num());
	for (ASelectablePawnMaster* EachPawn : TSelectedPawnMasters)
	{
		const bool bActorValid = RTSFunctionLibrary::RTSIsValid(EachPawn);
		const bool bCompValid = bActorValid && IsValid(EachPawn->GetRTSComponent());
		if (bActorValid && bCompValid)
		{
			NewPawns.Add(EachPawn);
		}
		else
		{
			AmountRemovedUnits++;
			if (bActorValid) { EachPawn->SetUnitSelected(false); }
		}
	}
	TSelectedPawnMasters = MoveTemp(NewPawns);

	// -------- Squads --------
	TArray<ASquadController*> NewSquads;
	NewSquads.Reserve(TSelectedSquadControllers.Num());
	for (ASquadController* EachSquad : TSelectedSquadControllers)
	{
		const bool bActorValid = RTSFunctionLibrary::RTSIsValid(EachSquad);
		const bool bCompValid = bActorValid && IsValid(EachSquad->GetRTSComponent());
		if (bActorValid && bCompValid)
		{
			NewSquads.Add(EachSquad);
		}
		else
		{
			AmountRemovedUnits++;
			if (bActorValid) { EachSquad->SetSquadSelected(false); }
		}
	}
	TSelectedSquadControllers = MoveTemp(NewSquads);

	// -------- Actors --------
	TArray<ASelectableActorObjectsMaster*> NewActors;
	NewActors.Reserve(TSelectedActorsMasters.Num());
	for (ASelectableActorObjectsMaster* EachActor : TSelectedActorsMasters)
	{
		const bool bActorValid = RTSFunctionLibrary::RTSIsValid(EachActor);
		const bool bCompValid = bActorValid && IsValid(EachActor->GetRTSComponent());
		if (bActorValid && bCompValid)
		{
			NewActors.Add(EachActor);
		}
		else
		{
			AmountRemovedUnits++;
			if (bActorValid) { EachActor->SetUnitSelected(false); }
		}
	}
	TSelectedActorsMasters = MoveTemp(NewActors);

	if (AmountRemovedUnits > 0 && GetIsValidGameUIController())
	{
		RTSFunctionLibrary::PrintString(
			"Some selected units were removed due to being invalid!"
			"\n amount removed: " + FString::FromInt(AmountRemovedUnits));

		UpdateUIForSelectionAction(ESelectionChangeAction::UnitRemoved_RebuildUIHierarchy, nullptr);
	}
}

bool ACPPController::CheckResourcesForConstruction()
{
	// TODO
	return true;
}


void ACPPController::DebugActorAndCommand(const AActor* ClickedActor, const ECommandType Command)
{
	if (DeveloperSettings::Debugging::GPlayerClickAndAction_Compile_DebugSymbols)
	{
		FString TargetActorName = "nullptr";
		if (IsValid(ClickedActor))
		{
			TargetActorName = ClickedActor->GetName();
		}
		RTSFunctionLibrary::PrintString(
			"ActionButtonAttack command from clicked actor: " + GetStringCommandType(Command)
			+ " TargetActor: " + TargetActorName, FColor::Blue);
	}
}

bool ACPPController::SetupPlayerHQReference()
{
	// find all ANomadicVehicle actors in the world
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ANomadicVehicle::StaticClass(), FoundActors);
	for (auto EachActor : FoundActors)
	{
		if (ANomadicVehicle* NomadicVehicle = Cast<ANomadicVehicle>(EachActor))
		{
			// todo not faction agnostic!!!
			if (URTSComponent* Comp = NomadicVehicle->GetRTSComponent(); IsValid(Comp) && Comp->
				GetSubtypeAsNomadicSubtype() == ENomadicSubtype::Nomadic_GerHq)
			{
				M_PlayerHQ = NomadicVehicle;
				if (GetIsValidPlayerResourceManager())
				{
					M_PlayerResourceManager->SetupPlayerHQDropOffReference(M_PlayerHQ);
				}
				break;
			}
		}
	}
	return IsValid(M_PlayerHQ);
}

void ACPPController::ShowPlayerBuildRadius(const bool bShowRadius) const
{
	if (GetIsValidPlayerBuildRadiusManager())
	{
		M_PlayerBuildRadiusManager->ShowBuildRadius(bShowRadius);
	}
}

void ACPPController::DeactivateActionButton()
{
	bM_IsActionButtonActive = false;
	M_ActiveAbility = EAbilityID::IdNoAbility;
	UpdateCursor();
}

void ACPPController::InitFowManager()
{
	// get actor of class:
	AActor* NewManager = UGameplayStatics::GetActorOfClass(this, AFowManager::StaticClass());
	if (IsValid(NewManager))
	{
		M_FowManager = Cast<AFowManager>(NewManager);
		if (IsValid(M_FowManager))
		{
			return;
		}
	}
	RTSFunctionLibrary::ReportError("The player controller could not find a valid FOW manager!");
}

bool ACPPController::GetIsValidFowManager() const
{
	if (not IsValid(M_FowManager))
	{
		RTSFunctionLibrary::ReportError("Invalid fow manager on ACppController!");
		return false;
	}
	return true;
}

bool ACPPController::GetIsValidMainMenu() const
{
	if (not IsValid(GetMainMenuUI()))
	{
		RTSFunctionLibrary::ReportError("Invalid main menu UI on ACppController!");
		return false;
	}
	return true;
}

bool ACPPController::GetIsValidConstructionPreview() const
{
	if (not IsValid(CPPConstructionPreviewRef))
	{
		RTSFunctionLibrary::ReportError(
			"Invalid construction preview on ACppController! ");
		return false;
	}
	return true;
}

void ACPPController::UpdateHoveringActorInfo(float DeltaTime, const FVector2D CurrentMouseScreenPosition,
                                             const FHitResult& MouseHitResult, const bool bHit)
{
	if (bM_IsInTechTree)
	{
		// Do not use the widget when in tech tree.
		HideHoveringWidget();
		return;
	}
	using DeveloperSettings::UIUX::HoverTime;
	using DeveloperSettings::UIUX::HoverMouseMoveThreshold;

	// Compare current position with last frame
	const float MouseMoveDistance = FVector2D::Distance(CurrentMouseScreenPosition, M_LastFrameMousePosition);

	// Determine if mouse moved significantly
	const bool bMouseMoved = (MouseMoveDistance > HoverMouseMoveThreshold);

	// Raycast under cursor to find hovered actor
	AActor* UnderCursorActor = bHit ? MouseHitResult.GetActor() : nullptr;

	if (DeveloperSettings::Debugging::GMouseHover_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(
			FString::Printf(TEXT("UnderCursorActor: %s | MouseMoved: %s"),
			                (UnderCursorActor ? *UnderCursorActor->GetName() : TEXT("None")),
			                bMouseMoved ? TEXT("true") : TEXT("false")),
			FColor::Yellow);
	}

	if (bMouseMoved || UnderCursorActor != M_CurrentHoveredActor.Get())
	{
		// Mark possible previous actor as unhovered.
		if (M_CurrentHoveredActor.IsValid())
		{
			OnActorHovered(M_CurrentHoveredActor.Get(), false);
		}
		// If mouse moved or actor changed, reset timer and hide widget
		M_TimeWithoutMouseMovement = 0.0f;
		M_CurrentHoveredActor = UnderCursorActor;
		// Mark new actor as hovered.
		OnActorHovered(M_CurrentHoveredActor.Get(), true);
		HideHoveringWidget();
		return;
	}
	// Mouse not moved and actor is the same as before
	M_TimeWithoutMouseMovement += DeltaTime;

	if (M_TimeWithoutMouseMovement > HoverTime && M_CurrentHoveredActor.IsValid())
	{
		if (GetIsValidHoverWidget() && M_HoveringActorWidget->GetVisibility() == ESlateVisibility::Hidden)
		{
			M_HoveringActorWidget->SetHoveredActor(M_CurrentHoveredActor.Get());
			// Mark new actor as hovered.
			OnActorHovered(M_CurrentHoveredActor.Get(), true);
			// Set alignment to top-left
			M_HoveringActorWidget->SetAlignmentInViewport(FVector2D(-0.33, -0.33));
			// Position the widget at the mouse location
			M_HoveringActorWidget->SetPositionInViewport(CurrentMouseScreenPosition, false);
		}
	}
}


void ACPPController::HideHoveringWidget()
{
	if (GetIsValidHoverWidget())
	{
		M_HoveringActorWidget->HideWidget();
	}
}


bool ACPPController::GetIsValidHoverWidget()
{
	if (IsValid(M_HoveringActorWidget))
	{
		return true;
	}
	if (not IsValid(M_HoveringActorWidgetClass))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_HoveringActorWidgetClass",
		                                                      "GetIsValidHoverWidget", this);
		return false;
	}
	M_HoveringActorWidget = CreateWidget<UW_HoveringActor>(this, M_HoveringActorWidgetClass);
	if (IsValid(M_HoveringActorWidget))
	{
		M_HoveringActorWidget->AddToViewport();
		M_HoveringActorWidget->HideWidget();
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_HoveringActorWidget",
	                                                      "GetIsValidHoverWidget", this);
	return false;
}

void ACPPController::OnActorHovered(const AActor* HoveredActor, const bool bIsHovered) const
{
	if (not IsValid(HoveredActor))
	{
		return;
	}
	if (USelectionComponent* Comp = HoveredActor->FindComponentByClass<USelectionComponent>())
	{
		Comp->OnUnitHoverChange(bIsHovered);
	}
}

void ACPPController::LoadPlayerProfile(const FVector& SpawnCenterOfUnits, const bool bDoNotLoadPlayerProfileUnits)
{
	if (not GetIsValidPlayerProfileLoader())
	{
		return;
	}
	M_PlayerProfileLoader->LoadProfile(M_RTSAsyncSpawner, SpawnCenterOfUnits, this, bDoNotLoadPlayerProfileUnits);
	// // Find Nomadic HQ on the map.
	// if (not SetupPlayerHQReference())
	// {
	// 	auto RetryFindHq = [this]()
	// 	{
	// 		if (SetupPlayerHQReference())
	// 		{
	// 			GetWorld()->GetTimerManager().ClearTimer(M_HqReferenceHandle);
	// 		}
	// 	};
	// 	if (GetWorld())
	// 	{
	// 		// retry every second.
	// 		GetWorld()->GetTimerManager().SetTimer(M_HqReferenceHandle, RetryFindHq, 1.0f, true);
	// 	}
	// }
}

void ACPPController::OnPlayerProfileLoadComplete()
{
	// Has to be first; also sets reference to the HQ DropOff for the player resource manager to add the player cards'
	// resource changes.
	SetupPlayerHQReference();
	M_PlayerProfileLoadingStatus.bHasLoadedPlayerProfile = true;
	if (M_PlayerProfileLoadingStatus.bInitializeResourcesSettingsOnLoad)
	{
		// The RTS game settings were loaded before the player profile was, so we can now initialize the player resources.
		if (GetIsValidPlayerResourceManager() && M_RTSGameSettingsHandler.IsValid())
		{
			M_PlayerResourceManager->InitializeResourcesSettings(M_RTSGameSettingsHandler.Get());
		}
		else
		{
			RTSFunctionLibrary::ReportError("PlayerResourceManager or RTSGameSettingsHandler is not valid!"
				"\n at function OnPlayerProifleLoadComplete");
		}
	}
	if (M_PlayerProfileLoadingStatus.bInitializeHqResourceBonusesFromProfileCardsOnLoad)
	{
		if (not GetIsValidPlayerResourceManager())
		{
			RTSFunctionLibrary::ReportError(
				"Cannot load HQ resources after loading profile as the resource manager of the player"
				"is invalid");
			return;
		}
		M_PlayerResourceManager->OnPlayerProfileLoaded_SetupHQResourceBonuses();
	}
}

void ACPPController::InitPlayerStartLocation(const FVector& SpawnCenter, const bool bDoNotLoadPlayerProfileUnits)
{
	// Check if we do not choose the location; otherwise wait with loading profile.
	if (not GetIsGameWithChoosingStartingLocation())
	{
		StartGameAtLocation(SpawnCenter, bDoNotLoadPlayerProfileUnits);
	}
}

void ACPPController::StartGameAtLocation(const FVector& StartLocation, const bool bDoNotLoadPlayerProfileUnits = false)
{
	FocusCameraOnLocation(StartLocation);
	// Needs the async spawner reference to be set.
	LoadPlayerProfile(StartLocation, bDoNotLoadPlayerProfileUnits);
}

bool ACPPController::GetIsGameWithChoosingStartingLocation()
{
	const ARTSLandscapeDivider* LandscapeDivider = FRTS_Statics::GetRTSLandscapeDivider(this);
	bool bStartByPickingLocation = false;
	if (IsValid(LandscapeDivider))
	{
		bStartByPickingLocation = true;
	}
	if (DeveloperSettings::Debugging::GPlayerStartLocations_Compile_DebugSymbols)
	{
		const FString StartByChoosing = bStartByPickingLocation ? "Picking start location" : "Start With NO picking";
		RTSFunctionLibrary::PrintString("Game mode detected: " + StartByChoosing);
	}
	return bStartByPickingLocation;
}
