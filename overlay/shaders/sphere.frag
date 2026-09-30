#version 450 core
// a ball lit from -u, a little towards the viewer, so most of the face is lit and the terminator runs
// along the far side. The vertex alpha says how: below one half a planet whose shine is twice the alpha,
// above it a star, which lights itself and only darkens towards its limb.
// The uv carries the face too: u shifted by 8 times (1 + the atlas tile) - 0 for a ball of the vertex
// colour. The texture bound is ImGui's, set 0 binding 0, the atlas of faces for a ball that has one
layout(location = 0) in vec4 col;
layout(location = 1) in vec2 uv;
layout(location = 0) out vec4 out_col;
layout(set = 0, binding = 0) uniform sampler2D faces;

const vec3 light = normalize(vec3(-0.8, -0.2, 0.6));
// the night side keeps some of its colour - the colour says what the body is
const float ambient = 0.28;
// the light wraps a little past the terminator, a soft edge rather than a hard one
const float wrap = 0.2;
// the atlas is 8 x 8 faces of 128 pixels
const float across = 8.0;
const float side = 128.0;

void main()
{
  float tile = floor((uv.x + 4.0) / 8.0);
  vec2 ball = vec2(uv.x - 8.0 * tile, uv.y);
  float r = length(ball);
  float cover = clamp((1.0 - r) / max(fwidth(r), 1e-4) + 0.5, 0.0, 1.0);
  // how far one screen pixel reaches across the face, taken before any pixel leaves
  vec2 step_x = dFdx(ball) * 0.25 / across;
  vec2 step_y = dFdy(ball) * 0.25 / across;
  if(cover <= 0.0)
    discard;
  vec3 n = vec3(ball.x, ball.y, sqrt(max(0.0, 1.0 - r * r)));

  // the face's own square, kept half a pixel inside so its neighbour never bleeds in; four samples over
  // the screen pixel, since a face of 128 is drawn smaller and without mip levels would sparkle
  vec3 base = col.rgb;
  if(tile > 0.5)
    {
    float index = tile - 1.0;
    vec2 cell = vec2(mod(index, across), floor(index / across));
    vec2 low = (cell + vec2(0.5 / side)) / across;
    vec2 high = (cell + vec2(1.0 - 0.5 / side)) / across;
    vec2 at = (cell + ball * 0.5 + 0.5) / across;
    base = (textureLod(faces, clamp(at - step_x - step_y, low, high), 0.0).rgb
            + textureLod(faces, clamp(at + step_x - step_y, low, high), 0.0).rgb
            + textureLod(faces, clamp(at - step_x + step_y, low, high), 0.0).rgb
            + textureLod(faces, clamp(at + step_x + step_y, low, high), 0.0).rgb) * 0.25;
    }

  vec3 colour;
  if(col.a > 0.5)
    colour = base * (0.55 + 0.45 * n.z);
  else
    {
    float gloss = clamp(col.a * 2.0, 0.0, 1.0);
    float diffuse = clamp((dot(n, light) + wrap) / (1.0 + wrap), 0.0, 1.0);
    vec3 half_way = normalize(light + vec3(0.0, 0.0, 1.0));
    // a small soft highlight, never a glare: at full gloss a third of white at the most
    float shine = gloss * 0.35 * pow(max(dot(n, half_way), 0.0), mix(6.0, 48.0, gloss));
    colour = base * (ambient + (1.0 - ambient) * diffuse) + vec3(shine);
    }
  out_col = vec4(min(colour, vec3(1.0)), cover);
}
