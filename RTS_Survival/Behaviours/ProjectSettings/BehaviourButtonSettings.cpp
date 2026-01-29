#include "BehaviourButtonSettings.h"
#include "RTS_Survival/Behaviours/Icons/BehaviourIcon.h"
#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

UBehaviourButtonSettings::UBehaviourButtonSettings()
{
        CategoryName = TEXT("Game");
        SectionName = TEXT("Behaviour Buttons");
}

void UBehaviourButtonSettings::ResolveBehaviourIconStyles(TMap<EBehaviourIcon, FBehaviourWidgetStyle>& OutIconStyles) const
{
        OutIconStyles.Reset();

        for (const TPair<EBehaviourIcon, FBehaviourWidgetStyleSoft>& BehaviourStylePair : BehaviourIconStyles)
        {
                FBehaviourWidgetStyle ResolvedStyle;
                ResolvedStyle.IconTexture = BehaviourStylePair.Value.IconTexture.IsNull()
                        ? nullptr
                        : BehaviourStylePair.Value.IconTexture.LoadSynchronous();

                OutIconStyles.Add(BehaviourStylePair.Key, MoveTemp(ResolvedStyle));
        }
}

const TMap<EBehaviourIcon, FBehaviourWidgetStyle>& UBehaviourButtonSettings::GetResolvedBehaviourIconStyles() const
{
        if (bM_HasResolvedBehaviourIconStyles)
        {
                return M_ResolvedBehaviourIconStyles;
        }

        ResolveBehaviourIconStyles(M_ResolvedBehaviourIconStyles);
        bM_HasResolvedBehaviourIconStyles = true;
        return M_ResolvedBehaviourIconStyles;
}

void UBehaviourButtonSettings::InvalidateResolvedBehaviourIconStyles() const
{
        bM_HasResolvedBehaviourIconStyles = false;
        M_ResolvedBehaviourIconStyles.Reset();
}

#if WITH_EDITOR
void UBehaviourButtonSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
        Super::PostEditChangeProperty(PropertyChangedEvent);
        InvalidateResolvedBehaviourIconStyles();
}
#endif
