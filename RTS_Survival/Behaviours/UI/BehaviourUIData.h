// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "RTS_Survival/Behaviours/Icons/BehaviourIcon.h"

#include "BehaviourUIData.generated.h"

USTRUCT()
struct FBehaviourUIData
{
	GENERATED_BODY()

	UPROPERTY()
	EBehaviourIcon BehaviourIcon = EBehaviourIcon::None;

	UPROPERTY()
	FString TitleText;

	UPROPERTY()
	FString DescriptionText;
	
	UPROPERTY()
	EBehaviourLifeTime LifeTimeType = EBehaviourLifeTime::None;

	UPROPERTY()
	float TotalLifeTime = 0.f;
	
	UPROPERTY()
	EBuffDebuffType BuffDebuffType = EBuffDebuffType::Neutral;
};
