#include "Shape.h"

#include <queue>
#include <fstream>
#include <sstream>
#include <cstring>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void copyMemF(float* &dst, const void* src, int count)
{
	memcpy(dst, src, count << 2);
	dst += count;
}

void addMemF(float*& dst, float v)
{
	*dst++ = v;
}

void addMemF2(float*& dst, float u, float v)
{
	*dst++ = u;
	*dst++ = v;
}

void addMemF3(float*& dst, float u, float v, float w)
{
	*dst++ = u;
	*dst++ = v;
	*dst++ = w;
}

Shape::Shape() :
	m_WithTangents(false)
{
	m_Layout.add<GL_FLOAT>(3);
	m_Layout.add<GL_FLOAT>(2);
	m_Layout.add<GL_FLOAT>(3);
}

Shape::~Shape()
{
}

Shape::Shape(int vertexCount, int type) :
	m_VertexCount(vertexCount), m_Type(type), m_WithTangents(false)
{
	m_Layout.add<GL_FLOAT>(3);
	m_Layout.add<GL_FLOAT>(2);
	m_Layout.add<GL_FLOAT>(3);
}

void Shape::addTangents()
{
	if (m_Buffer == nullptr)
	{
		std::cout << "No data yet" << std::endl;
		return;
	}
	if (m_WithTangents)
	{
		std::cout << "Already added tangents" << std::endl;
		return;
	}
	int trianglesCount = m_VertexCount / 3;
	float* tmp = new float[m_VertexCount * 11];
	float* indDst = tmp;
	float* indSrc = m_Buffer;
	for (int i = 0; i < trianglesCount; i++)
	{
		glm::vec3 p1(m_Buffer[i * 24 + 0], m_Buffer[i * 24 + 1], m_Buffer[i * 24 + 2]);
		glm::vec3 p2(m_Buffer[i * 24 + 8], m_Buffer[i * 24 + 9], m_Buffer[i * 24 + 10]);
		glm::vec3 p3(m_Buffer[i * 24 + 16], m_Buffer[i * 24 + 17], m_Buffer[i * 24 + 18]);
		glm::vec2 uv1(m_Buffer[i * 24 + 3], m_Buffer[i * 24 + 4]);
		glm::vec2 uv2(m_Buffer[i * 24 + 11], m_Buffer[i * 24 + 12]);
		glm::vec2 uv3(m_Buffer[i * 24 + 19], m_Buffer[i * 24 + 20]);

		glm::vec3 dp1 = p2 - p1;
		glm::vec3 dp2 = p3 - p1;
		glm::vec2 duv1 = uv2 - uv1;
		glm::vec2 duv2 = uv3 - uv1;

		float f = 1.0f / (duv1.x * duv2.y - duv2.x * duv1.y);

		float tanX = f * (duv2.y * dp1.x - duv1.y * dp2.x);
		float tanY = f * (duv2.y * dp1.y - duv1.y * dp2.y);
		float tanZ = f * (duv2.y * dp1.z - duv1.y * dp2.z);
		float btanX = f * (-duv2.x * dp1.x + duv1.x * dp2.x);
		float btanY = f * (-duv2.x * dp1.y + duv1.x * dp2.y);
		float btanZ = f * (-duv2.x * dp1.z + duv1.x * dp2.z);

		glm::vec3 tangent = glm::normalize(glm::vec3(tanX, tanY, tanZ));
		glm::vec3 btangent = glm::normalize(glm::vec3(btanX, btanY, btanZ));

		for (int j = 0; j < 3; j++)
		{
			copyMemF(indDst, indSrc, 8);
			copyMemF(indDst, &tangent, 3);
			indSrc += 8;
		}
	}
	delete[]m_Buffer;
	m_Buffer = tmp;
	m_WithTangents = true;
	m_Layout.add<GL_FLOAT>(3);
}

void Shape::setupVA()
{
	if (m_Buffer == nullptr)
	{
		std::cout << "Error: Shape::setupVA: Do not set up VA repeatedly" << std::endl;
		return;
	}
	m_VB.allocate(m_VertexCount * m_Layout.stride(), m_Buffer, m_VertexCount);
	m_VA.addBuffer(m_VB, m_Layout);
	delete[]m_Buffer;
}

void Shape::set(float* buffer, int type, int vertexCount)
{
	m_Buffer = buffer;
	m_Type = type;
	m_VertexCount = vertexCount;
}

