// Copyright Bas Blokzijl - All rights reserved.
#include "NomadicVehicle.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/BuildRadiusComp/BuildRadiusComp.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpOwnerComp/BuildingExpansionOwnerComp.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Player/CPPController.h"

#include "Components/AudioComponent.h"
#include "ExpansionRadius/ExpansionRadiusComp.h"
#include "NomadicAttachments/NomadicAttachmentHelpers/FNomadicAttachmentHelpers.h"
#include "RTS_Survival/Buildings/EnergyComponent/EnergyComp.h"
#include "RTS_Survival/GameUI/Healthbar/GarrisonHealthBar/W_GarrisonHealthBar.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"
#include "RTS_Survival/Player/PlayerBuildRadiusManager/PlayerBuildRadiusManager.h"
#include "RTS_Survival/Resources/ResourceDropOff/ResourceDropOff.h"
#include "RTS_Survival/Resources/ResourceStorageOwner/ResourceStorageLevel.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "Sound/SoundCue.h"
#include "RTS_Survival/RTSComponents/TimeProgressBarWidget.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/RTSComponents/NavCollision/RTSNavCollision.h"
#include "RTS_Survival/Units/Aircraft/AirBase/AircraftOwnerComp/AircraftOwnerComp.h"
#include "RTS_Survival/Units/Squads/Reinforcement/ReinforcementPoint.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

ANomadicVehicle::ANomadicVehicle(const FObjectInitializer& ObjectInitializer)
	: ATrackedTankMaster(ObjectInitializer),
	  M_PreviewMesh(NULL),
	  M_BuildingMesh(NULL),
	  M_NomadStatus(ENomadStatus::Truck),
	  M_StaticPreviewMesh(NULL),
	  M_BuildingTransform(FTransform::Identity),
	  M_ConstructionAnimationMaterial(NULL),
	  M_ConstructionMontageTime(20.f),
	  M_MeshAnimationTime(20.f),
	  M_ConstructionFrames(30.f),
	  M_ConvertToVehicleTime(20.f),
	  M_MaterialIndex(0),
	  M_AmountSmokesCovertToVehicle(0),
	  M_ConversionSmokeRadius(100.f),
	  M_DesiredTruckUIOffset(FVector::ZeroVector)
{
	M_NomadStatus = ENomadStatus::Truck;
	BuildingExpansionComponent = CreateDefaultSubobject<
		UBuildingExpansionOwnerComp>(TEXT("BuildingExpansionComponent"));
	if (!IsValid(BuildingExpansionComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "BuildingExpansionComponent", "ANomadicVehicle");
	}
}

static void DebugSocketsPreCheck(TArray<UStaticMeshSocket*> FoundSockets, TArray<FName> OccupiedSocketNames)
{
	if constexpr (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
	{
		FString Message = "Before filtering occupied sockets we have:";
		for (auto eachFound : FoundSockets)
		{
			if (not eachFound)
			{
				continue;
			}
			Message += "\n Found Socket Name: " + eachFound->SocketName.ToString();
		}
		Message += "\n\nOccupied Socket Names:";
		for (auto eachOccupied : OccupiedSocketNames)
		{
			Message += "\n Occupied Socket Name: " + eachOccupied.ToString();
		}
		RTSFunctionLibrary::PrintString(Message);
	}
}

static void DebugFreeSockets(TArray<UStaticMeshSocket*> FreeSockets)
{
	if constexpr (DeveloperSettings::Debugging::GBuilding_Mode_Compile_DebugSymbols)
	{
		FString Message = "Free sockets found: ";
		for (auto eachFree : FreeSockets)
		{
			if (not eachFree)
			{
				continue;
			}
			Message += "\n Free Socket Name: " + eachFree->SocketName.ToString();
		}
		RTSFunctionLibrary::PrintString(Message);
	}
}

void ANomadicVehicle::SetStaticPreviewMesh(AStaticPreviewMesh* NewStaticPreviewMesh)
{
	if (IsValid(M_StaticPreviewMesh))
	{
		M_StaticPreviewMesh->Destroy();
	}
	M_StaticPreviewMesh = NewStaticPreviewMesh;
}

UBuildingExpansionOwnerComp& ANomadicVehicle::GetBuildingExpansionData() const
{
	return *BuildingExpansionComponent;
}

void ANomadicVehicle::OnBuildingExpansionDestroyed(ABuildingExpansion* BuildingExpansion,
                                                   const bool bDestroyedPackedExpansion)
{
	// Updates the array and entries.
	IBuildingExpansionOwner::OnBuildingExpansionDestroyed(BuildingExpansion, bDestroyedPackedExpansion);
	FResourceConversionHelper::OnBxpCreatedOrDestroyed(this);
}

void ANomadicVehicle::NoQueue_ExecuteSetResourceConversionEnabled(const bool bEnabled)
{
	FResourceConversionHelper::ManuallySetConvertersEnabled(bEnabled, this);
}

bool ANomadicVehicle::IsBuildingAbleToExpand() const
{
	if (M_NomadStatus == ENomadStatus::Building)
	{
		return true;
	}
	return false;
}

void ANomadicVehicle::UnitDies(const ERTSDeathType DeathType)
{
	if (not IsUnitAlive())
	{
		// no double calls.
		return;
	}
	DestroyAllBuildingAttachments();
	if (GetIsValidAircraftOwnerComp())
	{
		M_AircraftOwnerComp->OnAirbaseDies();
	}

	// Calls bp event on tank of BP_OnUnitDies(); sets is alive to false.
	Super::UnitDies(DeathType);
}

EAnnouncerVoiceLineType ANomadicVehicle::OverrideAnnouncerDeathVoiceLine(
	const EAnnouncerVoiceLineType OriginalLine) const
{
	if (M_NomadStatus == ENomadStatus::Building)
	{
		return EAnnouncerVoiceLineType::LostNomadicBuilding_StaticBuilding;
	}
	return EAnnouncerVoiceLineType::LostNomadicBuilding_Truck;
}

void ANomadicVehicle::InitNomadicVehicle(
	UStaticMesh* NewPreviewMesh,
	UStaticMesh* NewBuildingMesh,
	UMaterialInstance* NewConstructionAnimationMaterial,
	TArray<UNiagaraSystem*> SmokeSystems,
	const float NewConstructionFrames,
	const float NewConstructionMontageTime,
	const float NewMeshAnimationTime,
	const float NewConvertToVehicleTime,
	const int NewAmountSmokesCovertToVehicle,
	TArray<FBuildingAttachment> NewAttachmentsToSpawn,
	TArray<FBuildingNiagaraAttachment> NewNiagaraAttachmentsToSpawn,
	TArray<FBuildingSoundAttachment> NewSoundAttachmentsToSpawn,
	TArray<FBuildingNavModifierAttachment> NewBuildingNavModifiers,
	const float NewConversionSmokeRadius,
	UTimeProgressBarWidget* NewConversionProgressBar,
	const FVector NewTruckUIOffset,
	const FSelectionDecalSettings NewSelectionDecalSettings,
	const float NewExpansionBuildRadius,
	TArray<USoundCue*> NewConstructionSounds, UStaticMeshComponent* NewBuildingMeshComponent)
{
	if (IsValid(NewPreviewMesh))
	{
		M_PreviewMesh = NewPreviewMesh;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "NewPreviewMesh", "InitNomadicVehicle");
	}

	if (IsValid(NewBuildingMeshComponent))
	{
		BuildingMeshComponent = NewBuildingMeshComponent;
		BuildingMeshComponent->SetVisibility(false);
		BuildingMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (IsValid(RTSComponent))
		{
			FRTS_CollisionSetup::SetupBuildingCollision(BuildingMeshComponent, RTSComponent->GetOwningPlayer());
		}
		else
		{
			RTSFunctionLibrary::ReportError(
				"Failed to initialize building collision settings on nomadic vehicle BuildingMeshComponent because"
				"the RTS component is not valid!"
				"\n Vehicle: " + GetName());
		}
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "NewBuildingMeshComponent", "InitNomadicVehicle");
	}


	if (IsValid(NewBuildingMesh))
	{
		M_BuildingMesh = NewBuildingMesh;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "NewBuildingMesh", "InitNomadicVehicle");
	}
	if (IsValid(NewConstructionAnimationMaterial))
	{
		M_ConstructionAnimationMaterial = NewConstructionAnimationMaterial;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "NewConstructionAnimationMaterial",
		                                                  "InitNomadicVehicle");
	}


	M_SmokeSystems = SmokeSystems;
	if (NewConstructionFrames <= 0.f)
	{
		M_ConstructionFrames = 30.f;
	}
	else
	{
		M_ConstructionFrames = NewConstructionFrames;
	}
	if (NewConstructionMontageTime <= 0.f)
	{
		M_ConstructionMontageTime = 20.f;
	}
	else
	{
		M_ConstructionMontageTime = NewConstructionMontageTime;
	}
	if (NewMeshAnimationTime <= 0.f)
	{
		M_MeshAnimationTime = 30.f;
	}
	else
	{
		M_MeshAnimationTime = NewMeshAnimationTime;
	}
	if (NewConvertToVehicleTime <= 0.f)
	{
		M_ConvertToVehicleTime = 20.f;
	}
	else
	{
		M_ConvertToVehicleTime = NewConvertToVehicleTime;
	}
	M_AmountSmokesCovertToVehicle = NewAmountSmokesCovertToVehicle;
	M_AttachmentsToSpawn = NewAttachmentsToSpawn;
	M_NiagaraAttachmentsToSpawn = NewNiagaraAttachmentsToSpawn;
	M_SoundAttachmentsToSpawn = NewSoundAttachmentsToSpawn;
	if (NewConversionSmokeRadius <= 0.f)
	{
		M_ConversionSmokeRadius = 100.f;
	}
	else
	{
		M_ConversionSmokeRadius = NewConversionSmokeRadius;
	}
	M_ConversionProgressBar = NewConversionProgressBar;
	M_DesiredTruckUIOffset = NewTruckUIOffset;
	M_SelectionDecalSettings = NewSelectionDecalSettings;
	M_ExpansionBuildRadius = NewExpansionBuildRadius;
	M_NavModifierAttachmentsToSpawn = NewBuildingNavModifiers;
	for (auto EachNewConstructionSound : NewConstructionSounds)
	{
		if (IsValid(EachNewConstructionSound))
		{
			M_ConstructionSounds.Add(EachNewConstructionSound);
		}
	}
}

