// Copyright Bas Blokzijl - All rights reserved.
#include "NomadicDeployEditorLibrary.h"

#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"

#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#endif

#if WITH_EDITOR
namespace NomadicDeployEditorLibraryPrivate
{
	/**
	 * Old masters can carry UseConstant on the World Position Offset input (serialized from an earlier engine);
	 * FVectorMaterialInput::CompileWithDefault then returns the constant and ignores the wired expression.
	 * @return Description of the flag before it was cleared.
	 */
	FString ClearWorldPositionOffsetUseConstant(UMaterial& Material)
	{
		UMaterialEditorOnlyData* const EditorOnly = Material.GetEditorOnlyData();
		if (EditorOnly == nullptr)
		{
			return TEXT("no editor data");
		}
		FVectorMaterialInput& Input = EditorOnly->WorldPositionOffset;
		const FString Before = FString::Printf(TEXT("use_constant=%d constant=(%g,%g,%g)"), Input.UseConstant,
		                                       Input.Constant.X, Input.Constant.Y, Input.Constant.Z);
		Input.UseConstant = false;
		return Before;
	}
}
#endif

bool UNomadicDeployEditorLibrary::ConnectWorldPositionOffset(UMaterialExpression* FromExpression,
                                                             const FString& FromOutputName)
{
#if WITH_EDITOR
	if (not IsValid(FromExpression))
	{
		return false;
	}
	if (UMaterial* const Material = FromExpression->GetTypedOuter<UMaterial>())
	{
		NomadicDeployEditorLibraryPrivate::ClearWorldPositionOffsetUseConstant(*Material);
	}
	return UMaterialEditingLibrary::ConnectMaterialProperty(FromExpression, FromOutputName, MP_WorldPositionOffset);
#else
	return false;
#endif
}

UMaterialExpression* UNomadicDeployEditorLibrary::GetWorldPositionOffsetInput(UMaterial* Material,
                                                                              FString& OutOutputName)
{
#if WITH_EDITOR
	if (not IsValid(Material))
	{
		return nullptr;
	}
	OutOutputName = UMaterialEditingLibrary::GetMaterialPropertyInputNodeOutputName(Material, MP_WorldPositionOffset);
	return UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, MP_WorldPositionOffset);
#else
	return nullptr;
#endif
}

FString UNomadicDeployEditorLibrary::DescribeWorldPositionOffset(UMaterial* Material)
{
#if WITH_EDITOR
	if (not IsValid(Material))
	{
		return TEXT("invalid material");
	}
	const FExpressionInput* const Input = Material->GetExpressionInputForProperty(MP_WorldPositionOffset);
	const bool bEditorInputConnected = Input != nullptr && Input->IsConnected();
	const bool bCompilerSeesConnected = Material->IsPropertyConnected(MP_WorldPositionOffset);
	const UMaterialEditorOnlyData* const EditorOnly = Material->GetEditorOnlyData();
	const bool bUseConstant = EditorOnly != nullptr && EditorOnly->WorldPositionOffset.UseConstant;
	return FString::Printf(TEXT("editor_input_connected=%d cached_connected=%d use_constant=%d"), bEditorInputConnected,
	                       bCompilerSeesConnected, bUseConstant);
#else
	return TEXT("editor only");
#endif
}

FString UNomadicDeployEditorLibrary::WireSwitchToWorldPositionOffset(UMaterial* Material, const FName SwitchName)
{
#if WITH_EDITOR
	if (not IsValid(Material))
	{
		return TEXT("invalid material");
	}
	UMaterialExpressionStaticSwitchParameter* Switch = nullptr;
	for (UMaterialExpression* const Expression : Material->GetExpressions())
	{
		UMaterialExpressionStaticSwitchParameter* const Candidate = Cast<UMaterialExpressionStaticSwitchParameter>(Expression);
		if (IsValid(Candidate) && Candidate->ParameterName == SwitchName)
		{
			Switch = Candidate;
			break;
		}
	}
	if (Switch == nullptr)
	{
		return TEXT("no switch ") + SwitchName.ToString();
	}
	FExpressionInput* const Input = Material->GetExpressionInputForProperty(MP_WorldPositionOffset);
	const FString Before = Input != nullptr && Input->Expression != nullptr
		                       ? Input->Expression->GetClass()->GetName()
		                       : TEXT("none");
	// Keep an existing offset: the switch sits on B of an Add that already feeds the pin.
	UMaterialExpressionAdd* const ExistingAdd = Input != nullptr ? Cast<UMaterialExpressionAdd>(Input->Expression) : nullptr;
	const bool bAddHoldsSwitch = ExistingAdd != nullptr && ExistingAdd->B.Expression == Switch;
	const FString ConstantBefore = NomadicDeployEditorLibraryPrivate::ClearWorldPositionOffsetUseConstant(*Material);
	if (not bAddHoldsSwitch)
	{
		UMaterialEditingLibrary::ConnectMaterialProperty(Switch, FString(), MP_WorldPositionOffset);
	}
	const FString After = Input != nullptr && Input->Expression != nullptr
		                      ? Input->Expression->GetClass()->GetName()
		                      : TEXT("none");
	return FString::Printf(TEXT("before=%s after=%s kept_add=%d wpo_input_was[%s]"), *Before, *After, bAddHoldsSwitch,
	                       *ConstantBefore);
#else
	return TEXT("editor only");
#endif
}

FString UNomadicDeployEditorLibrary::DescribeCompiledResource(UMaterialInterface* MaterialInterface)
{
#if WITH_EDITOR
	if (not IsValid(MaterialInterface))
	{
		return TEXT("invalid");
	}
	FString Description;
	if (const UMaterialInstance* const Instance = Cast<UMaterialInstance>(MaterialInterface))
	{
		Description += FString::Printf(TEXT("static_permutation=%d "), Instance->bHasStaticPermutationResource);
	}
	const FMaterialResource* const Resource = MaterialInterface->GetMaterialResource(GMaxRHIFeatureLevel);
	if (Resource == nullptr)
	{
		return Description + TEXT("no resource");
	}
	Description += FString::Printf(TEXT("uses_wpo=%d finished=%d valid_map=%d"),
	                               Resource->MaterialUsesWorldPositionOffset_GameThread(),
	                               Resource->IsCompilationFinished(), Resource->HasValidGameThreadShaderMap());
	for (const FString& Error : Resource->GetCompileErrors())
	{
		Description += TEXT(" | ERROR ") + Error;
	}
	return Description;
#else
	return TEXT("editor only");
#endif
}

void UNomadicDeployEditorLibrary::RefreshMaterial(UMaterial* Material)
{
#if WITH_EDITOR
	if (not IsValid(Material))
	{
		return;
	}
	Material->UpdateCachedExpressionData();
	UMaterialEditingLibrary::RecompileMaterial(Material);
#endif
}
