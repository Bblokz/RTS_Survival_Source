// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PCGMarkLandscapeDataHelpers.h"

#include "PCGLandscapeDataManagedResource.h"
#include "RTS_Survival/LandscapeDataSystem/LandscapeDataManager/LandscapeDataManager.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "Graph/PCGStackContext.h"
#include "Utils/PCGLogErrors.h"

namespace PCGMarkLandscapeData
{
	constexpr float MinimumBoundsScale = 0.01f;
	constexpr float NoiseOffsetRange = 1000.0f;

	FString GetChannelTitle(const ERTSLandscapeDataChannel Channel)
	{
		switch (Channel)
		{
		case ERTSLandscapeDataChannel::Scorched:
			return TEXT("Scorched (R)");
		case ERTSLandscapeDataChannel::Gravel:
			return TEXT("Gravel (G)");
		case ERTSLandscapeDataChannel::Concrete:
			return TEXT("Concrete (B)");
		case ERTSLandscapeDataChannel::ExtraSand:
			return TEXT("Extra Sand (A)");
		}
		return FString();
	}

	FString GetPaintModeTitle(const ERTSLandscapePointPaintMode PaintMode)
	{
		switch (PaintMode)
		{
		case ERTSLandscapePointPaintMode::Solid:
			return TEXT("Solid Bounds");
		case ERTSLandscapePointPaintMode::Radial:
			return TEXT("Radial");
		case ERTSLandscapePointPaintMode::PerlinNoise:
			return TEXT("Perlin Noise");
		case ERTSLandscapePointPaintMode::RadialWithNoisyBounds:
			return TEXT("Radial With Noisy Bounds");
		}
		return FString();
	}

	FRTSLandscapeDataPaintConfiguration CreatePaintConfiguration(
		const ERTSLandscapePointPaintMode PaintMode,
		const FRTSLandscapeRadialPointPaintSettings& RadialSettings,
		const FRTSLandscapePerlinPointPaintSettings& PerlinSettings,
		const FRTSLandscapeNoisyRadialPointPaintSettings& NoisyRadialSettings,
		const int32 Seed)
	{
		FRTSLandscapeDataPaintConfiguration PaintConfiguration;
		PaintConfiguration.PaintMode = PaintMode;
		PaintConfiguration.RadialSettings = RadialSettings;
		PaintConfiguration.PerlinSettings = PerlinSettings;
		PaintConfiguration.NoisyRadialSettings = NoisyRadialSettings;

		FRandomStream RandomStream(Seed);
		PaintConfiguration.NoiseOffset = FVector2D(
			RandomStream.FRandRange(-NoiseOffsetRange, NoiseOffsetRange),
			RandomStream.FRandRange(-NoiseOffsetRange, NoiseOffsetRange));
		if (PaintMode != ERTSLandscapePointPaintMode::RadialWithNoisyBounds)
		{
			return PaintConfiguration;
		}

		const float RequestedMinimum = FMath::IsFinite(NoisyRadialSettings.BoundsScaleMinimum)
			? NoisyRadialSettings.BoundsScaleMinimum
			: MinimumBoundsScale;
		const float RequestedMaximum = FMath::IsFinite(NoisyRadialSettings.BoundsScaleMaximum)
			? NoisyRadialSettings.BoundsScaleMaximum
			: MinimumBoundsScale;
		const float BoundsScaleMinimum = FMath::Max(
			MinimumBoundsScale,
			FMath::Min(RequestedMinimum, RequestedMaximum));
		const float BoundsScaleMaximum = FMath::Max(
			BoundsScaleMinimum,
			FMath::Max(RequestedMinimum, RequestedMaximum));
		PaintConfiguration.BoundsScale = RandomStream.FRandRange(BoundsScaleMinimum, BoundsScaleMaximum);
		return PaintConfiguration;
	}

	FPCGCrc GetNodeInvocationCRC(const FPCGContext& Context)
	{
		FPCGStack InvocationStack(*Context.Stack);
		InvocationStack.PushFrame(Context.Node);
		return InvocationStack.GetCrc();
	}

	void PassThroughInput(FPCGContext& Context)
	{
		Context.InputData.GetInputsAndCrcsByPin(
			PCGPinConstants::DefaultInputLabel,
			Context.OutputData.TaggedData,
			Context.OutputData.DataCrcs);
		for (FPCGTaggedData& TaggedData : Context.OutputData.TaggedData)
		{
			TaggedData.Pin = PCGPinConstants::DefaultOutputLabel;
		}
	}

	ALandscapeDataManager* FindLandscapeDataManager(
		FPCGContext* Context,
		UPCGComponent& SourceComponent,
		const FText& NodeTitle)
	{
		FString FailureReason;
		ALandscapeDataManager* LandscapeDataManager =
			ALandscapeDataManager::FindUniqueManager(&SourceComponent, FailureReason);
		if (not IsValid(LandscapeDataManager))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(
				FText::FromString(TEXT("{0} could not find its manager: {1}")),
				NodeTitle,
				FText::FromString(FailureReason)));
			return nullptr;
		}

		return LandscapeDataManager;
	}

	UPCGLandscapeDataManagedResource* GetOrCreateManagedResource(
		UPCGComponent& SourceComponent,
		const uint64 SettingsUID,
		const FPCGCrc& StackCRC,
		bool& bOutShouldRegisterResource)
	{
		UPCGLandscapeDataManagedResource* ManagedResource =
			UPCGLandscapeDataManagedResource::FindReusableResource(
				SourceComponent,
				SettingsUID,
				StackCRC);
		bOutShouldRegisterResource = ManagedResource == nullptr;
		if (not bOutShouldRegisterResource)
		{
			return ManagedResource;
		}

		ManagedResource = NewObject<UPCGLandscapeDataManagedResource>(&SourceComponent);
		ManagedResource->InitializeIdentity(SettingsUID, StackCRC);
		return ManagedResource;
	}
}
