#include "PerfGrapherCommandlet.h"

#include "Chaos/AABB.h"
#include "Dom/JsonObject.h"
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

    if ( FMetricsParams metrics_params; !ParseParams( params, metrics_params ) )
    {
        return 1;
    }
    return 0;
}

bool UPerfGrapherCommandlet::ParseParams( const FString & params_str, FMetricsParams & out_params ) const
{
    TArray< FString > tokens;
    TArray< FString > switches;
    TMap< FString, FString > params_map;
    ParseCommandLine( *params_str, tokens, switches, params_map );

    // Log initial parameters
    UE_LOG( LogPerfGrapher, Display, TEXT( "Parsing commandlet parameters:" ) );
    UE_LOG( LogPerfGrapher, Display, TEXT( "Raw params: %s" ), *params_str );
    UE_LOG( LogPerfGrapher, Display, TEXT( "Found %d tokens, %d switches, %d params" ), tokens.Num(), switches.Num(), params_map.Num() );

    for ( const auto & param : params_map )
    {
        UE_LOG( LogPerfGrapher, Display, TEXT( "Param: %s = %s" ), *param.Key, *param.Value );
    }

    // Set defaults before parsing overrides
    out_params.CellSize = 1000.0f;
    out_params.GridOffset = FVector::ZeroVector;
    out_params.CameraHeight = 170.0f;
    out_params.CameraHeightOffset = 80.0f;
    out_params.CameraRotationDelta = 90.0f;
    out_params.ScreenshotPattern = TEXT( "screenshot_%d_%d_%d" );

    // Override with passed parameters
    if ( params_map.Contains( TEXT( "CellSize" ) ) )
    {
        out_params.CellSize = FCString::Atof( *params_map[ TEXT( "CellSize" ) ] );
    }
    if ( params_map.Contains( TEXT( "GridOffset" ) ) )
    {
        FString GridOffset = params_map[ TEXT( "GridOffset" ) ];
        TArray< FString > Components;
        GridOffset.ParseIntoArray( Components, TEXT( "," ) );
        if ( Components.Num() == 3 )
        {
            out_params.GridOffset = FVector(
                FCString::Atof( *Components[ 0 ] ),
                FCString::Atof( *Components[ 1 ] ),
                FCString::Atof( *Components[ 2 ] ) );
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
    UE_LOG( LogPerfGrapher, Display, TEXT( "Final parameter values:" ) );
    UE_LOG( LogPerfGrapher, Display, TEXT( "CellSize: %.1f" ), out_params.CellSize );
    UE_LOG( LogPerfGrapher, Display, TEXT( "GridOffset: X=%.1f Y=%.1f Z=%.1f" ), out_params.GridOffset.X, out_params.GridOffset.Y, out_params.GridOffset.Z );
    UE_LOG( LogPerfGrapher, Display, TEXT( "CameraHeight: %.1f" ), out_params.CameraHeight );
    UE_LOG( LogPerfGrapher, Display, TEXT( "CameraHeightOffset: %.1f" ), out_params.CameraHeightOffset );
    UE_LOG( LogPerfGrapher, Display, TEXT( "CameraRotationDelta: %.1f" ), out_params.CameraRotationDelta );
    UE_LOG( LogPerfGrapher, Display, TEXT( "ScreenshotPattern: %s" ), *out_params.ScreenshotPattern );

    return true;
}