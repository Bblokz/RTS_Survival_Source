// Copyright Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BuildingPreviewMode/PlayerBuildingPreviewMode.h"
#include "BxpConstructionData/BxpConstructionData.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetComponent.h"
#include "ConstructionPreviewSounds/ConstructionPreviewSounds.h"
#include "NomadicPreviewAttachments/FNomadicPreviewAttachments.h"
#include "NomadicPreviewAttachments/FNomadicPreviewAttachmentState.h"
#include "RTS_Survival/RTSComponents/RadiusComp/RadiusComp.h"
#include "SocketConstructionWidget/WSocketConstructionWidget.h"


#include "CPPConstructionPreview.generated.h"

class ABuildingGridOverlay;
struct FBxpConstructionData;
enum class EPlayerBuildingPreviewMode : uint8;
class AConstructionRadiusHelper;
class UW_PreviewStats;
class RTS_SURVIVAL_API ACPPController;
// Forward declarations.
class RTS_SURVIVAL_API AStaticPreviewMesh;

/**
 * @brief The construction preview of a building that is moved along a grid at the cursor location.
 * Start and stop the preview with StartBuildingPreview and StopBuildingPreview.
 * Needs to be initialized with InitConstructionPreview.
 * On the second click of building a static preview mesh is spawned which is saved at the constructor unit.
 */
UCLASS()
class RTS_SURVIVAL_API ACPPConstructionPreview : public AActorObjectsMaster
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ACPPConstructionPreview(const FObjectInitializer& ObjectInitializer);

	FVector GetBxpPreviewLocation(const FVector& ClickedLocation) const;

	FName GetBxpSocketName() const;

	/** @return If the building preview is overlapping with something. */
	bool GetIsBuildingPreviewBlocked() const;

	FBxpConstructionData GetBxpConstructionData() const { return BxpConstructionData; }

	AStaticPreviewMesh* CreateStaticMeshActor(const FRotator& Rotation, ANomadicVehicle* NomadicConstructing) const;

	/**
	 * @brief Starts the static mesh preview of the building of the provided mesh.
	 * @param NewPreviewMesh The mesh that will be displayed.
	 * @param BuildRadii The build radii in which we can place the building.
	 * @param bNeedWithinBuildRadius Whether the building needs to be placed within a build radius note that if this is set
	 * to false the radii will be ignored.
	 * @param PreviewAttachments
	 * @note Do not call directly but use the CppController::StartBuildingPreview.
	 */
	void StartNomadicPreview(
		UStaticMesh* NewPreviewMesh, const TArray<URadiusComp*>& BuildRadii, const bool bNeedWithinBuildRadius = false,
		const
		FNomadicPreviewAttachments& PreviewAttachments = {});


	// pre bxp construction data is valid.
	/**
	 * @brief Sets the active preview mode depending on the type of provided building expansion.
	 * @param NewPreviewMesh What mesh to use for preview (gets dymaic materials for construction applied).
	 * @param bNeedWithinBuildRadius Whether the preview mesh needs to be placed within a building radius. Note that
	 * if the mode is set to snap to sockets or origin then no radius checks are preformed.
	 * @param HostLocation 
	 * @param ConstructionData 
	 */
	void StartBxpPreview(UStaticMesh* NewPreviewMesh, const bool bNeedWithinBuildRadius = false, const FVector&
		                     HostLocation = {}, const FBxpConstructionData& ConstructionData = FBxpConstructionData());

	void StartFieldConstructionPreview(UStaticMesh* PreviewMesh,
		const bool bNeedWithinBuildRadius = false)


	inline UStaticMesh* GetPreviewMesh() const { return PreviewMesh->GetStaticMesh(); }

	/**
	 * @brief Stops the static mesh preview of the building by setting the preview mesh to nullptr.
	 * And letting the tick function know that there is no active preview.
	 */
	UFUNCTION(BlueprintCallable)
	void StopBuildingPreview(const bool bIsPlacedSuccessfully = false);

	/**
	 * @brief Rotates the building preview clockwise by m_RotationDegrees.
	 *
	 * This function updates the rotation of the preview mesh, rotating it clockwise around its up axis
	 * by the angle specified in m_RotationDegrees.
	 */
	UFUNCTION()
	void RotatePreviewClockwise() const;

	/**
	 * @brief Rotates the building preview counterclockwise by m_RotationDegrees.
	 *
	 * This function updates the rotation of the preview mesh, rotating it counterclockwise around its up axis
	 * by the angle specified in m_RotationDegrees.
	 */
	UFUNCTION()
	void RotatePreviewCounterclockwise() const;

	FRotator GetPreviewRotation() const;

	UStaticMeshSocket* GetBxpSocketSnappingSnappedToSocket() const { return M_SocketToSnapTo; }

