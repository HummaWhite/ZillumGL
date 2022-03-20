@type compute

layout(rgba32f, binding = 0) uniform image2D uImage;
uniform ivec2 uTexSize;

void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	if (coord.x >= uTexSize.x || coord.y >= uTexSize.y)
		return;
	imageStore(uImage, coord, vec4(0.0, 0.0, 0.0, 1.0));
}