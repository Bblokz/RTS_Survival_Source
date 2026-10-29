// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSMaterialGraphLibrary.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpressionComment.h"

bool URTSMaterialGraphLibrary::SetCommentSize(UMaterialExpressionComment* Comment, const int32 SizeX, const int32 SizeY)
{
#if WITH_EDITORONLY_DATA
	if (not IsValid(Comment))
	{
		return false;
	}
	Comment->Modify();
	Comment->SizeX = SizeX;
	Comment->SizeY = SizeY;
	return true;
#else
	return false;
#endif
}

bool URTSMaterialGraphLibrary::SetMaterialOutputPosition(UMaterial* Material, const int32 PositionX,
                                                         const int32 PositionY)
{
#if WITH_EDITORONLY_DATA
	if (not IsValid(Material))
	{
		return false;
	}
	Material->Modify();
	Material->EditorX = PositionX;
	Material->EditorY = PositionY;
	return true;
#else
	return false;
#endif
}
