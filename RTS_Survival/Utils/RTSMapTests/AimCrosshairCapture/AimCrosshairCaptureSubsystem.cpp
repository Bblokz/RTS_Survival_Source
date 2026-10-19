// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "AimCrosshairCaptureSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerAimAbilitiy/PlayerAimAbility.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAimCrosshairCapture, Log, All);

namespace AimCrosshairCaptureConstants
{
	const TCHAR* const EnableSwitch = TEXT("AimCrosshairCapture");
	// Cases separated by '+': Label=AimType:Radius[:/Game/Material/Override], e.g. Small=SmallCrosshair:200.
	const TCHAR* const CasesSwitch = TEXT("AimCrosshairCases=");
	const TCHAR* const DirectorySwitch = TEXT("AimCrosshairDir=");
	// Spots separated by ';', each X,Y; they are dropped onto the ground.
	const TCHAR* const SpotsSwitch = TEXT("AimCrosshairAt=");
	const TCHAR* const AimActorClassSwitch = TEXT("AimCrosshairClass=");
	// Turns the aim actor away from the camera's forward, in degrees, to see direction marks off-axis.
	const TCHAR* const AimYawSwitch = TEXT("AimCrosshairYaw=");
	// Puts the new crosshair materials and this UnitsPerScale on the spawned aim actor, for checking them before
	// the aim cursor Blueprint is wired (PythonAutomation/TargetCrosshairs/unreal_setup_crosshairs.py wire).
	const TCHAR* const WireUnitsPerScaleSwitch = TEXT("AimCrosshairWireUnitsPerScale=");
	const TCHAR* const CrosshairMaterialFolder = TEXT("/Game/RTS_Survival/Materials/AimMaterials/Crosshairs/");
	const TCHAR* const CrosshairMaterialPerAimType[][2] = {
		{TEXT("SmallCrosshair"), TEXT("MI_AimCrosshair_Small")},
		{TEXT("MediumCrosshair"), TEXT("MI_AimCrosshair_Medium")},
		{TEXT("LargeCrosshair"), TEXT("MI_AimCrosshair_Large")},
		{TEXT("LargeComplexCrosshair"), TEXT("MI_AimCrosshair_LargeComplex")},
		{TEXT("AntiInfantryGrenade"), TEXT("MI_AimCrosshair_GrenadeAntiInfantry")},
		{TEXT("AntiVehicleGrenade"), TEXT("MI_AimCrosshair_GrenadeAntiVehicle")}
	};
	const FName MaterialMapPropertyName(TEXT("MaterialPerAbilityAim"));
	const FName UnitsPerScalePropertyName(TEXT("UnitsPerScale"));
	const TCHAR* const SkyFillSwitch = TEXT("AimCrosshairSkyFill=");
	const TCHAR* const ExposureBiasSwitch = TEXT("AimCrosshairExposureBias=");
	const TCHAR* const DefaultAimActorClass =
		TEXT("/Game/RTS_Survival/Blueprints/Player/BP_PlayerAimAbilityCursor.BP_PlayerAimAbilityCursor_C");
	const TCHAR* const DefaultCases =
		TEXT("Small=SmallCrosshair:200+Medium=MediumCrosshair:700+Large=LargeCrosshair:1400")
		TEXT("+LargeComplex=LargeComplexCrosshair:1600")
		TEXT("+GrenadeAntiInfantry=AntiInfantryGrenade:500+GrenadeAntiVehicle=AntiVehicleGrenade:350");
	const TCHAR* const AnchorClassPrefix = TEXT("BP_UnitImage_LandscapeAnchor");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	const FName RadiusParameterName(TEXT("Radius"));
	const TCHAR* const CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const CylinderMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

	// Real seconds: the HUD and start widget need a moment before the game can be started.
	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 60.0;
	// Game seconds.
	constexpr float SettleSeconds = 3.f;
	constexpr float MaxSettleSeconds = 120.f;
	// Long enough for temporal anti-aliasing and auto exposure to settle on the new reticle.
	constexpr float CaseSettleSeconds = 1.5f;
	constexpr float WarmUpSecondsPerCase = 0.4f;
	constexpr float MaxCaseSeconds = 60.f;

	constexpr int32 PictureWidth = 1920;
	constexpr int32 PictureHeight = 1080;
	constexpr float GroundTraceHalfHeight = 5000.f;
	constexpr float ExposureAdaptSpeed = 10.f;

