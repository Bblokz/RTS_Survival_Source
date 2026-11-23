// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GarrisonBarSnapshot.h"

#if RTS_WITH_MAP_TESTS
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/WidgetComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/BufferArchive.h"
#include "Slate/WidgetRenderer.h"
#include "RTS_Survival/GameUI/Healthbar/GarrisonHealthBar/W_GarrisonHealthBar.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "LandscapeComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSGarrisonBarSnapshot, Log, All);

namespace RTSGarrisonBarSnapshotPrivate
{
	// VehicleModuleBalance::DefaultModuleIconWidth / Height: the size the health component gives module icons.
	constexpr float ModuleIconEdge = 225.f;
	constexpr int32 FakeModuleIconCount = 2;
	const FName ModuleBoxName(TEXT("ModuleBox"));
	const FLinearColor FakeIconColor(1.f, 0.25f, 0.1f, 1.f);
	const FLinearColor BackgroundColor(0.12f, 0.12f, 0.12f, 1.f);

	void AddFakeModuleIcons(UUserWidget& HealthBar)
	{
		UHorizontalBox* ModuleBox = Cast<UHorizontalBox>(HealthBar.GetWidgetFromName(ModuleBoxName));
		if (not IsValid(ModuleBox) || not IsValid(HealthBar.WidgetTree))
		{
			return;
		}
		for (int32 IconIndex = 0; IconIndex < FakeModuleIconCount; ++IconIndex)
		{
			UImage* Icon = HealthBar.WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			FSlateBrush Brush;
			Brush.ImageSize = FVector2D(ModuleIconEdge, ModuleIconEdge);
			Brush.TintColor = FSlateColor(FakeIconColor);
			Icon->SetBrush(Brush);
			ModuleBox->AddChildToHorizontalBox(Icon);
		}
		ModuleBox->SetVisibility(ESlateVisibility::Visible);
	}

	// Shows the footprint of occupied garrison seat boxes (they draw nothing while empty).
	void FillGarrisonSeatBoxes(UUserWidget& HealthBar)
	{
		constexpr int32 SeatBoxCount = 3;
		constexpr float SeatImageEdge = 176.f;
		const FLinearColor SeatColor(0.2f, 0.45f, 1.f, 1.f);
		for (int32 SeatIndex = 0; SeatIndex < SeatBoxCount; ++SeatIndex)
		{
			UWidget* SeatBox = HealthBar.GetWidgetFromName(*FString::Printf(TEXT("GarrisonBox%d"), SeatIndex));
			UImage* SeatImage = Cast<UImage>(HealthBar.GetWidgetFromName(*FString::Printf(TEXT("GarrisonImage%d"), SeatIndex)));
			if (not IsValid(SeatBox) || not IsValid(SeatImage))
			{
				continue;
			}
			FSlateBrush Brush;
			Brush.ImageSize = FVector2D(SeatImageEdge, SeatImageEdge);
			Brush.TintColor = FSlateColor(SeatColor);
			SeatImage->SetBrush(Brush);
			SeatBox->SetVisibility(ESlateVisibility::Visible);
		}
	}

	bool WriteWidgetPng(UUserWidget& HealthBar, const FVector2D& DrawSize, const FString& FilePath)
	{
		UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false);
		if (not IsValid(Target))
		{
			return false;
		}
		Target->ClearColor = BackgroundColor;
		FWidgetRenderer Renderer(true, false);
		Renderer.DrawWidget(Target, HealthBar.TakeWidget(), DrawSize, 0.f);
		FlushRenderingCommands();
		FBufferArchive PngBytes;
		if (not FImageUtils::ExportRenderTarget2DAsPNG(Target, PngBytes))
		{
			return false;
		}
		return FFileHelper::SaveArrayToFile(PngBytes, *FilePath);
	}

	bool TryWriteSnapshot(AActor& Unit, UUserWidget& HealthBar, const UWidgetComponent& WidgetComponent,
	                      const FString& OutputDirectory)
	{
		HealthBar.SetVisibility(ESlateVisibility::Visible);
		AddFakeModuleIcons(HealthBar);
		FillGarrisonSeatBoxes(HealthBar);
		const FVector2D DrawSize = WidgetComponent.GetDrawSize();
		const FString FilePath = FPaths::Combine(OutputDirectory, FString::Printf(
			                                         TEXT("%s_%s.png"), *Unit.GetClass()->GetName(),
			                                         *HealthBar.GetClass()->GetName()));
		const bool bWritten = WriteWidgetPng(HealthBar, DrawSize, FilePath);
		UE_LOG(LogRTSGarrisonBarSnapshot, Display,
		       TEXT("RTS_GARRISON_SNAPSHOT unit=%s bar=%s draw_size=%.0fx%.0f module_box=%d written=%d path=%s"),
		       *Unit.GetName(), *HealthBar.GetClass()->GetName(), DrawSize.X, DrawSize.Y,
		       HealthBar.GetWidgetFromName(ModuleBoxName) != nullptr ? 1 : 0, bWritten ? 1 : 0, *FilePath);
		return bWritten;
	}
}

