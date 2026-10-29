// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UnitVariantAbilityCards.h"

#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectablePawnMaster.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/ApplyBehaviourAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedRockets/AttachedRockets.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AttachedWeaponAbilityComponent/AttachedWeaponAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ModeAbilityComponent/ModeAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/TurretSwapComponent/TurretSwapComp.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RTS_Survival/Units/Variants/UnitVariantPresentationLibrary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"

namespace UnitVariantAbilityCardsPrivate
{
	// Authored names of the Struct_Buttons columns and the FCustomAbilityData fields of CustomTypeMapping.
	const TCHAR* const RowIconName = TEXT("Icon");
	const TCHAR* const RowTitleName = TEXT("Title");
	const TCHAR* const RowDescriptionName = TEXT("Description");
	const TCHAR* const CustomTypeMappingName = TEXT("CustomTypeMapping");
	const TCHAR* const CustomIconName = TEXT("CustomIcon");
	const TCHAR* const CustomTitleName = TEXT("CustomTitle");
	const TCHAR* const CustomDescriptionName = TEXT("CustomDescription");
	constexpr int32 RowOwnCardCustomType = 0;

	/** The component names a variant drops from its base at BeginPlay; their abilities never reach the unit. */
	TArray<FName> GetRemovedInheritedComponents(const UClass* UnitClass)
	{
		const ASelectablePawnMaster* const DefaultPawn = Cast<ASelectablePawnMaster>(UnitClass->GetDefaultObject());
		return DefaultPawn != nullptr ? DefaultPawn->GetRemovedInheritedComponents() : TArray<FName>();
	}

	/** Native and Blueprint-added components, inherited templates resolved to the overrides of UnitClass. */
	void CollectComponentTemplates(const UClass* UnitClass, TArray<const UActorComponent*>& OutTemplates)
	{
		const TArray<FName> RemovedComponents = GetRemovedInheritedComponents(UnitClass);
		if (const AActor* const DefaultActor = Cast<AActor>(UnitClass->GetDefaultObject()))
		{
			TInlineComponentArray<UActorComponent*> NativeComponents;
			DefaultActor->GetComponents(NativeComponents);
			for (UActorComponent* const NativeComponent : NativeComponents)
			{
				if (IsValid(NativeComponent) && not RemovedComponents.Contains(NativeComponent->GetFName()))
				{
					OutTemplates.Add(NativeComponent);
				}
			}
		}
		UBlueprintGeneratedClass* const MostDerivedClass = Cast<UBlueprintGeneratedClass>(const_cast<UClass*>(UnitClass));
		for (const UClass* Class = UnitClass; Class != nullptr; Class = Class->GetSuperClass())
		{
			const UBlueprintGeneratedClass* const BlueprintClass = Cast<UBlueprintGeneratedClass>(Class);
			if (BlueprintClass == nullptr || BlueprintClass->SimpleConstructionScript == nullptr)
			{
				continue;
			}
			for (USCS_Node* const Node : BlueprintClass->SimpleConstructionScript->GetAllNodes())
			{
				if (Node != nullptr && not RemovedComponents.Contains(Node->GetVariableName()))
				{
					OutTemplates.Add(Node->GetActualComponentTemplate(MostDerivedClass));
				}
			}
		}
	}

	bool TryMakeAbilityEntry(const UActorComponent* Template, FUnitAbilityEntry& OutEntry)
	{
		if (const UApplyBehaviourAbilityComponent* const Behaviour = Cast<UApplyBehaviourAbilityComponent>(Template))
		{
			OutEntry = Behaviour->MakeAbilityEntry();
		}
		else if (const UAimAbilityComponent* const Aim = Cast<UAimAbilityComponent>(Template))
		{
			OutEntry = Aim->MakeAbilityEntry();
		}
		else if (const UAttachedRockets* const Rockets = Cast<UAttachedRockets>(Template))
		{
			OutEntry = Rockets->MakeAbilityEntry();
		}
		else if (const UAttachedWeaponAbilityComponent* const AttachedWeapon = Cast<UAttachedWeaponAbilityComponent>(Template))
		{
			OutEntry = AttachedWeapon->MakeAbilityEntry();
		}
		else if (const UModeAbilityComponent* const Mode = Cast<UModeAbilityComponent>(Template))
		{
			OutEntry = Mode->MakeAbilityEntry();
		}
		else if (const UTurretSwapComp* const TurretSwap = Cast<UTurretSwapComp>(Template))
		{
			OutEntry = TurretSwap->MakeAbilityEntry();
		}
		else
		{
			return false;
		}
		return OutEntry.AbilityId != EAbilityID::IdNoAbility;
	}