	// The game camera is a spring arm of 3000 at -70 degrees with a 90 degree field of view; the player zooms
	// between roughly the close and far arm lengths.
	const FAimCrosshairCaptureView Views[] = {
		{TEXT("Game"), -70.f, 3000.f, 90.f},
		{TEXT("Close"), -70.f, 1600.f, 90.f},
		{TEXT("Far"), -70.f, 5500.f, 90.f}
	};

	// A medium tank and a soldier inside the larger reticles, to judge how the reticle reads over units.
	const FVector TankBlockOffset(350.f, -450.f, 120.f);
	const FVector TankBlockScale(6.5f, 3.2f, 2.4f);
	const FVector SoldierBlockOffset(-120.f, 260.f, 90.f);
	const FVector SoldierBlockScale(0.5f, 0.5f, 1.8f);

	bool GetIsStillCompilingOrLoading()
	{
		const bool bIsCompilingShaders = GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0;
		// Freshly imported textures build on first use; until then the reticle samples a placeholder.
		bool bIsCompilingTextures = false;
#if WITH_EDITOR
		bIsCompilingTextures = FTextureCompilingManager::Get().GetNumRemainingTextures() > 0;
#endif
		return bIsCompilingShaders || bIsCompilingTextures || IsAsyncLoading();
	}
}

bool UAimCrosshairCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), AimCrosshairCaptureConstants::EnableSwitch);
}

void UAimCrosshairCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	LoadCases();
	if (M_Cases.IsEmpty())
	{
		UE_LOG(LogRTSAimCrosshairCapture, Error, TEXT("RTS_AIM_CROSSHAIR no valid cases"));
		Finish();
		return;
	}
	M_OutputDirectory = FPaths::ProjectSavedDir() / TEXT("AimCrosshairs");
	FParse::Value(FCommandLine::Get(), AimCrosshairCaptureConstants::DirectorySwitch, M_OutputDirectory);
	FParse::Value(FCommandLine::Get(), AimCrosshairCaptureConstants::AimYawSwitch, M_AimYawOffset);
	// Error dialogs are answered automatically in unattended runs, but each one stalls a frame; log them instead.
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	EnterPhase(EAimCrosshairCapturePhase::WaitingForGameStart);
	UE_LOG(LogRTSAimCrosshairCapture, Display, TEXT("RTS_AIM_CROSSHAIR started: %d cases, writing to %s"),
	       M_Cases.Num(), *M_OutputDirectory);
}

void UAimCrosshairCaptureSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

void UAimCrosshairCaptureSubsystem::Tick(float DeltaTime)
{
	if (M_Phase == EAimCrosshairCapturePhase::Inactive || M_Phase == EAimCrosshairCapturePhase::Done)
	{
		return;
	}
	if (M_Phase != EAimCrosshairCapturePhase::WaitingForGameStart)
	{
		KeepGameRunning();
		AimCameras();
	}
	switch (M_Phase)
	{
	case EAimCrosshairCapturePhase::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case EAimCrosshairCapturePhase::Settling:
		TickSettling();
		break;
	case EAimCrosshairCapturePhase::WarmingUp:
		TickWarmingUp();
		break;
	case EAimCrosshairCapturePhase::ShowingCase:
		TickShowingCase();
		break;
	default:
		break;
	}
}

TStatId UAimCrosshairCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAimCrosshairCaptureSubsystem, STATGROUP_Tickables);
}

void UAimCrosshairCaptureSubsystem::TickWaitingForGameStart()
{
	if (GetRealSecondsInPhase() > AimCrosshairCaptureConstants::MaxStartSeconds)
	{
		UE_LOG(LogRTSAimCrosshairCapture, Error, TEXT("RTS_AIM_CROSSHAIR could not start the game"));
		Finish();
		return;
	}
	if (GetRealSecondsInPhase() < AimCrosshairCaptureConstants::HudBuildSeconds || not TryStartGame())
	{
		return;
	}
	SpawnSkyFill();
	if (not LoadSpots() || not SpawnAimActor() || not CreateCameras())
	{
		Finish();
		return;
	}
	for (const FVector& Spot : M_Spots)
	{
		SpawnScaleReferences(Spot);
	}
	EnterPhase(EAimCrosshairCapturePhase::Settling);
}

