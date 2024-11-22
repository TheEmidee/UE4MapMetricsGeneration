#include "LevelStatsCollectorState.h"

#include "LevelStatsCollector.h"

#include <Components/SceneCaptureComponent2D.h>
#include <Engine/TextureRenderTarget2D.h>
#include <IImageWrapper.h>
#include <IImageWrapperModule.h>
#include <ImageUtils.h>
#include <Modules/ModuleManager.h>

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

        const auto base_path = Collector->GetBasePath();
        IFileManager::Get().MakeDirectory( *base_path, true );
        const auto screenshot_path = Collector->GetScreenshotPath();

        CaptureComponent->CaptureScene();
        TWeakObjectPtr< ALevelStatsCollector > weak_collector( Collector );
        const auto cell_index = CurrentCellIndex;
        const auto rotation = Collector->CurrentRotation;
        Collector->bIsCapturing = false;

        CaptureAndSaveAsync( CaptureComponent->TextureTarget, screenshot_path )
            .Next( [ weak_collector, cell_index, rotation, screenshot_path ]( bool bSuccess ) {
                AsyncTask( ENamedThreads::GameThread, [ weak_collector, cell_index, rotation, screenshot_path, bSuccess ] {
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

    return Async( EAsyncExecution::ThreadPool, [ image = MoveTemp( image ), output_path ]() mutable {
        FImage resized_image;

        constexpr auto new_width = 1280;
        constexpr auto new_height = 720;

        ResizeImageAllocDest(
            image,
            resized_image,
            new_width,
            new_height,
            ERawImageFormat::BGRA8,
            EGammaSpace::sRGB,
            FImageCore::EResizeImageFilter::AdaptiveSmooth );

        auto & image_wrapper_module = FModuleManager::LoadModuleChecked< IImageWrapperModule >( TEXT( "ImageWrapper" ) );
        const TSharedPtr< IImageWrapper > image_wrapper = image_wrapper_module.CreateImageWrapper( EImageFormat::PNG );
        const auto stride = resized_image.SizeX * 4;

        if ( !image_wrapper.IsValid() )
        {
            UE_LOG( LogLevelStatsCollector, Error, TEXT( "Failed to create image wrapper" ) );
            return false;
        }

        if ( !image_wrapper->SetRaw(
                 resized_image.RawData.GetData(),
                 resized_image.RawData.Num(),
                 resized_image.SizeX,
                 resized_image.SizeY,
                 ERGBFormat::BGRA,
                 8,
                 stride ) )
        {
            UE_LOG( LogLevelStatsCollector, Error, TEXT( "Failed to set raw image data." ) );
            return false;
        }

        const TArray64< uint8 > & png_data = image_wrapper->GetCompressed();
        if ( png_data.Num() == 0 )
        {
            UE_LOG( LogLevelStatsCollector, Error, TEXT( "Failed to compress image data" ) );
            return false;
        }

        if ( !FFileHelper::SaveArrayToFile( png_data, *output_path ) )
        {
            UE_LOG( LogLevelStatsCollector, Error, TEXT( "Failed to save file: %s" ), *output_path );
            return false;
        }
        return true;
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
        Collector->CaptureTopDownMapView();
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

    const auto screenshot_path = FString::Printf( TEXT( "screenshot_cell%d_rotation_%.0f.png" ),
        CurrentCellIndex,
        CurrentRotation );

    Collector->PerformanceReport.AddRotationData(
        CurrentRotation,
        screenshot_path,
        CurrentPerformanceChart->GetMetricsJson() );

    GEngine->RemovePerformanceDataConsumer( CurrentPerformanceChart );
    CurrentPerformanceChart.Reset();
}
