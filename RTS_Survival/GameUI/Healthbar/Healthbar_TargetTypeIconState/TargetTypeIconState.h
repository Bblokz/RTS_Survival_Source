#pragma once

#include "CoreMinimal.h"

#include "TargetTypeIconState.generated.h"

class UTexture2D;
enum class ETargetTypeIcon : uint8;
class UImage;

USTRUCT(BlueprintType)
struct FTargetTypeIconBrushes
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTexture2D> PlayerBrush = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTexture2D> EnemyBrush = nullptr;
};

USTRUCT(Blueprintable)
struct FTargetTypeIconState
{
	GENERATED_BODY()

	bool GetImplementsTargetTypeIcon() const;

	void SetNewTargetTypeIcon(const int8 OwningPlayer, const ETargetTypeIcon NewTargetTypeIcon, const AActor* ActorContext);

	UPROPERTY(Transient)
	TWeakObjectPtr<UImage> TargetTypeIconImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<ETargetTypeIcon, FTargetTypeIconBrushes> TypeToBrush;

private:
	bool GetIsValidTargetTypeIconImage() const;
	bool IsTypeContainedAndValid(const ETargetTypeIcon Type, const AActor* ActorContext);
};