void Shape::setBuffer(float* buffer)
{
	m_Buffer = buffer;
}

void Shape::saveObj(const char* path)
{
	if (m_Buffer == nullptr)
	{
		std::cout << "Error: Shape::ExportObj: No data" << std::endl;
		return;
	}

	if (m_WithTangents)
	{
		std::cout << "Error: Shape::ExportObj: Already added tangents" << std::endl;
		return;
	}

	std::ofstream file;
	file.open(path);

	for (int i = 0; i < m_VertexCount; i++)
	{
		file << "v " << std::fixed << m_Buffer[i * 8 + 0] << " " << m_Buffer[i * 8 + 2] << " " << m_Buffer[i * 8 + 1] << std::endl;
	}

	for (int i = 0; i < m_VertexCount; i++)
	{
		file << "vt " << std::fixed << m_Buffer[i * 8 + 3] << " " << m_Buffer[i * 8 + 4] << std::endl;
	}

	for (int i = 0; i < m_VertexCount; i++)
	{
		float x = m_Buffer[i * 8 + 5];
		x = _isnan(x) ? 0.0f : x;
		float y = m_Buffer[i * 8 + 7];
		y = _isnan(y) ? 0.0f : y;
		float z = m_Buffer[i * 8 + 6];
		z = _isnan(z) ? 1.0f : z;
		file << "vn " << std::fixed << x << " " << y << " " << z << std::endl;
	}

	for (int i = 1; i <= m_VertexCount; i += 3)
	{
		file << "f ";
		file << i + 0 << "/" << i + 0 << "/" << i + 0 << " ";
		file << i + 1 << "/" << i + 1 << "/" << i + 1 << " ";
		file << i + 2 << "/" << i + 2 << "/" << i + 2 << " ";
		file << std::endl;
	}
	file.close();
}

Cube::Cube() :
	Shape(36, CUBE)
{
	float* buffer = new float[288];
	memcpy(buffer, CUBE_VERTICES, 288 * sizeof(float));
	setBuffer(buffer);
}

Square::Square() :
	Shape(6, SQUARE)
{
	float* buffer = new float[48];
	memcpy(buffer, SQUARE_VERTICES, 48 * sizeof(float));
	setBuffer(buffer);
}

Cone::Cone(int faces, float radius, float height, int normalType) :
	Shape(faces * 3, CONE)
{
	int vertexCount = faces * 3;
	float* buf = new float[vertexCount * 8];
	float* ind = buf;
	float alpha = glm::radians(360.0f / (float)faces);
	for (int i = 0; i < faces; i++)
	{
		float a = i * alpha, ap = (i + 1) * alpha;
		float x1 = radius * cos(a), y1 = radius * sin(a);
		float x2 = radius * cos(ap), y2 = radius * sin(ap);
		glm::vec3 Pa(x1, y1, 0.0);
		glm::vec3 Pb(x2, y2, 0.0);
		glm::vec3 Pt(0.0, 0.0, height);
		glm::vec3 va(x1, y1, -height);
		glm::vec3 vb(x2, y2, -height);
		glm::vec3 norm = glm::normalize(glm::cross(va, vb));
		glm::vec3 normA = glm::normalize(glm::cross(va, glm::vec3(-y1, x1, 0.0)));
		glm::vec3 normB = glm::normalize(glm::cross(vb, glm::vec3(-y2, x2, 0.0)));
		float sqx1 = 1.0f / faces * i;
		float sqx2 = 1.0f / faces * (i + 1);

		copyMemF(ind, &Pa, 3), addMemF2(ind, sqx1, 1.0);
		if (normalType == Shape::VERTEX) copyMemF(ind, &normA, 3);
		else copyMemF(ind, &norm, 3);
		copyMemF(ind, &Pb, 3), addMemF2(ind, sqx2, 1.0);
		if (normalType == Shape::VERTEX) copyMemF(ind, &normB, 3);
		else copyMemF(ind, &norm, 3);
		copyMemF(ind, &Pt, 3), addMemF2(ind, sqx1, 0.0), copyMemF(ind, &norm, 3);
	}
	setBuffer(buf);
}

