#version 450 core
// ImGui's own vertex stage, as for the ground, only the uv goes on too: the quad's uv is the ball's own
// frame, -1..1 across it, turned so that the light always comes from -u
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 col;
layout(push_constant) uniform constants_t { vec2 scale; vec2 translate; } pc;
layout(location = 0) out vec4 out_col;
layout(location = 1) out vec2 out_uv;
void main()
{
  out_col = col;
  out_uv = uv;
  gl_Position = vec4(pos * pc.scale + pc.translate, 0.0, 1.0);
}
