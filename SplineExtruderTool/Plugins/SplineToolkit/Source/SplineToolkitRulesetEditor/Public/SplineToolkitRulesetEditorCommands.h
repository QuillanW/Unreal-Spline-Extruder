// Based on:
// https://github.com/shaderwitch/AssetEditorTemplate

#pragma once

#include "Styling/AppStyle.h"
#include "Framework/Commands/Commands.h"
#include "Templates/SharedPointer.h"

class FUICommandInfo;

class FAssetEditorTemplateCommands : public TCommands<FAssetEditorTemplateCommands>
{
public:
	/** Constructor */
	FAssetEditorTemplateCommands() 
		: TCommands<FAssetEditorTemplateCommands>("AssetEditorTemplateEditor", NSLOCTEXT("Contexts", "AssetTemplateEditor", "Asset Editor Template Editor"), NAME_None, FAppStyle::GetAppStyleSetName())
	{
		
	}
	
	/** Focuses Viewport on Mesh */
	TSharedPtr<FUICommandInfo> FocusViewport;
	TSharedPtr<FUICommandInfo> ToggleAutoUpdate;
	
	TSharedPtr<FUICommandInfo> SelectPreviewTrack;
	TSharedPtr<FUICommandInfo> SelectPreviewLoop;
	TSharedPtr<FUICommandInfo> SelectPreviewBend;
	
	/** Initialize commands */
	virtual void RegisterCommands() override;
};