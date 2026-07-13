// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PCGMarkPointsInLandscape.h"

#include "PCGLandscapeDataManagedResource.h"
#include "PCGMarkLandscapeDataHelpers.h"
#include "RTS_Survival/LandscapeDataSystem/LandscapeDataManager/LandscapeDataManager.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGHelpers.h"
#include "Utils/PCGLogErrors.h"

#define LOCTEXT_NAMESPACE "PCGMarkPointsInLandscape"

namespace PCGMarkPointsInLandscape::Private
{
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
		FRTSLandscapeDataPointStamp& OutPointStamp)
	{
		if (Settings.PaintMode != ERTSLandscapePointPaintMode::RadialWithNoisyBounds)
		{
			return;
		}

		const float BoundsScale = OutPointStamp.PaintConfiguration.BoundsScale;
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
		OutPointStamp.PaintConfiguration = PCGMarkLandscapeData::CreatePaintConfiguration(
			Settings.PaintMode,
			Settings.RadialSettings,
			Settings.PerlinSettings,
			Settings.NoisyRadialSettings,
			PaintSeed);
		ConfigureNoisyBounds(Settings, OutPointStamp);
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
		*PCGMarkLandscapeData::GetChannelTitle(Channel),
		*PCGMarkLandscapeData::GetPaintModeTitle(PaintMode));
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
	PCGMarkLandscapeData::PassThroughInput(*Context);

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
		PCGMarkLandscapeData::FindLandscapeDataManager(
			Context,
			*SourceComponent,
			LOCTEXT("NodeTitle", "Mark Points In Landscape"));
	if (not IsValid(LandscapeDataManager))
	{
		return true;
	}

	const FPCGCrc StackCRC = PCGMarkLandscapeData::GetNodeInvocationCRC(*Context);
	bool bShouldRegisterResource = false;
	UPCGLandscapeDataManagedResource* ManagedResource =
		PCGMarkLandscapeData::GetOrCreateManagedResource(
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
