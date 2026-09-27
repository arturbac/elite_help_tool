#version 450 core
// ImGui's own vertex stage: the same vertex layout and push constants, so the draw list's buffers
// and the backend's pipeline layout serve this pipeline too
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 col;
layout(push_constant) uniform constants_t { vec2 scale; vec2 translate; } pc;
layout(location = 0) out vec4 out_col;
void main()
{
  out_col = col;
  gl_Position = vec4(pos * pc.scale + pc.translate, 0.0, 1.0);
}