void ANomadicVehicle::BeginPlay()
{
	Super::BeginPlay();
	if (IsValid(M_RadiusComp))
	{
		if (UPlayerBuildRadiusManager* PlayerBuildRadiusManager = FRTS_Statics::GetPlayerBuildRadiusManager(this))
		{
			PlayerBuildRadiusManager->RegisterBuildRadiusComponent(M_RadiusComp);
		}
		else
		{
			RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "PlayerBuildRadiusManager",
			                                                      "ANomadicVehicle::BeginPlay", this);
		}
	}
	if (bStartAsConvertedToBuilding)
	{
		StartAsConvertedBuilding();
	}
	if (IsValid(M_CargoComp))
	{
		// Hide the cargo UI before it is setup (cargo UI is setup after we convert to building)
		HideCargoUI();
	}
}

void ANomadicVehicle::BeginDestroy()
{
	if (IsValid(M_RadiusComp))
	{
		if (UPlayerBuildRadiusManager* PlayerBuildRadiusManager = FRTS_Statics::GetPlayerBuildRadiusManager(this))
		{
			PlayerBuildRadiusManager->UnregisterBuildRadiusComponent(M_RadiusComp);
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MaterialReapplyTimerHandle);
		World->GetTimerManager().ClearTimer(ConvertToVehicleTimerHandle);
	}
	DestroyAllBuildingAttachments();
	Super::BeginDestroy();
}

void ANomadicVehicle::PostInitializeComponents()
{
	// Inits components
	Super::PostInitializeComponents();
	// find if we have a UBuildRadiusComp component.
	if (UBuildRadiusComp* RadiusComp = FindComponentByClass<UBuildRadiusComp>())
	{
		M_RadiusComp = RadiusComp;
	}
	if (UCargo* CargoComp = FindComponentByClass<UCargo>())
	{
		M_CargoComp = CargoComp;
	}
	if (UEnergyComp* EnergyComp = FindComponentByClass<UEnergyComp>())
	{
		M_EnergyComp = EnergyComp;
	}
	if (UResourceDropOff* ResourceDropOff = FindComponentByClass<UResourceDropOff>())
	{
		M_ResourceDropOff = ResourceDropOff;
		// Start inactive as a truck cannot have resources being dropped off.
		M_ResourceDropOff->SetIsDropOffActive(false);
	}
	// Discover optional AircraftOwner component on this vehicle BP
	if (UAircraftOwnerComp* Found = FindComponentByClass<UAircraftOwnerComp>())
	{
		M_AircraftOwnerComp = Found;
	}
	if (UTrainerComponent* TrainerComp = FindComponentByClass<UTrainerComponent>())
	{
		M_TrainerComponent = TrainerComp;
	}
	if(UReinforcementPoint* ReinforcementPointComp = FindComponentByClass<UReinforcementPoint>())
	{
		M_ReinforcementPoint = ReinforcementPointComp;
	}
}

void ANomadicVehicle::SetupMountCollision(UMeshComponent* MountMesh)
{
	bool bOwnedByPlayer1 = false;
	if (GetIsValidRTSComponent())
	{
		bOwnedByPlayer1 = RTSComponent->GetOwningPlayer() == 1;
	}
	if (EnsureRTSOverlapEvasionComponent())
	{
		RTSOverlapEvasionComponent->TrackOverlapMeshOfOwner(this, MountMesh);
	}
	FRTS_CollisionSetup::SetupCollisionForNomadicMount(MountMesh, bOwnedByPlayer1);
}

void ANomadicVehicle::ExecuteMoveCommand(const FVector MoveToLocation)
{
	if (M_NomadStatus == ENomadStatus::Truck)
	{
		// Executes move command in bp and sets gears in ChaosVehicle.
		// Sets turrets to autoEngage enemies in TankMaster.
		Super::ExecuteMoveCommand(MoveToLocation);
	}
	else
	{
		// todo warning unit not able to move.
		DoneExecutingCommand(EAbilityID::IdMove);
	}
}

void ANomadicVehicle::OnUnitIdleAndNoNewCommands()
{
	// Manual broadcast instead of using super call as tank master does the rotation logic differently and calling super
	// would result in conflicting rotation logic.
	OnUnitIdleAndNoNewCommandsDelegate.Broadcast();
	if (not GetIsSpawning())
	{
		if (M_NomadStatus == ENomadStatus::Truck)
		{
			// Only affect navmesh if we are a truck.
			if (IsValid(RTSNavCollision))
			{
				RTSNavCollision->EnableAffectNavmesh(true);
			}
			else
			{
				RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "RTSNavCollision",
				                                                      "ANomadicVehicle::OnUnitIdleAndNoNewCommands",
				                                                      this);
			}
		}
	}
	// do the tank master logic by hand as we do not want to call super due to the nomad status related logic.
	const bool bAlwaysFinalRotate = GetForceFinalRotationRegardlessOfReverse();
	const bool bUseFinalRotation = (not bWasLastMovementReverse || bAlwaysFinalRotate);
	if (bUseFinalRotation && M_NomadStatus == ENomadStatus::Truck)
	{
		// We finished our chain of commands and ended in a non-reverse movement; rotate towards the movement direction.
		RotateTowardsFinalMovementRotation();
		return;
	}
	// In this case we have finished the chain of commands with possibly a movement command at the end, however,
	// this command ended in a reverse movement and thus we do not want the vehicle to rotate; reset.
	ResetRotateTowardsFinalMovementRotation();
}

void ANomadicVehicle::TerminateMoveCommand()
{
	Super::TerminateMoveCommand();
}


void ANomadicVehicle::ExecuteCreateBuildingCommand(const FVector BuildingLocation, const FRotator BuildingRotation)
{
	if (GetIsValidRTSNavCollision())
	{
		RTSNavCollision->EnableAffectNavmesh(false);
	}
	CreateBuildingAtLocationBP(BuildingLocation, BuildingRotation);
}

UEnergyComp* ANomadicVehicle::GetEnergyComp() const
{
	return M_EnergyComp;
}

float ANomadicVehicle::GetBuildingRadius() const
{
	if (IsValid(M_RadiusComp))
	{
		return M_RadiusComp->GetRadius();
	}
	return 0.f;
}

// Step 1
void ANomadicVehicle::StartBuildingConstruction()
{
	if (IsValid(M_StaticPreviewMesh) && IsValid(AINomadicVehicle) && IsValid(ChassisMesh))
	{
		OnNomadConvertToBuilding.Broadcast();
		M_NomadStatus = ENomadStatus::CreatingBuildingRotating;
		// Save the preview transform.
		M_BuildingTransform = M_StaticPreviewMesh->GetTransform();
		// Destroy after start mesh animation as we need the preview mesh to block other buildings.
		M_StaticPreviewMesh->SetActorHiddenInGame(true);
		AINomadicVehicle->StopBehaviourTree();
		// teleport to the building location.
		FHitResult HitResult;
		ChassisMesh->SetWorldLocation(M_BuildingTransform.GetLocation(), false, &HitResult,
		                              ETeleportType::TeleportPhysics);
		// Standalone rotation without command queue.
		ExecuteRotateTowardsCommand(M_BuildingTransform.Rotator(), false);
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			this,
			"M_StaticPreviewMesh, AINomadicVehicle or ChassisMesh",
			"ANomadicVehicle::StartBuildingConstruction");
	}
}

float ANomadicVehicle::GetBxpExpansionRange() const
{
	return M_ExpansionBuildRadius;
}

FVector ANomadicVehicle::GetBxpOwnerLocation() const
{
	return GetActorLocation();
}

// Step 2
void ANomadicVehicle::OnFinishedStandaloneRotation()
{
	if (IsValid(M_ConversionProgressBar))
	{
		RTSFunctionLibrary::PrintString("Finished standalone rotation");
		M_NomadStatus = ENomadStatus::CreatingBuildingTruckAnim;
		M_ConversionProgressBar->StartProgressBar(GetTotalConstructionTime());

		CreateSmokeForVehicleConversion();

		// Prevents any further commands from being executed called with true.
		// See OnBuildingFinished.
		SetCommandQueueEnabled(false);
		// Start the full timer of montage and building animation.
		// Propagate start building to blueprint.
		BeginBuildingTruckAnimationMontage();
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			this,
			"M_ConversionProgressBar",
			"ANomadicVehicle::OnFinishedStandaloneRotation");
	}
}


// Step 3
void ANomadicVehicle::OnTruckMontageFinished()
{
	const UWorld* World = GetWorld();
	if (World && IsValid(BuildingMeshComponent))
	{
		// Hide the vehicle mesh
		SetDisableChaosVehicleMesh(true);
		M_NomadStatus = ENomadStatus::CreatingBuildingMeshAnim;

		// Set transform of the building mesh to the transform of the preview mesh.
		// BuildingMeshComponent->SetWorldTransform(m_BuildingTransform);
		BuildingMeshComponent->SetWorldRotation(M_BuildingTransform.Rotator());

		// Cleanup preview mesh as now the collision of the building mesh will take over.
		if (IsValid(M_StaticPreviewMesh))
		{
			M_StaticPreviewMesh->Destroy();
		}

		// Show the building mesh.
		BuildingMeshComponent->SetStaticMesh(M_BuildingMesh);
		BuildingMeshComponent->SetVisibility(true);
		BuildingMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

		// Offset the truck UI.
		MoveTruckUIWithLocalOffsets(true);

		// Cache original materials
		CacheOriginalMaterials();
		// Init smoke locations for material animation.
		InitSmokeLocations();

		// Set decal to buildig mode
		AdjustSelectionDecalToConversion(true);

		// apply construction materials and calculate amount materials to exclude.
		const uint32 AmountMaterialsToExclude = ApplyConstructionMaterial(false);
		RTSFunctionLibrary::PrintString(
			"\n\nMaterials to exclude::" + FString::FromInt(AmountMaterialsToExclude) + "\n\n");

		// Start timer to reapply original materials
		const float Interval = M_MeshAnimationTime / (M_CachedOriginalMaterials.Num() - AmountMaterialsToExclude);
		SetAnimationTimer(Interval);
		BPOnStartMeshAnimation();
	}
}

