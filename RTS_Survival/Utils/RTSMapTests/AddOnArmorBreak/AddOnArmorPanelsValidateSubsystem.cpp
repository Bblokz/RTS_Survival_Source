// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AddOnArmorPanelsValidateSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/CommandLine.h"
#include "NiagaraComponent.h"
#include "NiagaraDataSetAccessor.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "UObject/UObjectIterator.h"
#include "ShaderCompiler.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/AddOnArmorPanels/AddOnArmorPanelDebris.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/AddOnArmorPanels/AddOnArmorPanelsComponent.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleDamageEvent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponShellType/WeaponShellType.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAddOnArmorPanelsValidate, Log, All);

namespace AddOnArmorPanelsValidateConstants
{
	const TCHAR* const ValidateSwitch = TEXT("AddOnArmorPanelsValidate");
	const TCHAR* const VehicleSwitch = TEXT("AddOnArmorPanelsVehicle=");
	const TCHAR* const InjectSwitch = TEXT("AddOnArmorPanelsInject=");
	// Object path of the mesh the injected one replaces (bolt-on panel meshes); default: the Hull component.
	const TCHAR* const InjectReplaceSwitch = TEXT("AddOnArmorPanelsInjectReplace=");
	const TCHAR* const DirectorySwitch = TEXT("AddOnArmorPanelsDir=");
	const TCHAR* const TagSwitch = TEXT("AddOnArmorPanelsTag=");
	// Also pictures the hull with every panel bit set and with the panel section in the engine's grid material.
	const TCHAR* const ProbeSwitch = TEXT("AddOnArmorPanelsProbe");
	const TCHAR* const PanelSectionProbeMaterial = TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial");
	// Probe: material shown on the panel section instead of the grid, with every mask bit set (CPD 0-3 and 8).
	const TCHAR* const ProbeMaterialSwitch = TEXT("AddOnArmorPanelsProbeMaterial=");
	// Starts the vehicle module setup on Blueprints that never call it (RTS_BP_AssaultTank children), so their panel
	// hulls can be checked before the Blueprint is wired; the run logs that it did.
	const TCHAR* const StartModuleSetupSwitch = TEXT("AddOnArmorPanelsStartModuleSetup");
	// Takes the test tank out of the fog of war, so the debris checks also run on enemy-faction tanks.
	const TCHAR* const RevealTankSwitch = TEXT("AddOnArmorPanelsRevealTank");
	const FName StartModuleSetupFunctionName(TEXT("BeginPlay_OnModulesComplete_FinalizeVehicleModules"));
	constexpr float AllPanelBits = 1048575.f;
	const TCHAR* const DefaultVehicleClass = TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/"
		"PanzerII/Ger_Vehicle_PanzerII_F_SpacedArmor.Ger_Vehicle_PanzerII_F_SpacedArmor_C");
	const TCHAR* const AnchorClassPrefix = TEXT("BP_UnitImage_LandscapeAnchor");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));

	// Real seconds: the HUD and start widget need a moment before the game can be started.
	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 60.0;
	// Game seconds.
	// Includes compiling the panel material's shaders on a fresh machine.
	constexpr float MaxModuleWaitSeconds = 600.f;
	constexpr float LandingSeconds = 3.f;
	constexpr float PictureSettleSeconds = 1.5f;
	constexpr float SecondsBetweenHits = 0.4f;
	// A resting debris plate lies flat (face normal within ~25 degrees of vertical) with its centre on the ground plane.
	constexpr float MinRestingNormalZ = 0.9f;
	constexpr float MaxRestingHeightAboveGround = 5.f;
	// Game seconds after the first shell at which its debris is pictured in the air.
	constexpr float DebrisFlightPictureSeconds = 0.2f;
	// Game seconds after the first shell at which its pieces must lie flat: after landing, before they shrink.
	constexpr float DebrisSettleCheckSeconds = 2.5f;
	constexpr int32 MaxHitsUntilDestroyed = 40;

	constexpr float SpawnHeight = 150.f;
	constexpr float GroundTraceHalfHeight = 5000.f;
	// The shell starts just outside the panel's centre and flies into the hull.
	constexpr float HitSurfaceOffset = 3.f;
	constexpr float ShellBaseDamage = 120.f;
	constexpr float ShellCalibreMm = 50.f;
	constexpr float ShellPenetration = 100.f;
	// Hull damage per shell; small so the tank survives the whole sequence.
	constexpr float HullDamagePerShell = 1.f;
	// The add-on behaviour's yellow multiplier (UVehicleModuleAddOnArmorBehaviour).
	constexpr float DamagedContribution = 0.5f;
	constexpr float ArmorTolerance = 0.01f;
	constexpr int32 PictureWidth = 1280;
	constexpr int32 PictureHeight = 720;
	const FVector CameraLookOffset(0.f, 0.f, 90.f);

	int32 CountBits(const int32 Mask)
	{
		return FMath::CountBits(static_cast<uint64>(static_cast<uint32>(Mask)));
	}
}

bool UAddOnArmorPanelsValidateSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), AddOnArmorPanelsValidateConstants::ValidateSwitch);
}

void UAddOnArmorPanelsValidateSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	using namespace AddOnArmorPanelsValidateConstants;
	Super::OnWorldBeginPlay(InWorld);
	M_VehicleClassPath = DefaultVehicleClass;
	FParse::Value(FCommandLine::Get(), VehicleSwitch, M_VehicleClassPath);
	FParse::Value(FCommandLine::Get(), InjectSwitch, M_InjectedHullPath);
	FParse::Value(FCommandLine::Get(), InjectReplaceSwitch, M_InjectReplacedMeshPath);
	M_OutputDirectory = FPaths::ProjectSavedDir() / TEXT("AddOnArmorBreak");
	FParse::Value(FCommandLine::Get(), DirectorySwitch, M_OutputDirectory);
	M_Tag = TEXT("PanelsValidate");
	FParse::Value(FCommandLine::Get(), TagSwitch, M_Tag);
	bM_RunProbes = FParse::Param(FCommandLine::Get(), ProbeSwitch);
	bM_StartModuleSetup = FParse::Param(FCommandLine::Get(), StartModuleSetupSwitch);
	bM_RevealTank = FParse::Param(FCommandLine::Get(), RevealTankSwitch);
	// Error dialogs are answered automatically in unattended runs; log them instead.
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	EnterStep(EAddOnArmorPanelsValidateStep::WaitingForGameStart);
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display, TEXT("RTS_ADDON_PANELS started vehicle=%s inject=%s tag=%s"),
	       *M_VehicleClassPath, M_InjectedHullPath.IsEmpty() ? TEXT("none") : *M_InjectedHullPath, *M_Tag);
}

void UAddOnArmorPanelsValidateSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

TStatId UAddOnArmorPanelsValidateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAddOnArmorPanelsValidateSubsystem, STATGROUP_Tickables);
}

