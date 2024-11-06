#include "LevelStatsCollector.h"

#include <Components/SceneCaptureComponent2D.h>
#include <Engine/Engine.h>
#include <Engine/LevelBounds.h>
#include <Engine/TextureRenderTarget2D.h>
#include <ImageUtils.h>

FCustomPerformanceChart::FCustomPerformanceChart( const FDateTime & in_start_time, const FString & in_chart_label, const FString & in_output_path ) :
    FPerformanceTrackingChart( in_start_time, in_chart_label ),
    CustomOutputPath( in_output_path )
{
}

void FCustomPerformanceChart::DumpFPSChartToCustomLocation( const FString & in_map_name )
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

FString FCustomPerformanceChart::CreateFileNameForChart( const FString &, const FString &, const FString & file_extension )
{
    const FString platform = FPlatformProperties::PlatformName();
    return TEXT( "metrics" ) + file_extension;
}

ALevelStatsCollector::ALevelStatsCollector() :
    CurrentState( ECaptureState::Idle ),
    CurrentMetricsCaptureTime( 0.0f ),
    MetricsDuration( 5.0f ),
    MetricsWaitDelay( 1.0f ),
    CellSize( 10000.0f ),
    GridCenterOffset( FVector::ZeroVector ),
    CameraHeight( 10000.0f ),
    CameraHeightOffset( 250.0f ),
    CameraRotationDelta( 90.0f ),
    CaptureDelay( 0.1f ),
    CurrentCellIndex( 0 ),
    CurrentRotation( 0.0f ),
    CurrentCaptureDelay( 0.0f ),
    TotalCaptureCount( 0 ),
    bIsCapturing( false ),
    bIsInitialized( false )

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
    IConsoleManager::Get().FindConsoleVariable( TEXT( "t.FPSChart.OpenFolderOnDump" ) )->Set( 0 );
    CurrentState = ECaptureState::Idle;
    InitializeGrid();
}

void ALevelStatsCollector::Tick( float delta_time )
{
    Super::Tick( delta_time );

    if ( !bIsCapturing || !bIsInitialized )
    {
        return;
    }

    switch ( CurrentState )
    {
        case ECaptureState::Idle:
            CurrentCaptureDelay += delta_time;
            if ( CurrentCaptureDelay >= MetricsWaitDelay )
            {
                CurrentCaptureDelay = 0.0f;
                StartMetricsCapture();
            }
            break;

        case ECaptureState::CapturingMetrics:
            ProcessMetricsCapture( delta_time );
            break;

        case ECaptureState::WaitingForSnapshot:
            CurrentCaptureDelay += delta_time;
            if ( CurrentCaptureDelay >= CaptureDelay )
            {
                CurrentCaptureDelay = 0.0f;
                CaptureCurrentView();
                CurrentState = ECaptureState::ProcessingNextRotation;
            }
            break;

        case ECaptureState::ProcessingNextRotation:
            CurrentRotation += CameraRotationDelta;
            if ( CurrentRotation >= 360.0f )
            {
                CurrentCellIndex++;
                CurrentState = ECaptureState::ProcessingNextCell;
            }
            else
            {
                CaptureComponent->SetRelativeRotation( FRotator( 0.0f, CurrentRotation, 0.0f ) );
                CurrentState = ECaptureState::Idle;
            }
            break;

        case ECaptureState::ProcessingNextCell:
            if ( !ProcessNextCell() )
            {
                bIsCapturing = false;
                UE_LOG( LogTemp, Log, TEXT( "Capture process complete! Total captures: %d" ), TotalCaptureCount );
            }
            else
            {
                CurrentState = ECaptureState::Idle;
            }
            break;
    }
}