// Step 4
void ANomadicVehicle::OnFinishedConvertingToBuilding()
{
	AnnounceConversion();
	FResourceConversionHelper::OnNomadicExpanded(this, true);
	OnConvertToBuilding_PlacePackedBxps();
	OnFinishedConvertToBuilding_HandleInstanceSpecificComponents();

	CreateBuildingAttachments();
	// Set max health to building health.
	AdjustMaxHealthForConversion(false);
	// Restore any resource visualisation of resources stored in the DropOff Component.
	RestoreResourceStorageVisualisation();
	// Initialize airbase if present.
	OnFinishedConvertToBuilding_UnpackAirbase();
	if (IsValid(M_ConversionProgressBar) && IsValid(PlayerController))
	{
		M_NomadStatus = ENomadStatus::Building;
		// Stop before the training component uses the same progress bar.
		M_ConversionProgressBar->StopProgressBar();
		PlayerController->TruckConverted(this, true);
	}
	else
	{
		RTSFunctionLibrary::ReportError("M_ConversionProgressBar or PlayerController is null!"
			"\n In function: ANomadicVehicle::OnBuildingFinished"
			"For nomadic vehicle: " + GetName());
	}
	// IMPORTANT: set after status change to building!!
	// Open command queue by calling cancel.
	SetCommandQueueEnabled(true);
	// Propagate to blueprints.
	BP_OnFinishedConvertingToBuilding();
}

void ANomadicVehicle::OnFinishedConvertToBuilding_HandleInstanceSpecificComponents()
{
	// This evaluating to false is not an error as not all nomadic vehicles have a build radius component.
	SetRadiusComponentActive(true);
	SetReinforcementPointActive(true);
	SetEnergyComponentActive(true);
	// Allow Resources to be dropped off now.
	SetResourceDropOffActive(true);
	// If we have a trainer comp, set the training to be enabled.
	// This component will now use the progress bar.
	SetTrainingEnabled(true);
	SetCargoEnabled(true);
}

void ANomadicVehicle::OnFinishedConvertToBuilding_UnpackAirbase()
{
	if (GetIsValidAircraftOwnerComp() && IsValid(BuildingMeshComponent) && GetIsValidRTSComponent())
	{
		bool bValidData = false;
		const FNomadicData NomadicData = FRTS_Statics::BP_GetNomadicDataOfPlayer(
			RTSComponent->GetOwningPlayer(), RTSComponent->GetSubtypeAsNomadicSubtype(), this, bValidData);
		if (not bValidData)
		{
			RTSFunctionLibrary::ReportWarning(
				"FAILED to get valid nomadic data in ANomadicVehicle::OnFinishedConvertingToBuilding");
		}
		M_AircraftOwnerComp->UnpackAirbase(BuildingMeshComponent, NomadicData.RepairPowerMlt);
	}
}


void ANomadicVehicle::TerminateCreateBuildingCommand()
{
	Super::TerminateCreateBuildingCommand();
	if (IsValid(PlayerController) && IsValid(M_ConversionProgressBar) && IsValid(AINomadicVehicle))
	{
		M_ConversionProgressBar->StopProgressBar();
		switch (M_NomadStatus)
		{
		case ENomadStatus::Truck:
			RTSFunctionLibrary::PrintString("terminate building command as truck");
			AINomadicVehicle->StopBehaviourTree();
			if (M_StaticPreviewMesh)
			{
				M_StaticPreviewMesh->SetActorEnableCollision(false);
				M_StaticPreviewMesh->SetActorHiddenInGame(true);
				M_StaticPreviewMesh->Destroy();
				M_StaticPreviewMesh = nullptr;
			}
			break;
		case ENomadStatus::CreatingBuildingRotating:
			// Note that BT and static preview are already stopped/destroyed.
			StopRotating();
			RTSFunctionLibrary::PrintString("terminate building command as ROTATING creating building");
			M_NomadStatus = ENomadStatus::Truck;
			break;
		case ENomadStatus::CreatingBuildingTruckAnim:
			BPStopTruckAnimationMontage();
			RTSFunctionLibrary::PrintString("terminate building command as MONTAGE");
			M_NomadStatus = ENomadStatus::Truck;
			break;
		case ENomadStatus::CreatingBuildingMeshAnim:
			RTSFunctionLibrary::PrintString("terminate building command as MESH ANIMATION");
			CancelBuildingMeshAnimation();
		// Set decal to truck mode
			AdjustSelectionDecalToConversion(false);
			M_NomadStatus = ENomadStatus::Truck;
			break;
		case ENomadStatus::Building:
			break;
		default:
			RTSFunctionLibrary::PrintString("cancel building command but is not building!");
		}

		// Only show conversion to construct building if the vehicle did cancel the building command
		// NOT if the vehicle ended the construction command and is now a building.
		if (PlayerController && PlayerController->GetMainMenuUI() && M_NomadStatus == ENomadStatus::Truck)
		{
			// Only show the construct building button if this unit is the primary selected unit.
			PlayerController->GetMainMenuUI()->RequestShowConstructBuilding(this);
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError("M_ConversionProgressBar, PlayerController or AINomadicVehicle is null!"
			"\n In function: ANomadicVehicle::TerminateCreateBuildingCommand"
			"For nomadic vehicle: " + GetName());
	}
}

void ANomadicVehicle::ExecuteConvertToVehicleCommand()
{
	if (not IsValid(M_ConversionProgressBar) || not IsValid(PlayerController))
	{
		RTSFunctionLibrary::ReportError("M_ConversionProgressBar or PlayerController is null!"
			"\n In function: ANomadicVehicle::ExecuteConvertToVehicleCommand"
			"For nomadic vehicle: " + GetName());
		return;
	}
	M_NomadStatus = ENomadStatus::CreatingTruck;
	FResourceConversionHelper::OnNomadicExpanded(this, false);
	OnStartConvertToVehicle_HandleInstanceSpecificComponents();

	OnNomadConvertToVehicle.Broadcast();

	// Starts animations for packing up expansions.
	StartPackUpAllExpansions(M_ConvertToVehicleTime);

	// Prevents any further commands from being added to the queue until called with true.
	// See finished converting to vehicle.
	SetCommandQueueEnabled(false);


	if (UseTrainingPreview())
	{
		M_TrainingPreviewHelper.HandleTrainingDisabledOrNomadTruck();
	}

	// Save the building materials to reapply to the mesh after deconstruction is complete.
	CacheOriginalMaterials();

	CreateSmokeForVehicleConversion();
	DestroyAllBuildingAttachments();

	// check if smoke systems are initiated
	uint8 ZeroCounter = 0;
	for (auto SmokeLocation : M_CreateSmokeTransforms)
	{
		if (SmokeLocation.Equals(FTransform::Identity))
		{
			ZeroCounter++;
		}
		if (ZeroCounter > 1)
		{
			// Init smoke locations for material animation.
			InitSmokeLocations();
			break;
		}
	}
	// don't apply construction materials but only calculate the amount of materials to exclude.
	const uint32 AmountMaterialsToExclude = ApplyConstructionMaterial(true);
	RTSFunctionLibrary::PrintString(
		"\n\nMaterials to exclude::" + FString::FromInt(AmountMaterialsToExclude) + "\n\n");

	// We now reapply construction materials from the top of the array.
	M_MaterialIndex = M_CachedOriginalMaterials.Num() - 1;

	// After training component stopped using it.
	M_ConversionProgressBar->StartProgressBar(M_ConvertToVehicleTime);

	// Start timer to reapply construction materials, m_NomadStatus is used to determine if we are creating the truck or the building.
	const float Interval = M_ConvertToVehicleTime / (M_CachedOriginalMaterials.Num() - AmountMaterialsToExclude);
	SetAnimationTimer(Interval);
	if (PlayerController->GetMainMenuUI())
	{
		PlayerController->GetMainMenuUI()->RequestShowCancelVehicleConversion(this);
	}
	// Propagate to blueprints.
	BP_OnStartedConvertingToVehicle();
}

void ANomadicVehicle::TerminateConvertToVehicleCommand()
{
	if (not IsValid(PlayerController) || not IsValid(M_ConversionProgressBar))
	{
		RTSFunctionLibrary::ReportError("M_ConversionProgressBar or PlayerController is null!"
			"\n In function: ANomadicVehicle::TerminateConvertToVehicleCommand"
			"For nomadic vehicle: " + GetName());
	}
	if (M_NomadStatus != ENomadStatus::CreatingTruck)
	{
		return;
	}
	OnCancelledConvertToVehicle_HandleInstanceSpecificComponents();

	if (GetIsValidAircraftOwnerComp())
	{
		M_AircraftOwnerComp->OnPackUpAirbaseCancelled();
	}

	M_NomadStatus = ENomadStatus::Building;
	M_ConversionProgressBar->StopProgressBar();
	const UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(MaterialReapplyTimerHandle);
	}
	// set building materials to original materials before construction materials were applied.
	SetAllBuildingMaterialsToCache();
	// Reset index and cache.
	ResetCachedMaterials();

	if (PlayerController->GetMainMenuUI())
	{
		// Only show the convert to vehicle button if this unit is the primary selected unit.
		PlayerController->GetMainMenuUI()->RequestShowConvertToVehicle(this);
	}
	// Propagate to possible bxps to cancel the packing.
	CancelPackUpExpansions();
	FResourceConversionHelper::OnNomadicExpanded(this, true);
	// Create the building attachments again.
	CreateBuildingAttachments();

	SetCommandQueueEnabled(true);
	// Propagate to Blueprints.
	BP_OnCancelledConvertingToVehicle();
}