void UAimCrosshairCaptureSubsystem::TickSettling()
{
	using namespace AimCrosshairCaptureConstants;
	const bool bIsStillPreparing = GetIsStillCompilingOrLoading() && GetGameSecondsInPhase() < MaxSettleSeconds;
	if (GetGameSecondsInPhase() < SettleSeconds || bIsStillPreparing)
	{
		return;
	}
	if (not ShowFirstShowableCase())
	{
		Finish();
		return;
	}
	EnterPhase(EAimCrosshairCapturePhase::WarmingUp);
}

void UAimCrosshairCaptureSubsystem::TickWarmingUp()
{
	if (GetGameSecondsInPhase() < AimCrosshairCaptureConstants::WarmUpSecondsPerCase)
	{
		return;
	}
	const bool bIsLastCaseOfSpot = not M_Cases.IsValidIndex(M_CaseIndex + 1);
	if (not bIsLastCaseOfSpot && AdvanceCase() && ShowFirstShowableCase())
	{
		EnterPhase(EAimCrosshairCapturePhase::WarmingUp);
		return;
	}
	FinishPendingCompilation();
	M_CaseIndex = 0;
	M_SpotIndex = 0;
	if (not ShowFirstShowableCase())
	{
		Finish();
		return;
	}
	EnterPhase(EAimCrosshairCapturePhase::ShowingCase);
}

void UAimCrosshairCaptureSubsystem::FinishPendingCompilation() const
{
#if WITH_EDITOR
	FTextureCompilingManager::Get().FinishAllCompilation();
#endif
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
}

void UAimCrosshairCaptureSubsystem::TickShowingCase()
{
	using namespace AimCrosshairCaptureConstants;
	const bool bIsStillPreparing = GetIsStillCompilingOrLoading() && GetGameSecondsInPhase() < MaxCaseSeconds;
	if (GetGameSecondsInPhase() < CaseSettleSeconds || bIsStillPreparing)
	{
		return;
	}
	ExportCameras();
	if (not AdvanceCase() || not ShowFirstShowableCase())
	{
		Finish();
		return;
	}
	EnterPhase(EAimCrosshairCapturePhase::ShowingCase);
}

void UAimCrosshairCaptureSubsystem::EnterPhase(const EAimCrosshairCapturePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
	M_PhaseStartGameSeconds = GetGameSeconds();
}

double UAimCrosshairCaptureSubsystem::GetRealSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartRealSeconds;
}

float UAimCrosshairCaptureSubsystem::GetGameSecondsInPhase() const
{
	return GetGameSeconds() - M_PhaseStartGameSeconds;
}

float UAimCrosshairCaptureSubsystem::GetGameSeconds() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void UAimCrosshairCaptureSubsystem::LoadCases()
{
	FString CasesText = AimCrosshairCaptureConstants::DefaultCases;
	// Not stopping on separators: case texts contain ':' and '/'.
	FParse::Value(FCommandLine::Get(), AimCrosshairCaptureConstants::CasesSwitch, CasesText, false);
	TArray<FString> CaseTexts;
	CasesText.ParseIntoArray(CaseTexts, TEXT("+"), true);
	for (const FString& CaseText : CaseTexts)
	{
		FAimCrosshairCaptureCase CaptureCase;
		if (TryParseCase(CaseText, CaptureCase))
		{
			M_Cases.Add(CaptureCase);
			continue;
		}
		UE_LOG(LogRTSAimCrosshairCapture, Error, TEXT("RTS_AIM_CROSSHAIR cannot read case '%s'"), *CaseText);
	}
}

bool UAimCrosshairCaptureSubsystem::TryParseCase(const FString& CaseText, FAimCrosshairCaptureCase& OutCase) const
{
	FString Label;
	FString Definition;
	if (not CaseText.Split(TEXT("="), &Label, &Definition))
	{
		return false;
	}
	TArray<FString> Parts;
	Definition.ParseIntoArray(Parts, TEXT(":"), true);
	if (Parts.Num() < 2)
	{
		return false;
	}
	const int64 AimTypeValue = StaticEnum<EPlayerAimAbilityTypes>()->GetValueByNameString(Parts[0]);
	if (AimTypeValue == INDEX_NONE)
	{
		return false;
	}
	OutCase.Label = Label;
	OutCase.AimType = static_cast<EPlayerAimAbilityTypes>(AimTypeValue);
	OutCase.Radius = FCString::Atof(*Parts[1]);
	// The override is a package path; rejoin it in case it was split on a ':'.
	if (Parts.Num() > 2)
	{
		OutCase.MaterialOverridePath = FString::Join(TArrayView<const FString>(Parts).RightChop(2), TEXT(":"));
	}
	return OutCase.Radius > 1.f;
}

