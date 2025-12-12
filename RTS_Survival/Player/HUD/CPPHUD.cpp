// Copyright (C) 2020-2023 Bas Blokzijl - All rights reserved.
#include "CPPHUD.h"

#include "components/BoxComponent.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectableActorObjectsMaster.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"


FVector2D ACPPHUD::GetMousePosition2D() const
{
	float PosX;
	float PosY;
	GetOwningPlayerController()->GetMousePosition(PosX, PosY);
	return FVector2D(PosX, PosY);
}


void ACPPHUD::GetSquadUnitsInSelectionRectangle(const FVector2D& FirstPoint,
                                                const FVector2D& SecondPoint,
                                                TArray<ASquadUnit*>& OutActors,
                                                bool bActorMustBeFullyEnclosed) const
{
	// Clear previous content.
	OutActors.Reset();

	// Create the selection rectangle.
	FBox2D SelectionRectangle(ForceInit);
	SelectionRectangle += FirstPoint;
	SelectionRectangle += SecondPoint;

	const FVector BoundsPointMapping[8] =
	{
		FVector(1.f, 1.f, 1.f),
		FVector(1.f, 1.f, -1.f),
		FVector(1.f, -1.f, 1.f),
		FVector(1.f, -1.f, -1.f),
		FVector(-1.f, 1.f, 1.f),
		FVector(-1.f, 1.f, -1.f),
		FVector(-1.f, -1.f, 1.f),
		FVector(-1.f, -1.f, -1.f)
	};

	// Iterate through all squad units.
	for (TActorIterator<ASquadUnit> Itr(GetWorld(), ASquadUnit::StaticClass()); Itr; ++Itr)
	{
		ASquadUnit* EachActor = *Itr;

		if (not EachActor || not EachActor->GetSelectionComponent() || not EachActor->GetSelectionComponent()->
			GetSelectionArea() ||
			not EachActor->GetRTSComponent())
		{
			continue;
		}


		// Get the actor's bounding box.
		const FBox EachActorBounds = EachActor->GetSelectionComponent()->GetSelectionArea()->Bounds.GetBox();
		const FVector BoxCenter = EachActorBounds.GetCenter();
		const FVector BoxExtents = EachActorBounds.GetExtent();

		// Check if actor is in view frustum.
		if (!IsInViewFrustum(BoxCenter))
		{
			continue; // Skip if not in view.
		}

		// Build 2D bounding box of actor in screen space.
		FBox2D ActorBox2D(ForceInit);
		for (uint8 BoundsPointItr = 0; BoundsPointItr < 8; BoundsPointItr++)
		{
			FVector2D ScreenLocation;
			if (GetOwningPlayerController()->ProjectWorldLocationToScreen(
				BoxCenter + (BoundsPointMapping[BoundsPointItr] * BoxExtents), ScreenLocation))
			{
				ActorBox2D += ScreenLocation;
			}
		}

		// Check for selection conditions.
		if (EachActor->GetSelectionComponent()->GetCanBeSelected() && EachActor->GetRTSComponent()->GetOwningPlayer() ==
			1)
		{
			if (bActorMustBeFullyEnclosed)
			{
				if (SelectionRectangle.IsInside(ActorBox2D))
				{
					OutActors.Add(EachActor);
				}
			}
			else
			{
				if (SelectionRectangle.Intersect(ActorBox2D))
				{
					OutActors.Add(EachActor);
				}
			}
		}
	}
}