void ALevelStatsCollector::SetupSceneCapture() const
{
    if ( !CaptureComponent )
    {
        UE_LOG( LogTemp, Error, TEXT( "CaptureComponent is not set!" ) );
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

void ALevelStatsCollector::InitializeGrid()
{
    CalculateGridBounds();

    const auto total_cells = GridDimensions.X * GridDimensions.Y;
    GridCells.Empty( total_cells );

    for ( auto y = 0; y < GridDimensions.Y; ++y )
    {
        for ( auto x = 0; x < GridDimensions.X; ++x )
        {
            GridCells.Emplace( GridBounds.Min + FVector( x * CellSize + CellSize / 2, y * CellSize + CellSize / 2, 0.0f ) );
        }
    }

    CurrentCellIndex = 0;
    CurrentRotation = 0.0f;
    bIsCapturing = true;
    bIsInitialized = true;

    LogGridInfo();
    ProcessNextCell();
}

bool ALevelStatsCollector::ProcessNextCell()
{
    if ( !GridCells.IsValidIndex( CurrentCellIndex ) )
    {
        return false;
    }

    auto & current_cell = GridCells[ CurrentCellIndex ];
    const auto trace_start = current_cell.Center + FVector( 0, 0, CameraHeight );

    if ( const auto hit_location = TraceGroundPosition( trace_start ) )
    {
        current_cell.GroundHeight = hit_location.GetValue().Z;
        const auto camera_location = hit_location.GetValue() + FVector( 0, 0, CameraHeightOffset );
        SetActorLocation( camera_location );
        CurrentRotation = 0.0f;
        CaptureComponent->SetRelativeRotation( FRotator::ZeroRotator );
        return true;
    }

    UE_LOG( LogTemp, Warning, TEXT( "Failed to find ground position for cell at %s" ), *current_cell.Center.ToString() );
    CurrentCellIndex++;
    return ProcessNextCell();
}

void ALevelStatsCollector::CaptureCurrentView()
{
    if ( CaptureComponent == nullptr || CaptureComponent->TextureTarget == nullptr )
    {
        UE_LOG( LogTemp, Error, TEXT( "Invalid capture component or render target!" ) );
        return;
    }

    const auto current_path = GetCurrentRotationPath();
    IFileManager::Get().MakeDirectory( *current_path, true );

    CaptureComponent->CaptureScene();

    FImage image;
    if ( !FImageUtils::GetRenderTargetImage( Cast< UTextureRenderTarget >( CaptureComponent->TextureTarget ), image ) )
    {
        UE_LOG( LogTemp, Error, TEXT( "Failed to get render target image for cell %d rotation %.0f" ), CurrentCellIndex, CurrentRotation );
        return;
    }

    const auto screenshot_path = current_path + TEXT( "screenshot.png" );
    if ( FImageUtils::SaveImageByExtension( *screenshot_path, image ) )
    {
        const auto & current_cell = GridCells[ CurrentCellIndex ];
        UE_LOG( LogTemp, Log, TEXT( "Image captured at coordinates (%f, %f, %f), saved to: %s" ), current_cell.Center.X, current_cell.Center.Y, current_cell.Center.Z, *screenshot_path );
        TotalCaptureCount++;
    }
    else
    {
        UE_LOG( LogTemp, Error, TEXT( "Failed to save image: %s" ), *screenshot_path );
    }
}

TOptional< FVector > ALevelStatsCollector::TraceGroundPosition( const FVector & start_location ) const
{
    FHitResult hit_result;
    const auto end_location = start_location - FVector( 0, 0, CameraHeight * 2 );

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

    const auto finalize_bounds = [ & ]() {
        // :NOTE: Add padding to ensure we capture the edges properly
        const auto bounds_padding = CellSize * 0.5f;
        level_bounds = level_bounds.ExpandBy( FVector( bounds_padding, bounds_padding, 0 ) );

        const auto origin = level_bounds.GetCenter();
        const auto extent = level_bounds.GetExtent();

        GridBounds.Min = FVector(
                             FMath::FloorToFloat( origin.X - extent.X ) / CellSize * CellSize,
                             FMath::FloorToFloat( origin.Y - extent.Y ) / CellSize * CellSize,
                             0 ) +
                         GridCenterOffset;

        GridBounds.Max = FVector(
                             FMath::CeilToFloat( origin.X + extent.X ) / CellSize * CellSize,
                             FMath::CeilToFloat( origin.Y + extent.Y ) / CellSize * CellSize,
                             0 ) +
                         GridCenterOffset;

        const auto grid_size = GridBounds.Max - GridBounds.Min;
        GridDimensions = FIntPoint(
            FMath::CeilToInt( grid_size.X / CellSize ),
            FMath::CeilToInt( grid_size.Y / CellSize ) );

        const auto expected_size_x = GridDimensions.X * CellSize;
        const auto expected_size_y = GridDimensions.Y * CellSize;
        const auto actual_size_x = grid_size.X;
        const auto actual_size_y = grid_size.Y;

        if ( !FMath::IsNearlyEqual( expected_size_x, actual_size_x, KINDA_SMALL_NUMBER ) ||
             !FMath::IsNearlyEqual( expected_size_y, actual_size_y, KINDA_SMALL_NUMBER ) )
        {
            GridBounds.Max = GridBounds.Min + FVector( expected_size_x, expected_size_y, 0.0f );

            UE_LOG( LogTemp, Warning, TEXT( "Grid size adjusted for cell alignment. Original: (%f, %f), Adjusted: (%f, %f)" ), actual_size_x, actual_size_y, expected_size_x, expected_size_y );
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

        UE_LOG( LogTemp, Log, TEXT( "Using explicit grid dimensions: %f x %f" ), GridSizeX, GridSizeY );
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
                UE_LOG( LogTemp, Log, TEXT( "Got bounds from LevelBoundsActor: %s" ), *level_bounds.ToString() );
                finalize_bounds();
                return;
            }
        }
    }

    // :NOTE: Use default area if no bounds found
    UE_LOG( LogTemp, Warning, TEXT( "No valid bounds source found, using default 10000x10000 area" ) );
    level_bounds = FBox( FVector( -5000, -5000, 0 ), FVector( 5000, 5000, 0 ) ); // Arbitrary default area
    finalize_bounds();
}

void ALevelStatsCollector::StartMetricsCapture()
{
    const auto label = FString::Printf( TEXT( "Cell_%d_Rot_%.0f" ),
        CurrentCellIndex,
        CurrentRotation );

    if ( CurrentPerformanceChart.IsValid() )
    {
        GEngine->RemovePerformanceDataConsumer( CurrentPerformanceChart );
        CurrentPerformanceChart.Reset();
    }

    CurrentPerformanceChart = MakeShareable( new FCustomPerformanceChart(
        FDateTime::Now(),
        label,
        GetCurrentRotationPath() ) );

    GEngine->AddPerformanceDataConsumer( CurrentPerformanceChart );

    CurrentMetricsCaptureTime = 0.0f;
    CurrentState = ECaptureState::CapturingMetrics;
}

void ALevelStatsCollector::ProcessMetricsCapture( float delta_time )
{
    CurrentMetricsCaptureTime += delta_time;

    if ( CurrentMetricsCaptureTime >= MetricsDuration )
    {
        FinishMetricsCapture();
        CurrentState = ECaptureState::WaitingForSnapshot;
    }
}

void ALevelStatsCollector::FinishMetricsCapture()
{
    if ( CurrentPerformanceChart.IsValid() )
    {
        const auto cell_name = FString::Printf( TEXT( "Cell_%d_Rot_%.0f" ), CurrentCellIndex, CurrentRotation );
        CurrentPerformanceChart->DumpFPSChartToCustomLocation( cell_name );
        GEngine->RemovePerformanceDataConsumer( CurrentPerformanceChart );
        CurrentPerformanceChart.Reset();
    }
}

FString ALevelStatsCollector::GetBasePath() const
{
    const FString base_path = TEXT( "Saved/LevelStatsCollector/" );
    return FPaths::ProjectDir() + base_path;
}

FString ALevelStatsCollector::GetCurrentCellPath() const
{
    return GetBasePath() + FString::Printf( TEXT( "Cell_%d/" ), CurrentCellIndex );
}

FString ALevelStatsCollector::GetCurrentRotationPath() const
{
    return GetCurrentCellPath() + FString::Printf( TEXT( "Rotation_%.0f/" ), CurrentRotation );
}

void ALevelStatsCollector::LogGridInfo() const
{
    UE_LOG( LogTemp, Log, TEXT( "Grid Configuration:" ) );
    UE_LOG( LogTemp, Log, TEXT( "  Bounds: Min(%s), Max(%s)" ), *GridBounds.Min.ToString(), *GridBounds.Max.ToString() );
    UE_LOG( LogTemp, Log, TEXT( "  Dimensions: %dx%d cells" ), GridDimensions.X, GridDimensions.Y );
    UE_LOG( LogTemp, Log, TEXT( "  Cell Size: %f" ), CellSize );
    UE_LOG( LogTemp, Log, TEXT( "  Total Cells: %d" ), GridCells.Num() );
    UE_LOG( LogTemp, Log, TEXT( "  Center Offset: %s" ), *GridCenterOffset.ToString() );
    UE_LOG( LogTemp, Log, TEXT( "Camera Configuration:" ) );
    UE_LOG( LogTemp, Log, TEXT( "  Height: %f" ), CameraHeight );
    UE_LOG( LogTemp, Log, TEXT( "  Height Offset: %f" ), CameraHeightOffset );
    UE_LOG( LogTemp, Log, TEXT( "  Rotation Delta: %f" ), CameraRotationDelta );
    UE_LOG( LogTemp, Log, TEXT( "Capture Configuration:" ) );
    UE_LOG( LogTemp, Log, TEXT( "  Capture Delay: %f" ), CaptureDelay );
}