bool UAimCrosshairCaptureSubsystem::LoadSpots()
{
	using namespace AimCrosshairCaptureConstants;
	FString SpotsText;
	if (FParse::Value(FCommandLine::Get(), SpotsSwitch, SpotsText, false))
	{
		TArray<FString> SpotTexts;
		SpotsText.ParseIntoArray(SpotTexts, TEXT(";"), true);
		for (const FString& SpotText : SpotTexts)
		{
			TArray<FString> Coordinates;
			SpotText.ParseIntoArray(Coordinates, TEXT(","), true);
			if (Coordinates.Num() >= 2)
			{
				const FVector Spot(FCString::Atof(*Coordinates[0]), FCString::Atof(*Coordinates[1]), 0.f);
				M_Spots.Add(DropToGround(Spot));
			}
		}
	}
	for (TActorIterator<AActor> It(GetWorld()); It && M_Spots.IsEmpty(); ++It)
	{
		if (It->GetClass()->GetName().StartsWith(AnchorClassPrefix))
		{
			M_Spots.Add(DropToGround(It->GetActorLocation()));
			M_ViewYaw = It->GetActorRotation().Yaw;
		}
	}
	for (TActorIterator<APlayerStart> It(GetWorld()); It && M_Spots.IsEmpty(); ++It)
	{
		M_Spots.Add(DropToGround(It->GetActorLocation()));
	}
	if (M_Spots.IsEmpty())
	{
		UE_LOG(LogRTSAimCrosshairCapture, Error, TEXT("RTS_AIM_CROSSHAIR no -AimCrosshairAt, %s or player start"),
		       AnchorClassPrefix);
		return false;
	}
	for (const FVector& Spot : M_Spots)
	{
		UE_LOG(LogRTSAimCrosshairCapture, Display, TEXT("RTS_AIM_CROSSHAIR spot %s"), *Spot.ToCompactString());
	}
	return true;
}

bool UAimCrosshairCaptureSubsystem::TryStartGame()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	M_PlayerController = Cast<ACPPController>(World->GetFirstPlayerController());
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
	UE_LOG(LogRTSAimCrosshairCapture, Display, TEXT("RTS_AIM_CROSSHAIR game started"));
	return true;
}

void UAimCrosshairCaptureSubsystem::KeepGameRunning() const
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
			Picker->WidgetTree->FindWidget(AimCrosshairCaptureConstants::DifficultyButtonName));
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

FVector UAimCrosshairCaptureSubsystem::DropToGround(const FVector& Location) const
{
	const FVector TraceOffset(0.f, 0.f, AimCrosshairCaptureConstants::GroundTraceHalfHeight);
	FHitResult GroundHit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AimCrosshairCaptureGround), false);
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, Location + TraceOffset, Location - TraceOffset,
	                                         ECC_Visibility, QueryParams))
	{
		return GroundHit.ImpactPoint;
	}
	return Location;
}

void UAimCrosshairCaptureSubsystem::SpawnScaleReferences(const FVector& Spot) const
{
	using namespace AimCrosshairCaptureConstants;
	UStaticMesh* const CubeMesh = LoadObject<UStaticMesh>(nullptr, CubeMeshPath);
	UStaticMesh* const CylinderMesh = LoadObject<UStaticMesh>(nullptr, CylinderMeshPath);
	const FRotator ViewRotation(0.f, M_ViewYaw, 0.f);
	const TPair<UStaticMesh*, FTransform> Blocks[] = {
		{CubeMesh, FTransform(ViewRotation, Spot + ViewRotation.RotateVector(TankBlockOffset), TankBlockScale)},
		{CylinderMesh, FTransform(ViewRotation, Spot + ViewRotation.RotateVector(SoldierBlockOffset), SoldierBlockScale)}
	};
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const TPair<UStaticMesh*, FTransform>& Block : Blocks)
	{
		AStaticMeshActor* const BlockActor = GetWorld()->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), Block.Value, SpawnParameters);
		if (not IsValid(BlockActor) || not IsValid(Block.Key))
		{
			continue;
		}
		BlockActor->SetMobility(EComponentMobility::Movable);
		BlockActor->GetStaticMeshComponent()->SetStaticMesh(Block.Key);
		BlockActor->SetActorScale3D(Block.Value.GetScale3D());
		BlockActor->SetActorEnableCollision(false);
	}
}

