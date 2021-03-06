// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AI/Navigation/NavRelevantComponent.h"
#include "NavModifierComponent.generated.h"

class UNavArea;
struct FCompositeNavModifier;

UCLASS(ClassGroup = (Navigation), meta = (BlueprintSpawnableComponent), hidecategories = (Activation))
class UNavModifierComponent : public UNavRelevantComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Navigation)
	TSubclassOf<UNavArea> AreaClass;

	virtual void CalcAndCacheBounds() const override;
	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;

protected:
	mutable FVector ObstacleExtent;
};
