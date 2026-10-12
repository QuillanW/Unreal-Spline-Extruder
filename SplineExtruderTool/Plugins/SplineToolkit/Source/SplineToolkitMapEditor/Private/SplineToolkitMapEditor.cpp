// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineToolkitMapEditor.h"

#include "PropertyCustomizationHelpers.h"
#include "Components/SplineComponent.h"

#define LOCTEXT_NAMESPACE "FSplineToolkitMapEditorModule"

const FEditorModeID USplineToolkitEditMode::EM_USplineToolkitEditModeID = TEXT("EM_USplineToolkitEditModeID");
	
IMPLEMENT_MODULE(FSplineToolkitMapEditorModule, SplineToolkitMapEditor)

void USplineToolkitEditMode::Enter()
{
	Super::Enter();
}

void USplineToolkitEditMode::Exit()
{
	Super::Exit();
}

bool USplineToolkitEditMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy,
	const FViewportClick& Click)
{
	if (CurrentEditMode == ESplineToolkitEditorModes::None)
		return Super::HandleClick(InViewportClient, HitProxy, Click);
	
	if (Click.GetKey() != EKeys::LeftMouseButton) return false;

	const FVector Start = Click.GetOrigin();
	const FVector End = Start + Click.GetDirection() * 1000000.0;

	UWorld* World = GetWorld();
	FHitResult Hit;
	FVector ClickedPos;
	if (World && World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility))
		ClickedPos = Hit.ImpactPoint;
	else
		return false;
	
	switch (CurrentEditMode)
	{
	case ESplineToolkitEditorModes::Select:
		SelectClosestSpline(ClickedPos);
		break;
	case ESplineToolkitEditorModes::Connect:
		ConnectSplines(ClickedPos);
		break;
	case ESplineToolkitEditorModes::Disconnect:
		DisconnectSplines(ClickedPos);
		break;
	case ESplineToolkitEditorModes::UpdateRuleset:
		UpdateRuleset(ClickedPos);
		break;
	case ESplineToolkitEditorModes::None:
		break;
	}
	
	return true;
}

bool USplineToolkitEditMode::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key,
	EInputEvent Event)
{
	// Hotkeys for different edit modes
	if (Event == IE_Pressed)
	{
		if (Key == EKeys::One)   { CurrentEditMode = ESplineToolkitEditorModes::Select;        return true; }
		if (Key == EKeys::Two)   { CurrentEditMode = ESplineToolkitEditorModes::Connect;       return true; }
		if (Key == EKeys::Three) { CurrentEditMode = ESplineToolkitEditorModes::Disconnect;    return true; }
		if (Key == EKeys::Four)  { CurrentEditMode = ESplineToolkitEditorModes::UpdateRuleset; return true; }
	}
	
	return Super::InputKey(ViewportClient, Viewport, Key, Event);
}

USplineToolkitEditMode::USplineToolkitEditMode()
{
	Info = FEditorModeInfo(EM_USplineToolkitEditModeID, INVTEXT("Spline Editor"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.SplineComponent"), true);
}

static bool HasSpline(const AActor* Actor)
{
	return Actor && Actor->FindComponentByClass<USplineComponent>() != nullptr;
}

bool USplineToolkitEditMode::IsSelectionAllowed(AActor* InActor, bool bInSelection) const
{
	// Always allow deselecting, only allow selecting actors that have a spline
	return !bInSelection || HasSpline(InActor);
}

bool USplineToolkitEditMode::IsSelectionDisallowed(AActor* InActor, bool bInSelection) const
{
	return bInSelection && !HasSpline(InActor);
}

void USplineToolkitEditMode::SetEditMode(ESplineToolkitEditorModes Mode)
{
	CurrentEditMode = Mode;
	SelectedSpline = nullptr;
	LastSelectedSpline = nullptr;
}

void USplineToolkitEditMode::CreateToolkit()
{
	Toolkit = MakeShared<FSplineToolkitModeToolkit>(this);
}

TSharedPtr<SWidget> FSplineToolkitModeToolkit::GetInlineContent() const
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().MinHeight(16.f).Padding(4.f)[ MakeButton(INVTEXT("Select"), ESplineToolkitEditorModes::Select) ]
		+ SVerticalBox::Slot().MinHeight(16.f).Padding(4.f)[ MakeButton(INVTEXT("Connect"), ESplineToolkitEditorModes::Connect) ]
		+ SVerticalBox::Slot().MinHeight(16.f).Padding(4.f)[ MakeButton(INVTEXT("Disconnect"), ESplineToolkitEditorModes::Disconnect) ]
		+ SVerticalBox::Slot().MinHeight(16.f).Padding(4.f)[ MakeButton(INVTEXT("Update Ruleset"), ESplineToolkitEditorModes::UpdateRuleset) ]
		+ SVerticalBox::Slot().MinHeight(16.f).Padding(4.f)[ MakeRulesetPicker() ];
}

TSharedRef<SWidget> FSplineToolkitModeToolkit::MakeButton(const FText& Label, ESplineToolkitEditorModes BtnMode) const
{
	TWeakObjectPtr<USplineToolkitEditMode> WeakMode = Mode;

	return SNew(SButton)
		.Text(Label)
		.ButtonColorAndOpacity_Lambda([WeakMode, BtnMode]()
		{
			const bool bActive = WeakMode.IsValid() && WeakMode->CurrentMode() == BtnMode;
			return bActive ? FLinearColor(0.2f, 0.5f, 1.0f) : FLinearColor::White;
		})
		.OnClicked_Lambda([WeakMode, BtnMode]()
		{
			UE_LOG(LogTemp, Warning, TEXT("Button clicked, mode valid: %d"), WeakMode.IsValid());
			if (WeakMode.IsValid())
				WeakMode->SetEditMode(BtnMode);
			return FReply::Handled();
		});
}

TSharedRef<SWidget> FSplineToolkitModeToolkit::MakeRulesetPicker() const
{
	TWeakObjectPtr<USplineToolkitEditMode> WeakMode = Mode;

	return SNew(SObjectPropertyEntryBox)
		.AllowedClass(USplineToolkitRuleset::StaticClass())
		.AllowClear(true)
		.DisplayThumbnail(false)
		.ObjectPath_Lambda([WeakMode]() -> FString
		{
			return (WeakMode.IsValid() && WeakMode->SelectedRuleset)
				? FSoftObjectPath(WeakMode->SelectedRuleset).ToString()
				: FString();
		})
		.OnObjectChanged_Lambda([WeakMode](const FAssetData& AssetData)
		{
			if (WeakMode.IsValid())
			{
				WeakMode->SelectedRuleset = Cast<USplineToolkitRuleset>(AssetData.GetAsset());
			}
		});
}


#undef LOCTEXT_NAMESPACE
