#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;

const float radius = 0.49;
const float thickness = 0.02;
const float edge = 0.01;
const vec3 outlineColor = vec3(0.964, 0.321, 0.627);

void main() {
  vec2 uv = fragTexCoord;
  vec4 tex = texture(texture0, uv);
  float dist = length(uv - 0.5);
  float outer = smoothstep(radius, radius + edge, dist);

  float inner = 1 - smoothstep(radius - thickness, radius - thickness - edge, dist);
  vec3 rgb = mix(tex.rgb, outlineColor, inner + outer);
  finalColor = vec4(rgb, tex.a * (1.0 - outer));      
}
