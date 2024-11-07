#include "LevelStatsCollector.h"

#include "LevelStatsCollectorState.h"
#include "LevelStatsPerformanceReport.h"

#include <Components/SceneCaptureComponent2D.h>
#include <Dom/JsonObject.h>
#include <Dom/JsonValue.h>
#include <Engine/Engine.h>
#include <Engine/LevelBounds.h>
#include <Engine/TextureRenderTarget2D.h>
#include <ImageUtils.h>
#include <Serialization/JsonSerializer.h>
#include <Serialization/JsonWriter.h>

DEFINE_LOG_CATEGORY( LogLevelStatsCollector );

void FPerformanceMetricsCapture::CaptureMetrics() const
{
    // :NOTE: Core Performance Metrics
    const auto core_object = MakeShared< FJsonObject >();
    core_object->SetNumberField( "AverageFramerate", GetAverageFramerate() );
    core_object->SetNumberField( "TotalFrames", GetNumFrames() );
    MetricsObject->SetObjectField( "CoreMetrics", core_object );

    // :NOTE: Frame Time Analysis
    const auto frametime_object = MakeShared< FJsonObject >();
    const auto num_frames_float = static_cast< float >( GetNumFrames() );

    // :NOTE: Main thread times (in milliseconds)
    frametime_object->SetNumberField( "GameThread_Avg", ( TotalFrameTime_GameThread / num_frames_float ) * 1000.0f );
    frametime_object->SetNumberField( "RenderThread_Avg", ( TotalFrameTime_RenderThread / num_frames_float ) * 1000.0f );
    frametime_object->SetNumberField( "GPU_Avg", ( TotalFrameTime_GPU / num_frames_float ) * 1000.0f );

    // :NOTE: Thread bottleneck analysis
    frametime_object->SetNumberField( "GameThread_Bound_Pct", ( static_cast< float >( NumFramesBound_GameThread ) / num_frames_float ) * 100.0f );
    frametime_object->SetNumberField( "RenderThread_Bound_Pct", ( static_cast< float >( NumFramesBound_RenderThread ) / num_frames_float ) * 100.0f );
    frametime_object->SetNumberField( "GPU_Bound_Pct", ( static_cast< float >( NumFramesBound_GPU ) / num_frames_float ) * 100.0f );
    MetricsObject->SetObjectField( "FrameTime", frametime_object );

    // :NOTE: Hitching Analysis
    const auto hitch_object = MakeShared< FJsonObject >();
    hitch_object->SetNumberField( "Total_Hitches", GetNumHitches() );
    hitch_object->SetNumberField( "Hitches_Per_Minute", GetAvgHitchesPerMinute() );
    hitch_object->SetNumberField( "Average_Hitch_Length_MS", GetAvgHitchFrameLength() * 1000.0f );

    // :NOTE: Hitch categorization
    const auto total_hitches = static_cast< float >( GetNumHitches() );
    if ( total_hitches > 0 )
    {
        hitch_object->SetNumberField( "GameThread_Hitches_Pct", ( static_cast< float >( TotalGameThreadBoundHitchCount ) / total_hitches ) * 100.0f );
        hitch_object->SetNumberField( "RenderThread_Hitches_Pct", ( static_cast< float >( TotalRenderThreadBoundHitchCount ) / total_hitches ) * 100.0f );
        hitch_object->SetNumberField( "GPU_Hitches_Pct", ( static_cast< float >( TotalGPUBoundHitchCount ) / total_hitches ) * 100.0f );
    }
    MetricsObject->SetObjectField( "Hitching", hitch_object );

    // :NOTE: Memory Health
    const auto mem_object = MakeShared< FJsonObject >();
    mem_object->SetNumberField( "Physical_Memory_Used_MB", static_cast< float >( MaxPhysicalMemory ) / ( 1024.0f * 1024.0f ) );
    mem_object->SetNumberField( "Available_Physical_Memory_MB", static_cast< float >( MinAvailablePhysicalMemory ) / ( 1024.0f * 1024.0f ) );
    mem_object->SetNumberField( "Memory_Pressure_Frames_Pct", ( static_cast< float >( NumFramesAtCriticalMemoryPressure ) / num_frames_float ) * 100.0f );
    MetricsObject->SetObjectField( "Memory", mem_object );

    // :NOTE: Rendering Stats
    const auto rendering_object = MakeShared< FJsonObject >();
    rendering_object->SetNumberField( "Average_DrawCalls", static_cast< float >( TotalDrawCalls ) / num_frames_float );
    rendering_object->SetNumberField( "Peak_DrawCalls", static_cast< float >( MaxDrawCalls ) );
    rendering_object->SetNumberField( "Average_Primitives", static_cast< float >( TotalDrawnPrimitives ) / num_frames_float );
    rendering_object->SetNumberField( "Peak_Primitives", static_cast< float >( MaxDrawnPrimitives ) );
    MetricsObject->SetObjectField( "Rendering", rendering_object );

    // :NOTE: Loading Performance
    const auto loading_object = MakeShared< FJsonObject >();
    if ( TotalFlushAsyncLoadingCalls > 0 )
    {
        loading_object->SetNumberField( "Average_AsyncLoad_Time_MS", ( TotalFlushAsyncLoadingTime / static_cast< float >( TotalFlushAsyncLoadingCalls ) ) * 1000.0f );
        loading_object->SetNumberField( "Peak_AsyncLoad_Time_MS", MaxFlushAsyncLoadingTime * 1000.0f );
        loading_object->SetNumberField( "Total_AsyncLoad_Calls", TotalFlushAsyncLoadingCalls );
    }
    loading_object->SetNumberField( "Sync_Load_Count", TotalSyncLoadCount );
    MetricsObject->SetObjectField( "Loading", loading_object );
}

