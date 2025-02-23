#include "LevelStatsCollectorCommandlet.h"

#include <Dom/JsonObject.h>
#include <Editor.h>
#include <Engine/LevelStreaming.h>
#include <Engine/World.h>
#include <LevelStatsCollector.h>
#include <Misc/OutputDevice.h>
#include <Misc/PackageName.h>
#include <Serialization/JsonSerializer.h>
#include <Serialization/JsonWriter.h>
#include <WorldPartition/WorldPartition.h>
// :NOTE: ReSharper disable once CppInconsistentNaming
DEFINE_LOG_CATEGORY_STATIC( LogPerfGrapher, Verbose, All )

class FWorldHandler
{
public:
    explicit FWorldHandler( UWorld * world ) :
        World( world )
    {
        if ( World != nullptr )
        {
            Initialize();
        }
    }
    ~FWorldHandler()
    {
        Cleanup();
    }

    FWorldHandler( const FWorldHandler & ) = delete;
    FWorldHandler & operator=( const FWorldHandler & ) = delete;

    UWorld * GetWorld() const;

private:
    void Initialize() const
    {
        if ( World->GetWorldPartition() )
        {
            World->GetWorldPartition()->Initialize( World, FTransform::Identity );
        }

        World->WorldType = EWorldType::Editor;
        World->AddToRoot();

        if ( !World->bIsWorldInitialized )
        {
            UWorld::InitializationValues IVs;
            IVs.RequiresHitProxies( true )
                .ShouldSimulatePhysics( true )
                .EnableTraceCollision( true )
                .CreateNavigation( true )
                .CreateAISystem( true )
                .AllowAudioPlayback( true )
                .CreatePhysicsScene( true );

            World->InitWorld( IVs );
            World->PersistentLevel->UpdateModelComponents();
            World->UpdateWorldComponents( true, false );
        }
    }

    void Cleanup() const
    {
        if ( World->GetWorldPartition() )
        {
            World->GetWorldPartition()->Uninitialize();
        }

        World->CleanupWorld();
        World->RemoveFromRoot();
        World->FlushLevelStreaming( EFlushLevelStreamingType::Full );

        // :NOTE: Force garbage collection
        CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
    }

    UWorld * World;
};

FORCEINLINE UWorld * FWorldHandler::GetWorld() const
{
    return World;
}

ULevelStatsCollectorCommandlet::ULevelStatsCollectorCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = true;

    // :NOTE: Set up commandlet help info
    HelpDescription = TEXT( "Generate metrics and screenshots for a map using a grid system" );
    HelpUsage = TEXT( "[-Map=MapName] [-CellSize=<size>] [-GridOffset=X,Y,Z] [-CameraHeight=<height>] [-CameraHeightOffset=<offset>] [-CameraRotationDelta=<angle>] [-CameraFOVAngle=<angle>] [-ScreenshotPattern=<pattern>]" );

    HelpParamNames.Add( TEXT( "MapName" ) );
    HelpParamNames.Add( TEXT( "CellSize" ) );
    HelpParamNames.Add( TEXT( "GridOffset" ) );
    HelpParamNames.Add( TEXT( "CameraHeight" ) );
    HelpParamNames.Add( TEXT( "CameraHeightOffset" ) );
    HelpParamNames.Add( TEXT( "CameraRotationDelta" ) );
    HelpParamNames.Add( TEXT( "CameraFOVAngle" ) );
    HelpParamNames.Add( TEXT( "ScreenshotPattern" ) );

    HelpParamDescriptions.Add( TEXT( "Name of the map to process" ) );
    HelpParamDescriptions.Add( TEXT( "Size of each grid cell" ) );
    HelpParamDescriptions.Add( TEXT( "Optional offset for the grid origin (X,Y,Z)" ) );
    HelpParamDescriptions.Add( TEXT( "Height to place camera (default: 170)" ) );
    HelpParamDescriptions.Add( TEXT( "Additional height offset after ground trace (default: 80)" ) );
    HelpParamDescriptions.Add( TEXT( "Rotation angle between screenshots (default: 90)" ) );
    HelpParamDescriptions.Add( TEXT( "Field of view angle for the camera (default: 90)" ) );
    HelpParamDescriptions.Add( TEXT( "Pattern for screenshot filenames (default: screenshot_%d_%d_%d)" ) );
}

