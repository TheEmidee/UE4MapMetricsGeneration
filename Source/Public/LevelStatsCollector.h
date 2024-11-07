#pragma once

#include <ChartCreation.h>
#include <CoreMinimal.h>

#include "LevelStatsCollector.generated.h"

class FLevelStatsCollectorState;

class FCustomPerformanceChart final : public FPerformanceTrackingChart
{
public:
    FCustomPerformanceChart( const FDateTime & start_time, FStringView chart_label, FStringView output_path );
    void DumpFPSChartToCustomLocation( FStringView map_name );

private:
    static FString CreateFileNameForChart( FStringView chart_type, FStringView in_map_name, FStringView file_extension );
    FString CustomOutputPath;
};

DECLARE_LOG_CATEGORY_EXTERN( LogLevelStatsCollector, Log, All );

UCLASS()
class MAPMETRICSGENERATION_API ALevelStatsCollector final : public AActor
{
    GENERATED_BODY()

    friend class FIdleState;
    friend class FWaitingForSnapshotState;
    friend class FCapturingMetricsState;
    friend class FProcessingNextRotationState;
    friend class FProcessingNextCellState;

public:
    ALevelStatsCollector();

    void PostInitializeComponents() override;
    void BeginPlay() override;
    void Tick( float delta_time ) override;

    void TransitionToState( const TSharedPtr< FLevelStatsCollectorState > & new_state );

private:
    void UpdateRotation();
    void IncrementCellIndex();
    void FinishCapture();

    void StartMetricsCapture();
    void FinishMetricsCapture();
    void CaptureCurrentView();
    bool ProcessNextCell();

    void InitializeGrid();
    void SetupSceneCapture() const;
    TOptional< FVector > TraceGroundPosition( const FVector & start_location ) const;
    void CalculateGridBounds();

    FString GetBasePath() const;
    FString GetCurrentCellPath() const;
    FString GetCurrentRotationPath() const;

    void LogGridInfo() const;

    struct FGridCell
    {
        explicit FGridCell( const FVector & center ) :
            Center( center ),
            GroundHeight( 0.0f )
        {}

        FGridCell() :
            Center( FVector::ZeroVector ),
            GroundHeight( 0.0f )
        {}

        FVector Center;
        float GroundHeight;
    };

    UPROPERTY()
    USceneCaptureComponent2D * CaptureComponent;

    TSharedPtr< FLevelStatsCollectorState > CurrentState;
    TSharedPtr< FCustomPerformanceChart > CurrentPerformanceChart;
    TArray< FGridCell > GridCells;
    FVector GridCenterOffset;
    int32 TotalCaptureCount;
    int32 CurrentCellIndex;
    FIntPoint GridDimensions;
    FBox GridBounds;

    float MetricsDuration;
    float MetricsWaitDelay;
    float GridSizeX;
    float GridSizeY;
    float CellSize;
    float CameraHeight;
    float CameraHeightOffset;
    float CameraRotationDelta;
    float CaptureDelay;
    float CurrentRotation;
    float CurrentCaptureDelay;

    bool bIsCapturing;
    bool bIsInitialized;
};