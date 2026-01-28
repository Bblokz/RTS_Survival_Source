#pragma once

enum class ERTSOutLineTypes : uint8;

struct FRTSOutlineHelpers
{
	static void SetRTSOutLineOnComponent(UPrimitiveComponent* ValidComponent,
		const ERTSOutLineTypes OutlineType);
};
