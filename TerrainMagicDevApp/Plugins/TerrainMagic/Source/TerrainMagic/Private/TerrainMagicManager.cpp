﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "TerrainMagicManager.h"

#include "Kismet/KismetRenderingLibrary.h"


// Sets default values
ATerrainMagicManager::ATerrainMagicManager()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ATerrainMagicManager::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATerrainMagicManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

UTextureRenderTarget2D* ATerrainMagicManager::GetHeightMap()
{
	if (CachedHeightMap != nullptr)
	{
		return CachedHeightMap;
	}
	
	return HeightRenderTarget;
}

void ATerrainMagicManager::CacheHeightMap(UTextureRenderTarget2D* HeightMap)
{
	CachedHeightMap = HeightMap;
}

UTextureRenderTarget2D* ATerrainMagicManager::EnsureHeightRenderTarget(const int Width, const int Height)
{
	if (HeightRenderTarget == nullptr)
	{
		HeightRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), Width, Height, RTF_RGBA8);
	}

	CachedHeightMap = nullptr;
	return HeightRenderTarget;
}

UTextureRenderTarget2D* ATerrainMagicManager::EnsureWeightRenderTarget(const int Width, const int Height)
{
	if (WeightRenderTarget == nullptr)
	{
		WeightRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), Width, Height, RTF_R8);
	}

	return WeightRenderTarget;
}

