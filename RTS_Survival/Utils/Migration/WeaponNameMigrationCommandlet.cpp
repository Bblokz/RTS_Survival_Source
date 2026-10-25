#include "WeaponNameMigrationCommandlet.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/ChildActorComponent.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_SwitchEnum.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
// UE 5.5 location; before 5.5 this was Engine/UserDefinedStruct.h.
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/SavePackage.h"

namespace WeaponNameMigration
{
	const TCHAR* const TsvHeader =
		TEXT("AssetPackage\tKind\tContainer\tNodeClass\tNodeTitle\tNodeGuid\tPinOrPropertyPath\tDirection\tRawValue\tCanonicalValue\tNumericValue\tLinkSource");
	constexpr int32 TsvColumnCount = 12;
	// Loading every Blueprint without collecting runs the commandlet out of memory on this project.
	constexpr int32 AssetsPerGarbageCollect = 50;
	constexpr int32 AssetsPerProgressLog = 500;
	// Init structs nest at most one level (FInitWeaponStateRailgun); the margin covers user defined structs.
	constexpr int32 MaxStructSearchDepth = 4;
	// Before the migration every package that stores a weapon value imports the enum, so its name table holds this.
	const ANSICHAR* const WeaponEnumNameInPackage = "EWeaponName";
	const TCHAR* const WeaponNameFieldSuffix = TEXT("WeaponName");

	struct FRow
	{
		FString AssetPackage;
		// Pin | StructPin | SwitchCase | Variable | LocalVariable | Property
		FString Kind;
		// Graph name for pins; CDO / SCS:<Var> / ICH:<Name> / ChildActor:<Var> / Row:<RowName> for properties.
		FString Container;
		FString NodeClass;
		FString NodeTitle;
		FString NodeGuid;
		FString PinOrPropertyPath;
		// In | Out for pins, "-" otherwise. Only unwired In pins hold a literal; Out rows show where a graph reads the value.
		FString Direction = TEXT("-");
		// Exactly as stored: "Kar_98k", "EWeaponName::KwK37_75MM", or a stale redirected name.
		FString RawValue;
		// Current enumerator short name after EnumRedirects. Empty means it could not be resolved; investigate.
		FString CanonicalValue;
		int64 NumericValue = INDEX_NONE;
		// Set when an input pin is wired: the value comes from another node, so there is no literal to restore.
		FString LinkSource;

		FString GetKey() const
		{
			return AssetPackage + TEXT("|") + Kind + TEXT("|") + Container + TEXT("|") + NodeGuid + TEXT("|") + PinOrPropertyPath;
		}
	};

	FString Sanitize(const FString& Value)
	{
		FString Result = Value;
		Result.ReplaceInline(TEXT("\t"), TEXT(" "));
		Result.ReplaceInline(TEXT("\r"), TEXT(" "));
		Result.ReplaceInline(TEXT("\n"), TEXT(" "));
		return Result;
	}

	void ResolveValue(const UEnum* WeaponEnum, FRow& Row)
	{
		// GetValueByNameString accepts the short and the prefixed form and follows the ini EnumRedirects.
		Row.NumericValue = WeaponEnum->GetValueByNameString(Row.RawValue);
		if (Row.NumericValue == INDEX_NONE)
		{
			return;
		}
		Row.CanonicalValue = WeaponEnum->GetNameStringByValue(Row.NumericValue);
	}