	/** User defined struct properties carry a GUID suffix; the authored name is the one shown in the editor. */
	const FProperty* FindPropertyByAuthoredName(const UStruct* Struct, const TCHAR* AuthoredName)
	{
		for (TFieldIterator<FProperty> PropertyIt(Struct); PropertyIt; ++PropertyIt)
		{
			if (PropertyIt->GetAuthoredName() == AuthoredName)
			{
				return *PropertyIt;
			}
		}
		return nullptr;
	}

	/** Struct_Buttons keeps its own title and description as strings, FCustomAbilityData as texts. */
	FText ReadTextField(const UStruct* Struct, const void* Container, const TCHAR* AuthoredName)
	{
		const FProperty* const Property = FindPropertyByAuthoredName(Struct, AuthoredName);
		if (const FTextProperty* const TextProperty = CastField<FTextProperty>(Property))
		{
			return TextProperty->GetPropertyValue_InContainer(Container);
		}
		if (const FStrProperty* const StringProperty = CastField<FStrProperty>(Property))
		{
			return FText::FromString(StringProperty->GetPropertyValue_InContainer(Container));
		}
		return FText::GetEmpty();
	}

	UTexture2D* ReadIconField(const UStruct* Struct, const void* Container, const TCHAR* AuthoredName)
	{
		const FObjectPropertyBase* const IconProperty = CastField<FObjectPropertyBase>(
			FindPropertyByAuthoredName(Struct, AuthoredName));
		if (IconProperty == nullptr)
		{
			return nullptr;
		}
		return Cast<UTexture2D>(IconProperty->GetObjectPropertyValue_InContainer(Container));
	}

	/** @return The FCustomAbilityData of CustomType in the row's mapping, null when it has none. */
	const uint8* FindCustomTypeEntry(const UStruct* RowStruct, const uint8* RowData, const int32 CustomType,
	                                 const UStruct*& OutEntryStruct)
	{
		const FMapProperty* const MappingProperty = CastField<FMapProperty>(
			FindPropertyByAuthoredName(RowStruct, CustomTypeMappingName));
		const FStructProperty* const EntryProperty = MappingProperty != nullptr
			                                             ? CastField<FStructProperty>(MappingProperty->ValueProp)
			                                             : nullptr;
		if (EntryProperty == nullptr || not MappingProperty->KeyProp->IsA<FIntProperty>())
		{
			return nullptr;
		}
		OutEntryStruct = EntryProperty->Struct;
		FScriptMapHelper MapHelper(MappingProperty, MappingProperty->ContainerPtrToValuePtr<void>(RowData));
		for (FScriptMapHelper::FIterator MapIt(MapHelper); MapIt; ++MapIt)
		{
			if (*reinterpret_cast<const int32*>(MapHelper.GetKeyPtr(MapIt)) == CustomType)
			{
				return MapHelper.GetValuePtr(MapIt);
			}
		}
		return nullptr;
	}

	/** The action bar description widget turns the escaped new lines the table stores into real ones too. */
	FText ConvertEscapedNewLines(const FText& Description)
	{
		FString DescriptionString = Description.ToString();
		DescriptionString.ReplaceInline(TEXT("\\n"), TEXT("\n"));
		DescriptionString.TrimEndInline();
		return FText::FromString(DescriptionString);
	}
}