int32 ULevelStatsCollectorCommandlet::Main( const FString & params )
{
    UE_LOG( LogPerfGrapher, Log, TEXT( "--------------------------------------------------------------------------------------------" ) );
    UE_LOG( LogPerfGrapher, Log, TEXT( "Running PerfGrapher Commandlet" ) );

    TMap< FString, FString > params_map;
    FMetricsParams metrics_params;

    if ( !ParseParams( params, metrics_params, params_map ) )
    {
        UE_LOG( LogPerfGrapher, Error, TEXT( "Failed to parse parameters" ) );
        return 1;
    }
    UE_LOG( LogPerfGrapher, Log, TEXT( "Parameters parsed successfully" ) );

    TArray< FString > package_names;

    for ( const auto & param_key_pair : params_map )
    {
        if ( param_key_pair.Key == "Map" )
        {
            auto map_parameter_value = param_key_pair.Value;
            UE_LOG( LogPerfGrapher, Log, TEXT( "Processing map: %s" ), *map_parameter_value );

            const auto add_package = [ &package_names ]( const FString & package_name ) {
                FString map_file;
                FPackageName::SearchForPackageOnDisk( package_name, nullptr, &map_file );

                if ( map_file.IsEmpty() )
                {
                    UE_LOG( LogPerfGrapher, Error, TEXT( "Could not find package %s" ), *package_name );
                }
                else
                {
                    UE_LOG( LogPerfGrapher, Log, TEXT( "Found package file: %s" ), *map_file );
                    package_names.Add( *map_file );
                }
            };

            TArray< FString > maps_package_names;
            map_parameter_value.ParseIntoArray( maps_package_names, TEXT( "+" ) );

            if ( maps_package_names.Num() > 0 )
            {
                for ( const auto & map_package_name : maps_package_names )
                {
                    add_package( map_package_name );
                }
            }
            else
            {
                add_package( map_parameter_value );
            }
        }
    }

    UE_LOG( LogPerfGrapher, Log, TEXT( "Found %d packages to process" ), package_names.Num() );

    if ( package_names.Num() == 0 )
    {
        UE_LOG( LogPerfGrapher, Error, TEXT( "No maps were checked" ) );
        return 2;
    }

    for ( const auto & package_name : package_names )
    {
        UE_LOG( LogPerfGrapher, Log, TEXT( "Processing package: %s" ), *package_name );
        if ( !RunLevelStatsCommandlet( package_name, metrics_params ) )
        {
            UE_LOG( LogPerfGrapher, Error, TEXT( "Failed to process map %s" ), *package_name );
            return 1;
        }
    }

    UE_LOG( LogPerfGrapher, Log, TEXT( "All packages processed successfully" ) );
    return 0;
}

bool ULevelStatsCollectorCommandlet::RunLevelStatsCommandlet( const FString & package_name, const FMetricsParams & metrics_params ) const
{
    auto * package = LoadPackage( nullptr, *package_name, LOAD_None );
    if ( package == nullptr )
    {
        UE_LOG( LogPerfGrapher, Error, TEXT( "Cannot load package %s" ), *package_name );
        return false;
    }
    UE_LOG( LogPerfGrapher, Log, TEXT( "Package %s loaded" ), *package_name );

    auto * world = UWorld::FindWorldInPackage( package );
    if ( world == nullptr )
    {
        UE_LOG( LogPerfGrapher, Error, TEXT( "Cannot get a world in the package %s" ), *package_name );
        return false;
    }
    UE_LOG( LogPerfGrapher, Log, TEXT( "World %s found" ), *world->GetName() );

    {
        const FWorldHandler world_handler( world );
        UE_LOG( LogPerfGrapher, Log, TEXT( "World %s initialized" ), *world->GetName() );

        const auto * Collector = world_handler.GetWorld()->SpawnActor< ALevelStatsCollector >( FVector::ZeroVector, FRotator::ZeroRotator );

        UE_LOG( LogPerfGrapher, Log, TEXT( "Attempting to spawn collector..." ) );
        if ( Collector == nullptr )
        {
            UE_LOG( LogPerfGrapher, Error, TEXT( "Failed to spawn collector" ) );
            return false;
        }
        UE_LOG( LogPerfGrapher, Log, TEXT( "Collector spawned successfully at location %s" ), *Collector->GetActorLocation().ToString() );
    }

    UE_LOG( LogPerfGrapher, Log, TEXT( "World %s cleaned up" ), *world->GetName() );
    return true;
}

