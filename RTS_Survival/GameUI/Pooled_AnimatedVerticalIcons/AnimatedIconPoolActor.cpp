#include "AnimatedIconPoolActor.h"

#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

AAnimatedIconPoolActor::AAnimatedIconPoolActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	SetActorEnableCollision(false);
	M_PoolRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PoolRoot"));
	SetRootComponent(M_PoolRoot);
}

UWidgetComponent* AAnimatedIconPoolActor::CreateIconComponent(UClass* WidgetClass)
{
	if (not GetIsValidPoolRoot() || not IsValid(WidgetClass))
	{
		return nullptr;
	}

	UWidgetComponent* IconComponent = NewObject<UWidgetComponent>(this, NAME_None, RF_Transient);
	if (not IsValid(IconComponent))
	{
		RTSFunctionLibrary::ReportError(TEXT("Animated Icons: failed to create a widget component."));
		return nullptr;
	}

	M_IconComponents.Add(IconComponent);
	AddInstanceComponent(IconComponent);
	IconComponent->SetWidgetSpace(EWidgetSpace::Screen);
	IconComponent->SetTickMode(ETickMode::Automatic);
	IconComponent->SetDrawAtDesiredSize(true);
	IconComponent->SetPivot(FVector2D(0.5, 0.5));
	IconComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IconComponent->SetGenerateOverlapEvents(false);
	IconComponent->SetVisibility(false);
	IconComponent->SetHiddenInGame(true);
	IconComponent->SetWidgetClass(WidgetClass);
	IconComponent->SetupAttachment(M_PoolRoot);
	IconComponent->RegisterComponent();
	IconComponent->InitWidget();
	return IconComponent;
}

bool AAnimatedIconPoolActor::GetIsValidPoolRoot() const
{
	if (IsValid(M_PoolRoot))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_PoolRoot"),
		TEXT("GetIsValidPoolRoot"), this);
	return false;
}
