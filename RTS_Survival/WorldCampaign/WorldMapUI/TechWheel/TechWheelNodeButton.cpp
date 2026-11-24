// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/TechWheel/TechWheelNodeButton.h"

void UTechWheelNodeButton::InitNodeButton(const FName NodeId)
{
	M_NodeId = NodeId;
	FSlateBrush NoBrush;
	NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	FButtonStyle GlassOnlyStyle;
	GlassOnlyStyle.SetNormal(NoBrush);
	GlassOnlyStyle.SetHovered(NoBrush);
	GlassOnlyStyle.SetPressed(NoBrush);
	GlassOnlyStyle.SetDisabled(NoBrush);
	GlassOnlyStyle.SetNormalPadding(FMargin(0.f));
	GlassOnlyStyle.SetPressedPadding(FMargin(0.f));
	SetStyle(GlassOnlyStyle);
	OnClicked.AddUniqueDynamic(this, &UTechWheelNodeButton::HandleClicked);
}

void UTechWheelNodeButton::HandleClicked()
{
	OnNodeClicked.Broadcast(M_NodeId);
}
