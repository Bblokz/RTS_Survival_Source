// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GrenadeTestHelpers.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/CommandLine.h"
#include "NavigationSystem.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

namespace GrenadeTestHelpersPrivate
{
	const TCHAR* const AnchorClassPrefix = TEXT("BP_UnitImage_LandscapeAnchor");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	constexpr float GroundTraceHalfHeight = 10000.f;
	constexpr float SpawnNavExtent = 500.f;
	constexpr float SpawnHeightOffset = 100.f;
	constexpr int32 PictureWidth = 1280;
	constexpr int32 PictureHeight = 720;
	const FRotator GameCameraRotation(-70.f, 0.f, 0.f);
	constexpr float GameCameraDistance = 3000.f;
}

bool FGrenadeTestHelpers::TryStartGame(UWorld& World, TWeakObjectPtr<ACPPController>& OutPlayerController)
{
	OutPlayerController = Cast<ACPPController>(World.GetFirstPlayerController());
	if (not OutPlayerController.IsValid())
	{
		return false;
	}
	UPlayerStartGameControl* StartGameControl = OutPlayerController->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	KeepGameRunning(World, OutPlayerController.Get());
	return true;
}

void FGrenadeTestHelpers::KeepGameRunning(UWorld& World, ACPPController* PlayerController)
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(&World, Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(
			Picker->WidgetTree->FindWidget(GrenadeTestHelpersPrivate::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	if (IsValid(PlayerController) && PlayerController->GetIsGameOnPause())
	{
		PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

bool FGrenadeTestHelpers::FindTestSpot(UWorld& World, const TCHAR* LocationSwitch, FVector& OutLocation)
{
	FString LocationText;
	if (FParse::Value(FCommandLine::Get(), LocationSwitch, LocationText, false))
	{
		TArray<FString> Coordinates;
		LocationText.ParseIntoArray(Coordinates, TEXT(","), true);
		if (Coordinates.Num() >= 2)
		{
			OutLocation = FVector(FCString::Atof(*Coordinates[0]), FCString::Atof(*Coordinates[1]), 0.f);
			DropToGround(World, OutLocation);
			return true;
		}
	}
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		if (It->GetClass()->GetName().StartsWith(GrenadeTestHelpersPrivate::AnchorClassPrefix))
		{
			OutLocation = It->GetActorLocation();
			DropToGround(World, OutLocation);
			return true;
		}
	}
	for (TActorIterator<APlayerStart> It(&World); It; ++It)
	{
		OutLocation = It->GetActorLocation();
		DropToGround(World, OutLocation);
		return true;
	}
	return false;
}

bool FGrenadeTestHelpers::DropToGround(UWorld& World, FVector& InOutLocation)
{
	const FVector TraceOffset(0.f, 0.f, GrenadeTestHelpersPrivate::GroundTraceHalfHeight);
	FHitResult GroundHit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GrenadeTestGround), false);
	if (not World.LineTraceSingleByChannel(GroundHit, InOutLocation + TraceOffset, InOutLocation - TraceOffset,
	                                       ECC_Visibility, QueryParams))
	{
		return false;
	}
	InOutLocation = GroundHit.ImpactPoint;
	return true;
}

bool FGrenadeTestHelpers::ProjectOntoNavMesh(UWorld& World, FVector& InOutLocation, const FVector& SearchExtent)
{
	UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(&World);
	FNavLocation NavLocation;
	if (not IsValid(NavigationSystem) ||
		not NavigationSystem->ProjectPointToNavigation(InOutLocation, NavLocation, SearchExtent))
	{
		return false;
	}
	InOutLocation = NavLocation.Location;
	return true;
}

AActor* FGrenadeTestHelpers::SpawnUnitForPlayer(UWorld& World, const TCHAR* ClassPath, const FTransform& Transform,
                                                const uint8 OwningPlayer)
{
	using namespace GrenadeTestHelpersPrivate;
	UClass* UnitClass = LoadClass<AActor>(nullptr, ClassPath);
	if (not IsValid(UnitClass))
	{
		return nullptr;
	}
	FVector SpawnLocation = Transform.GetLocation();
	DropToGround(World, SpawnLocation);
	ProjectOntoNavMesh(World, SpawnLocation, FVector(SpawnNavExtent));
	SpawnLocation.Z += SpawnHeightOffset;
	const FTransform SpawnTransform(Transform.GetRotation(), SpawnLocation);
	return FRTSUnitOwnerSpawning::SpawnUnitForPlayer(World, UnitClass, SpawnTransform, OwningPlayer);
}

bool FGrenadeTestHelpers::GetIsLivingSoldier(ASquadUnit* Soldier)
{
	// IsUnitAlive is not const on AActor.
	return IsValid(Soldier) && Soldier->IsUnitAlive();
}

int32 FGrenadeTestHelpers::GetLivingSoldierCount(ASquadController& Squad)
{
	int32 LivingSoldiers = 0;
	for (ASquadUnit* Soldier : Squad.GetSquadUnitsChecked())
	{
		LivingSoldiers += GetIsLivingSoldier(Soldier) ? 1 : 0;
	}
	return LivingSoldiers;
}

bool FGrenadeTestHelpers::CreateCaptureCamera(UWorld& World, const float FieldOfView, ASceneCapture2D*& OutCapture,
                                              UTextureRenderTarget2D*& OutRenderTarget)
{
	using namespace GrenadeTestHelpersPrivate;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	OutCapture = World.SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform::Identity, SpawnParameters);
	OutRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(&World, PictureWidth, PictureHeight, RTF_RGBA8_SRGB);
	if (not IsValid(OutCapture) || not IsValid(OutRenderTarget))
	{
		return false;
	}
	USceneCaptureComponent2D* CaptureComponent = OutCapture->GetCaptureComponent2D();
	CaptureComponent->TextureTarget = OutRenderTarget;
	CaptureComponent->CaptureSource = SCS_FinalColorLDR;
	CaptureComponent->FOVAngle = FieldOfView;
	// Rendering every frame with kept state lets auto exposure and temporal AA settle like the game view.
	CaptureComponent->bCaptureEveryFrame = true;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	// The fog of war is a post-process material; the pictures show the effect, not what the player may see.
	CaptureComponent->ShowFlags.SetPostProcessMaterial(false);
	return true;
}

void FGrenadeTestHelpers::AimLikeGameCamera(ASceneCapture2D& Capture, const FVector& LookAt)
{
	using namespace GrenadeTestHelpersPrivate;
	Capture.SetActorLocationAndRotation(LookAt - GameCameraRotation.Vector() * GameCameraDistance, GameCameraRotation);
}

void FGrenadeTestHelpers::SaveCapture(UWorld& World, UTextureRenderTarget2D& RenderTarget, const FString& Directory,
                                      const FString& FileName)
{
	UKismetRenderingLibrary::ExportRenderTarget(&World, &RenderTarget, Directory, FileName);
}

#endif
