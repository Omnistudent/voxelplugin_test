// Copyright 2020 Phyronnaz

#include "VoxelMaterialCollectionDetails.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"

#include "VoxelGlobals.h"
#include "VoxelEditorDetailsUtilities.h"
#include "VoxelRender/VoxelMaterialCollection.h"
#include "Materials/Material.h"
#include "VoxelMaterialCollectionHelpers.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "Misc/MessageDialog.h"
#include "DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "Voxel"

TSharedRef<IDetailCustomization> FVoxelMaterialCollectionDetails::MakeInstance()
{
	return MakeShareable(new FVoxelMaterialCollectionDetails());
}

FVoxelMaterialCollectionDetails::FVoxelMaterialCollectionDetails()
{

}

void FVoxelMaterialCollectionDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}
	Collection = CastChecked<UVoxelMaterialCollection>(Objects[0].Get());;

	FVoxelEditorUtilities::AddButtonToCategory(DetailLayout,
		"Generate",
		LOCTEXT("GenerateSingle", "Generate Single"),
		LOCTEXT("GenerateSingleMaterials", "Generate Single Materials"),
		LOCTEXT("GenerateSingle", "Generate Single"),
		false,
		FOnClicked::CreateSP(this, &FVoxelMaterialCollectionDetails::OnGenerateSingleMaterials));

	FVoxelEditorUtilities::AddButtonToCategory(DetailLayout,
		"Generate",
		LOCTEXT("GenerateDouble", "Generate Double"),
		LOCTEXT("GenerateDoubleMaterials", "Generate Double Materials"),
		LOCTEXT("GenerateDouble", "Generate Double"),
		false,
		FOnClicked::CreateSP(this, &FVoxelMaterialCollectionDetails::OnGenerateDoubleMaterials));

	FVoxelEditorUtilities::AddButtonToCategory(DetailLayout,
		"Generate",
		LOCTEXT("GenerateTriple", "Generate Triple"),
		LOCTEXT("GenerateTripleMaterials", "Generate Triple Materials"),
		LOCTEXT("GenerateTriple", "Generate Triple"),
		false,
		FOnClicked::CreateSP(this, &FVoxelMaterialCollectionDetails::OnGenerateTripleMaterials));
}


FReply FVoxelMaterialCollectionDetails::OnGenerateSingleMaterials()
{
	FString Error;
	if (FVoxelMaterialCollectionHelpers::GenerateSingleMaterials(Collection.Get(), Error))
	{
		FNotificationInfo Info(LOCTEXT("Success", "Success"));
		Info.ExpireDuration = 5.f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error));
		return FReply::Handled();
	}

	return FReply::Handled();
}


FReply FVoxelMaterialCollectionDetails::OnGenerateDoubleMaterials()
{
	FString Error;
	if (FVoxelMaterialCollectionHelpers::GenerateDoubleMaterials(Collection.Get(), Error))
	{
		FNotificationInfo Info(LOCTEXT("Success", "Success"));
		Info.ExpireDuration = 5.f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error));
		return FReply::Handled();
	}

	return FReply::Handled();
}

FReply FVoxelMaterialCollectionDetails::OnGenerateTripleMaterials()
{
	auto ThisWillTakeAWhileValue = FMessageDialog::Open(EAppMsgType::YesNoCancel, LOCTEXT("ThisWillTakeAWhile", "This will take a while! Triple materials are only needed when 3 different materials are on a same triangle, which is really rare. You don't need them to test your materials. Do you want to continue?"));

    switch(ThisWillTakeAWhileValue)
    {
    case EAppReturnType::Yes:
        break;
    case EAppReturnType::No:
    case EAppReturnType::Cancel:
        return FReply::Handled();
    }
	
	FString Error;
	if (FVoxelMaterialCollectionHelpers::GenerateTripleMaterials(Collection.Get(), Error))
	{
		FNotificationInfo Info(LOCTEXT("Success", "Success"));
		Info.ExpireDuration = 5.f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error));
		return FReply::Handled();
	}

	return FReply::Handled();
}

void FVoxelMaterialCollectionElementCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyHandle = InPropertyHandle;
	PropertyHandle->GetOuterObjects(Outers);

	IndexHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_STATIC(FVoxelMaterialCollectionElement, Index));
	MaterialHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_STATIC(FVoxelMaterialCollectionElement, MaterialFunction));
	PhysicalMaterialHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_STATIC(FVoxelMaterialCollectionElement, PhysicalMaterial));
	ChildrenHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_STATIC(FVoxelMaterialCollectionElement, Children));
		   
	auto IndexWidget = IndexHandle->CreatePropertyValueWidget();
	IndexWidget->SetVisibility(TAttribute<EVisibility>(this, &FVoxelMaterialCollectionElementCustomization::NoChildrenOnlyVisibility));
		
	HeaderRow
	.NameContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			IndexWidget
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SSpacer)
		]
	]
	.ValueContent()
	.HAlign(HAlign_Fill)
	.MaxDesiredWidth(TOptional<float>())
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				MaterialHandle->CreatePropertyValueWidget()
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(SSpacer)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(TAttribute<float>(this, &FVoxelMaterialCollectionElementCustomization::NoChildrenOnlyPhysicalMaterialsMaxHeight))
		[
			SNew(SHorizontalBox)
			.Visibility_Raw(this, &FVoxelMaterialCollectionElementCustomization::NoChildrenOnlyPhysicalMaterialsVisibility)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				PhysicalMaterialHandle->CreatePropertyValueWidget()
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(SSpacer)
			]
		]
	];
}

void FVoxelMaterialCollectionElementCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	ChildrenHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_STATIC(FVoxelMaterialCollectionElement, Children));

	TSharedRef<FDetailArrayBuilder> NodeArrayBuilder = MakeShareable(new FDetailArrayBuilder(ChildrenHandle.ToSharedRef()));
	NodeArrayBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FVoxelMaterialCollectionElementCustomization::GenerateArrayElementWidget));
	ChildBuilder.AddCustomBuilder(NodeArrayBuilder);
}

bool FVoxelMaterialCollectionElementCustomization::ArePhysicalMaterialsHidden() const
{
	for (auto* Outer : Outers)
	{
		if (auto* Collection = Cast<UVoxelMaterialCollection>(Outer))
		{
			if (Collection->bHidePhysicalMaterials)
			{
				return true;
			}
		}
	}

	return false;
}

bool FVoxelMaterialCollectionElementCustomization::HasChildren() const
{
	uint32 Num;
	ChildrenHandle->AsArray()->GetNumElements(Num);
	return Num > 0;
}

void FVoxelMaterialCollectionElementCustomization::GenerateArrayElementWidget(TSharedRef<IPropertyHandle> ChildHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
{
	auto ChildIndexHandle = ChildHandle->GetChildHandle(GET_MEMBER_NAME_STATIC(FVoxelMaterialCollectionElementIndex, InstanceIndex));
	auto ChildInstanceHandle = ChildHandle->GetChildHandle(GET_MEMBER_NAME_STATIC(FVoxelMaterialCollectionElementIndex, MaterialInstance));
	auto ChildPhysicalMaterialHandle = ChildHandle->GetChildHandle(GET_MEMBER_NAME_STATIC(FVoxelMaterialCollectionElementIndex, PhysicalMaterial));

	ChildrenBuilder.AddCustomRow(FText())
	.NameContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			ChildIndexHandle->CreatePropertyValueWidget()
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SSpacer)
		]
	]
	.ValueContent()
	.HAlign(HAlign_Fill)
	.MaxDesiredWidth(TOptional<float>())
	[
		SNew(SHorizontalBox)
	    + SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					ChildInstanceHandle->CreatePropertyValueWidget()
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew(SSpacer)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(TAttribute<float>(this, &FVoxelMaterialCollectionElementCustomization::PhysicalMaterialsMaxHeight))
			[
				SNew(SHorizontalBox)
				.Visibility_Raw(this, &FVoxelMaterialCollectionElementCustomization::PhysicalMaterialsVisibility)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					ChildPhysicalMaterialHandle->CreatePropertyValueWidget()
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew(SSpacer)
				]
			]
		]
	    + SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(2.0f, 1.0f)
		[
			PropertyCustomizationHelpers::MakeInsertDeleteDuplicateButton(
				FExecuteAction::CreateLambda([=]() { ChildrenHandle->AsArray()->Insert(ArrayIndex); }),
				FExecuteAction::CreateLambda([=]() { ChildrenHandle->AsArray()->DeleteItem(ArrayIndex); }), 
				FExecuteAction::CreateLambda([=]() { ChildrenHandle->AsArray()->DuplicateItem(ArrayIndex); }))
		]
	];
}

#undef LOCTEXT_NAMESPACE
