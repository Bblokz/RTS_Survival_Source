// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_GlobalAbilityPanel.h"

void UW_GlobalAbilityPanel::InitWithLoadedAbilities(TArray<TObjectPtr<UGlobalAbility>> LoadedAbilities)
{
	// todo setup _1, 2 ,3 ,4 and 5 with abilities if less are loaded collapse those that have no ability.
	// if more are loaded report error.
}

void UW_GlobalAbilityPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
}
