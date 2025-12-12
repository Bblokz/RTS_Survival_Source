
#include "ExperienceInterface.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/ExperienceComponent/ExperienceComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

int32 IExperienceInterface::GetExperienceWorth() const
{
    if(not GetIsValidExperienceComponent())
    {
        return 0.0f;
    }
    const auto ExperienceComponent = GetExperienceComponent();
    return ExperienceComponent->GetExperienceWorth();
}


void IExperienceInterface::IExpOnKilledActor(AActor* KilledActor) const
{
    if(not GetIsValidExperienceComponent())
    {
        return;
    }
    const auto ExperienceComponent = GetExperienceComponent();
    if(not KilledActor)
    {
        Debug_Experience("Killed actor is null cannot calculate experience!");
        return;
    }
    const auto EnemyExperience = Cast<IExperienceProvider>(KilledActor);
    if(not EnemyExperience)
    {
        Debug_Experience("Killed actor does not implement IExperienceProvider interface! actor: " + IsValid(KilledActor) ? KilledActor->GetName() : "Null");
        return;
    }
    const float EnemyXp = EnemyExperience->GetExperienceWorth();
    Debug_Experience("Amount of experience from kill:" + FString::SanitizeFloat(EnemyXp));
    // Will call level up on this owner if needed.
    ExperienceComponent->ReceiveExperience(EnemyXp);
}

bool IExperienceInterface::GetIsValidExperienceComponent() const
{
    if(GetExperienceComponent() != nullptr)
    {
        return true;
    }
    RTSFunctionLibrary::ReportError("Experience component is NULL for experience interface! "
    "\n See function IExperienceInterface::GetIsValidExperienceComponent");
    return false;
}

void IExperienceInterface::Debug_Experience(const FString& Message) const
{
    if(DeveloperSettings::Debugging::GExperienceSystem_Compile_DebugSymbols)
    {
        RTSFunctionLibrary::PrintString(Message);
    }
}
