// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidTargetPlatformModule.cpp: Implements the FAndroidTargetPlatformModule class.
=============================================================================*/

#include "Android_PVRTCTargetPlatformPrivatePCH.h"

#define LOCTEXT_NAMESPACE "FAndroid_PVRTCTargetPlatformModule" 

/**
 * Android cooking platform which cooks only PVRTC based textures.
 */
class FAndroid_PVRTCTargetPlatform
	: public FAndroidTargetPlatform<FAndroid_PVRTCPlatformProperties>
{
	virtual FString GetAndroidVariantName( ) override
	{
		return TEXT("PVRTC");
	}

	virtual FText DisplayName( ) const override
	{
		return LOCTEXT("Android_PVRTC", "Android (PVRTC)");
	}

	virtual FString PlatformName() const override
	{
		return FString(FAndroid_PVRTCPlatformProperties::PlatformName());
	}

	virtual bool SupportsTextureFormat( FName Format ) const override
	{
		if( Format == AndroidTexFormat::NamePVRTC2 ||
			Format == AndroidTexFormat::NamePVRTC4 ||
			Format == AndroidTexFormat::NameAutoPVRTC )
		{
			return true;
		}
		return false;
	}

	virtual bool SupportedByExtensionsString( const FString& ExtensionsString, const int GLESVersion ) const override
	{
		return ExtensionsString.Contains(TEXT("GL_IMG_texture_compression_pvrtc"));
	}

	virtual FText GetVariantDisplayName() const override
	{
		return LOCTEXT("Android_PVRTC_ShortName", "PVRTC");
	}

	virtual float GetVariantPriority() const override
	{
		return 0.8f;
	}
};

/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* AndroidTargetSingleton = NULL;


/**
 * Module for the Android target platform.
 */
class FAndroid_PVRTCTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FAndroid_PVRTCTargetPlatformModule( )
	{
		AndroidTargetSingleton = NULL;
	}


public:
	
	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform() override
	{
		if (AndroidTargetSingleton == NULL)
		{
			AndroidTargetSingleton = new FAndroid_PVRTCTargetPlatform();
		}
		
		return AndroidTargetSingleton;
	}

	// End ITargetPlatformModule interface
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FAndroid_PVRTCTargetPlatformModule, Android_PVRTCTargetPlatform);
