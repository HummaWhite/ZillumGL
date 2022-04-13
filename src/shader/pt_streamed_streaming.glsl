@type compute

layout(r32f, binding = 0) uniform image2D uFrame;

layout(rgba32f, binding = 2) uniform imageBuffer uPositionQueue;
layout(rgba32f, binding = 3) uniform imageBuffer uThroughputQueue;
layout(rgba32f, binding = 4) uniform imageBuffer uWoQueue;
layout(r32i, binding = 5) uniform iimageBuffer uObjIdQueue;

uniform int uQueueCapacity;

struct QueueItem
{
	vec3 pos;
	vec3 throughput;
	vec3 wo;
	int id;
};

void accumulateImage(ivec2 iuv, vec3 color)
{
	imageAtomicAdd(uFrame, ivec2(iuv.x * 3 + 0, iuv.y), color.r);
	imageAtomicAdd(uFrame, ivec2(iuv.x * 3 + 1, iuv.y), color.g);
	imageAtomicAdd(uFrame, ivec2(iuv.x * 3 + 2, iuv.y), color.b);
}

void pushQueue(vec3 pos, vec3 throughput, vec3 wo, int id)
{
	int tailPtr = imageAtomicAdd(uObjIdQueue, 1, 1);
	tailPtr = ((tailPtr % uQueueCapacity) + uQueueCapacity) % uQueueCapacity;
	imageStore(uPositionQueue, tailPtr, vec4(pos, 0.0));
	imageStore(uThroughputQueue, tailPtr, vec4(throughput, 0.0));
	imageStore(uWoQueue, tailPtr, vec4(wo, 1.0));
	imageStore(uObjIdQueue, tailPtr + 2, ivec4(id, 0.0, 0.0, 0.0));
}

bool fetchQueue(out QueueItem item)
{
	if (imageLoad(uObjIdQueue, 0).x == imageLoad(uObjIdQueue, 1).x)
		return false;

	int headPtr = imageAtomicAdd(uObjIdQueue, 0, 1);
	headPtr = ((headPtr % uQueueCapacity) + uQueueCapacity) % uQueueCapacity;

	item.pos = imageLoad(uPositionQueue, headPtr).xyz;
	item.throughput = imageLoad(uThroughputQueue, headPtr).rgb;
	item.wo = imageLoad(uWoQueue, headPtr).xyz;
	item.id = imageLoad(uObjIdQueue, headPtr).x;
	return true;
}

void main()
{
}