void ANomadicVehicle::CreateSmokeForVehicleConversion() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Center = GetActorLocation();
	const FVector BaseScale = FVector(1.f);
	const float Z = Center.Z; // Use the Z coordinate of the actor's location for all smoke effects

	for (int32 i = 0; i < M_AmountSmokesCovertToVehicle; ++i)
	{
		// Calculate angle for this smoke effect
		const float Angle = (360.f / M_AmountSmokesCovertToVehicle) * i;
		const float Radians = FMath::DegreesToRadians(Angle);

		// Calculate x and y position for smoke effect in the circle
		const float X = Center.X + M_ConversionSmokeRadius * FMath::Cos(Radians);
		const float Y = Center.Y + M_ConversionSmokeRadius * FMath::Sin(Radians);
		const FVector SmokeLocation = FVector(X, Y, Z);
		// 20% variability in scale.
		const FVector SmokeScale = BaseScale + FVector(FMath::RandRange(-0.2f, 0.2f),
		                                               FMath::RandRange(-0.2f, 0.2f),
		                                               FMath::RandRange(-0.2f, 0.2f));

		// Spawn smoke effect at this location
		CreateRandomSmokeSystemAtTransform(FTransform(FRotator::ZeroRotator, SmokeLocation, SmokeScale));
	}
}

void ANomadicVehicle::SetAllBuildingMaterialsToCache()
{
	if (IsValid(BuildingMeshComponent))
	{
		for (int Index = 0; Index < M_CachedOriginalMaterials.Num(); ++Index)
		{
			BuildingMeshComponent->SetMaterial(Index, M_CachedOriginalMaterials[Index]);
		}
	}
}

void ANomadicVehicle::SetAnimationTimer(const float Interval)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MaterialReapplyTimerHandle);
		World->GetTimerManager().SetTimer(MaterialReapplyTimerHandle, this, &ANomadicVehicle::ReapplyOriginalMaterial,
		                                  Interval, true);
	}
}

void ANomadicVehicle::MoveTruckUIWithLocalOffsets(const bool bMoveToBuildingPosition)
{
	if (IsValid(M_ConversionProgressBar) && IsValid(HealthComponent))
	{
		if (M_DesiredTruckUIOffset.Equals(FVector::ZeroVector))
		{
			return;
		}
		if (bMoveToBuildingPosition)
		{
			// Calculate the offset to apply to the UI components based on the building mesh
			const FVector DesiredLocation = BuildingMeshComponent->GetRelativeLocation() + M_DesiredTruckUIOffset;
			// Save components original locations
			M_TruckUIElementLocations.Key = HealthComponent->GetLocalLocation();
			M_TruckUIElementLocations.Value = M_ConversionProgressBar->GetLocalLocation();
			const float ZDifference = M_TruckUIElementLocations.Key.Z - M_TruckUIElementLocations.Value.Z;
			HealthComponent->SetLocalLocation(DesiredLocation);
			M_ConversionProgressBar->SetLocalLocation(DesiredLocation + FVector(0.f, 0.f, ZDifference));
		}
		else
		{
			HealthComponent->SetLocalLocation(M_TruckUIElementLocations.Key);
			M_ConversionProgressBar->SetLocalLocation(M_TruckUIElementLocations.Value);
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError("M_ConversionProgressBar or HealthComponent is null!"
			"\n In function: ANomadicVehicle::MoveTruckUIWithLocalOffsets"
			"For nomadic vehicle: " + GetName());
	}
}

void ANomadicVehicle::OnConvertToBuilding_PlacePackedBxps()
{
	TScriptInterface<IBuildingExpansionOwner> BxpOwnerSmartPointer = this;
	if (not BxpOwnerSmartPointer || not GetIsValidPlayerControler())
	{
		return;
	}
	// Uses the async spawner to batch load all of the building expansions that are packed and attached.
	BxpOwnerSmartPointer->BatchAsyncLoadAttachedPackedBxps(
		PlayerController,
		BxpOwnerSmartPointer);
}

void ANomadicVehicle::HideCargoUI()
{
	if (not GetIsValidHealthComponent())
	{
		return;
	}
	TWeakObjectPtr<ANomadicVehicle> WeakThis(this);
	auto OnHealthUILoaded = [WeakThis](UHealthComponent* HealthComp)
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		UWidgetComponent* WidgetComp = HealthComp->GetHealthBarWidgetComp();
		if (not IsValid(WidgetComp))
		{
			RTSFunctionLibrary::ReportErrorVariableNotInitialised(WeakThis.Get(),
			                                                      "HealthComponent->GetHealthBarWidgetComp()",
			                                                      "ANomadicVehicle::HideCargoUI", WeakThis.Get());
			return;
		}
		if (UUserWidget* UserWidget = WidgetComp->GetUserWidgetObject())
		{
			if (UW_GarrisonHealthBar* CargoWidget = Cast<UW_GarrisonHealthBar>(UserWidget))
			{
				CargoWidget->ForceHideCargoUI();
			}
		}
	};
	HealthComponent->M_HealthBarWidgetCallbacks.CallbackOnHealthBarReady(
		OnHealthUILoaded, this);
}

void ANomadicVehicle::AdjustSelectionDecalToConversion(const bool bSetToBuildingPosition) const
{
	if (IsValid(SelectionComponent))
	{
		const TPair<UMaterialInterface*, UMaterialInterface*> Materials = SelectionComponent->GetMaterials();
		// Update with the other set of matierals.
		SelectionComponent->UpdateSelectionMaterials(M_SelectionDecalSettings.State2_SelectionDecalMat,
		                                             M_SelectionDecalSettings.Sate2_DeselectionDecalMat);
		// Save the previous set of materials.
		M_SelectionDecalSettings.State2_SelectionDecalMat = Materials.Key;
		M_SelectionDecalSettings.Sate2_DeselectionDecalMat = Materials.Value;
		if (bSetToBuildingPosition)
		{
			SelectionComponent->UpdateDecalScale(M_SelectionDecalSettings.State2_SelectionDecalSize);
			SelectionComponent->SetDecalRelativeLocation(M_SelectionDecalSettings.State2_DecalPosition);
			SelectionComponent->UpdateSelectionArea(
				M_SelectionDecalSettings.State2_AreaSize,
				M_SelectionDecalSettings.State2_AreaPosition);
		}
		else
		{
			SelectionComponent->UpdateDecalScale(M_SelectionDecalSettings.State1_SelectionDecalSize);
			SelectionComponent->SetDecalRelativeLocation(M_SelectionDecalSettings.State1_DecalPosition);
			SelectionComponent->UpdateSelectionArea(
				M_SelectionDecalSettings.State1_AreaSize,
				M_SelectionDecalSettings.State1_AreaPosition);
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError("SelectionComponent is null!"
			"\n In function: ANomadicVehicle::AdjustSelectionDecalToConversion"
			"For nomadic vehicle: " + GetName());
	}
}

void ANomadicVehicle::OnStartConvertToVehicle_HandleInstanceSpecificComponents()
{
	// Disable Training if this nomadic vehicle has a training component.
	// This component will now no longer use the progress bar; make sure this happens before the bar is set
	// for the conversion by the vehicle.
	SetTrainingEnabled(false);
	SetCargoEnabled(false);
	SetReinforcementPointActive(false);
	// If we have an airbase owner and aircraft are inside, force them to takeoff first.
	if (GetIsValidAircraftOwnerComp())
	{
		M_AircraftOwnerComp->OnPackUpAirbaseStart(M_ConstructionAnimationMaterial);
	}

	// Disable DropOff of resources.
	SetResourceDropOffActive(false);
}

void ANomadicVehicle::OnCancelledConvertToVehicle_HandleInstanceSpecificComponents()
{
	SetTrainingEnabled(true);
	SetCargoEnabled(true);
	SetReinforcementPointActive(true);
	SetResourceDropOffActive(true);
}

// Converted to vehicle
void ANomadicVehicle::OnFinishedConvertingToVehicle()
{
	FResourceConversionHelper::OnNomadicExpanded(this, false);

	if (not IsValid(M_ConversionProgressBar) || not IsValid(PlayerController) || not IsValid(BuildingMeshComponent))
	{
		RTSFunctionLibrary::ReportError("M_ConversionProgressBar, PlayerController or BuildingMeshComponent is null!"
			"\n In function: ANomadicVehicle::OnFinishedConvertingToVehicle"
			"For nomadic vehicle: " + GetName());
		return;
	}

	M_NomadStatus = ENomadStatus::Truck;
	// Pack up all building expansions.
	FinishPackUpAllExpansions();
	OnFinishedConvertingToVehicle_HandleInstanceSpecificComponents();

	M_ConversionProgressBar->StopProgressBar();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MaterialReapplyTimerHandle);
	}
	// Show the vehicle mesh
	SetDisableChaosVehicleMesh(false);

	// hide the building mesh.
	BuildingMeshComponent->SetVisibility(false);
	BuildingMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// set building materials to original materials before construction materials were applied.
	SetAllBuildingMaterialsToCache();
	// Reset index and cache.
	ResetCachedMaterials();

	// free up the command queue, important to call this after the status is set to truck as otherwise the
	// terminate conversion command will trigger because enableing the queue will clear it first -> Sets unit to idle.
	SetCommandQueueEnabled(true);

	if (PlayerController && PlayerController->GetMainMenuUI())
	{
		// Update Game UI.
		PlayerController->TruckConverted(this, false);
	}

	// Move Truck UI back in place.
	MoveTruckUIWithLocalOffsets(false);

	// Set decal to truck mode.
	AdjustSelectionDecalToConversion(false);

	// Set max health to vehicle health.
	AdjustMaxHealthForConversion(true);

	// Destroy any resource visualisation.
	DestroyAllResourceStorageMeshComponents();

	BP_OnFinishedConvertingToVehicle();
}

