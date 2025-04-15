#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec2 resolution;

void main() {
  vec2 uv = fragTexCoord;
  vec2 center = vec2(0.5, 0.5);
  float dist = distance(uv, center);

  if (dist > 0.5) discard;

  finalColor = texture(texture0, uv);
}
