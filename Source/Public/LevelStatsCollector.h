#pragma once

#include "LevelStatsPerformanceReport.h"

#include <ChartCreation.h>
#include <CoreMinimal.h>

#include "LevelStatsCollector.generated.h"

class FLevelStatsCollectorState;
class FJsonObject;

struct FLevelStatsSettings
{
    float CellSize = 10000.0f;
    float CameraHeight = 10000.0f;
    float CameraHeightOffset = 250.0f;
    float CameraRotationDelta = 90.0f;
    float CaptureDelay = 0.1f;
    float MetricsDuration = 2.0f;
    float MetricsWaitDelay = 1.0f;
};

class FPerformanceMetricsCapture final : public FPerformanceTrackingChart
{
public:
    FPerformanceMetricsCapture( const FDateTime & start_time, const FStringView chart_label );
    TSharedPtr< FJsonObject > GetMetricsJson() const;
    void CaptureMetrics() const;

private:
    TSharedPtr< FJsonObject > MetricsObject;
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
    const FLevelStatsSettings & GetSettings() const;

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

    void AddRotationToReport() const;

    FString GetBasePath() const;
    FString GetCurrentCellPath() const;
    FString GetCurrentRotationPath() const;
    FString GetJsonOutputPath() const;

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

    FLevelStatsPerformanceReport PerformanceReport;
    FLevelStatsSettings Settings;
    FString ReportFolderName;

    TSharedPtr< FLevelStatsCollectorState > CurrentState;
    TSharedPtr< FPerformanceMetricsCapture > CurrentPerformanceChart;

    TArray< FGridCell > GridCells;
    FIntPoint GridDimensions;
    FVector GridCenterOffset;
    FBox GridBounds;
    float GridSizeX;
    float GridSizeY;

    int32 TotalCaptureCount;
    int32 CurrentCellIndex;
    float CurrentRotation;
    float CurrentCaptureDelay;
    bool IsCaptureInProgress;
    bool IsCollectorInitialized;
};

FORCEINLINE FPerformanceMetricsCapture::FPerformanceMetricsCapture( const FDateTime & start_time, const FStringView chart_label ) :
    FPerformanceTrackingChart( start_time, FString( chart_label ) )
{
    MetricsObject = MakeShared< FJsonObject >();
}

FORCEINLINE TSharedPtr< FJsonObject > FPerformanceMetricsCapture::GetMetricsJson() const
{
    return MetricsObject;
}

FORCEINLINE const FLevelStatsSettings & ALevelStatsCollector::GetSettings() const
{
    return Settings;
}