int32 RTSGarrisonBarSnapshot::WriteSnapshots(UWorld* World, const FString& OutputDirectory)
{
	using namespace RTSGarrisonBarSnapshotPrivate;
	if (not IsValid(World))
	{
		return 0;
	}
	bool bHasGarrisonBar = false;
	bool bHasModulesBar = false;
	int32 Written = 0;
	for (TActorIterator<AActor> ActorIterator(World); ActorIterator; ++ActorIterator)
	{
		const UHealthComponent* Health = ActorIterator->FindComponentByClass<UHealthComponent>();
		const UWidgetComponent* WidgetComponent = IsValid(Health) ? Health->GetHealthBarWidgetComp() : nullptr;
		UUserWidget* HealthBar = IsValid(WidgetComponent) ? WidgetComponent->GetUserWidgetObject() : nullptr;
		if (not IsValid(HealthBar))
		{
			continue;
		}
		// Transports only: garrisoned buildings also use the garrison bar but never show module icons.
		const bool bIsGarrisonBar = HealthBar->IsA<UW_GarrisonHealthBar>() && ActorIterator->IsA<ATankMaster>();
		const bool bIsModulesBar = not HealthBar->IsA<UW_GarrisonHealthBar>() &&
			HealthBar->GetWidgetFromName(ModuleBoxName) != nullptr;
		const bool bWanted = (bIsGarrisonBar && not bHasGarrisonBar) || (bIsModulesBar && not bHasModulesBar);
		if (not bWanted)
		{
			continue;
		}
		bHasGarrisonBar |= bIsGarrisonBar;
		bHasModulesBar |= bIsModulesBar;
		Written += TryWriteSnapshot(**ActorIterator, *HealthBar, *WidgetComponent, OutputDirectory) ? 1 : 0;
		if (bHasGarrisonBar && bHasModulesBar)
		{
			break;
		}
	}
	return Written;
}
int32 RTSGarrisonBarSnapshot::LogLandscapeComponents(UWorld* World)
{
	if (not IsValid(World))
	{
		return 0;
	}
	int32 Logged = 0;
	for (TActorIterator<ALandscapeProxy> ProxyIterator(World); ProxyIterator; ++ProxyIterator)
	{
		for (const ULandscapeComponent* Component : ProxyIterator->LandscapeComponents)
		{
			if (not IsValid(Component))
			{
				continue;
			}
			FString Layers;
			for (const FWeightmapLayerAllocationInfo& Allocation : Component->GetWeightmapLayerAllocations())
			{
				Layers += (IsValid(Allocation.LayerInfo) ? Allocation.LayerInfo->LayerName.ToString() : FString(TEXT("None")))
					+ TEXT(",");
			}
			const UMaterialInterface* Material = Component->GetMaterialInstance(0, false);
			UE_LOG(LogRTSGarrisonBarSnapshot, Display,
			       TEXT("RTS_LANDSCAPE_COMPONENT %s mic=%s location=%s weightmaps=%d layers=%d %s"),
			       *Component->GetName(), *GetNameSafe(Material), *Component->GetComponentLocation().ToCompactString(),
			       Component->GetWeightmapTextures().Num(), Component->GetWeightmapLayerAllocations().Num(), *Layers);
			++Logged;
		}
	}
	return Logged;
}
#endif
