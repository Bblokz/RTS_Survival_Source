// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "MissionMarker.generated.h"

UCLASS()
class RTS_SURVIVAL_API AMissionMarker : public AActor
{
	GENERATED_BODY()

public:
	AMissionMarker();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	// Used to spawn the on-screen marker widget
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ScreenMarker")
	TSubclassOf<class UW_MissionMarker> MarkerWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<UW_MissionMarker> M_MarkerWidget = nullptr;
	bool GetIsValidMarkerWidget()const;

	FVector2D M_ViewportSize;

	void BeginPlay_SetViewPortSize();
	void BeginPlay_CreateMarkerWidget();

	bool EnsureMarkerWidgetClassIsValid() const;

	void UpdateScreenMarker() const;

};