ALevelStatsCollector::ALevelStatsCollector() :
    GridCenterOffset( FVector::ZeroVector ),
    TotalCaptureCount( 0 ),
    CurrentCellIndex( 0 ),
    CurrentRotation( 0.0f ),
    CurrentCaptureDelay( 0.0f ),
    IsCaptureInProgress( false ),
    IsCollectorInitialized( false )

{
    PrimaryActorTick.bCanEverTick = true;

    CaptureComponent = CreateDefaultSubobject< USceneCaptureComponent2D >( TEXT( "CaptureComponent" ) );
    RootComponent = CaptureComponent;
}

void ALevelStatsCollector::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    SetupSceneCapture();
}

void ALevelStatsCollector::BeginPlay()
{

    Super::BeginPlay();

    ReportFolderName = FString::Printf( TEXT( "Report_%s" ), *FDateTime::Now().ToString( TEXT( "%Y-%m-%d_%H-%M-%S" ) ) );

    IConsoleManager::Get().FindConsoleVariable( TEXT( "t.FPSChart.OpenFolderOnDump" ) )->Set( 0 );
    PerformanceReport.Initialize( GetWorld(), Settings );
    TransitionToState( MakeShared< FIdleState >( this ) );
    InitializeGrid();
}

void ALevelStatsCollector::Tick( const float delta_time )
{
    Super::Tick( delta_time );

    if ( !IsCaptureInProgress || !IsCollectorInitialized )
    {
        return;
    }

    if ( CurrentState != nullptr )
    {
        CurrentState->Tick( delta_time );
    }
}

void ALevelStatsCollector::TransitionToState( const TSharedPtr< FLevelStatsCollectorState > & new_state )
{
    if ( CurrentState != nullptr )
    {
        CurrentState->Exit();
    }

    CurrentState = new_state;
    CurrentState->Enter();
}

void ALevelStatsCollector::UpdateRotation()
{
    CurrentRotation += Settings.CameraRotationDelta;
    CaptureComponent->SetRelativeRotation( FRotator( 0.0f, CurrentRotation, 0.0f ) );
}

