#pragma once

#include <ChartCreation.h>
#include <CoreMinimal.h>

#include "LevelStatsCollector.generated.h"

class MAPMETRICSGENERATION_API FCustomPerformanceChart final : public FPerformanceTrackingChart
{
public:
    FCustomPerformanceChart( const FDateTime & in_start_time, const FString & in_chart_label, const FString & in_output_path ) :
        FPerformanceTrackingChart( in_start_time, in_chart_label ),
        CustomOutputPath( in_output_path )
    {
    }

    void DumpFPSChartToCustomLocation( const FString & in_map_name )
    {
        TArray< const FPerformanceTrackingChart * > charts;
        charts.Add( this );

        DumpChartsToOutputLog( AccumulatedChartTime, charts, in_map_name );

#if ALLOW_DEBUG_FILES
        IFileManager::Get().MakeDirectory( *CustomOutputPath, true );

        {
            const auto log_filename = CustomOutputPath / CreateFileNameForChart( TEXT( "FPS" ), in_map_name, TEXT( ".log" ) );
            DumpChartsToLogFile( AccumulatedChartTime, charts, in_map_name, log_filename );
        }

        {
            const auto map_and_chart_label = ChartLabel.IsEmpty() ? in_map_name : ( ChartLabel + TEXT( "-" ) + in_map_name );
            const auto html_filename = CustomOutputPath / CreateFileNameForChart( TEXT( "FPS" ),
                                                              *( map_and_chart_label + TEXT( "-" ) + CaptureStartTime.ToString() ),
                                                              TEXT( ".html" ) );
            DumpChartsToHTML( AccumulatedChartTime, charts, map_and_chart_label, html_filename );
        }
#endif
    }

private:
    static FString CreateFileNameForChart( const FString & /* chart_type */, const FString & /* in_map_name */, const FString & file_extension )
    {
        const FString platform = FPlatformProperties::PlatformName();
        return TEXT( "metrics" ) + file_extension;
    }

    FString CustomOutputPath;
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
    TSharedPtr< FCustomPerformanceChart > CurrentPerformanceChart;
    float CurrentMetricsCaptureTime;
    float MetricsDuration;
    float MetricsWaitDelay;

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