void UAimCrosshairCaptureSubsystem::SpawnSkyFill()
{
	float SkyFillIntensity = 0.f;
	if (not FParse::Value(FCommandLine::Get(), AimCrosshairCaptureConstants::SkyFillSwitch, SkyFillIntensity))
	{
		return;
	}
	UTextureCube* MapSkyCubemap = nullptr;
	for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
	{
		USkyLightComponent* MapSkyLight = It->GetLightComponent();
		MapSkyCubemap = MapSkyLight->Cubemap;
		// Only the newest registered sky light renders; the map's one would replace the fill a frame later.
		MapSkyLight->SetVisibility(false);
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkyLight* SkyFill = GetWorld()->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity,
	                                                        SpawnParameters);
	if (not IsValid(SkyFill))
	{
		return;
	}
	USkyLightComponent* SkyLight = SkyFill->GetLightComponent();
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SourceType = IsValid(MapSkyCubemap) ? SLS_SpecifiedCubemap : SLS_CapturedScene;
	SkyLight->SetCubemap(MapSkyCubemap);
	SkyLight->SetIntensity(SkyFillIntensity);
	SkyLight->RecaptureSky();
	M_SkyFill = SkyFill;
}

bool UAimCrosshairCaptureSubsystem::SpawnAimActor()
{
	FString ClassPath = AimCrosshairCaptureConstants::DefaultAimActorClass;
	FParse::Value(FCommandLine::Get(), AimCrosshairCaptureConstants::AimActorClassSwitch, ClassPath);
	UClass* const AimActorClass = LoadClass<APlayerAimAbility>(nullptr, *ClassPath);
	if (not IsValid(AimActorClass))
	{
		UE_LOG(LogRTSAimCrosshairCapture, Error, TEXT("RTS_AIM_CROSSHAIR cannot load aim actor class %s"), *ClassPath);
		return false;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Our own instance: the controller's one follows the cursor every tick.
	M_AimActor = GetWorld()->SpawnActor<APlayerAimAbility>(AimActorClass, FTransform::Identity, SpawnParameters);
	if (not GetIsValidAimActor())
	{
		return false;
	}
	if (M_PlayerController.IsValid())
	{
		M_AimActor->InitPlayerAimAbility(M_PlayerController.Get());
	}
	ApplyTestWiring();
	return true;
}

void UAimCrosshairCaptureSubsystem::ApplyTestWiring() const
{
	using namespace AimCrosshairCaptureConstants;
	float UnitsPerScale = 0.f;
	if (not FParse::Value(FCommandLine::Get(), WireUnitsPerScaleSwitch, UnitsPerScale))
	{
		return;
	}
	// Protected Blueprint defaults: set through reflection so the gameplay class needs no test hooks.
	UClass* const AimActorClass = M_AimActor->GetClass();
	FProperty* const MapProperty = AimActorClass->FindPropertyByName(MaterialMapPropertyName);
	FFloatProperty* const UnitsProperty = CastField<FFloatProperty>(
		AimActorClass->FindPropertyByName(UnitsPerScalePropertyName));
	if (MapProperty == nullptr || UnitsProperty == nullptr)
	{
		UE_LOG(LogRTSAimCrosshairCapture, Error, TEXT("RTS_AIM_CROSSHAIR cannot find the aim actor's wiring properties"));
		return;
	}
	TArray<FString> Entries;
	for (const auto& AimTypeAndMaterial : CrosshairMaterialPerAimType)
	{
		const FString MaterialPath = FString(CrosshairMaterialFolder) + AimTypeAndMaterial[1] + TEXT(".") +
			AimTypeAndMaterial[1];
		Entries.Add(FString::Printf(TEXT("(%s, \"%s\")"), AimTypeAndMaterial[0], *MaterialPath));
	}
	const FString MapText = TEXT("(") + FString::Join(Entries, TEXT(",")) + TEXT(")");
	MapProperty->ImportText_InContainer(*MapText, M_AimActor.Get(), M_AimActor.Get(), PPF_None);
	UnitsProperty->SetPropertyValue_InContainer(M_AimActor.Get(), UnitsPerScale);
	UE_LOG(LogRTSAimCrosshairCapture, Display, TEXT("RTS_AIM_CROSSHAIR test wiring: UnitsPerScale %.2f, map %s"),
	       UnitsPerScale, *MapText);
}

bool UAimCrosshairCaptureSubsystem::GetIsValidAimActor() const
{
	if (IsValid(M_AimActor))
	{
		return true;
	}
	UE_LOG(LogRTSAimCrosshairCapture, Error, TEXT("RTS_AIM_CROSSHAIR the aim actor is not valid"));
	return false;
}

bool UAimCrosshairCaptureSubsystem::CreateCameras()
{
	using namespace AimCrosshairCaptureConstants;
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FAimCrosshairCaptureView& View : Views)
	{
		ASceneCapture2D* Capture = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(),
		                                                               FTransform::Identity, SpawnParameters);
		UTextureRenderTarget2D* RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
			World, PictureWidth, PictureHeight, RTF_RGBA8_SRGB);
		if (not IsValid(Capture) || not IsValid(RenderTarget))
		{
			UE_LOG(LogRTSAimCrosshairCapture, Error, TEXT("RTS_AIM_CROSSHAIR camera %s could not be created"),
			       *View.Name);
			return false;
		}
		ConfigureCaptureComponent(*Capture->GetCaptureComponent2D(), *RenderTarget, View);
		FAimCrosshairCaptureCamera& Camera = M_Cameras.AddDefaulted_GetRef();
		Camera.View = View;
		Camera.Capture = Capture;
		Camera.RenderTarget.Reset(RenderTarget);
	}
	return true;
}

