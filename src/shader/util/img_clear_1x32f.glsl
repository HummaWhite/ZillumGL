@type compute

layout(r32f, binding = 0) uniform image2D uImage;
uniform ivec2 uTexSize;

void main()
{
    ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);
    if (iuv.x >= uTexSize.x || iuv.y >= uTexSize.y)
        return;
    imageStore(uImage, ivec2(iuv.x * 3 + 0, iuv.y), vec4(0.0, 0.0, 0.0, 1.0));
    imageStore(uImage, ivec2(iuv.x * 3 + 1, iuv.y), vec4(0.0, 0.0, 0.0, 1.0));
    imageStore(uImage, ivec2(iuv.x * 3 + 2, iuv.y), vec4(0.0, 0.0, 0.0, 1.0));
}