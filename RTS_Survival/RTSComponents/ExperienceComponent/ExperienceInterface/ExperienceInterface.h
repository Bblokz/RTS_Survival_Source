#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ExperienceInterface.generated.h"

class URTSExperienceComp;
class FString;

// For those units that do not have an experience component tracking  their experience but when killed do provide experience.
UINTERFACE(MinimalAPI)
class UExperienceProvider : public UInterface
{
    GENERATED_BODY()
};

class RTS_SURVIVAL_API IExperienceProvider
{
    GENERATED_BODY()

public:
    virtual int32 GetExperienceWorth() const = 0;
};

// For units that both provide exp upon death and keep track of their own experience levels with an experience component.
UINTERFACE(MinimalAPI)
class UExperienceInterface : public UExperienceProvider
{
    GENERATED_BODY()
};

class RTS_SURVIVAL_API IExperienceInterface : public IExperienceProvider
{
    GENERATED_BODY()

    friend class URTSExperienceComp;

public:
    virtual int32 GetExperienceWorth() const override;

protected:
    void IExpOnKilledActor(AActor* KilledActor) const;
    
    virtual URTSExperienceComp* GetExperienceComponent() const = 0;
    virtual void OnUnitLevelUp() = 0;

private:
    bool GetIsValidExperienceComponent() const;
    void Debug_Experience(const FString& Message) const;
};
