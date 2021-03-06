// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CodeAudioEffects.cpp: Unreal CoreAudio audio effects interface object.
=============================================================================*/

/*------------------------------------------------------------------------------------
 Audio includes.
 ------------------------------------------------------------------------------------*/

#include "CoreAudioDevice.h"
#include "AudioEffect.h"
#include "CoreAudioEffects.h"
#include "Engine.h"

extern FCoreAudioSoundSource *GAudioChannels[MAX_AUDIOCHANNELS + 1];

static CFBundleRef LoadRadioEffectComponent()
{
	bool bLoaded = false;
	CFBundleRef Bundle = NULL;

	CFBundleRef MainBundleRef = CFBundleGetMainBundle();
	if( MainBundleRef )
	{
		CFURLRef ComponentURLRef = CFBundleCopyResourceURL( MainBundleRef, CFSTR("RadioEffectUnit"), CFSTR("component"), 0 );
		if( ComponentURLRef )
		{
			Bundle = CFBundleCreate(kCFAllocatorDefault, ComponentURLRef);
			if(Bundle)
			{
				CFArrayRef Components = (CFArrayRef)CFBundleGetValueForInfoDictionaryKey(Bundle, CFSTR("AudioComponents"));
				if ( Components && CFArrayGetCount(Components) )
				{
					CFDictionaryRef ComponentInfo = (CFDictionaryRef)CFArrayGetValueAtIndex(Components, 0);
					CFNumberRef ComponentVersion = (CFNumberRef)CFDictionaryGetValue(ComponentInfo, CFSTR("version"));
					CFStringRef ComponentFactoryFunction = (CFStringRef)CFDictionaryGetValue(ComponentInfo, CFSTR("factoryFunction"));
					if( ComponentVersion && ComponentFactoryFunction )
					{
						int32 Version = 0;
						CFNumberGetValue(ComponentVersion, kCFNumberSInt32Type, &Version);
						
						AudioComponentFactoryFunction Factory = (AudioComponentFactoryFunction)CFBundleGetFunctionPointerForName(Bundle, ComponentFactoryFunction);
						if(Factory)
						{
							//	fill out the AudioComponentDescription
							AudioComponentDescription theDescription;
							theDescription.componentType = kAudioUnitType_Effect;
							theDescription.componentSubType = 'Rdio';
							theDescription.componentManufacturer = 'Epic';
							theDescription.componentFlagsMask = 0;
							
							AudioComponent RadioComponent = AudioComponentFindNext(nullptr, &theDescription);
							if ( !RadioComponent )
							{
								RadioComponent = AudioComponentRegister(&theDescription, CFSTR("Epic Games: RadioEffectUnit"), Version, Factory);
								check(RadioComponent);
							}
							bLoaded = (RadioComponent != NULL);
						}
					}
				}
				if(!bLoaded)
				{
					CFRelease(Bundle);
					Bundle = NULL;
				}
			}

			CFRelease( ComponentURLRef );
		}
	}

	return Bundle;
}

/*------------------------------------------------------------------------------------
	FCoreAudioEffectsManager.
------------------------------------------------------------------------------------*/

TConsoleVariableData<float>* FCoreAudioEffectsManager::Radio_ChebyshevPowerMultiplier	= IConsoleManager::Get().RegisterConsoleVariable( TEXT( "Radio_ChebyshevPowerMultiplier" ), 2.0f, TEXT( "A parameter to tweak the radio filter." ), ECVF_Default )->AsVariableFloat();
TConsoleVariableData<float>* FCoreAudioEffectsManager::Radio_ChebyshevPower				= IConsoleManager::Get().RegisterConsoleVariable( TEXT( "Radio_ChebyshevPower" ), 5.0f, TEXT( "A parameter to tweak the radio filter." ), ECVF_Default )->AsVariableFloat();
TConsoleVariableData<float>* FCoreAudioEffectsManager::Radio_ChebyshevCubedMultiplier	= IConsoleManager::Get().RegisterConsoleVariable( TEXT( "Radio_ChebyshevCubedMultiplier" ), 5.0f, TEXT( "A parameter to tweak the radio filter." ), ECVF_Default )->AsVariableFloat();
TConsoleVariableData<float>* FCoreAudioEffectsManager::Radio_ChebyshevMultiplier		= IConsoleManager::Get().RegisterConsoleVariable( TEXT( "Radio_ChebyshevMultiplier" ), 3.0f, TEXT( "A parameter to tweak the radio filter." ), ECVF_Default )->AsVariableFloat();

