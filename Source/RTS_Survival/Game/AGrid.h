// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AGrid.generated.h"

UCLASS()
class RTS_SURVIVAL_API AAGrid : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AAGrid();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int NumRows;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int NumColumns;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float TileSize;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float LineThickness;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor LineColor;

	// color of tile when you select it
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor SelectionColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float LineOpacity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float SelectionOpacity;
	

};
