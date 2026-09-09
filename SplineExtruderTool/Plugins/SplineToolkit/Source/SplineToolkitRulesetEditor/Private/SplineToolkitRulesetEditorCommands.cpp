// Fill out your copyright notice in the Description page of Project Settings.


#include "SplineToolkitRulesetEditorCommands.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "FAssetEditorTemplateCommands"

void FAssetEditorTemplateCommands::RegisterCommands()
{
	UI_COMMAND(FocusViewport, "Focus Viewport", "Focus Viewport on Mesh", EUserInterfaceActionType::Button, FInputChord(EKeys::F));
	UI_COMMAND(ToggleAutoUpdate, "Update Preview", "Toggle whether the preview should update when settings are changed", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::T));
	
	UI_COMMAND(SelectPreviewTrack, "Track", "Change the spline used for the preview", EUserInterfaceActionType::RadioButton, FInputChord(EKeys::P));
	UI_COMMAND(SelectPreviewLoop, "Loop", "Change the spline used for the preview", EUserInterfaceActionType::RadioButton, FInputChord(EKeys::O));
	UI_COMMAND(SelectPreviewBend, "S-Bend", "Change the spline used for the preview", EUserInterfaceActionType::RadioButton, FInputChord(EKeys::I));
}

#undef LOCTEXT_NAMESPACE