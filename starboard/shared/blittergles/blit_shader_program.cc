// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/shared/blittergles/blit_shader_program.h"

#include <GLES2/gl2.h>

#include <memory>

#include "starboard/blitter.h"
#include "starboard/shared/blittergles/blitter_context.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/blittergles/shader_program.h"
#include "starboard/shared/gles/gl_call.h"

namespace starboard {
namespace shared {
namespace blittergles {

namespace {

// Location of the blit shader attribute "a_blit_position."
static const int kBlitPositionAttribute = 0;

// Location of the blit shader attribute "a_tex_coord."
static const int kTexCoordAttribute = 1;

}  // namespace

BlitShaderProgram::BlitShaderProgram() {
  const char* vertex_shader_source =
      "attribute vec2 a_blit_position;"
      "attribute vec2 a_tex_coord;"
      "varying vec2 v_tex_coord;"
      "void main() {"
      "  gl_Position = vec4(a_blit_position.x, a_blit_position.y, 0, 1);"
      "  v_tex_coord = a_tex_coord;"
      "}";
  const char* fragment_shader_source =
      "precision mediump float;"
      "uniform sampler2D tex;"
      "uniform vec4 u_tex_coord_clamp;"
      "varying vec2 v_tex_coord;"
      "void main() {"
      "  gl_FragColor = texture2D(tex, "
      "      clamp(v_tex_coord, u_tex_coord_clamp.xy, u_tex_coord_clamp.zw));"
      "}";
  InitializeShaders(vertex_shader_source, fragment_shader_source);
  GL_CALL(glBindAttribLocation(GetProgramHandle(), kBlitPositionAttribute,
                               "a_blit_position"));
  GL_CALL(glBindAttribLocation(GetProgramHandle(), kTexCoordAttribute,
                               "a_tex_coord"));

  int link_status;
  GL_CALL(glLinkProgram(GetProgramHandle()));
  GL_CALL(glGetProgramiv(GetProgramHandle(), GL_LINK_STATUS, &link_status));
  SB_CHECK(link_status);

  clamp_uniform_ =
      glGetUniformLocation(GetProgramHandle(), "u_tex_coord_clamp");
  SB_CHECK(clamp_uniform_ != -1);
}

bool BlitShaderProgram::Draw(SbBlitterContext context,
                             SbBlitterSurface surface,
                             SbBlitterRect src_rect,
                             SbBlitterRect dst_rect) const {
  GL_CALL(glUseProgram(GetProgramHandle()));

  float dst_vertices[8], src_vertices[8];
  SetTexCoords(src_rect, surface->info.width, surface->info.height,
               src_vertices);
  SetNDC(dst_rect, context->current_render_target->width,
         context->current_render_target->height, dst_vertices);

  // Clamp so fragment shader does not sample beyond edges of texture.
  const float kTexelInset = 0.499f;
  float texel_clamps[] = {
      src_vertices[0] + kTexelInset / src_rect.width,    // min u
      src_vertices[1] + kTexelInset / src_rect.height,   // min v
      src_vertices[4] - kTexelInset / src_rect.width,    // max u
      src_vertices[3] - kTexelInset / src_rect.height};  // max v
  GL_CALL(glVertexAttribPointer(kBlitPositionAttribute, 2, GL_FLOAT, GL_FALSE,
                                0, dst_vertices));
  GL_CALL(glVertexAttribPointer(kTexCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0,
                                src_vertices));
  GL_CALL(glEnableVertexAttribArray(kBlitPositionAttribute));
  GL_CALL(glEnableVertexAttribArray(kTexCoordAttribute));
  GL_CALL(glUniform4f(clamp_uniform_, texel_clamps[0], texel_clamps[1],
                      texel_clamps[2], texel_clamps[3]));

  GL_CALL(glActiveTexture(GL_TEXTURE0));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, surface->color_texture_handle));

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  bool success = glGetError() == GL_NO_ERROR;

  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
  GL_CALL(glDisableVertexAttribArray(kTexCoordAttribute));
  GL_CALL(glDisableVertexAttribArray(kBlitPositionAttribute));
  GL_CALL(glUseProgram(0));
  return success;
}

}  // namespace blittergles
}  // namespace shared
}  // namespace starboard