// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "FRTSRequirementHelpers.h"

#include "RTS_Survival/GameUI/TrainingUI/TrainingUIWidget/TrainingItemState/TrainingWidgetState.h"
#include "RTS_Survival/Requirement/TechRequirement/TechRequirement.h"
#include "RTS_Survival/Requirement/UnitRequirement/UnitRequirement.h"
#include "RTS_Survival/Requirement/VacantAirPadRequirement/VacantAirPadRequirement.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h" // Global_GetTechDisplayName
#include "RTS_Survival/Requirement/DoubleRequirement/DoubleRequirement.h"
#include "RTS_Survival/Requirement/OrRequirement/OrDoubleRequirement.h"
#include "RTS_Survival/Requirement/UnitRequirement/ExpansionRequirement/ExpansionRequirement.h"

// ----------------------------- Singles ---------------------------------------

UTechRequirement* FRTS_RequirementHelpers::CreateTechRequirement(UObject* Outer, const ETechnology RequiredTechnology)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UTechRequirement* TechRequirement = NewObject<UTechRequirement>(UseOuter);
	if (not IsValid(TechRequirement))
	{
		RTSFunctionLibrary::ReportError("Failed to create TechRequirement (allocation failed).");
		return nullptr;
	}
	TechRequirement->InitTechRequirement(RequiredTechnology);
	return TechRequirement;
}

UUnitRequirement* FRTS_RequirementHelpers::CreateUnitRequirement(UObject* Outer, const FTrainingOption& UnitRequired)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UUnitRequirement* UnitRequirement = NewObject<UUnitRequirement>(UseOuter);
	if (not IsValid(UnitRequirement))
	{
		RTSFunctionLibrary::ReportError("Failed to create UnitRequirement (allocation failed)."
			"\n Unit: " + UnitRequired.GetTrainingName());
		return nullptr;
	}
	UnitRequirement->InitUnitRequirement(UnitRequired);
	return UnitRequirement;
}

UVacantAirPadRequirement* FRTS_RequirementHelpers::CreateVacantAirPadRequirement(UObject* Outer)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UVacantAirPadRequirement* VacantAirPadRequirement = NewObject<UVacantAirPadRequirement>(UseOuter);
	if (not IsValid(VacantAirPadRequirement))
	{
		RTSFunctionLibrary::ReportError("Failed to create VacantAirPadRequirement (allocation failed).");
		return nullptr;
	}
	VacantAirPadRequirement->InitVacantAirPadRequirement();
	return VacantAirPadRequirement;
}

UExpansionRequirement* FRTS_RequirementHelpers::CreateExpansionRequirement(
	UObject* Outer,
	const FTrainingOption& ExpansionRequired)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UExpansionRequirement* ExpansionReq = NewObject<UExpansionRequirement>(UseOuter);
	if (not IsValid(ExpansionReq))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to create ExpansionRequirement (allocation failed)."
			"\n Expansion: " + ExpansionRequired.GetTrainingName());
		return nullptr;
	}

	ExpansionReq->InitExpansionRequirement(ExpansionRequired);
	return ExpansionReq;
}

ETrainingItemStatus FRTS_RequirementHelpers::MapKindToStatus(const ERTSRequirement& Requirement)
{
	switch (Requirement)
	{
	case ERTSRequirement::Requirement_Tech: return ETrainingItemStatus::TS_LockedByTech;
	case ERTSRequirement::Requirement_Expansion: return ETrainingItemStatus::TS_LockedByExpansion;
	case ERTSRequirement::Requirement_Unit: return ETrainingItemStatus::TS_LockedByUnit;
	case ERTSRequirement::Requirement_VacantAircraftPad: return ETrainingItemStatus::TS_LockedByNeedVacantAircraftPad;
	default:
		{
			RTSFunctionLibrary::ReportError("The Requirement state: "
				+ FString::FromInt(static_cast<int32>(Requirement))
				+ " is not mapped to a valid TrainingItemStatus; will return Unlocked.");
			return ETrainingItemStatus::TS_Unlocked;
		}
	}
}

// ----------------------------- Doubles (AND) ---------------------------------

