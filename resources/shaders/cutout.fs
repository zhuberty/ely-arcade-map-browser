#version 330

// Cutout shader fragment stage: soft-edged elliptical hole around the player so the
// player can be seen through blocks drawn over them.

in vec2 fragTexCoord;
in vec4 fragColor;
in vec2 fragPos;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec2 holeCenter;      // map pixels
uniform vec2 holeRadii;       // map pixels (x, y)
uniform float holeMinAlpha;   // alpha at the hole center (0 = fully transparent)

out vec4 finalColor;

void main()
{
    vec4 c = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    float d = length((fragPos - holeCenter) / holeRadii);
    c.a *= mix(holeMinAlpha, 1.0, smoothstep(0.5, 1.0, d));
    finalColor = c;
}
