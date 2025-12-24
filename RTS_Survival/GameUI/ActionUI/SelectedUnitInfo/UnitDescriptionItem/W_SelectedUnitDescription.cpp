// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_SelectedUnitDescription.h"

#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "RTS_Survival/Game/GameState/UnitDataHelpers/UnitDataHelpers.h"
#include "RTS_Survival/GameUI/Healthbar/Healthbar_TargetTypeIconState/ProjectSettings/TargetTypeIconSettings.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/RTSComponents/ArmorComponent/Armor.h"
#include "RTS_Survival/UnitData/ArmorAndResistanceData.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

void UW_SelectedUnitDescription::SetupUnitDescription(const AActor* SelectedActor,
                                                      const EAllUnitType PrimaryUnitType,
                                                      const ENomadicSubtype NomadicSubtype,
                                                      const ETankSubtype TankSubtype,
                                                      const ESquadSubtype SquadSubtype, const EBuildingExpansionType BxpSubtype)
{
	if (not IsValid(SelectedActor))
	{
		return;
	}
	bool bValidResistanceData = false;
	FResistanceAndDamageReductionData ResistanceData = FUnitResistanceDataHelpers::GetUnitResistanceData(
		SelectedActor, bValidResistanceData);
	UArmorCalculation* ArmorCalculation = SelectedActor->FindComponentByClass<UArmorCalculation>();
	if (not IsValid(ArmorCalculation))
	{
		// Hide armor information.
		HullArmorBox->SetVisibility(ESlateVisibility::Collapsed);
		TurretArmorBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		HullArmorBox->SetVisibility(ESlateVisibility::Visible);
		TurretArmorBox->SetVisibility(ESlateVisibility::Visible);
		SetupHullArmor(ArmorCalculation);
		SetupTurretArmor(ArmorCalculation);
		SetupResistanceDescriptions(ResistanceData, SelectedActor);
	}
	OnSetupUnitDescription(PrimaryUnitType, NomadicSubtype, TankSubtype, SquadSubtype, ResistanceData, BxpSubtype);
}

void UW_SelectedUnitDescription::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Load target type icon from game settings.
	if (const UTargetTypeIconSettings* Settings = UTargetTypeIconSettings::Get())
	{
		TMap<ETargetTypeIcon, FTargetTypeIconBrushes> ResolvedMap;
		Settings->ResolveTypeToBrushMap(ResolvedMap);

		if (ResolvedMap.Num() > 0)
		{
			M_TargetTypeIconState.TypeToBrush = MoveTemp(ResolvedMap);
		}
	}
	if (TargetTypeIcon)
	{
		// Set the reference so we can change the icon using the struct.
		M_TargetTypeIconState.TargetTypeIconImage= TargetTypeIcon;
	}
}


FText UW_SelectedUnitDescription::FormatFloatToText(float Value, int32 DecimalPlaces) const
{
	// Check if the value has no fractional part
	if (FMath::FloorToFloat(Value) == Value)
	{
		DecimalPlaces = 0;
	}
	FNumberFormattingOptions NumberFormatOptions;
	NumberFormatOptions.MinimumFractionalDigits = DecimalPlaces;
	NumberFormatOptions.MaximumFractionalDigits = DecimalPlaces;

	// If DecimalPlaces is zero, we need to RTSRound the value to the nearest integer
	if (DecimalPlaces == 0)
	{
		Value = FMath::RoundToFloat(Value);
	}

	return FText::AsNumber(Value, &NumberFormatOptions);
}