void UAimCrosshairCaptureSubsystem::ConfigureCaptureComponent(USceneCaptureComponent2D& CaptureComponent,
                                                              UTextureRenderTarget2D& RenderTarget,
                                                              const FAimCrosshairCaptureView& View) const
{
	using namespace AimCrosshairCaptureConstants;
	CaptureComponent.TextureTarget = &RenderTarget;
	CaptureComponent.CaptureSource = SCS_FinalColorLDR;
	CaptureComponent.FOVAngle = View.FieldOfView;
	// Rendering every frame with kept state lets auto exposure and temporal AA settle like the game view.
	CaptureComponent.bCaptureEveryFrame = true;
	CaptureComponent.bCaptureOnMovement = false;
	CaptureComponent.bAlwaysPersistRenderingState = true;
	// The fog of war is a post-process material; the pictures show the reticle, not what the player may see.
	CaptureComponent.ShowFlags.SetPostProcessMaterial(false);
	FPostProcessSettings& Settings = CaptureComponent.PostProcessSettings;
	Settings.bOverride_AutoExposureSpeedUp = true;
	Settings.AutoExposureSpeedUp = ExposureAdaptSpeed;
	Settings.bOverride_AutoExposureSpeedDown = true;
	Settings.AutoExposureSpeedDown = ExposureAdaptSpeed;
	float ExposureBias = 0.f;
	if (FParse::Value(FCommandLine::Get(), ExposureBiasSwitch, ExposureBias))
	{
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = ExposureBias;
	}
	CaptureComponent.PostProcessBlendWeight = 1.f;
}

void UAimCrosshairCaptureSubsystem::AimCameras() const
{
	if (not M_Spots.IsValidIndex(M_SpotIndex))
	{
		return;
	}
	const FVector LookAt = M_Spots[M_SpotIndex];
	for (const FAimCrosshairCaptureCamera& Camera : M_Cameras)
	{
		if (not Camera.Capture.IsValid())
		{
			continue;
		}
		const FRotator CameraRotation(Camera.View.Pitch, M_ViewYaw, 0.f);
		const FVector CameraLocation = LookAt - CameraRotation.Vector() * Camera.View.Distance;
		Camera.Capture->SetActorLocationAndRotation(CameraLocation, CameraRotation);
	}
}