void ALevelStatsCollector::IncrementCellIndex()
{
    CurrentCellIndex++;
    CurrentRotation = 0.0f;
}

void ALevelStatsCollector::FinishCapture()
{
    IsCaptureInProgress = false;
    UE_LOG( LogLevelStatsCollector, Log, TEXT( "Capture process complete! Total captures: %d" ), TotalCaptureCount );
}

bool ALevelStatsCollector::ProcessNextCell()
{
    if ( !GridCells.IsValidIndex( CurrentCellIndex ) )
    {
        PerformanceReport.FinalizeAndSave( GetBasePath(), TotalCaptureCount );
        return false;
    }

    auto & current_cell = GridCells[ CurrentCellIndex ];
    const auto trace_start = current_cell.Center + FVector( 0, 0, Settings.CameraHeight );

    if ( const auto hit_location = TraceGroundPosition( trace_start ) )
    {
        current_cell.GroundHeight = hit_location.GetValue().Z;
        const auto camera_location = hit_location.GetValue() + FVector( 0, 0, Settings.CameraHeightOffset );
        SetActorLocation( camera_location );
        CurrentRotation = 0.0f;
        CaptureComponent->SetRelativeRotation( FRotator::ZeroRotator );
        PerformanceReport.StartNewCell( CurrentCellIndex, current_cell.Center, current_cell.GroundHeight );
        return true;
    }

    UE_LOG( LogLevelStatsCollector, Warning, TEXT( "Failed to find ground position for cell at %s" ), *current_cell.Center.ToString() );
    CurrentCellIndex++;
    return ProcessNextCell();
}

void ALevelStatsCollector::CaptureCurrentView()
{
    if ( CaptureComponent == nullptr || CaptureComponent->TextureTarget == nullptr )
    {
        UE_LOG( LogLevelStatsCollector, Error, TEXT( "Invalid capture component or render target!" ) );
        return;
    }

    const auto current_path = GetCurrentRotationPath();
    IFileManager::Get().MakeDirectory( *current_path, true );

    CaptureComponent->CaptureScene();

    FImage image;
    if ( !FImageUtils::GetRenderTargetImage( Cast< UTextureRenderTarget >( CaptureComponent->TextureTarget ), image ) )
    {
        UE_LOG( LogLevelStatsCollector, Error, TEXT( "Failed to get render target image for cell %d rotation %.0f" ), CurrentCellIndex, CurrentRotation );
        return;
    }

    const auto screenshot_path = FString::Printf( TEXT( "%sscreenshot.png" ), *current_path );
    if ( FImageUtils::SaveImageByExtension( *screenshot_path, image ) )
    {
        const auto & current_cell = GridCells[ CurrentCellIndex ];
        UE_LOG( LogLevelStatsCollector,
            Log,
            TEXT( "Image captured at coordinates (%f, %f, %f), saved to: %s" ),
            current_cell.Center.X,
            current_cell.Center.Y,
            current_cell.Center.Z,
            *screenshot_path );

        TotalCaptureCount++;
    }
    else
    {
        UE_LOG( LogLevelStatsCollector, Error, TEXT( "Failed to save image: %s" ), *screenshot_path );
    }
}

void ALevelStatsCollector::StartMetricsCapture()
{
    const auto label = FString::Printf( TEXT( "Cell_%d_Rot_%.0f" ), CurrentCellIndex, CurrentRotation );

    if ( CurrentPerformanceChart.IsValid() )
    {
        GEngine->RemovePerformanceDataConsumer( CurrentPerformanceChart );
        CurrentPerformanceChart.Reset();
    }

    CurrentPerformanceChart = MakeShareable( new FPerformanceMetricsCapture( FDateTime::Now(), label ) );

    GEngine->AddPerformanceDataConsumer( CurrentPerformanceChart );
}

