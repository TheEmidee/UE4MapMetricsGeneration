#include "LevelStatsCollectorState.h"

#include "LevelStatsCollector.h"

#include <Components/SceneCaptureComponent2D.h>
#include <Engine/TextureRenderTarget2D.h>
#include <ImageUtils.h>

FLevelStatsCollectorState::FLevelStatsCollectorState( ALevelStatsCollector * collector ) :
    Collector( collector )
{}

void FLevelStatsCollectorState::Enter()
{}

void FLevelStatsCollectorState::Exit()
{}

// :NOTE: FIdleState Implementation
FIdleState::FIdleState( ALevelStatsCollector * collector ) :
    FLevelStatsCollectorState( collector ),
    CurrentDelay( 0.0f )
{}

void FIdleState::Enter()
{
    CurrentDelay = 0.0f;
}

void FIdleState::Tick( const float delta_time )
{
    CurrentDelay += delta_time;
    if ( CurrentDelay >= Collector->Settings.MetricsWaitDelay )
    {
        Collector->TransitionToState( MakeShared< FCapturingMetricsState >( Collector ) );
    }
}

void FIdleState::Exit()
{
    CurrentDelay = 0.0f;
}

// :NOTE: FWaitingForSnapshotState Implementation
FWaitingForSnapshotState::FWaitingForSnapshotState( ALevelStatsCollector * collector ) :
    FLevelStatsCollectorState( collector ),
    CurrentDelay( 0.0f ),
    CaptureComponent( collector->CaptureComponent ),
    CurrentCellIndex( collector->CurrentCellIndex )
{}

void FWaitingForSnapshotState::Enter()
{
    CurrentDelay = 0.0f;
}

void FWaitingForSnapshotState::Tick( const float delta_time )
{
    CurrentDelay += delta_time;

    if ( CurrentDelay >= Collector->Settings.CaptureDelay )
    {
        if ( CaptureComponent == nullptr || CaptureComponent->TextureTarget == nullptr )
        {
            UE_LOG( LogLevelStatsCollector, Error, TEXT( "Invalid capture component or render target!" ) );
            return;
        }

        const auto current_path = Collector->GetCurrentRotationPath();
        IFileManager::Get().MakeDirectory( *current_path, true );
        const auto screenshot_path = FString::Printf( TEXT( "%sscreenshot.png" ), *current_path );

        CaptureComponent->CaptureScene();
        TWeakObjectPtr< ALevelStatsCollector > weak_collector( Collector );
        const auto cell_index = CurrentCellIndex;
        const auto rotation = Collector->CurrentRotation;
        Collector->bIsCapturing = false;

        CaptureAndSaveAsync( CaptureComponent->TextureTarget, screenshot_path )
            .Next( [ weak_collector, cell_index, rotation, screenshot_path ]( bool bSuccess ) {
                AsyncTask( ENamedThreads::GameThread, [ weak_collector, cell_index, rotation, screenshot_path, bSuccess ]() {
                    if ( !weak_collector.IsValid() )
                    {
                        return;
                    }

                    ALevelStatsCollector * safe_collector = weak_collector.Get();

                    if ( bSuccess )
                    {
                        const auto & current_cell = safe_collector->GridConfig.GridCells[ cell_index ];
                        UE_LOG( LogLevelStatsCollector,
                            Log,
                            TEXT( "Image captured at coordinates (%f, %f, %f), saved to: %s" ),
                            current_cell.Center.X,
                            current_cell.Center.Y,
                            current_cell.Center.Z,
                            *screenshot_path );

                        safe_collector->TotalCaptureCount++;
                        safe_collector->CurrentRotation = rotation;
                        safe_collector->CurrentCellIndex = cell_index;
                        safe_collector->bIsCapturing = true;

                        safe_collector->TransitionToState( MakeShared< FProcessingNextRotationState >( safe_collector ) );
                    }
                    else
                    {
                        UE_LOG( LogLevelStatsCollector, Error, TEXT( "Failed to save image: %s" ), *screenshot_path );
                        safe_collector->bIsCapturing = true;
                    }
                } );
            } );
    }
}

void FWaitingForSnapshotState::Exit()
{
    CurrentDelay = 0.0f;
}

