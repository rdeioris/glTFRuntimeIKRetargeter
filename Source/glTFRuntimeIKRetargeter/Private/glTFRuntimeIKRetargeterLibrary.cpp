// Copyright 2025, Roberto De Ioris.


#include "glTFRuntimeIKRetargeterLibrary.h"
#include "Retargeter/IKRetargetProcessor.h"

UAnimSequence* UglTFRuntimeIKRetargeterLibrary::LoadAndIKRetargetSkeletalAnimationByName(UglTFRuntimeAsset* Asset, const FString& AnimationName, USkeletalMesh* SrcSkeletalMesh, USkeletalMesh* DstSkeletalMesh, UIKRetargeter* IKRetargeter, const FglTFRuntimeSkeletalAnimationConfig& SkeletalAnimationConfig, const bool bCaseSensitive)
{
	if (!SrcSkeletalMesh || !DstSkeletalMesh || !IKRetargeter)
	{
		return nullptr;
	}

	TMap<FString, FRawAnimSequenceTrack> Tracks;
	TMap<FName, TArray<TPair<float, float>>> MorphTargetCurves;
	float Duration = 0;

	if (!Asset->GetParser()->LoadAnimationByNameAsTracksAndMorphTargets(AnimationName, Tracks, MorphTargetCurves, Duration, SkeletalAnimationConfig, bCaseSensitive))
	{
		return nullptr;
	}

	const int32 NumFrames = FMath::Max<int32>(Duration * SkeletalAnimationConfig.FramesPerSecond, 1);

	for (TPair<FString, FRawAnimSequenceTrack>& Pair : Tracks)
	{
		if (!Asset->GetParser()->SanitizeBoneTrack(SrcSkeletalMesh->GetSkeleton()->GetReferenceSkeleton(), Pair.Key, NumFrames, Pair.Value, SkeletalAnimationConfig))
		{
			UE_LOG(LogGLTFRuntime, Warning, TEXT("Unable to sanitize bone track %s for the IK Retargeter"), *Pair.Key);
			return nullptr;
		}
	}

	UIKRetargetProcessor* Processor = NewObject<UIKRetargetProcessor>();
	FRetargetProfile RetargetProfile;
	IKRetargeter->FillProfileWithAssetSettings(RetargetProfile);
	Processor->Initialize(SrcSkeletalMesh, DstSkeletalMesh, IKRetargeter, RetargetProfile);
	if (!Processor->IsInitialized())
	{
		UE_LOG(LogGLTFRuntime, Warning, TEXT("Unable to initialize the IK Retargeter"));
		return nullptr;
	}

	// target skeleton data
	const FRetargetSkeleton& TargetSkeleton = Processor->GetSkeleton(ERetargetSourceOrTarget::Target);
	const TArray<FName>& TargetBoneNames = TargetSkeleton.BoneNames;
	const int32 NumTargetBones = TargetBoneNames.Num();

	// source skeleton data
	const FRetargetSkeleton& SourceSkeleton = Processor->GetSkeleton(ERetargetSourceOrTarget::Source);
	const TArray<FName>& SourceBoneNames = SourceSkeleton.BoneNames;
	const int32 NumSourceBones = SourceBoneNames.Num();

	TArray<FTransform> SourceComponentPose;
	SourceComponentPose.SetNum(NumSourceBones);

	// reset the planting state
	Processor->ResetPlanting();


	UE_LOG(LogTemp, Error, TEXT("NumFrames: %d"), NumFrames);

	auto GetWorldTransform = [&Tracks, SrcSkeletalMesh](const FName& BoneName, const int32 FrameIndex) -> FTransform
		{
			FTransform CurrentTransform = FTransform::Identity;
			int32 CurrentBoneIndex = SrcSkeletalMesh->GetRefSkeleton().FindBoneIndex(BoneName);
			while (CurrentBoneIndex > INDEX_NONE)
			{
				const FName CurrentBoneName = SrcSkeletalMesh->GetRefSkeleton().GetBoneName(CurrentBoneIndex);
				const FRawAnimSequenceTrack& SourceTrack = Tracks[CurrentBoneName.ToString()];
				FTransform BoneTransform = FTransform::Identity;
				BoneTransform.SetLocation(FVector(SourceTrack.PosKeys[FrameIndex]));
				BoneTransform.SetRotation(FQuat(SourceTrack.RotKeys[FrameIndex]));
				//BoneTransform.SetScale3D(FVector(SourceTrack.ScaleKeys[FrameIndex]));
				CurrentTransform *= BoneTransform;
				CurrentBoneIndex = SrcSkeletalMesh->GetRefSkeleton().GetParentIndex(CurrentBoneIndex);
			}
			return CurrentTransform;
		};

	auto GetBoneWorldTransform = [SrcSkeletalMesh](const FName& BoneName) -> FTransform
		{
			FTransform CurrentTransform = FTransform::Identity;
			int32 CurrentBoneIndex = SrcSkeletalMesh->GetRefSkeleton().FindBoneIndex(BoneName);
			while (CurrentBoneIndex > INDEX_NONE)
			{
				const FName CurrentBoneName = SrcSkeletalMesh->GetRefSkeleton().GetBoneName(CurrentBoneIndex);
				const FTransform BoneTransform = SrcSkeletalMesh->GetRefSkeleton().GetRefBonePose()[CurrentBoneIndex];
				CurrentTransform *= BoneTransform;
				CurrentBoneIndex = SrcSkeletalMesh->GetRefSkeleton().GetParentIndex(CurrentBoneIndex);
			}

			CurrentTransform.SetScale3D(FVector::OneVector);

			return CurrentTransform;
		};

	for (int32 FrameIndex = 0; FrameIndex < NumFrames; FrameIndex++)
	{
		int32 BoneIndex = 0;
		for (const FName& BoneName : SourceBoneNames)
		{
			if (!Tracks.Contains(BoneName.ToString()))
			{
				SourceComponentPose[BoneIndex] = GetBoneWorldTransform(BoneName);
			}
			else
			{
				const FRawAnimSequenceTrack& SourceTrack = Tracks[BoneName.ToString()];
				SourceComponentPose[BoneIndex] = GetWorldTransform(BoneName, FrameIndex);
			}
			BoneIndex++;
		}

		// update goals
		Processor->CopyIKRigSettingsFromAsset();

		FRetargetProfile SettingsProfile;
		IKRetargeter->FillProfileWithAssetSettings(SettingsProfile);

		// run the retargeter
		const TArray<FTransform>& TargetComponentPose = Processor->RunRetargeter(SourceComponentPose, {}, FrameIndex * Duration, SettingsProfile);

		// convert to a local-space pose
		TArray<FTransform> TargetLocalPose = TargetComponentPose;
		TargetSkeleton.UpdateLocalTransformsBelowBone(0, TargetLocalPose, TargetComponentPose);

		const TArray<FTransform>& DstPoses = DstSkeletalMesh->GetRefSkeleton().GetRefBonePose();

		BoneIndex = 0;
		for (const FName& BoneName : TargetBoneNames)
		{
			if (Tracks.Contains(BoneName.ToString()))
			{
				FRawAnimSequenceTrack& SourceTrack = Tracks[BoneName.ToString()];
				if (SourceTrack.PosKeys.Num() > FrameIndex)
				{
					SourceTrack.PosKeys[FrameIndex] = FVector3f(TargetLocalPose[BoneIndex].GetLocation());
					SourceTrack.RotKeys[FrameIndex] = FQuat4f(TargetLocalPose[BoneIndex].GetRotation().GetNormalized());

					const int32 DstBoneIndex = DstSkeletalMesh->GetRefSkeleton().FindBoneIndex(BoneName);

					if (BoneName == SourceBoneNames[0])
					{
						SourceTrack.PosKeys[FrameIndex] = FVector3f(0, 0, DstPoses[DstBoneIndex].GetLocation().Z);
					}

					SourceTrack.ScaleKeys[FrameIndex] = FVector3f(DstPoses[DstBoneIndex].GetScale3D());
				}
			}
			BoneIndex++;
		}

	}

	return Asset->GetParser()->LoadSkeletalAnimationFromTracksAndMorphTargets(DstSkeletalMesh, Tracks, MorphTargetCurves, Duration, SkeletalAnimationConfig);
}