void ANomadicVehicle::OnFinishedConvertingToVehicle_HandleInstanceSpecificComponents()
{
	// Evaluating to false is not an error as not all nomadic vehicles have build radii.
	SetRadiusComponentActive(false);
	SetEnergyComponentActive(false);

	if (GetIsValidAircraftOwnerComp())
	{
		M_AircraftOwnerComp->OnPackUpAirbaseComplete();
	}
}

void ANomadicVehicle::SetDisableChaosVehicleMesh(const bool bDisable)
{
	if (IsValid(ChassisMesh))
	{
		ChassisMesh->SetVisibility(!bDisable);
		ChassisMesh->SetGenerateOverlapEvents(!bDisable);
		ChassisMesh->SetSimulatePhysics(!bDisable);
		ChassisMesh->SetCollisionEnabled(
			bDisable ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	}
}

void ANomadicVehicle::SetTrainingEnabled(const bool bSetEnabled)
{
	UTrainerComponent* TrainingComp = GetTrainerComponent();
	if (IsValid(TrainingComp))
	{
		TrainingComp->SetTrainingEnabled(bSetEnabled);
	}
}

void ANomadicVehicle::SetCargoEnabled(const bool bSetEnabled) const
{
	if (not IsValid(M_CargoComp))
	{
		return;
	}
	M_CargoComp->SetIsEnabled(bSetEnabled);
	if (bSetEnabled)
	{
		M_CargoComp->SetupCargo(BuildingMeshComponent,
		                        CargoSettings.MaxSupportedSquads,
		                        CargoSettings.CargoPositionsOffset,
		                        EGarrisonSeatsTextType::Units,
		                        true
		);
	}
}

void ANomadicVehicle::AdjustMaxHealthForConversion(const bool bSetToTruckMaxHealth) const
{
	if (!IsValid(HealthComponent))
	{
		return;
	}
	if (bSetToTruckMaxHealth)
	{
		HealthComponent->InitHealthAndResistance(M_TruckHealthData, M_TruckMaxHealth);
	}
	else
	{
		HealthComponent->InitHealthAndResistance(M_BuildingHealthData, M_BuildingMaxHealth);
	}
}

void ANomadicVehicle::PlayRandomConstructionSound()
{
	USoundAttenuation* Auten = RTSFunctionLibrary::CreateSoundAttenuation(4000);
	if (M_ConstructionSounds.Num() > 0 && PlayerController)
	{
		int32 RandomIndex;
		do
		{
			RandomIndex = FMath::RandRange(0, M_ConstructionSounds.Num() - 1);
		}
		while (RandomIndex == M_LastPlayedSoundIndex && M_ConstructionSounds.Num() > 1);

		M_LastPlayedSoundIndex = RandomIndex;
		USoundCue* RandomSound = M_ConstructionSounds[RandomIndex];
		UGameplayStatics::PlaySoundAtLocation(this, RandomSound, GetActorLocation(), FRotator::ZeroRotator, 1.0f, 1.0f,
		                                      0.0f, Auten, nullptr, nullptr);
	}
}

void ANomadicVehicle::SetResourceDropOffActive(const bool bIsActive) const
{
	if (IsValid(M_ResourceDropOff))
	{
		M_ResourceDropOff->SetIsDropOffActive(bIsActive);
	}
}

void ANomadicVehicle::SetEnergyComponentActive(const bool bIsActive) const
{
	if (IsValid(M_EnergyComp))
	{
		M_EnergyComp->SetEnabled(bIsActive);
	}
}

void ANomadicVehicle::SetRadiusComponentActive(const bool bIsActive) const
{
	if (IsValid(M_RadiusComp))
	{
		M_RadiusComp->SetEnabled(bIsActive);
	}
}

void ANomadicVehicle::SetReinforcementPointActive(const bool bIsActive) const
{
	if (IsValid(M_ReinforcementPoint))
	{
		M_ReinforcementPoint->SetReinforcementEnabled(bIsActive);
	}
}

void ANomadicVehicle::StartAsConvertedBuilding()
{
	OnFinishedStandaloneRotation();
}

bool ANomadicVehicle::GetIsValidAircraftOwnerComp() const
{
	if (IsValid(M_AircraftOwnerComp))
	{
		return true;
	}
	// Not an error: many vehicles do not support aircraft.
	return false;
}

bool ANomadicVehicle::UseTrainingPreview() const
{
	if (not IsValid(M_TrainerComponent))
	{
		return false;
	}
	return M_TrainerComponent->GetUseTrainingPreview();
}

void ANomadicVehicle::AnnounceConversion() const
{
	if (not GetIsValidRTSComponent() || not GetIsValidPlayerControler())
	{
		return;
	}
	if (RTSComponent->GetOwningPlayer() != 1)
	{
		return;
	}
	PlayerController->PlayAnnouncerVoiceLine(EAnnouncerVoiceLineType::NomadicBuildingConvertedToBuilding, true, true);
}

float ANomadicVehicle::GetConstructionTimeInPlayRate() const
{
	// Assume AnimationFPS is the frame rate the animation was authored at.
	constexpr float AnimationFPS = 30.f;
	const float MontageDuration = M_ConstructionFrames / AnimationFPS;

	// Calculate the play rate required to play the montage over m_ConstructionTime seconds.
	const float PlayRate = MontageDuration / M_ConstructionMontageTime;
	return PlayRate;
}

UTrainerComponent* ANomadicVehicle::GetTrainerComponent()
{
	return M_TrainerComponent;
}

void ANomadicVehicle::SetAINomadicVehicle(AAINomadicVehicle* NewAINomadicVehicle)
{
	if (IsValid(NewAINomadicVehicle))
	{
		AINomadicVehicle = NewAINomadicVehicle;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "AINomadicVehicle", "SetAINomadicVehicle");
	}
}

UStaticMeshComponent* ANomadicVehicle::GetAttachToMeshComponent() const
{
	return BuildingMeshComponent;
}

TArray<UStaticMeshSocket*> ANomadicVehicle::GetFreeSocketList(const int32 ExpansionSlotToIgnore) const
{
	using DeveloperSettings::GamePlay::Construction::BxpSocketTrigger;
	if (not IsValid(M_BuildingMesh))
	{
		const FString ActorName = GetName();
		RTSFunctionLibrary::ReportError("No building mesh component found for vehicle: " + ActorName +
			"\n Cannot get free sockets for attachments. Please ensure the building mesh component is set correctly.");
		return TArray<UStaticMeshSocket*>();
	}

	TArray<UStaticMeshSocket*> SocketList;

	TArray<UStaticMeshSocket*> MeshSockets = M_BuildingMesh->Sockets;
	for (UStaticMeshSocket* MeshSocket : MeshSockets)
	{
		if (!MeshSocket) continue;

		// Convert socket name to string and check if it contains the trigger
		if (MeshSocket->SocketName.ToString().Contains(BxpSocketTrigger))
		{
			SocketList.Add(MeshSocket);
		}
	}
	if (SocketList.Num() == 0)
	{
		return TArray<UStaticMeshSocket*>();
	}
	TArray<UStaticMeshSocket*> NonOccupiedSockets;
	TArray<FName> OccupiedSocketNames = GetOccupiedSocketNames(ExpansionSlotToIgnore);
	DebugSocketsPreCheck(SocketList, OccupiedSocketNames);
	for (auto EachSocket : SocketList)
	{
		if (not EachSocket)
		{
			continue;
		}
		const bool bIsOccupied = OccupiedSocketNames.Contains(EachSocket->SocketName);
		if (bIsOccupied)
		{
			OccupiedSocketNames.Remove(EachSocket->SocketName);
		}
		else
		{
			NonOccupiedSockets.Add(EachSocket);
		}
	}
	DebugFreeSockets(NonOccupiedSockets);
	return NonOccupiedSockets;
}

float ANomadicVehicle::GetUnitRepairRadius()
{
	if (M_NomadStatus == ENomadStatus::Building)
	{
		if (!IsValid(M_BuildingMesh))
		{
			RTSFunctionLibrary::ReportError(
				"Attempted to get repair radius of nomadic vehicle in building mode "
				"but the building mesh is not valid!");
			return Super::GetUnitRepairRadius();
		}

		// Pull the box half-extents (X,Y,Z) from the mesh bounds
		// Return the average over x and y.
		const FVector HalfExtents = M_BuildingMesh->GetBounds().BoxExtent;
		const float AvgXYRadius = 0.5f * (HalfExtents.X + HalfExtents.Y);
		return AvgXYRadius;
	}

	return Super::GetUnitRepairRadius();
}


void ANomadicVehicle::OnTrainingProgressStartedOrResumed(const FTrainingOption& StartedOption)
{
	BpOnTrainingProgressStartedOrResumed(StartedOption);
	if (UseTrainingPreview())
	{
		M_TrainingPreviewHelper.OnStartOrResumeTraining(StartedOption, this);
	}
	FNomadicAttachmentHelpers::StartAttachmentsMontages(&M_SpawnedAttachments);
}

void ANomadicVehicle::OnTrainingItemAddedToQueue(const FTrainingOption& AddedOption)
{
	// If this is an aircraft, reserve a bay
	if (AddedOption.UnitType == EAllUnitType::UNType_Aircraft && GetIsValidAircraftOwnerComp())
	{
		const EAircraftSubtype Sub = static_cast<EAircraftSubtype>(AddedOption.SubtypeValue);
		const bool bReserved = M_AircraftOwnerComp->OnAircraftTrainingStart(Sub);
		if (not bReserved)
		{
			RTSFunctionLibrary::ReportError("Airbase full: cannot reserve bay for queued aircraft."
				"\n This should not happen as the training item should have been disabled by requirement"
				"on a full airfield!");
		}
	}
}


