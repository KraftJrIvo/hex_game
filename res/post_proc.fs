#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform int nDrops;
uniform vec2 dropCenters[128];
uniform float dropTimes[128];
uniform float time;       
uniform vec2 screenSize;

out vec4 finalColor;

void main() {
    if (nDrops > 0) {
        vec2 uv = fragTexCoord;

        for (int i = 0; i < nDrops; ++i) {
            // Convert drop center from pixels to UV
            vec2 dropUV = vec2(dropCenters[i].x, screenSize.y - dropCenters[i].y) / screenSize;

            // Time since drop started
            float t = time - dropTimes[i];
            if (t < 0.0) {
                // Drop hasn't happened yet
                finalColor = texture(texture0, uv) * fragColor * colDiffuse;
                return;
            }

            // Vector from center to pixel
            vec2 delta = (uv - dropUV) * vec2(1., screenSize.y / screenSize.x);
            float dist = length(delta);

            // Wave parameters
            float speed = 10.;
            float frequency = 30.0;
            float timeFade = 0.2;
            float amplitude = 0.1 * (1.0 - clamp((t - dist)/timeFade, 0., 1.));
            float fade = 7.0;

            float wave = sin(dist * frequency - t * speed) * amplitude;

            // Apply distortion where the wave has reached
            float mask = smoothstep(0.0, 1.0, t * speed - dist);
            uv += normalize(delta) * wave * mask * exp(-dist * fade);
        }

        vec4 texelColor = texture(texture0, uv);
        finalColor = texelColor * fragColor * colDiffuse;
    } else {
        vec4 texelColor = texture(texture0, fragTexCoord);
        finalColor = texelColor * fragColor * colDiffuse;
    }
}
