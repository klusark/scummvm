/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "engines/grim/remastered/look/look_consts.h"

namespace Grim {

static const LookConstMember litMeshVertexMembers[] = {
	{ "ViewDistance", kLookConstVec4, 0, 1 },
	{ "Model", kLookConstMat4, 16, 1 },
	{ "View", kLookConstMat4, 80, 1 }
};

static const LookConstMember shadowMeshMembers[] = {
	{ "LocalToScreen", kLookConstMat4, 0, 1 },
	{ "LocalToWorld", kLookConstMat4, 64, 1 }
};

static const LookConstMember projectedVertexMembers[] = {
	{ "ViewDistance", kLookConstVec4, 0, 1 },
	{ "LocalToCamera", kLookConstMat4, 16, 1 }
};

static const LookConstMember smoothResolveMembers[] = {
	{ "TexelSize", kLookConstVec2, 0, 1 }
};

static const LookConstMember charLightFragmentMembers[] = {
	{ "WorldToLocal", kLookConstMat4, 0, 1 },
	{ "Light_InnerColor_InnerRadius", kLookConstVec4, 64, 1 },
	{ "Light_OuterColor_FalloffPower", kLookConstVec4, 80, 1 },
	{ "Light_Position_InvDelta", kLookConstVec4, 96, 1 },
	{ "Light_VerticalInnerRadius_VerticalFalloffPower_VerticalInvDelta_User", kLookConstVec4, 112, 1 },
	{ "Light_ShadingBias_ShadingInvDelta", kLookConstVec4, 128, 1 },
	{ "WorldToShadowMap", kLookConstMat4, 144, 1 },
	{ "Shadow_Bias_Softness_Quality", kLookConstVec4, 208, 1 },
	{ "CameraToWorld", kLookConstMat4, 224, 1 },
	{ "ViewDistance", kLookConstVec4, 288, 1 }
};

static const LookConstMember charAoFragmentMembers[] = {
	{ "ViewDistance", kLookConstVec4, 0, 1 },
	{ "WorldToLocal", kLookConstMat4, 16, 1 },
	{ "CameraToWorld", kLookConstMat4, 80, 1 }
};

static const LookConstMember charResolveMembers[] = {
	{ "EnvShadow_MinLuminance_InvDeltaLuminance_Power", kLookConstVec4, 0, 1 },
	{ "Shadow_Black_AoStrength", kLookConstVec4, 16, 1 },
	{ "Shadow_White_LightType", kLookConstVec4, 32, 1 },
	{ "Iris_Center_Size", kLookConstVec4, 48, 1 }
};

static const LookConstMember charForwardResolveMembers[] = {
	{ "EnvShadow_MinLuminance_InvDeltaLuminance_Power", kLookConstVec4, 0, 1 },
	{ "Shadow_Black_AoStrength", kLookConstVec4, 16, 1 },
	{ "Shadow_White_LightType", kLookConstVec4, 32, 1 },
	{ "Iris_Center_Size", kLookConstVec4, 48, 1 },
	{ "CameraToWorld", kLookConstMat4, 64, 1 },
	{ "AoWorldSpacePosition_ViewDistance", kLookConstVec4, 128, 1 },
	{ "WorldToLocal", kLookConstMat4, 144, 1 },
	{ "Light_InnerColor_InnerRadius", kLookConstVec4, 208, 1 },
	{ "Light_OuterColor_FalloffPower", kLookConstVec4, 224, 1 },
	{ "Light_Position_InvDelta", kLookConstVec4, 240, 1 },
	{ "Light_VerticalInnerRadius_VerticalFalloffPower_VerticalInvDelta_User", kLookConstVec4, 256, 1 },
	{ "Light_ShadingBias_ShadingInvDelta", kLookConstVec4, 272, 1 },
	{ "WorldToShadowMap", kLookConstMat4, 288, 1 },
	{ "Shadow_Bias_Softness_Quality_Enabled", kLookConstVec4, 352, 1 }
};

static const LookConstMember deferredResolveMembers[] = {
	{ "ViewDistance", kLookConstVec4, 0, 1 }
};

static const LookConstMember deferredDirectMembers[] = {
	{ "LightSettings", kLookConstVec4, 0, 1 },
	{ "LightColor", kLookConstVec4, 16, 1 },
	{ "LightDirection", kLookConstVec4, 32, 1 }
};

static const LookConstMember deferredOmniMembers[] = {
	{ "LightSettings", kLookConstVec4, 0, 1 },
	{ "LightColor", kLookConstVec4, 16, 1 },
	{ "LightPosition", kLookConstVec4, 32, 1 }
};

static const LookConstMember deferredSpotMembers[] = {
	{ "LightSettings", kLookConstVec4, 0, 1 },
	{ "LightColor", kLookConstVec4, 16, 1 },
	{ "LightPosition", kLookConstVec4, 32, 1 },
	{ "LightDirection", kLookConstVec4, 48, 1 }
};

static const LookConstMember postProcessMembers[] = {
	{ "FXAAPosOffset", kLookConstVec4, 0, 1 },
	{ "FXAARcpFrameOpt1", kLookConstVec4, 16, 1 },
	{ "FXAARcpFrameOpt2", kLookConstVec4, 32, 1 },
	{ "FXAAEdgeSharpness", kLookConstFloat, 48, 1 },
	{ "FXAAEdgeThreshold", kLookConstFloat, 52, 1 },
	{ "FXAAEdgeThresholdMin", kLookConstFloat, 56, 1 }
};

static const LookConstMember litMeshFragmentMembers[] = {
	{ "WorldToLocal", kLookConstMat4, 0, 1 },
	{ "Light_InnerColor_InnerRadius", kLookConstVec4, 64, 1 },
	{ "Light_OuterColor_FalloffPower", kLookConstVec4, 80, 1 },
	{ "Light_Position_InvDelta", kLookConstVec4, 96, 1 },
	{ "Light_VerticalInnerRadius_VerticalFalloffPower_VerticalInvDelta_User", kLookConstVec4, 112, 1 },
	{ "Light_ShadingBias_ShadingInvDelta", kLookConstVec4, 128, 1 },
	{ "WorldToShadowMap", kLookConstMat4, 144, 1 },
	{ "Shadow_Bias_Softness_Quality_Enabled", kLookConstVec4, 208, 1 }
};

static const LookConstMember forwardMeshVertexMembers[] = {
	{ "ViewDistance", kLookConstVec4, 0, 1 },
	{ "Model", kLookConstMat4, 16, 1 },
	{ "View", kLookConstMat4, 80, 1 },
	{ "ProbeCenter", kLookConstVec4, 144, 1 },
	{ "ProbeSize", kLookConstVec4, 160, 1 },
	{ "IrradianceXpos", kLookConstVec4Array, 176, 7 },
	{ "IrradianceXneg", kLookConstVec4Array, 288, 7 },
	{ "IrradianceYpos", kLookConstVec4Array, 400, 7 },
	{ "IrradianceYneg", kLookConstVec4Array, 512, 7 },
	{ "IrradianceZpos", kLookConstVec4Array, 624, 7 },
	{ "IrradianceZneg", kLookConstVec4Array, 736, 7 }
};

static const LookConstMember quadMembers[] = {
	{ "MultiplyColor", kLookConstVec4, 0, 1 },
	{ "TexelSizeDesaturateGamma", kLookConstVec4, 16, 1 },
	{ "ColorKey", kLookConstVec4, 32, 1 }
};

static const LookConstMember colorQuadMembers[] = {
	{ "FillColor", kLookConstVec4, 0, 1 }
};

static const LookConstMember textMembers[] = {
	{ "TextColor", kLookConstVec4, 0, 1 },
	{ "ShadowColor", kLookConstVec4, 16, 1 },
	{ "Origin", kLookConstVec2, 32, 1 },
	{ "PixelSize", kLookConstVec2, 40, 1 },
	{ "TexelSize", kLookConstVec2, 48, 1 }
};

#define LOOK_LAYOUT(blockName, size, members) { blockName, size, sizeof(members) / sizeof(members[0]), members }

static const LookConstLayout layouts[RemasteredLookConsts::kNumLayouts] = {
	LOOK_LAYOUT("LitMeshVertexConstants", 144, litMeshVertexMembers),
	LOOK_LAYOUT("ShadowMeshConstants", 128, shadowMeshMembers),
	LOOK_LAYOUT("ZShapeVertexConstants", 80, projectedVertexMembers),
	LOOK_LAYOUT("SmoothLightingResolveConstants", 8, smoothResolveMembers),
	LOOK_LAYOUT("CharacterLightVertexConstants", 80, projectedVertexMembers),
	LOOK_LAYOUT("CharacterLightFragmentConstants", 304, charLightFragmentMembers),
	LOOK_LAYOUT("CharacterAOVertexConstants", 80, projectedVertexMembers),
	LOOK_LAYOUT("CharacterAOFragmentConstants", 144, charAoFragmentMembers),
	LOOK_LAYOUT("CharacterLightResolveConstants", 64, charResolveMembers),
	LOOK_LAYOUT("CharacterForwardLightResolveConstants", 368, charForwardResolveMembers),
	LOOK_LAYOUT("DeferredResolveConstants", 16, deferredResolveMembers),
	LOOK_LAYOUT("DeferredDirectConstants", 48, deferredDirectMembers),
	LOOK_LAYOUT("DeferredOmniConstants", 48, deferredOmniMembers),
	LOOK_LAYOUT("DeferredSpotConstants", 64, deferredSpotMembers),
	LOOK_LAYOUT("DeferredPostProcessConstants", 60, postProcessMembers),
	LOOK_LAYOUT("LitMeshFragmentConstants", 224, litMeshFragmentMembers),
	LOOK_LAYOUT("ForwardLitMeshVertexConstants", 848, forwardMeshVertexMembers),
	LOOK_LAYOUT("TexturedQuadConstants", 48, quadMembers),
	LOOK_LAYOUT("ColorQuadConstants", 16, colorQuadMembers),
	LOOK_LAYOUT("TextConstants", 56, textMembers)
};

#undef LOOK_LAYOUT

const LookConstLayout *RemasteredLookConsts::getLayout(int layout) {
	if (layout < 0 || layout >= kNumLayouts)
		return nullptr;

	return &layouts[layout];
}

const LookConstLayout *RemasteredLookConsts::getVertexLayout(int program) {
	switch (program) {
	case kLookProgramTexturedLit:
	case kLookProgramUntexturedLit:
	case kLookProgramSmoothLighting:
		return &layouts[kLayoutLitMeshVertex];
	case kLookProgramShadowMesh:
		return &layouts[kLayoutShadowMesh];
	case kLookProgramDeferredZShape:
		return &layouts[kLayoutZShapeVertex];
	case kLookProgramTexturedQuad565Linear:
		return &layouts[kLayoutQuad];
	case kLookProgramCharLightGradient:
	case kLookProgramCharLightPoint:
	case kLookProgramCharLightSpot:
	case kLookProgramCharLightSpotShadow:
	case kLookProgramCharLightSpotShadowEnv:
	case kLookProgramCharLightDirectional:
	case kLookProgramCharLightDirectionalShadow:
	case kLookProgramCharLightDirectionalShadowEnv:
		return &layouts[kLayoutCharLightVertex];
	case kLookProgramCharAo:
		return &layouts[kLayoutCharAoVertex];
	case kLookProgramText:
		return &layouts[kLayoutText];
	case kLookProgramForwardTextured:
	case kLookProgramForwardTexturedSpot:
	case kLookProgramForwardTexturedDirectional:
	case kLookProgramForwardUntextured:
	case kLookProgramForwardUntexturedSpot:
	case kLookProgramForwardUntexturedDirectional:
	case kLookProgramForwardTexturedLow:
	case kLookProgramForwardTexturedSpotLow:
	case kLookProgramForwardTexturedDirectionalLow:
	case kLookProgramForwardUntexturedLow:
	case kLookProgramForwardUntexturedSpotLow:
	case kLookProgramForwardUntexturedDirectionalLow:
		return &layouts[kLayoutForwardMeshVertex];
	default:
		return nullptr;
	}
}

const LookConstLayout *RemasteredLookConsts::getPixelLayout(int program) {
	switch (program) {
	case kLookProgramTexturedLit:
	case kLookProgramUntexturedLit:
	case kLookProgramSmoothLighting:
		return &layouts[kLayoutLitMeshVertex];
	case kLookProgramSmoothLightingResolve:
		return &layouts[kLayoutSmoothLightingResolve];
	case kLookProgramZQuad:
	case kLookProgramZQuad565:
	case kLookProgramTexturedQuad:
	case kLookProgramTexturedQuad565Linear:
	case kLookProgramTexturedGammaQuad:
		return &layouts[kLayoutQuad];
	case kLookProgramCharLightGradient:
	case kLookProgramCharLightPoint:
	case kLookProgramCharLightSpot:
	case kLookProgramCharLightSpotShadow:
	case kLookProgramCharLightSpotShadowEnv:
	case kLookProgramCharLightDirectional:
	case kLookProgramCharLightDirectionalShadow:
	case kLookProgramCharLightDirectionalShadowEnv:
	case kLookProgramCharLightViz:
		return &layouts[kLayoutCharLightFragment];
	case kLookProgramCharAo:
		return &layouts[kLayoutCharAoFragment];
	case kLookProgramCharResolve:
		return &layouts[kLayoutCharResolve];
	case kLookProgramCharResolveShadows:
		return &layouts[kLayoutCharForwardResolve];
	case kLookProgramDeferredResolve:
		return &layouts[kLayoutDeferredResolve];
	case kLookProgramDeferredLightDirect:
		return &layouts[kLayoutDeferredDirect];
	case kLookProgramDeferredLightOmni:
		return &layouts[kLayoutDeferredOmni];
	case kLookProgramDeferredLightSpot:
		return &layouts[kLayoutDeferredSpot];
	case kLookProgramPostProcess:
		return &layouts[kLayoutPostProcess];
	case kLookProgramText:
		return &layouts[kLayoutText];
	case kLookProgramColorQuad:
		return &layouts[kLayoutColorQuad];
	case kLookProgramForwardTextured:
	case kLookProgramForwardTexturedSpot:
	case kLookProgramForwardTexturedDirectional:
	case kLookProgramForwardUntextured:
	case kLookProgramForwardUntexturedSpot:
	case kLookProgramForwardUntexturedDirectional:
	case kLookProgramForwardTexturedLow:
	case kLookProgramForwardTexturedSpotLow:
	case kLookProgramForwardTexturedDirectionalLow:
	case kLookProgramForwardUntexturedLow:
	case kLookProgramForwardUntexturedSpotLow:
	case kLookProgramForwardUntexturedDirectionalLow:
		return &layouts[kLayoutLitMeshFragment];
	default:
		return nullptr;
	}
}

} // End of namespace Grim