void ANomadicVehicle::OnTrainingComplete(const FTrainingOption& CompletedOption, AActor* SpawnedUnit)
{
	// Reset preview FIRST to avoid hiding the next preview that may be started by the BP callback.
	if (UseTrainingPreview())
	{
		M_TrainingPreviewHelper.OnTrainingCompleted(CompletedOption);
	}

	BPOnTrainingComplete(CompletedOption);
	FNomadicAttachmentHelpers::StopAttachmentsMontages(&M_SpawnedAttachments);

	if (CompletedOption.UnitType != EAllUnitType::UNType_Aircraft)
	{
		return;
	}

	if (not GetIsValidAircraftOwnerComp())
	{
		RTSFunctionLibrary::ReportError("Training produced an aircraft but this vehicle has no AircraftOwnerComp!");
		return;
	}

	AAircraftMaster* Aircraft = Cast<AAircraftMaster>(SpawnedUnit);
	if (not IsValid(Aircraft))
	{
		RTSFunctionLibrary::ReportError("OnTrainingComplete expected AAircraftMaster but got something else.");
		return;
	}

	const EAircraftSubtype Sub = static_cast<EAircraftSubtype>(CompletedOption.SubtypeValue);
	const bool bAssigned = M_AircraftOwnerComp->OnAircraftTrained(Aircraft, Sub);
	if (not bAssigned)
	{
		RTSFunctionLibrary::ReportError("OnAircraftTrained failed to assign bay (unexpected).");
	}
}

void ANomadicVehicle::OnTrainingCancelled(const FTrainingOption& CancelledOption, const bool bRemovedActiveItem)
{
	if (CancelledOption.UnitType == EAllUnitType::UNType_Aircraft && GetIsValidAircraftOwnerComp())
	{
		const EAircraftSubtype Sub = static_cast<EAircraftSubtype>(CancelledOption.SubtypeValue);
		M_AircraftOwnerComp->OnAircraftTrainingCancelled(Sub);
	}

	if (not bRemovedActiveItem)
	{
		// Inactive item cancelled -> leave preview invariant.
		return;
	}

	// Reset preview FIRST to avoid hiding the next preview that may be started by the BP callback.
	if (UseTrainingPreview())
	{
		M_TrainingPreviewHelper.OnActiveTrainingCancelled(CancelledOption);
	}

	BPOnTrainingCancelled(CancelledOption);
	FNomadicAttachmentHelpers::StopAttachmentsMontages(&M_SpawnedAttachments);
}

void ANomadicVehicle::OnResourceStorageChanged(int32 PercentageResourcesFilled, const ERTSResourceType ResourceType)
{
	if (M_NomadStatus == ENomadStatus::Building)
	{
		UpdateResourceVisualsForType(ResourceType, PercentageResourcesFilled);
		OnResourceStorageChangedBP(PercentageResourcesFilled, ResourceType);
	}
}

void ANomadicVehicle::InitResourceStorageLevels(
	const TMap<ERTSResourceType, FResourceStorageLevels> HighToLowLevelsPerResource,
	const TMap<ERTSResourceType, FName> ResourceToSocketMap)
{
	M_HighToLowLevelsPerResource = HighToLowLevelsPerResource;
	M_ResourceToSocketMap = ResourceToSocketMap;

	TArray<ERTSResourceType> ResourcesToCheck = {
		ERTSResourceType::Resource_Radixite,
		ERTSResourceType::Resource_Metal,
		ERTSResourceType::Resource_VehicleParts
	};
	for (auto EachSupportedResource : ResourcesToCheck)
	{
		if (M_HighToLowLevelsPerResource.Find(EachSupportedResource)
			&& !M_ResourceToSocketMap.Find(EachSupportedResource))
		{
			RTSFunctionLibrary::ReportError(
				"ResourceToSocketMap is missing a key for a supported resource type in "
				"InitResourceStorageLevels for nomadic vehicle: " + GetName() +
				"\n Missing socket resource type: " + UEnum::GetValueAsString(EachSupportedResource));
		}
		if (!M_HighToLowLevelsPerResource.Find(EachSupportedResource)
			&& M_ResourceToSocketMap.Find(EachSupportedResource))
		{
			RTSFunctionLibrary::ReportError(
				"HighToLowLevelsPerResource is missing a key for a supported resource type in "
				"InitResourceStorageLevels for nomadic vehicle: " + GetName() +
				"\n Missing storage levels resource type: " + UEnum::GetValueAsString(EachSupportedResource));
		}
	}

	for (auto& EachResource : M_HighToLowLevelsPerResource)
	{
		// Ensure all mesh references are valid
		for (auto EachLevel : EachResource.Value.StorageLevels)
		{
			if (!IsValid(EachLevel.Mesh))
			{
				RTSFunctionLibrary::ReportError("Mesh reference is not valid for a storage level in "
					"InitResourceStorageLevels for nomadic vehicle: " + GetName() +
					"\n Resource type: " + UEnum::GetValueAsString(EachResource.Key) +
					"\n Level: " + FString::FromInt(EachLevel.Level));
			}
		}

		// Sort levels from highest to lowest
		TArray<FStorageMeshLevel>& Levels = EachResource.Value.StorageLevels;
		Levels.Sort([](const FStorageMeshLevel& A, const FStorageMeshLevel& B)
		{
			return A.Level > B.Level; // descending
		});

		// Verify after sorting that the highest is at index 0
		for (int32 i = 0; i < Levels.Num() - 1; ++i)
		{
			if (Levels[i].Level < Levels[i + 1].Level)
			{
				RTSFunctionLibrary::ReportError(
					"Failed to sort storage levels from high to low in "
					"InitResourceStorageLevels for nomadic vehicle: " + GetName() +
					"\n Resource type: " + UEnum::GetValueAsString(EachResource.Key) +
					"\n Found " + FString::FromInt(Levels[i].Level) + " < " +
					FString::FromInt(Levels[i + 1].Level));
			}
		}
	}
}

void ANomadicVehicle::SetupNomadicHealthData(FResistanceAndDamageReductionData TruckHealthData,
	FResistanceAndDamageReductionData BuildingHealthData, const float TruckMaxHealth, const float BuildingMaxHealth)
{
	M_TruckHealthData = TruckHealthData;
	M_BuildingHealthData = BuildingHealthData;
	M_TruckMaxHealth = TruckMaxHealth;
	M_BuildingMaxHealth = BuildingMaxHealth;
}


void ANomadicVehicle::UpdateResourceVisualsForType(const ERTSResourceType ResourceType,
                                                   const int32 PercentageResourcesFilled)
{
	UStaticMesh* MeshForLevel = nullptr;
	if (not M_HighToLowLevelsPerResource.Find(ResourceType))
	{
		RTSFunctionLibrary::ReportError(
			"Cannot update resource visuals on nomadic vehicle as provided type is not supported!"
			"\n type: " + UEnum::GetValueAsString(ResourceType) +
			"\n On Nomadic Vehicle: " + GetName());
		return;
	}
	const FResourceStorageLevels ResourceLevelContainer = M_HighToLowLevelsPerResource[ResourceType];
	for (auto EachLevel : ResourceLevelContainer.StorageLevels)
	{
		if (PercentageResourcesFilled >= EachLevel.Level)
		{
			// Set to highest possible level.
			MeshForLevel = EachLevel.Mesh;
			break;
		}
	}
	// Creates a component on the building mesh if not yet registered.
	SetResourceMeshVisual(MeshForLevel, ResourceType);
}

void ANomadicVehicle::SetResourceMeshVisual(UStaticMesh* Mesh, const ERTSResourceType ResourceType)
{
	if (not M_ResourceLevelComponents.Find(ResourceType))
	{
		CreateResourceStorageMeshComponent(ResourceType);
	}
	M_ResourceLevelComponents[ResourceType]->SetStaticMesh(Mesh);
}

void ANomadicVehicle::CreateResourceStorageMeshComponent(const ERTSResourceType ResourceType)
{
	if (M_ResourceLevelComponents.Find(ResourceType))
	{
		RTSFunctionLibrary::ReportError(
			"Resource Storage Mesh Component is already created for ResourceType: " + UEnum::GetValueAsString(
				ResourceType)
			+ "\n On Nomadic Vehicle: " + GetName() +
			"\n At function CreateResourceStorageMeshComponent");
		return;
	}
	if (not M_ResourceToSocketMap.Find(ResourceType))
	{
		RTSFunctionLibrary::ReportError(
			"Attempted to create resource visualization component for ResourceType: " + UEnum::GetValueAsString(
				ResourceType)
			+ " but no socket name was found in the ResourceToSocketMap!"
			"\n On Nomadic Vehicle: " + GetName());
		return;
	}
	const FName SocketName = M_ResourceToSocketMap[ResourceType];
	if (not IsValid(BuildingMeshComponent) || not BuildingMeshComponent->DoesSocketExist(SocketName))
	{
		RTSFunctionLibrary::ReportError(
			"Attempted to create resource visualization component for ResourceType: " + UEnum::GetValueAsString(
				ResourceType)
			+ " but the socket name was not found on the building mesh component!"
			"\n On Nomadic Vehicle: " + GetName());
		return;
	}
	UStaticMeshComponent* NewMeshComp = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass());
	if (!IsValid(NewMeshComp))
	{
		RTSFunctionLibrary::ReportError("Failed to create resource mesh component!"
			"\n On nomadic vehicle: " + GetName());
		return;
	}


	NewMeshComp->AttachToComponent(
		BuildingMeshComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		SocketName
	);

	NewMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NewMeshComp->SetCanEverAffectNavigation(false);
	NewMeshComp->RegisterComponent();
	M_ResourceLevelComponents.Add(ResourceType, NewMeshComp);
}

