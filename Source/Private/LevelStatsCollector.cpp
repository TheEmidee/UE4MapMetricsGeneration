#include "LevelStatsCollector.h"

#include <Components/SceneCaptureComponent2D.h>
#include <Engine/TextureRenderTarget2D.h>
#include <Engine/World.h>
#include <ImageUtils.h>
#include <Kismet/GameplayStatics.h>

ALevelStatsCollector::ALevelStatsCollector()
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

    CalculateGridBounds();
    CurrentCell = GridMin;
    CurrentRotation = 0.0f;
    CurrentCaptureCount = 0;
    bIsCapturing = true;

    ProcessNextCell();
}

void ALevelStatsCollector::Tick( float delta_time )
{
    Super::Tick( delta_time );

    if ( !bIsCapturing )
        return;

    // Take the next capture after a small delay to ensure previous capture is complete
    static float delay = 0.1f;
    static float current_delay = 0.0f;

    current_delay += delta_time;
    if ( current_delay >= delay )
    {
        current_delay = 0.0f;
        CaptureCurrentView();

        // Rotate for next capture or move to next cell
        CurrentRotation += CameraRotationDelta;
        if ( CurrentRotation >= 360.0f )
        {
            CurrentRotation = 0.0f;
            if ( !MoveToNextPosition() )
            {
                bIsCapturing = false;
                UE_LOG( LogTemp, Log, TEXT( "Capture process complete!" ) );
            }
        }

        // Update capture component rotation
        const FRotator new_rotation( 0.0f, CurrentRotation, 0.0f );
        CaptureComponent->SetRelativeRotation( new_rotation );
    }
}

void ALevelStatsCollector::SetupSceneCapture() const
{
    if ( CaptureComponent )
    {
        // Create render target
        UTextureRenderTarget2D * render_target = NewObject< UTextureRenderTarget2D >();
        render_target->InitCustomFormat( 1920, 1080, PF_B8G8R8A8, true ); // HD resolution
        render_target->UpdateResource();

        // Setup capture component
        CaptureComponent->CaptureSource = SCS_FinalColorLDR;
        CaptureComponent->TextureTarget = render_target;
        CaptureComponent->FOVAngle = 90.0f; // Standard FPS FOV
        CaptureComponent->bCaptureEveryFrame = false;
    }
}

void ALevelStatsCollector::ProcessNextCell()
{
    const FVector trace_start = CurrentCell + FVector( 0, 0, CameraHeight );
    FVector hit_location;

    if ( TraceGroundPosition( trace_start, hit_location ) )
    {
        // Position camera at hit location + offset
        const FVector camera_location = hit_location + FVector( 0, 0, CameraHeightOffset );
        SetActorLocation( camera_location );

        // Reset rotation for new cell
        CurrentRotation = 0.0f;
        const FRotator new_rotation( 0.0f, CurrentRotation, 0.0f );
        CaptureComponent->SetRelativeRotation( new_rotation );
    }
}

void ALevelStatsCollector::CaptureCurrentView()
{
    if ( !CaptureComponent || !CaptureComponent->TextureTarget )
    {
        return;
    }

    // Generate filename based on position and rotation
    const auto file_name = FString::Printf( TEXT( "/Game/Captures/capture_%lld_%lld_%lld_rot_%d" ),
        FMath::RoundToInt( CurrentCell.X ),
        FMath::RoundToInt( CurrentCell.Y ),
        FMath::RoundToInt( CurrentCell.Z ),
        FMath::RoundToInt( CurrentRotation ) );

    // Capture the view
    CaptureComponent->CaptureScene();

    // Get the captured image
    FImage image;
    if ( !FImageUtils::GetRenderTargetImage( Cast< UTextureRenderTarget >( CaptureComponent->TextureTarget ), image ) )
    {
        UE_LOG( LogTemp, Error, TEXT( "Failed to get render target image for: %s" ), *file_name );
        return;
    }

    // Ensure screenshots directory exists
    const FString screenshot_dir = FPaths::ProjectDir() + TEXT( "Saved/Screenshots/" );
    IFileManager::Get().MakeDirectory( *screenshot_dir, true );

    // Save as PNG file
    const FString screenshot_path = screenshot_dir + file_name + TEXT( ".png" );
    if ( FImageUtils::SaveImageByExtension( *screenshot_path, image ) )
    {
        UE_LOG( LogTemp, Log, TEXT( "Captured view saved: %s" ), *file_name );
        CurrentCaptureCount++;
    }
    else
    {
        UE_LOG( LogTemp, Error, TEXT( "Failed to save image: %s" ), *screenshot_path );
    }
}

bool ALevelStatsCollector::TraceGroundPosition( const FVector & start_location, FVector & outhit_location ) const
{
    FHitResult hit_result;
    const FVector end_location = start_location - FVector( 0, 0, CameraHeight * 2 );

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor( this );

    if ( GetWorld()->LineTraceSingleByChannel( hit_result, start_location, end_location, ECC_Visibility, QueryParams ) )
    {
        outhit_location = hit_result.Location;
        return true;
    }

    return false;
}

void ALevelStatsCollector::CalculateGridBounds()
{
    // Calculate bounds based on all visible geometry in the level
    FBox level_bounds( ForceInit );

    // Collect all visible static meshes in the level
    TArray< AActor * > found_actors;
    UGameplayStatics::GetAllActorsOfClass( GetWorld(), AActor::StaticClass(), found_actors );

    for ( AActor * actor : found_actors )
    {
        if ( actor && actor != this )
        {
            level_bounds += actor->GetComponentsBoundingBox();
        }
    }

    const FVector Origin = level_bounds.GetCenter();
    const FVector Extent = level_bounds.GetExtent();

    // Calculate grid bounds based on cell size
    GridMin = FVector(
                  FMath::FloorToFloat( Origin.X - Extent.X ) / CellSize * CellSize,
                  FMath::FloorToFloat( Origin.Y - Extent.Y ) / CellSize * CellSize,
                  0 ) +
              GridOffset;

    GridMax = FVector(
                  FMath::CeilToFloat( Origin.X + Extent.X ) / CellSize * CellSize,
                  FMath::CeilToFloat( Origin.Y + Extent.Y ) / CellSize * CellSize,
                  0 ) +
              GridOffset;
}

bool ALevelStatsCollector::MoveToNextPosition()
{
    CurrentCell.X += CellSize;

    if ( CurrentCell.X > GridMax.X )
    {
        CurrentCell.X = GridMin.X;
        CurrentCell.Y += CellSize;

        if ( CurrentCell.Y > GridMax.Y )
            return false;
    }

    ProcessNextCell();
    return true;
}