Sphere::Sphere(int columns, int rows, float radius, int normalType, float Atheta, float Arho) :
	Shape(rows * columns * 6, SPHERE)
{
	int vertexCount = rows * columns * 6;
	float* buffer = new float[vertexCount * 8];
	float* ind = buffer;
	float theta = glm::radians(Atheta / (float)columns);
	float rho = glm::radians(Arho / (float)rows);
	for (int i = 0; i < columns; i++)
	{
		float t = i * theta, tp = (i + 1) * theta;
		for (int j = 0; j < rows; j++)
		{
			float r = j * rho, rp = (j + 1) * rho;
			float z1 = radius * cos(r);
			float z2 = radius * cos(rp);
			float x1 = radius * sin(r) * cos(t);
			float x2 = radius * sin(r) * cos(tp);
			float x3 = radius * sin(rp) * cos(t);
			float x4 = radius * sin(rp) * cos(tp);
			float y1 = radius * sin(r) * sin(t);
			float y2 = radius * sin(r) * sin(tp);
			float y3 = radius * sin(rp) * sin(t);
			float y4 = radius * sin(rp) * sin(tp);
			glm::vec3 va(x3 - x2, y3 - y2, z2 - z1);
			glm::vec3 vb(x4 - x1, y4 - y1, z2 - z1);
			glm::vec3 norm = glm::normalize(glm::cross(va, vb));
			glm::vec3 norm1 = glm::normalize(glm::vec3(x1, y1, z1));
			glm::vec3 norm2 = glm::normalize(glm::vec3(x2, y2, z1));
			glm::vec3 norm3 = glm::normalize(glm::vec3(x3, y3, z2));
			glm::vec3 norm4 = glm::normalize(glm::vec3(x4, y4, z2));
			float sqx1 = 1.0f / columns * i;
			float sqx2 = 1.0f / columns * (i + 1);
			float sqy1 = 1.0f / rows * j;
			float sqy2 = 1.0f / rows * (j + 1);

			addMemF3(ind, x1, y1, z1), addMemF2(ind, sqx1, sqy1);
			if (normalType == FACE) copyMemF(ind, &norm, 3);
			else	 copyMemF(ind, &norm1, 3);
			addMemF3(ind, x2, y2, z1), addMemF2(ind, sqx2, sqy1);
			if (normalType == FACE) copyMemF(ind, &norm, 3);
			else	 copyMemF(ind, &norm2, 3);
			addMemF3(ind, x3, y3, z2), addMemF2(ind, sqx1, sqy2);
			if (normalType == FACE) copyMemF(ind, &norm, 3);
			else	 copyMemF(ind, &norm3, 3);
			addMemF3(ind, x4, y4, z2), addMemF2(ind, sqx2, sqy2);
			if (normalType == FACE) copyMemF(ind, &norm, 3);
			else	 copyMemF(ind, &norm4, 3);
			addMemF3(ind, x3, y3, z2), addMemF2(ind, sqx1, sqy2);
			if (normalType == FACE) copyMemF(ind, &norm, 3);
			else	 copyMemF(ind, &norm3, 3);
			addMemF3(ind, x2, y2, z1), addMemF2(ind, sqx2, sqy1);
			if (normalType == FACE) copyMemF(ind, &norm, 3);
			else	 copyMemF(ind, &norm2, 3);
		}
	}
	setBuffer(buffer);
}

