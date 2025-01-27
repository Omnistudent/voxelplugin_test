// Copyright 2020 Phyronnaz

#include "VoxelEditorModule.h"

#include "Interfaces/IPluginManager.h"
#include "IPlacementModeModule.h"
#include "PropertyEditorModule.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "EditorModeRegistry.h"
#include "EditorSupportDelegates.h"
#include "LevelEditor.h"
#include "EngineUtils.h"
#include "MessageLogModule.h"
#include "EditorReimportHandler.h"
#include "Framework/Commands/Commands.h"

#include "VoxelGraphGenerator.h"
#include "VoxelWorld.h"
#include "VoxelTexture.h"
#include "VoxelPlaceableItems/VoxelDisableEditsBox.h"
#include "VoxelPlaceableItems/VoxelAssetActor.h"
#include "VoxelMessages.h"
#include "VoxelMessagesEditor.h"

#include "AssetTools/AssetTypeActions_VoxelDataAsset.h"
#include "AssetTools/AssetTypeActions_VoxelHeightmapAsset.h"
#include "AssetTools/AssetTypeActions_VoxelGraphWorldGenerator.h"
#include "AssetTools/AssetTypeActions_VoxelGraphOutputsConfig.h"
#include "AssetTools/AssetTypeActions_VoxelSpawnerConfig.h"
#include "AssetTools/AssetTypeActions_VoxelSpawners.h"
#include "AssetTools/AssetTypeActions_VoxelGraphMacro.h"
#include "AssetTools/AssetTypeActions_VoxelWorldSaveObject.h"
#include "AssetTools/AssetTypeActions_VoxelMaterialCollection.h"

#include "Thumbnails/VoxelGraphGeneratorThumbnailRenderer.h"
#include "Thumbnails/VoxelSpawnersThumbnailRenderer.h"
#include "Thumbnails/VoxelDataAssetThumbnailRenderer.h"
#include "Thumbnails/VoxelHeightmapAssetThumbnailRenderer.h"

#include "EdMode/VoxelEdMode.h"

#include "ActorFactoryVoxelWorld.h"
#include "ActorFactoryVoxelPlaceableItems.h"
#include "ActorFactoryVoxelMeshImporter.h"

#include "Details/VoxelWorldDetails.h"
#include "Details/VoxelLandscapeImporterDetails.h"
#include "Details/VoxelMeshImporterDetails.h"
#include "Details/VoxelAssetActorDetails.h"
#include "Details/VoxelWorldGeneratorPickerCustomization.h"
#include "Details/VoxelMaterialCollectionDetails.h"
#include "Details/VoxelMaterialCollectionHelpers.h"
#include "Details/RangeAnalysisDebuggerDetails.h"
#include "Details/VoxelPaintMaterialCustomization.h"
#include "Details/VoxelMeshSpawnerBaseDetails.h"
#include "Details/VoxelBasicSpawnerScaleSettingsCustomization.h"
#include "Details/VoxelSpawnerOutputNameCustomization.h"
#include "Details/VoxelGraphOutputCustomization.h"
#include "Details/BoolVectorCustomization.h"

#include "VoxelImporters/VoxelMeshImporter.h"
#include "VoxelImporters/VoxelLandscapeImporter.h"

#define LOCTEXT_NAMESPACE "Voxel"

const FVector2D Icon14x14(14.0f, 14.0f);
const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);
const FVector2D Icon64x64(64.0f, 64.0f);
const FVector2D Icon512x512(512.0f, 512.0f);

class FVoxelEditorCommands : public TCommands<FVoxelEditorCommands>
{
public:
	FVoxelEditorCommands()
		: TCommands<FVoxelEditorCommands>
		(
		"VoxelEditor", // Context name for icons
		NSLOCTEXT("Contexts", "VoxelEditor", "Voxel Editor"), // Localized context name for displaying
		NAME_None, // Parent
		"VoxelStyle" // Icon Style Set
		)
	{
	}
	
	TSharedPtr<FUICommandInfo> RefreshVoxelWorlds;

	virtual void RegisterCommands() override
	{
		UI_COMMAND(
			RefreshVoxelWorlds, 
			"Retoggle", 
			"Retoggle the voxel worlds", 
			EUserInterfaceActionType::Button, 
			FInputChord(EModifierKey::Control, EKeys::F5));
	}
};

