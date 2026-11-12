// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "MinimapTerrainExportSubsystem.generated.h"

/**
 * @brief Headless tool, started with -MinimapTerrainExport (with -game -RenderOffscreen on the map to export): waits
 * until the minimap's terrain capture has run its recaptures, writes the render target the minimap shows to
 * Saved/InfoTakeover/Minimap/<MapName>.png and quits. The camera controls takeover uses that picture, so its
 * illustrations show the real minimap of the map instead of drawn art. Grep MINIMAP_EXPORT. Not created in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UMinimapTerrainExportSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	double M_BeginPlaySeconds = 0.0;
	bool bM_HasStarted = false;
	bool bM_IsDone = false;

	void ExportAndQuit();
};
