// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PlayerAimAbilityTypes/PlayerAimAbilityTypes.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "PlayerAimAbility.generated.h"

enum class EGrenadeAbilityType : uint8;
enum class EAbilityID : uint8;
class UGlobalAbility;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
struct FGlobalAbilityAimSettings;

/** @brief What the player is aiming with the custom target area of a global ability. */
UENUM()
enum class EGlobalAbilityAreaAimState : uint8
{
	Inactive,
	// A round area or the start of a strip follows the cursor.
	FollowingCursor,
	// The first click fixed the start of the strip; its end follows the cursor.
	DrawingStrip
};

UCLASS()
class RTS_SURVIVAL_API APlayerAimAbility : public AActorObjectsMaster
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APlayerAimAbility(const FObjectInitializer& ObjectInitializer);
	void InitPlayerAimAbility(ACPPController* PlayerController);

	void DetermineShowAimRadiusForAbility(
		const EAbilityID MainAbility,
		const int32 AbilitySubType, AActor* PrimarySelectedActor);
	void HideRadius();
	void ShowGlobalAbilityRadius(const float Radius, const EPlayerAimAbilityTypes AimType);

	/**
	 * @brief Shows the custom target area of a global ability instead of a crosshair.
	 * @param AimSettings Shape, shader and sizes of the area.
	 * @param Ability Asked where its drops will land while a strip is drawn; may be null.
	 */
	void ShowGlobalAbilityArea(const FGlobalAbilityAimSettings& AimSettings, const UGlobalAbility* Ability);

	/** @return True while a custom target area is shown; it does not turn with the selected unit. */
	[[nodiscard]] bool IsAreaAimActive() const { return M_AreaAimState != EGlobalAbilityAreaAimState::Inactive; }

	void UpdateGlobalAbilityArea(const FVector& CursorLocation);

	/**
	 * @brief The first click of a strip fixes where the flight starts.
	 * @param ClickedLocation Ground location of the click.
	 * @return True when the click was used for this, so it must not execute the ability.
	 */
	[[nodiscard]] bool TryAnchorStrip(const FVector& ClickedLocation);

	/**
	 * @param ClickedLocation Ground location of the click that finishes the strip.
	 * @param OutLineStart Where the flight starts.
	 * @param OutLineEnd Where the flight ends, limited to the length the ability allows.
	 * @return True when a strip is being drawn.
	 */
	[[nodiscard]] bool GetStripLine(const FVector& ClickedLocation, FVector& OutLineStart, FVector& OutLineEnd) const;

	[[nodiscard]] bool IsPlayerAimActive() const { return bM_IsPlayerAimActive; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UStaticMeshComponent> AimMeshComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TMap<EPlayerAimAbilityTypes, UMaterialInterface*> MaterialPerAbilityAim;

	// How 1 unit of scale increase translates to world units.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float UnitsPerScale = 100.0f;

private:
	[[nodiscard]] bool GetIsValidAimMeshComponent() const;
	[[nodiscard]] bool GetIsValidMaterialForAimType(const EPlayerAimAbilityTypes AimType,
	                                                UMaterialInterface*& OutMaterial);
	UPROPERTY()
	bool bM_IsPlayerAimActive = false;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	[[nodiscard]] bool GetIsValidPlayerController() const;

	void OnAimActivated_PlayAnnouncerVl();


	EPlayerAimAbilityTypes GetAimTypeForAbility(const EAbilityID MainAbility, const int32 AbilitySubType,
	                                            const AActor* RTSValid_PrimarySelectedActor,
	                                            float& OutAbilityRadius) const;

	EPlayerAimAbilityTypes GetAimTypeFromGrenadeAbility(const EGrenadeAbilityType GrenadeAbilityType,
	                                                    const AActor* RTSValid_PrimarySelectedActor,
	                                                    float& OutAbilityRadius) const;

	void FailedToShowAimRadius();
	void OnNoRadiusForValidAimAbility(const float Radius, const EPlayerAimAbilityTypes AimType,
	                                  const EAbilityID MainAbility, const int32 AbilitySubType) const;

	void SetMaterialParameter(const float Radius, const EPlayerAimAbilityTypes TypeSet);

	// Second plane for custom target areas; its X and Y scale separately, which the crosshair textures cannot do.
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_AreaMeshComponent;

	// One instance per drop point of the strip, in world space on the ground.
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> M_DropPointMarkers;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_AreaMaterialInstance;

	TWeakObjectPtr<const UGlobalAbility> M_AreaAimAbility;

	EGlobalAbilityAreaAimState M_AreaAimState = EGlobalAbilityAreaAimState::Inactive;
	FVector M_StripStart = FVector::ZeroVector;

	// Copy of the settings the area was shown with; the ability template can not be reached from here.
	struct FAreaAimShownState
	{
		EGlobalAbilityAimShape Shape = EGlobalAbilityAimShape::Reticle;
		float AreaRadius = 0.f;
		float InnerRadius = 0.f;
		float MinStripLength = 0.f;
		float MaxStripLength = 0.f;
		float LengthBeforeAnchor = 0.f;
		int32 ShaderMarkerCount = 0;
		float DropPointScale = 1.f;
	};

	FAreaAimShownState M_AreaAimShown;

	void HideArea();
	FVector GetClampedStripEnd(const FVector& CursorLocation) const;
	float GetCameraYaw() const;

	/**
	 * @brief Puts the plane on a strip and hands the shader its sizes in cm.
	 * @param LineStart Start of the strip.
	 * @param LineEnd End of the strip; equal to LineStart for a round area.
	 * @param Yaw Direction of the strip, used when the line has no length.
	 */
	void PlaceArea(const FVector& LineStart, const FVector& LineEnd, const float Yaw);
	void ApplyAreaParameters(const float StripLength, const float PlaneLength, const float PlaneWidth) const;
	void UpdateDropPointMarkers(const FVector& LineStart, const FVector& LineEnd);
};