void UAddOnArmorPanelsValidateSubsystem::Tick(const float DeltaTime)
{
	using namespace AddOnArmorPanelsValidateConstants;
	if (M_Step == EAddOnArmorPanelsValidateStep::Done)
	{
		return;
	}
	if (M_Step != EAddOnArmorPanelsValidateStep::WaitingForGameStart)
	{
		KeepGameRunning();
		AimCameras();
		const bool bNeedsPanelMesh = M_Step != EAddOnArmorPanelsValidateStep::WaitingForModules;
		// A killed tank may be destroyed at once; the death step checks its debris without it.
		if (bM_DeathTriggered)
		{
			TickDeath();
			return;
		}
		if (not M_Tank.IsValid() || not M_Armor.IsValid() || (bNeedsPanelMesh && not M_PanelMesh.IsValid()))
		{
			UE_LOG(LogRTSAddOnArmorPanelsValidate, Error,
			       TEXT("RTS_ADDON_PANELS lost step=%d tank=%d armour=%d panel_mesh=%d last_tank_location=%s"),
			       static_cast<int32>(M_Step), M_Tank.IsValid(), M_Armor.IsValid(), M_PanelMesh.IsValid(),
			       *M_LastTankLocation.ToString());
			Expect(false, TEXT("tank, armour and panel mesh stay alive"));
			Finish();
			return;
		}
		M_LastTankLocation = M_Tank->GetActorLocation();
		if (bM_RevealTank)
		{
			RevealTankFromFog();
		}
		TickFirstShotDebris();
	}
	switch (M_Step)
	{
	case EAddOnArmorPanelsValidateStep::WaitingForGameStart:
		if (FPlatformTime::Seconds() - M_StepStartRealSeconds > MaxStartSeconds)
		{
			Expect(false, TEXT("the game starts"));
			Finish();
			return;
		}
		if (FPlatformTime::Seconds() - M_StepStartRealSeconds < HudBuildSeconds || not TryStartGame())
		{
			return;
		}
		if (not SpawnTank())
		{
			Finish();
			return;
		}
		CreateCameras();
		EnterStep(EAddOnArmorPanelsValidateStep::WaitingForModules);
		return;
	case EAddOnArmorPanelsValidateStep::WaitingForModules:
		if (GetGameSecondsInStep() > MaxModuleWaitSeconds)
		{
			Expect(false, TEXT("the tank finalizes its modules"));
			Finish();
			return;
		}
		if (GetGameSecondsInStep() < LandingSeconds)
		{
			return;
		}
		LogModuleWaitReason();
		if (not M_Armor->GetAreModulesFinalized())
		{
			StartModuleSetupOnce();
			return;
		}
		// Pictures taken while the panel material compiles would show the fallback material.
		if ((GShaderCompilingManager != nullptr && GShaderCompilingManager->GetNumRemainingJobs() > 0) || IsAsyncLoading())
		{
			return;
		}
		if (not CollectModulesAndPanels())
		{
			Finish();
			return;
		}
		EnterStep(EAddOnArmorPanelsValidateStep::Intact);
		return;
	case EAddOnArmorPanelsValidateStep::Intact:
		TickStep(bM_RunProbes ? EAddOnArmorPanelsValidateStep::ProbeAllHidden : EAddOnArmorPanelsValidateStep::FirstHit,
		         [] {}, [this] { CheckIntact(); });
		return;
	case EAddOnArmorPanelsValidateStep::ProbeAllHidden:
		TickStep(EAddOnArmorPanelsValidateStep::ProbeSection,
		         [this] { M_PanelMesh->SetCustomPrimitiveDataFloat(AddOnArmorPanelMask::HiddenPanelsPrimitiveDataIndex, 1023.f); },
		         [this]
		         {
			         ExportPictures(TEXT("Probe_AllBitsSet"));
			         M_PanelMesh->SetCustomPrimitiveDataFloat(AddOnArmorPanelMask::HiddenPanelsPrimitiveDataIndex, 0.f);
		         });
		return;
	case EAddOnArmorPanelsValidateStep::ProbeSection:
		TickStep(EAddOnArmorPanelsValidateStep::FirstHit, [this] { ShowPanelSection(true); },
		         [this]
		         {
			         ExportPictures(TEXT("Probe_PanelSection"));
			         ShowPanelSection(false);
		         });
		return;
	case EAddOnArmorPanelsValidateStep::FirstHit:
		TickStep(EAddOnArmorPanelsValidateStep::HitUntilDestroyed,
		         [this] { Expect(FireAtPanel(M_LeftPanels[M_LeftPanels.Num() / 2]), TEXT("first shell resolves a plate")); },
		         [this] { CheckFirstHit(); });
		return;
	case EAddOnArmorPanelsValidateStep::HitUntilDestroyed:
		TickHitUntilDestroyed();
		return;
	case EAddOnArmorPanelsValidateStep::CrewRepair:
		TickStep(EAddOnArmorPanelsValidateStep::FullService,
		         [this] { Expect(M_Armor->AdvanceModuleRepairForCrew(M_LeftModuleId), TEXT("crew repair advances")); },
		         [this] { CheckCrewRepair(); });
		return;
	case EAddOnArmorPanelsValidateStep::FullService:
		TickStep(EAddOnArmorPanelsValidateStep::Death, [this] { M_Armor->RestoreAllModulesToHealthy(); },
		         [this] { CheckFullService(); });
		return;
	case EAddOnArmorPanelsValidateStep::Death:
		TickDeath();
		return;
	default:
		return;
	}
}

void UAddOnArmorPanelsValidateSubsystem::EnterStep(const EAddOnArmorPanelsValidateStep NewStep)
{
	M_Step = NewStep;
	M_StepStartRealSeconds = FPlatformTime::Seconds();
	M_StepStartGameSeconds = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
	bM_StepActionDone = false;
	if (NewStep == EAddOnArmorPanelsValidateStep::Done)
	{
		Finish();
	}
}

float UAddOnArmorPanelsValidateSubsystem::GetGameSecondsInStep() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() - M_StepStartGameSeconds : 0.f;
}

bool UAddOnArmorPanelsValidateSubsystem::TryStartGame()
{
	UWorld* World = GetWorld();
	M_PlayerController = IsValid(World) ? Cast<ACPPController>(World->GetFirstPlayerController()) : nullptr;
	if (not M_PlayerController.IsValid())
	{
		return false;
	}
	UPlayerStartGameControl* StartGameControl = M_PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	KeepGameRunning();
	return true;
}

void UAddOnArmorPanelsValidateSubsystem::KeepGameRunning() const
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(
			Picker->WidgetTree->FindWidget(AddOnArmorPanelsValidateConstants::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	if (M_PlayerController.IsValid() && M_PlayerController->GetIsGameOnPause())
	{
		M_PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

bool UAddOnArmorPanelsValidateSubsystem::SpawnTank()
{
	using namespace AddOnArmorPanelsValidateConstants;
	UWorld* World = GetWorld();
	for (TActorIterator<AActor> It(World); It && M_BaseLocation.IsZero(); ++It)
	{
		if (It->GetClass()->GetName().StartsWith(AnchorClassPrefix))
		{
			M_BaseLocation = It->GetActorLocation();
		}
	}
	for (TActorIterator<APlayerStart> It(World); It && M_BaseLocation.IsZero(); ++It)
	{
		M_BaseLocation = It->GetActorLocation();
	}
	const FVector TraceOffset(0.f, 0.f, GroundTraceHalfHeight);
	FHitResult GroundHit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AddOnArmorPanelsGround), false);
	if (World->LineTraceSingleByChannel(GroundHit, M_BaseLocation + TraceOffset, M_BaseLocation - TraceOffset,
	                                    ECC_Visibility, QueryParams))
	{
		M_BaseLocation = GroundHit.ImpactPoint;
	}

	UClass* const TankClass = LoadClass<ATankMaster>(nullptr, *M_VehicleClassPath);
	if (not IsValid(TankClass))
	{
		Expect(false, TEXT("the vehicle class loads: ") + M_VehicleClassPath);
		return false;
	}
	const FTransform SpawnTransform(FRotator::ZeroRotator, M_BaseLocation + FVector(0.f, 0.f, SpawnHeight));
	ATankMaster* const Tank = World->SpawnActorDeferred<ATankMaster>(TankClass, SpawnTransform, nullptr, nullptr,
	                                                                 ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(Tank))
	{
		Expect(false, TEXT("the tank spawns"));
		return false;
	}
	if (not M_InjectedHullPath.IsEmpty())
	{
		InjectPanels(*Tank);
	}
	// Construction scripts create the Blueprint components here and BeginPlay installs the modules right after.
	Tank->FinishSpawning(SpawnTransform);
	M_Tank = Tank;
	M_Armor = Tank->FindComponentByClass<UArmorCalculation>();
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display, TEXT("RTS_ADDON_PANELS spawned %s at %s"), *Tank->GetName(),
	       *M_BaseLocation.ToCompactString());
	return M_Armor.IsValid();
}

