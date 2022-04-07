@type lib

@include math.glsl

uniform vec3 uCamF;
uniform vec3 uCamR;
uniform vec3 uCamU;
uniform mat3 uCamMatInv;
uniform vec3 uCamPos;
uniform float uTanFOV;
uniform float uLensRadius;
uniform float uFocalDist;
uniform float uCamAsp;
uniform ivec2 uFilmSize;

struct CameraPdf
{
	float pdfPos;
	float pdfDir;
};

CameraPdf makeCameraPdf(float pdfPos, float pdfDir)
{
	CameraPdf ret;
	ret.pdfPos = pdfPos;
	ret.pdfDir = pdfDir;
	return ret;
}

struct CameraIiSample
{
	vec3 wi;
	vec3 Ii;
	float dist;
	vec2 uv;
	float pdf;
};

CameraIiSample makeCameraIiSample(vec3 wi, vec3 Ii, float dist, vec2 uv, float pdf)
{
	CameraIiSample ret;
	ret.wi = wi;
	ret.Ii = Ii;
	ret.dist = dist;
	ret.uv = uv;
	ret.pdf = pdf;
	return ret;
}

const CameraIiSample InvalidIiSample = makeCameraIiSample(vec3(0.0), vec3(0.0), 0.0, vec2(0.0), 0);

bool inFilmBound(vec2 uv)
{
	return uv.x >= 0 && uv.x <= 1.0 && uv.y >= 0 && uv.y <= 1.0;
}

bool thinLensCameraDelta()
{
	return uLensRadius <= 1e-6;
}

Ray thinLensCameraSampleRay(vec2 uv, vec4 u)
{
	vec2 texelSize = 1.0 / vec2(uFilmSize);
	vec2 biasedCoord = uv + texelSize * u.xy;
	vec2 ndc = biasedCoord * 2.0 - 1.0;

	vec3 pLens = vec3(toConcentricDisk(u.zw) * uLensRadius, 0.0);
	vec3 pFocusPlane = vec3(ndc * vec2(uCamAsp, 1.0) * uFocalDist * uTanFOV, uFocalDist);
	vec3 dir = pFocusPlane - pLens;
	dir = normalize(uCamR * dir.x + uCamU * dir.y + uCamF * dir.z);

	Ray ret;
	ret.ori = uCamPos + uCamR * pLens.x + uCamU * pLens.y;
	ret.dir = dir;
	return ret;
}

vec2 thinLensCameraRasterPos(Ray ray)
{
	float cosTheta = dot(ray.dir, uCamF);
	float dFocus = uFocalDist / cosTheta;
	vec3 pFocus = uCamMatInv * (rayPoint(ray, dFocus) - uCamPos);

	vec2 filmSize = vec2(uFilmSize);
	float aspect = filmSize.x / filmSize.y;

	pFocus /= vec3(vec2(aspect, 1.0) * uTanFOV, 1.0) * uFocalDist;
	vec2 ndc = pFocus.xy;
	return (ndc + 1.0) * 0.5;
}

vec3 thinLensCameraIe(Ray ray)
{
	float cosTheta = dot(ray.dir, uCamF);
	if (cosTheta < 1e-6)
		return vec3(0.0);

	vec2 pRaster = thinLensCameraRasterPos(ray);
	if (!inFilmBound(pRaster))
		return vec3(0.0);

	float tanFOVInv = 1.0 / uTanFOV;
	float cos2Theta = cosTheta * cosTheta;
	float lensArea = thinLensCameraDelta() ? 1.0 : Pi * uLensRadius * uLensRadius;
	return vec3(0.25) * square(tanFOVInv / cos2Theta) / (lensArea * uCamAsp);
}

CameraIiSample thinLensCameraSampleIi(vec3 ref, vec2 u)
{
	vec3 pLens = vec3(toConcentricDisk(u) * uLensRadius, 0.0);
	vec3 y = uCamPos + uCamR * pLens.x + uCamU * pLens.y + uCamF * pLens.z;
	float dist = distance(ref, y);

	vec3 wi = normalize(y - ref);
	float cosTheta = satDot(uCamF, -wi);
	if (cosTheta < 1e-6)
		return InvalidIiSample;

	Ray ray = makeRay(y, -wi);
	vec3 Ie = thinLensCameraIe(ray);
	vec2 uv = thinLensCameraRasterPos(ray);
	float lensArea = thinLensCameraDelta() ? 1.0 : Pi * uLensRadius * uLensRadius;
	float pdf = dist * dist / (cosTheta * lensArea);

	return makeCameraIiSample(wi, Ie, dist, uv, pdf);
}

CameraPdf thinLensCameraPdfIe(Ray ray)
{
	float cosTheta = dot(uCamF, ray.dir);
	if (cosTheta < 1e-6)
		return makeCameraPdf(0.0, 0.0);

	vec2 pRaster = thinLensCameraRasterPos(ray);
	if (!inFilmBound(pRaster))
		return makeCameraPdf(0.0, 0.0);

	float pdfPos = thinLensCameraDelta() ? 1.0 : 1.0 / (Pi * uLensRadius * uLensRadius);
	float pdfDir = 1.0 / (cosTheta * cosTheta * cosTheta);
	return makeCameraPdf(pdfPos, pdfDir);
}