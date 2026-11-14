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

#include "engines/grim/remastered/look/look_programs.h"

namespace Grim {

struct LookProgramInfo {
	const char *pixelName;
	const char *vertexName;
	int vertexFormat;
	int outputKind;
};

static const LookProgramInfo programTable[kLookNumPrograms] = {
	{ "textured_lit_p", "textured_lit_v", kLookVertexTextured, RemasteredLookPrograms::kOutputThree },
	{ "untextured_lit_p", "untextured_lit_v", kLookVertexTextured, RemasteredLookPrograms::kOutputThree },
	{ "smooth_lighting_p", "smooth_lighting_v", kLookVertexTextured, RemasteredLookPrograms::kOutputOne },
	{ "copy_p", "copy_v", kLookVertexQuad, RemasteredLookPrograms::kOutputOne },
	{ "smooth_lighting_resolve_p", "smooth_lighting_resolve_v", kLookVertexQuad, RemasteredLookPrograms::kOutputOne },
	{ "shadow_mesh_p", "shadow_mesh_v", kLookVertexTextured, RemasteredLookPrograms::kOutputOne },
	{ "shadow_p", "shadow_v", kLookVertexShadow, RemasteredLookPrograms::kOutputOne },
	{ "z_quad_p", "z_quad_v", kLookVertexQuad, RemasteredLookPrograms::kOutputFloat },
	{ "z_quad_565_p", "z_quad_565_v", kLookVertexQuad, RemasteredLookPrograms::kOutputFloat },
	{ "deferred_zshape_p", "deferred_zshape_v", kLookVertexVector3, RemasteredLookPrograms::kOutputOne },
	{ "textured_quad_p", "textured_quad_v", kLookVertexQuad, RemasteredLookPrograms::kOutputOne },
	{ "textured_quad_565_linear_p", "textured_quad_565_linear_v", kLookVertexQuad, RemasteredLookPrograms::kOutputOne },
	{ "deferred_char_light_gradient_p", "deferred_char_light_gradient_v", kLookVertexVector3, RemasteredLookPrograms::kOutputOne },
	{ "deferred_char_light_point_p", "deferred_char_light_point_v", kLookVertexVector3, RemasteredLookPrograms::kOutputOne },
	{ "deferred_char_light_spot_p", "deferred_char_light_spot_v", kLookVertexVector3, RemasteredLookPrograms::kOutputOne },
	{ "deferred_char_light_spot_shadow_p", "deferred_char_light_spot_shadow_v", kLookVertexVector3, RemasteredLookPrograms::kOutputOne },
	{ "deferred_char_light_spot_shadow_env_p", "deferred_char_light_spot_shadow_env_v", kLookVertexVector3, RemasteredLookPrograms::kOutputOne },
	{ "deferred_char_light_directional_p", "deferred_char_light_directional_v", kLookVertexVector3, RemasteredLookPrograms::kOutputOne },
	{ "deferred_char_light_directional_shadow_p", "deferred_char_light_directional_shadow_v", kLookVertexVector3, RemasteredLookPrograms::kOutputOne },
	{ "deferred_char_light_directional_shadow_env_p", "deferred_char_light_directional_shadow_env_v", kLookVertexVector3, RemasteredLookPrograms::kOutputOne },
	{ "deferred_char_light_viz_p", "deferred_char_light_viz_v", kLookVertexVector3, RemasteredLookPrograms::kOutputOne },
	{ "deferred_char_ao_p", "deferred_char_ao_v", kLookVertexVector3, RemasteredLookPrograms::kOutputOne },
	{ "deferred_char_resolve_p", "deferred_char_resolve_v", kLookVertexQuad, RemasteredLookPrograms::kOutputOne },
	{ "deferred_char_resolve_shadows_p", "deferred_char_resolve_v", kLookVertexQuad, RemasteredLookPrograms::kOutputOne },
	{ "deferred_resolve_p", "deferred_resolve_v", kLookVertexQuad, RemasteredLookPrograms::kOutputOne },
	{ "deferred_light_direct_p", "deferred_light_direct_v", kLookVertexQuad, RemasteredLookPrograms::kOutputOne },
	{ "deferred_light_omni_p", "deferred_light_omni_v", kLookVertexQuad, RemasteredLookPrograms::kOutputOne },
	{ "deferred_light_spot_p", "deferred_light_spot_v", kLookVertexQuad, RemasteredLookPrograms::kOutputOne },
	{ "deferred_post_process_p", "deferred_post_process_v", kLookVertexQuad, RemasteredLookPrograms::kOutputOne },
	{ "text_p", "text_v", kLookVertexText, RemasteredLookPrograms::kOutputOne },
	{ "color_quad_p", "color_quad_v", kLookVertexQuad, RemasteredLookPrograms::kOutputOne },
	{ "textured_gamma_quad_p", "textured_gamma_quad_v", kLookVertexQuad, RemasteredLookPrograms::kOutputOne },
	{ "forward_textured_p", "forward_vertex_common_v", kLookVertexTextured, RemasteredLookPrograms::kOutputOne },
	{ "forward_textured_spot_p", "forward_vertex_common_v", kLookVertexTextured, RemasteredLookPrograms::kOutputOne },
	{ "forward_textured_directional_p", "forward_vertex_common_v", kLookVertexTextured, RemasteredLookPrograms::kOutputOne },
	{ "forward_untextured_p", "forward_vertex_common_v", kLookVertexTextured, RemasteredLookPrograms::kOutputOne },
	{ "forward_untextured_spot_p", "forward_vertex_common_v", kLookVertexTextured, RemasteredLookPrograms::kOutputOne },
	{ "forward_untextured_directional_p", "forward_vertex_common_v", kLookVertexTextured, RemasteredLookPrograms::kOutputOne },
	{ "forward_textured_p", "forward_vertex_low_common_v", kLookVertexTextured, RemasteredLookPrograms::kOutputOne },
	{ "forward_textured_spot_p", "forward_vertex_low_common_v", kLookVertexTextured, RemasteredLookPrograms::kOutputOne },
	{ "forward_textured_directional_p", "forward_vertex_low_common_v", kLookVertexTextured, RemasteredLookPrograms::kOutputOne },
	{ "forward_untextured_p", "forward_vertex_low_common_v", kLookVertexTextured, RemasteredLookPrograms::kOutputOne },
	{ "forward_untextured_spot_p", "forward_vertex_low_common_v", kLookVertexTextured, RemasteredLookPrograms::kOutputOne },
	{ "forward_untextured_directional_p", "forward_vertex_low_common_v", kLookVertexTextured, RemasteredLookPrograms::kOutputOne }
};

bool RemasteredLookPrograms::isKnown(int program) {
	return program >= 0 && program < kLookNumPrograms;
}

const char *RemasteredLookPrograms::getPixelName(int program) {
	if (!isKnown(program))
		return nullptr;

	return programTable[program].pixelName;
}

const char *RemasteredLookPrograms::getVertexName(int program) {
	if (!isKnown(program))
		return nullptr;

	return programTable[program].vertexName;
}

int RemasteredLookPrograms::getVertexFormat(int program) {
	if (!isKnown(program))
		return kLookVertexVector3;

	return programTable[program].vertexFormat;
}

int RemasteredLookPrograms::getOutputKind(int program) {
	if (!isKnown(program))
		return kOutputOne;

	return programTable[program].outputKind;
}

} // End of namespace Grim
