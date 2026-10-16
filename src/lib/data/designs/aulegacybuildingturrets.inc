// DpBuilding::RenderTurret (2016 0x5edf10) shares these exact thirteen
// control points and sixteen dome triangles with the spacecraft renderer.
QVector<QVector3D> TurretPoints(const QVector3D& center, const QVector3D& right,
	const QVector3D& outward, const QVector3D& up)
{
	// DaSpaceShip::RenderTurret and RenderExteriorWalls use the same 13 points.
	const float r = float(AuLegacyShip::CoordinateSpacing);
	const QVector3D shape[] = {{r, 0, 0}, {2, 0, -2}, {0, 0, -r}, {-2, 0, -2},
		{-r, 0, 0}, {-2, 0, 2}, {0, 0, r}, {2, 0, 2},
		{2, 2, 0}, {0, 2, -2}, {-2, 2, 0}, {0, 2, 2}, {0, r, 0}};
	QVector<QVector3D> points;
	for(const QVector3D& p : shape)
		points.append(center + right * p.x() + outward * p.y() + up * p.z());
	return points;
}

void TurretDome(Builder& out, const QVector<QVector3D>& points, int deck, bool interior)
{
	static const int triangles[][3] = {{8,12,9}, {1,8,9}, {8,1,0}, {8,0,7},
		{9,12,10}, {3,9,10}, {9,3,2}, {9,2,1}, {10,12,11}, {5,10,11},
		{10,5,4}, {10,4,3}, {11,12,8}, {7,11,8}, {11,7,6}, {11,6,5}};
	// Both original dome paths use the lower-left glass half of ST_Turret.
	// The upper-right hull half belongs to the surrounding wall corners.
	const QVector2D uv[3] = {{0, 0}, {0, 1}, {1, 0}};
	for(const auto& tri : triangles)
	{
		if(interior) out.Triangle(36, deck, true, points[tri[0]], points[tri[1]], points[tri[2]], uv[0], uv[1], uv[2]);
		else out.Triangle(36, deck, false, points[tri[0]], points[tri[2]], points[tri[1]], uv[0], uv[2], uv[1]);
	}
}

void SideTurret(Builder& out, int deck, const QVector3D& a0, const QVector3D& b0,
	const QVector3D& b1, const QVector3D& a1, bool interior)
{
	const QVector3D center = (a0 + b1) * .5f;
	const QVector3D right = (a0 - b0).normalized();
	const QVector3D outward(right.y(), -right.x(), 0);
	// right points opposite the boundary walk, so its left normal faces out.
	const QVector3D facing = -outward;
	const QVector<QVector3D> points = TurretPoints(center, right, facing, {0, 0, 1});
	TurretDome(out, points, deck, interior);
	// RenderExteriorWalls maps a complete tile onto each corner separately.
	const QVector3D corners[] = {a0, a1, b1, b0};
	const int ring[][3] = {{0, 1, 2}, {6, 7, 0}, {4, 5, 6}, {2, 3, 4}};
	const QVector2D cornerUV(1, 1), firstUV(1, 0), lastUV(0, 1);
	const QVector2D bevelUV(.6767767f, .6767767f);
	const auto triangle = [&](const QVector3D& a, const QVector3D& b, const QVector3D& c,
		const QVector2D& ua, const QVector2D& ub, const QVector2D& uc)
	{
		if(interior) out.Triangle(36, deck, true, a, b, c, ua, ub, uc);
		else out.Triangle(36, deck, false, a, c, b, ua, uc, ub);
	};
	for(int i = 0; i < 4; ++i)
	{
		triangle(corners[i], points[ring[i][0]], points[ring[i][1]], cornerUV, firstUV, bevelUV);
		triangle(corners[i], points[ring[i][1]], points[ring[i][2]], cornerUV, bevelUV, lastUV);
	}
	if((a0 - b0).length() > 2 * AuLegacyShip::CoordinateSpacing + Epsilon)
	{
		triangle(b0, points[4], b1, {0, 1}, {1, 1}, {1, 0});
		triangle(a1, points[0], a0, {0, 1}, {1, 1}, {1, 0});
	}
}