static void RefreshVoxelWorlds_Execute(UObject* MatchingGenerator = nullptr)
{
	FViewport* Viewport = GEditor->GetActiveViewport();
	if (Viewport)
	{
		FViewportClient* Client = Viewport->GetClient();
		if (Client)
		{
			UWorld* World = Client->GetWorld();
			if (World && (World->WorldType == EWorldType::Editor || World->WorldType == EWorldType::EditorPreview))
			{
				for (TActorIterator<AVoxelWorld> It(World); It; ++It)
				{
					if (It->IsCreated() && (!MatchingGenerator || It->WorldGenerator.GetObject() == MatchingGenerator))
					{
						It->Toggle();
						It->Toggle();
					}
				}
				for (TActorIterator<AVoxelAssetActor> It(World); It; ++It)
				{
					It->UpdatePreview();
				}
			}
		}
	}
}

static void BindEditorDelegates(IVoxelEditorDelegatesInterface* Interface, UObject* Object)
{
	check(Interface && Object);
	
	if (!FEditorDelegates::PreSaveWorld.IsBoundToObject(Object))
	{
		FEditorDelegates::PreSaveWorld.AddWeakLambda(Object, [=](uint32 SaveFlags, UWorld* World) { Interface->OnPreSaveWorld(SaveFlags, World); });
	}
	if (!FEditorDelegates::PreBeginPIE.IsBoundToObject(Object))
	{
		FEditorDelegates::PreBeginPIE.AddWeakLambda(Object, [=](bool bIsSimulating) { Interface->OnPreBeginPIE(bIsSimulating); });
	}
	if (!FEditorDelegates::EndPIE.IsBoundToObject(Object))
	{
		FEditorDelegates::EndPIE.AddWeakLambda(Object, [=](bool bIsSimulating) { Interface->OnEndPIE(bIsSimulating); });
	}
	if (!FEditorSupportDelegates::PrepareToCleanseEditorObject.IsBoundToObject(Object))
	{
		FEditorSupportDelegates::PrepareToCleanseEditorObject.AddWeakLambda(Object, [=](UObject* InObject) { Interface->OnPrepareToCleanseEditorObject(InObject); });
	}
	if (!FCoreDelegates::OnPreExit.IsBoundToObject(Object))
	{
		FCoreDelegates::OnPreExit.AddWeakLambda(Object, [=]() { Interface->OnPreExit(); });
	}
}

/**
 * Implements the VoxelEditor module.
 */
