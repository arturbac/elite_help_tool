#version 450 core
// the colour alone - the blending does the work, see ground_pipeline in vk_draw.cc
layout(location = 0) in vec4 col;
layout(location = 0) out vec4 out_col;
void main()
{
  out_col = col;
}
