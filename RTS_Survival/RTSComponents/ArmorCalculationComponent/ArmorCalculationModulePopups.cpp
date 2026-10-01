// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ArmorCalculation.h"

#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextWidgetPoolManager/AnimatedTextWidgetPoolManager.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace ArmorCalculationModulePopups
{
	FString GetModuleDisplayName(const EVehicleModuleTypes Type)
	{
		switch (Type)
		{
		case EVehicleModuleTypes::AddOnArmor:
			return TEXT("Add-On Armor");
		case EVehicleModuleTypes::Tracks:
			return TEXT("Tracks");
		case EVehicleModuleTypes::Engine:
			return TEXT("Engine");
		case EVehicleModuleTypes::Ammo:
			return TEXT("Ammo");
		case EVehicleModuleTypes::Turret:
			return TEXT("Turret");
		case EVehicleModuleTypes::Weapon:
			return TEXT("Weapon");
		case EVehicleModuleTypes::Wheels:
			return TEXT("Wheels");
		default:
			return FString();
		}
	}

	void ShowModulePopup(const ATankMaster& Tank, const FString& Text, const ERTSRichText RichTextStyle)
	{
		UAnimatedTextWidgetPoolManager* PoolManager = FRTS_Statics::GetVerticalAnimatedTextWidgetPoolManager(&Tank);
		if (not IsValid(PoolManager))
		{
			return;
		}

		constexpr float PopupHeight = 250.f;
		constexpr float PopupDelta = 150.f;
		constexpr float PopupVisibleDuration = 2.33f;
		constexpr float PopupFadeOutDuration = 1.f;
		constexpr float PopupWrapWidth = 350.f;
		FRTSVerticalAnimTextSettings TextSettings;
		TextSettings.DeltaZ = PopupDelta;
		TextSettings.VisibleDuration = PopupVisibleDuration;
		TextSettings.FadeOutDuration = PopupFadeOutDuration;

		PoolManager->ShowAnimatedText(
			FRTSRichTextConverter::MakeRTSRich(Text, RichTextStyle),
			Tank.GetActorLocation() + FVector(0.f, 0.f, PopupHeight),
			false,
			PopupWrapWidth,
			ETextJustify::Type::Left,
			TextSettings
		);
	}
}

void UArmorCalculation::ShowModuleStateChangePopups(const FModuleChangeBatch& Changes) const
{
	if (not GetIsValidModuleTank())
	{
		return;
	}

	const ATankMaster& Tank = *M_ModuleTank.Get();
	const TConstArrayView<FModuleStateChange> ModuleChanges = Changes.GetChanges();
	for (const FModuleStateChange& Change : ModuleChanges)
	{
		if (Change.Cause != EModuleChangeCause::Damage)
		{
			continue;
		}
		if (Change.NewState == EVehicleModuleState::Destroyed
			&& Change.PreviousState != EVehicleModuleState::Destroyed)
		{
			ShowModuleDestroyedPopup(Tank, Change.Type);
		}
		else if (Change.NewState == EVehicleModuleState::Damaged
			&& Change.PreviousState == EVehicleModuleState::Healthy)
		{
			ShowModuleDamagedPopup(Tank, Change.Type);
		}
	}
}

void UArmorCalculation::ShowModuleDamagedPopup(const ATankMaster& Tank, const EVehicleModuleTypes Type) const
{
	const FString ModuleName = ArmorCalculationModulePopups::GetModuleDisplayName(Type);
	if (ModuleName.IsEmpty())
	{
		return;
	}

	ArmorCalculationModulePopups::ShowModulePopup(Tank, ModuleName + TEXT(" Damaged"), ERTSRichText::Text_Exp);
}

void UArmorCalculation::ShowModuleDestroyedPopup(const ATankMaster& Tank, const EVehicleModuleTypes Type) const
{
	const FString ModuleName = ArmorCalculationModulePopups::GetModuleDisplayName(Type);
	if (ModuleName.IsEmpty())
	{
		return;
	}

	ArmorCalculationModulePopups::ShowModulePopup(Tank, ModuleName + TEXT(" DESTROYED"), ERTSRichText::Text_Bad14);
}
