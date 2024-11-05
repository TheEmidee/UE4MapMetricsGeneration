#include "LevelStatsCollector.h"

#include <Components/SceneCaptureComponent2D.h>
#include <Engine/LevelBounds.h>
#include <Engine/TextureRenderTarget2D.h>
#include <ImageUtils.h>

ALevelStatsCollector::ALevelStatsCollector() :
    CellSize( 10000.0f ),
    GridCenterOffset( FVector::ZeroVector ),
    CameraHeight( 10000.0f ),
    CameraHeightOffset( 250.0f ),
    CameraRotationDelta( 90.0f ),
    CaptureDelay( 0.1f ),
    OutputDirectory( TEXT( "Saved/Screenshots/LevelStatsCollector/" ) ),
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
    InitializeGrid();
}

void ALevelStatsCollector::Tick( float delta_time )
{
    Super::Tick( delta_time );

    if ( !bIsCapturing || !bIsInitialized )
    {
        return;
    }

    // :NOTE: Take the next capture after a small delay to ensure previous capture is complete
    CurrentCaptureDelay += delta_time;
    if ( CurrentCaptureDelay >= CaptureDelay )
    {
        CurrentCaptureDelay = 0.0f;
        CaptureCurrentView();

        CurrentRotation += CameraRotationDelta;
        if ( CurrentRotation >= 360.0f )
        {
            CurrentCellIndex++;
            if ( !ProcessNextCell() )
            {
                bIsCapturing = false;
                UE_LOG( LogTemp, Log, TEXT( "Capture process complete! Total captures: %d" ), TotalCaptureCount );
            }
        }

        CaptureComponent->SetRelativeRotation( FRotator( 0.0f, CurrentRotation, 0.0f ) );
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

    // :NOTE: Calculate total number of cells and initialize grid
    const auto total_cells = GridDimensions.X * GridDimensions.Y;
    GridCells.Empty( total_cells );

    // :NOTE: Initialize grid cells
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
        // :NOTE: Set up the new cell position and reset rotation
        current_cell.GroundHeight = hit_location.GetValue().Z;
        const auto camera_location = hit_location.GetValue() + FVector( 0, 0, CameraHeightOffset );
        SetActorLocation( camera_location );
        CurrentRotation = 0.0f;
        CaptureComponent->SetRelativeRotation( FRotator::ZeroRotator );
        return true;
    }

    // :NOTE: If we failed to process current cell, log warning and try next cell
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

    const auto file_name = GenerateFileName();

    // :NOTE: Capture the view
    CaptureComponent->CaptureScene();

    // :NOTE: Get the captured image
    FImage image;
    if ( !FImageUtils::GetRenderTargetImage( Cast< UTextureRenderTarget >( CaptureComponent->TextureTarget ), image ) )
    {
        UE_LOG( LogTemp, Error, TEXT( "Failed to get render target image for: %s" ), *file_name );
        return;
    }

    // :NOTE: Ensure screenshots directory exists
    const auto screenshot_dir = FPaths::ProjectDir() + OutputDirectory;
    IFileManager::Get().MakeDirectory( *screenshot_dir, true );

    // :NOTE: For now, Save as PNG file with unique (non normalized) name
    const auto screenshot_path = screenshot_dir + file_name + TEXT( ".png" );
    if ( FImageUtils::SaveImageByExtension( *screenshot_path, image ) )
    {
        const auto & current_cell = GridCells[ CurrentCellIndex ];
        UE_LOG( LogTemp, Log, TEXT( "Image captured at coordinates (%f, %f, %f), with name: %s" ), current_cell.Center.X, current_cell.Center.Y, current_cell.Center.Z, *file_name );
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

    auto FinalizeBounds = [ & ]() {
        // :NOTE: Add padding to ensure we capture the edges properly
        const auto bounds_padding = CellSize * 0.5f;
        level_bounds = level_bounds.ExpandBy( FVector( bounds_padding, bounds_padding, 0 ) );

        // :NOTE: Calculate grid bounds ensuring alignment with CellSize
        const auto origin = level_bounds.GetCenter();
        const auto extent = level_bounds.GetExtent();

        // :NOTE: Calculate initial grid bounds
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

        // :NOTE: Calculate and validate grid dimensions
        const auto grid_size = GridBounds.Max - GridBounds.Min;
        GridDimensions = FIntPoint(
            FMath::CeilToInt( grid_size.X / CellSize ),
            FMath::CeilToInt( grid_size.Y / CellSize ) );

        // :NOTE: Ensure cell alignment
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

        // :NOTE: Store the actual sizes back to our parameters
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
        FinalizeBounds();
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
                FinalizeBounds();
                return;
            }
        }
    }

    // :NOTE: Use default area if no bounds found
    UE_LOG( LogTemp, Warning, TEXT( "No valid bounds source found, using default 10000x10000 area" ) );
    level_bounds = FBox( FVector( -5000, -5000, 0 ), FVector( 5000, 5000, 0 ) ); // Arbitrary default area
    FinalizeBounds();
}

FString ALevelStatsCollector::GenerateFileName() const
{
    if ( !GridCells.IsValidIndex( CurrentCellIndex ) )
    {
        return TEXT( "invalid_capture" );
    }

    const FGridCell & current_cell = GridCells[ CurrentCellIndex ];
    return FString::Printf( TEXT( "capture_%lld_%lld_%lld_rot_%d" ),
        FMath::RoundToInt( current_cell.Center.X ),
        FMath::RoundToInt( current_cell.Center.Y ),
        FMath::RoundToInt( current_cell.Center.Z ),
        FMath::RoundToInt( CurrentRotation ) );
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
    UE_LOG( LogTemp, Log, TEXT( "  Output Directory: %s" ), *OutputDirectory );
}