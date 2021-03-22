// Copyright 2020 Phyronnaz

#include "AssetTypeActions_VoxelDataAsset.h"
#include "VoxelEditorModule.h"
#include "VoxelMessages.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UIAction.h"
#include "EditorStyleSet.h"
#include "EditorReimportHandler.h"

#define LOCTEXT_NAMESPACE "Voxel"

void FAssetTypeActions_VoxelDataAsset::OpenAssetEditor(const TArray<UObject *>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	FVoxelMessages::ShowVoxelPluginProError("The Voxel Data Asset Editor is only available in Voxel Plugin Pro");
}

void FAssetTypeActions_VoxelDataAsset::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	const auto Assets = GetTypedWeakObjectPtrs<UVoxelDataAsset>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ReimportVoxelAssetLabel", "Reimport"),
		LOCTEXT("ReimportVoxelAssetTooltip", "Reimport the selected asset(s)."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_VoxelDataAsset::ExecuteReimport, Assets),
			FCanExecuteAction::CreateSP(this, &FAssetTypeActions_VoxelDataAsset::CanExecuteReimport, Assets)
		)
	);
}

bool FAssetTypeActions_VoxelDataAsset::CanExecuteReimport(const TArray<TWeakObjectPtr<UVoxelDataAsset>> Objects) const
{
	for (auto& Object : Objects)
	{
		if (Object.IsValid() && (Object->Source == EVoxelDataAssetImportSource::MagicaVox || Object->Source == EVoxelDataAssetImportSource::RawVox))
		{
			return true;
		}
	}
	return false;
}

void FAssetTypeActions_VoxelDataAsset::ExecuteReimport(const TArray<TWeakObjectPtr<UVoxelDataAsset>> Objects) const
{
	for (auto& Object : Objects)
	{
		if (Object.IsValid() && (Object->Source == EVoxelDataAssetImportSource::MagicaVox || Object->Source == EVoxelDataAssetImportSource::RawVox))
		{
			FReimportManager::Instance()->Reimport(Object.Get(), /*bAskForNewFileIfMissing=*/true);
		}
	}
}
#undef LOCTEXT_NAMESPACE