#pragma once

#include <iostream>
#include <vector>

#include "../util/NamespaceDecl.h"

class AliasTable
{
public:
	template<typename T>
	static std::pair<std::vector<T>, std::vector<float>> build(const std::vector<float>& pdf)
	{
		typedef std::pair<int, float> Element;
		int n = pdf.size();
		std::vector<T> alias(n);
		std::vector<float> prob = pdf;

		float sumPdf = 0.0f;
		for (int i = 0; i < n; i++)
			sumPdf += prob[i];

		float sumInv = static_cast<float>(n) / sumPdf;

		Element* greater = new Element[n * 2], * lesser = new Element[n * 2];
		int gTop = 0, lTop = 0;

		for (int i = 0; i < n; i++)
		{
			prob[i] *= sumInv;
			(prob[i] < 1.0f ? lesser[lTop++] : greater[gTop++]) = Element(i, prob[i]);
		}
		while (gTop != 0 && lTop != 0)
		{
			auto [l, pl] = lesser[--lTop];
			auto [g, pg] = greater[--gTop];
			alias[l] = g, prob[l] = pl;

			pg += pl - 1.0f;
			(pg < 1.0f ? lesser[lTop++] : greater[gTop++]) = Element(g, pg);
		}
		while (gTop != 0)
		{
			auto [g, pg] = greater[--gTop];
			alias[g] = g, prob[g] = pg;
		}
		while (lTop != 0)
		{
			auto [l, pl] = lesser[--lTop];
			alias[l] = l, prob[l] = pl;
		}

		delete[] greater;
		delete[] lesser;

		return { alias, prob };
	}
};