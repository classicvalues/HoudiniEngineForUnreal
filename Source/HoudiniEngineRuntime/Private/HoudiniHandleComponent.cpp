/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
*
* Produced by:
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniHandleComponent.h"
#include "HoudiniApi.h"


UHoudiniHandleComponent::UHoudiniHandleComponent(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)

{
}

UHoudiniHandleComponent::~UHoudiniHandleComponent()
{
}

bool
UHoudiniHandleComponent::Construct(
	HAPI_AssetId AssetId,
	int32 HandleIdx,
	const FString& HandleName,
	const HAPI_HandleInfo& HandleInfo,
	const TMap<HAPI_ParmId, UHoudiniAssetParameter*>& Parameters
)
{
	TArray<HAPI_HandleBindingInfo> BindingInfos;
	BindingInfos.SetNumZeroed(HandleInfo.bindingsCount);

	if ( HAPI_RESULT_SUCCESS != FHoudiniApi::GetHandleBindingInfo(
			FHoudiniEngine::Get().GetSession(),
			AssetId, HandleIdx, &BindingInfos[0], 0, HandleInfo.bindingsCount
		)
	)
	{
		return false;
	}

	HAPI_TransformEuler HapiEulerXform;
	HapiEulerXform.position[0] = HapiEulerXform.position[1] = HapiEulerXform.position[2] = 0.0f;
	HapiEulerXform.rotationEuler[0] = HapiEulerXform.rotationEuler[1] = HapiEulerXform.rotationEuler[2] = 0.0f;
	HapiEulerXform.scale[0] = HapiEulerXform.scale[1] = HapiEulerXform.scale[2] = 1.0f;

	HapiEulerXform.rstOrder = HAPI_SRT;
	HapiEulerXform.rotationOrder = HAPI_XYZ;
	
	for ( const auto& BindingInfo : BindingInfos )
	{
		FString HandleParmName;
		FHoudiniEngineUtils::GetHoudiniString(BindingInfo.handleParmNameSH, HandleParmName);

		const HAPI_AssetId AssetParmId = BindingInfo.assetParmId;

		(void)(	XformParms[EXformParameter::TX].Bind( HapiEulerXform.position[0], "tx", 0, HandleParmName, AssetParmId, Parameters )
			||	XformParms[EXformParameter::TY].Bind( HapiEulerXform.position[1], "ty", 1, HandleParmName, AssetParmId, Parameters )
			||	XformParms[EXformParameter::TZ].Bind( HapiEulerXform.position[2], "tz", 2, HandleParmName, AssetParmId, Parameters )

			||	XformParms[EXformParameter::RX].Bind( HapiEulerXform.rotationEuler[0], "rx", 0, HandleParmName, AssetParmId, Parameters )
			||	XformParms[EXformParameter::RY].Bind( HapiEulerXform.rotationEuler[1], "ry", 1, HandleParmName, AssetParmId, Parameters )
			||	XformParms[EXformParameter::RZ].Bind( HapiEulerXform.rotationEuler[2], "rz", 2, HandleParmName, AssetParmId, Parameters )

			||	XformParms[EXformParameter::SX].Bind( HapiEulerXform.scale[0], "sx", 0, HandleParmName, AssetParmId, Parameters )
			||	XformParms[EXformParameter::SY].Bind( HapiEulerXform.scale[1], "sy", 1, HandleParmName, AssetParmId, Parameters )
			||	XformParms[EXformParameter::SZ].Bind( HapiEulerXform.scale[2], "sz", 2, HandleParmName, AssetParmId, Parameters )

			||	RSTParm.Bind( HapiEulerXform.rstOrder, "trs_order", 0, HandleParmName, AssetParmId, Parameters )
			||	RotOrderParm.Bind( HapiEulerXform.rotationOrder, "xyz_order", 0, HandleParmName, AssetParmId, Parameters )
		);
	}

	FTransform UnrealXform;
	FHoudiniEngineUtils::TranslateHapiTransform(HapiEulerXform, UnrealXform);

	SetRelativeTransform(UnrealXform);
	UpdateComponentToWorld(true, ETeleportType::None);
	return true;
}

void
UHoudiniHandleComponent::UpdateTransformParameters()
{
	HAPI_Transform HapiXform;
	FHoudiniEngineUtils::TranslateUnrealTransform(GetRelativeTransform(), HapiXform);

	const HAPI_Session* Session = FHoudiniEngine::Get().GetSession();

	float HapiMatrix[16];
	FHoudiniApi::ConvertTransformQuatToMatrix(Session, &HapiXform, HapiMatrix);

	HAPI_TransformEuler HapiEulerXform;
	FHoudiniApi::ConvertMatrixToEuler(
		Session,
		HapiMatrix,
		RSTParm.Get(HAPI_SRT),
		RotOrderParm.Get(HAPI_XYZ),
		&HapiEulerXform
	);

	XformParms[EXformParameter::TX] = HapiEulerXform.position[0];
	XformParms[EXformParameter::TY] = HapiEulerXform.position[1];
	XformParms[EXformParameter::TZ] = HapiEulerXform.position[2];

	XformParms[EXformParameter::RX] = HapiEulerXform.rotationEuler[0];
	XformParms[EXformParameter::RY] = HapiEulerXform.rotationEuler[1];
	XformParms[EXformParameter::RZ] = HapiEulerXform.rotationEuler[2];

	XformParms[EXformParameter::SX] = HapiEulerXform.scale[0];
	XformParms[EXformParameter::SY] = HapiEulerXform.scale[1];
	XformParms[EXformParameter::SZ] = HapiEulerXform.scale[2];
}

void
UHoudiniHandleComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniHandleComponent* This = Cast<UHoudiniHandleComponent>(InThis);

	if ( This && !This->IsPendingKill() )
	{
		for ( size_t i = 0; i < EXformParameter::COUNT; ++i )
		{
			This->XformParms[i].AddReferencedObject(Collector, InThis);
		}

		This->RSTParm.AddReferencedObject(Collector, InThis);
		This->RotOrderParm.AddReferencedObject(Collector, InThis);
	}
}

void
UHoudiniHandleComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	for ( size_t i = 0; i < EXformParameter::COUNT; ++i )
	{
		Ar << XformParms[i];
	}

	Ar << RSTParm;
	Ar << RotOrderParm;
}

#if WITH_EDITOR

void
UHoudiniHandleComponent::PostEditUndo()
{
	Super::PostEditUndo();

	UHoudiniAssetComponent* AttachComponent = Cast<UHoudiniAssetComponent>(AttachParent);
	if(AttachComponent)
	{
		//UploadControlPoints();
		AttachComponent->StartTaskAssetCooking(true);
	}
}

#endif

