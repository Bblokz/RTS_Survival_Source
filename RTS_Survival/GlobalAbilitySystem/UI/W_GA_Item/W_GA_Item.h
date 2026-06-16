// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "W_GA_Item.generated.h"

class UGlobalAbilitiesManager;
class UW_HotKey;
class UGlobalAbility;
/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UW_GA_Item : public UUserWidget
{
	GENERATED_BODY()
	
	public:
	void SetupGa_Item(TWeakObjectPtr<UGlobalAbility> GlobalAbility, 
		UGlobalAbilitiesManager* GlobalAbilityManager);
	
	void OnAbilityHovered(UGlobalAbility* HoveredAbility, const bool bIsHover);
	void SetAbilityAvailable(const bool bIsEnabled, const bool bUseGreyTint);
	UGlobalAbility* GetLoadedAbility() const { return M_GlobalAbility.Get(); }
	
	protected:
	
	UPROPERTY( BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UButton> AbilityButton;
	
	UPROPERTY( BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_HotKey> HotkeyWidget;

	UFUNCTION( BlueprintCallable, NotBlueprintable)
	void OnClickedAbilityButton();
	
	UFUNCTION( BlueprintCallable, NotBlueprintable)
	void OnHoveredAbilityButton(const bool bIsHover);
	
private:
	UPROPERTY()
	TWeakObjectPtr<UGlobalAbility> M_GlobalAbility;
	[[nodiscard]] bool EnsureIsValidAbility();
	
	UPROPERTY()
	TWeakObjectPtr<UGlobalAbilitiesManager> M_GlobalAbilityManager;
	[[nodiscard]] bool EnsureIsValidAbilityManager();

	FButtonStyle M_OriginalButtonStyle;
	bool bM_HasOriginalButtonStyle = false;
};
