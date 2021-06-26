=type lib

uniform samplerBuffer halton;

uint randSeed;

uint hash(uint seed)
{
	seed = (seed ^ uint(61)) ^ (seed >> uint(16));
	seed *= uint(9);
	seed = seed ^ (seed >> uint(4));
	seed *= uint(0x27d4eb2d);
	seed = seed ^ (seed >> uint(15));
	return seed;
}

float rand()
{
	randSeed = hash(randSeed);
	return float(randSeed) * (1.0 / 4294967296.0);
}

uint randUint(uint vMin, uint vMax)
{
	randSeed = hash(randSeed);
	return randSeed % (vMax - vMin);
}

float noise(float x)
{
	float y = fract(sin(x) * 100000.0);
	return y;
}

float noise(vec2 st)
{
	return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float rand(float vMin, float vMax)
{
	return mix(vMin, vMax, rand());
}

vec2 randBox()
{
	return vec2(rand(), rand());
}

uniform int sampleDim;
uniform samplerBuffer sobolSeq;
uniform sampler2D noiseTex;
uniform int sampleNum;
uniform int sampler;

#define Sampler int

int sampleOffset;
uint sampleSeed;

float sample1D(inout Sampler s)
{
	if (sampler == 0) return rand();
	
	float r = texelFetch(sobolSeq, sampleOffset + s).r;
	r += float(sampleSeed) / 4294967296.0;
	sampleSeed = hash(sampleSeed);
	s++;
	//if (s >= sampleDim) s = 0;
	return r > 1.0 ? r - 1.0 : r;
}

vec2 sample2D(inout Sampler s)
{
	return vec2(sample1D(s), sample1D(s));
}