	bool IsWeaponNameProperty(const FProperty* Property, const UEnum* WeaponEnum)
	{
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			return EnumProperty->GetEnum() == WeaponEnum;
		}
		// After the migration the Blueprint-facing fields are FName properties that kept the WeaponName name.
		return Property->IsA<FNameProperty>() && Property->GetName().EndsWith(WeaponNameFieldSuffix);
	}

	bool StructHoldsWeaponName(const UStruct* Struct, const UEnum* WeaponEnum, const int32 Depth)
	{
		if (not Struct || Depth > MaxStructSearchDepth)
		{
			return false;
		}
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (IsWeaponNameProperty(*It, WeaponEnum))
			{
				return true;
			}
			const FStructProperty* StructProperty = CastField<FStructProperty>(*It);
			if (StructProperty && StructHoldsWeaponName(StructProperty->Struct, WeaponEnum, Depth + 1))
			{
				return true;
			}
		}
		return false;
	}

	bool IsWeaponNamePin(const UEdGraphPin* Pin, const UEnum* WeaponEnum)
	{
		if (Pin->PinType.PinSubCategoryObject == WeaponEnum)
		{
			return true;
		}
		// After the migration the struct and function pins are FName pins that kept the WeaponName name.
		return Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name
			&& Pin->PinName.ToString().EndsWith(WeaponNameFieldSuffix);
	}

	bool IsWeaponStructType(const FEdGraphPinType& PinType, const UEnum* WeaponEnum)
	{
		if (PinType.PinCategory != UEdGraphSchema_K2::PC_Struct)
		{
			return false;
		}
		return StructHoldsWeaponName(Cast<UStruct>(PinType.PinSubCategoryObject.Get()), WeaponEnum, 0);
	}

	/**
	 * @brief Finds the WeaponName values inside an exported struct literal such as "(WeaponName=KwK37_75MM,Range=...)".
	 * Unsplit struct pins and struct-typed variables store their weapon only in this text.
	 * @return One value per WeaponName field, in text order; nested structs are included.
	 */
	TArray<FString> ExtractWeaponNamesFromStructText(const FString& StructText)
	{
		TArray<FString> Values;
		const FString FieldPrefix = FString(WeaponNameFieldSuffix) + TEXT("=");
		int32 SearchFrom = 0;
		while (true)
		{
			const int32 FieldIndex = StructText.Find(FieldPrefix, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
			if (FieldIndex == INDEX_NONE)
			{
				return Values;
			}
			SearchFrom = FieldIndex + FieldPrefix.Len();
			// Skip fields that only end in WeaponName, e.g. SecondaryWeaponName=.
			const TCHAR Preceding = FieldIndex > 0 ? StructText[FieldIndex - 1] : TEXT('(');
			if (Preceding != TEXT('(') && Preceding != TEXT(','))
			{
				continue;
			}
			int32 ValueEnd = SearchFrom;
			while (ValueEnd < StructText.Len() && StructText[ValueEnd] != TEXT(',') && StructText[ValueEnd] != TEXT(')'))
			{
				++ValueEnd;
			}
			FString Value = StructText.Mid(SearchFrom, ValueEnd - SearchFrom);
			Value.TrimQuotesInline();
			Values.Add(Value);
		}
	}

	FString GetLinkSource(const UEdGraphPin* Pin)
	{
		if (Pin->Direction != EGPD_Input || Pin->LinkedTo.IsEmpty())
		{
			return FString();
		}
		const UEdGraphPin* SourcePin = Pin->LinkedTo[0];
		const UEdGraphNode* SourceNode = SourcePin->GetOwningNode();
		return SourceNode->GetNodeTitle(ENodeTitleType::ListView).ToString() + TEXT(".") + SourcePin->PinName.ToString();
	}

	FRow MakeNodeRow(const UEdGraphNode* Node, const FString& Package, const FString& GraphName, const FString& Kind)
	{
		FRow Row;
		Row.AssetPackage = Package;
		Row.Kind = Kind;
		Row.Container = GraphName;
		Row.NodeClass = Node->GetClass()->GetName();
		Row.NodeTitle = Sanitize(Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
		Row.NodeGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
		return Row;
	}

	void CollectSwitchCases(const UK2Node_SwitchEnum* SwitchNode, const FString& Package, const FString& GraphName,
	                        TArray<FRow>& OutRows)
	{
		for (const UEdGraphPin* Pin : SwitchNode->Pins)
		{
			if (Pin->Direction != EGPD_Output || Pin->LinkedTo.IsEmpty())
			{
				continue;
			}
			FRow Row = MakeNodeRow(SwitchNode, Package, GraphName, TEXT("SwitchCase"));
			Row.PinOrPropertyPath = Pin->PinName.ToString();
			Row.RawValue = Pin->PinName.ToString();
			OutRows.Add(Row);
		}
	}

	// Unwired struct input pins (e.g. a Setup*Weapon call whose init struct is typed in) hold the weapon in their text.
	void CollectStructPinRows(const UEdGraphNode* Node, const UEdGraphPin* Pin, const FString& Package,
	                          const FString& GraphName, const UEnum* WeaponEnum, TArray<FRow>& OutRows)
	{
		if (Pin->Direction != EGPD_Input || not Pin->LinkedTo.IsEmpty() || Pin->DefaultValue.IsEmpty())
		{
			return;
		}
		const TArray<FString> Values = ExtractWeaponNamesFromStructText(Pin->DefaultValue);
		for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
		{
			FRow Row = MakeNodeRow(Node, Package, GraphName, TEXT("StructPin"));
			Row.PinOrPropertyPath = Pin->PinName.ToString() + TEXT(".WeaponName#") + FString::FromInt(ValueIndex);
			Row.Direction = TEXT("In");
			Row.RawValue = Values[ValueIndex];
			ResolveValue(WeaponEnum, Row);
			OutRows.Add(Row);
		}
	}

	void CollectNodeRows(const UEdGraphNode* Node, const FString& Package, const FString& GraphName,
	                     const UEnum* WeaponEnum, TArray<FRow>& OutRows)
	{
		const UK2Node_SwitchEnum* SwitchNode = Cast<UK2Node_SwitchEnum>(Node);
		if (SwitchNode && SwitchNode->Enum == WeaponEnum)
		{
			CollectSwitchCases(SwitchNode, Package, GraphName, OutRows);
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (IsWeaponStructType(Pin->PinType, WeaponEnum))
			{
				CollectStructPinRows(Node, Pin, Package, GraphName, WeaponEnum, OutRows);
				continue;
			}
			if (not IsWeaponNamePin(Pin, WeaponEnum))
			{
				continue;
			}
			FRow Row = MakeNodeRow(Node, Package, GraphName, TEXT("Pin"));
			Row.PinOrPropertyPath = Pin->PinName.ToString() + (Pin->bOrphanedPin ? TEXT(" [ORPHANED]") : TEXT(""));
			Row.Direction = Pin->Direction == EGPD_Input ? TEXT("In") : TEXT("Out");
			Row.RawValue = Pin->DefaultValue;
			Row.LinkSource = Sanitize(GetLinkSource(Pin));
			ResolveValue(WeaponEnum, Row);
			OutRows.Add(Row);
		}
	}

	void CollectVariableRows(const TArray<FBPVariableDescription>& Variables, const FString& Package,
	                         const FString& Kind, const FString& Container, const UEnum* WeaponEnum,
	                         TArray<FRow>& OutRows)
	{
		for (const FBPVariableDescription& Variable : Variables)
		{
			FRow Row;
			Row.AssetPackage = Package;
			Row.Kind = Kind;
			Row.Container = Container;
			if (Variable.VarType.PinSubCategoryObject == WeaponEnum)
			{
				Row.PinOrPropertyPath = Variable.VarName.ToString();
				Row.RawValue = Variable.DefaultValue;
				ResolveValue(WeaponEnum, Row);
				OutRows.Add(Row);
				continue;
			}
			if (not IsWeaponStructType(Variable.VarType, WeaponEnum))
			{
				continue;
			}
			// Member variables also show up as CDO property rows; local struct variables only exist here.
			const TArray<FString> Values = ExtractWeaponNamesFromStructText(Variable.DefaultValue);
			for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
			{
				FRow StructRow = Row;
				StructRow.PinOrPropertyPath = Variable.VarName.ToString() + TEXT(".WeaponName#") + FString::FromInt(ValueIndex);
				StructRow.RawValue = Values[ValueIndex];
				ResolveValue(WeaponEnum, StructRow);
				OutRows.Add(StructRow);
			}
		}
	}

	bool TryReadWeaponProperty(const FProperty* Property, const void* Value, const UEnum* WeaponEnum, FString& OutRaw)
	{
		if (not IsWeaponNameProperty(Property, WeaponEnum))
		{
			return false;
		}
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			const int64 IntValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value);
			OutRaw = WeaponEnum->GetNameStringByValue(IntValue);
			return true;
		}
		OutRaw = CastField<FNameProperty>(Property)->GetPropertyValue(Value).ToString();
		return true;
	}

	void CollectPropertyRows(const UStruct* Struct, const void* Container, const FString& Package,
	                         const FString& ContainerLabel, const UEnum* WeaponEnum, TArray<FRow>& OutRows)
	{
		if (not Struct || not Container)
		{
			return;
		}
		for (TPropertyValueIterator<const FProperty> It(Struct, Container); It; ++It)
		{
			FString RawValue;
			if (not TryReadWeaponProperty(It.Key(), It.Value(), WeaponEnum, RawValue))
			{
				continue;
			}
			FRow Row;
			Row.AssetPackage = Package;
			Row.Kind = TEXT("Property");
			Row.Container = ContainerLabel;
			Row.PinOrPropertyPath = It.GetPropertyPathDebugString();
			Row.RawValue = RawValue;
			ResolveValue(WeaponEnum, Row);
			OutRows.Add(Row);
		}
	}

	// Instanced subobjects (e.g. the global abilities inside DA_GlobalAbilitySystem, C++ default subobject components
	// on a CDO) are separate UObjects, so the property walk of their owner does not reach them.
	void CollectSubobjectRows(UObject* Owner, const FString& Package, const FString& LabelPrefix, const UEnum* WeaponEnum,
	                          TArray<FRow>& OutRows)
	{
		TArray<UObject*> Subobjects;
		GetObjectsWithOuter(Owner, Subobjects, true);
		for (UObject* Subobject : Subobjects)
		{
			if (not Subobject)
			{
				continue;
			}
			CollectPropertyRows(Subobject->GetClass(), Subobject, Package, LabelPrefix + Subobject->GetPathName(Owner),
			                    WeaponEnum, OutRows);
		}
	}

	void CollectConstructionScriptTemplates(const UBlueprint* Blueprint, const FString& Package, const UEnum* WeaponEnum,
	                                        TArray<FRow>& OutRows)
	{
		if (not Blueprint->SimpleConstructionScript)
		{
			return;
		}
		for (const USCS_Node* ScsNode : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			UActorComponent* Template = ScsNode ? ScsNode->ComponentTemplate.Get() : nullptr;
			if (not Template)
			{
				continue;
			}
			const FString VariableName = ScsNode->GetVariableName().ToString();
			CollectPropertyRows(Template->GetClass(), Template, Package, TEXT("SCS:") + VariableName, WeaponEnum, OutRows);
			const UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>(Template);
			AActor* ChildTemplate = ChildActorComponent ? ChildActorComponent->GetChildActorTemplate() : nullptr;
			if (ChildTemplate)
			{
				CollectPropertyRows(ChildTemplate->GetClass(), ChildTemplate, Package, TEXT("ChildActor:") + VariableName,
				                    WeaponEnum, OutRows);
			}
		}
	}

	void CollectComponentTemplates(UBlueprint* Blueprint, const FString& Package, const UEnum* WeaponEnum,
	                               TArray<FRow>& OutRows)
	{
		CollectConstructionScriptTemplates(Blueprint, Package, WeaponEnum, OutRows);
		const UInheritableComponentHandler* Handler = Blueprint->GetInheritableComponentHandler(false);
		if (not Handler)
		{
			return;
		}
		TArray<UActorComponent*> Templates;
		Handler->GetAllTemplates(Templates);
		for (UActorComponent* Template : Templates)
		{
			CollectPropertyRows(Template->GetClass(), Template, Package, TEXT("ICH:") + Template->GetName(),
			                    WeaponEnum, OutRows);
		}
	}

	/**
	 * @param bRefreshNodes True after the migration: loading does not guarantee nodes were reconstructed against
	 * the new FName types, and reading the stale enum pins would report a false "verified".
	 */
	void CollectBlueprint(UBlueprint* Blueprint, const UEnum* WeaponEnum, const bool bRefreshNodes, TArray<FRow>& OutRows)
	{
		if (bRefreshNodes)
		{
			FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
		}
		const FString Package = Blueprint->GetOutermost()->GetName();
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (const UEdGraph* Graph : Graphs)
		{
			if (not Graph)
			{
				continue;
			}
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				if (not Node)
				{
					continue;
				}
				CollectNodeRows(Node, Package, Graph->GetName(), WeaponEnum, OutRows);
				if (const UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
				{
					CollectVariableRows(Entry->LocalVariables, Package, TEXT("LocalVariable"), Graph->GetName(),
					                    WeaponEnum, OutRows);
				}
			}
		}
		CollectVariableRows(Blueprint->NewVariables, Package, TEXT("Variable"), TEXT("Blueprint"), WeaponEnum, OutRows);
		if (UClass* GeneratedClass = Blueprint->GeneratedClass)
		{
			UObject* DefaultObject = GeneratedClass->GetDefaultObject();
			CollectPropertyRows(GeneratedClass, DefaultObject, Package, TEXT("CDO"), WeaponEnum, OutRows);
			CollectSubobjectRows(DefaultObject, Package, TEXT("CDOSub:"), WeaponEnum, OutRows);
		}
		CollectComponentTemplates(Blueprint, Package, WeaponEnum, OutRows);
	}

	void CollectDataTable(const UDataTable* Table, const UEnum* WeaponEnum, TArray<FRow>& OutRows)
	{
		const FString Package = Table->GetOutermost()->GetName();
		for (const TPair<FName, uint8*>& RowPair : Table->GetRowMap())
		{
			CollectPropertyRows(Table->GetRowStruct(), RowPair.Value, Package,
			                    TEXT("Row:") + RowPair.Key.ToString(), WeaponEnum, OutRows);
		}
	}

	void CollectAsset(UObject* Asset, const UEnum* WeaponEnum, const bool bRefreshNodes, TArray<FRow>& OutRows)
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
		{
			CollectBlueprint(Blueprint, WeaponEnum, bRefreshNodes, OutRows);
			return;
		}
		if (const UDataTable* Table = Cast<UDataTable>(Asset))
		{
			CollectDataTable(Table, WeaponEnum, OutRows);
			return;
		}
		if (const UUserDefinedStruct* UserStruct = Cast<UUserDefinedStruct>(Asset))
		{
			CollectPropertyRows(UserStruct, UserStruct->GetDefaultInstance(), UserStruct->GetOutermost()->GetName(),
			                    TEXT("StructDefaults"), WeaponEnum, OutRows);
			return;
		}
		if (Asset)
		{
			const FString Package = Asset->GetOutermost()->GetName();
			CollectPropertyRows(Asset->GetClass(), Asset, Package, TEXT("Asset"), WeaponEnum, OutRows);
			CollectSubobjectRows(Asset, Package, TEXT("Sub:"), WeaponEnum, OutRows);
		}
	}

	// Cheap byte search so the snapshot only loads the few hundred packages that can hold a weapon value.
	bool PackageFileMentionsWeaponEnum(const FName PackageName)
	{
		FString Filename;
		if (not FPackageName::DoesPackageExist(PackageName.ToString(), &Filename))
		{
			return false;
		}
		TArray<uint8> Bytes;
		if (not FFileHelper::LoadFileToArray(Bytes, *Filename))
		{
			return false;
		}
		const int32 NeedleLength = FCStringAnsi::Strlen(WeaponEnumNameInPackage);
		const uint8* Needle = reinterpret_cast<const uint8*>(WeaponEnumNameInPackage);
		const int32 LastStart = Bytes.Num() - NeedleLength;
		for (int32 ByteIndex = 0; ByteIndex <= LastStart; ++ByteIndex)
		{
			if (Bytes[ByteIndex] == Needle[0] && FMemory::Memcmp(&Bytes[ByteIndex], Needle, NeedleLength) == 0)
			{
				return true;
			}
		}
		return false;
	}

	TArray<FAssetData> GetCandidateAssets()
	{
		IAssetRegistry& AssetRegistry =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		AssetRegistry.SearchAllAssets(true);

		FARFilter Filter;
		Filter.PackagePaths.Add(TEXT("/Game"));
		Filter.bRecursivePaths = true;
		Filter.bRecursiveClasses = true;
		// bRecursiveClasses also picks up UWidgetBlueprint and other UBlueprint subclasses.
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UDataAsset::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UDataTable::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UUserDefinedStruct::StaticClass()->GetClassPathName());
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(Filter, Assets);
		return Assets;
	}

	/**
	 * @brief Loads candidate assets and records their weapon values. Never saves: RefreshAllNodes only changes memory.
	 * @param OnlyPackages Packages to scan (Verify uses the snapshot's); null scans every package that names the enum.
	 */
	TArray<FRow> CollectAll(const UEnum* WeaponEnum, const bool bRefreshNodes, const TSet<FString>* OnlyPackages)
	{
		const TArray<FAssetData> Assets = GetCandidateAssets();
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration: %d candidate assets"), Assets.Num());
		TArray<FRow> Rows;
		int32 LoadedCount = 0;
		for (int32 AssetIndex = 0; AssetIndex < Assets.Num(); ++AssetIndex)
		{
			if (AssetIndex > 0 && AssetIndex % AssetsPerProgressLog == 0)
			{
				UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration: %d / %d scanned, %d loaded, %d rows"),
				       AssetIndex, Assets.Num(), LoadedCount, Rows.Num());
			}
			const FAssetData& AssetData = Assets[AssetIndex];
			const bool bIsCandidate = OnlyPackages
				? OnlyPackages->Contains(AssetData.PackageName.ToString())
				: PackageFileMentionsWeaponEnum(AssetData.PackageName);
			if (not bIsCandidate)
			{
				continue;
			}
			if (LoadedCount > 0 && LoadedCount % AssetsPerGarbageCollect == 0)
			{
				// RF_NoFlags: in an editor commandlet the default keep flags keep every loaded asset alive.
				CollectGarbage(RF_NoFlags);
			}
			++LoadedCount;
			CollectAsset(AssetData.GetAsset(), WeaponEnum, bRefreshNodes, Rows);
		}
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration: loaded %d packages"), LoadedCount);
		return Rows;
	}

	bool WriteTsv(const TArray<FRow>& Rows, const FString& Path)
	{
		TArray<FString> Lines;
		Lines.Add(TsvHeader);
		for (const FRow& Row : Rows)
		{
			Lines.Add(FString::Join(TArray<FString>{
				Row.AssetPackage, Row.Kind, Row.Container, Row.NodeClass, Row.NodeTitle, Row.NodeGuid,
				Row.PinOrPropertyPath, Row.Direction, Sanitize(Row.RawValue), Row.CanonicalValue,
				LexToString(Row.NumericValue), Row.LinkSource}, TEXT("\t")));
		}
		return FFileHelper::SaveStringArrayToFile(Lines, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	TArray<FRow> ReadTsv(const FString& Path)
	{
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *Path);
		TArray<FRow> Rows;
		for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
		{
			TArray<FString> Cells;
			Lines[LineIndex].ParseIntoArray(Cells, TEXT("\t"), false);
			if (Cells.Num() != TsvColumnCount)
			{
				UE_LOG(LogTemp, Error, TEXT("WeaponNameMigration: malformed TSV line %d"), LineIndex + 1);
				continue;
			}
			FRow Row;
			Row.AssetPackage = Cells[0]; Row.Kind = Cells[1]; Row.Container = Cells[2]; Row.NodeClass = Cells[3];
			Row.NodeTitle = Cells[4]; Row.NodeGuid = Cells[5]; Row.PinOrPropertyPath = Cells[6];
			Row.Direction = Cells[7]; Row.RawValue = Cells[8]; Row.CanonicalValue = Cells[9];
			LexFromString(Row.NumericValue, *Cells[10]); Row.LinkSource = Cells[11];
			Rows.Add(Row);
		}
		return Rows;
	}

	UEdGraphNode* FindNodeByGuid(UBlueprint* Blueprint, const FString& GuidString)
	{
		FGuid Guid;
		if (not FGuid::Parse(GuidString, Guid))
		{
			return nullptr;
		}
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (const UEdGraph* Graph : Graphs)
		{
			const TObjectPtr<UEdGraphNode>* Found = Graph
				? Graph->Nodes.FindByPredicate([&Guid](const UEdGraphNode* Node) { return Node && Node->NodeGuid == Guid; })
				: nullptr;
			if (Found)
			{
				return *Found;
			}
		}
		return nullptr;
	}

	FString GetSnapshotPinName(const FRow& Row)
	{
		FString PinName = Row.PinOrPropertyPath;
		PinName.RemoveFromEnd(TEXT(" [ORPHANED]"));
		return PinName;
	}

	// @return Whether an orphan was removed, so the caller knows the Blueprint must be saved.
	bool RemoveOrphanedPin(UEdGraphNode* Node, const FName PinName)
	{
		UEdGraphPin* const* Orphan = Node->Pins.FindByPredicate([PinName](const UEdGraphPin* Pin)
		{
			return Pin->bOrphanedPin && Pin->PinName == PinName && Pin->LinkedTo.IsEmpty();
		});
		if (not Orphan)
		{
			return false;
		}
		Node->Modify();
		Node->RemovePin(*Orphan);
		return true;
	}

	// Only literals on unwired input pins can be restored automatically; everything else is a manual fix.
	bool IsRestorableRow(const FRow& Row)
	{
		return Row.Kind == TEXT("Pin") && Row.Direction == TEXT("In") && Row.LinkSource.IsEmpty()
			&& not Row.CanonicalValue.IsEmpty();
	}

	UEdGraphPin* FindLivePin(UEdGraphNode* Node, const FName PinName)
	{
		UEdGraphPin* const* LivePin = Node->Pins.FindByPredicate([PinName](const UEdGraphPin* Pin)
		{
			return not Pin->bOrphanedPin && Pin->PinName == PinName;
		});
		return LivePin ? *LivePin : nullptr;
	}

	// Returns the migrated FName pin. A pin that is still the old enum pin means the node was not reconstructed
	// yet; writing to it would be undone by the next reconstruct, so reconstruct first and refuse if it stays stale.
	UEdGraphPin* FindMigratedPin(UEdGraphNode* Node, const FName PinName)
	{
		UEdGraphPin* LivePin = FindLivePin(Node, PinName);
		if (LivePin && LivePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
		{
			return LivePin;
		}
		Node->ReconstructNode();
		LivePin = FindLivePin(Node, PinName);
		if (LivePin && LivePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
		{
			return LivePin;
		}
		return nullptr;
	}

	// @return Whether the Blueprint was modified and must be saved.
	bool ApplyRowToBlueprint(UBlueprint* Blueprint, const FRow& Row)
	{
		UEdGraphNode* Node = FindNodeByGuid(Blueprint, Row.NodeGuid);
		const FName PinName(*GetSnapshotPinName(Row));
		UEdGraphPin* MigratedPin = Node ? FindMigratedPin(Node, PinName) : nullptr;
		if (not MigratedPin)
		{
			UE_LOG(LogTemp, Error, TEXT("WeaponNameMigration FAILED: no migrated Name pin %s on node %s (%s) in %s"),
			       *PinName.ToString(), *Row.NodeTitle, *Row.NodeGuid, *Row.AssetPackage);
			return false;
		}
		const bool bRemovedOrphan = RemoveOrphanedPin(Node, PinName);
		if (MigratedPin->DefaultValue == Row.CanonicalValue)
		{
			return bRemovedOrphan;
		}
		Node->Modify();
		GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*MigratedPin, Row.CanonicalValue);
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration restored %s.%s = %s in %s"),
		       *Row.NodeTitle, *PinName.ToString(), *Row.CanonicalValue, *Row.AssetPackage);
		return true;
	}

	bool SavePackageToDisk(UPackage* Package)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, nullptr, *Filename, SaveArgs);
	}

	void ApplyBlueprintRows(const FString& Package, const TArray<const FRow*>& Rows)
	{
		const FString ObjectPath = Package + TEXT(".") + FPackageName::GetShortName(Package);
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
		if (not Blueprint)
		{
			UE_LOG(LogTemp, Error, TEXT("WeaponNameMigration FAILED: could not load %s"), *ObjectPath);
			return;
		}
		// Make K2 rebuild every node against the new FName types before we look at any pin.
		FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
		bool bAnyChanged = false;
		for (const FRow* Row : Rows)
		{
			bAnyChanged |= ApplyRowToBlueprint(Blueprint, *Row);
		}
		if (not bAnyChanged)
		{
			return;
		}
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (not SavePackageToDisk(Blueprint->GetOutermost()))
		{
			UE_LOG(LogTemp, Error, TEXT("WeaponNameMigration FAILED: could not save %s"), *Package);
		}
	}

	int32 RunApply(const FString& TsvPath)
	{
		const TArray<FRow> Rows = ReadTsv(TsvPath);
		TMap<FString, TArray<const FRow*>> RowsByPackage;
		for (const FRow& Row : Rows)
		{
			if (IsRestorableRow(Row))
			{
				RowsByPackage.FindOrAdd(Row.AssetPackage).Add(&Row);
			}
		}
		for (const TPair<FString, TArray<const FRow*>>& Pair : RowsByPackage)
		{
			ApplyBlueprintRows(Pair.Key, Pair.Value);
			// Save per package and collect garbage so a failure late in the run does not lose earlier work.
			CollectGarbage(RF_NoFlags);
		}
		return 0;
	}

	int32 RunVerify(const UEnum* WeaponEnum, const FString& TsvPath)
	{
		const TArray<FRow> SnapshotRows = ReadTsv(TsvPath);
		TSet<FString> SnapshotPackages;
		for (const FRow& Row : SnapshotRows)
		{
			SnapshotPackages.Add(Row.AssetPackage);
		}
		TMap<FString, FRow> AfterByKey;
		for (const FRow& Row : CollectAll(WeaponEnum, true, &SnapshotPackages))
		{
			FRow Normalised = Row;
			Normalised.PinOrPropertyPath.RemoveFromEnd(TEXT(" [ORPHANED]"));
			AfterByKey.Add(Normalised.GetKey(), Normalised);
		}
		int32 ProblemCount = 0;
		for (FRow Before : SnapshotRows)
		{
			Before.PinOrPropertyPath.RemoveFromEnd(TEXT(" [ORPHANED]"));
			const FRow* After = AfterByKey.Find(Before.GetKey());
			if (not After)
			{
				UE_LOG(LogTemp, Warning, TEXT("VERIFY MISSING  %s  %s  %s = %s"), *Before.AssetPackage,
				       *Before.NodeTitle, *Before.PinOrPropertyPath, *Before.CanonicalValue);
				++ProblemCount;
				continue;
			}
			if (After->CanonicalValue != Before.CanonicalValue || After->LinkSource != Before.LinkSource)
			{
				UE_LOG(LogTemp, Warning, TEXT("VERIFY CHANGED  %s  %s  %s : %s -> %s"), *Before.AssetPackage,
				       *Before.NodeTitle, *Before.PinOrPropertyPath, *Before.CanonicalValue, *After->CanonicalValue);
				++ProblemCount;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration verify: %d problem rows"), ProblemCount);
		return ProblemCount == 0 ? 0 : 1;
	}
}
#endif

UWeaponNameMigrationCommandlet::UWeaponNameMigrationCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UWeaponNameMigrationCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WeaponNameMigration;
	const UEnum* WeaponEnum = StaticEnum<EWeaponName>();
	FString Mode;
	FString TsvPath;
	if (not WeaponEnum || not FParse::Value(*Params, TEXT("Mode="), Mode) || not FParse::Value(*Params, TEXT("Tsv="), TsvPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Usage: -run=WeaponNameMigration -Mode=Snapshot|Apply|Verify -Tsv=<path>"));
		return 1;
	}
	if (Mode == TEXT("Snapshot"))
	{
		// No refresh: before the migration the loaded graphs are exactly what is on disk.
		const TArray<FRow> Rows = CollectAll(WeaponEnum, false, nullptr);
		UE_LOG(LogTemp, Display, TEXT("WeaponNameMigration snapshot: %d rows"), Rows.Num());
		return WriteTsv(Rows, TsvPath) ? 0 : 1;
	}
	if (Mode == TEXT("Apply"))
	{
		return RunApply(TsvPath);
	}
	if (Mode == TEXT("Verify"))
	{
		return RunVerify(WeaponEnum, TsvPath);
	}
	UE_LOG(LogTemp, Error, TEXT("WeaponNameMigration: unknown mode %s"), *Mode);
	return 1;
#else
	return 1;
#endif
}