Torus::Torus(int columns, int rows, float majorRadius, float minorRadius, int normalType, float Atheta, float Btheta) :
	Shape(columns* rows * 6, TORUS)
{
	int vertexCount = columns * rows * 6;
	float* buffer = new float[vertexCount * 8];
	float* ind = buffer;
	float dAtheta = glm::radians(Atheta / (float)columns);
	float dBtheta = glm::radians(Btheta / (float)rows);
	for (int i = 0; i < columns; i++)
	{
		for (int j = 0; j < rows; j++)
		{
			float at = i * dAtheta, atp = (i + 1) * dAtheta;
			float bt = j * dBtheta, btp = (j + 1) * dBtheta;
			float z1 = minorRadius * sin(bt);
			float z2 = minorRadius * sin(btp);
			float x1 = (majorRadius - minorRadius * cos(bt)) * cos(at);
			float x2 = (majorRadius - minorRadius * cos(bt)) * cos(atp);
			float x3 = (majorRadius - minorRadius * cos(btp)) * cos(at);
			float x4 = (majorRadius - minorRadius * cos(btp)) * cos(atp);
			float y1 = (majorRadius - minorRadius * cos(bt)) * sin(at);
			float y2 = (majorRadius - minorRadius * cos(bt)) * sin(atp);
			float y3 = (majorRadius - minorRadius * cos(btp)) * sin(at);
			float y4 = (majorRadius - minorRadius * cos(btp)) * sin(atp);
			glm::vec3 va(x3 - x2, y3 - y2, z2 - z1);
			glm::vec3 vb(x4 - x1, y4 - y1, z2 - z1);
			glm::vec3 norm = glm::normalize(glm::cross(va, vb));
			glm::vec3 N1(x1 - majorRadius * cos(at), y1 - majorRadius * sin(at), z1);
			glm::vec3 N2(x2 - majorRadius * cos(atp), y2 - majorRadius * sin(atp), z1);
			glm::vec3 N3(x3 - majorRadius * cos(at), y3 - majorRadius * sin(at), z2);
			glm::vec3 N4(x4 - majorRadius * cos(atp), y4 - majorRadius * sin(atp), z2);
			float sqx1 = 1.0f / columns * i;
			float sqx2 = 1.0f / columns * (i + 1);
			float sqy1 = 1.0f / rows * j;
			float sqy2 = 1.0f / rows * (j + 1);

			addMemF3(ind, x1, y1, z1), addMemF2(ind, sqx1, sqy1);
			if (normalType == FACE) copyMemF(ind, &norm, 3);
			else	 copyMemF(ind, &N1, 3);
			addMemF3(ind, x2, y2, z1), addMemF2(ind, sqx2, sqy1);
			if (normalType == FACE) copyMemF(ind, &norm, 3);
			else	 copyMemF(ind, &N2, 3);
			addMemF3(ind, x3, y3, z2), addMemF2(ind, sqx1, sqy2);
			if (normalType == FACE) copyMemF(ind, &norm, 3);
			else	 copyMemF(ind, &N3, 3);
			addMemF3(ind, x4, y4, z2), addMemF2(ind, sqx2, sqy2);
			if (normalType == FACE) copyMemF(ind, &norm, 3);
			else	 copyMemF(ind, &N4, 3);
			addMemF3(ind, x3, y3, z2), addMemF2(ind, sqx1, sqy2);
			if (normalType == FACE) copyMemF(ind, &norm, 3);
			else	 copyMemF(ind, &N3, 3);
			addMemF3(ind, x2, y2, z1), addMemF2(ind, sqx2, sqy1);
			if (normalType == FACE) copyMemF(ind, &norm, 3);
			else	 copyMemF(ind, &N2, 3);
		}
	}
	setBuffer(buffer);
}

const float EPS = 1e-6;

const float C[5][5] =
{
	{ 1, 0, 0, 0, 0 },
	{ 1, 1, 0, 0, 0 },
	{ 1, 2, 1, 0, 0 },
	{ 1, 3, 3, 1, 0 },
	{ 1, 4, 6, 4, 1 }
};

float B(int n, int i, float u)
{
	return C[n][i] * pow(u, i) * pow(1 - u, n - i);
}

float dB(int n, int i, float u)
{
	return C[n][i] * (i == 0 ? 0 : i * pow(u, i - 1) * pow(1 - u, n - i))
		- C[n][i] * (i == n ? 0 : (n - i) * pow(u, i) * pow(1 - u, n - i - 1));
}

glm::vec3 calcBezierPoint(int n, int m, float u, float v, const std::vector<glm::vec3>& points)
{
	glm::vec3 ret(0.0);
	for (int i = 0; i <= n; i++)
	{
		for (int j = 0; j <= m; j++)
		{
			ret += B(n, i, u) * B(m, j, v) * points[i * (m + 1) + j];
		}
	}
	return ret;
}

glm::vec3 calcBezierNormal(int n, int m, float u, float v, const std::vector<glm::vec3>& points)
{
	glm::vec3 tan(0.0), btg(0.0);
	for (int i = 0; i <= n; i++)
	{
		for (int j = 0; j <= m; j++)
		{
			tan += dB(n, i, u) * B(m, j, v) * points[i * (m + 1) + j];
			btg += B(n, i, u) * dB(m, j, v) * points[i * (m + 1) + j];
		}
	}
	return -glm::normalize(glm::cross(tan, btg));
}

