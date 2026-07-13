// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PCGMarkPointsInLandscape.h"

#include "PCGLandscapeDataManagedResource.h"
#include "RTS_Survival/LandscapeDataSystem/LandscapeDataManager/LandscapeDataManager.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Graph/PCGStackContext.h"
#include "Helpers/PCGHelpers.h"
#include "Utils/PCGLogErrors.h"

#define LOCTEXT_NAMESPACE "PCGMarkPointsInLandscape"

namespace PCGMarkPointsInLandscape::Private
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

	FPCGCrc GetNodeInvocationCRC(const FPCGContext& Context)
	{
		FPCGStack InvocationStack(*Context.Stack);
		InvocationStack.PushFrame(Context.Node);
		return InvocationStack.GetCrc();
	}

	bool HasUsableBounds(const FPCGPoint& Point)
	{
		if (Point.Transform.ContainsNaN()
			|| Point.BoundsMin.ContainsNaN()
			|| Point.BoundsMax.ContainsNaN()
			|| not FMath::IsFinite(Point.Density))
		{
			return false;
		}

		return Point.BoundsMin.X <= Point.BoundsMax.X
			&& Point.BoundsMin.Y <= Point.BoundsMax.Y
			&& Point.BoundsMin.Z <= Point.BoundsMax.Z;
	}

	void ConfigureNoisyBounds(
		const UPCGMarkPointsInLandscapeSettings& Settings,
		FRandomStream& RandomStream,
		FRTSLandscapeDataPointStamp& OutPointStamp)
	{
		if (Settings.PaintMode != ERTSLandscapePointPaintMode::RadialWithNoisyBounds)
		{
			return;
		}

		const float RequestedMinimum = FMath::IsFinite(Settings.NoisyRadialSettings.BoundsScaleMinimum)
			? Settings.NoisyRadialSettings.BoundsScaleMinimum
			: MinimumBoundsScale;
		const float RequestedMaximum = FMath::IsFinite(Settings.NoisyRadialSettings.BoundsScaleMaximum)
			? Settings.NoisyRadialSettings.BoundsScaleMaximum
			: MinimumBoundsScale;
		const float BoundsScaleMinimum = FMath::Max(
			MinimumBoundsScale,
			FMath::Min(RequestedMinimum, RequestedMaximum));
		const float BoundsScaleMaximum = FMath::Max(
			BoundsScaleMinimum,
			FMath::Max(RequestedMinimum, RequestedMaximum));
		const float BoundsScale = RandomStream.FRandRange(BoundsScaleMinimum, BoundsScaleMaximum);
		const FVector BoundsCenter = (OutPointStamp.BoundsMin + OutPointStamp.BoundsMax) * 0.5f;
		const FVector BoundsExtent = (OutPointStamp.BoundsMax - OutPointStamp.BoundsMin) * 0.5f;
		OutPointStamp.BoundsMin = BoundsCenter - BoundsExtent * BoundsScale;
		OutPointStamp.BoundsMax = BoundsCenter + BoundsExtent * BoundsScale;
	}

	void ConfigurePointPaint(
		const UPCGMarkPointsInLandscapeSettings& Settings,
		const int32 PaintSeed,
		FRTSLandscapeDataPointStamp& OutPointStamp)
	{
		OutPointStamp.PaintConfiguration.PaintMode = Settings.PaintMode;
		OutPointStamp.PaintConfiguration.RadialSettings = Settings.RadialSettings;
		OutPointStamp.PaintConfiguration.PerlinSettings = Settings.PerlinSettings;
		OutPointStamp.PaintConfiguration.NoisyRadialSettings = Settings.NoisyRadialSettings;

		FRandomStream RandomStream(PaintSeed);
		OutPointStamp.PaintConfiguration.NoiseOffset = FVector2D(
			RandomStream.FRandRange(-NoiseOffsetRange, NoiseOffsetRange),
			RandomStream.FRandRange(-NoiseOffsetRange, NoiseOffsetRange));
		ConfigureNoisyBounds(Settings, RandomStream, OutPointStamp);
	}

	bool TryCopyPointStamp(
		const FPCGPoint& Point,
		const UPCGMarkPointsInLandscapeSettings& Settings,
		const int32 PaintSeed,
		FRTSLandscapeDataPointStamp& OutPointStamp)
	{
		if (not HasUsableBounds(Point))
		{
			return false;
		}

		constexpr float MinimumStrength = 0.0f;
		constexpr float MaximumStrength = 1.0f;
		const float Strength = FMath::Clamp(Point.Density, MinimumStrength, MaximumStrength);
		if (Strength <= MinimumStrength)
		{
			return false;
		}

		OutPointStamp.Transform = Point.Transform;
		OutPointStamp.BoundsMin = Point.BoundsMin;
		OutPointStamp.BoundsMax = Point.BoundsMax;
		OutPointStamp.Strength = Strength;
		ConfigurePointPaint(Settings, PaintSeed, OutPointStamp);
		return true;
	}

	void AppendPointStamps(
		const UPCGPointData& PointData,
		const UPCGMarkPointsInLandscapeSettings& Settings,
		const int32 ContextSeed,
		int32& InOutPointOrdinal,
		TArray<FRTSLandscapeDataPointStamp>& OutPointStamps)
	{
		const TArray<FPCGPoint>& Points = PointData.GetPoints();
		OutPointStamps.Reserve(OutPointStamps.Num() + Points.Num());
		for (const FPCGPoint& Point : Points)
		{
			const int32 PaintSeed = PCGHelpers::ComputeSeed(
				ContextSeed,
				Point.Seed,
				InOutPointOrdinal);
			++InOutPointOrdinal;
			FRTSLandscapeDataPointStamp PointStamp;
			if (TryCopyPointStamp(Point, Settings, PaintSeed, PointStamp))
			{
				OutPointStamps.Add(MoveTemp(PointStamp));
			}
		}
	}

	TArray<FRTSLandscapeDataPointStamp> CopyPointStamps(
		const FPCGContext& Context,
		const UPCGMarkPointsInLandscapeSettings& Settings)
	{
		TArray<FRTSLandscapeDataPointStamp> PointStamps;
		int32 PointOrdinal = 0;
		const TArray<FPCGTaggedData>& Inputs =
			Context.InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
		for (const FPCGTaggedData& TaggedData : Inputs)
		{
			const UPCGPointData* PointData = Cast<UPCGPointData>(TaggedData.Data.Get());
			if (PointData != nullptr)
			{
				AppendPointStamps(
					*PointData,
					Settings,
					Context.GetSeed(),
					PointOrdinal,
					PointStamps);
			}
		}

		return PointStamps;
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
		UPCGComponent& SourceComponent)
	{
		FString FailureReason;
		ALandscapeDataManager* LandscapeDataManager =
			ALandscapeDataManager::FindUniqueManager(&SourceComponent, FailureReason);
		if (not IsValid(LandscapeDataManager))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(
				LOCTEXT("ManagerLookupFailed", "Mark Points In Landscape could not find its manager: {0}"),
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

bool UPCGMarkPointsInLandscapeSettings::UseSeed() const
{
	return true;
}

#if WITH_EDITOR
FName UPCGMarkPointsInLandscapeSettings::GetDefaultNodeName() const
{
	return TEXT("MarkPointsInLandscape");
}

FText UPCGMarkPointsInLandscapeSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Mark Points In Landscape");
}

FText UPCGMarkPointsInLandscapeSettings::GetNodeTooltipText() const
{
	return LOCTEXT(
		"NodeTooltip",
		"Paints each point's transformed bounds and density with a solid, radial, Perlin, or noisy-radial shape, then passes the points through unchanged.");
}

EPCGSettingsType UPCGMarkPointsInLandscapeSettings::GetType() const
{
	return EPCGSettingsType::InputOutput;
}

FString UPCGMarkPointsInLandscapeSettings::GetAdditionalTitleInformation() const
{
	return FString::Printf(
		TEXT("%s | %s"),
		*PCGMarkPointsInLandscape::Private::GetChannelTitle(Channel),
		*PCGMarkPointsInLandscape::Private::GetPaintModeTitle(PaintMode));
}
#endif

TArray<FPCGPinProperties> UPCGMarkPointsInLandscapeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace_GetRef(
		PCGPinConstants::DefaultInputLabel,
		EPCGDataType::Point).SetRequiredPin();
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGMarkPointsInLandscapeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return PinProperties;
}

