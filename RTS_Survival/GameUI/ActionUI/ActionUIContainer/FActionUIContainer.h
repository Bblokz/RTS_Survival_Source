#pragma once

#include "CoreMinimal.h"


#include "FActionUIContainer.generated.h"

class UHorizontalBox;
class UBorder;

USTRUCT(Blueprintable)
struct FActionUIContainer
{
	GENERATED_BODY()

	void SetActionUIVisibility(const bool bVisible) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UBorder* M_ActionUIBorder = nullptr;

	// The action UI is setup in a way that the hz box is not the child of the border and so for visiblity both need to be
	// set to visible or collapsed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UHorizontalBox* M_ActionUIHorizontalBox= nullptr;

	bool EnsureActionUIIsValid() const;
};
