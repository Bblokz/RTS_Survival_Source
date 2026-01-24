#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/Texture2D.h"
#include "BehaviourButtonSettings.generated.h"

class UTexture2D;
struct FPropertyChangedEvent;

enum class EBehaviourIcon : uint8;

USTRUCT(BlueprintType)
struct FBehaviourWidgetStyle
{
        GENERATED_BODY()

        /** Behaviour icon texture. */
        UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Behaviour Button")
        TObjectPtr<UTexture2D> IconTexture = nullptr;
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

        /** Access resolved icon styles while keeping strong references for GC safety. */
        const TMap<EBehaviourIcon, FBehaviourWidgetStyle>& GetResolvedBehaviourIconStyles() const;

        /**
         * Resolve soft references to hard pointers for runtime use.
         * Loads assets synchronously the first time they are requested.
         */
        void ResolveBehaviourIconStyles(TMap<EBehaviourIcon, FBehaviourWidgetStyle>& OutIconStyles) const;

#if WITH_EDITOR
        virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
        // Resolved runtime styles cached on the settings object to prevent GC from invalidating pointers.
        UPROPERTY(Transient)
        mutable TMap<EBehaviourIcon, FBehaviourWidgetStyle> M_ResolvedBehaviourIconStyles;

        UPROPERTY(Transient)
        mutable bool bM_HasResolvedBehaviourIconStyles = false;

        void InvalidateResolvedBehaviourIconStyles() const;
};
