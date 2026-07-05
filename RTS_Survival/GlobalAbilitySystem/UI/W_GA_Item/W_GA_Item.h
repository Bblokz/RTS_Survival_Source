// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "W_GA_Item.generated.h"

class UW_CoolDownItem;
class UGlobalAbilitiesManager;
class UW_HotKey;
class UGlobalAbility;

/**
 * @brief Item widget used by the global ability panel to route ability button interactions.
 */
UCLASS()
class RTS_SURVIVAL_API UW_GA_Item : public UUserWidget
{
	GENERATED_BODY()
	
	public:
	void SetupGa_Item(TWeakObjectPtr<UGlobalAbility> GlobalAbility, 
		UGlobalAbilitiesManager* GlobalAbilityManager);
	
	void OnAbilityHovered(UGlobalAbility* HoveredAbility, const bool bIsHover);
	void SetAbilityAvailable(const bool bIsGaEnabled, const bool bUseGreyTint) const;
	void RefreshCooldownFromLoadedAbility() const;
	UGlobalAbility* GetLoadedAbility() const { return M_GlobalAbility.Get(); }
	
	protected:
	virtual void NativeDestruct() override;
	
	UPROPERTY( BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_CoolDownItem> AbilityButton;
	
	UPROPERTY( BlueprintReadOnly, meta=(BindWidget))
	TObjectPtr<UW_HotKey> HotkeyWidget;

	UFUNCTION( BlueprintCallable, NotBlueprintable)
	void OnClickedAbilityButton();
	
	UFUNCTION( BlueprintCallable, NotBlueprintable)
	void OnHoveredAbilityButton(const bool bIsHover);

	UFUNCTION()
	void HandleAbilityButtonHovered();

	UFUNCTION()
	void HandleAbilityButtonUnhovered();
	
private:
	UPROPERTY()
	TWeakObjectPtr<UGlobalAbility> M_GlobalAbility;
	[[nodiscard]] bool EnsureIsValidAbility() const;
	
	UPROPERTY()
	TWeakObjectPtr<UGlobalAbilitiesManager> M_GlobalAbilityManager;
	[[nodiscard]] bool EnsureIsValidAbilityManager();

	FButtonStyle M_OriginalButtonStyle;
	bool bM_HasOriginalButtonStyle = false;

	void SetupAbilityButton();
	void BindAbilityButtonCallbacks();
	void UnbindAbilityButtonCallbacks();
	UButton* GetCooldownButton() const;
	[[nodiscard]] bool EnsureIsValidAbilityButton() const;
};