void ALevelStatsCollector::FinishMetricsCapture()
{
    if ( CurrentPerformanceChart.IsValid() )
    {
        AddRotationToReport();
        GEngine->RemovePerformanceDataConsumer( CurrentPerformanceChart );
        CurrentPerformanceChart.Reset();
    }
}

void ALevelStatsCollector::InitializeGrid()
{
    CalculateGridBounds();

    const auto total_cells = GridDimensions.X * GridDimensions.Y;
    GridCells.Empty( total_cells );

    for ( auto y = 0; y < GridDimensions.Y; ++y )
    {
        for ( auto x = 0; x < GridDimensions.X; ++x )
        {
            GridCells.Emplace( GridBounds.Min + FVector( x * Settings.CellSize + Settings.CellSize / 2,
                                                    y * Settings.CellSize + Settings.CellSize / 2,
                                                    0.0f ) );
        }
    }

    CurrentCellIndex = 0;
    CurrentRotation = 0.0f;
    IsCaptureInProgress = true;
    IsCollectorInitialized = true;

    LogGridInfo();
    ProcessNextCell();
}

void ALevelStatsCollector::SetupSceneCapture() const
{
    if ( CaptureComponent == nullptr )
    {
        UE_LOG( LogLevelStatsCollector, Error, TEXT( "CaptureComponent is not set!" ) );
        return;
    }

    auto * render_target = NewObject< UTextureRenderTarget2D >();
    render_target->InitCustomFormat( 1920, 1080, PF_B8G8R8A8, false );
    render_target->TargetGamma = 2.2f;
    render_target->UpdateResource();

    CaptureComponent->CaptureSource = SCS_FinalColorLDR;
    CaptureComponent->TextureTarget = render_target;
    CaptureComponent->FOVAngle = 90.0f;
    CaptureComponent->bCaptureEveryFrame = false;
}

TOptional< FVector > ALevelStatsCollector::TraceGroundPosition( const FVector & start_location ) const
{
    FHitResult hit_result;
    const auto end_location = start_location - FVector( 0, 0, Settings.CameraHeight * 2 );

    FCollisionQueryParams query_params;
    query_params.AddIgnoredActor( this );

    if ( GetWorld()->LineTraceSingleByChannel( hit_result, start_location, end_location, ECC_Visibility, query_params ) )
    {
        return hit_result.Location;
    }

    return TOptional< FVector >();
}