void UW_SelectedUnitDescription::SetupHullArmor(UArmorCalculation* ArmorCalculation)
{
	if (!IsValid(HullArmorDescription))
	{
		UE_LOG(LogTemp, Warning, TEXT("HullArmorDescription widget is invalid, skipping hull armor setup."));
		return;
	}

	// 1) grab the raw setup
	const FArmorSetup& Setup = ArmorCalculation->GetArmorSetup();

	// 2) collect all non-zero plates from mesh 0 (the hull)
	TMap<EArmorPlate, TArray<float>> HullMap;
	constexpr int32 MaxPlates = DeveloperSettings::GameBalance::Weapons::MaxArmorPlatesPerMesh;
	for (int32 i = 0; i < MaxPlates; ++i)
	{
		// safety: FArmorSettings arrays in FArmorSetup are guaranteed size MaxPlates
		const FArmorSettings& S = Setup.ArmorSettings0[i];
		if (S.ArmorValue > 0.f)
		{
			HullMap.FindOrAdd(S.ArmorType).Add(S.ArmorValue);
		}
	}

	// 3) helper to format a float list as "Nmm" or "N-Mmm"
	auto BuildRangeString = [&](const TArray<float>& Vals)
	{
		if (Vals.Num() == 0) return FString();
		float MinV = TNumericLimits<float>::Max(), MaxV = TNumericLimits<float>::Lowest();
		for (float v : Vals)
		{
			MinV = FMath::Min(MinV, v);
			MaxV = FMath::Max(MaxV, v);
		}
		FText MinT = FormatFloatToText(MinV, 0);
		FText MaxT = FormatFloatToText(MaxV, 0);
		return (MinV == MaxV)
			       ? (MinT.ToString() + TEXT("mm"))
			       : (MinT.ToString() + TEXT("-") + MaxT.ToString() + TEXT("mm"));
	};

	// 4) build lines—only for plates we actually have
	FString Out;
	if (auto* Arr = HullMap.Find(EArmorPlate::Plate_Front))
	{
		Out += TEXT("Front: ")
			+ FRTSRichTextConverter::MakeRTSRich(BuildRangeString(*Arr), ERTSRichText::Text_Armor)
			+ TEXT("\n");
	}
	if (auto* Arr = HullMap.Find(EArmorPlate::Plate_FrontUpperGlacis))
	{
		Out += TEXT("Upper Glacis Plate: ")
			+ FRTSRichTextConverter::MakeRTSRich(BuildRangeString(*Arr), ERTSRichText::Text_Armor)
			+ TEXT("\n");
	}
	if (auto* Arr = HullMap.Find(EArmorPlate::Plate_FrontLowerGlacis))
	{
		Out += TEXT("Lower Glacis Plate: ")
			+ FRTSRichTextConverter::MakeRTSRich(BuildRangeString(*Arr), ERTSRichText::Text_Armor)
			+ TEXT("\n");
	}

	// Sides = left + right
	{
		TArray<float> SideVals;
		if (auto* L = HullMap.Find(EArmorPlate::Plate_SideLeft)) SideVals.Append(*L);
		if (auto* R = HullMap.Find(EArmorPlate::Plate_SideRight)) SideVals.Append(*R);
		if (SideVals.Num() > 0)
		{
			Out += TEXT("Sides: ")
				+ FRTSRichTextConverter::MakeRTSRich(BuildRangeString(SideVals), ERTSRichText::Text_Armor)
				+ TEXT("\n");
		}
	}

	// Lower sides
	{
		TArray<float> LowSideVals;
		if (auto* L = HullMap.Find(EArmorPlate::Plate_SideLowerLeft)) LowSideVals.Append(*L);
		if (auto* R = HullMap.Find(EArmorPlate::Plate_SideLowerRight)) LowSideVals.Append(*R);
		if (LowSideVals.Num() > 0)
		{
			Out += TEXT("Lower Sides: ")
				+ FRTSRichTextConverter::MakeRTSRich(BuildRangeString(LowSideVals), ERTSRichText::Text_Armor)
				+ TEXT("\n");
		}
	}

	if (auto* Arr = HullMap.Find(EArmorPlate::Plate_Rear))
	{
		Out += TEXT("Rear: ")
			+ FRTSRichTextConverter::MakeRTSRich(BuildRangeString(*Arr), ERTSRichText::Text_Armor);
	}

	// 5) commit
	HullArmorDescription->SetText(FText::FromString(Out));
}

