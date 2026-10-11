// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MedicPatientSubsystem.generated.h"

class ASquadUnit;

/**
 * @brief Lets medics see which soldiers another medic is already on its way to or healing.
 * A medic picks a soldier nobody tends to first, so several medics spread over several patients.
 */
UCLASS()
class RTS_SURVIVAL_API UMedicPatientSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void ClaimPatient(const ASquadUnit* Patient);
	void ReleasePatient(const ASquadUnit* Patient);
	int32 GetMedicCountForPatient(const ASquadUnit* Patient) const;

private:
	// Weak keys: a patient that dies while it is tended to leaves no dangling entry behind.
	TMap<TWeakObjectPtr<const ASquadUnit>, int32> M_MedicCountPerPatient;

	void RemoveStalePatients();
};
