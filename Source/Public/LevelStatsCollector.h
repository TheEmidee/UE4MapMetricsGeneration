#pragma once

#include <ChartCreation.h>
#include <CoreMinimal.h>

#include "LevelStatsCollector.generated.h"

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
    void Tick( float DeltaTime ) override;

private:
    void InitializeGrid();
    void SetupSceneCapture() const;
    bool ProcessNextCell();
    void CaptureCurrentView();
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

    enum class ECaptureState
    {
        Idle,
        CapturingMetrics,
        WaitingForSnapshot,
        ProcessingNextRotation,
        ProcessingNextCell
    };

    UPROPERTY()
    USceneCaptureComponent2D * CaptureComponent;

    ECaptureState CurrentState;
    TSharedPtr< FPerformanceMetricsCapture > CurrentPerformanceChart;
    float CurrentMetricsCaptureTime;
    float MetricsDuration;
    float MetricsWaitDelay;
    TSharedPtr< FJsonObject > CaptureReport;
    TSharedPtr< FJsonObject > CurrentCellObject;

    float GridSizeX;
    float GridSizeY;
    float CellSize;
    FVector GridCenterOffset;
    float CameraHeight;
    float CameraHeightOffset;
    float CameraRotationDelta;
    float CaptureDelay;
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

FORCEINLINE FPerformanceMetricsCapture::FPerformanceMetricsCapture( const FDateTime & in_start_time, const FString & in_chart_label ) :
    FPerformanceTrackingChart( in_start_time, in_chart_label )
{
    MetricsObject = MakeShared< FJsonObject >();
}

FORCEINLINE TSharedPtr< FJsonObject > FPerformanceMetricsCapture::GetMetricsJson() const
{
    return MetricsObject;
}