TFuture< bool > FWaitingForSnapshotState::CaptureAndSaveAsync( UTextureRenderTarget2D * render_target, const FString & output_path )
{
    FImage image;
    if ( !FImageUtils::GetRenderTargetImage( render_target, image ) )
    {
        return MakeFulfilledPromise< bool >( false ).GetFuture();
    }

    return Async( EAsyncExecution::ThreadPool, [ image = MoveTemp( image ), output_path ] {
        return FImageUtils::SaveImageByExtension( *output_path, image );
    } );
}

// :NOTE: FProcessingNextRotationState Implementation
FProcessingNextRotationState::FProcessingNextRotationState( ALevelStatsCollector * collector ) :
    FLevelStatsCollectorState( collector ),
    CurrentRotation( collector->CurrentRotation )
{}

void FProcessingNextRotationState::Enter()
{
    CurrentRotation += Collector->Settings.CameraRotationDelta;
    Collector->CaptureComponent->SetRelativeRotation( FRotator( 0.0f, CurrentRotation, 0.0f ) );
    Collector->CurrentRotation = CurrentRotation;
}

void FProcessingNextRotationState::Tick( float delta_time )
{
    if ( CurrentRotation >= 360.0f )
    {
        Collector->TransitionToState( MakeShared< FProcessingNextCellState >( Collector ) );
    }
    else
    {
        Collector->TransitionToState( MakeShared< FIdleState >( Collector ) );
    }
}

void FProcessingNextRotationState::Exit()
{}

// :NOTE: FProcessingNextCellState Implementation
FProcessingNextCellState::FProcessingNextCellState( ALevelStatsCollector * collector ) :
    FLevelStatsCollectorState( collector )
{}

void FProcessingNextCellState::Enter()
{
    Collector->CurrentCellIndex++;
    Collector->CurrentRotation = 0.0f;
}

void FProcessingNextCellState::Tick( float delta_time )
{
    if ( Collector->ProcessNextCell() )
    {
        Collector->TransitionToState( MakeShared< FIdleState >( Collector ) );
    }
    else
    {
        Collector->bIsCapturing = false;
        UE_LOG( LogLevelStatsCollector, Log, TEXT( "Capture process complete! Total captures: %d" ), Collector->TotalCaptureCount );
    }
}

void FProcessingNextCellState::Exit()
{}

// :NOTE: FCapturingMetricsState Implementation
FCapturingMetricsState::FCapturingMetricsState( ALevelStatsCollector * collector ) :
    FLevelStatsCollectorState( collector ),
    CurrentCaptureTime( 0.0f ),
    CurrentCellIndex( collector->CurrentCellIndex ),
    CurrentRotation( collector->CurrentRotation )
{}

void FCapturingMetricsState::Enter()
{
    CurrentCaptureTime = 0.0f;
    const auto label = FString::Printf( TEXT( "Cell_%d_Rot_%.0f" ), CurrentCellIndex, CurrentRotation );

    if ( CurrentPerformanceChart.IsValid() )
    {
        GEngine->RemovePerformanceDataConsumer( CurrentPerformanceChart );
        CurrentPerformanceChart.Reset();
    }

    CurrentPerformanceChart = MakeShareable( new FPerformanceMetricsCapture( FDateTime::Now(), label ) );

    GEngine->AddPerformanceDataConsumer( CurrentPerformanceChart );
}

void FCapturingMetricsState::Tick( const float delta_time )
{
    CurrentCaptureTime += delta_time;

    if ( CurrentCaptureTime >= Collector->Settings.MetricsDuration )
    {
        Collector->TransitionToState( MakeShared< FWaitingForSnapshotState >( Collector ) );
    }
}

void FCapturingMetricsState::Exit()
{
    if ( !CurrentPerformanceChart.IsValid() )
    {
        UE_LOG( LogLevelStatsCollector, Warning, TEXT( "AddRotationToReport: Invalid performance chart" ) );
        return;
    }

    CurrentPerformanceChart->CaptureMetrics();

    const auto screenshot_path = FString::Printf( TEXT( "Cell_%d/Rotation_%.0f/screenshot.png" ),
        CurrentCellIndex,
        CurrentRotation );

    Collector->PerformanceReport.AddRotationData(
        CurrentCellIndex,
        CurrentRotation,
        screenshot_path,
        CurrentPerformanceChart->GetMetricsJson(),
        Collector->GetCurrentRotationPath() );

    GEngine->RemovePerformanceDataConsumer( CurrentPerformanceChart );
    CurrentPerformanceChart.Reset();
}