void UAddOnArmorPanelsValidateSubsystem::InjectPanels(ATankMaster& Tank) const
{
	UStaticMesh* const PanelHull = LoadObject<UStaticMesh>(nullptr, *M_InjectedHullPath);
	if (not IsValid(PanelHull) || IsValid(Tank.FindComponentByClass<UAddOnArmorPanelsComponent>()))
	{
		UE_LOG(LogRTSAddOnArmorPanelsValidate, Error, TEXT("RTS_ADDON_PANELS INJECTED nothing (hull %s, component present %d)"),
		       *M_InjectedHullPath, IsValid(Tank.FindComponentByClass<UAddOnArmorPanelsComponent>()) ? 1 : 0);
		return;
	}
	// The Blueprint components do not exist before FinishSpawning, so the hull is swapped on the class's component
	// templates (in memory only; this run never saves).
	int32 SwappedTemplates = 0;
	for (UClass* Class = Tank.GetClass(); Class != nullptr; Class = Class->GetSuperClass())
	{
		UBlueprintGeneratedClass* const BlueprintClass = Cast<UBlueprintGeneratedClass>(Class);
		if (BlueprintClass == nullptr)
		{
			continue;
		}
		TArray<UActorComponent*> Templates;
		if (UInheritableComponentHandler* const Overrides = BlueprintClass->GetInheritableComponentHandler(false))
		{
			Overrides->GetAllTemplates(Templates);
		}
		if (BlueprintClass->SimpleConstructionScript != nullptr)
		{
			for (const USCS_Node* const Node : BlueprintClass->SimpleConstructionScript->GetAllNodes())
			{
				Templates.Add(Node->ComponentTemplate);
			}
		}
		for (UActorComponent* const Template : Templates)
		{
			UStaticMeshComponent* const MeshTemplate = Cast<UStaticMeshComponent>(Template);
			// The Hull component's templates only: other components such as HullMg also start with "Hull".
			const bool bIsReplaced = IsValid(MeshTemplate) && (M_InjectReplacedMeshPath.IsEmpty()
				? MeshTemplate->GetName().StartsWith(TEXT("Hull_GEN_VARIABLE"))
				: GetPathNameSafe(MeshTemplate->GetStaticMesh()) == M_InjectReplacedMeshPath);
			if (bIsReplaced)
			{
				MeshTemplate->SetStaticMesh(PanelHull);
				++SwappedTemplates;
			}
		}
	}
	UAddOnArmorPanelsComponent* const Panels = NewObject<UAddOnArmorPanelsComponent>(&Tank, TEXT("AddOnArmorPanels"));
	Panels->RegisterComponent();
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display,
	       TEXT("RTS_ADDON_PANELS INJECTED panels component and hull %s on %d templates"), *M_InjectedHullPath,
	       SwappedTemplates);
}

bool UAddOnArmorPanelsValidateSubsystem::CollectModulesAndPanels()
{
	TInlineComponentArray<UStaticMeshComponent*> MeshComponents(M_Tank.Get());
	for (UStaticMeshComponent* const MeshComponent : MeshComponents)
	{
		UStaticMesh* const Mesh = MeshComponent->GetStaticMesh();
		const UAddOnArmorPanelLayout* const Layout = IsValid(Mesh) ? Mesh->GetAssetUserData<UAddOnArmorPanelLayout>() : nullptr;
		if (not IsValid(Layout))
		{
			continue;
		}
		M_PanelMesh = MeshComponent;
		// Bolted-on panel meshes cover the plates of the registered mesh they hang on.
		for (USceneComponent* Component = MeshComponent; IsValid(Component) && not M_ArmorMesh.IsValid();
		     Component = Component->GetAttachParent())
		{
			UMeshComponent* const CandidateMesh = Cast<UMeshComponent>(Component);
			M_ArmorMesh = IsValid(CandidateMesh) && M_Armor->GetIsMeshRegisteredForArmor(CandidateMesh) ? CandidateMesh : nullptr;
		}
		// Like the panels component: a panel mesh beside the hull covers the first registered (hull) mesh.
		if (not M_ArmorMesh.IsValid())
		{
			M_ArmorMesh = M_Armor->GetArmorSetup().MeshWithArmor0.Get();
		}
		for (int32 PanelIndex = 0; PanelIndex < Layout->GetPanels().Num(); ++PanelIndex)
		{
			const bool bLeft = GetIsPanelOnLeft(PanelIndex);
			(bLeft ? M_LeftPanelMask : M_RightPanelMask) |= 1 << PanelIndex;
			if (bLeft)
			{
				M_LeftPanels.Add(PanelIndex);
			}
		}
	}
	for (int32 SlotIndex = 0; SlotIndex < VehicleModuleBalance::MaxModuleInstances; ++SlotIndex)
	{
		const FVehicleModuleSnapshot Snapshot = M_Armor->GetModuleSnapshotForSlot(SlotIndex);
		if (not Snapshot.bInstalled || Snapshot.Type != EVehicleModuleTypes::AddOnArmor)
		{
			continue;
		}
		(M_LeftModuleId == INDEX_NONE ? M_LeftModuleId : M_RightModuleId) = Snapshot.ModuleId;
	}
	// Penetrating side hits also roll the ammo module; a cook-off would end the run before the repairs are checked.
	M_Armor->SetVehicleModuleFiresEnabled(false, false);
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display, TEXT("RTS_ADDON_PANELS engine fires and ammo cook-offs disabled on the test tank"));
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display,
	       TEXT("RTS_ADDON_PANELS modules left=%d right=%d panels left_mask=0x%x right_mask=0x%x mesh=%s"),
	       M_LeftModuleId, M_RightModuleId, M_LeftPanelMask, M_RightPanelMask,
	       M_PanelMesh.IsValid() ? *GetNameSafe(M_PanelMesh->GetStaticMesh()) : TEXT("none"));
	Expect(M_PanelMesh.IsValid(), TEXT("a hull mesh with a panel layout"));
	Expect(M_ArmorMesh.IsValid(), TEXT("the panel mesh or a parent of it is registered for armour"));
	Expect(M_LeftModuleId != INDEX_NONE && M_RightModuleId != INDEX_NONE, TEXT("two add-on armour zones installed"));
	Expect(M_LeftPanels.Num() > 1 && M_RightPanelMask != 0, TEXT("panels on both sides"));
	return M_PanelMesh.IsValid() && M_ArmorMesh.IsValid() && M_RightModuleId != INDEX_NONE && M_LeftPanels.Num() > 1;
}

