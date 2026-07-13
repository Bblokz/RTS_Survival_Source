// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PCGMarkVolumesInLandscape.h"

#include "PCGLandscapeDataManagedResource.h"
#include "RTS_Survival/LandscapeDataSystem/LandscapeDataManager/LandscapeDataManager.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "Data/PCGSpatialData.h"
#include "Graph/PCGStackContext.h"
#include "Utils/PCGLogErrors.h"

#define LOCTEXT_NAMESPACE "PCGMarkVolumesInLandscape"

namespace PCGMarkVolumesInLandscape::Private
{
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

	FPCGCrc GetNodeInvocationCRC(const FPCGContext& Context)
	{
		FPCGStack InvocationStack(*Context.Stack);
		InvocationStack.PushFrame(Context.Node);
		return InvocationStack.GetCrc();
	}

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
				LOCTEXT("ManagerLookupFailed", "Mark Volumes In Landscape could not find its manager: {0}"),
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

bool UPCGMarkVolumesInLandscapeSettings::UseSeed() const
{
	return false;
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
		"Paints each volume's XY projection into the selected Landscape data channel using its sampled volume density, then passes the volumes through unchanged.");
}

EPCGSettingsType UPCGMarkVolumesInLandscapeSettings::GetType() const
{
	return EPCGSettingsType::InputOutput;
}

FString UPCGMarkVolumesInLandscapeSettings::GetAdditionalTitleInformation() const
{
	return PCGMarkVolumesInLandscape::Private::GetChannelTitle(Channel);
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
	PCGMarkVolumesInLandscape::Private::PassThroughInput(*Context);

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
		PCGMarkVolumesInLandscape::Private::FindLandscapeDataManager(Context, *SourceComponent);
	if (not IsValid(LandscapeDataManager))
	{
		return true;
	}

	const FPCGCrc StackCRC = PCGMarkVolumesInLandscape::Private::GetNodeInvocationCRC(*Context);
	bool bShouldRegisterResource = false;
	UPCGLandscapeDataManagedResource* ManagedResource =
		PCGMarkVolumesInLandscape::Private::GetOrCreateManagedResource(
		*SourceComponent,
		Settings->GetStableUID(),
		StackCRC,
		bShouldRegisterResource);

	const TArray<const UPCGSpatialData*> SpatialData =
		PCGMarkVolumesInLandscape::Private::BorrowVolumeData(*Context);
	if (not ManagedResource->ReplaceVolumeContribution(
		*LandscapeDataManager,
		Settings->Channel,
		SpatialData,
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
