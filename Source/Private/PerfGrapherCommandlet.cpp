#include "PerfGrapherCommandlet.h"

#include "Chaos/AABB.h"
#include "Dom/JsonObject.h"
#include "Misc/OutputDevice.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <Editor.h>
#include <Engine/LevelStreaming.h>
#include <Engine/World.h>
#include <Misc/PackageName.h>
// ReSharper disable once CppInconsistentNaming
DEFINE_LOG_CATEGORY_STATIC( LogPerfGrapher, Verbose, All )

UPerfGrapherCommandlet::UPerfGrapherCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = true;

    // Set up commandlet help info
    HelpDescription = TEXT( "Generate metrics and screenshots for a map using a grid system" );
    HelpUsage = TEXT( "<MapName> -CellSize=<size> [-GridOffset=X,Y,Z] [-CameraHeight=<height>] [-CameraHeightOffset=<offset>] [-CameraRotationDelta=<angle>] [-ScreenshotPattern=<pattern>]" );

    HelpParamNames.Add( TEXT( "MapName" ) );
    HelpParamNames.Add( TEXT( "CellSize" ) );
    HelpParamNames.Add( TEXT( "GridOffset" ) );
    HelpParamNames.Add( TEXT( "CameraHeight" ) );
    HelpParamNames.Add( TEXT( "CameraHeightOffset" ) );
    HelpParamNames.Add( TEXT( "CameraRotationDelta" ) );
    HelpParamNames.Add( TEXT( "ScreenshotPattern" ) );

    HelpParamDescriptions.Add( TEXT( "Name of the map to process" ) );
    HelpParamDescriptions.Add( TEXT( "Size of each grid cell" ) );
    HelpParamDescriptions.Add( TEXT( "Optional offset for the grid origin (X,Y,Z)" ) );
    HelpParamDescriptions.Add( TEXT( "Height to place camera (default: 170)" ) );
    HelpParamDescriptions.Add( TEXT( "Additional height offset after ground trace (default: 80)" ) );
    HelpParamDescriptions.Add( TEXT( "Rotation angle between screenshots (default: 90)" ) );
    HelpParamDescriptions.Add( TEXT( "Pattern for screenshot filenames (default: screenshot_%d_%d_%d)" ) );
}

int32 UPerfGrapherCommandlet::Main( const FString & params )
{
    UE_LOG( LogPerfGrapher, Log, TEXT( "--------------------------------------------------------------------------------------------" ) );
    UE_LOG( LogPerfGrapher, Log, TEXT( "Running MapMetricsGeneration Commandlet" ) );

    TMap< FString, FString > params_map;
    FMetricsParams metrics_params;

    if ( !ParseParams( params, metrics_params, params_map ) )
    {
        UE_LOG( LogPerfGrapher, Error, TEXT( "Failed to parse parameters" ) );
        return 1;
    }

    TArray< FString > package_names;

    for ( const auto & param_key_pair : params_map )
    {
        if ( param_key_pair.Key == "Map" )
        {
            auto map_parameter_value = param_key_pair.Value;

            const auto add_package = [ &package_names ]( const FString & package_name ) {
                FString map_file;
                FPackageName::SearchForPackageOnDisk( package_name, nullptr, &map_file );

                if ( map_file.IsEmpty() )
                {
                    UE_LOG( LogPerfGrapher, Error, TEXT( "Could not find package %s" ), *package_name );
                }
                else
                {
                    package_names.Add( *map_file );
                }
            };

            add_package( map_parameter_value );
        }
    }

    if ( package_names.Num() == 0 )
    {
        UE_LOG( LogPerfGrapher, Error, TEXT( "No maps were checked" ) );
        return 2;
    }

    for ( const auto & package_name : package_names )
    {
        auto * package = LoadPackage( nullptr, *package_name, 0 );
        if ( package == nullptr )
        {
            UE_LOG( LogPerfGrapher, Error, TEXT( "Could not load package %s" ), *package_name );
            return 2;
        }

        auto * world = UWorld::FindWorldInPackage( package );
        if ( world == nullptr )
        {
            UE_LOG( LogPerfGrapher, Error, TEXT( "Could not get a world in the package %s" ), *package_name );
            return 2;
        }

        world->WorldType = EWorldType::Editor;
    }
    return 0;
}

bool UPerfGrapherCommandlet::ParseParams( const FString & params, FMetricsParams & out_params, TMap< FString, FString > & params_map ) const
{
    TArray< FString > tokens;
    TArray< FString > switches;
    ParseCommandLine( *params, tokens, switches, params_map );

    // Log initial parameters
    UE_LOG( LogPerfGrapher, Display, TEXT( "Parsing commandlet parameters:" ) );
    UE_LOG( LogPerfGrapher, Display, TEXT( "Raw params: %s" ), *params );
    UE_LOG( LogPerfGrapher, Display, TEXT( "Found %d tokens, %d switches, %d params" ), tokens.Num(), switches.Num(), params_map.Num() );

    for ( const auto & param : params_map )
    {
        UE_LOG( LogPerfGrapher, Display, TEXT( "In Param: %s = %s" ), *param.Key, *param.Value );
    }

    // Set defaults before parsing overrides
    out_params.MapName = TEXT( "default_map" ); // :NOTE: This is a placeholder you can edit to set a default map
    out_params.CellSize = 1000.0f;
    out_params.GridOffset = FVector::ZeroVector;
    out_params.CameraHeight = 170.0f;
    out_params.CameraHeightOffset = 80.0f;
    out_params.CameraRotationDelta = 90.0f;
    out_params.ScreenshotPattern = TEXT( "screenshot_%d_%d_%d" );

    // Override with passed parameters
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

    if ( params_map.Contains( TEXT( "ScreenshotPattern" ) ) )
    {
        out_params.ScreenshotPattern = params_map[ TEXT( "ScreenshotPattern" ) ];
    }

    // Log final values
    for ( const auto & param : params_map )
    {
        UE_LOG( LogPerfGrapher, Display, TEXT( "Out Param: %s = %s" ), *param.Key, *param.Value );
    }

    return true;
}