void ALevelStatsCollector::CalculateGridBounds()
{
    FBox level_bounds( ForceInit );

    const auto finalize_bounds = [ & ] {
        // :NOTE: Add padding to ensure we capture the edges properly
        const auto bounds_padding = Settings.CellSize * 0.5f;
        level_bounds = level_bounds.ExpandBy( FVector( bounds_padding, bounds_padding, 0 ) );

        const auto origin = level_bounds.GetCenter();
        const auto extent = level_bounds.GetExtent();

        GridBounds.Min = FVector(
                             FMath::FloorToFloat( origin.X - extent.X ) / Settings.CellSize * Settings.CellSize,
                             FMath::FloorToFloat( origin.Y - extent.Y ) / Settings.CellSize * Settings.CellSize,
                             0 ) +
                         GridCenterOffset;

        GridBounds.Max = FVector(
                             FMath::CeilToFloat( origin.X + extent.X ) / Settings.CellSize * Settings.CellSize,
                             FMath::CeilToFloat( origin.Y + extent.Y ) / Settings.CellSize * Settings.CellSize,
                             0 ) +
                         GridCenterOffset;

        const auto grid_size = GridBounds.Max - GridBounds.Min;
        GridDimensions = FIntPoint(
            FMath::CeilToInt( grid_size.X / Settings.CellSize ),
            FMath::CeilToInt( grid_size.Y / Settings.CellSize ) );

        const auto expected_size_x = GridDimensions.X * Settings.CellSize;
        const auto expected_size_y = GridDimensions.Y * Settings.CellSize;
        const auto actual_size_x = grid_size.X;
        const auto actual_size_y = grid_size.Y;

        if ( !FMath::IsNearlyEqual( expected_size_x, actual_size_x, KINDA_SMALL_NUMBER ) ||
             !FMath::IsNearlyEqual( expected_size_y, actual_size_y, KINDA_SMALL_NUMBER ) )
        {
            GridBounds.Max = GridBounds.Min + FVector( expected_size_x, expected_size_y, 0.0f );

            UE_LOG( LogLevelStatsCollector,
                Warning,
                TEXT( "Grid size adjusted for cell alignment. Original: (%f, %f), Adjusted: (%f, %f)" ),
                actual_size_x,
                actual_size_y,
                expected_size_x,
                expected_size_y );
        }

        GridSizeX = expected_size_x;
        GridSizeY = expected_size_y;
    };

    // :NOTE: Use explicit grid dimensions if provided
    if ( GridSizeX > 0.0f && GridSizeY > 0.0f )
    {
        const auto half_size_x = GridSizeX * 0.5f;
        const auto half_size_y = GridSizeY * 0.5f;

        level_bounds = FBox(
            FVector( -half_size_x, -half_size_y, 0.0f ),
            FVector( half_size_x, half_size_y, 0.0f ) );

        UE_LOG( LogLevelStatsCollector, Log, TEXT( "Using explicit grid dimensions: %f x %f" ), GridSizeX, GridSizeY );
        finalize_bounds();
        return;
    }

    // :NOTE: Use LevelBoundsActor if no explicit dimensions provided
    if ( const auto * current_level = GetWorld()->GetCurrentLevel() )
    {
        if ( const auto * level_bounds_actor = current_level->LevelBoundsActor.Get() )
        {
            level_bounds = level_bounds_actor->GetComponentsBoundingBox( true );
            if ( level_bounds.IsValid )
            {
                UE_LOG( LogLevelStatsCollector, Log, TEXT( "Got bounds from LevelBoundsActor: %s" ), *level_bounds.ToString() );
                finalize_bounds();
                return;
            }
        }
    }

    // :NOTE: Use default area if no bounds found
    UE_LOG( LogLevelStatsCollector, Warning, TEXT( "No valid bounds source found, using default 10000x10000 area" ) );
    level_bounds = FBox( FVector( -5000, -5000, 0 ), FVector( 5000, 5000, 0 ) );
    finalize_bounds();
}

void ALevelStatsCollector::AddRotationToReport() const
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

    PerformanceReport.AddRotationData(
        CurrentCellIndex,
        CurrentRotation,
        screenshot_path,
        CurrentPerformanceChart->GetMetricsJson(),
        GetCurrentRotationPath() );
}

FString ALevelStatsCollector::GetBasePath() const
{
    return FString::Printf( TEXT( "%sSaved/LevelStatsCollector/%s/" ), *FPaths::ProjectDir(), *ReportFolderName );
}

FString ALevelStatsCollector::GetCurrentCellPath() const
{
    return FString::Printf( TEXT( "%sCell_%d/" ), *GetBasePath(), CurrentCellIndex );
}

FString ALevelStatsCollector::GetCurrentRotationPath() const
{
    return FString::Printf( TEXT( "%sRotation_%.0f/" ), *GetCurrentCellPath(), CurrentRotation );
}

FString ALevelStatsCollector::GetJsonOutputPath() const
{
    return GetBasePath() + TEXT( "capture_report.json" );
}

