#include "AsyncUnitDetailedState.h"

FAsyncDetailedUnitState::FAsyncDetailedUnitState(): OwningPlayer(0), UnitType(), UnitSubtypeRaw(0),
                                                    CurrentActiveAbility(),
                                                    bIsInCombat(false),
                                                    HealthRatio(0),
                                                    UnitLocation()
{
}
