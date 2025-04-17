#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;

const float radius = 0.49;
const float edge = 0.01;

void main() {
  vec2 uv = fragTexCoord;
  vec4 tex = texture(texture0, uv);
  float dist = length(uv - 0.5);
  float alpha = 1 - smoothstep(radius, radius + edge, dist);

  finalColor = vec4(tex.rgb, tex.a * alpha);
}
