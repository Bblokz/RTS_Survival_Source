// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GroundSurfaceSubsystem.h"

#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "RTS_Survival/Environment/GroundSurface/GroundSurfaceMap.h"
#include "RTS_Survival/Environment/GroundSurface/RTSGroundSurfaceSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSGroundSurface, Log, All);

bool UGroundSurfaceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	if (not IsValid(World))
	{
		return false;
	}
	return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
}

void UGroundSurfaceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BeginPlay_BuildSmokeLooks();
	BeginPlay_ReadSurfaceMap(InWorld);
}

EGroundSurfaceType UGroundSurfaceSubsystem::GetSurfaceTypeAt(const FVector& WorldLocation) const
{
	if (not bM_HasSurfaceMap)
	{
		return EGroundSurfaceType::None;
	}
	const int32 CellX = FMath::FloorToInt32((WorldLocation.X - M_Origin.X) * M_InverseCellSize);
	const int32 CellY = FMath::FloorToInt32((WorldLocation.Y - M_Origin.Y) * M_InverseCellSize);
	if (CellX < 0 || CellY < 0 || CellX >= M_CellCount.X || CellY >= M_CellCount.Y)
	{
		return EGroundSurfaceType::None;
	}
	const uint8 SurfaceValue = M_SurfaceCells[CellY * M_CellCount.X + CellX];
	return SurfaceValue < static_cast<uint8>(EGroundSurfaceType::Num)
		       ? static_cast<EGroundSurfaceType>(SurfaceValue)
		       : EGroundSurfaceType::None;
}

const FGroundSurfaceSmokeLook& UGroundSurfaceSubsystem::GetSmokeLookAt(const FVector& WorldLocation) const
{
	static const FGroundSurfaceSmokeLook FallbackLook;
	const int32 SurfaceIndex = static_cast<int32>(GetSurfaceTypeAt(WorldLocation));
	return M_SmokeLookPerSurface.IsValidIndex(SurfaceIndex) ? M_SmokeLookPerSurface[SurfaceIndex] : FallbackLook;
}

void UGroundSurfaceSubsystem::BeginPlay_ReadSurfaceMap(const UWorld& InWorld)
{
	bM_HasSurfaceMap = false;
	const URTSGroundSurfaceSettings* Settings = URTSGroundSurfaceSettings::Get();
	if (Settings == nullptr)
	{
		return;
	}
	const FString MapName = UWorld::RemovePIEPrefix(FPackageName::GetShortName(InWorld.GetOutermost()));
	const UGroundSurfaceMap* SurfaceMap = Cast<UGroundSurfaceMap>(Settings->GetSurfaceMapPath(MapName).TryLoad());
	if (not IsValid(SurfaceMap) || not SurfaceMap->GetIsValidMap())
	{
		UE_LOG(LogRTSGroundSurface, Display, TEXT("Ground surface: map %s has no baked surface map; track smoke keeps one look."),
		       *MapName);
		return;
	}
	M_Origin = SurfaceMap->Origin;
	M_CellCount = SurfaceMap->CellCount;
	M_InverseCellSize = 1.f / SurfaceMap->CellSize;
	M_SurfaceCells = SurfaceMap->SurfaceCells;
	bM_HasSurfaceMap = true;
	UE_LOG(LogRTSGroundSurface, Display, TEXT("Ground surface: map %s uses its surface map (%dx%d cells of %.0f cm)."),
	       *MapName, M_CellCount.X, M_CellCount.Y, SurfaceMap->CellSize);
}

void UGroundSurfaceSubsystem::BeginPlay_BuildSmokeLooks()
{
	const URTSGroundSurfaceSettings* Settings = URTSGroundSurfaceSettings::Get();
	const FGroundSurfaceSmokeLook NoSurfaceLook = Settings != nullptr ? Settings->NoSurfaceSmokeLook : FGroundSurfaceSmokeLook();
	M_SmokeLookPerSurface.Init(NoSurfaceLook, static_cast<int32>(EGroundSurfaceType::Num));
	if (Settings == nullptr)
	{
		return;
	}
	for (const FGroundSurfaceDefinition& Definition : Settings->Surfaces)
	{
		const int32 SurfaceIndex = static_cast<int32>(Definition.SurfaceType);
		if (Definition.SurfaceType == EGroundSurfaceType::None || not M_SmokeLookPerSurface.IsValidIndex(SurfaceIndex))
		{
			continue;
		}
		M_SmokeLookPerSurface[SurfaceIndex] = Definition.SmokeLook;
	}
}
