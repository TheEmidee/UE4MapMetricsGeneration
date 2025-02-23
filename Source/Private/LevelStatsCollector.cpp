﻿#include "LevelStatsCollector.h"

#include "LevelStatsCollectorState.h"
#include "LevelStatsPerformanceReport.h"

#include <Components/SceneCaptureComponent2D.h>
#include <Dom/JsonObject.h>
#include <Engine/Engine.h>
#include <Engine/TextureRenderTarget2D.h>
#include <ImageUtils.h>
#include <WorldPartition/WorldPartitionMiniMapHelper.h>

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
    TotalCaptureCount( 0 ),
    CurrentCellIndex( 0 ),
    CurrentRotation( 0.0f ),
    CurrentCaptureDelay( 0.0f ),
    bIsCapturing( false ),
    bIsInitialized( false )
{
    Settings.CameraHeight = 10000.0f;
    Settings.CameraHeightOffset = 250.0f;
    Settings.CameraRotationDelta = 90.0f;
    Settings.CaptureDelay = 0.1f;
    Settings.MetricsDuration = 1.0f;
    Settings.MetricsWaitDelay = 1.0f;
    Settings.CellSize = 10000.0f;
    Settings.GridCenterOffset = FVector::ZeroVector;

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
    InitializeGrid();
    TransitionToState( MakeShared< FIdleState >( this ) );
}

