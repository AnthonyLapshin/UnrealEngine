// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ActorFactory.generated.h"

class FAssetData;
class UBlueprint;
class ULevel;

UCLASS(collapsecategories, hidecategories=Object, editinlinenew, config=Editor, abstract, transient)
class UNREALED_API UActorFactory : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Name used as basis for 'New Actor' menu. */
	UPROPERTY()
	FText DisplayName;

	/** Indicates how far up the menu item should be. The higher the number, the higher up the list.*/
	UPROPERTY(config)
	int32 MenuPriority;

	/** name of actor subclass this actorfactory creates - dynamically loaded.  Overrides NewActorClass. */
	UPROPERTY(config)
	FString NewActorClassName;

	/**  AActor  subclass this ActorFactory creates. */
	UPROPERTY()
	TSubclassOf<class AActor>  NewActorClass;

	/** Whether to appear in the editor add actor quick menu */
	UPROPERTY()
	uint32 bShowInEditorQuickMenu:1;

	UPROPERTY()
	uint32 bUseSurfaceOrientation:1;

	/** Translation applied to the spawn position. */
	UPROPERTY()
	FVector SpawnPositionOffset;

	/** Called to actual create an actor with the supplied transform (scale is ignored), using the properties in the ActorFactory */
	AActor* CreateActor( UObject* Asset, ULevel* InLevel, FTransform Transform, EObjectFlags ObjectFlags = RF_Transactional, const FName Name = NAME_None );

	/** Called to create a blueprint class that can be used to spawn an actor from this factory */
	UBlueprint* CreateBlueprint( UObject* Instance, UObject* Outer, const FName Name, const FName CallingContext = NAME_None );

	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg );

	/** Name to put on context menu. */
	FText GetDisplayName() const { return DisplayName; }

	/** Initialize NewActorClass if necessary, and return default actor for that class. */
	virtual AActor* GetDefaultActor( const FAssetData& AssetData );

	/** Initialize NewActorClass if necessary, and return that class. */
	UClass* GetDefaultActorClass( const FAssetData& AssetData );

	/** Given an instance of an actor pertaining to this factory, find the asset that should be used to create a new actor */
	virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance);

	/** Return a quaternion which aligns this actor type to the specified surface normal */
	virtual FQuat AlignObjectToSurfaceNormal(const FVector& InSurfaceNormal, const FQuat& ActorRotation = FQuat::Identity) const;

protected:

	virtual bool PreSpawnActor( UObject* Asset, FTransform& InOutLocation);
	virtual AActor* SpawnActor( UObject* Asset, ULevel* InLevel, const FVector& Location, const FRotator& Rotation, EObjectFlags ObjectFlags, const FName& Name );

	/** Subclasses may implement this to modify the actor after it has been spawned 
	    IMPORTANT: If you override this, you should usually also override PostCreateBlueprint()! */
	virtual void PostSpawnActor( UObject* Asset, AActor* NewActor );

	/** Override this in derived factory classes if needed.  This is called after a blueprint is created by this factory to
	    update the blueprint's CDO properties with state from the asset for this factory.
		IMPORTANT: If you override this, you should usually also override PostSpawnActor()! */
	virtual void PostCreateBlueprint( UObject* Asset, AActor* CDO );
};



