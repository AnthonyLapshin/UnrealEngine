// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "TestBeaconClient.h"

ATestBeaconHost::ATestBeaconHost(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	BeaconTypeName = TEST_BEACON_TYPE;
	ClientBeaconActorClass = ATestBeaconClient::StaticClass();
}

bool ATestBeaconHost::Init()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogBeacon, Verbose, TEXT("Init"));
#endif
	return true;
}

void ATestBeaconHost::ClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection)
{
#if !UE_BUILD_SHIPPING
	Super::ClientConnected(NewClientActor, ClientConnection);

	ATestBeaconClient* BeaconClient = Cast<ATestBeaconClient>(NewClientActor);
	if (BeaconClient != NULL)
	{
		BeaconClient->ClientPing();
	}
#endif
}

AOnlineBeaconClient* ATestBeaconHost::SpawnBeaconActor(UNetConnection* ClientConnection)
{	
#if !UE_BUILD_SHIPPING
	return Super::SpawnBeaconActor(ClientConnection);
#else
	return NULL;
#endif
}
