#pragma once

#include <ChartCreation.h>
#include <CoreMinimal.h>

#include "LevelStatsCollector.generated.h"

class FLevelStatsCollectorState;

class FPerformanceMetricsCapture final : public FPerformanceTrackingChart
{
public:
    FPerformanceMetricsCapture( const FDateTime & in_start_time, const FString & in_chart_label );
    TSharedPtr< FJsonObject > GetMetricsJson() const;
    void CaptureMetrics() const;

private:
    TSharedPtr< FJsonObject > MetricsObject;
};

UCLASS()
class MAPMETRICSGENERATION_API ALevelStatsCollector final : public AActor
{
    GENERATED_BODY()

public:
    ALevelStatsCollector();

    void PostInitializeComponents() override;
    void BeginPlay() override;
    void Tick( float delta_time ) override;

    float GetMetricsWaitDelay() const;
    float GetMetricsDuration() const;
    float GetCaptureDelay() const;
    float GetCurrentRotation() const;

    void TransitionToState( const TSharedPtr< FLevelStatsCollectorState > & new_state );
    void UpdateRotation();
    void IncrementCellIndex();
    void FinishCapture();

    void StartMetricsCapture();
    void FinishMetricsCapture();
    void CaptureCurrentView();
    bool ProcessNextCell();

private:
    void InitializeGrid();
    void SetupSceneCapture() const;
    TOptional< FVector > TraceGroundPosition( const FVector & start_location ) const;
    void CalculateGridBounds();

    void StartMetricsCapture();
    void ProcessMetricsCapture( float DeltaTime );
    void FinishMetricsCapture();

    void InitializeJsonReport();
    void AddCellToReport();
    void AddRotationToReport();
    void FinalizeAndSaveReport() const;
    void SaveRotationMetrics( const TSharedPtr< FJsonObject > & rotation_object );

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

    TSharedPtr< FLevelStatsCollectorState > CurrentState;
    TSharedPtr< FCustomPerformanceChart > CurrentPerformanceChart;
    TArray< FGridCell > GridCells;
    FVector GridCenterOffset;
    int32 TotalCaptureCount;
    int32 CurrentCellIndex;
    FIntPoint GridDimensions;
    FBox GridBounds;

    float CurrentMetricsCaptureTime;
    float MetricsDuration;
    float MetricsWaitDelay;
    TSharedPtr< FJsonObject > CaptureReport;
    TSharedPtr< FJsonObject > CurrentCellObject;

    float GridSizeX;
    float GridSizeY;
    float CellSize;
    float CameraHeight;
    float CameraHeightOffset;
    float CameraRotationDelta;
    float CaptureDelay;
    float CurrentRotation;
    float CurrentCaptureDelay;

    bool IsCaptureInProgress;
    bool IsCollectorInitialized;
};

FORCEINLINE FPerformanceMetricsCapture::FPerformanceMetricsCapture( const FDateTime & in_start_time, const FString & in_chart_label ) :
    FPerformanceTrackingChart( in_start_time, in_chart_label )
{
    MetricsObject = MakeShared< FJsonObject >();
}

FORCEINLINE TSharedPtr< FJsonObject > FPerformanceMetricsCapture::GetMetricsJson() const
{
    return MetricsObject;
}
FORCEINLINE float ALevelStatsCollector::GetMetricsWaitDelay() const
{
    return MetricsWaitDelay;
}

FORCEINLINE float ALevelStatsCollector::GetMetricsDuration() const
{
    return MetricsDuration;
}

FORCEINLINE float ALevelStatsCollector::GetCaptureDelay() const
{
    return CaptureDelay;
}

FORCEINLINE float ALevelStatsCollector::GetCurrentRotation() const
{
    return CurrentRotation;
}