#pragma once

#include <CoreMinimal.h>

#include "PerfGrapherObserver.generated.h"

UCLASS()
class MAPMETRICSGENERATION_API APerfGrapherObserver final : public AActor
{
    GENERATED_BODY()
public:
    void PostInitializeComponents() override;
};