URTSRequirement* FRTS_RequirementHelpers::CreateDoubleRequirement(
	UObject* Outer,
	URTSRequirement* First,
	URTSRequirement* Second)
{
	if (not IsValid(First) || not IsValid(Second))
	{
		RTSFunctionLibrary::ReportError("CreateDoubleRequirement called with invalid child requirement(s).");
		return nullptr;
	}

	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UDoubleRequirement* DoubleReq = NewObject<UDoubleRequirement>(UseOuter);
	if (not IsValid(DoubleReq))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate UDoubleRequirement.");
		return nullptr;
	}

	// InitDuplicate inside DoubleRequirement will duplicate children into this composite.
	DoubleReq->InitDoubleRequirement(First, Second);
	return DoubleReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateDouble_Unit_Unit(
	UObject* Outer,
	const FTrainingOption& RequiredUnitA,
	const FTrainingOption& RequiredUnitB)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UDoubleRequirement* DoubleReq = NewObject<UDoubleRequirement>(UseOuter);
	if (not IsValid(DoubleReq))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate UDoubleRequirement (Unit+Unit).");
		return nullptr;
	}

	// Children owned by the composite to ensure snapshot duplication clones the full graph.
	UUnitRequirement* A = NewObject<UUnitRequirement>(DoubleReq);
	UUnitRequirement* B = NewObject<UUnitRequirement>(DoubleReq);
	if (not IsValid(A) || not IsValid(B))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate child UnitRequirement(s) (Unit+Unit).");
		return nullptr;
	}

	A->InitUnitRequirement(RequiredUnitA);
	B->InitUnitRequirement(RequiredUnitB);

	DoubleReq->InitDoubleRequirement(A, B);
	return DoubleReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateDouble_Unit_Tech(
	UObject* Outer,
	const FTrainingOption& RequiredUnit,
	const ETechnology RequiredTech)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UDoubleRequirement* DoubleReq = NewObject<UDoubleRequirement>(UseOuter);
	if (not IsValid(DoubleReq))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate UDoubleRequirement (Unit+Tech).");
		return nullptr;
	}

	UUnitRequirement* A = NewObject<UUnitRequirement>(DoubleReq);
	UTechRequirement* B = NewObject<UTechRequirement>(DoubleReq);
	if (not IsValid(A) || not IsValid(B))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate child requirements (Unit+Tech).");
		return nullptr;
	}

	A->InitUnitRequirement(RequiredUnit);
	B->InitTechRequirement(RequiredTech);

	DoubleReq->InitDoubleRequirement(A, B);
	return DoubleReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateDouble_Tech_Tech(
	UObject* Outer,
	const ETechnology RequiredTechA,
	const ETechnology RequiredTechB)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UDoubleRequirement* DoubleReq = NewObject<UDoubleRequirement>(UseOuter);
	if (not IsValid(DoubleReq))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate UDoubleRequirement (Tech+Tech).");
		return nullptr;
	}

	UTechRequirement* A = NewObject<UTechRequirement>(DoubleReq);
	UTechRequirement* B = NewObject<UTechRequirement>(DoubleReq);
	if (not IsValid(A) || not IsValid(B))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate child TechRequirement(s) (Tech+Tech).");
		return nullptr;
	}

	A->InitTechRequirement(RequiredTechA);
	B->InitTechRequirement(RequiredTechB);

	DoubleReq->InitDoubleRequirement(A, B);
	return DoubleReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateDouble_Unit_VacantPad(
	UObject* Outer,
	const FTrainingOption& RequiredUnit)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UDoubleRequirement* DoubleReq = NewObject<UDoubleRequirement>(UseOuter);
	if (not IsValid(DoubleReq))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate UDoubleRequirement (Unit+VacantPad).");
		return nullptr;
	}

	UUnitRequirement* A = NewObject<UUnitRequirement>(DoubleReq);
	UVacantAirPadRequirement* B = NewObject<UVacantAirPadRequirement>(DoubleReq);
	if (not IsValid(A) || not IsValid(B))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate child requirements (Unit+VacantPad).");
		return nullptr;
	}

	A->InitUnitRequirement(RequiredUnit);
	B->InitVacantAirPadRequirement();

	DoubleReq->InitDoubleRequirement(A, B);
	return DoubleReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateDouble_Tech_VacantPad(
	UObject* Outer,
	const ETechnology RequiredTech)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UDoubleRequirement* DoubleReq = NewObject<UDoubleRequirement>(UseOuter);
	if (not IsValid(DoubleReq))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate UDoubleRequirement (Tech+VacantPad).");
		return nullptr;
	}

	UTechRequirement* A = NewObject<UTechRequirement>(DoubleReq);
	UVacantAirPadRequirement* B = NewObject<UVacantAirPadRequirement>(DoubleReq);
	if (not IsValid(A) || not IsValid(B))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate child requirements (Tech+VacantPad).");
		return nullptr;
	}

	A->InitTechRequirement(RequiredTech);
	B->InitVacantAirPadRequirement();

	DoubleReq->InitDoubleRequirement(A, B);
	return DoubleReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateDouble_Expansion_Expansion(
	UObject* Outer,
	const FTrainingOption& RequiredExpansionA,
	const FTrainingOption& RequiredExpansionB)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UDoubleRequirement* DoubleReq = NewObject<UDoubleRequirement>(UseOuter);
	if (not IsValid(DoubleReq))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate UDoubleRequirement (Expansion+Expansion).");
		return nullptr;
	}

	UExpansionRequirement* A = NewObject<UExpansionRequirement>(DoubleReq);
	UExpansionRequirement* B = NewObject<UExpansionRequirement>(DoubleReq);
	if (not IsValid(A) || not IsValid(B))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate child ExpansionRequirement(s) (Expansion+Expansion).");
		return nullptr;
	}

	A->InitExpansionRequirement(RequiredExpansionA);
	B->InitExpansionRequirement(RequiredExpansionB);

	DoubleReq->InitDoubleRequirement(A, B);
	return DoubleReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateDouble_Unit_Expansion(
	UObject* Outer,
	const FTrainingOption& RequiredUnit,
	const FTrainingOption& RequiredExpansion)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UDoubleRequirement* DoubleReq = NewObject<UDoubleRequirement>(UseOuter);
	if (not IsValid(DoubleReq))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate UDoubleRequirement (Unit+Expansion).");
		return nullptr;
	}

	UUnitRequirement* A = NewObject<UUnitRequirement>(DoubleReq);
	UExpansionRequirement* B = NewObject<UExpansionRequirement>(DoubleReq);
	if (not IsValid(A) || not IsValid(B))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate child requirements (Unit+Expansion).");
		return nullptr;
	}

	A->InitUnitRequirement(RequiredUnit);
	B->InitExpansionRequirement(RequiredExpansion);

	DoubleReq->InitDoubleRequirement(A, B);
	return DoubleReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateDouble_Tech_Expansion(
	UObject* Outer,
	const ETechnology RequiredTech,
	const FTrainingOption& RequiredExpansion)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UDoubleRequirement* DoubleReq = NewObject<UDoubleRequirement>(UseOuter);
	if (not IsValid(DoubleReq))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate UDoubleRequirement (Tech+Expansion).");
		return nullptr;
	}

	UTechRequirement* A = NewObject<UTechRequirement>(DoubleReq);
	UExpansionRequirement* B = NewObject<UExpansionRequirement>(DoubleReq);
	if (not IsValid(A) || not IsValid(B))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate child requirements (Tech+Expansion).");
		return nullptr;
	}

	A->InitTechRequirement(RequiredTech);
	B->InitExpansionRequirement(RequiredExpansion);

	DoubleReq->InitDoubleRequirement(A, B);
	return DoubleReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateDouble_Expansion_VacantPad(
	UObject* Outer,
	const FTrainingOption& RequiredExpansion)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UDoubleRequirement* DoubleReq = NewObject<UDoubleRequirement>(UseOuter);
	if (not IsValid(DoubleReq))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate UDoubleRequirement (Expansion+VacantPad).");
		return nullptr;
	}

	UExpansionRequirement* A = NewObject<UExpansionRequirement>(DoubleReq);
	UVacantAirPadRequirement* B = NewObject<UVacantAirPadRequirement>(DoubleReq);
	if (not IsValid(A) || not IsValid(B))
	{
		RTSFunctionLibrary::ReportError("Failed to allocate child requirements (Expansion+VacantPad).");
		return nullptr;
	}

	A->InitExpansionRequirement(RequiredExpansion);
	B->InitVacantAirPadRequirement();

	DoubleReq->InitDoubleRequirement(A, B);
	return DoubleReq;
}

