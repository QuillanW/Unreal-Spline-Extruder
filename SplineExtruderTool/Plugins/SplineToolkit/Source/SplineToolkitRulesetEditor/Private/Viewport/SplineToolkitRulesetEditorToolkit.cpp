// Fill out your copyright notice in the Description page of Project Settings.


#include "Viewport/SplineToolkitRulesetEditorToolkit.h"

#include "SplineToolkit.h"
#include "SplineToolkitRulesetEditor.h"
#include "SplineToolkitRulesetEditorCommands.h"
#include "SplineToolkitRuleset.h"
#include "Viewport/SplineToolkitRulesetEditorPreviewScene.h"
#include "Viewport/SplineToolkitRulesetEditorViewport.h"

#define LOCTEXT_NAMESPACE "SplineToolkitRulesetEditor"

FSplineToolkitRulesetEditorToolkit::FSplineToolkitRulesetEditorToolkit()
{
	
}

FSplineToolkitRulesetEditorToolkit::~FSplineToolkitRulesetEditorToolkit()
{
}

void FSplineToolkitRulesetEditorToolkit::InitAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, USplineToolkitRuleset* InSplineToolkitRuleset)
{
	BindCommands();
	
	SplineToolkitRuleset = Cast<USplineToolkitRuleset>(InSplineToolkitRuleset);
	
	// Create viewport widget
	PreviewViewportWidget = SNew(SSplineToolkitRulesetViewport, SharedThis(this), CreatePreviewScene());
	
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("SplineToolkitRulesetEditorLayoutv1.0")
	->AddArea
	(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewSplitter()
			->SetSizeCoefficient(0.6f)
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.8f)
				->AddTab("SplineToolkitRulesetViewportTab", ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.2f)
				->AddTab("SplineToolkitRulesetDetailsTab", ETabState::OpenedTab)
			)
		)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.4f)
			->AddTab("OutputLog", ETabState::OpenedTab)
		)
	);
	
	FAssetEditorToolkit::InitAssetEditor(EToolkitMode::Standalone, {}, "SplineToolkitRulesetEditor", Layout, true, true, InSplineToolkitRuleset);
	
	//Add buttons to the Asset Editor
	ExtendToolbars();

	//Focus the viewport on preview bounds
	FocusViewport();
}

void FSplineToolkitRulesetEditorToolkit::BindCommands()
{
	FAssetEditorTemplateCommands::Register();
	
	const FAssetEditorTemplateCommands& Commands = FAssetEditorTemplateCommands::Get();
	
	ToolkitCommands->MapAction(Commands.FocusViewport,
	FExecuteAction::CreateSP(this, &FSplineToolkitRulesetEditorToolkit::FocusViewport));
}

TSharedPtr<FSplineToolkitRulesetPreviewScene> FSplineToolkitRulesetEditorToolkit::CreatePreviewScene()
{
	// Create Preview Scene
	if (!PreviewScene.IsValid())
	{
		PreviewScene = MakeShareable(
			new FSplineToolkitRulesetPreviewScene(
				FPreviewScene::ConstructionValues()
				.AllowAudioPlayback(true)
				.ShouldSimulatePhysics(true)
				.ForceUseMovementComponentInNonGameWorld(true),
				StaticCastSharedRef<FSplineToolkitRulesetEditorToolkit>(AsShared())));
	}

	return PreviewScene;
}

void FSplineToolkitRulesetEditorToolkit::ExtendToolbars()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.BeginSection("ExtendToolbarItem");
			{
				ToolbarBuilder.AddToolBarButton(
					FAssetEditorTemplateCommands::Get().FocusViewport,
					NAME_None,
					LOCTEXT("FocusViewport", "Focus Viewport"),
					LOCTEXT("FocusViewportTooltip", "Focuses Viewport on selected Mesh"),
					FSlateIcon()
				);
				
			}
			ToolbarBuilder.EndSection();
		}
	};
	
	//Register Toolbar Extenders
	const TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar)
	);
	
	AddToolbarExtender(ToolbarExtender);
	
	FSplineToolkitRulesetEditorModule* AssetEditorTemplateModule = &FModuleManager::LoadModuleChecked<FSplineToolkitRulesetEditorModule>("AssetEditorTemplateEditor");
	AddToolbarExtender(AssetEditorTemplateModule->GetEditorToolbarExtensibilityManager()->GetAllExtenders());

	RegenerateMenusAndToolbars();
}

void FSplineToolkitRulesetEditorToolkit::FocusViewport() const
{
	if(PreviewViewportWidget.IsValid())
	{
		PreviewViewportWidget->OnFocusViewportToSelection();
	}
}

void FSplineToolkitRulesetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	//Register Viewport
	InTabManager->RegisterTabSpawner(
	"SplineToolkitRulesetViewportTab",
	FOnSpawnTab::CreateSP(this, &FSplineToolkitRulesetEditorToolkit::SpawnTab_Viewport))
	.SetDisplayName(LOCTEXT("PreviewSceneViewport", "Preview Viewport"))
	.SetGroup(WorkspaceMenuCategory.ToSharedRef())
	.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));
	
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(INVTEXT("Simple Asset Editor"));
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	
	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObjects(TArray<UObject*>{ SplineToolkitRuleset });
	
	InTabManager->RegisterTabSpawner("SplineToolkitRulesetDetailsTab", FOnSpawnTab::CreateLambda([=](const FSpawnTabArgs&)
	{
		return SNew(SDockTab)
		[
			DetailsView
		];
	}))
	.SetDisplayName(INVTEXT("Details"))
	.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	
}

void FSplineToolkitRulesetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner("SplineToolkitRulesetDetailsTab");
	InTabManager->UnregisterTabSpawner("SplineToolkitRulesetViewportTab");
}

TSharedRef<SDockTab> FSplineToolkitRulesetEditorToolkit::SpawnTab_Viewport(const FSpawnTabArgs& Args) const
{
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab).Label(LOCTEXT("ViewportTab_Title", "Viewport"));

	if (PreviewViewportWidget.IsValid())
	{
		SpawnedTab->SetContent(PreviewViewportWidget.ToSharedRef());
	}

	return SpawnedTab;
}


void FSplineToolkitRulesetEditorToolkit::OnClose()
{
	SplineToolkitRuleset = nullptr;

	if(PreviewScene.IsValid())
	{
		PreviewScene.Reset();
	}
		
	if(PreviewViewportWidget.IsValid())
	{
		PreviewViewportWidget.Reset();
	}
}

#undef LOCTEXT_NAMESPACE