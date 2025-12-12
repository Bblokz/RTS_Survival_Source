#pragma once

#include "CoreMinimal.h"

class AScavengeableObject;
class AItemsMaster;
class ACPPResourceMaster;
class ABuildingExpansion;
class AActor;
class ACharacterObjectsMaster;

union FTargetUnion
{
	ACharacterObjectsMaster* TargetCharacter;
	AActor* TargetActor;
	ABuildingExpansion* TargetBxp;
	ACPPResourceMaster* TargetResource;
	AItemsMaster* PickupItem;
	AScavengeableObject* ScavengeObject;
};