void UAddOnArmorPanelsValidateSubsystem::CreateCameras()
{
	using namespace AddOnArmorPanelsValidateConstants;
	// The tank faces +X; its left side looks towards -Y, so the cameras stand on -Y looking at +Y (yaw 90).
	const TArray<TTuple<FString, float, float, float, float>> CameraSettings = {
		MakeTuple(FString(TEXT("LeftClose")), 90.f, -12.f, 750.f, 55.f),
		MakeTuple(FString(TEXT("LeftRts")), 60.f, -55.f, 1900.f, 55.f)
	};
	for (const TTuple<FString, float, float, float, float>& Settings : CameraSettings)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASceneCapture2D* const Capture = GetWorld()->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(),
		                                                                         FTransform::Identity, SpawnParameters);
		UTextureRenderTarget2D* const RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
			GetWorld(), PictureWidth, PictureHeight, RTF_RGBA8_SRGB);
		if (not IsValid(Capture) || not IsValid(RenderTarget))
		{
			continue;
		}
		USceneCaptureComponent2D* const CaptureComponent = Capture->GetCaptureComponent2D();
		CaptureComponent->TextureTarget = RenderTarget;
		CaptureComponent->CaptureSource = SCS_FinalColorLDR;
		CaptureComponent->FOVAngle = Settings.Get<4>();
		CaptureComponent->bCaptureEveryFrame = true;
		CaptureComponent->bCaptureOnMovement = false;
		CaptureComponent->bAlwaysPersistRenderingState = true;
		// The fog of war is a post-process material; the pictures show the panels, not what the player may see.
		CaptureComponent->ShowFlags.SetPostProcessMaterial(false);
		FAddOnArmorPanelsCamera& Camera = M_Cameras.AddDefaulted_GetRef();
		Camera.Name = Settings.Get<0>();
		Camera.Yaw = Settings.Get<1>();
		Camera.Pitch = Settings.Get<2>();
		Camera.Distance = Settings.Get<3>();
		Camera.Capture = Capture;
		Camera.RenderTarget.Reset(RenderTarget);
	}
}

void UAddOnArmorPanelsValidateSubsystem::AimCameras() const
{
	for (const FAddOnArmorPanelsCamera& Camera : M_Cameras)
	{
		if (not Camera.Capture.IsValid() || not M_Tank.IsValid())
		{
			continue;
		}
		const FVector LookAt = M_Tank->GetActorLocation() + AddOnArmorPanelsValidateConstants::CameraLookOffset;
		const FRotator CameraRotation(Camera.Pitch, M_Tank->GetActorRotation().Yaw + Camera.Yaw, 0.f);
		Camera.Capture->SetActorLocationAndRotation(LookAt - CameraRotation.Vector() * Camera.Distance, CameraRotation);
	}
}

void UAddOnArmorPanelsValidateSubsystem::ExportPictures(const FString& Label) const
{
	for (const FAddOnArmorPanelsCamera& Camera : M_Cameras)
	{
		if (Camera.RenderTarget.IsValid())
		{
			UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), Camera.RenderTarget.Get(), M_OutputDirectory / M_Tag,
			                                            FString::Printf(TEXT("%s_%s.png"), *Camera.Name, *Label));
		}
	}
}

void UAddOnArmorPanelsValidateSubsystem::TickStep(const EAddOnArmorPanelsValidateStep NextStep,
                                                  const TFunctionRef<void()> Action, const TFunctionRef<void()> Check)
{
	if (not bM_StepActionDone)
	{
		Action();
		bM_StepActionDone = true;
		return;
	}
	if (GetGameSecondsInStep() < AddOnArmorPanelsValidateConstants::PictureSettleSeconds)
	{
		return;
	}
	// A material shown for the first time compiles first; until then the pictures show the fallback grid.
	if (GShaderCompilingManager != nullptr && GShaderCompilingManager->GetNumRemainingJobs() > 0)
	{
		return;
	}
	Check();
	CheckRightSideUntouched();
	EnterStep(NextStep);
}

void UAddOnArmorPanelsValidateSubsystem::TickHitUntilDestroyed()
{
	using namespace AddOnArmorPanelsValidateConstants;
	const float Now = GetWorld()->GetTimeSeconds();
	if (GetModuleState(M_LeftModuleId) == EVehicleModuleState::Destroyed)
	{
		if (M_DestroyedGameSeconds < 0.f)
		{
			M_DestroyedGameSeconds = Now;
		}
		if (Now - M_DestroyedGameSeconds >= PictureSettleSeconds)
		{
			CheckDestroyed();
			CheckRightSideUntouched();
			EnterStep(EAddOnArmorPanelsValidateStep::CrewRepair);
		}
		return;
	}
	if (M_HitSerial >= MaxHitsUntilDestroyed || M_CoveredLeftPanels.IsEmpty())
	{
		Expect(false, TEXT("the left zone is destroyed within the hit budget"));
		Finish();
		return;
	}
	if (Now < M_NextHitGameSeconds)
	{
		return;
	}
	M_NextHitGameSeconds = Now + SecondsBetweenHits;
	// Alternate between the panels in front of covered plates so each hit lands elsewhere than the last one.
	const int32 PanelCount = M_LeftPanels.Num();
	const int32 TargetPanel = M_CoveredLeftPanels[M_HitSerial % M_CoveredLeftPanels.Num()];
	Expect(FireAtPanel(TargetPanel), TEXT("shell resolves a plate"));
	const int32 HiddenLeft = CountBits(GetHiddenPanelMask() & M_LeftPanelMask);
	const FVehicleModuleSnapshot Snapshot = M_Armor->GetModuleSnapshot(M_LeftModuleId);
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display, TEXT("RTS_ADDON_PANELS hit=%d panel=%d health=%.2f state=%d hidden_left=%d"),
	       M_HitSerial, TargetPanel, Snapshot.MaxHp > 0.f ? Snapshot.CurrentHp / Snapshot.MaxHp : 0.f,
	       static_cast<int32>(Snapshot.State), HiddenLeft);
	Expect(HiddenLeft >= M_PreviousHiddenLeftCount, TEXT("damage never brings a panel back"));
	if (Snapshot.State == EVehicleModuleState::Damaged)
	{
		Expect(HiddenLeft >= 1 && HiddenLeft <= PanelCount - 1, TEXT("a damaged zone hides between one and all-but-one panels"));
	}
	M_PreviousHiddenLeftCount = HiddenLeft;
}

bool UAddOnArmorPanelsValidateSubsystem::GetIsPanelOnLeft(const int32 PanelIndex) const
{
	// The vehicle's side, not the mesh's: hull components can be mounted rotated on the actor.
	UStaticMesh* const Mesh = M_PanelMesh->GetStaticMesh();
	const FVector MeshCentre = Mesh->GetAssetUserData<UAddOnArmorPanelLayout>()->GetPanels()[PanelIndex].MeshSpaceCentre;
	const FVector WorldCentre = M_PanelMesh->GetComponentTransform().TransformPosition(MeshCentre);
	return M_Tank->GetActorTransform().InverseTransformPosition(WorldCentre).Y < 0.0;
}

FAddOnArmorPanelsShot UAddOnArmorPanelsValidateSubsystem::ResolveShotAtPanel(const int32 PanelIndex) const
{
	using namespace AddOnArmorPanelsValidateConstants;
	FAddOnArmorPanelsShot Shot;
	UStaticMesh* const Mesh = M_PanelMesh->GetStaticMesh();
	const UAddOnArmorPanelLayout* const Layout = Mesh->GetAssetUserData<UAddOnArmorPanelLayout>();
	const FVector Centre = M_PanelMesh->GetComponentTransform().TransformPosition(
		Layout->GetPanels()[PanelIndex].MeshSpaceCentre);
	const bool bLeft = GetIsPanelOnLeft(PanelIndex);
	const FVector Inward = M_Tank->GetActorRightVector() * (bLeft ? 1.f : -1.f);
	Shot.HitLocation = Centre - Inward * HitSurfaceOffset;
	float AdjustedPenetration = ShellPenetration;
	Shot.EffectiveArmor = M_Armor->GetEffectiveArmorOnHit(M_ArmorMesh.Get(), Shot.HitLocation, Inward, -Inward,
	                                                      Shot.RawArmor, AdjustedPenetration, Shot.PlateHit,
	                                                      Shot.PlateRegistrationId);
	return Shot;
}

