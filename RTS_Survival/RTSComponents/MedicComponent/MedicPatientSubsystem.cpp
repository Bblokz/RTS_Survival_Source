// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MedicPatientSubsystem.h"

#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

void UMedicPatientSubsystem::ClaimPatient(const ASquadUnit* Patient)
{
	if (not IsValid(Patient))
	{
		return;
	}
	RemoveStalePatients();
	++M_MedicCountPerPatient.FindOrAdd(Patient);
}

void UMedicPatientSubsystem::ReleasePatient(const ASquadUnit* Patient)
{
	// Not IsValid: a medic also lets go of a patient that is being destroyed.
	if (Patient == nullptr)
	{
		return;
	}
	const TWeakObjectPtr<const ASquadUnit> PatientKey(Patient);
	int32* MedicCount = M_MedicCountPerPatient.Find(PatientKey);
	if (MedicCount == nullptr)
	{
		return;
	}
	--(*MedicCount);
	if (*MedicCount <= 0)
	{
		M_MedicCountPerPatient.Remove(PatientKey);
	}
}

int32 UMedicPatientSubsystem::GetMedicCountForPatient(const ASquadUnit* Patient) const
{
	const int32* MedicCount = M_MedicCountPerPatient.Find(Patient);
	return MedicCount != nullptr ? *MedicCount : 0;
}

void UMedicPatientSubsystem::RemoveStalePatients()
{
	for (auto PatientIterator = M_MedicCountPerPatient.CreateIterator(); PatientIterator; ++PatientIterator)
	{
		if (not PatientIterator.Key().IsValid())
		{
			PatientIterator.RemoveCurrent();
		}
	}
}
