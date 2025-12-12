#pragma once


#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/RichTextBlock.h"
#include "RTS_Survival/GameUI/Resources/WPlayerResource/W_PlayerResource.h"

#include "W_TurnCounter.generated.h"

UCLASS()
class RTS_SURVIVAL_API UW_TurnCounter : public UUserWidget
{
	GENERATED_BODY()


protected:
	void InitTurnCounter(const TArray<ERTSResourceType>& ResourceTypes, const int32 TurnCount);
	
	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<URichTextBlock> M_TurnCounter;

	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<UW_PlayerResource> BP_W_PlayerResource1;
	
	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<UW_PlayerResource> BP_W_PlayerResource2;
	
	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<UW_PlayerResource> BP_W_PlayerResource3;
	
	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<UW_PlayerResource> BP_W_PlayerResource4;
	
	UPROPERTY(meta=(BindWidget), BlueprintReadOnly)
	TObjectPtr<UW_PlayerResource> BP_W_PlayerResource5;

private:
	static bool EnsureResourceIsValid(const TObjectPtr<UW_PlayerResource>& ResourceToCheck);
	UPlayerResourceManager* GetPlayerResourceManager() const;
	void InitResourceWidgets(TArray<ERTSResourceType> ResourceTypes);
	void SetTurnCounterText(const int32 TurnCount) const;
};