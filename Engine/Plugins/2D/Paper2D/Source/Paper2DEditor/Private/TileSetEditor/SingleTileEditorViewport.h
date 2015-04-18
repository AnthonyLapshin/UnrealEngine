// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SEditorViewport.h"

class FSingleTileEditorViewportClient;
class FEdModeTileMap;

#include "Editor/UnrealEd/Public/SCommonEditorViewportToolbarBase.h"

//////////////////////////////////////////////////////////////////////////
// SSingleTileEditorViewport

class SSingleTileEditorViewport : public SEditorViewport, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS(SSingleTileEditorViewport) {}
	SLATE_END_ARGS()

	~SSingleTileEditorViewport();

	void Construct(const FArguments& InArgs, TSharedPtr<FSingleTileEditorViewportClient> InViewportClient);

	// SEditorViewport interface
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	// End of SEditorViewport interface

	// ICommonEditorViewportToolbarInfoProvider interface
	virtual TSharedRef<SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	// End of ICommonEditorViewportToolbarInfoProvider interface

protected:
	FText GetTitleText() const;

	bool IsVisible() const;

private:
	TSharedPtr<FSingleTileEditorViewportClient> TypedViewportClient;
	FEdModeTileMap* TileMapEditor;
};