protected:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TSubclassOf<AConstructionRadiusHelper> ExpansionRadiusHelperClass;
	/**
	 * Call in begin play.
	 * @param NewPlayerController The playercontroller.
	 * @param ConstructionMaterial The material to use for the preview mesh.
	 * @param PreviewStatsWidget The widget that shows building instructions and current stats of the construction placement.
	 * @param PreviewStatsWidgetComponent The widget component that shows the preview stats widget.
	 * @param NewPreviewSoundSettings
	 * @param InSocketWidgetClass
	 * @param InGridOverlayClass
	 */
	UFUNCTION(BlueprintCallable, Category="ReferenceCasts")
	void InitConstructionPreview(
		ACPPController* NewPlayerController,
		UMaterialInstance* ConstructionMaterial,
		UW_PreviewStats* PreviewStatsWidget,
		UWidgetComponent* PreviewStatsWidgetComponent,
		FConstructionPreviewSounds NewPreviewSoundSettings, TSubclassOf<UWSocketConstructionWidget> InSocketWidgetClass,
		TSubclassOf
		<ABuildingGridOverlay> InGridOverlayClass);

	UPROPERTY(BlueprintReadOnly, Category="Reference")
	ACPPController* PlayerController;

	/**
	 * Calculates the mouse position on the landscape and snaps it to the grid if there is an active preview.
	 * Displays the preview mesh at the cursor location with the valid or invalid material depending on the overlap.
	 * @param DeltaTime 
	 */
	virtual void Tick(float DeltaTime) override;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Name for the preview mesh component.
	static FName PreviewMeshComponentName;

	/** The static mesh on which the preview is placed. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Transient)
	TObjectPtr<UStaticMeshComponent> PreviewMesh;

	UPROPERTY(BlueprintReadOnly)
	FVector CursorWorldPosition;

	// Degrees to rotate the building preview
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Building Rotation")
	float RotationDegrees;

private:
	bool EnsureBxpPreviewRequestIsValid(
		const UStaticMesh* NewPreviewMesh,
		const bool bNeedWithinBuildRadius, const FVector& HostLocation,
		const FBxpConstructionData& ConstructionData) const;

	// Keeps track of the build radius that the nomadic building will provide once placed.
	FNomadicPreviewAttachmentState M_NomadicPreviewAttachmentState;

	// The tick used for nomadic building placement and free bxp building placement
	// Note that it is still possible to not have to place in building radii if bM_NeedWithinBuildRadius is false for
	// example when placing the Nomadic HQ building.
	bool TickRadiiBuildingPlacement(bool& bOutHasValidIncline);

	// Snaps to the closest socket location, disregards slopes/inlcline.
	bool TickSocketBxpBuildingPlacement(bool& bOutHasValidIncline);

	// Snaps to the origin of the building, disregards slopes/incline.
	bool TickAtBuildingOriginBxpPlacement(bool& bOutHasValidIncline);

	UPROPERTY()
	TObjectPtr<AConstructionRadiusHelper> M_ExpansionRadiusHelper;

	bool GetIsValidExpansionRadiusHelper() const;

	/** @return The world mouse position snapped to the grid. */
	FVector GetGridSnapAdjusted(
		const FVector& MouseWorldPosition,
		const float ExtraHeight = 0) const;

	/** @return The Socket location of the one closest to the mouse position. */
	FVector GetSnapToClosestSocketLocation(
		const UStaticMeshComponent* SocketMeshComp,
		const TArray<UStaticMeshSocket*>& SocketList,
		const FVector& InMouseWorldPosition, bool& bOutValidPosition, UStaticMeshSocket*& OutSocket, FRotator&
		OutSocketRotation) const;

	/** @return Regardless of the mouse world position for this type of bxp we snap to the origin of the building. */
	FVector GetSnapToBuildingOriginLocation(
		const UStaticMeshComponent* SocketMeshComp,
		bool& bOutValidPosition) const;

	// Whether there is a preview active.
	EConstructionPreviewMode M_ActivePreviewStatus;
	bool GetHasActivePreview() const;


	/** @return Whether the overlay footprint overlaps with something. */
	bool IsOverlayFootprintOverlapping() const;

	// The Displayed building Material.
	// Safe non-owning reference using GC system.
	UPROPERTY()
	UMaterialInstance* M_ConstructionPreviewMaterial;

	bool bM_IsValidCursorLocation;

	bool bM_IsValidBuildingLocation;

	// The angle in degrees that the slope of the landscape has to the preview mesh.
	float M_SlopeAngle;

	// Widget that shows building instructions and current stats of the construction placement.
	UPROPERTY()
	UW_PreviewStats* M_PreviewStatsWidget;

	// Reference to the stats widget as component.
	UPROPERTY()
	UWidgetComponent* M_PreviewStatsWidgetComponent;

	bool EnsureIsValidPreviewMesh() const;
	bool EnsureIsValidPreviewStatsWidget() const;
	bool EnsureIsValidPreviewStatsWidgetComponent() const;

	/**
	 * @brief Updates the preview widget with the data of the preview.
	 * @param bIsInclineValid Whether the preview's incline results in valid placement.
	 */
	void UpdatePreviewStatsWidget(const bool bIsInclineValid) const;

	void RotatePreviewStatsToCamera() const;

	/** @return Whether the location is within the build radius of the host location. */
	bool IsWithinBuildRadius(const FVector& Location) const;

	/**
	 * @brief Checks if the slope at a given location is within the allowed angle for building placement.
	 *
	 * This function performs a line trace at the specified location to determine the surface normal.
	 * It then calculates the angle between this normal and the world's up vector. If the angle is less than
	 * or equal to m_DegreesAllowedOnHill, the function returns true, indicating the slope is valid for building.
	 *
	 * @param Location The world location where the slope's validity is to be checked.
	 * @return bool True if the slope angle at the location is within the allowed limit, false otherwise.
	 *
	 * @note Attemps to trace from the preview mesh at the predetermined socket points ["FL", "FR", "RL", "RR"].
	 * If these are not found we revert back to the box extend of the preview mesh.
	 */
	bool IsSlopeValid(const FVector& Location);

	/**
	 * @brief Adds the box extent to the provided trace points.
	 * @param OutTracePoints The collection of trace points to add the box extent to.
	 * @param Location The pivot location.
	 * @param BoxExtent The box extend of the preview mesh.
	 */
	void AddBoxExtentToTracePoints(
		TArray<FVector>& OutTracePoints,
		const FVector& Location,
		const FVector& BoxExtent) const;

	// Pool to store dynamic material instances for each material slot.
	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> M_DynamicMaterialPool;

	/**
	 * @brief Initializes the dynamic material pool with the provided base material.
	 * @param BaseMaterial The base material to use for the dynamic material instances.
	 */
	void InitializeDynamicMaterialPool(UMaterialInstance* BaseMaterial);

	/**
	 * @brief Updates the material of the preview mesh to the valid or invalid material.
	 * @param bIsValidLocation Whether the location is valid.
	 */
	void UpdatePreviewMaterial(const bool bIsValidLocation);

	/** Moves the construction widget to the height of the preview mesh. */
	void MoveWidgetToMeshHeight() const;

	/**
	 * @brief Sets the Cursor location.
	 * @param CursorLocation The to landscape snapped position of the cursor.
	 */
	void SetCursorPosition(const FVector& CursorLocation);

	bool GetisWithinBuildRadii(const FVector& PreviewLocation);

	/**
	 * @brief Setup the preview mesh for tick-move-preview-with-mouse and set up the preview stats widget.
	 * @param NewPreviewMesh The mesh to use for previewing.
	 * @param PreviewMode What type of building mode we are in.
	 * @param bPreviewUseCollision Whether the preview mesh will have collision enabled.
	 */
	void InitPreviewAndStatWidgetForConstruction(
		UStaticMesh* NewPreviewMesh,
		const EConstructionPreviewMode PreviewMode,
		const bool bPreviewUseCollision);

	void InitExpansionRadiusHelperForPlacement(const FVector& HostLocation, const float BxpBuildRadius) const;

	UPROPERTY()
	TArray<URadiusComp*> M_BuildRadii;

	UPROPERTY()
	float M_DistanceToClosestRadius;

	UPROPERTY()
	float M_MaXDistanceToClosestRadius;

	// Whether the building needs to be placed within a building radius, it is possible for this to be true but not radii
	// to be available in which case we need to prevent the building from being placed at all.
	UPROPERTY()
	bool bM_NeedWithinBuildRadius;

	// Contains all info for the construction of the current building expansion.
	UPROPERTY()
	FBxpConstructionData BxpConstructionData;

	// The socket the bxp that uses socket snapping is currently snapped to.
	UPROPERTY()
	UStaticMeshSocket* M_SocketToSnapTo;

	FConstructionPreviewSounds M_PreviewSounds;

	bool GetIsRotationAllowed() const;

	/** Widget class to use for socket indicators */
	UPROPERTY()
	TSubclassOf<UWSocketConstructionWidget> SocketWidgetClass;

	/** Height offset above each socket */
	float M_SocketWidgetHeight;

	/** Spawned widget‐components for each socket */
	UPROPERTY()
	TArray<UWidgetComponent*> SocketWidgetComponents;

	void CreateSocketWidgets();
	void DestroySocketWidgets();

	void CreateGridOverlay();

	UPROPERTY()
	TSubclassOf<ABuildingGridOverlay> M_GridOverlayClass;

	UPROPERTY()
	TObjectPtr<ABuildingGridOverlay> M_GridOverlay;

	void SetGridOverlayEnabled(const bool bEnabled) const;

	bool EnsureIsValidGridOverlay() const;
	void UpdateOverlayGridOverlaps();
};