bool UAddOnArmorPanelsValidateSubsystem::FireAtPanel(const int32 PanelIndex)
{
	using namespace AddOnArmorPanelsValidateConstants;
	const FAddOnArmorPanelsShot Shot = ResolveShotAtPanel(PanelIndex);
	if (Shot.PlateRegistrationId == INDEX_NONE)
	{
		return false;
	}
	FVehicleModuleBallisticHit BallisticHit;
	BallisticHit.ArmorCalculation = M_Armor.Get();
	BallisticHit.HitLocation = Shot.HitLocation;
	BallisticHit.PlateHit = Shot.PlateHit;
	BallisticHit.PlateRegistrationId = Shot.PlateRegistrationId;
	BallisticHit.EffectiveArmor = Shot.EffectiveArmor;
	BallisticHit.ProjectileBaseDamage = ShellBaseDamage;
	BallisticHit.ProjectileCalibre = ShellCalibreMm;
	BallisticHit.bPenetrated = true;
	BallisticHit.ShellType = EWeaponShellType::Shell_AP;
	BallisticHit.ShotActivationId = static_cast<uint32>(++M_HitSerial);
	const FVehicleModuleDamageEvent DamageEvent = FVehicleModuleDamageEvent::MakeBallisticEvent(
		BallisticHit, UDamageType::StaticClass());
	M_Tank->TakeDamage(GetHullDamagePerShell(), DamageEvent, nullptr, nullptr);
	return true;
}

float UAddOnArmorPanelsValidateSubsystem::GetHullDamagePerShell() const
{
	using namespace AddOnArmorPanelsValidateConstants;
	// Module damage only resolves when some hull damage gets through; some Blueprints add a flat damage reduction
	// (the Soviet T-34E: AddHealth with 5 reduction), which would swallow the test's small hull damage.
	const UHealthComponent* const Health = M_Tank->FindComponentByClass<UHealthComponent>();
	const FStructProperty* const ReductionProperty = FindFProperty<FStructProperty>(
		UHealthComponent::StaticClass(), TEXT("DamageReductionSettings"));
	if (not IsValid(Health) || ReductionProperty == nullptr)
	{
		return HullDamagePerShell;
	}
	const FDamageReductionSettings* const Reduction =
		ReductionProperty->ContainerPtrToValuePtr<FDamageReductionSettings>(Health);
	constexpr float MinReductionMultiplier = 0.01f;
	return (HullDamagePerShell + FMath::Max(0.f, Reduction->DamageReduction))
		/ FMath::Max(Reduction->DamageReductionMlt, MinReductionMultiplier);
}

FAddOnArmorPanelsSideReading UAddOnArmorPanelsValidateSubsystem::ReadSide(const bool bLeftSide) const
{
	using namespace AddOnArmorPanelsValidateConstants;
	FAddOnArmorPanelsSideReading Reading;
	UStaticMesh* const Mesh = M_PanelMesh->GetStaticMesh();
	const UAddOnArmorPanelLayout* const Layout = Mesh->GetAssetUserData<UAddOnArmorPanelLayout>();
	// The middle panel of the side: the end panels reach over the front and rear plate boxes.
	const int32 SideMask = bLeftSide ? M_LeftPanelMask : M_RightPanelMask;
	TArray<int32> SidePanels;
	for (int32 Index = 0; Index < Layout->GetPanels().Num(); ++Index)
	{
		if ((SideMask & (1 << Index)) != 0)
		{
			SidePanels.Add(Index);
		}
	}
	if (SidePanels.IsEmpty())
	{
		return Reading;
	}
	const int32 PanelIndex = SidePanels[SidePanels.Num() / 2];
	const FVector Centre = M_PanelMesh->GetComponentTransform().TransformPosition(Layout->GetPanels()[PanelIndex].MeshSpaceCentre);
	const FVector Inward = M_Tank->GetActorRightVector() * (bLeftSide ? 1.f : -1.f);
	float AdjustedPenetration = ShellPenetration;
	EArmorPlate PlateHit = EArmorPlate::Plate_Front;
	(void)M_Armor->GetEffectiveArmorOnHit(M_ArmorMesh.Get(), Centre - Inward * HitSurfaceOffset, Inward, -Inward,
	                                      Reading.RawArmor, AdjustedPenetration, PlateHit, Reading.PlateRegistrationId);
	if (Reading.PlateRegistrationId == INDEX_NONE)
	{
		return Reading;
	}
	const FArmorSetup Setup = M_Armor->GetArmorSetup();
	const int32 MeshSlot = Reading.PlateRegistrationId / VehicleModuleBalance::MaxArmorPlatesPerRegisteredMesh;
	const int32 PlateIndex = Reading.PlateRegistrationId % VehicleModuleBalance::MaxArmorPlatesPerRegisteredMesh;
	const FArmorSettings* const Plates = MeshSlot == 0 ? Setup.ArmorSettings0 : MeshSlot == 1 ? Setup.ArmorSettings1 : Setup.ArmorSettings2;
	Reading.StructuralArmor = Plates[PlateIndex].ArmorValue;
	Reading.AddOnArmor = Plates[PlateIndex].AddOnArmorValue;
	Reading.PlateName = UEnum::GetValueAsString(PlateHit);
	return Reading;
}

int32 UAddOnArmorPanelsValidateSubsystem::GetHiddenPanelMask() const
{
	const TArray<float>& Data = M_PanelMesh->GetCustomPrimitiveData().Data;
	const int32 MaskIndex = AddOnArmorPanelMask::HiddenPanelsPrimitiveDataIndex;
	return Data.IsValidIndex(MaskIndex) ? FMath::RoundToInt32(Data[MaskIndex]) : 0;
}

void UAddOnArmorPanelsValidateSubsystem::ShowPanelSection(const bool bShow) const
{
	using namespace AddOnArmorPanelsValidateConstants;
	FString ProbeMaterialPath = PanelSectionProbeMaterial;
	const bool bHasProbeMaterial = FParse::Value(FCommandLine::Get(), ProbeMaterialSwitch, ProbeMaterialPath);
	// A given probe material is shown with every mask bit set; the grid with none.
	const float ProbeBits = bShow && bHasProbeMaterial ? AllPanelBits : 0.f;
	constexpr int32 TestMaskWords = 4;
	for (int32 Index = 0; Index < TestMaskWords; ++Index)
	{
		M_PanelMesh->SetCustomPrimitiveDataFloat(Index, ProbeBits);
	}
	M_PanelMesh->SetCustomPrimitiveDataFloat(AddOnArmorPanelMask::HiddenPanelsPrimitiveDataIndex, ProbeBits);
	UMaterialInterface* const ProbeMaterial = LoadObject<UMaterialInterface>(nullptr, *ProbeMaterialPath);
	// Panel geometry keeps one slot per source slot: AddOnArmorPanels, AddOnArmorPanels_1, ...
	const FString PanelSlotPrefix = AddOnArmorPanelMask::PanelMaterialSlotName.ToString();
	const TArray<FName> SlotNames = M_PanelMesh->GetMaterialSlotNames();
	for (const FName SlotName : SlotNames)
	{
		if (not SlotName.ToString().StartsWith(PanelSlotPrefix))
		{
			continue;
		}
		const int32 PanelSlot = M_PanelMesh->GetMaterialIndex(SlotName);
		M_PanelMesh->SetMaterial(PanelSlot, bShow ? ProbeMaterial : M_PanelMesh->GetStaticMesh()->GetMaterial(PanelSlot));
		UE_LOG(LogRTSAddOnArmorPanelsValidate, Display, TEXT("RTS_ADDON_PANELS probe slot=%d material=%s"), PanelSlot,
		       *GetNameSafe(M_PanelMesh->GetMaterial(PanelSlot)));
	}
}

EVehicleModuleState UAddOnArmorPanelsValidateSubsystem::GetModuleState(const int32 ModuleId) const
{
	return M_Armor->GetModuleSnapshot(ModuleId).State;
}