bool ULevelStatsCollectorCommandlet::ParseParams( const FString & params, FMetricsParams & out_params, TMap< FString, FString > & params_map ) const
{
    TArray< FString > tokens;
    TArray< FString > switches;
    ParseCommandLine( *params, tokens, switches, params_map );

    UE_LOG( LogPerfGrapher, Display, TEXT( "Parsing commandlet parameters:" ) );
    UE_LOG( LogPerfGrapher, Display, TEXT( "Raw params: %s" ), *params );
    UE_LOG( LogPerfGrapher, Display, TEXT( "Found %d tokens, %d switches, %d params" ), tokens.Num(), switches.Num(), params_map.Num() );

    for ( const auto & param : params_map )
    {
        UE_LOG( LogPerfGrapher, Display, TEXT( "In Param: %s = %s" ), *param.Key, *param.Value );
    }

    // :NOTE: Set defaults before parsing overrides
    out_params.MapName = TEXT( "" );
    out_params.CellSize = 1000.0f;
    out_params.GridOffset = FVector::ZeroVector;
    out_params.CameraHeight = 170.0f;
    out_params.CameraHeightOffset = 80.0f;
    out_params.CameraRotationDelta = 90.0f;
    out_params.CameraFOVAngle = 90.0f;
    out_params.ScreenshotPattern = TEXT( "screenshot_%d_%d_%d" );

    // :NOTE: Override with passed parameters
    if ( params_map.Contains( TEXT( "Map" ) ) )
    {
        out_params.MapName = params_map[ TEXT( "Map" ) ];
    }

    if ( params_map.Contains( TEXT( "CellSize" ) ) )
    {
        out_params.CellSize = FCString::Atof( *params_map[ TEXT( "CellSize" ) ] );
    }
    if ( params_map.Contains( TEXT( "GridOffset" ) ) )
    {
        const auto grid_offset = params_map[ TEXT( "GridOffset" ) ];
        TArray< FString > components;
        grid_offset.ParseIntoArray( components, TEXT( "," ) );
        if ( components.Num() == 3 )
        {
            out_params.GridOffset = FVector(
                FCString::Atof( *components[ 0 ] ),
                FCString::Atof( *components[ 1 ] ),
                FCString::Atof( *components[ 2 ] ) );
        }
    }

    if ( params_map.Contains( TEXT( "CameraHeight" ) ) )
    {
        out_params.CameraHeight = FCString::Atof( *params_map[ TEXT( "CameraHeight" ) ] );
    }

    if ( params_map.Contains( TEXT( "CameraHeightOffset" ) ) )
    {
        out_params.CameraHeightOffset = FCString::Atof( *params_map[ TEXT( "CameraHeightOffset" ) ] );
    }

    if ( params_map.Contains( TEXT( "CameraRotationDelta" ) ) )
    {
        out_params.CameraRotationDelta = FCString::Atof( *params_map[ TEXT( "CameraRotationDelta" ) ] );
    }

    if ( params_map.Contains( TEXT( "CameraFOVAngle" ) ) )
    {
        out_params.CameraFOVAngle = FCString::Atof( *params_map[ TEXT( "CameraFOVAngle" ) ] );
    }

    if ( params_map.Contains( TEXT( "ScreenshotPattern" ) ) )
    {
        out_params.ScreenshotPattern = params_map[ TEXT( "ScreenshotPattern" ) ];
    }

    // :NOTE: Log final values
    for ( const auto & param : params_map )
    {
        UE_LOG( LogPerfGrapher, Display, TEXT( "Out Param: %s = %s" ), *param.Key, *param.Value );
    }

    return true;
}