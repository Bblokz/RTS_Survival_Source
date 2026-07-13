// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "RTS_Survival/LandscapeDataSystem/LandscapeDataTypes.h"

class ALandscapeDataManager;
struct FPCGContext;
class UPCGComponent;
class UPCGLandscapeDataManagedResource;

/**
 * @brief Provides the shared execution and resource-lifetime flow for Landscape marking PCG nodes.
 * Keep node-specific input conversion and artistic settings in the individual point or volume node.
 */
namespace PCGMarkLandscapeData
{
	FString GetChannelTitle(ERTSLandscapeDataChannel Channel);
	FString GetPaintModeTitle(ERTSLandscapePointPaintMode PaintMode);
	FRTSLandscapeDataPaintConfiguration CreatePaintConfiguration(
		ERTSLandscapePointPaintMode PaintMode,
		const FRTSLandscapeRadialPointPaintSettings& RadialSettings,
		const FRTSLandscapePerlinPointPaintSettings& PerlinSettings,
		const FRTSLandscapeNoisyRadialPointPaintSettings& NoisyRadialSettings,
		int32 Seed);
	FPCGCrc GetNodeInvocationCRC(const FPCGContext& Context);
	void PassThroughInput(FPCGContext& Context);
	ALandscapeDataManager* FindLandscapeDataManager(
		FPCGContext* Context,
		UPCGComponent& SourceComponent,
		const FText& NodeTitle);
	UPCGLandscapeDataManagedResource* GetOrCreateManagedResource(
		UPCGComponent& SourceComponent,
		uint64 SettingsUID,
		const FPCGCrc& StackCRC,
		bool& bOutShouldRegisterResource);
}