/**
 * Init all sound effect related code
 */
FCoreAudioEffectsManager::FCoreAudioEffectsManager( FAudioDevice* InDevice )
	: FAudioEffectsManager( InDevice )
{
#if RADIO_ENABLED
	RadioBundle = LoadRadioEffectComponent();
	bRadioAvailable = (RadioBundle != NULL);
#endif
}

FCoreAudioEffectsManager::~FCoreAudioEffectsManager()
{
#if RADIO_ENABLED
	if(RadioBundle)
	{
		CFRelease(RadioBundle);
		RadioBundle = NULL;
	}
#endif
}

/** 
 * Calls the platform specific code to set the parameters that define reverb
 */
void FCoreAudioEffectsManager::SetReverbEffectParameters( const FAudioReverbEffect& ReverbEffectParameters )
{
	float DryWetMix = FMath::Sin(ReverbEffectParameters.Volume*M_PI_2) * 100.0f;										// 0.0-100.0, 100.0
	float SmallLargeMix = ReverbEffectParameters.GainHF * 100.0f;	// 0.0-100.0, 50.0
	float PreDelay = ReverbEffectParameters.ReflectionsDelay;										// 0.001->0.03, 0.025
	float ModulationRate = 1.0f;									// 0.001->2.0, 1.0
	float ModulationDepth = 0.2f;									// 0.0 -> 1.0, 0.2
	float FilterGain = FMath::Clamp<float>( VolumeToDeciBels( ReverbEffectParameters.Gain * ReverbEffectParameters.Volume ), -18.0f, 18.0f );	// -18.0 -> +18.0, 0.0
	
	float LargeDecay = FMath::Clamp<float>( ( ReverbEffectParameters.DecayTime - 1.0f ) * 0.25f, 0.0f, 1.0f );
	float SmallDecay = FMath::Clamp<float>( LargeDecay * ReverbEffectParameters.DecayHFRatio * 0.5f, 0.0f, 1.0f );
	
	float SmallSize = FMath::Max<float>( SmallDecay * 0.05f, 0.001f );					// 0.0001->0.05, 0.0048
	float SmallDensity = ReverbEffectParameters.ReflectionsGain;						// 0->1, 0.28
	float SmallBrightness = FMath::Max<float>(ReverbEffectParameters.Diffusion*ReverbEffectParameters.GainHF, 0.1f);	// 0.1->1, 0.96
	float SmallDelayRange = ReverbEffectParameters.ReflectionsDelay;					// 0->1 0.5
	
	float LargeSize = FMath::Max<float>( LargeDecay * 0.15f, 0.005f );					// 0.005->0.15, 0.04
	float LargeDelay = FMath::Max<float>( ReverbEffectParameters.LateDelay, 0.001 );	// 0.001->0.1, 0.035
	float LargeDensity = ReverbEffectParameters.LateGain;								// 0->1, 0.82
	float LargeDelayRange = 0.3f;														// 0->1, 0.3
	float LargeBrightness = FMath::Max<float>(ReverbEffectParameters.Density*ReverbEffectParameters.Gain, 0.1f);	// 0.1->1, 0.49

	for( uint32 Index = 1; Index < MAX_AUDIOCHANNELS + 1; Index++ )
	{
		FCoreAudioSoundSource *Source = GAudioChannels[Index];
		if( Source && Source->ReverbUnit )
		{
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_DryWetMix, kAudioUnitScope_Global, 0, DryWetMix, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_SmallLargeMix, kAudioUnitScope_Global, 0, SmallLargeMix, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_PreDelay, kAudioUnitScope_Global, 0, PreDelay, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_ModulationRate, kAudioUnitScope_Global, 0, ModulationRate, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_ModulationDepth, kAudioUnitScope_Global, 0, ModulationDepth, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_FilterFrequency, kAudioUnitScope_Global, 0, DEFAULT_HIGH_FREQUENCY, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_FilterGain, kAudioUnitScope_Global, 0, FilterGain, 0);
			
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_SmallSize, kAudioUnitScope_Global, 0, SmallSize, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_SmallDensity, kAudioUnitScope_Global, 0, SmallDensity, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_SmallBrightness, kAudioUnitScope_Global, 0, SmallBrightness, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_SmallDelayRange, kAudioUnitScope_Global, 0, SmallDelayRange, 0);

			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_LargeSize, kAudioUnitScope_Global, 0, LargeSize, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_LargeDelay, kAudioUnitScope_Global, 0, LargeDelay, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_LargeDensity, kAudioUnitScope_Global, 0, LargeDensity, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_LargeDelayRange, kAudioUnitScope_Global, 0, LargeDelayRange, 0);
			AudioUnitSetParameter(Source->ReverbUnit, kReverbParam_LargeBrightness, kAudioUnitScope_Global, 0, LargeBrightness, 0);
		}
	}
}

