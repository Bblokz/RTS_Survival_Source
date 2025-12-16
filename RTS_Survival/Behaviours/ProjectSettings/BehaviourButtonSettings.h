#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/Texture2D.h"
#include "BehaviourButtonSettings.generated.h"

class UTexture2D;

enum class EBehaviourIcon : uint8;

USTRUCT(BlueprintType)
struct FBehaviourWidgetStyle
{
        GENERATED_BODY()

        /** Behaviour icon texture. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour Button")
        UTexture2D* IconTexture = nullptr;
};

USTRUCT(BlueprintType)
struct FBehaviourWidgetStyleSoft
{
        GENERATED_BODY()

        /** Behaviour icon texture as soft reference for configuration. */
        UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Behaviour Button")
        TSoftObjectPtr<UTexture2D> IconTexture;
};

/**
 * @brief Project settings mapping behaviour icons to widget styles.
 * Appears under Project Settings as: UI ► Behaviour Buttons (configurable per project).
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Behaviour Buttons"))
class RTS_SURVIVAL_API UBehaviourButtonSettings : public UDeveloperSettings
{
        GENERATED_BODY()

public:
        UBehaviourButtonSettings();

        /** Configure behaviour button icon styles globally for the project. */
        UPROPERTY(EditAnywhere, Config, Category="Behaviour Button Styles", meta=(ForceInlineRow))
        TMap<EBehaviourIcon, FBehaviourWidgetStyleSoft> BehaviourIconStyles;

        /** Convenience accessor. */
        static const UBehaviourButtonSettings* Get()
        {
                return GetDefault<UBehaviourButtonSettings>();
        }

        /**
         * Resolve soft references to hard pointers for runtime use.
         * Loads assets synchronously the first time they are requested.
         */
        void ResolveBehaviourIconStyles(TMap<EBehaviourIcon, FBehaviourWidgetStyle>& OutIconStyles) const;
};