void ALevelStatsCollector::Tick( const float delta_time )
{
    Super::Tick( delta_time );

    if ( !bIsCapturing || !bIsInitialized )
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

bool ALevelStatsCollector::ProcessNextCell()
{
    if ( !GridConfig.IsValidCellIndex( CurrentCellIndex ) )
    {
        PerformanceReport.FinalizeAndSave( GetBasePath(), TotalCaptureCount );
        return false;
    }

    if ( CurrentCellIndex > 0 )
    {
        PerformanceReport.FinishCurrentCell();
    }

    auto & current_cell = GridConfig.GridCells[ CurrentCellIndex ];
    const auto trace_start = current_cell.Center + FVector( 0, 0, Settings.CameraHeight );

    if ( const auto hit_location = TraceGroundPosition( trace_start ) )
    {
        current_cell.GroundHeight = hit_location.GetValue().Z;
        const auto camera_location = hit_location.GetValue() + FVector( 0, 0, Settings.CameraHeightOffset );
        current_cell.CameraHeight = camera_location.Z;
        SetActorLocation( camera_location );
        CurrentRotation = 0.0f;
        CaptureComponent->SetRelativeRotation( FRotator::ZeroRotator );
        PerformanceReport.StartNewCell( CurrentCellIndex, current_cell.Center, current_cell.GroundHeight, current_cell.CameraHeight );
        return true;
    }

    UE_LOG( LogLevelStatsCollector, Warning, TEXT( "Failed to find ground position for cell at %s" ), *current_cell.Center.ToString() );
    CurrentCellIndex++;
    return ProcessNextCell();
}

void ALevelStatsCollector::InitializeGrid()
{
    // :NOTE: Add GridSize in the future as an optional param
    GridConfig.Initialize( Settings.GridCenterOffset, Settings.CellSize ); 
    GridConfig.CalculateBounds( GetWorld() );
    GridConfig.GenerateCells();

    CurrentCellIndex = 0;
    CurrentRotation = 0.0f;
    bIsCapturing = true;
    bIsInitialized = true;

    GridConfig.LogGridInfo();
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

void ALevelStatsCollector::CaptureTopDownMapView()
{
    const auto base_path = GetBasePath();
    IFileManager::Get().MakeDirectory( *base_path, true );
    UTexture2D * overview_texture = nullptr;

    DrawGridDebug();

    FWorldPartitionMiniMapHelper::CaptureBoundsMiniMapToTexture(
        GetWorld(),
        this,
        2048,
        2048,
        overview_texture,
        TEXT( "MiniMapOverview" ),
        GridConfig.GridBounds,
        SCS_FinalColorLDR,
        10 );

    if ( !overview_texture )
    {
        UE_LOG( LogLevelStatsCollector, Error, TEXT( "Failed to capture overview texture" ) );
        return;
    }

    FImage image;
    if ( !FImageUtils::GetTexture2DSourceImage( overview_texture, image ) )
    {
        UE_LOG( LogLevelStatsCollector, Error, TEXT( "Failed to get texture source image" ) );
        return;
    }

    const auto output_path = base_path + TEXT( "map.png" );
    if ( !FImageUtils::SaveImageByExtension( *output_path, image ) )
    {
        UE_LOG( LogLevelStatsCollector, Error, TEXT( "Failed to save overview map to: %s" ), *output_path );
        return;
    }

    UE_LOG( LogLevelStatsCollector, Log, TEXT( "Successfully saved overview map to: %s" ), *output_path );
}

FString ALevelStatsCollector::GetBasePath() const
{
    return FString::Printf( TEXT( "%sSaved/LevelStatsCollector/%s/" ), *FPaths::ProjectDir(), *ReportFolderName );
}

FString ALevelStatsCollector::GetScreenshotPath() const
{
    return FString::Printf( TEXT( "%sscreenshot_cell%d_rotation_%.0f.png" ), *GetBasePath(), CurrentCellIndex, CurrentRotation );
}

FString ALevelStatsCollector::GetJsonOutputPath() const
{
    return GetBasePath() + TEXT( "capture_report.json" );
}

void ALevelStatsCollector::DrawGridDebug() const
{

    constexpr auto grid_debug_lifetime = 2.0f;
    constexpr auto line_thickness = 30.0f;
    const auto grid_color = FColor::White;
    constexpr auto line_height = 8200.0f;

    for ( auto x = 0; x <= GridConfig.GridDimensions.X; x++ )
    {
        const auto line_x = GridConfig.GridBounds.Min.X + ( x * Settings.CellSize );
        const FVector LineStart( line_x, GridConfig.GridBounds.Min.Y, line_height );
        const FVector LineEnd( line_x, GridConfig.GridBounds.Max.Y, line_height );
        DrawDebugLine( GetWorld(), LineStart, LineEnd, grid_color, true, grid_debug_lifetime, 0, line_thickness );
    }

    for ( auto y = 0; y <= GridConfig.GridDimensions.Y; y++ )
    {
        const auto line_y = GridConfig.GridBounds.Min.Y + ( y * Settings.CellSize );
        const FVector LineStart( GridConfig.GridBounds.Min.X, line_y, line_height );
        const FVector LineEnd( GridConfig.GridBounds.Max.X, line_y, line_height );
        DrawDebugLine( GetWorld(), LineStart, LineEnd, grid_color, false, grid_debug_lifetime, 0, line_thickness );
    }
}

// :NOTE: This is just an example of several metrics that can be captured — To be deleted in the future
/*
void FPerformanceMetricsCapture::CaptureMetrics() const
{
    // Basic metrics (existing)
    MetricsObject->SetNumberField("AverageFramerate", GetAverageFramerate());
    MetricsObject->SetNumberField("TotalFrames", GetNumFrames());
    MetricsObject->SetNumberField("TimeDisregarded", TimeDisregarded);
    MetricsObject->SetNumberField("FramesDisregarded", FramesDisregarded);

    // Detailed timing metrics
    TSharedPtr<FJsonObject> TimingObject = MakeShared<FJsonObject>();
    TimingObject->SetNumberField("AverageGameThreadTime", TotalFrameTime_GameThread / GetNumFrames() * 1000.0f);
    TimingObject->SetNumberField("AverageRenderThreadTime", TotalFrameTime_RenderThread / GetNumFrames() * 1000.0f);
    TimingObject->SetNumberField("AverageRHIThreadTime", TotalFrameTime_RHIThread / GetNumFrames() * 1000.0f);
    TimingObject->SetNumberField("AverageGPUTime", TotalFrameTime_GPU / GetNumFrames() * 1000.0f);
    TimingObject->SetNumberField("TotalGameThreadTime", TotalFrameTime_GameThread * 1000.0f);
    TimingObject->SetNumberField("TotalRenderThreadTime", TotalFrameTime_RenderThread * 1000.0f);
    TimingObject->SetNumberField("TotalRHIThreadTime", TotalFrameTime_RHIThread * 1000.0f);
    TimingObject->SetNumberField("TotalGPUTime", TotalFrameTime_GPU * 1000.0f);
    MetricsObject->SetObjectField("Timing", TimingObject);

    // Thread boundedness
    TSharedPtr<FJsonObject> BoundObject = MakeShared<FJsonObject>();
    BoundObject->SetNumberField("GameThreadBound", static_cast<float>(NumFramesBound_GameThread) / GetNumFrames() * 100.0f);
    BoundObject->SetNumberField("RenderThreadBound", static_cast<float>(NumFramesBound_RenderThread) / GetNumFrames() * 100.0f);
    BoundObject->SetNumberField("RHIThreadBound", static_cast<float>(NumFramesBound_RHIThread) / GetNumFrames() * 100.0f);
    BoundObject->SetNumberField("GPUBound", static_cast<float>(NumFramesBound_GPU) / GetNumFrames() * 100.0f);
    BoundObject->SetNumberField("TotalGameThreadBoundFrames", NumFramesBound_GameThread);
    BoundObject->SetNumberField("TotalRenderThreadBoundFrames", NumFramesBound_RenderThread);
    BoundObject->SetNumberField("TotalRHIThreadBoundFrames", NumFramesBound_RHIThread);
    BoundObject->SetNumberField("TotalGPUBoundFrames", NumFramesBound_GPU);
    MetricsObject->SetObjectField("BoundPercentages", BoundObject);

    // Hitching metrics
    TSharedPtr<FJsonObject> HitchObject = MakeShared<FJsonObject>();
    HitchObject->SetNumberField("TotalHitches", GetNumHitches());
    HitchObject->SetNumberField("HitchesPerMinute", GetAvgHitchesPerMinute());
    HitchObject->SetNumberField("PercentTimeHitching", GetPercentHitchTime());
    HitchObject->SetNumberField("TotalHitchTime", GetTotalHitchFrameTime());
    HitchObject->SetNumberField("AverageHitchFrameLength", GetAvgHitchFrameLength());
    HitchObject->SetNumberField("GameThreadBoundHitches", TotalGameThreadBoundHitchCount);
    HitchObject->SetNumberField("RenderThreadBoundHitches", TotalRenderThreadBoundHitchCount);
    HitchObject->SetNumberField("RHIThreadBoundHitches", TotalRHIThreadBoundHitchCount);
    HitchObject->SetNumberField("GPUBoundHitches", TotalGPUBoundHitchCount);
    MetricsObject->SetObjectField("Hitching", HitchObject);

    // Memory metrics (if available)
    TSharedPtr<FJsonObject> MemoryObject = MakeShared<FJsonObject>();
    MemoryObject->SetNumberField("MaxPhysicalMemory", MaxPhysicalMemory);
    MemoryObject->SetNumberField("MinPhysicalMemory", MinPhysicalMemory);
    MemoryObject->SetNumberField("MaxVirtualMemory", MaxVirtualMemory);
    MemoryObject->SetNumberField("MinVirtualMemory", MinVirtualMemory);
    MemoryObject->SetNumberField("MinAvailablePhysicalMemory", MinAvailablePhysicalMemory);
    MemoryObject->SetNumberField("TotalPhysicalMemoryUsed", TotalPhysicalMemoryUsed);
    MemoryObject->SetNumberField("TotalVirtualMemoryUsed", TotalVirtualMemoryUsed);
    MemoryObject->SetNumberField("FramesAtCriticalMemoryPressure", NumFramesAtCriticalMemoryPressure);
    MetricsObject->SetObjectField("Memory", MemoryObject);

    // Draw call and primitive metrics
    TSharedPtr<FJsonObject> RenderingObject = MakeShared<FJsonObject>();
    RenderingObject->SetNumberField("MaxDrawCalls", MaxDrawCalls);
    RenderingObject->SetNumberField("MinDrawCalls", MinDrawCalls);
    RenderingObject->SetNumberField("TotalDrawCalls", TotalDrawCalls);
    RenderingObject->SetNumberField("MaxDrawnPrimitives", MaxDrawnPrimitives);
    RenderingObject->SetNumberField("MinDrawnPrimitives", MinDrawnPrimitives);
    RenderingObject->SetNumberField("TotalDrawnPrimitives", TotalDrawnPrimitives);
    MetricsObject->SetObjectField("Rendering", RenderingObject);

    // Async loading metrics
    TSharedPtr<FJsonObject> LoadingObject = MakeShared<FJsonObject>();
    LoadingObject->SetNumberField("TotalFlushAsyncLoadingTime", TotalFlushAsyncLoadingTime);
    LoadingObject->SetNumberField("MaxFlushAsyncLoadingTime", MaxFlushAsyncLoadingTime);
    LoadingObject->SetNumberField("TotalFlushAsyncLoadingCalls", TotalFlushAsyncLoadingCalls);
    LoadingObject->SetNumberField("TotalSyncLoadCount", TotalSyncLoadCount);
    MetricsObject->SetObjectField("AsyncLoading", LoadingObject);
}
*/