void ANomadicVehicle::DestroyAllResourceStorageMeshComponents()
{
	for (auto EachComponent : M_ResourceLevelComponents)
	{
		if (EachComponent.Value.IsValid())
		{
			EachComponent.Value->DestroyComponent();
			continue;
		}
		RTSFunctionLibrary::ReportError("Attempted to call DestroyComponent on ResourceStorageVisualization Component"
			"but it was already Invalid!"
			"\n On Nomadic Vehicle: " + GetName());
	}
	M_ResourceLevelComponents.Empty();
}

void ANomadicVehicle::RestoreResourceStorageVisualisation()
{
	if (not IsValid(M_ResourceDropOff))
	{
		// This nomadic vehicle is not a DropOff
		return;
	}
	// For each resource in the DropOffComponent restore the visualisation of the resource.
	TMap<ERTSResourceType, FHarvesterCargoSlot> Capacity = M_ResourceDropOff->GetResourceDropOffCapacity();
	for (auto EachCargoSlot : Capacity)
	{
		const int32 Percentage = EachCargoSlot.Value.GetPercentageFilled();
		if (Percentage >= 0)
		{
			UpdateResourceVisualsForType(EachCargoSlot.Key, Percentage);
		}
	}
}

void ANomadicVehicle::SetupChassisMeshCollision()
{
	if (IsValid(RTSComponent))
	{
		if (RTSComponent->GetOwningPlayer() == 1)
		{
			FRTS_CollisionSetup::SetupNomadicMvtPlayer(ChassisMesh);
		}
		else
		{
			FRTS_CollisionSetup::SetupNomadicMvtEnemy(ChassisMesh);
		}
	}
}


void ANomadicVehicle::CacheOriginalMaterials()
{
	if (IsValid(BuildingMeshComponent))
	{
		ResetCachedMaterials();
		if (BuildingMeshComponent)
		{
			for (int32 i = 0; i < BuildingMeshComponent->GetNumMaterials(); ++i)
			{
				M_CachedOriginalMaterials.Add(BuildingMeshComponent->GetMaterial(i));
			}
		}
	}
}

uint32 ANomadicVehicle::ApplyConstructionMaterial(const bool bOnlyCalculateExcludedMaterials) const
{
	uint32 AmountBuildingTruckMaterials = 0;
	if (!(IsValid(BuildingMeshComponent) && IsValid(M_ConstructionAnimationMaterial) && IsValid(ChassisMesh)))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "BuildingMeshComponent, M_ConstructionAnimationMaterial "
		                                             "or ChassisMesh",
		                                             "ApplyConstructionMaterial");
		return AmountBuildingTruckMaterials;
	}
	for (int32 i = 0; i < BuildingMeshComponent->GetNumMaterials(); ++i)
	{
		// Don't apply construction material to materials that are part of the truck.
		if (!ChassisMesh->GetMaterials().Contains(BuildingMeshComponent->GetMaterial(i)))
		{
			if (!bOnlyCalculateExcludedMaterials)
			{
				BuildingMeshComponent->SetMaterial(i, M_ConstructionAnimationMaterial);
			}
		}
		else
		{
			// Count the amount of materials that are part of the truck and the building.
			AmountBuildingTruckMaterials++;
		}
	}
	return AmountBuildingTruckMaterials;
}

void ANomadicVehicle::ReapplyOriginalMaterial()
{
	if (!IsValid(BuildingMeshComponent) && IsValid(ChassisMesh) && IsValid(M_ConstructionAnimationMaterial))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			this, "BuildingMeshComponent, ChassisMesh or M_ConstructionAnimationMaterial",
			"ReapplyOriginalMaterial");
		return;
	}
	// If we are creating the truck we reverse apply construction materials to the building mesh.
	if (M_NomadStatus == ENomadStatus::CreatingTruck)
	{
		if (M_MaterialIndex != 0)
		{
			if (ChassisMesh->GetMaterials().Contains(M_CachedOriginalMaterials[M_MaterialIndex]))
			{
				// This material is part of the truck, not the animation, skip it.
				M_MaterialIndex--;
				// Retry.
				ReapplyOriginalMaterial();
			}
			else
			{
				BuildingMeshComponent->SetMaterial(M_MaterialIndex, M_ConstructionAnimationMaterial);
				CreateRandomSmokeSystemAtTransform(M_CreateSmokeTransforms[M_MaterialIndex]);
				M_MaterialIndex--;
				PlayRandomConstructionSound();
			}
		}
		else
		{
			OnFinishedConvertingToVehicle();
		}
	}
	else
	{
		if (M_MaterialIndex < M_CachedOriginalMaterials.Num())
		{
			if (ChassisMesh->GetMaterials().Contains(M_CachedOriginalMaterials[M_MaterialIndex]))
			{
				// This material is part of the truck, not the animation, skip it.
				M_MaterialIndex++;
				// Retry.
				ReapplyOriginalMaterial();
			}
			else
			{
				// We have an unique building material slot to apply the original material to.
				BuildingMeshComponent->SetMaterial(M_MaterialIndex, M_CachedOriginalMaterials[M_MaterialIndex]);
				CreateRandomSmokeSystemAtTransform(M_CreateSmokeTransforms[M_MaterialIndex]);
				M_MaterialIndex++;
				PlayRandomConstructionSound();
			}
		}
		else
		{
			FinishReapplyingMaterials();
		}
	}
}

void ANomadicVehicle::FinishReapplyingMaterials()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MaterialReapplyTimerHandle);
		OnFinishedConvertingToBuilding();
		// Empty the cached materials and reset the index.
		ResetCachedMaterials();
	}
}

FVector ANomadicVehicle::CalculateMeanMaterialLocation(
	const int32 MaterialIndex,
	const TArray<FVector3f>& VertexPositions,
	const FTransform& TransformOfBuildingMesh) const
{
	FVector3f MeanLocation = FVector3f::ZeroVector;
	int32 VertexCount = VertexPositions.Num();

	for (const FVector3f& Position : VertexPositions)
	{
		MeanLocation += Position;
	}

	if (VertexCount > 0)
	{
		MeanLocation /= VertexCount;
	}

	// Convert MeanLocation to FVector for returning, and transform to world space.
	return TransformOfBuildingMesh.TransformPosition((FVector)MeanLocation);
}

FVector ANomadicVehicle::CalculateMeshPartSize(const TArray<FVector3f>& VertexPositions) const
{
	FVector Min(FLT_MAX);
	FVector Max(-FLT_MAX);

	for (const FVector3f& Position : VertexPositions)
	{
		// Convert to FVector
		FVector ConvertedPosition = FVector(Position);
		Min = Min.ComponentMin(ConvertedPosition);
		Min = Min.ComponentMin(ConvertedPosition);
		Max = Max.ComponentMax(ConvertedPosition);
	}

	// Size vector of the mesh part
	return Max - Min;
}

void ANomadicVehicle::InitSmokeLocations()
{
	if (not IsValid(BuildingMeshComponent) || not BuildingMeshComponent->GetStaticMesh())
	{
		RTSFunctionLibrary::ReportError("Building Mesh component or static mesh is not set on vehicle: " + GetName());
		return;
	}

	SetSmokeLocationsToRandomInBox();
	// Save guard to check if the mesh allows CPU access otherwise it breaks shipped builds.
	if (BuildingMeshComponent->GetStaticMesh()->bAllowCPUAccess)
	{
		FStaticMeshLODResources& LODResources = BuildingMeshComponent->GetStaticMesh()->GetRenderData()->LODResources[0];
		FIndexArrayView Indices = LODResources.IndexBuffer.GetArrayView();
		TArray<TArray<FVector3f>> VertexPositionsByMaterial;
		VertexPositionsByMaterial.SetNum(BuildingMeshComponent->GetNumMaterials());

		TArray<float> MeshPartSizes;
		MeshPartSizes.SetNum(BuildingMeshComponent->GetNumMaterials());
		float MaxPartSize = 0;

		// Collect vertices and calculate mesh part sizes. REQUIRES "Allow CPUAccess" in the static mesh editor.
		for (int32 SectionIndex = 0; SectionIndex < LODResources.Sections.Num(); ++SectionIndex)
		{
			const FStaticMeshSection& Section = LODResources.Sections[SectionIndex];
			TArray<FVector3f> PartVertices;

			for (uint32 i = Section.FirstIndex; i < Section.FirstIndex + Section.NumTriangles * 3; i++)
			{
				PartVertices.Add(LODResources.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[i]));
			}

			FVector PartSize = CalculateMeshPartSize(PartVertices);
			MeshPartSizes[Section.MaterialIndex] = PartSize.Size();

			MaxPartSize = FMath::Max(MaxPartSize, MeshPartSizes[Section.MaterialIndex]);
			VertexPositionsByMaterial[Section.MaterialIndex] = MoveTemp(PartVertices);
		}

		const FTransform ComponentTransform = BuildingMeshComponent->GetComponentTransform();
		FVector OriginLocation = ComponentTransform.GetLocation();
		RTSFunctionLibrary::PrintString("InitSmokeLocations:: Before AsyncTask");
		// A weak pointer to this object to prevent a dangling pointer in the async task.
		TWeakObjectPtr<ANomadicVehicle> WeakThis(this);
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakThis, VertexPositionsByMaterial,
			          ComponentTransform, OriginLocation, MeshPartSizes, MaxPartSize]()
		          {
			          RTSFunctionLibrary::PrintString("InitSmokeLocations:: At async task");
			          if (not WeakThis.IsValid())
			          {
				          return;
			          }

			          ANomadicVehicle* StrongThis = WeakThis.Get();
			          TArray<FTransform> CalculatedTransforms;
			          for (int32 i = 0; i < VertexPositionsByMaterial.Num(); ++i)
			          {
				          FVector MeanLocation = StrongThis->CalculateMeanMaterialLocation(
					          i, VertexPositionsByMaterial[i], ComponentTransform);
				          // Normalize scale factor based on the largest part
				          const float ScaleFactor = FMath::Max(2 * (MeshPartSizes[i] / MaxPartSize), 0.33);

				          FRotator Rotation = (MeanLocation - OriginLocation).Rotation();
				          FTransform Transform(Rotation, MeanLocation,
				                               FVector(ScaleFactor, ScaleFactor, ScaleFactor));
				          CalculatedTransforms.Add(Transform);
			          }
			          AsyncTask(ENamedThreads::GameThread, [WeakThis, CalculatedTransforms]()
			          {
				          // Check if the object still exists we cannot access members, for that we need the strong pointer.
				          if (WeakThis.IsValid())
				          {
					          ANomadicVehicle* StrongThis = WeakThis.Get();
					          StrongThis->M_CreateSmokeTransforms = CalculatedTransforms;
					          RTSFunctionLibrary::PrintString("InitSmokeLocations:: Rewrite back to game thread");
				          }
			          });
		          });
	}
	else
	{
		RTSFunctionLibrary::ReportError("Mesh is not CPU Accessible, cannot calculate smoke locations.");
	}
}