FPCGElementPtr UPCGMarkPointsInLandscapeSettings::CreateElement() const
{
	return MakeShared<FPCGMarkPointsInLandscapeElement>();
}

bool FPCGMarkPointsInLandscapeElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	PCGMarkPointsInLandscape::Private::PassThroughInput(*Context);

	const UPCGMarkPointsInLandscapeSettings* Settings =
		Context->GetInputSettings<UPCGMarkPointsInLandscapeSettings>();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (Settings == nullptr
		|| not IsValid(SourceComponent)
		|| Context->Stack == nullptr
		|| not IsValid(Context->Node))
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT(
			"InvalidExecutionContext",
			"Mark Points In Landscape requires valid settings, a source PCG component, and a graph stack."));
		return true;
	}

	ALandscapeDataManager* LandscapeDataManager =
		PCGMarkPointsInLandscape::Private::FindLandscapeDataManager(Context, *SourceComponent);
	if (not IsValid(LandscapeDataManager))
	{
		return true;
	}

	const FPCGCrc StackCRC = PCGMarkPointsInLandscape::Private::GetNodeInvocationCRC(*Context);
	bool bShouldRegisterResource = false;
	UPCGLandscapeDataManagedResource* ManagedResource =
		PCGMarkPointsInLandscape::Private::GetOrCreateManagedResource(
		*SourceComponent,
		Settings->GetStableUID(),
		StackCRC,
		bShouldRegisterResource);

	const TArray<FRTSLandscapeDataPointStamp> PointStamps =
		PCGMarkPointsInLandscape::Private::CopyPointStamps(*Context, *Settings);
	if (not ManagedResource->ReplacePointContribution(
		*LandscapeDataManager,
		Settings->Channel,
		PointStamps,
		SourceComponent))
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT(
			"ContributionFailed",
			"Mark Points In Landscape could not replace its Landscape data contribution."));
		return true;
	}

	if (bShouldRegisterResource)
	{
		SourceComponent->AddToManagedResources(ManagedResource);
	}

	return true;
}

bool FPCGMarkPointsInLandscapeElement::CanExecuteOnlyOnMainThread(FPCGContext*) const
{
	return true;
}

bool FPCGMarkPointsInLandscapeElement::IsCacheable(const UPCGSettings*) const
{
	return false;
}

#undef LOCTEXT_NAMESPACE