/** 
 * Calls the platform specific code to set the parameters that define EQ
 */
void FCoreAudioEffectsManager::SetEQEffectParameters( const FAudioEQEffect& EQEffectParameters )
{
	float LowGain = VolumeToDeciBels(EQEffectParameters.LFGain);
	float CenterGain = VolumeToDeciBels(EQEffectParameters.MFGain);
	float HighGain = VolumeToDeciBels(EQEffectParameters.HFGain);

	for( uint32 Index = 1; Index < MAX_AUDIOCHANNELS + 1; Index++ )
	{
		FCoreAudioSoundSource *Source = GAudioChannels[Index];
		if( Source && Source->EQUnit )
		{
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_LowFrequency, kAudioUnitScope_Global, 0, EQEffectParameters.LFFrequency, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_LowGain, kAudioUnitScope_Global, 0, LowGain, 0 );

			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_CenterFreq1, kAudioUnitScope_Global, 0, (EQEffectParameters.MFCutoffFrequency - EQEffectParameters.LFFrequency) / 2.0f, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_CenterGain1, kAudioUnitScope_Global, 0, CenterGain, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_Bandwidth1, kAudioUnitScope_Global, 0, EQEffectParameters.MFBandwidth, 0 );
			
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_CenterFreq2, kAudioUnitScope_Global, 0, EQEffectParameters.MFCutoffFrequency, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_CenterGain2, kAudioUnitScope_Global, 0, CenterGain, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_Bandwidth2, kAudioUnitScope_Global, 0, EQEffectParameters.MFBandwidth, 0 );

			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_CenterFreq3, kAudioUnitScope_Global, 0, (EQEffectParameters.HFFrequency - EQEffectParameters.MFCutoffFrequency) / 2.0f, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_CenterGain3, kAudioUnitScope_Global, 0, CenterGain, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_Bandwidth3, kAudioUnitScope_Global, 0, EQEffectParameters.MFBandwidth, 0 );

			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_HighFrequency, kAudioUnitScope_Global, 0, EQEffectParameters.HFFrequency, 0 );
			AudioUnitSetParameter( Source->EQUnit, kMultibandFilter_HighGain, kAudioUnitScope_Global, 0, HighGain, 0 );
		}
	}
}

/** 
 * Calls the platform specific code to set the parameters that define a radio effect.
 * 
 * @param	RadioEffectParameters	The new parameters for the radio distortion effect. 
 */
void FCoreAudioEffectsManager::SetRadioEffectParameters( const FAudioRadioEffect& RadioEffectParameters )
{
	enum ERadioEffectParams
	{
		RadioParam_ChebyshevPowerMultiplier,
		RadioParam_ChebyshevPower,
		RadioParam_ChebyshevMultiplier,
		RadioParam_ChebyshevCubedMultiplier
	};

	const float ChebyshevPowerMultiplier = Radio_ChebyshevPowerMultiplier->GetValueOnGameThread();
	const float ChebyshevPower = Radio_ChebyshevPower->GetValueOnGameThread();
	const float ChebyshevMultiplier = Radio_ChebyshevMultiplier->GetValueOnGameThread();
	const float ChebyshevCubedMultiplier = Radio_ChebyshevCubedMultiplier->GetValueOnGameThread();

	for( uint32 Index = 1; Index < MAX_AUDIOCHANNELS + 1; Index++ )
	{
		FCoreAudioSoundSource *Source = GAudioChannels[Index];
		if( Source && Source->RadioUnit )
		{
			AudioUnitSetParameter( Source->RadioUnit, RadioParam_ChebyshevPowerMultiplier, kAudioUnitScope_Global, 0, ChebyshevPowerMultiplier, 0 );
			AudioUnitSetParameter( Source->RadioUnit, RadioParam_ChebyshevPower, kAudioUnitScope_Global, 0, ChebyshevPower, 0 );
			AudioUnitSetParameter( Source->RadioUnit, RadioParam_ChebyshevMultiplier, kAudioUnitScope_Global, 0, ChebyshevMultiplier, 0 );
			AudioUnitSetParameter( Source->RadioUnit, RadioParam_ChebyshevCubedMultiplier, kAudioUnitScope_Global, 0, ChebyshevCubedMultiplier, 0 );
		}
	}
}
