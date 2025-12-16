#include "BehaviourButtonSettings.h"
#include "RTS_Survival/Behaviours/Icons/BehaviourIcon.h"

UBehaviourButtonSettings::UBehaviourButtonSettings()
{
        CategoryName = TEXT("UI");
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
