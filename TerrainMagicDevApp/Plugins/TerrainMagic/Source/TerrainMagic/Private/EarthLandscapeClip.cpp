﻿// Copyright (c) 2022 GDi4K. All Rights Reserved.

#include "EarthLandscapeClip.h"

#include "HttpModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/Material.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Utils/MapBoxUtils.h"
#include "Utils/TerrainMagicThreading.h"


float smoothstep (float edge0, float edge1, float x)
{
	if (x < edge0)
		return 0;

	if (x >= edge1)
		return 1;

	// Scale/bias into [0..1] range
	x = (x - edge0) / (edge1 - edge0);

	return x * x * (3 - 2 * x);
}

// Sets default values
AEarthLandscapeClip::AEarthLandscapeClip()
{
}

UMaterial* AEarthLandscapeClip::GetSourceMaterialForHeight() const
{
	const FName MaterialPath = "/TerrainMagic/Core/Materials/M_TM_LandscapeClip_HeightChange.M_TM_LandscapeClip_HeightChange";
	return Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr, *MaterialPath.ToString()));
}

TArray<FTerrainMagicMaterialParam> AEarthLandscapeClip::GetMaterialParams()
{
	TArray<FTerrainMagicMaterialParam> MaterialParams;
	
	MaterialParams.Push({"Texture", HeightMap});
	MaterialParams.Push({"BlurDistance", static_cast<float>(BlurDistance)});
	MaterialParams.Push({"BlurDistanceSteps", static_cast<float>(BlurDistanceSteps)});
	MaterialParams.Push({"BlurRadialSteps", static_cast<float>(BlurRadialSteps)});
	
	MaterialParams.Push({"HeightMultiplier", static_cast<float>(HeightMultiplier)});
	MaterialParams.Push({"SelectedBlendMode", static_cast<float>(BlendMode)});

	MaterialParams.Push({"HeightMapInputMin", HeightMapRange.InputMin});
	MaterialParams.Push({"HeightMapInputMax", HeightMapRange.InputMax});
	MaterialParams.Push({"HeightMapOutputMin", HeightMapRange.OutputMin});
	MaterialParams.Push({"HeightMapOutputMax", HeightMapRange.OutputMax});

	MaterialParams.Push({"HeightSaturation", HeightSaturation});

	MaterialParams.Push({"SelectedFadeMode", static_cast<float>(FadeMode)});
	MaterialParams.Push({"FadeSaturation", FadeSaturation});
	MaterialParams.Push({"FadeMaskSpan", FadeMaskSpan});
	
	return MaterialParams;
}

int AEarthLandscapeClip::GetHeightMultiplier() const
{
	return HeightMultiplier;
}

FVector2D AEarthLandscapeClip::GetClipBaseSize() const
{
	return HeightMapBaseSize;
}

void AEarthLandscapeClip::SetClipBaseSize(FVector2D BaseSize)
{
	HeightMapBaseSize = BaseSize;
}

bool AEarthLandscapeClip::IsEnabled() const
{
	return bEnabled;
}

void AEarthLandscapeClip::SetEnabled(bool bEnabledInput)
{
	bEnabled = bEnabledInput;
}

void AEarthLandscapeClip::SetZIndex(int Index)
{
	ZIndex = Index;
}

int AEarthLandscapeClip::GetZIndex() const
{
	return ZIndex;
}

void AEarthLandscapeClip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if (GetWorld()->WorldType != EWorldType::Editor)
	{
		return;
	}
	ReloadTextureIfNeeded();
}

UTexture* AEarthLandscapeClip::GetHeightMap() const
{
	return HeightMap;
}

TArray<FLandscapeClipPaintLayerSettings> AEarthLandscapeClip::GetPaintLayerSettings() const
{
	return PaintLayerSettings;
}

void AEarthLandscapeClip::ReloadTextureIfNeeded()
{
	if (HasTextureReloaded)
	{
		return;
	}
	HasTextureReloaded = true;

	if (CurrentHeightData.Num() == 0)
	{
		return;
	}

	const int32 TextureWidth = FMath::Sqrt(static_cast<float>(CurrentHeightData.Num()));
	FMapBoxUtils::MakeG16Texture(TextureWidth, CurrentHeightData.GetData(), [this](UTexture2D* Texture)
	{
		HeightMap = Texture;
	});
}

void AEarthLandscapeClip::DownloadTile(TFunction<void(FEarthTileDownloadStatus)> StatusCallback)
{
	TArray<FString> Parts;
	TileInfoString.TrimStartAndEnd().ParseIntoArray(Parts, TEXT(","), true);
	checkf(Parts.Num() == 3, TEXT("TileInfo text is invalid!"))
	
	FMapBoxTileQuery TileQuery = {};
	TileQuery.X = FCString::Atoi(*Parts[0].TrimStartAndEnd());
	TileQuery.Y = FCString::Atoi(*Parts[1].TrimStartAndEnd());
	TileQuery.Zoom = FCString::Atoi(*Parts[2].TrimStartAndEnd());
	TileQuery.ZoomInLevels = TileResolution;

	TileDownloadProgress = "Start downloading tiles";
	FMapBoxUtils::DownloadTileSet(TileQuery, [this, TileQuery, StatusCallback](TSharedPtr<FMapBoxTileDownloadProgress> DownloadProgress, TSharedPtr<FMapBoxTileResponse> TileResponseData)
	{
		TileDownloadProgress = FString::Printf(TEXT("Completed: %d/%d"), DownloadProgress->TilesDownloaded, DownloadProgress->TotalTiles);

		if (TileResponseData == nullptr)
		{
			return;
		}

		if (!TileResponseData->IsSuccess)
		{
			FEarthTileDownloadStatus Status;
			Status.IsError = true;
			Status.ErrorMessage = TileResponseData->ErrorMessage;
			
			StatusCallback(Status);
			return;
		}
		
		const int32 TilesPerRow = FMath::Pow(2.0, static_cast<float>(TileQuery.ZoomInLevels));
		const int32 PixelsPerRow = 512 * TilesPerRow;
	
		// This is important, otherwise the TileData will be garbage collected
		CurrentTileResponse = TileResponseData;
		FMapBoxUtils::MakeG16Texture(PixelsPerRow, TileResponseData->HeightData.GetData(), [this, StatusCallback](UTexture2D* Texture)
		{
			FTerrainMagicThreading::RunOnGameThread([this, Texture, StatusCallback]()
			{
				HeightMap = Texture;
				CurrentHeightData = CurrentTileResponse->HeightData;
				CurrentTileResponse = nullptr;
				_Invalidate();

				if (StatusCallback != nullptr)
				{
					FEarthTileDownloadStatus Status;
					Status.IsError = false;
					
					StatusCallback(Status);
				}
			});
		});
	});
}