void UAddOnArmorPanelsValidateSubsystem::TickDeath()
{
	using namespace AddOnArmorPanelsValidateConstants;
	if (not bM_StepActionDone)
	{
		// Some panels already shot off, so the death must throw only the ones still on the hull.
		Expect(FireAtPanel(M_LeftPanels[M_LeftPanels.Num() / 2]), TEXT("the shell before the death resolves a plate"));
		bM_StepActionDone = true;
		return;
	}
	if (not bM_DeathTriggered)
	{
		if (GetGameSecondsInStep() >= PictureSettleSeconds)
		{
			KillTank();
		}
		return;
	}
	if (GetWorld()->GetTimeSeconds() - M_DeathGameSeconds >= DebrisFlightPictureSeconds)
	{
		CheckDeathDebris();
		EnterStep(EAddOnArmorPanelsValidateStep::Done);
	}
}

void UAddOnArmorPanelsValidateSubsystem::KillTank()
{
	const int32 AllPanels = M_LeftPanelMask | M_RightPanelMask;
	const int32 HiddenBefore = GetHiddenPanelMask() & AllPanels;
	M_ExpectedDeathPieces = AddOnArmorPanelsValidateConstants::CountBits(AllPanels & ~HiddenBefore);
	M_ExpectedDeathBursts = CountExpectedDeathBursts(AllPanels & ~HiddenBefore);
	M_BurstsBeforeDeath.Reset();
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		const UNiagaraSystem* const System = It->GetAsset();
		if (It->GetWorld() == GetWorld() && It->IsActive() && IsValid(System)
			&& System->GetPathName() == AddOnArmorPanelDebris::SystemPath)
		{
			M_BurstsBeforeDeath.Add(*It);
		}
	}
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display,
	       TEXT("RTS_ADDON_PANELS death: mask before=0x%x intact panels=%d expected bursts=%d earlier bursts=%d"),
	       HiddenBefore, M_ExpectedDeathPieces, M_ExpectedDeathBursts, M_BurstsBeforeDeath.Num());
	Expect(HiddenBefore != 0 && M_ExpectedDeathPieces > 0, TEXT("the tank dies with some panels shot off and some intact"));
	bM_DeathTriggered = true;
	M_DeathGameSeconds = GetWorld()->GetTimeSeconds();
	M_Tank->Debug_DieRegularDeath(ERegularDeathVariant::Standard, -M_Tank->GetActorRightVector());
	// The panel mesh may be gone already when the death destroys the tank at once.
	if (M_PanelMesh.IsValid())
	{
		const int32 HiddenAfter = GetHiddenPanelMask() & AllPanels;
		UE_LOG(LogRTSAddOnArmorPanelsValidate, Display, TEXT("RTS_ADDON_PANELS death: mask after=0x%x"), HiddenAfter);
		Expect(HiddenAfter == AllPanels, TEXT("a dead tank hides every panel"));
	}
}

int32 UAddOnArmorPanelsValidateSubsystem::CountExpectedDeathBursts(const int32 IntactPanelMask) const
{
	UStaticMesh* const Hull = M_PanelMesh.IsValid() ? M_PanelMesh->GetStaticMesh() : nullptr;
	const UAddOnArmorPanelLayout* const Layout = IsValid(Hull) ? Hull->GetAssetUserData<UAddOnArmorPanelLayout>() : nullptr;
	if (not IsValid(Layout))
	{
		return 0;
	}
	int32 Bursts = 0;
	for (const int32 SideMask : {M_LeftPanelMask, M_RightPanelMask})
	{
		TSet<const UStaticMesh*> SideMeshes;
		for (int32 PanelIndex = 0; PanelIndex < Layout->GetPanels().Num(); ++PanelIndex)
		{
			bool bIsOwnSideMesh = false;
			const UStaticMesh* const DebrisMesh = Layout->GetDebrisMeshForPanel(PanelIndex, bIsOwnSideMesh);
			if ((SideMask & IntactPanelMask & (1 << PanelIndex)) != 0 && IsValid(DebrisMesh))
			{
				SideMeshes.Add(DebrisMesh);
			}
		}
		Bursts += SideMeshes.Num();
	}
	return Bursts;
}

void UAddOnArmorPanelsValidateSubsystem::RevealTankFromFog()
{
	if (not bM_DidStopTankFow)
	{
		UFowComp* const FowComponent = M_Tank->FindComponentByClass<UFowComp>();
		if (IsValid(FowComponent))
		{
			FowComponent->StopFowParticipation();
		}
		bM_DidStopTankFow = true;
		UE_LOG(LogRTSAddOnArmorPanelsValidate, Display, TEXT("RTS_ADDON_PANELS NOTE tank taken out of the fog of war (fow component %d)"),
		       IsValid(FowComponent) ? 1 : 0);
	}
	if (M_Tank->IsHidden())
	{
		M_Tank->SetActorHiddenInGame(false);
	}
}

void UAddOnArmorPanelsValidateSubsystem::CheckDeathDebris()
{
	int32 Bursts = 0;
	int32 Pieces = 0;
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent* const Burst = *It;
		const UNiagaraSystem* const System = Burst->GetAsset();
		if (Burst->GetWorld() != GetWorld() || not Burst->IsActive() || not IsValid(System)
			|| System->GetPathName() != AddOnArmorPanelDebris::SystemPath
			|| M_BurstsBeforeDeath.Contains(TWeakObjectPtr<UNiagaraComponent>(Burst)))
		{
			continue;
		}
		++Bursts;
		Pieces += CountParticles(*Burst);
	}
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display,
	       TEXT("RTS_ADDON_PANELS death debris bursts=%d (expected %d) pieces=%d (expected %d) tank_alive=%d"),
	       Bursts, M_ExpectedDeathBursts, Pieces, M_ExpectedDeathPieces, M_Tank.IsValid());
	Expect(Bursts == M_ExpectedDeathBursts, TEXT("the death throws one burst per side and debris mesh that still had panels"));
	Expect(Pieces == M_ExpectedDeathPieces, TEXT("the death throws exactly the panels that were still on the hull"));
	ExportPictures(TEXT("5_Death"));
}

void UAddOnArmorPanelsValidateSubsystem::CheckIntact()
{
	M_IntactLeft = ReadSide(true);
	M_IntactRight = ReadSide(false);
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display,
	       TEXT("RTS_ADDON_PANELS intact left %s raw=%.2f structural=%.2f addon=%.2f right %s raw=%.2f structural=%.2f addon=%.2f mask=0x%x"),
	       *M_IntactLeft.PlateName, M_IntactLeft.RawArmor, M_IntactLeft.StructuralArmor, M_IntactLeft.AddOnArmor,
	       *M_IntactRight.PlateName, M_IntactRight.RawArmor, M_IntactRight.StructuralArmor, M_IntactRight.AddOnArmor,
	       GetHiddenPanelMask());
	Expect(GetHiddenPanelMask() == 0, TEXT("no panel hidden at spawn"));
	Expect(M_IntactLeft.AddOnArmor > 0.f && M_IntactRight.AddOnArmor > 0.f, TEXT("both sides carry add-on armour"));
	Expect(FMath::IsNearlyEqual(M_IntactLeft.RawArmor, M_IntactLeft.StructuralArmor + M_IntactLeft.AddOnArmor,
	                            AddOnArmorPanelsValidateConstants::ArmorTolerance), TEXT("intact left armour = structural + add-on"));
	// A shell at a panel only reaches the zone when the armour resolves a plate the zone covers.
	for (const int32 PanelIndex : M_LeftPanels)
	{
		const FAddOnArmorPanelsShot Shot = ResolveShotAtPanel(PanelIndex);
		const bool bCovered = Shot.PlateHit == EArmorPlate::Plate_SideLeft || Shot.PlateHit == EArmorPlate::Plate_SideLowerLeft;
		UE_LOG(LogRTSAddOnArmorPanelsValidate, Display, TEXT("RTS_ADDON_PANELS %s panel %d centre resolves %s raw=%.2f"),
		       bCovered ? TEXT("covered") : TEXT("NOTE uncovered"), PanelIndex, *UEnum::GetValueAsString(Shot.PlateHit),
		       Shot.RawArmor);
		if (bCovered && Shot.PlateRegistrationId != INDEX_NONE)
		{
			M_CoveredLeftPanels.Add(PanelIndex);
		}
	}
	Expect(M_CoveredLeftPanels.Contains(M_LeftPanels[M_LeftPanels.Num() / 2]), TEXT("the middle left panel is in front of a covered plate"));
	ExportPictures(TEXT("0_Intact"));
}