void ALevelStatsCollector::LogGridInfo() const
{
    UE_LOG( LogLevelStatsCollector, Log, TEXT( "Grid Configuration:" ) );
    UE_LOG( LogLevelStatsCollector, Log, TEXT( "  Bounds: Min(%s), Max(%s)" ), *GridBounds.Min.ToString(), *GridBounds.Max.ToString() );
    UE_LOG( LogLevelStatsCollector, Log, TEXT( "  Dimensions: %dx%d cells" ), GridDimensions.X, GridDimensions.Y );
    UE_LOG( LogLevelStatsCollector, Log, TEXT( "  Cell Size: %f" ), Settings.CellSize );
    UE_LOG( LogLevelStatsCollector, Log, TEXT( "  Total Cells: %d" ), GridCells.Num() );
    UE_LOG( LogLevelStatsCollector, Log, TEXT( "  Center Offset: %s" ), *GridCenterOffset.ToString() );
    UE_LOG( LogLevelStatsCollector, Log, TEXT( "Camera Configuration:" ) );
    UE_LOG( LogLevelStatsCollector, Log, TEXT( "  Height: %f" ), Settings.CameraHeight );
    UE_LOG( LogLevelStatsCollector, Log, TEXT( "  Height Offset: %f" ), Settings.CameraHeightOffset );
    UE_LOG( LogLevelStatsCollector, Log, TEXT( "  Rotation Delta: %f" ), Settings.CameraRotationDelta );
    UE_LOG( LogLevelStatsCollector, Log, TEXT( "Capture Configuration:" ) );
    UE_LOG( LogLevelStatsCollector, Log, TEXT( "  Capture Delay: %f" ), Settings.CaptureDelay );
}