void ACPPHUD::GetSelectableActorsInRectangle(const FVector2D& FirstPoint, const FVector2D& SecondPoint,
                                             TArray<ASelectableActorObjectsMaster*>& OutActors,
                                             bool bActorMustBeFullyEnclosed)
{
	// Clear previous content.
	OutActors.Reset();

	// Create the selection rectangle.
	FBox2D SelectionRectangle(ForceInit);
	SelectionRectangle += FirstPoint;
	SelectionRectangle += SecondPoint;

	const FVector BoundsPointMapping[8] =
	{
		FVector(1.f, 1.f, 1.f),
		FVector(1.f, 1.f, -1.f),
		FVector(1.f, -1.f, 1.f),
		FVector(1.f, -1.f, -1.f),
		FVector(-1.f, 1.f, 1.f),
		FVector(-1.f, 1.f, -1.f),
		FVector(-1.f, -1.f, 1.f),
		FVector(-1.f, -1.f, -1.f)
	};

	// Iterate through all selectable actors.
	for (TActorIterator<ASelectableActorObjectsMaster> Itr(GetWorld(), ASelectableActorObjectsMaster::StaticClass());
	     Itr; ++Itr)
	{
		ASelectableActorObjectsMaster* EachActor = *Itr;

		if (not EachActor || not EachActor->GetSelectionComponent() || not EachActor->GetSelectionComponent()->
			GetSelectionArea() ||
			not EachActor->GetRTSComponent())
		{
			continue;
		}


		// Get the actor's bounding box.
		const FBox EachActorBounds = EachActor->GetSelectionComponent()->GetSelectionArea()->Bounds.GetBox();
		const FVector BoxCenter = EachActorBounds.GetCenter();
		const FVector BoxExtents = EachActorBounds.GetExtent();

		// Check if actor is in view frustum.
		if (!IsInViewFrustum(BoxCenter))
		{
			continue; // Skip if not in view.
		}

		// Build 2D bounding box of actor in screen space.
		FBox2D ActorBox2D(ForceInit);
		for (uint8 BoundsPointItr = 0; BoundsPointItr < 8; BoundsPointItr++)
		{
			FVector2D ScreenLocation;
			if (GetOwningPlayerController()->ProjectWorldLocationToScreen(
				BoxCenter + (BoundsPointMapping[BoundsPointItr] * BoxExtents), ScreenLocation))
			{
				ActorBox2D += ScreenLocation;
			}
		}

		// Check for selection conditions.
		if (EachActor->GetSelectionComponent()->GetCanBeSelected() && EachActor->GetRTSComponent()->GetOwningPlayer() ==
			1 && !EachActor->bIsHidden)
		{
			if (bActorMustBeFullyEnclosed)
			{
				if (SelectionRectangle.IsInside(ActorBox2D))
				{
					OutActors.Add(EachActor);
				}
			}
			else
			{
				if (SelectionRectangle.Intersect(ActorBox2D))
				{
					OutActors.Add(EachActor);
				}
			}
		}
	}
}

void ACPPHUD::GetSelectablePawnsInRectangle(const FVector2D& FirstPoint,
                                            const FVector2D& SecondPoint,
                                            TArray<ASelectablePawnMaster*>& OutActors,
                                            bool bActorMustBeFullyEnclosed)
{
	// Clear previous content.
	OutActors.Reset();

	// Create the selection rectangle.
	FBox2D SelectionRectangle(ForceInit);
	SelectionRectangle += FirstPoint;
	SelectionRectangle += SecondPoint;

	const FVector BoundsPointMapping[8] =
	{
		FVector(1.f, 1.f, 1.f),
		FVector(1.f, 1.f, -1.f),
		FVector(1.f, -1.f, 1.f),
		FVector(1.f, -1.f, -1.f),
		FVector(-1.f, 1.f, 1.f),
		FVector(-1.f, 1.f, -1.f),
		FVector(-1.f, -1.f, 1.f),
		FVector(-1.f, -1.f, -1.f)
	};

	// Iterate through all selectable pawns.
	for (TActorIterator<ASelectablePawnMaster> Itr(GetWorld(), ASelectablePawnMaster::StaticClass()); Itr; ++Itr)
	{
		ASelectablePawnMaster* EachActor = *Itr;

		// Get the actor's bounding box.
		if (not EachActor || not EachActor->GetSelectionComponent() || not EachActor->GetSelectionComponent()->
			GetSelectionArea() ||
			not EachActor->GetRTSComponent())
		{
			continue;
		}
		const FBox EachActorBounds = EachActor->GetSelectionComponent()->GetSelectionArea()->Bounds.GetBox();
		const FVector BoxCenter = EachActorBounds.GetCenter();
		const FVector BoxExtents = EachActorBounds.GetExtent();

		// Check if actor is in view frustum.
		if (!IsInViewFrustum(BoxCenter))
		{
			continue; // Skip if not in view.
		}

		// Build 2D bounding box of actor in screen space.
		FBox2D ActorBox2D(ForceInit);
		for (uint8 BoundsPointItr = 0; BoundsPointItr < 8; BoundsPointItr++)
		{
			FVector2D ScreenLocation;
			if (GetOwningPlayerController()->ProjectWorldLocationToScreen(
				BoxCenter + (BoundsPointMapping[BoundsPointItr] * BoxExtents), ScreenLocation))
			{
				ActorBox2D += ScreenLocation;
			}
		}

		// Check for selection conditions.
		if ((EachActor->GetSelectionComponent()->GetCanBeSelected() && EachActor->GetRTSComponent()->GetOwningPlayer()
			== 1) && !EachActor->GetIsHidden())
		{
			if (bActorMustBeFullyEnclosed)
			{
				if (SelectionRectangle.IsInside(ActorBox2D))
				{
					OutActors.Add(EachActor);
				}
			}
			else
			{
				if (SelectionRectangle.Intersect(ActorBox2D))
				{
					OutActors.Add(EachActor);
				}
			}
		}
	}
}

