@type compute

layout(r32f, binding = 0) uniform readonly image2D uInImage;
layout(rgba32f, binding = 1) uniform writeonly image2D uOutImage;
uniform ivec2 uTexSize;

void main()
{
    ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);
    if (iuv.x >= uTexSize.x || iuv.y >= uTexSize.y)
        return;
    float r = imageLoad(uInImage, ivec2(iuv.x * 3 + 0, iuv.y)).r;
    float g = imageLoad(uInImage, ivec2(iuv.x * 3 + 1, iuv.y)).r;
    float b = imageLoad(uInImage, ivec2(iuv.x * 3 + 2, iuv.y)).r;
    imageStore(uOutImage, iuv, vec4(r, g, b, 1.0));
}