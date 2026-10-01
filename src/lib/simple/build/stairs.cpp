#include "stairs.h"
#include "internal.h"

using namespace SimpleMode;
using namespace SimpleMode::detail;

namespace SimpleMode::detail {

void SlopedBar(AuScFaceList& f, const StairFrame& fr, double a0, double a1, double t0, double t1, double lo0, double hi0,
	double lo1, double hi1, int skip)
{
	if(a1 - a0 < 1e-4 || t1 - t0 < 1e-4)
		return;
	const AuPoint c[8] = {fr.At(t0, a0, lo0), fr.At(t0, a1, lo0), fr.At(t0, a1, hi0), fr.At(t0, a0, hi0),
		fr.At(t1, a0, lo1), fr.At(t1, a1, lo1), fr.At(t1, a1, hi1), fr.At(t1, a0, hi1)};
	AuPoint mid;
	for(const AuPoint& p : c)
		mid = mid + p * 0.125;
	auto quad = [&](int i, int j, int k, int l) {
		AuPoint n = (c[j] - c[i]).Cross(c[l] - c[i]);
		double len = n.Length();
		if(len < 1e-9)
			n = (c[k] - c[j]).Cross(c[i] - c[j]), len = n.Length();
		if(len < 1e-9)
			return;
		n = n * (1 / len);
		AuPoint centre = (c[i] + c[j] + c[k] + c[l]) * 0.25;
		if(n.Dot(centre - mid) < 0)
			n = n * -1;
		AddQuad(f, c[i], c[j], c[k], c[l], n);
	};
	if(!(skip & BoxStart))
		quad(0, 1, 2, 3);
	if(!(skip & BoxEnd))
		quad(4, 5, 6, 7);
	if(!(skip & BoxBottom))
		quad(0, 1, 5, 4);
	if(!(skip & BoxTop))
		quad(3, 2, 6, 7);
	if(!(skip & BoxBack))
		quad(0, 3, 7, 4);
	if(!(skip & BoxFront))
		quad(1, 2, 6, 5);
}

void StairFlightMesh(AuScFaceList& main, AuScFaceList& dark, AuScFaceList& glass, const StairFrame& fr, double z0, double z1,
	const QString& style)
{
	const double L = fr.len, h = fr.w / 2;
	const int n = qMax(2, qRound((z1 - z0) / 0.19));
	const double riser = (z1 - z0) / n, tread = L / n;
	auto z = [&](double t) { return z0 + (z1 - z0) * t / L; };
	auto flat = [&](AuScFaceList& f, double a0, double a1, double t0, double t1, double lo, double hi, int skip = 0) {
		SlopedBar(f, fr, a0, a1, t0, t1, lo, hi, lo, hi, skip);
	};
	auto rails = [&](AuScFaceList& f, double off, double post, double every, bool balusters) {
		for(double side : {-1.0, 1.0})
		{
			double a = side * (h + off);
			int posts = qMax(1, int(std::ceil((L - 0.1) / every)));
			for(int i = 0; i <= posts; i++)
			{
				double t = 0.05 + (L - 0.1) * i / posts;
				flat(f, a - post / 2, a + post / 2, t - post / 2, t + post / 2, z(t), z(t) + 1.0);
			}
			SlopedBar(f, fr, a - 0.03, a + 0.03, 0, L, z(0) + 0.9, z(0) + 0.96, z(L) + 0.9, z(L) + 0.96);
			if(balusters)
				for(double t = 0.3; t < L - 0.2; t += 0.25)
				{
					int k = qBound(0, int(t / tread), n - 1);
					flat(f, a - 0.015, a + 0.015, t - 0.015, t + 0.015, z0 + (k + 1) * riser, z(t) + 0.9, BoxTop);
				}
		}
	};
	if(style == "ramp")
	{
		SlopedBar(main, fr, -h, h, 0, L, z(0) - 0.15, z(0), z(L) - 0.15, z(L));
		for(double side : {-1.0, 1.0})
			SlopedBar(main, fr, side < 0 ? -h : h - 0.1, side < 0 ? -h + 0.1 : h, 0, L, z(0), z(0) + 0.1, z(L), z(L) + 0.1);
		rails(dark, -0.05, 0.05, 1.2, false);
		return;
	}
	for(int k = 0; k < n; k++)
	{
		double t0 = k * tread, t1 = (k + 1) * tread, top = z0 + (k + 1) * riser;
		if(style == "stone")
		{
			flat(main, -h, h, t0, t1, 0, top, BoxBottom | BoxBack | BoxFront | BoxStart | (k + 1 < n ? BoxEnd : 0));
			const AuPoint back = fr.At(-1, 0, 0) - fr.At(0, 0, 0);
			AddQuad(main, fr.At(t0, -h, top - riser), fr.At(t0, -h, top), fr.At(t0, h, top), fr.At(t0, h, top - riser), back);
		}
		else if(style == "steel")
			flat(dark, -h, h, t0, t1 - 0.01, top - 0.03, top);
		else if(style == "floating")
			flat(main, -h, h, t0 + 0.02, t1, top - 0.08, top);
		else
		{
			flat(dark, -h, h, t0 - 0.03, t1, top - 0.04, top);
			flat(main, -h + 0.02, h - 0.02, t0, t0 + 0.02, top - riser, top - 0.04, BoxTop);
		}
	}
	if(style == "stone")
	{
		for(double side : {-1.0, 1.0})
		{
			double a0 = side < 0 ? -h - 0.2 : h, a1 = side < 0 ? -h : h + 0.2;
			SlopedBar(main, fr, a0, a1, 0, L, 0, z(0) + 0.9, 0, z(L) + 0.9, BoxBottom | BoxTop);
			SlopedBar(dark, fr, a0 - 0.02, a1 + 0.02, 0, L, z(0) + 0.9, z(0) + 0.98, z(L) + 0.9, z(L) + 0.98);
		}
		return;
	}
	if(style == "floating")
	{
		SlopedBar(dark, fr, -0.12, 0.12, 0, L, z(0) - 0.35, z(0) - 0.08, z(L) - 0.35, z(L) - 0.08);
		for(double side : {-1.0, 1.0})
		{
			double a = side * (h + 0.02);
			SlopedBar(glass, fr, a - 0.006, a + 0.006, 0.05, L - 0.05, z(0) + 0.05, z(0) + 0.95, z(L) + 0.05, z(L) + 0.95);
			SlopedBar(dark, fr, a - 0.025, a + 0.025, 0, L, z(0) + 0.95, z(0) + 1.0, z(L) + 0.95, z(L) + 1.0);
		}
		return;
	}
	for(double side : {-1.0, 1.0})
	{
		double a0 = side < 0 ? -h - 0.05 : h, a1 = side < 0 ? -h : h + 0.05;
		SlopedBar(style == "steel" ? main : main, fr, a0, a1, 0, L, z(0) - 0.25, z(0) + 0.12, z(L) - 0.25, z(L) + 0.12);
	}
	if(style == "steel")
	{
		rails(main, 0.03, 0.04, 1.0, false);
		for(double side : {-1.0, 1.0})
		{
			double a = side * (h + 0.03);
			SlopedBar(main, fr, a - 0.02, a + 0.02, 0, L, z(0) + 0.48, z(0) + 0.52, z(L) + 0.48, z(L) + 0.52);
		}
	}
	else
		rails(main, 0.025, 0.08, L, true);
}

}
