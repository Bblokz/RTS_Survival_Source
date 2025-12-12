// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "EngineUtils.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

#include "CPPHUD.generated.h"

// forward declaration
class RTS_SURVIVAL_API ACPPController;
class RTS_SURVIVAL_API ASelectableActorObjectsMaster;
class RTS_SURVIVAL_API ASelectablePawnMaster;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API ACPPHUD : public AHUD
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	ACPPController* PLayerController;

	/** @brief Starts unit selection using marquee. */
	void StartSelection();

	/** @brief Stops marquee unit selection. */
	void StopSelection();
	
	/** @brief returns the UnitMasters in the selection rectangle, resets after */
	TArray<ASquadUnit*> GetSquadUnitsInRectangle();

	/** @brief returns the SelectableActors in rectangle, resets after */
	TArray<ASelectableActorObjectsMaster*> GetSelectableActors();
	
	/** @brief returns the SelectablePawns in rectangle, resets after */
	TArray<ASelectablePawnMaster*> GetSelectablePawns();

	/** @brief Resets any current selection marquee. No units will be selected. */
	inline void CancelSelection() {bM_StartSelecting = false;};
	

private:
	UPROPERTY()
	// Point where we start Drawing
	FVector2D M_InitialPoint;
	
	// Mouse
	UFUNCTION()
    FVector2D GetMousePosition2D() const;

	/**
	* @brief Selects all Actors of type UnitMaster inside the marquee selection box given by the two passed on points.
	* Uses a projection to screen space.
	* @param FirstPoint: Begin point of the drawn marquee selection.
	* @param SecondPoint: Current/last point of the marquee that is drawn.
	* @param OutActors: TArray with all unitmasters that are selected.
	* @param bActorMustBeFullyEnclosed: If the marquee needs to contain all of the actor's bounds.
	*/ 
	void GetSquadUnitsInSelectionRectangle(const FVector2D& FirstPoint,
	                                        const FVector2D& SecondPoint,
	                                        TArray<ASquadUnit*>& OutActors,
	                                        bool bActorMustBeFullyEnclosed) const;

	/**
	* @brief Selects all Actors of type SelectableActor inside the marquee selection box given by the two passed on points.
	* Uses a projection to screen space.
	* @param FirstPoint: Begin point of the drawn marquee selection.
	* @param SecondPoint: Current/last point of the marquee that is drawn.
	* @param OutActors: TArray with all SelectableActors that are selected.
	* @param bActorMustBeFullyEnclosed: If the marquee needs to contain all of the actor's bounds.
	*/ 
	void GetSelectableActorsInRectangle( const FVector2D& FirstPoint,
		const FVector2D& SecondPoint,
		TArray<ASelectableActorObjectsMaster*>& OutActors,
		bool bActorMustBeFullyEnclosed);
	
	/**
	* @brief Selects all Actors of type ASelectablePawnMaster inside the marquee selection box given by the two passed on points.
	* Uses a projection to screen space.
	* @param FirstPoint: Begin point of the drawn marquee selection.
	* @param SecondPoint: Current/last point of the marquee that is drawn.
	* @param OutActors: TArray with all ASelectablePawnMaster that are selected.
	* @param bActorMustBeFullyEnclosed: If the marquee needs to contain all of the actor's bounds.
	*/ 
	void GetSelectablePawnsInRectangle( const FVector2D& FirstPoint,
		const FVector2D& SecondPoint,
		TArray<ASelectablePawnMaster*>& OutActors,
		bool bActorMustBeFullyEnclosed);
	bool IsInViewFrustum(const FVector& ObjectLocation) const;

	virtual void DrawHUD() override;
	void OnSelectionEnded(const FVector2D MarqueeEndPoint);
	void ResetMarqueeForNextDraw();

	FTimerHandle M_MousePositionHandle;
	
	bool bM_StartSelecting = false;
	
	bool bM_EndSelection  = false;

	bool bM_CancelSelection = false;
	
	UPROPERTY()
	TArray<ASquadUnit*> M_TSquadUnitsInRectangle;

	UPROPERTY()
	TArray<ASelectableActorObjectsMaster*> M_TSelectableActorsInRectangle;

	UPROPERTY()
	TArray<ASelectablePawnMaster*> M_TSelectablePawnsInRectangle;
	
};
