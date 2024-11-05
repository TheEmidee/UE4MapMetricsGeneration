#pragma once

#include <CoreMinimal.h>

#include "LevelStatsCollector.generated.h"

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
    TOptional< FVector > TraceGroundPosition( const FVector & start_location ) const;
    void CalculateGridBounds();
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
    struct FGridCell
    {
        FVector Center;
        float GroundHeight;

        explicit FGridCell( const FVector & center ) :
            Center( center ),
            GroundHeight( 0.0f )
        {}

        FGridCell() :
            Center( FVector::ZeroVector ),
            GroundHeight( 0.0f )
        {}
    };

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