void UW_SelectedUnitDescription::SetupTurretArmor(UArmorCalculation* ArmorCalculation)
{
	// 0) validate the widget
	if (!IsValid(TurretArmorDescription))
	{
		UE_LOG(LogTemp, Warning, TEXT("TurretArmorDescription widget is invalid, skipping turret armor setup."));
		return;
	}

	const FArmorSetup& Setup = ArmorCalculation->GetArmorSetup();
	TMap<EArmorPlate, TArray<float>> TurretMap;
	constexpr int32 MaxPlates = DeveloperSettings::GameBalance::Weapons::MaxArmorPlatesPerMesh;

	for (int32 i = 0; i < MaxPlates; ++i)
	{
		const FArmorSettings& S = Setup.ArmorSettings1[i];
		if (S.ArmorValue > 0.f)
		{
			TurretMap.FindOrAdd(S.ArmorType).Add(S.ArmorValue);
		}
	}

	auto BuildRangeString = [&](const TArray<float>& Vals)
	{
		if (Vals.Num() == 0) return FString();
		float MinV = TNumericLimits<float>::Max(), MaxV = TNumericLimits<float>::Lowest();
		for (float v : Vals)
		{
			MinV = FMath::Min(MinV, v);
			MaxV = FMath::Max(MaxV, v);
		}
		FText MinT = FormatFloatToText(MinV, 0);
		FText MaxT = FormatFloatToText(MaxV, 0);
		return (MinV == MaxV)
			       ? (MinT.ToString() + TEXT("mm"))
			       : (MinT.ToString() + TEXT("-") + MaxT.ToString() + TEXT("mm"));
	};

	FString Out;
	if (auto* Arr = TurretMap.Find(EArmorPlate::Turret_Mantlet))
	{
		Out += TEXT("Gun Mantlet Plate: ")
			+ FRTSRichTextConverter::MakeRTSRich(BuildRangeString(*Arr), ERTSRichText::Text_Armor)
			+ TEXT("\n");
	}
	if (auto* Arr = TurretMap.Find(EArmorPlate::Turret_Front))
	{
		Out += TEXT("Turret Front: ")
			+ FRTSRichTextConverter::MakeRTSRich(BuildRangeString(*Arr), ERTSRichText::Text_Armor)
			+ TEXT("\n");
	}

	// Sides or fallback to Turret_SidesAndRear
	{
		TArray<float> SideVals;
		if (auto* L = TurretMap.Find(EArmorPlate::Turret_SideLeft)) SideVals.Append(*L);
		if (auto* R = TurretMap.Find(EArmorPlate::Turret_SideRight)) SideVals.Append(*R);
		if (SideVals.Num() == 0 && TurretMap.Contains(EArmorPlate::Turret_SidesAndRear))
		{
			SideVals = TurretMap[EArmorPlate::Turret_SidesAndRear];
		}
		if (SideVals.Num() > 0)
		{
			Out += TEXT("Sides: ")
				+ FRTSRichTextConverter::MakeRTSRich(BuildRangeString(SideVals), ERTSRichText::Text_Armor)
				+ TEXT("\n");
		}
	}

	// Rear or fallback
	{
		TArray<float> RearVals;
		if (auto* R = TurretMap.Find(EArmorPlate::Turret_Rear)) RearVals.Append(*R);
		if (RearVals.Num() == 0 && TurretMap.Contains(EArmorPlate::Turret_SidesAndRear))
		{
			RearVals = TurretMap[EArmorPlate::Turret_SidesAndRear];
		}
		if (RearVals.Num() > 0)
		{
			Out += TEXT("Turret Rear: ")
				+ FRTSRichTextConverter::MakeRTSRich(BuildRangeString(RearVals), ERTSRichText::Text_Armor)
				+ TEXT("\n");
		}
	}

	if (auto* Arr = TurretMap.Find(EArmorPlate::Turret_Cupola))
	{
		Out += TEXT("Cupola: ")
			+ FRTSRichTextConverter::MakeRTSRich(BuildRangeString(*Arr), ERTSRichText::Text_Armor);
	}

	TurretArmorDescription->SetText(FText::FromString(Out));
}

void UW_SelectedUnitDescription::SetupResistanceDescriptions(const FResistanceAndDamageReductionData& ResistanceData, const AActor* SelectedActor)
{
	SetupArmorIcon(ResistanceData.TargetTypeIcon, SelectedActor);
	SetupLaserResistance(ResistanceData.LaserAndRadiationDamageMlt.LaserMltPerPart);
	SetupRadiationResistance(ResistanceData.LaserAndRadiationDamageMlt.RadiationMltPerPart);
	SetupFireResistance(ResistanceData.FireResistanceData);
}