bool UAimCrosshairCaptureSubsystem::ShowCurrentCase()
{
	if (not M_Cases.IsValidIndex(M_CaseIndex) || not M_Spots.IsValidIndex(M_SpotIndex) || not GetIsValidAimActor())
	{
		return false;
	}
	const FAimCrosshairCaptureCase& CaptureCase = M_Cases[M_CaseIndex];
	// Facing away from the camera, like an ability fired from a unit at the bottom of the screen.
	M_AimActor->SetActorLocationAndRotation(M_Spots[M_SpotIndex], FRotator(0.f, M_ViewYaw + M_AimYawOffset, 0.f));
	M_AimActor->ShowGlobalAbilityRadius(CaptureCase.Radius, CaptureCase.AimType);
	if (not M_AimActor->IsPlayerAimActive())
	{
		UE_LOG(LogRTSAimCrosshairCapture, Error, TEXT("RTS_AIM_CROSSHAIR %s: the aim actor refused %s"),
		       *CaptureCase.Label, *UEnum::GetValueAsString(CaptureCase.AimType));
		return false;
	}
	ApplyMaterialOverride(CaptureCase);
	UE_LOG(LogRTSAimCrosshairCapture, Display, TEXT("RTS_AIM_CROSSHAIR showing %s (%s, radius %.0f) at spot %d"),
	       *CaptureCase.Label, *UEnum::GetValueAsString(CaptureCase.AimType), CaptureCase.Radius, M_SpotIndex);
	return true;
}

void UAimCrosshairCaptureSubsystem::ApplyMaterialOverride(const FAimCrosshairCaptureCase& CaptureCase) const
{
	if (CaptureCase.MaterialOverridePath.IsEmpty())
	{
		return;
	}
	UMaterialInterface* const Material = LoadObject<UMaterialInterface>(nullptr, *CaptureCase.MaterialOverridePath);
	UStaticMeshComponent* const AimMesh = M_AimActor->FindComponentByClass<UStaticMeshComponent>();
	if (not IsValid(Material) || not IsValid(AimMesh))
	{
		UE_LOG(LogRTSAimCrosshairCapture, Error, TEXT("RTS_AIM_CROSSHAIR %s: cannot apply override %s"),
		       *CaptureCase.Label, *CaptureCase.MaterialOverridePath);
		return;
	}
	UMaterialInstanceDynamic* const DynamicMaterial = UMaterialInstanceDynamic::Create(Material, M_AimActor);
	DynamicMaterial->SetScalarParameterValue(AimCrosshairCaptureConstants::RadiusParameterName, CaptureCase.Radius);
	AimMesh->SetMaterial(0, DynamicMaterial);
}

void UAimCrosshairCaptureSubsystem::ExportCameras() const
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || not M_Cases.IsValidIndex(M_CaseIndex))
	{
		return;
	}
	const FString& Label = M_Cases[M_CaseIndex].Label;
	for (const FAimCrosshairCaptureCamera& Camera : M_Cameras)
	{
		if (not Camera.RenderTarget.IsValid())
		{
			continue;
		}
		const FString FileName = FString::Printf(TEXT("%s_spot%d_%s.png"), *Label, M_SpotIndex, *Camera.View.Name);
		UKismetRenderingLibrary::ExportRenderTarget(World, Camera.RenderTarget.Get(), M_OutputDirectory, FileName);
	}
	UE_LOG(LogRTSAimCrosshairCapture, Display, TEXT("RTS_AIM_CROSSHAIR captured %s at spot %d"), *Label, M_SpotIndex);
}

bool UAimCrosshairCaptureSubsystem::ShowFirstShowableCase()
{
	while (not ShowCurrentCase())
	{
		if (not AdvanceCase())
		{
			return false;
		}
	}
	return true;
}

bool UAimCrosshairCaptureSubsystem::AdvanceCase()
{
	++M_CaseIndex;
	if (M_Cases.IsValidIndex(M_CaseIndex))
	{
		return true;
	}
	M_CaseIndex = 0;
	++M_SpotIndex;
	return M_Spots.IsValidIndex(M_SpotIndex);
}

void UAimCrosshairCaptureSubsystem::Finish()
{
	if (IsValid(M_AimActor))
	{
		M_AimActor->HideRadius();
	}
	M_Phase = EAimCrosshairCapturePhase::Done;
	UE_LOG(LogRTSAimCrosshairCapture, Display, TEXT("RTS_AIM_CROSSHAIR done"));
	FPlatformMisc::RequestExit(false, TEXT("AimCrosshairCapture"));
}

#else

bool UAimCrosshairCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UAimCrosshairCaptureSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UAimCrosshairCaptureSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UAimCrosshairCaptureSubsystem::Tick(float DeltaTime)
{
}

TStatId UAimCrosshairCaptureSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAimCrosshairCaptureSubsystem, STATGROUP_Tickables);
}

#endif
