// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

#include "FuelDeliveryNodeComponent.generated.h"

class UArrowComponent;
class UTextRenderComponent;

/** What a fuel delivery node marks for its UFuelDeliveryDepotComponent. */
UENUM(BlueprintType)
enum class EFuelDeliveryNodeRole : uint8
{
	// Where the truck appears (full) and where it drives back to and disappears. Arrow = spawn heading.
	TruckSpawn,
	// Where the truck stands while the crane unloads it. Arrow = the truck's heading when parked.
	TruckPark,
	// Where the crane's mast stands. Arrow = the crane's forward axis.
	Crane
};

/**
 * @brief A point of a fuel delivery depot. Plain scene component at runtime (no collision, no bounds); in editor worlds
 * only it draws a coloured arrow and a label, so it can be found and dragged on the map (select the depot, then the
 * node, and move it; each placed depot keeps its own node positions).
 */
UCLASS(ClassGroup=(RTS), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UFuelDeliveryNodeComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UFuelDeliveryNodeComponent();

	UFUNCTION(BlueprintPure, Category="FuelDelivery")
	EFuelDeliveryNodeRole GetNodeRole() const { return NodeRole; }

protected:
	virtual void OnRegister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="FuelDelivery")
	EFuelDeliveryNodeRole NodeRole = EFuelDeliveryNodeRole::TruckSpawn;

	// Colour of the editor-only arrow and label.
	UPROPERTY(EditAnywhere, Category="FuelDelivery|Editor")
	FLinearColor NodeColor = FLinearColor(1.f, 0.35f, 0.f);

	UPROPERTY(EditAnywhere, Category="FuelDelivery|Editor", meta=(ClampMin="0.1"))
	float ArrowSize = 4.f;

	UPROPERTY(EditAnywhere, Category="FuelDelivery|Editor", meta=(ClampMin="10"))
	float ArrowLength = 150.f;

	// Text over the node in the editor; empty uses the role name.
	UPROPERTY(EditAnywhere, Category="FuelDelivery|Editor")
	FText Label;

	UPROPERTY(EditAnywhere, Category="FuelDelivery|Editor", meta=(ClampMin="10"))
	float LabelHeight = 260.f;

private:
#if WITH_EDITORONLY_DATA
	// Created in editor worlds only (never in PIE or the game), so the node adds nothing to the actor at runtime.
	UPROPERTY(Transient)
	TObjectPtr<UArrowComponent> M_EditorArrow = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> M_EditorLabel = nullptr;
#endif

	void CreateEditorVisuals();
	void ApplyEditorVisuals() const;
	FText GetDisplayLabel() const;
};
