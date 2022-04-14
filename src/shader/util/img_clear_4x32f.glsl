@type compute

layout(rgba32f, binding = 0) uniform image2D uImage;
uniform ivec2 uTexSize;

void main()
{
    ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);
    if (iuv.x >= uTexSize.x || iuv.y >= uTexSize.y)
        return;
    imageStore(uImage, iuv, vec4(0.0, 0.0, 0.0, 1.0));
}