bool ACPPHUD::IsInViewFrustum(const FVector& ObjectLocation) const
{
	const FVector CameraLocation = GetOwningPlayerController()->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = GetOwningPlayerController()->PlayerCameraManager->GetActorForwardVector();
	const FVector DirectionToObject = (ObjectLocation - CameraLocation).GetSafeNormal();

	// Check if the dot product is above a certain threshold (e.g., 0.0)
	return FVector::DotProduct(CameraForward, DirectionToObject) > 0.0f;
}

void ACPPHUD::DrawHUD()
{
	if (not IsValid(PLayerController))
	{
		return;
	}
	if (bM_StartSelecting)
	{
		// Calculate Current Mouse Position on Screen
		const FVector2D CurrentPoint = GetMousePosition2D();

		if (bM_EndSelection)
		{
			OnSelectionEnded(CurrentPoint);
		}
		else
		{
			FLinearColor BlueColor = FLinearColor::Blue;
			BlueColor.A = 0.4f;
			DrawRect(BlueColor, M_InitialPoint.X, M_InitialPoint.Y,
			         CurrentPoint.X - M_InitialPoint.X, CurrentPoint.Y - M_InitialPoint.Y);
		}
	}
}

void ACPPHUD::OnSelectionEnded(const FVector2D MarqueeEndPoint)
{
	if (not bM_CancelSelection)
	{
		FLinearColor GreenColor = FLinearColor::Green;
		GreenColor.A = 0.4f;

		DrawRect(GreenColor, M_InitialPoint.X, M_InitialPoint.Y,
		         MarqueeEndPoint.X - M_InitialPoint.X, MarqueeEndPoint.Y - M_InitialPoint.Y);


		// Obtain units from marquee.
		GetSquadUnitsInSelectionRectangle(M_InitialPoint, MarqueeEndPoint,
		                                  M_TSquadUnitsInRectangle, false);
		GetSelectableActorsInRectangle(M_InitialPoint, MarqueeEndPoint,
		                               M_TSelectableActorsInRectangle, false);
		GetSelectablePawnsInRectangle(M_InitialPoint, MarqueeEndPoint,
		                              M_TSelectablePawnsInRectangle, false);
		PLayerController->SelectUnitsFromMarquee(M_TSquadUnitsInRectangle,
		                                         M_TSelectableActorsInRectangle, M_TSelectablePawnsInRectangle);
		M_TSquadUnitsInRectangle.Empty();
		M_TSelectableActorsInRectangle.Empty();
		M_TSelectablePawnsInRectangle.Empty();
	}
	ResetMarqueeForNextDraw();
}

void ACPPHUD::ResetMarqueeForNextDraw()
{
	bM_EndSelection = false;
	bM_StartSelecting = false;
	bM_CancelSelection = false;
}


void ACPPHUD::StartSelection()
{
	if (!bM_StartSelecting)
	{
		M_InitialPoint = GetMousePosition2D();
		bM_StartSelecting = true;
	}
}

void ACPPHUD::StopSelection()
{
	const FVector2D CurrentMousePosition = GetMousePosition2D();
	// Disregard two pixels of movement.
	constexpr float MoveTolerance = DeveloperSettings::UIUX::MouseDragThreshold;
	bM_CancelSelection = M_InitialPoint.Equals(CurrentMousePosition, MoveTolerance);
	if (bM_StartSelecting)
	{
		bM_EndSelection = true;
	}
}

TArray<ASquadUnit*> ACPPHUD::GetSquadUnitsInRectangle()
{
	TArray<ASquadUnit*> Temp = M_TSquadUnitsInRectangle;
	M_TSquadUnitsInRectangle.Empty();
	return Temp;
}

TArray<ASelectableActorObjectsMaster*> ACPPHUD::GetSelectableActors()
{
	TArray<ASelectableActorObjectsMaster*> Temp = M_TSelectableActorsInRectangle;
	M_TSelectableActorsInRectangle.Empty();
	return Temp;
}

TArray<ASelectablePawnMaster*> ACPPHUD::GetSelectablePawns()
{
	TArray<ASelectablePawnMaster*> Temp = M_TSelectablePawnsInRectangle;
	M_TSelectablePawnsInRectangle.Empty();
	return Temp;
}
