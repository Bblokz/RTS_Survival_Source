// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PCGMarkVolumesInLandscape.h"

#include "PCGLandscapeDataManagedResource.h"
#include "PCGMarkLandscapeDataHelpers.h"
#include "RTS_Survival/LandscapeDataSystem/LandscapeDataManager/LandscapeDataManager.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "Data/PCGSpatialData.h"
#include "Utils/PCGLogErrors.h"

#define LOCTEXT_NAMESPACE "PCGMarkVolumesInLandscape"

namespace PCGMarkVolumesInLandscape::Private
{
	TArray<const UPCGSpatialData*> BorrowVolumeData(const FPCGContext& Context)
	{
		TArray<const UPCGSpatialData*> SpatialData;
		const TArray<FPCGTaggedData>& Inputs =
			Context.InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
		SpatialData.Reserve(Inputs.Num());
		for (const FPCGTaggedData& TaggedData : Inputs)
		{
			const UPCGSpatialData* VolumeData = Cast<UPCGSpatialData>(TaggedData.Data.Get());
			if (VolumeData != nullptr
				&& EnumHasAnyFlags(VolumeData->GetDataType(), EPCGDataType::Volume))
			{
				SpatialData.Add(VolumeData);
			}
		}

		return SpatialData;
	}

}

bool UPCGMarkVolumesInLandscapeSettings::UseSeed() const
{
	return true;
}

#if WITH_EDITOR
FName UPCGMarkVolumesInLandscapeSettings::GetDefaultNodeName() const
{
	return TEXT("MarkVolumesInLandscape");
}

FText UPCGMarkVolumesInLandscapeSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Mark Volumes In Landscape");
}

FText UPCGMarkVolumesInLandscapeSettings::GetNodeTooltipText() const
{
	return LOCTEXT(
		"NodeTooltip",
		"Paints each volume's XY projection using its sampled density and selected solid, radial, Perlin, or noisy-radial shape, then passes the volumes through unchanged.");
}

EPCGSettingsType UPCGMarkVolumesInLandscapeSettings::GetType() const
{
	return EPCGSettingsType::InputOutput;
}

FString UPCGMarkVolumesInLandscapeSettings::GetAdditionalTitleInformation() const
{
	return FString::Printf(
		TEXT("%s | %s"),
		*PCGMarkLandscapeData::GetChannelTitle(Channel),
		*PCGMarkLandscapeData::GetPaintModeTitle(PaintMode));
}
#endif

TArray<FPCGPinProperties> UPCGMarkVolumesInLandscapeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace_GetRef(
		PCGPinConstants::DefaultInputLabel,
		EPCGDataType::Volume).SetRequiredPin();
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGMarkVolumesInLandscapeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Volume);
	return PinProperties;
}

FPCGElementPtr UPCGMarkVolumesInLandscapeSettings::CreateElement() const
{
	return MakeShared<FPCGMarkVolumesInLandscapeElement>();
}

bool FPCGMarkVolumesInLandscapeElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	PCGMarkLandscapeData::PassThroughInput(*Context);

	const UPCGMarkVolumesInLandscapeSettings* Settings =
		Context->GetInputSettings<UPCGMarkVolumesInLandscapeSettings>();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (Settings == nullptr
		|| not IsValid(SourceComponent)
		|| Context->Stack == nullptr
		|| not IsValid(Context->Node))
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT(
			"InvalidExecutionContext",
			"Mark Volumes In Landscape requires valid settings, a source PCG component, and a graph stack."));
		return true;
	}

	ALandscapeDataManager* LandscapeDataManager =
		PCGMarkLandscapeData::FindLandscapeDataManager(
			Context,
			*SourceComponent,
			LOCTEXT("NodeTitle", "Mark Volumes In Landscape"));
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

	const TArray<const UPCGSpatialData*> SpatialData =
		PCGMarkVolumesInLandscape::Private::BorrowVolumeData(*Context);
	const FRTSLandscapeDataPaintConfiguration PaintConfiguration =
		PCGMarkLandscapeData::CreatePaintConfiguration(
			Settings->PaintMode,
			Settings->RadialSettings,
			Settings->PerlinSettings,
			Settings->NoisyRadialSettings,
			Context->GetSeed());
	if (not ManagedResource->ReplaceVolumeContribution(
		*LandscapeDataManager,
		Settings->Channel,
		SpatialData,
		PaintConfiguration,
		SourceComponent))
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT(
			"ContributionFailed",
			"Mark Volumes In Landscape could not replace its Landscape data contribution."));
		return true;
	}

	if (bShouldRegisterResource)
	{
		SourceComponent->AddToManagedResources(ManagedResource);
	}

	return true;
}

bool FPCGMarkVolumesInLandscapeElement::CanExecuteOnlyOnMainThread(FPCGContext*) const
{
	return true;
}

bool FPCGMarkVolumesInLandscapeElement::IsCacheable(const UPCGSettings*) const
{
	return false;
}

#undef LOCTEXT_NAMESPACE