Bezier::Bezier(int _n, int _m, int _secU, int _secV, const std::vector<glm::vec3>& points, int normalType) :
	Shape(_secU * _secV * 6, BEZIER), n(_n), m(_m), secU(_secU), secV(_secV)
{
	float* buffer = bezierGenerate(n, m, secU, secV, points, normalType);
	setBuffer(buffer);
}

BezierCurves::BezierCurves(const char* BPTfilePath, int secU, int secV, int normalType)
{
	std::fstream file;
	file.open(BPTfilePath);
	int curveCount;
	file >> curveCount;
	int vertexCount = curveCount * secU * secV * 6;
	float* buffer = new float[vertexCount * 8];
	int stride = secU * secV * 6 * 8;
	int offset = 0;
	for (int i = 0; i < curveCount; i++)
	{
		int n, m;
		file >> n >> m;
		std::vector<glm::vec3> points;
		for (int j = 0; j < (n + 1) * (m + 1); j++)
		{
			glm::vec3 p;
			file >> p.x >> p.y >> p.z;
			points.push_back(p);
		}
		float* curve = bezierGenerate(n, m, secU, secV, points, normalType);
		memcpy(buffer + offset, curve, stride * sizeof(float));
		delete[]curve;
		offset += stride;
	}
	file.close();
	set(buffer, TEAPOT, vertexCount);
}

float* bezierGenerate(int n, int m, int secU, int secV, const std::vector<glm::vec3>& points, int normalType)
{
	if ((n + 1) * (m + 1) > points.size())
	{
		std::cout << "Error: Bezier generator::vector size not equal to (n + 1) * (m + 1)" << std::endl;
		return nullptr;
	}
	int vertexCount = secU * secV * 6;
	float* buf = new float[vertexCount * 8];
	float* ind = buf;

	float du = 1.0 / (float)secU;
	float dv = 1.0 / (float)secV;

	for (int i = 0; i < secU; i++)
	{
		for (int j = 0; j < secV; j++)
		{
			float u = i * du, v = j * dv;
			float up = (i + 1) * du, vp = (j + 1) * dv;
			glm::vec3 P1 = calcBezierPoint(n, m, u, v, points);
			glm::vec3 P2 = calcBezierPoint(n, m, up, v, points);
			glm::vec3 P3 = calcBezierPoint(n, m, up, vp, points);
			glm::vec3 P4 = calcBezierPoint(n, m, u, vp, points);
			glm::vec3 N1 = calcBezierNormal(n, m, u, v, points);
			glm::vec3 N2 = calcBezierNormal(n, m, up, v, points);
			glm::vec3 N3 = calcBezierNormal(n, m, up, vp, points);
			glm::vec3 N4 = calcBezierNormal(n, m, u, vp, points);
			glm::vec3 norm123 = glm::normalize(glm::cross(P3 - P1, P2 - P1));
			glm::vec3 norm134 = glm::normalize(glm::cross(P4 - P1, P3 - P1));

			bool degenerated = (glm::length(P1 - P2) < EPS)
				|| (glm::length(P1 - P4) < EPS)
				|| (glm::length(P2 - P3) < EPS)
				|| (glm::length(P3 - P4) < EPS);
			degenerated |= (normalType == Shape::FACE);

			copyMemF(ind, &P1, 3), addMemF2(ind, u, v);
			if (degenerated) copyMemF(ind, &norm123, 3);
			else	 copyMemF(ind, &N1, 3);
			copyMemF(ind, &P2, 3), addMemF2(ind, up, v);
			if (degenerated) copyMemF(ind, &norm123, 3);
			else	 copyMemF(ind, &N2, 3);
			copyMemF(ind, &P3, 3), addMemF2(ind, up, vp);
			if (degenerated) copyMemF(ind, &norm123, 3);
			else	 copyMemF(ind, &N3, 3);

			copyMemF(ind, &P1, 3), addMemF2(ind, u, v);
			if (degenerated) copyMemF(ind, &norm134, 3);
			else	 copyMemF(ind, &N1, 3);
			copyMemF(ind, &P3, 3), addMemF2(ind, up, vp);
			if (degenerated) copyMemF(ind, &norm134, 3);
			else	 copyMemF(ind, &N3, 3);
			copyMemF(ind, &P4, 3), addMemF2(ind, u, vp);
			if (degenerated) copyMemF(ind, &norm134, 3);
			else	 copyMemF(ind, &N4, 3);
		}
	}
	return buf;
}