void ANomadicVehicle::SetSmokeLocationsToRandomInBox()
{
	if (not IsValid(BuildingMeshComponent))
	{
		return;
	}

	const FBoxSphereBounds MeshBounds = BuildingMeshComponent->CalcBounds(
		BuildingMeshComponent->GetComponentTransform());
	const FBox Box = MeshBounds.GetBox();
	const int32 NumMaterials = BuildingMeshComponent->GetNumMaterials();

	M_CreateSmokeTransforms.Init(FTransform::Identity, NumMaterials);
	for (FTransform& Transform : M_CreateSmokeTransforms)
	{
		Transform.SetLocation(FMath::RandPointInBox(Box));
	}
}

void ANomadicVehicle::CreateRandomSmokeSystemAtTransform(const FTransform& Transform) const
{
	if (M_SmokeSystems.Num() == 0)
	{
		// No smoke systems are available
		return;
	}

	// Randomly select a smoke system from the array
	UNiagaraSystem* SelectedSystem = M_SmokeSystems[FMath::RandRange(0, M_SmokeSystems.Num() - 1)];
	if (SelectedSystem)
	{
		// Spawn the Niagara system using the provided transform
		const UWorld* World = GetWorld();
		if (World)
		{
			// Spawn the system at the transform's location, using its rotation
			UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World, SelectedSystem, Transform.GetLocation(), Transform.GetRotation().Rotator(),
				Transform.GetScale3D(), true, true, ENCPoolMethod::AutoRelease, true);
		}
	}
}

void ANomadicVehicle::CancelBuildingMeshAnimation()
{
	if (IsValid(BuildingMeshComponent))
	{
		if (const UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(MaterialReapplyTimerHandle);
		}
		// Reapply original materials to the building mesh for a possible later animation.
		for (int32 i = 0; i < M_CachedOriginalMaterials.Num(); ++i)
		{
			BuildingMeshComponent->SetMaterial(i, M_CachedOriginalMaterials[i]);
		}
		ResetCachedMaterials();

		// Show the vehicle mesh.
		SetDisableChaosVehicleMesh(false);

		// hide the building mesh.
		BuildingMeshComponent->SetVisibility(false);
		BuildingMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Move Truck UI back in place.
		MoveTruckUIWithLocalOffsets(false);

		// Add any attachments back on the vehicle.
		OnMeshAnimationCancelled();
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "BuildingMeshComponent", "CancelBuildingMeshAnimation");
	}
}

void ANomadicVehicle::CreateBuildingAttachments()
{
	if (!IsValid(BuildingMeshComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "BuildingMeshComponent", "CreateBuildingAttachments");
		return;
	}
	CreateChildActorAttachments();
	CreateNiagaraAttachments();
	CreateSoundAttachments();
	CreateNavModifierAttachments();
}

void ANomadicVehicle::CreateChildActorAttachments()

{
	for (const auto& [ActorToSpawn, SocketName, Scale] : M_AttachmentsToSpawn)
	{
		if (ActorToSpawn)
		{
			// Attempt to get the transform of the specified socket
			FTransform SocketTransform;
			if (!BuildingMeshComponent->DoesSocketExist(SocketName) ||
				!(SocketTransform = BuildingMeshComponent->GetSocketTransform(SocketName,
				                                                              ERelativeTransformSpace::RTS_World))
				.IsValid())
			{
				continue;
			}

			AActor* SpawnedActor = nullptr;
			if (UWorld* World = GetWorld())
			{
				// Spawn the actor at the socket's location and orientation
				SpawnedActor = World->SpawnActor<AActor>(ActorToSpawn,
				                                         SocketTransform.GetLocation(),
				                                         SocketTransform.GetRotation().Rotator());
			}
			if (SpawnedActor)
			{
				// Attach it to the BuildingMeshComponent at the specified socket
				SpawnedActor->AttachToComponent(BuildingMeshComponent,
				                                FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				                                SocketName);
				SpawnedActor->SetActorScale3D(Scale);
				M_SpawnedAttachments.Add(SpawnedActor);
			}
		}
	}
}

void ANomadicVehicle::CreateNiagaraAttachments()
{
	// Handle Niagara system attachments
	for (const auto& [NiagaraSystemToSpawn, SocketName, Scale] : M_NiagaraAttachmentsToSpawn)
	{
		FTransform SocketTransform = BuildingMeshComponent->GetSocketTransform(
			SocketName, ERelativeTransformSpace::RTS_World);
		if (SocketTransform.IsValid())
		{
			UNiagaraComponent* SpawnedSystem = UNiagaraFunctionLibrary::SpawnSystemAttached(
				NiagaraSystemToSpawn, BuildingMeshComponent, SocketName, FVector(0), FRotator(0),
				Scale, EAttachLocation::SnapToTarget, true, ENCPoolMethod::None, true, true);
			if (SpawnedSystem)
			{
				M_SpawnedNiagaraSystems.Add(SpawnedSystem);
				SpawnedSystem->SetWorldScale3D(Scale);
			}
		}
	}
}

void ANomadicVehicle::CreateSoundAttachments()
{
	// Handle sound cue attachments
	for (const FBuildingSoundAttachment& Attachment : M_SoundAttachmentsToSpawn)
	{
		FTransform SocketTransform = BuildingMeshComponent->GetSocketTransform(
			Attachment.SocketName, ERelativeTransformSpace::RTS_World);
		if (SocketTransform.IsValid())
		{
			UAudioComponent* AudioComponent = UGameplayStatics::SpawnSoundAttached(
				Attachment.SoundCueToSpawn, BuildingMeshComponent, Attachment.SocketName,
				SocketTransform.GetLocation(), SocketTransform.GetRotation().Rotator(),
				EAttachLocation::SnapToTarget); // Assuming Scale.X is used for volume or range scaling.
			if (AudioComponent)
			{
				M_SpawnedSoundCues.Add(AudioComponent);
			}
		}
	}
}

void ANomadicVehicle::CreateNavModifierAttachments()
{
	if (!IsValid(BuildingMeshComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "BuildingMeshComponent", "CreateNavModifierAttachments");
		return;
	}

	for (const FBuildingNavModifierAttachment& Attachment : M_NavModifierAttachmentsToSpawn)
	{
		if (!BuildingMeshComponent->DoesSocketExist(Attachment.SocketName))
		{
			RTSFunctionLibrary::PrintString(
				"Socket not valid for NavModifierAttachment: " + Attachment.SocketName.ToString());
			continue;
		}

		// Create a new Box Component
		UBoxComponent* BoxComponent = NewObject<UBoxComponent>(this);
		if (BoxComponent)
		{
			BoxComponent->AttachToComponent(
				BuildingMeshComponent,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				Attachment.SocketName);
			BoxComponent->SetRelativeScale3D(FVector(1.0f)); // Set to default scale
			BoxComponent->SetBoxExtent(Attachment.Scale * 50.f); // Because BoxExtent is half the size
			BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			BoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
			// Block the Pawn channel to prevent navigation
			BoxComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			// Set up to affect navigation
			BoxComponent->SetCanEverAffectNavigation(true);
			BoxComponent->bDynamicObstacle = true;
			BoxComponent->RegisterComponent();

			M_AttachedNavModifiers.Add(BoxComponent);
		}
	}
}

void ANomadicVehicle::DestroyNavModifierAttachments()
{
	for (UBoxComponent* BoxComponent : M_AttachedNavModifiers)
	{
		if (IsValid(BoxComponent))
		{
			BoxComponent->DestroyComponent();
		}
	}
	M_AttachedNavModifiers.Empty();
}

void ANomadicVehicle::DestroyAllBuildingAttachments()
{
	// Iterate over all the spawned attachment actors and destroy them
	for (AActor* SpawnedActor : M_SpawnedAttachments)
	{
		if (IsValid(SpawnedActor))
		{
			SpawnedActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			SpawnedActor->Destroy();
		}
	}
	M_SpawnedAttachments.Empty();

	for (const auto NiagaraSystem : M_SpawnedNiagaraSystems)
	{
		if (IsValid(NiagaraSystem))
		{
			NiagaraSystem->DestroyComponent();
		}
	}
	M_SpawnedNiagaraSystems.Empty();

	for (const auto Sound : M_SpawnedSoundCues)
	{
		if (IsValid(Sound))
		{
			Sound->DestroyComponent();
		}
	}
	DestroyNavModifierAttachments();
}


void ANomadicVehicle::ResetCachedMaterials()
{
	M_CachedOriginalMaterials.Empty();
	M_MaterialIndex = 0;
}
