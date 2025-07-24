#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;

uniform sampler2D tiles;

uniform vec2 maskTilePos;
uniform uint maskId;

const vec3 maskColors[5] = vec3[5](vec3(1.,0.,0.), vec3(0.,1.,0.), vec3(0.,0.,1.), vec3(0.,0.,0.), vec3(0.,1.,1.));

void main()
{
    vec4 texelColor = texture(texture0, fragTexCoord);
    vec2 sz = vec2(textureSize(tiles, 0));
    vec4 maskColor = texture(tiles, (maskTilePos.xy * 16. + vec2(mod(fragTexCoord * sz, 16.).x, mod(fragTexCoord * sz, 17.).y)) / sz);
    if (maskColor.rgb == maskColors[maskId])
        finalColor = texelColor * fragColor * colDiffuse;
    else
        discard;
}