class FVoxelEditorModule : public IVoxelEditorModule
{
public:
	virtual void StartupModule() override
	{
		// Clear texture cache on reimport
		FReimportManager::Instance()->OnPostReimport().AddLambda([](UObject*, bool) { FVoxelTextureUtilities::ClearCache(); });
		
		// Global commands
		FVoxelEditorCommands::Register();

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetGlobalLevelEditorActions()->MapAction(
			FVoxelEditorCommands::Get().RefreshVoxelWorlds,
			FExecuteAction::CreateStatic(&RefreshVoxelWorlds_Execute, (UObject*)nullptr),
			FCanExecuteAction());

		IVoxelEditorDelegatesInterface::BindEditorDelegatesDelegate.AddStatic(&BindEditorDelegates);

		// Blueprint errors
		FVoxelMessages::LogMessageDelegate.AddStatic(&FVoxelMessagesEditor::LogMessage);
		FVoxelMessages::ShowNotificationDelegate.AddStatic(&FVoxelMessagesEditor::ShowNotification);

		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		FMessageLogInitializationOptions InitOptions;
		InitOptions.bShowFilters = true;
		InitOptions.bShowPages = false;
		InitOptions.bAllowClear = true;
		MessageLogModule.RegisterLogListing("Voxel", LOCTEXT("Voxel", "Voxel"), InitOptions);

		// Voxel asset category
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		VoxelAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory("Voxel", LOCTEXT("VoxelAssetCategory", "Voxel"));

		RegisterPlacementModeExtensions();
		RegisterCustomClassLayouts();
		RegisterAssetTools();
		
		// Thumbnails
		auto& ThumbnailManager = UThumbnailManager::Get();
		ThumbnailManager.RegisterCustomRenderer(UVoxelGraphGenerator  ::StaticClass(), UVoxelGraphGeneratorThumbnailRenderer  ::StaticClass());
		ThumbnailManager.RegisterCustomRenderer(UVoxelDataAsset       ::StaticClass(), UVoxelDataAssetThumbnailRenderer       ::StaticClass());
		ThumbnailManager.RegisterCustomRenderer(UVoxelHeightmapAsset  ::StaticClass(), UVoxelHeightmapAssetThumbnailRenderer  ::StaticClass());
		ThumbnailManager.RegisterCustomRenderer(UVoxelMeshSpawner     ::StaticClass(), UVoxelMeshSpawnerThumbnailRenderer     ::StaticClass());
		ThumbnailManager.RegisterCustomRenderer(UVoxelMeshSpawnerGroup::StaticClass(), UVoxelMeshSpawnerGroupThumbnailRenderer::StaticClass());
		ThumbnailManager.RegisterCustomRenderer(UVoxelAssetSpawner    ::StaticClass(), UVoxelAssetSpawnerThumbnailRenderer    ::StaticClass());
		ThumbnailManager.RegisterCustomRenderer(UVoxelSpawnerGroup    ::StaticClass(), UVoxelSpawnerGroupThumbnailRenderer    ::StaticClass());

		// Icons
		{
			FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("Voxel"))->GetContentDir() + "/";

			StyleSet = MakeShareable(new FSlateStyleSet("VoxelStyle"));
			StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
			StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

			// VoxelWorld
			StyleSet->Set("ClassThumbnail.VoxelWorld"                        , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/World_64x.png"), Icon64x64));
			StyleSet->Set("ClassIcon.VoxelWorld"                             , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/World_16x.png"), Icon16x16));
																		     
			// Voxel Material Collection								     
			StyleSet->Set("ClassThumbnail.VoxelMaterialCollection"           , new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("Icons/AssetIcons/PaperTileMap_64x.png")), Icon64x64));
			StyleSet->Set("ClassIcon.VoxelMaterialCollection"                , new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("Icons/AssetIcons/PaperTileMap_16x.png")), Icon16x16));
			StyleSet->Set("ClassThumbnail.VoxelBasicMaterialCollection"      , new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("Icons/AssetIcons/PaperTileMap_64x.png")), Icon64x64));
			StyleSet->Set("ClassIcon.VoxelBasicMaterialCollection"           , new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("Icons/AssetIcons/PaperTileMap_16x.png")), Icon16x16));

			// Importers
			StyleSet->Set("ClassThumbnail.VoxelLandscapeImporter"            , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/Import_64x.png"), Icon64x64));
			StyleSet->Set("ClassIcon.VoxelLandscapeImporter"                 , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/Import_16x.png"), Icon16x16));
			StyleSet->Set("ClassThumbnail.VoxelMeshImporter"                 , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/Import_64x.png"), Icon64x64));
			StyleSet->Set("ClassIcon.VoxelMeshImporter"                      , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/Import_16x.png"), Icon16x16));
			
			// Spawners
			StyleSet->Set("ClassThumbnail.VoxelSpawnerConfig"                , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/SpawnerConfig_64x.png"), Icon64x64));
			StyleSet->Set("ClassIcon.VoxelSpawnerConfig"                     , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/SpawnerConfig_16x.png"), Icon16x16));
			StyleSet->Set("ClassThumbnail.VoxelSpawner"                      , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/Spawner_64x.png")  , Icon64x64));
			StyleSet->Set("ClassIcon.VoxelSpawner"                           , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/Spawner_16x.png")  , Icon16x16));
			StyleSet->Set("ClassThumbnail.VoxelSpawnerGroup"                 , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/SpawnerGroup_64x.png")  , Icon64x64));
			StyleSet->Set("ClassIcon.VoxelSpawnerGroup"                      , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/SpawnerGroup_16x.png")  , Icon16x16));
			StyleSet->Set("ClassThumbnail.VoxelMeshSpawnerGroup"             , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/SpawnerGroup_64x.png")  , Icon64x64));
			StyleSet->Set("ClassIcon.VoxelMeshSpawnerGroup"                  , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/SpawnerGroup_16x.png")  , Icon16x16));
																		     
			// Voxel Graph												     
			StyleSet->Set("ClassThumbnail.VoxelGraphGenerator"               , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/VoxelGraph_64x.png"), Icon64x64));
			StyleSet->Set("ClassIcon.VoxelGraphGenerator"                    , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/VoxelGraph_16x.png"), Icon16x16));
																		     
			// Data Asset												     
			StyleSet->Set("ClassThumbnail.VoxelDataAsset"                    , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/DataAsset_64x.png"), Icon64x64));
			StyleSet->Set("ClassIcon.VoxelDataAsset"                         , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/DataAsset_16x.png"), Icon16x16));
																		     
			// Landscape asset											     
			StyleSet->Set("ClassThumbnail.VoxelLandscapeAsset"               , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/Landscape_64x.png"), Icon64x64));
			StyleSet->Set("ClassIcon.VoxelLandscapeAsset"                    , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/Landscape_16x.png"), Icon16x16));	
																		     
			// Data Asset Editor										     
			StyleSet->Set("VoxelDataAssetEditor.InvertDataAsset"             , new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("Icons/icon_rotateb_40x.png")), Icon40x40));
																		     
			// Voxel Editor Tools										     
			StyleSet->Set("VoxelTools.Tab"                                   , new FSlateImageBrush(ContentDir + TEXT("Icons/UIIcons/mode_40.png"), Icon40x40));
			StyleSet->Set("VoxelTools.Tab.Small"                             , new FSlateImageBrush(ContentDir + TEXT("Icons/UIIcons/mode_40.png"), Icon16x16));
																		     
			// World generator											     
			StyleSet->Set("ClassThumbnail.VoxelWorldGenerator"               , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/WorldGenerator_64x.png"), Icon64x64));
			StyleSet->Set("ClassIcon.VoxelWorldGenerator"                    , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/WorldGenerator_16x.png"), Icon16x16));
																		     
			// Voxel World Object Save									     
			StyleSet->Set("ClassThumbnail.VoxelWorldSaveObject"              , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/VoxelWorldSaveObject_64x.png"), Icon64x64));
			StyleSet->Set("ClassIcon.VoxelWorldSaveObject"                   , new FSlateImageBrush(ContentDir + TEXT("Icons/AssetIcons/VoxelWorldSaveObject_16x.png"), Icon16x16));
			
			FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
		}

		// Voxel Editor Tools
		FEditorModeRegistry::Get().RegisterMode<FEdModeVoxel>(FEdModeVoxel::EM_Voxel, LOCTEXT("VoxelEdModeName", "Voxels"), FSlateIcon("VoxelStyle", "VoxelTools.Tab", "VoxelTools.Tab.Small"), true);
	}

	virtual void ShutdownModule() override
	{
		FEditorModeRegistry::Get().UnregisterMode(FEdModeVoxel::EM_Voxel);

		if (UObjectInitialized())
		{
			auto& ThumbnailManager = UThumbnailManager::Get();
			ThumbnailManager.UnregisterCustomRenderer(UVoxelGraphGenerator::StaticClass());
			ThumbnailManager.UnregisterCustomRenderer(UVoxelDataAsset::StaticClass());
			ThumbnailManager.UnregisterCustomRenderer(UVoxelHeightmapAsset::StaticClass());
			ThumbnailManager.UnregisterCustomRenderer(UVoxelMeshSpawner::StaticClass());
			ThumbnailManager.UnregisterCustomRenderer(UVoxelMeshSpawnerGroup::StaticClass());
			ThumbnailManager.UnregisterCustomRenderer(UVoxelAssetSpawner::StaticClass());
			ThumbnailManager.UnregisterCustomRenderer(UVoxelSpawnerGroup::StaticClass());
		}

		UnregisterPlacementModeExtensions();
		UnregisterClassLayout();
		UnregisterAssetTools();

		if (StyleSet.IsValid())
		{
			FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
			StyleSet.Reset();
		}
	}

	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}

private:
	template<typename T>
	void RegisterPlacementModeExtension(IPlacementModeModule& PlacementModeModule, UActorFactory* Factory = nullptr)
	{
		PlacementModeModule.RegisterPlaceableItem(PlacementCategoryInfo.UniqueHandle, MakeShared<FPlaceableItem>(Factory, FAssetData(T::StaticClass())));
	}
	void RegisterPlacementModeExtensions()
	{
		IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();
		PlacementModeModule.RegisterPlacementCategory(PlacementCategoryInfo);

		RegisterPlacementModeExtension<AVoxelWorld       >(PlacementModeModule, GetMutableDefault<UActorFactoryVoxelWorld       >());
		RegisterPlacementModeExtension<AVoxelDisableEditsBox>(PlacementModeModule, GetMutableDefault<UActorFactoryVoxelDisableEditsBox>());
		RegisterPlacementModeExtension<AVoxelAssetActor  >(PlacementModeModule, GetMutableDefault<UActorFactoryVoxelAssetActor  >());
		RegisterPlacementModeExtension<AVoxelMeshImporter>(PlacementModeModule, GetMutableDefault<UActorFactoryVoxelMeshImporter>());

		RegisterPlacementModeExtension<AVoxelLandscapeImporter>(PlacementModeModule);

		PlacementModeModule.RegenerateItemsForCategory(FBuiltInPlacementCategories::AllClasses());
	}
	void UnregisterPlacementModeExtensions()
	{
		if (IPlacementModeModule::IsAvailable())
		{
			IPlacementModeModule::Get().UnregisterPlacementCategory(PlacementCategoryInfo.UniqueHandle);
		}
	}

private:
	template<typename T>
	void RegisterCustomClassLayout(FPropertyEditorModule& PropertyModule, FName Name)
	{
		PropertyModule.RegisterCustomClassLayout(Name, FOnGetDetailCustomizationInstance::CreateStatic(&T::MakeInstance));
		RegisteredCustomClassLayouts.Add(Name);
	}
	template<typename T>
	void RegisterCustomPropertyLayout(FPropertyEditorModule& PropertyModule, FName Name)
	{
		PropertyModule.RegisterCustomPropertyTypeLayout(Name, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&T::MakeInstance));
		RegisteredCustomPropertyLayouts.Add(Name);
	}

	void RegisterCustomClassLayouts()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		RegisterCustomClassLayout<FVoxelWorldDetails                    >(PropertyModule, "VoxelWorld");
		RegisterCustomClassLayout<FVoxelMaterialCollectionDetails       >(PropertyModule, "VoxelMaterialCollection");
		RegisterCustomClassLayout<FVoxelLandscapeImporterDetails        >(PropertyModule, "VoxelLandscapeImporter");
		RegisterCustomClassLayout<FVoxelMeshImporterDetails             >(PropertyModule, "VoxelMeshImporter");
		RegisterCustomClassLayout<FVoxelAssetActorDetails               >(PropertyModule, "VoxelAssetActor");
		RegisterCustomClassLayout<FRangeAnalysisDebuggerDetails         >(PropertyModule, "VoxelNode_RangeAnalysisDebuggerFloat");
		RegisterCustomClassLayout<FVoxelMeshSpawnerBaseDetails          >(PropertyModule, "VoxelMeshSpawnerBase");

		RegisterCustomPropertyLayout<FVoxelWorldGeneratorPickerCustomization      >(PropertyModule, "VoxelWorldGeneratorPicker");
		RegisterCustomPropertyLayout<FVoxelWorldGeneratorPickerCustomization      >(PropertyModule, "VoxelTransformableWorldGeneratorPicker");
		RegisterCustomPropertyLayout<FVoxelMaterialCollectionElementCustomization >(PropertyModule, "VoxelMaterialCollectionElement");
		RegisterCustomPropertyLayout<FVoxelPaintMaterialCustomization             >(PropertyModule, "VoxelPaintMaterial");
		RegisterCustomPropertyLayout<FBoolVectorCustomization                     >(PropertyModule, "BoolVector");
		RegisterCustomPropertyLayout<FVoxelBasicSpawnerScaleSettingsCustomization >(PropertyModule, "VoxelBasicSpawnerScaleSettings");
		RegisterCustomPropertyLayout<FVoxelSpawnerOutputNameCustomization         >(PropertyModule, "VoxelSpawnerOutputName");
		RegisterCustomPropertyLayout<FVoxelGraphOutputCustomization               >(PropertyModule, "VoxelGraphOutput");

		PropertyModule.NotifyCustomizationModuleChanged();
	}

	void UnregisterClassLayout()
	{
		if (auto* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
		{
			for (auto& Name : RegisteredCustomClassLayouts)
			{
				PropertyModule->UnregisterCustomClassLayout(Name);
			}
			for (auto& Name : RegisteredCustomPropertyLayouts)
			{
				PropertyModule->UnregisterCustomPropertyTypeLayout(Name);
			}
			PropertyModule->NotifyCustomizationModuleChanged();
		}
	}
	
private:
	template<typename T>
	void RegisterAssetTypeAction(IAssetTools& AssetTools)
	{
		auto Action = MakeShared<T>(VoxelAssetCategoryBit);
		AssetTools.RegisterAssetTypeActions(Action);
		RegisteredAssetTypeActions.Add(Action);
	}

	void RegisterAssetTools()
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		RegisterAssetTypeAction<FAssetTypeActions_VoxelWorldSaveObject        >(AssetTools);
		RegisterAssetTypeAction<FAssetTypeActions_VoxelMaterialCollection     >(AssetTools);
		RegisterAssetTypeAction<FAssetTypeActions_VoxelBasicMaterialCollection>(AssetTools);
		RegisterAssetTypeAction<FAssetTypeActions_VoxelDataAsset              >(AssetTools);
		RegisterAssetTypeAction<FAssetTypeActions_VoxelSpawnerConfig          >(AssetTools);
		RegisterAssetTypeAction<FAssetTypeActions_VoxelAssetSpawner           >(AssetTools);
		RegisterAssetTypeAction<FAssetTypeActions_VoxelMeshSpawner            >(AssetTools);
		RegisterAssetTypeAction<FAssetTypeActions_VoxelMeshSpawnerGroup       >(AssetTools);
		RegisterAssetTypeAction<FAssetTypeActions_VoxelSpawnerGroup           >(AssetTools);
		RegisterAssetTypeAction<FAssetTypeActions_VoxelHeightmapAsset         >(AssetTools);
		RegisterAssetTypeAction<FAssetTypeActions_VoxelGraphWorldGenerator    >(AssetTools);
		RegisterAssetTypeAction<FAssetTypeActions_VoxelGraphMacro             >(AssetTools);
		RegisterAssetTypeAction<FAssetTypeActions_VoxelGraphOutputsConfig     >(AssetTools);
	}

	void UnregisterAssetTools()
	{
		if (auto* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools"))
		{
			IAssetTools& AssetTools = AssetToolsModule->Get();
			for (auto& Action : RegisteredAssetTypeActions)
			{
				AssetTools.UnregisterAssetTypeActions(Action);
			}
		}
	}
	
public:

	virtual bool GenerateSingleMaterials(UVoxelMaterialCollection* Collection, FString& OutError) override
	{
		return FVoxelMaterialCollectionHelpers::GenerateSingleMaterials(Collection, OutError);
	}

	virtual bool GenerateDoubleMaterials(UVoxelMaterialCollection* Collection, FString& OutError) override
	{
		return FVoxelMaterialCollectionHelpers::GenerateDoubleMaterials(Collection, OutError);
	}

	virtual bool GenerateTripleMaterials(UVoxelMaterialCollection* Collection, FString& OutError) override
	{
		return FVoxelMaterialCollectionHelpers::GenerateTripleMaterials(Collection, OutError);
	}

	virtual void RefreshVoxelWorlds(UObject* MatchingGenerator) override
	{
		RefreshVoxelWorlds_Execute(MatchingGenerator);
	}

private:
	/** The collection of registered asset type actions. */
	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;
	TArray<FName> RegisteredCustomClassLayouts;
	TArray<FName> RegisteredCustomPropertyLayouts;

	EAssetTypeCategories::Type VoxelAssetCategoryBit = EAssetTypeCategories::None;
	FPlacementCategoryInfo PlacementCategoryInfo = FPlacementCategoryInfo(LOCTEXT("VoxelCategoryName", "Voxel"), "Voxel", TEXT("PMVoxel"), 25);
	TSharedPtr<FSlateStyleSet> StyleSet;
};

IMPLEMENT_MODULE(FVoxelEditorModule, VoxelEditor);

#undef LOCTEXT_NAMESPACE