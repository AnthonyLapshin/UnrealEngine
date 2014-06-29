// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperFlipbookSceneProxy.h"
#include "PaperFlipbookComponent.h"

//////////////////////////////////////////////////////////////////////////
// UPaperFlipbookComponent

UPaperFlipbookComponent::UPaperFlipbookComponent(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	static ConstructorHelpers::FObjectFinder<UMaterial> DefaultMaterial(TEXT("/Paper2D/DefaultSpriteMaterial"));
	Material = DefaultMaterial.Object;

	SpriteColor = FLinearColor::White;

	Mobility = EComponentMobility::Movable;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	bTickInEditor = true;
	
	CachedFrameIndex = INDEX_NONE;
	AccumulatedTime = 0.0f;
	PlayRate = 1.0f;
}

FPrimitiveSceneProxy* UPaperFlipbookComponent::CreateSceneProxy()
{
	FPaperFlipbookSceneProxy* NewProxy = new FPaperFlipbookSceneProxy(this);

	CalculateCurrentFrame();
	UPaperSprite* SpriteToSend = NULL;
	if ((SourceFlipbook != NULL) && (CachedFrameIndex >= 0) && (CachedFrameIndex < SourceFlipbook->KeyFrames.Num()))
	{
		SpriteToSend = SourceFlipbook->KeyFrames[CachedFrameIndex].Sprite;
	}

	FSpriteDrawCallRecord DrawCall;
	DrawCall.BuildFromSprite(SpriteToSend);
	DrawCall.Color = SpriteColor;
	NewProxy->SetDrawCall_RenderThread(DrawCall);
	return NewProxy;
}

FBoxSphereBounds UPaperFlipbookComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	if (SourceFlipbook != nullptr)
	{
		// Graphics bounds.
		FBoxSphereBounds NewBounds = SourceFlipbook->GetRenderBounds().TransformBy(LocalToWorld);

		//@TODO: PAPER2D: Add collision support to flipbooks
#if 0
		// Add bounds of collision geometry (if present).
		if (UBodySetup* BodySetup = SourceFlipbook->BodySetup)
		{
			const FBox AggGeomBox = BodySetup->AggGeom.CalcAABB(LocalToWorld);
			if (AggGeomBox.IsValid)
			{
				NewBounds = Union(NewBounds, FBoxSphereBounds(AggGeomBox));
			}
		}
#endif

		// Apply bounds scale
		NewBounds.BoxExtent *= BoundsScale;
		NewBounds.SphereRadius *= BoundsScale;

		return NewBounds;
	}
	else
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
	}
}

void UPaperFlipbookComponent::CalculateCurrentFrame()
{
	if (SourceFlipbook != NULL && SourceFlipbook->FramesPerSecond > 0)
	{
		int32 LastCachedFrame = CachedFrameIndex;

		float SumTime = 0.0f;
		int32 FrameIndex;
		for (FrameIndex = 0; FrameIndex < SourceFlipbook->KeyFrames.Num(); ++FrameIndex)
		{
			SumTime += SourceFlipbook->KeyFrames[FrameIndex].FrameRun / SourceFlipbook->FramesPerSecond;

			if (AccumulatedTime < SumTime)
			{
				break;
			}
		}

		if (AccumulatedTime >= SumTime)
		{
			AccumulatedTime = FMath::Fmod(AccumulatedTime, SumTime);
			//@TODO: Could have gone farther than this!!!
			CachedFrameIndex = 0;
		}
		else
		{
			CachedFrameIndex = FrameIndex;
		}

		if (CachedFrameIndex != LastCachedFrame)
		{
			// Indicate we need to send new dynamic data.
			MarkRenderDynamicDataDirty();
		}
	}
}

void UPaperFlipbookComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Indicate we need to send new dynamic data.
	MarkRenderDynamicDataDirty();
	
	AccumulatedTime += DeltaTime * PlayRate;
	CalculateCurrentFrame();

	//Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UPaperFlipbookComponent::SendRenderDynamicData_Concurrent()
{
	if (SceneProxy != NULL)
	{
		UPaperSprite* SpriteToSend = NULL;
		if ((SourceFlipbook != NULL) && (CachedFrameIndex >= 0) && (CachedFrameIndex < SourceFlipbook->KeyFrames.Num()))
		{
			SpriteToSend = SourceFlipbook->KeyFrames[CachedFrameIndex].Sprite;
		}

		FSpriteDrawCallRecord DrawCall;
		DrawCall.BuildFromSprite(SpriteToSend);
		DrawCall.Color = SpriteColor;

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				FSendPaperRenderComponentDynamicData,
				FPaperRenderSceneProxy*,InSceneProxy,(FPaperRenderSceneProxy*)SceneProxy,
				FSpriteDrawCallRecord,InSpriteToSend,DrawCall,
			{
				InSceneProxy->SetDrawCall_RenderThread(InSpriteToSend);
			});
	}
}

bool UPaperFlipbookComponent::SetFlipbook(class UPaperFlipbook* NewFlipbook)
{
	if (NewFlipbook != SourceFlipbook)
	{
		// Don't allow changing the sprite if we are "static".
		AActor* Owner = GetOwner();
		if (!IsRegistered() || (Owner == NULL) || (Mobility != EComponentMobility::Static))
		{
			SourceFlipbook = NewFlipbook;

			// We need to also reset the frame and time also
			AccumulatedTime = 0.0f;
			CalculateCurrentFrame();

			// Need to send this to render thread at some point
			MarkRenderStateDirty();

			// Update physics representation right away
			RecreatePhysicsState();

			// Since we have new mesh, we need to update bounds
			UpdateBounds();

			return true;
		}
	}

	return false;
}

UPaperFlipbook* UPaperFlipbookComponent::GetFlipbook()
{
	return SourceFlipbook;
}

UMaterialInterface* UPaperFlipbookComponent::GetSpriteMaterial() const
{
	return Material;
}

void UPaperFlipbookComponent::SetSpriteColor(FLinearColor NewColor)
{
	// Can't set color on a static component
	if (!(IsRegistered() && (Mobility == EComponentMobility::Static)) && (SpriteColor != NewColor))
	{
		SpriteColor = NewColor;

		//@TODO: Should we send immediately?
		MarkRenderDynamicDataDirty();
	}
}

void UPaperFlipbookComponent::SetCurrentTime(float NewTime)
{
	// Can't set color on a static component
	if (!(IsRegistered() && (Mobility == EComponentMobility::Static)) && (NewTime != AccumulatedTime))
	{
		AccumulatedTime = NewTime;
		CalculateCurrentFrame();
	}
}

const UObject* UPaperFlipbookComponent::AdditionalStatObject() const
{
	return SourceFlipbook;
}