void UW_SelectedUnitDescription::SetupArmorIcon(const ETargetTypeIcon NewTargetTypeIcon, const AActor* SelectedActor)
{
	if (not M_TargetTypeIconState.GetImplementsTargetTypeIcon())
	{
		RTSFunctionLibrary::ReportError("The target type icon image is not set in Struct"
			"\n See FTargetTypeIconState M_TargetTypeIconState in :" + GetName());
		return;
	}
	M_TargetTypeIconState.SetNewTargetTypeIcon(1, NewTargetTypeIcon, SelectedActor);
	const FString ArmorName = GetTargetTypeDisplayName(NewTargetTypeIcon);
	if (not ArmorName.IsEmpty() && IsValid(ArmorTypeText))
	{
		FString RichDescription = RTSRichtText(ArmorName, ERTSRichText::Text_Armor);
		ArmorTypeText->SetText(FText::FromString(RichDescription));
	}
}

void UW_SelectedUnitDescription::SetupLaserResistance(const FDamageMltPerSide& LaserMltPerPart) const
{
	if (not LaserResistanceDescription)
	{
		return;
	}
	const FString Description = GetMltPerPartDescription(LaserMltPerPart);
	LaserResistanceDescription->SetText(FText::FromString(Description));
}

void UW_SelectedUnitDescription::SetupRadiationResistance(const FDamageMltPerSide& RadiationMltPerPart) const
{
	if (not RadiationResistanceDescription)
	{
		return;
	}
	const FString Description = GetMltPerPartDescription(RadiationMltPerPart);
	RadiationResistanceDescription->SetText(FText::FromString(Description));
}

void UW_SelectedUnitDescription::SetupFireResistance(const FFireResistanceData& FireResistanceData) const
{
	if (not FireResistanceDescription)
	{
		return;
	}
	FString Description;
	Description += TEXT("Fire Threshold: ") + RTSRichtText(
			FormatFloatToText(FireResistanceData.MaxFireThreshold, 2).ToString(), ERTSRichText::Text_Armor)
		+ RTSRichtText(" Hp", ERTSRichText::Text_DescriptionHelper)
		+ TEXT("\n");
	Description += TEXT("Fire Recovery: ") + RTSRichtText(
			FormatFloatToText(FireResistanceData.FireRecoveryPerSecond, 2).ToString(), ERTSRichText::Text_Armor)
		+ RTSRichtText("/sec", ERTSRichText::Text_DescriptionHelper)
		+ TEXT("\n");

	FireResistanceDescription->SetText(FText::FromString(Description));
}

FString UW_SelectedUnitDescription::GetMltPerPartDescription(const FDamageMltPerSide& MltPerPart) const
{
	FString Description;
	Description += TEXT("Front: ") + RTSRichtText(
			FString::FromInt(GetDmgPercentageFromMlt(MltPerPart.FrontMlt)), ERTSRichText::Text_Armor)
		+ RTSRichtText(" damage%", ERTSRichText::Text_DescriptionHelper)
		+ TEXT("\n");
	Description += TEXT("Sides: ") + RTSRichtText(
			FString::FromInt(GetDmgPercentageFromMlt(MltPerPart.SidesMlt)), ERTSRichText::Text_Armor)
		+ RTSRichtText(" damage%", ERTSRichText::Text_DescriptionHelper)
		+ TEXT("\n");
	Description += TEXT("Rear: ") + RTSRichtText(
			FString::FromInt(GetDmgPercentageFromMlt(MltPerPart.RearMlt)), ERTSRichText::Text_Armor)
		+ RTSRichtText(" damage%", ERTSRichText::Text_DescriptionHelper);
	return Description;
}

int32 UW_SelectedUnitDescription::GetDmgPercentageFromMlt(const float Mlt) const
{
	const float Abs = FMath::Abs(Mlt);
	return static_cast<int32>(Abs * 100);
}

FString UW_SelectedUnitDescription::RTSRichtText(const FString& Text, const ERTSRichText TextType) const
{
	return FRTSRichTextConverter::MakeRTSRich(Text, TextType);
}