void UAddOnArmorPanelsValidateSubsystem::CheckFirstHit()
{
	const int32 HitPanel = M_LeftPanels[M_LeftPanels.Num() / 2];
	const int32 Mask = GetHiddenPanelMask();
	const FAddOnArmorPanelsSideReading Left = ReadSide(true);
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display, TEXT("RTS_ADDON_PANELS first hit state=%d mask=0x%x left raw=%.2f"),
	       static_cast<int32>(GetModuleState(M_LeftModuleId)), Mask, Left.RawArmor);
	Expect(GetModuleState(M_LeftModuleId) == EVehicleModuleState::Damaged, TEXT("first hit turns the left zone yellow"));
	Expect((Mask & (1 << HitPanel)) != 0, TEXT("the hit panel is hidden"));
	Expect(AddOnArmorPanelsValidateConstants::CountBits(Mask & M_LeftPanelMask) >= 1, TEXT("a damaged zone hides a panel"));
	Expect(FMath::IsNearlyEqual(Left.RawArmor, Left.StructuralArmor + Left.AddOnArmor * AddOnArmorPanelsValidateConstants::DamagedContribution,
	                            AddOnArmorPanelsValidateConstants::ArmorTolerance), TEXT("yellow zone gives half its add-on armour"));
	M_PreviousHiddenLeftCount = AddOnArmorPanelsValidateConstants::CountBits(Mask & M_LeftPanelMask);
	ExportPictures(TEXT("1_FirstHit"));
}

void UAddOnArmorPanelsValidateSubsystem::CheckDestroyed()
{
	const int32 Mask = GetHiddenPanelMask();
	const FAddOnArmorPanelsSideReading Left = ReadSide(true);
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display, TEXT("RTS_ADDON_PANELS destroyed after %d hits mask=0x%x left raw=%.2f"),
	       M_HitSerial, Mask, Left.RawArmor);
	Expect((Mask & M_LeftPanelMask) == M_LeftPanelMask, TEXT("a destroyed zone hides all its panels"));
	Expect(FMath::IsNearlyEqual(Left.RawArmor, Left.StructuralArmor, AddOnArmorPanelsValidateConstants::ArmorTolerance),
	       TEXT("red zone gives no add-on armour"));
	ExportPictures(TEXT("2_Destroyed"));
}

void UAddOnArmorPanelsValidateSubsystem::CheckCrewRepair()
{
	const int32 HiddenLeft = AddOnArmorPanelsValidateConstants::CountBits(GetHiddenPanelMask() & M_LeftPanelMask);
	const FAddOnArmorPanelsSideReading Left = ReadSide(true);
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display, TEXT("RTS_ADDON_PANELS crew repair state=%d hidden_left=%d left raw=%.2f"),
	       static_cast<int32>(GetModuleState(M_LeftModuleId)), HiddenLeft, Left.RawArmor);
	Expect(GetModuleState(M_LeftModuleId) == EVehicleModuleState::Damaged, TEXT("crew repair brings the zone to yellow"));
	Expect(HiddenLeft >= 1 && HiddenLeft < M_LeftPanels.Num(), TEXT("a repaired yellow zone shows some panels again"));
	ExportPictures(TEXT("3_CrewRepair"));
}

void UAddOnArmorPanelsValidateSubsystem::CheckFullService()
{
	const FAddOnArmorPanelsSideReading Left = ReadSide(true);
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display, TEXT("RTS_ADDON_PANELS full service mask=0x%x left raw=%.2f"),
	       GetHiddenPanelMask(), Left.RawArmor);
	Expect(GetHiddenPanelMask() == 0, TEXT("a full service brings every panel back"));
	Expect(FMath::IsNearlyEqual(Left.RawArmor, M_IntactLeft.RawArmor, AddOnArmorPanelsValidateConstants::ArmorTolerance),
	       TEXT("a full service restores the intact armour"));
	ExportPictures(TEXT("4_FullService"));
}

void UAddOnArmorPanelsValidateSubsystem::CheckRightSideUntouched()
{
	Expect(GetModuleState(M_RightModuleId) == EVehicleModuleState::Healthy, TEXT("the right zone stays healthy"));
	Expect((GetHiddenPanelMask() & M_RightPanelMask) == 0, TEXT("no right panel is hidden"));
	const FAddOnArmorPanelsSideReading Right = ReadSide(false);
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display,
	       TEXT("RTS_ADDON_PANELS right now %s raw=%.2f structural=%.2f addon=%.2f (intact %s raw=%.2f)"),
	       *Right.PlateName, Right.RawArmor, Right.StructuralArmor, Right.AddOnArmor, *M_IntactRight.PlateName,
	       M_IntactRight.RawArmor);
	Expect(FMath::IsNearlyEqual(Right.RawArmor, M_IntactRight.RawArmor, AddOnArmorPanelsValidateConstants::ArmorTolerance),
	       TEXT("the right side keeps its armour"));
}

void UAddOnArmorPanelsValidateSubsystem::StartModuleSetupOnce()
{
	using namespace AddOnArmorPanelsValidateConstants;
	if (not bM_StartModuleSetup || bM_DidStartModuleSetup)
	{
		return;
	}
	bM_DidStartModuleSetup = true;
	UFunction* const StartModuleSetup = M_Tank->FindFunction(StartModuleSetupFunctionName);
	if (StartModuleSetup == nullptr)
	{
		Expect(false, TEXT("the tank has the module setup function"));
		return;
	}
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Warning,
	       TEXT("RTS_ADDON_PANELS NOTE the Blueprint did not start its vehicle module setup; the test starts it"));
	M_Tank->ProcessEvent(StartModuleSetup, nullptr);
}

void UAddOnArmorPanelsValidateSubsystem::LogModuleWaitReason()
{
	constexpr float WaitLogIntervalSeconds = 5.f;
	const float SecondsInStep = GetGameSecondsInStep();
	if (SecondsInStep < M_NextWaitLogSeconds)
	{
		return;
	}
	M_NextWaitLogSeconds = SecondsInStep + WaitLogIntervalSeconds;
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display,
	       TEXT("RTS_ADDON_PANELS waiting seconds=%.0f modules_finalized=%d shader_jobs=%d async_loading=%d"),
	       SecondsInStep, M_Armor->GetAreModulesFinalized(),
	       GShaderCompilingManager != nullptr ? GShaderCompilingManager->GetNumRemainingJobs() : 0, IsAsyncLoading());
}

