#version 330

// Cutout shader vertex stage. fragPos is the vertex position in map-pixel space
// (raylib does not pre-transform vertices when using Camera2D).

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;

uniform mat4 mvp;

out vec2 fragTexCoord;
out vec4 fragColor;
out vec2 fragPos;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragPos = vertexPosition.xy;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
