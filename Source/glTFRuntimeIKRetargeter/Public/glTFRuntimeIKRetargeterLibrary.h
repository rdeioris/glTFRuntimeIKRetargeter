// Copyright 2025, Roberto De Ioris.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "glTFRuntimeAsset.h"
#include "Retargeter/IKRetargeter.h"
#include "glTFRuntimeIKRetargeterLibrary.generated.h"

/**
 *
 */
UCLASS()
class GLTFRUNTIMEIKRETARGETER_API UglTFRuntimeIKRetargeterLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static UAnimSequence* LoadAndIKRetargetSkeletalAnimationByName(UglTFRuntimeAsset* Asset, const FString& AnimationName, USkeletalMesh* SrcSkeletalMesh, USkeletalMesh* DstSkeletalMesh, UIKRetargeter* IKRetargeter, const FglTFRuntimeSkeletalAnimationConfig& SkeletalAnimationConfig, const bool bCaseSensitive);

};
