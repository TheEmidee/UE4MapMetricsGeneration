#pragma once

#include "LevelStatsGridConfiguration.h"
#include "LevelStatsPerformanceReport.h"

#include <ChartCreation.h>
#include <CoreMinimal.h>

#include "LevelStatsCollector.generated.h"

class FLevelStatsCollectorState;
class FJsonObject;

struct FLevelStatsSettings
{
    float CameraHeight;
    float CameraHeightOffset;
    float CameraRotationDelta;
    float CaptureDelay;
    float MetricsDuration;
    float MetricsWaitDelay;
    float CellSize;
    FVector GridCenterOffset;
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
    bool ProcessNextCell();
    void InitializeGrid();
    void SetupSceneCapture() const;
    TOptional< FVector > TraceGroundPosition( const FVector & start_location ) const;

    FString GetBasePath() const;
    FString GetScreenshotPath() const;
    FString GetJsonOutputPath() const;

    UPROPERTY()
    USceneCaptureComponent2D * CaptureComponent;

    FLevelStatsPerformanceReport PerformanceReport;
    FLevelStatsGridConfiguration GridConfig;
    FLevelStatsSettings Settings;
    FString ReportFolderName;

    TSharedPtr< FLevelStatsCollectorState > CurrentState;

    int32 TotalCaptureCount;
    int32 CurrentCellIndex;
    float CurrentRotation;
    float CurrentCaptureDelay;
    bool bIsCapturing;
    bool bIsInitialized;
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