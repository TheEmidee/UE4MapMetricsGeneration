#pragma once

#include <CoreMinimal.h>

#include "LevelStatsCollector.generated.h"

USTRUCT()
struct FGridCell
{
    GENERATED_BODY()

    FVector Center;
    float GroundHeight;
    bool bProcessed;

    FGridCell() :
        Center( FVector::ZeroVector ),
        GroundHeight( 0.0f ),
        bProcessed( false )
    {}
};

UCLASS()
class MAPMETRICSGENERATION_API ALevelStatsCollector final : public AActor
{
    GENERATED_BODY()

public:
    ALevelStatsCollector();

    void PostInitializeComponents() override;
    void BeginPlay() override;
    void Tick( float DeltaTime ) override;

private:
    void InitializeGrid();
    void SetupSceneCapture() const;
    bool ProcessNextCell();
    void CaptureCurrentView();
    bool TraceGroundPosition( const FVector & start_location, FVector & out_hit_location ) const;
    void CalculateGridBounds();
    bool MoveToNextCell();
    FString GenerateFileName() const;
    void LogGridInfo() const;

public:
    UPROPERTY()
    float GridSizeX;

    UPROPERTY()
    float GridSizeY;

    UPROPERTY()
    float CellSize;

    UPROPERTY()
    FVector GridCenterOffset;

    UPROPERTY()
    float CameraHeight;

    UPROPERTY()
    float CameraHeightOffset;

    UPROPERTY()
    float CameraRotationDelta;

    UPROPERTY( EditAnywhere, Category = "Capture Configuration" )
    float CaptureDelay;

    UPROPERTY( EditAnywhere, Category = "Capture Configuration" )
    FString OutputDirectory;

private:
    UPROPERTY()
    USceneCaptureComponent2D * CaptureComponent;

    TArray< FGridCell > GridCells;
    int32 CurrentCellIndex;
    float CurrentRotation;
    float CurrentCaptureDelay;
    FBox GridBounds;
    FIntPoint GridDimensions;
    int32 TotalCaptureCount;
    bool bIsCapturing;
    bool bIsInitialized;
};
