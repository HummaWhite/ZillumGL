@type lib

uniform vec3 uCamF;
uniform vec3 uCamR;
uniform vec3 uCamU;
uniform vec3 uCamPos;
uniform float uTanFOV;
uniform float uLensRadius;
uniform float uFocalDist;
uniform float uCamAsp;
uniform ivec2 uFilmSize;

Ray sampleLensCamera(vec2 uv, inout Sampler s)
{
	vec2 texelSize = 1.0 / vec2(uFilmSize);
	vec2 biasedCoord = uv + texelSize * sample2D(s);
	vec2 ndc = biasedCoord * 2.0 - 1.0;

	vec3 pLens = vec3(toConcentricDisk(sample2D(s)) * uLensRadius, 0.0);
	vec3 pFocusPlane = vec3(ndc * vec2(uCamAsp, 1.0) * uFocalDist * uTanFOV, uFocalDist);
	vec3 dir = pFocusPlane - pLens;
	dir = normalize(uCamR * dir.x + uCamU * dir.y + uCamF * dir.z);

	Ray ret;
	ret.ori = uCamPos + uCamR * pLens.x + uCamU * pLens.y;
	ret.dir = dir;
	return ret;
}