void UAddOnArmorPanelsValidateSubsystem::TickFirstShotDebris()
{
	using namespace AddOnArmorPanelsValidateConstants;
	if (M_FirstShotGameSeconds < 0.f)
	{
		// The first shell is fired on the first tick of its step.
		if (M_Step == EAddOnArmorPanelsValidateStep::FirstHit && bM_StepActionDone)
		{
			M_FirstShotGameSeconds = M_StepStartGameSeconds;
		}
		return;
	}
	const float SecondsSinceShot = GetWorld()->GetTimeSeconds() - M_FirstShotGameSeconds;
	if (M_FirstDebrisBurst.IsValid() && not bM_DidDebrisSettleCheck
		&& FParse::Param(FCommandLine::Get(), TEXT("AddOnArmorPanelsTraceDebris")))
	{
		CheckRestingDebris(*M_FirstDebrisBurst, false);
	}
	if (not bM_DidFlightPicture && SecondsSinceShot >= DebrisFlightPictureSeconds)
	{
		bM_DidFlightPicture = true;
		M_FirstDebrisBurst = FindActiveDebrisBurst();
		const int32 Particles = M_FirstDebrisBurst.IsValid() ? CountParticles(*M_FirstDebrisBurst) : 0;
		const int32 HiddenLeft = CountBits(GetHiddenPanelMask() & M_LeftPanelMask);
		const bool bIsTankHidden = M_Tank->IsHidden();
		UE_LOG(LogRTSAddOnArmorPanelsValidate, Display,
		       TEXT("RTS_ADDON_PANELS first hit debris in flight particles=%d hidden_left=%d tank_hidden_by_fog=%d"),
		       Particles, HiddenLeft, bIsTankHidden ? 1 : 0);
		ExportPictures(TEXT("1_FirstHit_Flight"));
		if (bIsTankHidden)
		{
			// Enemy vehicles of the test stay in fog of war: no debris may show where they are.
			Expect(Particles == 0, TEXT("a tank hidden by fog of war throws no debris"));
			bM_DidDebrisSettleCheck = true;
			return;
		}
		Expect(Particles == HiddenLeft, TEXT("each panel the first hit hides flies off as one debris piece"));
	}
	if (not bM_DidDebrisSettleCheck && SecondsSinceShot >= DebrisSettleCheckSeconds)
	{
		bM_DidDebrisSettleCheck = true;
		Expect(M_FirstDebrisBurst.IsValid(), TEXT("the first debris burst lasts until its pieces have settled"));
		if (M_FirstDebrisBurst.IsValid())
		{
			CheckRestingDebris(*M_FirstDebrisBurst, true);
		}
	}
}

UNiagaraComponent* UAddOnArmorPanelsValidateSubsystem::FindActiveDebrisBurst() const
{
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent* const Burst = *It;
		const UNiagaraSystem* const System = Burst->GetAsset();
		if (Burst->GetWorld() == GetWorld() && Burst->IsActive() && IsValid(System)
			&& System->GetPathName() == AddOnArmorPanelDebris::SystemPath)
		{
			return Burst;
		}
	}
	return nullptr;
}

int32 UAddOnArmorPanelsValidateSubsystem::CountParticles(const UNiagaraComponent& Burst)
{
	const FNiagaraSystemInstanceControllerConstPtr Controller = Burst.GetSystemInstanceController();
	const FNiagaraSystemInstance* const Instance = Controller.IsValid() ? Controller->GetSystemInstance_Unsafe() : nullptr;
	if (Instance == nullptr)
	{
		return 0;
	}
	int32 ParticleCount = 0;
	for (const FNiagaraEmitterInstanceRef& Emitter : Instance->GetEmitters())
	{
		ParticleCount += Emitter->GetNumParticles();
	}
	return ParticleCount;
}

void UAddOnArmorPanelsValidateSubsystem::CheckRestingDebris(const UNiagaraComponent& Burst, const bool bExpectResting)
{
	using namespace AddOnArmorPanelsValidateConstants;
	const FNiagaraSystemInstanceControllerConstPtr Controller = Burst.GetSystemInstanceController();
	const FNiagaraSystemInstance* const Instance = Controller.IsValid() ? Controller->GetSystemInstance_Unsafe() : nullptr;
	if (Instance == nullptr || Instance->GetEmitters().IsEmpty())
	{
		Expect(not bExpectResting, TEXT("the first debris burst has particles to check"));
		return;
	}
	const FNiagaraDataSet& DataSet = Instance->GetEmitters()[0]->GetParticleData();
	const int32 ParticleCount = DataSet.GetCurrentData() != nullptr ? DataSet.GetCurrentData()->GetNumInstances() : 0;
	const float GroundHeight = Burst.GetOverrideParameters().GetParameterValue<float>(
		FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.GroundHeight")));
	const auto PositionReader = FNiagaraDataSetAccessor<FNiagaraPosition>::CreateReader(DataSet, TEXT("Position"));
	const auto OrientationReader = FNiagaraDataSetAccessor<FQuat4f>::CreateReader(DataSet, TEXT("MeshOrientation"));
	const auto RestingReader = FNiagaraDataSetAccessor<float>::CreateReader(DataSet, TEXT("Resting"));
	const auto ScaleReader = FNiagaraDataSetAccessor<FVector3f>::CreateReader(DataSet, TEXT("Scale"));
	const auto AgeReader = FNiagaraDataSetAccessor<float>::CreateReader(DataSet, TEXT("Age"));
	const auto VelocityReader = FNiagaraDataSetAccessor<FVector3f>::CreateReader(DataSet, TEXT("Velocity"));
	for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex)
	{
		const FVector3f Position = PositionReader.GetSafe(ParticleIndex, FVector3f::ZeroVector);
		const FVector3f Normal = OrientationReader.GetSafe(ParticleIndex, FQuat4f::Identity).RotateVector(FVector3f(0.f, 1.f, 0.f));
		const float Resting = RestingReader.GetSafe(ParticleIndex, -1.f);
		UE_LOG(LogRTSAddOnArmorPanelsValidate, Display,
		       TEXT("RTS_ADDON_PANELS debris piece=%d age=%.2f position=%s velocity=%s ground=%.1f resting=%.0f normal=%s scale=%s"),
		       ParticleIndex, AgeReader.GetSafe(ParticleIndex, -1.f), *Position.ToCompactString(),
		       *VelocityReader.GetSafe(ParticleIndex, FVector3f::ZeroVector).ToCompactString(), GroundHeight, Resting,
		       *Normal.ToCompactString(), *ScaleReader.GetSafe(ParticleIndex, FVector3f::ZeroVector).ToCompactString());
		if (not bExpectResting)
		{
			continue;
		}
		Expect(Resting > 0.5f, TEXT("a debris piece comes to rest"));
		Expect(FMath::Abs(Normal.Z) >= MinRestingNormalZ, TEXT("a resting debris piece lies flat"));
		Expect(Position.Z - GroundHeight <= MaxRestingHeightAboveGround && Position.Z >= GroundHeight,
		       TEXT("a resting debris piece lies on its ground plane"));
	}
}

void UAddOnArmorPanelsValidateSubsystem::Expect(const bool bCondition, const FString& What)
{
	if (bCondition)
	{
		UE_LOG(LogRTSAddOnArmorPanelsValidate, Display, TEXT("RTS_ADDON_PANELS PASS %s"), *What);
		return;
	}
	++M_FailureCount;
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Error, TEXT("RTS_ADDON_PANELS FAIL %s"), *What);
}

void UAddOnArmorPanelsValidateSubsystem::Finish()
{
	M_Step = EAddOnArmorPanelsValidateStep::Done;
	UE_LOG(LogRTSAddOnArmorPanelsValidate, Display, TEXT("RTS_ADDON_PANELS RESULT %s failures=%d"),
	       M_FailureCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailureCount);
	FPlatformMisc::RequestExit(false, TEXT("AddOnArmorPanelsValidate"));
}

#else

bool UAddOnArmorPanelsValidateSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UAddOnArmorPanelsValidateSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UAddOnArmorPanelsValidateSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UAddOnArmorPanelsValidateSubsystem::Tick(float DeltaTime)
{
}

TStatId UAddOnArmorPanelsValidateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAddOnArmorPanelsValidateSubsystem, STATGROUP_Tickables);
}

#endif // RTS_WITH_MAP_TESTS