//    void CaptureMetrics() const
// {
//     // Basic metrics (existing)
//     MetricsObject->SetNumberField("AverageFramerate", GetAverageFramerate());
//     MetricsObject->SetNumberField("TotalFrames", GetNumFrames());
//     MetricsObject->SetNumberField("TimeDisregarded", TimeDisregarded);
//     MetricsObject->SetNumberField("FramesDisregarded", FramesDisregarded);
//
//     // Detailed timing metrics
//     TSharedPtr<FJsonObject> TimingObject = MakeShared<FJsonObject>();
//     TimingObject->SetNumberField("AverageGameThreadTime", TotalFrameTime_GameThread / GetNumFrames() * 1000.0f);
//     TimingObject->SetNumberField("AverageRenderThreadTime", TotalFrameTime_RenderThread / GetNumFrames() * 1000.0f);
//     TimingObject->SetNumberField("AverageRHIThreadTime", TotalFrameTime_RHIThread / GetNumFrames() * 1000.0f);
//     TimingObject->SetNumberField("AverageGPUTime", TotalFrameTime_GPU / GetNumFrames() * 1000.0f);
//     TimingObject->SetNumberField("TotalGameThreadTime", TotalFrameTime_GameThread * 1000.0f);
//     TimingObject->SetNumberField("TotalRenderThreadTime", TotalFrameTime_RenderThread * 1000.0f);
//     TimingObject->SetNumberField("TotalRHIThreadTime", TotalFrameTime_RHIThread * 1000.0f);
//     TimingObject->SetNumberField("TotalGPUTime", TotalFrameTime_GPU * 1000.0f);
//     MetricsObject->SetObjectField("Timing", TimingObject);
//
//     // Thread boundedness
//     TSharedPtr<FJsonObject> BoundObject = MakeShared<FJsonObject>();
//     BoundObject->SetNumberField("GameThreadBound", static_cast<float>(NumFramesBound_GameThread) / GetNumFrames() * 100.0f);
//     BoundObject->SetNumberField("RenderThreadBound", static_cast<float>(NumFramesBound_RenderThread) / GetNumFrames() * 100.0f);
//     BoundObject->SetNumberField("RHIThreadBound", static_cast<float>(NumFramesBound_RHIThread) / GetNumFrames() * 100.0f);
//     BoundObject->SetNumberField("GPUBound", static_cast<float>(NumFramesBound_GPU) / GetNumFrames() * 100.0f);
//     BoundObject->SetNumberField("TotalGameThreadBoundFrames", NumFramesBound_GameThread);
//     BoundObject->SetNumberField("TotalRenderThreadBoundFrames", NumFramesBound_RenderThread);
//     BoundObject->SetNumberField("TotalRHIThreadBoundFrames", NumFramesBound_RHIThread);
//     BoundObject->SetNumberField("TotalGPUBoundFrames", NumFramesBound_GPU);
//     MetricsObject->SetObjectField("BoundPercentages", BoundObject);
//
//     // Hitching metrics
//     TSharedPtr<FJsonObject> HitchObject = MakeShared<FJsonObject>();
//     HitchObject->SetNumberField("TotalHitches", GetNumHitches());
//     HitchObject->SetNumberField("HitchesPerMinute", GetAvgHitchesPerMinute());
//     HitchObject->SetNumberField("PercentTimeHitching", GetPercentHitchTime());
//     HitchObject->SetNumberField("TotalHitchTime", GetTotalHitchFrameTime());
//     HitchObject->SetNumberField("AverageHitchFrameLength", GetAvgHitchFrameLength());
//     HitchObject->SetNumberField("GameThreadBoundHitches", TotalGameThreadBoundHitchCount);
//     HitchObject->SetNumberField("RenderThreadBoundHitches", TotalRenderThreadBoundHitchCount);
//     HitchObject->SetNumberField("RHIThreadBoundHitches", TotalRHIThreadBoundHitchCount);
//     HitchObject->SetNumberField("GPUBoundHitches", TotalGPUBoundHitchCount);
//     MetricsObject->SetObjectField("Hitching", HitchObject);
//
//     // Memory metrics (if available)
//     TSharedPtr<FJsonObject> MemoryObject = MakeShared<FJsonObject>();
//     MemoryObject->SetNumberField("MaxPhysicalMemory", MaxPhysicalMemory);
//     MemoryObject->SetNumberField("MinPhysicalMemory", MinPhysicalMemory);
//     MemoryObject->SetNumberField("MaxVirtualMemory", MaxVirtualMemory);
//     MemoryObject->SetNumberField("MinVirtualMemory", MinVirtualMemory);
//     MemoryObject->SetNumberField("MinAvailablePhysicalMemory", MinAvailablePhysicalMemory);
//     MemoryObject->SetNumberField("TotalPhysicalMemoryUsed", TotalPhysicalMemoryUsed);
//     MemoryObject->SetNumberField("TotalVirtualMemoryUsed", TotalVirtualMemoryUsed);
//     MemoryObject->SetNumberField("FramesAtCriticalMemoryPressure", NumFramesAtCriticalMemoryPressure);
//     MetricsObject->SetObjectField("Memory", MemoryObject);
//
//     // Draw call and primitive metrics
//     TSharedPtr<FJsonObject> RenderingObject = MakeShared<FJsonObject>();
//     RenderingObject->SetNumberField("MaxDrawCalls", MaxDrawCalls);
//     RenderingObject->SetNumberField("MinDrawCalls", MinDrawCalls);
//     RenderingObject->SetNumberField("TotalDrawCalls", TotalDrawCalls);
//     RenderingObject->SetNumberField("MaxDrawnPrimitives", MaxDrawnPrimitives);
//     RenderingObject->SetNumberField("MinDrawnPrimitives", MinDrawnPrimitives);
//     RenderingObject->SetNumberField("TotalDrawnPrimitives", TotalDrawnPrimitives);
//     MetricsObject->SetObjectField("Rendering", RenderingObject);
//
//     // Async loading metrics
//     TSharedPtr<FJsonObject> LoadingObject = MakeShared<FJsonObject>();
//     LoadingObject->SetNumberField("TotalFlushAsyncLoadingTime", TotalFlushAsyncLoadingTime);
//     LoadingObject->SetNumberField("MaxFlushAsyncLoadingTime", MaxFlushAsyncLoadingTime);
//     LoadingObject->SetNumberField("TotalFlushAsyncLoadingCalls", TotalFlushAsyncLoadingCalls);
//     LoadingObject->SetNumberField("TotalSyncLoadCount", TotalSyncLoadCount);
//     MetricsObject->SetObjectField("AsyncLoading", LoadingObject);
// }