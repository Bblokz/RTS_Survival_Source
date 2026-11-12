// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MinimapTerrainExportSubsystem.h"

#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/GameUI/MiniMap/MinimapTerrainCaptureComponent.h"

namespace MinimapTerrainExportConstants
{
	const TCHAR* const CommandLineSwitch = TEXT("MinimapTerrainExport");
	// The capture repeats 1, 4 and 10 s after begin play (shaders and streaming); export after the last one.
	constexpr double ExportAfterSeconds = 15.0;
}

bool UMinimapTerrainExportSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), MinimapTerrainExportConstants::CommandLineSwitch);
#endif
}

void UMinimapTerrainExportSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	M_BeginPlaySeconds = FPlatformTime::Seconds();
	bM_HasStarted = true;
	UE_LOG(LogTemp, Display, TEXT("MINIMAP_EXPORT started on %s"), *InWorld.GetMapName());
}

void UMinimapTerrainExportSubsystem::Tick(float DeltaTime)
{
	if (not bM_HasStarted || bM_IsDone)
	{
		return;
	}
	if (FPlatformTime::Seconds() - M_BeginPlaySeconds < MinimapTerrainExportConstants::ExportAfterSeconds)
	{
		return;
	}
	bM_IsDone = true;
	ExportAndQuit();
}

TStatId UMinimapTerrainExportSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMinimapTerrainExportSubsystem, STATGROUP_Tickables);
}

void UMinimapTerrainExportSubsystem::ExportAndQuit()
{
	const ACPPGameState* const GameState = GetWorld()->GetGameState<ACPPGameState>();
	const UMinimapTerrainCaptureComponent* const CaptureComponent = IsValid(GameState)
		                                                                ? GameState->GetMinimapTerrainCaptureComponent()
		                                                                : nullptr;
	UTextureRenderTarget2D* const RenderTarget = IsValid(CaptureComponent)
		                                             ? CaptureComponent->GetMinimapTerrainRenderTarget()
		                                             : nullptr;
	if (not IsValid(RenderTarget))
	{
		UE_LOG(LogTemp, Error, TEXT("MINIMAP_EXPORT FAIL no minimap terrain render target on this map"));
		FPlatformMisc::RequestExit(false, TEXT("MinimapTerrainExport"));
		return;
	}
	const FString MapName = UGameplayStatics::GetCurrentLevelName(this, true);
	const FString OutputPath = FPaths::ProjectSavedDir() / TEXT("InfoTakeover") / TEXT("Minimap") / (MapName + TEXT(".png"));
	FImage Image;
	const bool bRead = FImageUtils::GetRenderTargetImage(RenderTarget, Image);
	const bool bSaved = bRead && FImageUtils::SaveImageAutoFormat(*OutputPath, Image);
	UE_LOG(LogTemp, Display, TEXT("MINIMAP_EXPORT %s %dx%d -> %s"), bSaved ? TEXT("saved") : TEXT("FAIL could not save"),
	       Image.SizeX, Image.SizeY, *OutputPath);
	FPlatformMisc::RequestExit(false, TEXT("MinimapTerrainExport"));
}