TArray<FUnitAbilityEntry> UnitVariantAbilityCards::CollectComponentAbilities(const UClass* UnitClass)
{
	using namespace UnitVariantAbilityCardsPrivate;
	TArray<FUnitAbilityEntry> Abilities;
	if (not IsValid(UnitClass))
	{
		return Abilities;
	}
	TArray<const UActorComponent*> Templates;
	CollectComponentTemplates(UnitClass, Templates);
	for (const UActorComponent* const Template : Templates)
	{
		FUnitAbilityEntry Entry;
		if (not IsValid(Template) || not TryMakeAbilityEntry(Template, Entry))
		{
			continue;
		}
		const bool bIsDuplicate = Abilities.ContainsByPredicate([&Entry](const FUnitAbilityEntry& Existing)
		{
			return Existing.AbilityId == Entry.AbilityId && Existing.CustomType == Entry.CustomType;
		});
		if (not bIsDuplicate)
		{
			Abilities.Add(Entry);
		}
	}
	return Abilities;
}

bool UnitVariantAbilityCards::FindAbilityCard(const UDataTable* ButtonsTable, const FUnitAbilityEntry& Ability,
                                              FUnitVariantAbilityCard& OutCard)
{
	using namespace UnitVariantAbilityCardsPrivate;
	if (not IsValid(ButtonsTable) || ButtonsTable->GetRowStruct() == nullptr)
	{
		return false;
	}
	const FName RowName(URTSBlueprintFunctionLibrary::BP_GetAbilityIDString(Ability.AbilityId));
	const uint8* const RowData = ButtonsTable->FindRowUnchecked(RowName);
	if (RowData == nullptr)
	{
		return false;
	}
	const UStruct* const RowStruct = ButtonsTable->GetRowStruct();
	const UStruct* EntryStruct = nullptr;
	const uint8* const CustomEntry = Ability.CustomType == RowOwnCardCustomType
		                                 ? nullptr
		                                 : FindCustomTypeEntry(RowStruct, RowData, Ability.CustomType, EntryStruct);
	if (CustomEntry != nullptr)
	{
		OutCard.Title = ReadTextField(EntryStruct, CustomEntry, CustomTitleName);
		OutCard.Description = ReadTextField(EntryStruct, CustomEntry, CustomDescriptionName);
		OutCard.Icon = ReadIconField(EntryStruct, CustomEntry, CustomIconName);
	}
	else
	{
		OutCard.Title = ReadTextField(RowStruct, RowData, RowTitleName);
		OutCard.Description = ReadTextField(RowStruct, RowData, RowDescriptionName);
		OutCard.Icon = ReadIconField(RowStruct, RowData, RowIconName);
	}
	OutCard.AbilityId = Ability.AbilityId;
	OutCard.CustomType = Ability.CustomType;
	OutCard.Title = UUnitVariantPresentationLibrary::StripRichTextMarkup(OutCard.Title);
	OutCard.Description = ConvertEscapedNewLines(OutCard.Description);
	return true;
}

UObject* UnitVariantAbilityCards::FindRowObject(const UDataTable* Table, const FName RowName,
                                               const TCHAR* AuthoredColumnName)
{
	using namespace UnitVariantAbilityCardsPrivate;
	if (not IsValid(Table) || Table->GetRowStruct() == nullptr)
	{
		return nullptr;
	}
	const uint8* const RowData = Table->FindRowUnchecked(RowName);
	const FObjectPropertyBase* const Column = CastField<FObjectPropertyBase>(
		FindPropertyByAuthoredName(Table->GetRowStruct(), AuthoredColumnName));
	if (RowData == nullptr || Column == nullptr)
	{
		return nullptr;
	}
	return Column->GetObjectPropertyValue_InContainer(RowData);
}

FUnitVariantAbilityCardList UnitVariantAbilityCards::MakeAbilityCards(const UClass* UnitClass,
                                                                      const UDataTable* ButtonsTable)
{
	FUnitVariantAbilityCardList CardList;
	for (const FUnitAbilityEntry& Ability : CollectComponentAbilities(UnitClass))
	{
		FUnitVariantAbilityCard Card;
		if (FindAbilityCard(ButtonsTable, Ability, Card))
		{
			CardList.Cards.Add(Card);
		}
	}
	return CardList;
}
