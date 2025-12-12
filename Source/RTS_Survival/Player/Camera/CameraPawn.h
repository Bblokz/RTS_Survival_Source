// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "CameraPawn.generated.h"

class USpringArmComponent;
class ACPPController;
class UW_MiniMap;
class AFowManager;

USTRUCT()
struct FViewportProjections
{
	GENERATED_BODY()

	FVector2D TopLeft;
	FVector2D TopRight;
	FVector2D BottomLeft;
	FVector2D BottomRight;
};

UCLASS()
class RTS_SURVIVAL_API ACameraPawn : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ACameraPawn(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "CameraLocation")
	FVector GetCameraGroundLocation();

	inline UCameraComponent* GetCameraComponent() const { return M_CameraComponent; }

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/**
     * @brief Utility to obtain the sphere ground position of the camera.
     * @return The position at the landscape.
     */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Camera")
	FVector GetCameraGroundPosition();

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	inline AFowManager* GetFOWManager() const { return M_FowManager; }

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	inline UW_MiniMap* GetMiniMap() const { return M_MiniMap; }

	/** @brief Receives UV click from the mini‐map [0,1] and moves camera there. */
	UFUNCTION()
	void HandleMiniMapClick(FVector2D LocalClickUV);

protected:
	virtual void PostInitializeComponents() override;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "ReferenceCasts")
	void InitCameraPawn(UCameraComponent* NewCameraComponent);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnDrawCameraToMiniMap();

private:
	UPROPERTY()
	UCameraComponent* M_CameraComponent = nullptr;

	UPROPERTY()
	bool bM_UpdateMiniMapWithCamera = false;

	UPROPERTY()
	TObjectPtr<AFowManager> M_FowManager = nullptr;

	UPROPERTY()
	TObjectPtr<UW_MiniMap> M_MiniMap = nullptr;

	UPROPERTY()
	TObjectPtr<USpringArmComponent> M_SpringArmComponent = nullptr;

	UPROPERTY()
	TObjectPtr<ACPPController> M_PlayerController = nullptr;

	void OnMainMenuLoaded();

	void DrawCameraOnMiniMap();


	UPROPERTY()
	FViewportProjections M_ViewportProjections;

	UPROPERTY()
	FPlane M_LandscapePlane;

	FVector2D GetMiniMapLocationRelativeToFow(
		const float SpringArmLength,
		const FVector& FowLocation,
		const float MapExtent, const FVector2D ScreenPositionToProject, bool& bProjectionSuccessful
	) const;
};
