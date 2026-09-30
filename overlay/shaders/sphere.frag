#version 450 core
// a ball lit from -u, a little towards the viewer, so most of the face is lit and the terminator runs
// along the far side. The vertex alpha says how: below one half a planet whose shine is twice the alpha,
// above it a star, which lights itself and only darkens towards its limb
layout(location = 0) in vec4 col;
layout(location = 1) in vec2 uv;
layout(location = 0) out vec4 out_col;

const vec3 light = normalize(vec3(-0.8, -0.2, 0.6));
// the night side keeps some of its colour - the colour says what the body is
const float ambient = 0.28;
// the light wraps a little past the terminator, a soft edge rather than a hard one
const float wrap = 0.2;

void main()
{
  float r = length(uv);
  float cover = clamp((1.0 - r) / max(fwidth(r), 1e-4) + 0.5, 0.0, 1.0);
  if(cover <= 0.0)
    discard;
  vec3 n = vec3(uv.x, uv.y, sqrt(max(0.0, 1.0 - r * r)));

  vec3 colour;
  if(col.a > 0.5)
    colour = col.rgb * (0.55 + 0.45 * n.z);
  else
    {
    float gloss = clamp(col.a * 2.0, 0.0, 1.0);
    float diffuse = clamp((dot(n, light) + wrap) / (1.0 + wrap), 0.0, 1.0);
    vec3 half_way = normalize(light + vec3(0.0, 0.0, 1.0));
    // a small soft highlight, never a glare: at full gloss a third of white at the most
    float shine = gloss * 0.35 * pow(max(dot(n, half_way), 0.0), mix(6.0, 48.0, gloss));
    colour = col.rgb * (ambient + (1.0 - ambient) * diffuse) + vec3(shine);
    }
  out_col = vec4(min(colour, vec3(1.0)), cover);
}
