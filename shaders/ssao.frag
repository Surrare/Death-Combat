#version 330 core
// Death Combat — Screen-Space Ambient Occlusion (half resolution, depth only).
// Reconstructs view-space position and normal from the resolved depth buffer, tests 16 hemisphere samples
// against the depth buffer, and writes an occlusion factor (1 = open, 0 = fully occluded) to the red channel.
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uDepth;
uniform mat4 uProj;
uniform vec2 uTanHalf;          // (aspect * tan(fov/2), tan(fov/2))
uniform float uNear, uFar, uRadius, uStrength;

float linDepth(float d) { float z = d * 2.0 - 1.0; return 2.0 * uNear * uFar / (uFar + uNear - z * (uFar - uNear)); }
// Integer texel fetches: sampling a NEAREST depth texture at exact texel boundaries (which is where half-resolution
// pixel centres fall) picks rows inconsistently and shows up as horizontal bands.
vec3 viewPos(ivec2 ip) {
    ivec2 sz = textureSize(uDepth, 0);
    ip = clamp(ip, ivec2(0), sz - 1);
    float z = linDepth(texelFetch(uDepth, ip, 0).r);
    vec2 uv = (vec2(ip) + 0.5) / vec2(sz);
    return vec3((uv * 2.0 - 1.0) * uTanHalf * z, -z);
}
// interleaved gradient noise: well distributed per pixel, no row or column correlation
float ign(vec2 p) { return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))); }

void main() {
    ivec2 sz = textureSize(uDepth, 0);
    ivec2 ip = clamp(ivec2(gl_FragCoord.xy) * 2, ivec2(0), sz - 1);   // the full-resolution pixel under this half-res pixel
    float d0 = texelFetch(uDepth, ip, 0).r;
    if (d0 >= 0.9999) { FragColor = vec4(1.0); return; }      // sky / background
    vec3 p = viewPos(ip);
    // normal from central differences; one-sided differences only where the second difference shows a depth edge
    vec3 pr = viewPos(ip + ivec2(1, 0)), pl = viewPos(ip - ivec2(1, 0));
    vec3 pu = viewPos(ip + ivec2(0, 1)), pd = viewPos(ip - ivec2(0, 1));
    float ex = abs(pr.z + pl.z - 2.0 * p.z), ey = abs(pu.z + pd.z - 2.0 * p.z);
    float tx = 0.15 * max(abs(pr.z - p.z), abs(p.z - pl.z)) + 0.002;
    float ty = 0.15 * max(abs(pu.z - p.z), abs(p.z - pd.z)) + 0.002;
    vec3 dx = ex < tx ? (pr - pl) : (abs(pr.z - p.z) < abs(p.z - pl.z) ? pr - p : p - pl);
    vec3 dy = ey < ty ? (pu - pd) : (abs(pu.z - p.z) < abs(p.z - pd.z) ? pu - p : p - pd);
    vec3 n = normalize(cross(dx, dy));
    if (dot(n, -p) < 0.0) n = -n;

    float ang = 6.2831853 * ign(gl_FragCoord.xy);
    vec3 rvec = vec3(cos(ang), sin(ang), 0.0);
    vec3 t = normalize(rvec - n * dot(rvec, n));
    vec3 b = cross(n, t);

    float occ = 0.0;
    const int N = 16;
    for (int i = 0; i < N; i++) {
        float fi = (float(i) + 0.5) / float(N);
        float phi = float(i) * 2.399963;                       // golden angle spiral over the hemisphere
        float kz = mix(0.25, 1.0, fi);                         // stay off the tangent plane so flat surfaces do not self-occlude
        float r = sqrt(1.0 - kz * kz);
        vec3 k = vec3(r * cos(phi), r * sin(phi), kz);
        k *= mix(0.25, 1.0, fi * fi);                          // more samples close to the surface point
        vec3 sp = p + (t * k.x + b * k.y + n * k.z) * uRadius;
        vec4 clip = uProj * vec4(sp, 1.0);
        vec2 suv = clip.xy / clip.w * 0.5 + 0.5;
        if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) continue;
        float sceneZ = linDepth(texelFetch(uDepth, clamp(ivec2(suv * vec2(sz)), ivec2(0), sz - 1), 0).r);   // positive distance of the visible surface
        float sampleZ = -sp.z;
        float range = smoothstep(0.0, 1.0, uRadius / max(abs(p.z + sceneZ), 1e-4));   // ignore far-away occluders
        float bias = 0.03 + 0.015 * sampleZ;                    // depth precision falls off with distance
        occ += (sceneZ < sampleZ - bias ? 1.0 : 0.0) * range;
    }
    float ao = 1.0 - occ / float(N);
    ao = mix(1.0, ao, smoothstep(0.7, 2.2, -p.z));          // very close surfaces: the sample disc covers too many pixels and turns to noise
    FragColor = vec4(pow(clamp(ao, 0.0, 1.0), 1.0 + uStrength), 0.0, 0.0, 1.0);
}
