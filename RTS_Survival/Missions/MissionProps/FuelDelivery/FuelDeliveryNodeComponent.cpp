// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FuelDeliveryNodeComponent.h"

#include "Components/ArrowComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace FuelDeliveryNodePrivate
{
	// The label lies flat (text facing up) so it reads from the RTS camera.
	const FRotator LabelRotation(90.f, 0.f, 0.f);
	constexpr float LabelWorldSize = 90.f;
}

UFuelDeliveryNodeComponent::UFuelDeliveryNodeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetCanEverAffectNavigation(false);
}

void UFuelDeliveryNodeComponent::OnRegister()
{
	Super::OnRegister();
	CreateEditorVisuals();
}

void UFuelDeliveryNodeComponent::OnComponentDestroyed(const bool bDestroyingHierarchy)
{
#if WITH_EDITORONLY_DATA
	if (M_EditorArrow)
	{
		M_EditorArrow->DestroyComponent();
		M_EditorArrow = nullptr;
	}
	if (M_EditorLabel)
	{
		M_EditorLabel->DestroyComponent();
		M_EditorLabel = nullptr;
	}
#endif
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

#if WITH_EDITOR
void UFuelDeliveryNodeComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyEditorVisuals();
}
#endif

void UFuelDeliveryNodeComponent::CreateEditorVisuals()
{
#if WITH_EDITORONLY_DATA
	AActor* const Owner = GetOwner();
	const UWorld* const World = GetWorld();
	// Same rule as USceneComponent's editor sprite: editor and Blueprint preview worlds only.
	if (Owner == nullptr || World == nullptr || World->IsGameWorld())
	{
		return;
	}
	constexpr EObjectFlags VisualFlags = RF_Transactional | RF_Transient | RF_TextExportTransient;
	if (M_EditorArrow == nullptr)
	{
		M_EditorArrow = NewObject<UArrowComponent>(Owner, NAME_None, VisualFlags);
		M_EditorArrow->SetIsVisualizationComponent(true);
		M_EditorArrow->bIsEditorOnly = true;
		M_EditorArrow->SetHiddenInGame(true);
		M_EditorArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M_EditorArrow->SetCanEverAffectNavigation(false);
		M_EditorArrow->bIsScreenSizeScaled = false;
		M_EditorArrow->CreationMethod = CreationMethod;
		M_EditorArrow->SetupAttachment(this);
		M_EditorArrow->RegisterComponent();
	}
	if (M_EditorLabel == nullptr)
	{
		M_EditorLabel = NewObject<UTextRenderComponent>(Owner, NAME_None, VisualFlags);
		M_EditorLabel->SetIsVisualizationComponent(true);
		M_EditorLabel->bIsEditorOnly = true;
		M_EditorLabel->SetHiddenInGame(true);
		M_EditorLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M_EditorLabel->SetCanEverAffectNavigation(false);
		M_EditorLabel->SetHorizontalAlignment(EHTA_Center);
		M_EditorLabel->SetVerticalAlignment(EVRTA_TextCenter);
		M_EditorLabel->SetWorldSize(FuelDeliveryNodePrivate::LabelWorldSize);
		M_EditorLabel->CreationMethod = CreationMethod;
		M_EditorLabel->SetupAttachment(this);
		M_EditorLabel->RegisterComponent();
	}
	ApplyEditorVisuals();
#endif
}

void UFuelDeliveryNodeComponent::ApplyEditorVisuals() const
{
#if WITH_EDITORONLY_DATA
	if (M_EditorArrow)
	{
		M_EditorArrow->SetArrowColor(NodeColor);
		M_EditorArrow->ArrowSize = ArrowSize;
		M_EditorArrow->ArrowLength = ArrowLength;
		M_EditorArrow->MarkRenderStateDirty();
	}
	if (M_EditorLabel)
	{
		M_EditorLabel->SetText(GetDisplayLabel());
		M_EditorLabel->SetTextRenderColor(NodeColor.ToFColor(true));
		M_EditorLabel->SetRelativeLocationAndRotation(FVector(0.f, 0.f, LabelHeight),
		                                              FuelDeliveryNodePrivate::LabelRotation);
	}
#endif
}

FText UFuelDeliveryNodeComponent::GetDisplayLabel() const
{
	if (not Label.IsEmpty())
	{
		return Label;
	}
	switch (NodeRole)
	{
	case EFuelDeliveryNodeRole::TruckPark:
		return NSLOCTEXT("FuelDelivery", "ParkNode", "Fuel truck park");
	case EFuelDeliveryNodeRole::Crane:
		return NSLOCTEXT("FuelDelivery", "CraneNode", "Fuel crane");
	case EFuelDeliveryNodeRole::TruckSpawn:
	default:
		return NSLOCTEXT("FuelDelivery", "SpawnNode", "Fuel truck spawn");
	}
}
