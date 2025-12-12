// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "OrDoubleRequirement.h"

#include "RTS_Survival/GameUI/TrainingUI/TrainingUIWidget/TrainingItemState/TrainingWidgetState.h"
#include "RTS_Survival/Requirement/RTSRequirement.h"
#include "RTS_Survival/Requirement/RTSRequirementHelpers/FRTSRequirementHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UOrDoubleRequirement::UOrDoubleRequirement()
{
	// No single-type presentation anymore; widget shows two icons itself.
	SetRequirementType(ERTSRequirement::Requirement_None);
	SetRequirementCount(ERequirementCount::DoubleOrRequirement);
	M_RequiredTech1 = ETechnology::Tech_NONE;
	M_RequiredTech2 = ETechnology::Tech_NONE;
	M_RequiredUnit1 = FTrainingOption();
	M_RequiredUnit2 = FTrainingOption();
	M_FirstRequirementType = ERTSRequirement::Requirement_None;
	M_SecondRequirementType = ERTSRequirement::Requirement_None;
}

void UOrDoubleRequirement::InitOrRequirement(URTSRequirement* First, URTSRequirement* Second)
{
	if (!IsValid(First) || !IsValid(Second))
	{
		RTSFunctionLibrary::ReportError(TEXT("UOrDoubleRequirement::InitOrRequirement received invalid child(ren)."));
		return;
	}

	// Own/duplicate children into this composite so snapshots deep-copy the graph.
	auto OwnOrDup = [this](URTSRequirement* Child) -> URTSRequirement*
	{
		return (Child->GetOuter() == this) ? Child : DuplicateObject<URTSRequirement>(Child, this);
	};

	M_First  = OwnOrDup(First);
	M_Second = OwnOrDup(Second);

	// Seed to None; widget derives icons per child.
	SetRequirementType(ERTSRequirement::Requirement_None);

	// Prime initial text/presentation (treated as unmet for formatting preview).
	const bool bFirstMet  = false;
	const bool bSecondMet = false;
	UpdatePresentationFromChildren(bFirstMet, bSecondMet, /*Trainer=*/nullptr, /*bIsCheckingForQueuedItem=*/false);
}

bool UOrDoubleRequirement::CheckIsRequirementMet(UTrainerComponent* Trainer, const bool bIsCheckingForQueuedItem)
{
	if (!IsValid(M_First) || !IsValid(M_Second))
	{
		RTSFunctionLibrary::ReportError(TEXT("UOrDoubleRequirement has invalid child requirement(s)."));
		// Keep kinds consistent with what UpdatePresentationFromChildren will compute.
		UpdatePresentationFromChildren(/*bFirstMet*/false, /*bSecondMet*/false, Trainer, bIsCheckingForQueuedItem);
		return false;
	}

	const bool bFirstMet  = M_First->CheckIsRequirementMet(Trainer, bIsCheckingForQueuedItem);
	const bool bSecondMet = M_Second->CheckIsRequirementMet(Trainer, bIsCheckingForQueuedItem);

	// Always refresh per-child required fields and unmet kinds.
	UpdatePresentationFromChildren(bFirstMet, bSecondMet, Trainer, bIsCheckingForQueuedItem);

	// OR is met if any child is met.
	return (bFirstMet || bSecondMet);
}


void UOrDoubleRequirement::UpdatePresentationFromChildren(
	const bool bFirstMet, const bool bSecondMet,
	UTrainerComponent* /*Trainer*/, const bool /*bIsCheckingForQueuedItem*/)
{
	// Helper: populate required unit/tech for a child, regardless of met/unmet,
	// and return its requirement kind. AircraftPad → set BOTH fields deterministically.
	auto ExtractRequired = [](URTSRequirement* Req,
	                          /*out*/ FTrainingOption& OutUnit,
	                          /*out*/ ETechnology& OutTech) -> ERTSRequirement
	{
		if (!IsValid(Req))
		{
			OutUnit = FTrainingOption();
			OutTech = ETechnology::Tech_NONE;
			return ERTSRequirement::Requirement_None;
		}

		const ERTSRequirement Kind = Req->GetRequirementType();
		switch (Kind)
		{
		case ERTSRequirement::Requirement_Tech:
			OutUnit = FTrainingOption();
			OutTech = Req->GetRequiredTechnology();
			break;

		case ERTSRequirement::Requirement_Unit:
		case ERTSRequirement::Requirement_Expansion:
			OutUnit = Req->GetRequiredUnit();
			OutTech = ETechnology::Tech_NONE;
			break;

		case ERTSRequirement::Requirement_VacantAircraftPad:
			OutUnit = FTrainingOption();           // explicit, never uninitialized
			OutTech = ETechnology::Tech_NONE;      // explicit, never uninitialized
			break;

		default:
			OutUnit = FTrainingOption();
			OutTech = ETechnology::Tech_NONE;
			break;
		}
		return Kind;
	};

	// Always populate required fields for both children.
	const ERTSRequirement Kind1 = ExtractRequired(M_First,  M_RequiredUnit1, M_RequiredTech1);
	const ERTSRequirement Kind2 = ExtractRequired(M_Second, M_RequiredUnit2, M_RequiredTech2);

	// Unmet-kind flags drive the two icons.
	// We always set the kinds even if the requirements are met to make sure we can fill out both in the UI.
	M_FirstRequirementType   =  Kind1;
	M_SecondRequirementType =  Kind2;
	M_bSecondRequirementMet = bSecondMet;
	M_bFirstRequirementMet  = bFirstMet;

	// IMPORTANT: primary base enum must always reflect the FIRST requirement's kind,
	// so the UI's primary icon comes from GetTrainingStatusWhenRequirementNotMet().
	SetRequirementType(Kind1);
}


ETrainingItemStatus UOrDoubleRequirement::GetTrainingStatusWhenRequirementNotMet() const
{
	return FRTS_RequirementHelpers::MapKindToStatus(M_FirstRequirementType);
}

ETrainingItemStatus UOrDoubleRequirement::GetSecondaryRequirementStatus() const
{
	return FRTS_RequirementHelpers::MapKindToStatus(M_SecondRequirementType);
}