// ----------------------------- OR (ANY) --------------------------------------

URTSRequirement* FRTS_RequirementHelpers::CreateOrRequirement(
	UObject* Outer,
	URTSRequirement* First,
	URTSRequirement* Second)
{
	if (!IsValid(First) || !IsValid(Second))
	{
		RTSFunctionLibrary::ReportError(TEXT("CreateOrRequirement called with invalid child requirement(s)."));
		return nullptr;
	}

	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UOrDoubleRequirement* OrReq = NewObject<UOrDoubleRequirement>(UseOuter);
	if (!IsValid(OrReq))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate UOrDoubleRequirement."));
		return nullptr;
	}

	OrReq->InitOrRequirement(First, Second);
	return OrReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateOr_Unit_Unit(
	UObject* Outer,
	const FTrainingOption& RequiredUnitA,
	const FTrainingOption& RequiredUnitB)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UOrDoubleRequirement* OrReq = NewObject<UOrDoubleRequirement>(UseOuter);
	if (!IsValid(OrReq))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate UOrDoubleRequirement (Unit OR Unit)."));
		return nullptr;
	}

	// Own children by making them sub-objects of the composite.
	UUnitRequirement* A = NewObject<UUnitRequirement>(OrReq);
	UUnitRequirement* B = NewObject<UUnitRequirement>(OrReq);
	if (!IsValid(A) || !IsValid(B))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate child UnitRequirement(s) (Unit OR Unit)."));
		return nullptr;
	}

	A->InitUnitRequirement(RequiredUnitA);
	B->InitUnitRequirement(RequiredUnitB);

	OrReq->InitOrRequirement(A, B);
	return OrReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateOr_Unit_Tech(
	UObject* Outer,
	const FTrainingOption& RequiredUnit,
	const ETechnology RequiredTech)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UOrDoubleRequirement* OrReq = NewObject<UOrDoubleRequirement>(UseOuter);
	if (!IsValid(OrReq))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate UOrDoubleRequirement (Unit OR Tech)."));
		return nullptr;
	}

	UUnitRequirement* A = NewObject<UUnitRequirement>(OrReq);
	UTechRequirement* B = NewObject<UTechRequirement>(OrReq);
	if (!IsValid(A) || !IsValid(B))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate child requirements (Unit OR Tech)."));
		return nullptr;
	}

	A->InitUnitRequirement(RequiredUnit);
	B->InitTechRequirement(RequiredTech);

	OrReq->InitOrRequirement(A, B);
	return OrReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateOr_Tech_Tech(
	UObject* Outer,
	const ETechnology RequiredTechA,
	const ETechnology RequiredTechB)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UOrDoubleRequirement* OrReq = NewObject<UOrDoubleRequirement>(UseOuter);
	if (!IsValid(OrReq))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate UOrDoubleRequirement (Tech OR Tech)."));
		return nullptr;
	}

	UTechRequirement* A = NewObject<UTechRequirement>(OrReq);
	UTechRequirement* B = NewObject<UTechRequirement>(OrReq);
	if (!IsValid(A) || !IsValid(B))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate child TechRequirement(s) (Tech OR Tech)."));
		return nullptr;
	}

	A->InitTechRequirement(RequiredTechA);
	B->InitTechRequirement(RequiredTechB);

	OrReq->InitOrRequirement(A, B);
	return OrReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateOr_Unit_VacantPad(
	UObject* Outer,
	const FTrainingOption& RequiredUnit)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UOrDoubleRequirement* OrReq = NewObject<UOrDoubleRequirement>(UseOuter);
	if (!IsValid(OrReq))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate UOrDoubleRequirement (Unit OR VacantPad)."));
		return nullptr;
	}

	UUnitRequirement* A = NewObject<UUnitRequirement>(OrReq);
	UVacantAirPadRequirement* B = NewObject<UVacantAirPadRequirement>(OrReq);
	if (!IsValid(A) || !IsValid(B))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate child requirements (Unit OR VacantPad)."));
		return nullptr;
	}

	A->InitUnitRequirement(RequiredUnit);
	B->InitVacantAirPadRequirement();

	OrReq->InitOrRequirement(A, B);
	return OrReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateOr_Tech_VacantPad(
	UObject* Outer,
	const ETechnology RequiredTech)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UOrDoubleRequirement* OrReq = NewObject<UOrDoubleRequirement>(UseOuter);
	if (!IsValid(OrReq))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate UOrDoubleRequirement (Tech OR VacantPad)."));
		return nullptr;
	}

	UTechRequirement* A = NewObject<UTechRequirement>(OrReq);
	UVacantAirPadRequirement* B = NewObject<UVacantAirPadRequirement>(OrReq);
	if (!IsValid(A) || !IsValid(B))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate child requirements (Tech OR VacantPad)."));
		return nullptr;
	}

	A->InitTechRequirement(RequiredTech);
	B->InitVacantAirPadRequirement();

	OrReq->InitOrRequirement(A, B);
	return OrReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateOr_Expansion_Unit(

	UObject* Outer,
	const FTrainingOption& RequiredExpansion,
	const FTrainingOption& RequiredUnit)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UOrDoubleRequirement* OrReq = NewObject<UOrDoubleRequirement>(UseOuter);
	if (!IsValid(OrReq))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate UOrDoubleRequirement (Expansion OR Unit)."));
		return nullptr;
	}

	UExpansionRequirement* A = NewObject<UExpansionRequirement>(OrReq);
	UUnitRequirement* B = NewObject<UUnitRequirement>(OrReq);
	if (!IsValid(A) || !IsValid(B))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate child requirements (Expansion OR Unit)."));
		return nullptr;
	}

	A->InitExpansionRequirement(RequiredExpansion);
	B->InitUnitRequirement(RequiredUnit);

	OrReq->InitOrRequirement(A, B);
	return OrReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateOr_Expansion_Tech(
	UObject* Outer,
	const FTrainingOption& RequiredExpansion,
	const ETechnology RequiredTech)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UOrDoubleRequirement* OrReq = NewObject<UOrDoubleRequirement>(UseOuter);
	if (!IsValid(OrReq))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate UOrDoubleRequirement (Expansion OR Tech)."));
		return nullptr;
	}

	UExpansionRequirement* A = NewObject<UExpansionRequirement>(OrReq);
	UTechRequirement* B = NewObject<UTechRequirement>(OrReq);
	if (!IsValid(A) || !IsValid(B))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate child requirements (Expansion OR Tech)."));
		return nullptr;
	}

	A->InitExpansionRequirement(RequiredExpansion);
	B->InitTechRequirement(RequiredTech);

	OrReq->InitOrRequirement(A, B);
	return OrReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateOr_Expansion_VacantPad(
	UObject* Outer,
	const FTrainingOption& RequiredExpansion)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UOrDoubleRequirement* OrReq = NewObject<UOrDoubleRequirement>(UseOuter);
	if (!IsValid(OrReq))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate UOrDoubleRequirement (Expansion OR VacantPad)."));
		return nullptr;
	}

	UExpansionRequirement* A = NewObject<UExpansionRequirement>(OrReq);
	UVacantAirPadRequirement* B = NewObject<UVacantAirPadRequirement>(OrReq);
	if (!IsValid(A) || !IsValid(B))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate child requirements (Expansion OR VacantPad)."));
		return nullptr;
	}

	A->InitExpansionRequirement(RequiredExpansion);
	B->InitVacantAirPadRequirement();

	OrReq->InitOrRequirement(A, B);
	return OrReq;
}

URTSRequirement* FRTS_RequirementHelpers::CreateOr_Expansion_Expansion(
	UObject* Outer,
	const FTrainingOption& RequiredExpansionA,
	const FTrainingOption& RequiredExpansionB)
{
	UObject* const UseOuter = IsValid(Outer) ? Outer : GetTransientPackage();

	UOrDoubleRequirement* OrReq = NewObject<UOrDoubleRequirement>(UseOuter);
	if (!IsValid(OrReq))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to allocate UOrDoubleRequirement (Expansion OR Expansion)."));
		return nullptr;
	}

	UExpansionRequirement* A = NewObject<UExpansionRequirement>(OrReq);
	UExpansionRequirement* B = NewObject<UExpansionRequirement>(OrReq);
	if (!IsValid(A) || !IsValid(B))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Failed to allocate child ExpansionRequirement(s) (Expansion OR Expansion)."));
		return nullptr;
	}

	A->InitExpansionRequirement(RequiredExpansionA);
	B->InitExpansionRequirement(RequiredExpansionB);

	OrReq->InitOrRequirement(A, B);
	return OrReq;
}
