#pragma once

#include <QList>
#include <QMap>
#include <QPoint>
#include <QString>
#include <QStringList>

#include "entities.h"

namespace SimpleMode {

struct Plan
{
	QString name = "Simple Mode Test";
	int industry = 22;
	double cell = 3.0;
	double height = 3.0;
	double wall = 0.2;
	double partition = 0.2;
	double voidGap = 0.25;
	QString theme = "wood";
	bool interiors = true;
	bool lowPoly = false;
	QString outside, roofing;
	QString outsideTrim, outsideTrimTexture, outsideTrimColor;
	int outsideTrimStrength = 50;
	QMap<QString, int> scales;
	QList<QStringList> decks;
	QMap<QChar, Room> rooms;
	QList<Door> doors;
	QList<Window> windows;
	QList<Railing> railings;
	QList<RailingGap> gaps;
	QList<Station> stations;
	QList<Fixture> fixtures;
	QList<Stair> stairs;
	QList<Hatch> hatches;
	QList<Post> posts;
	QMap<QChar, Roof> roofs;
	QList<QStringList> roofGrids;
	QList<Dormer> dormers;
	QList<Skylight> skylights;

	[[nodiscard]] int Decks() const { return decks.size(); }
	[[nodiscard]] int Rows() const { return decks.isEmpty() ? 0 : decks.first().size(); }
	[[nodiscard]] int Cols() const { return Rows() ? decks.first().first().size() / 4 : 0; }
	bool OnGrid(const QPoint& cell) const { return cell.x() >= 0 && cell.y() >= 0 && cell.x() < Cols() && cell.y() < Rows(); }

	QChar RoofAt(int deck, const QPoint& cell) const
	{
		if(deck < 0 || deck >= roofGrids.size() || cell.x() < 0 || cell.y() < 0 || cell.y() >= roofGrids[deck].size()
				|| cell.x() >= roofGrids[deck][cell.y()].size())
			return '.';
		return roofGrids[deck][cell.y()][cell.x()];
	}
	void SetRoof(int deck, const QPoint& cell, QChar ch)
	{
		while(roofGrids.size() <= deck)
			roofGrids << QStringList();
		QStringList& grid = roofGrids[deck];
		while(grid.size() < Rows())
			grid << QString();
		for(QString& row : grid)
			if(row.size() < Cols())
				row += QString(Cols() - row.size(), '.');
		grid[cell.y()][cell.x()] = ch;
	}

	QChar Quarter(int deck, const QPoint& cell, int quarter) const
	{
		if(deck < 0 || deck >= decks.size() || !OnGrid(cell))
			return '.';
		return decks[deck][cell.y()][cell.x() * 4 + quarter];
	}
	QChar At(int deck, const QPoint& cell) const
	{
		QChar q = Quarter(deck, cell, North);
		for(int i = 1; i < 4; i++)
			if(Quarter(deck, cell, i) != q)
				return '*';
		return q;
	}
	void SetQuarter(int deck, const QPoint& cell, int quarter, QChar ch) { decks[deck][cell.y()][cell.x() * 4 + quarter] = ch; }
	void SetCell(int deck, const QPoint& cell, QChar ch)
	{
		for(int i = 0; i < 4; i++)
			SetQuarter(deck, cell, i, ch);
	}

	double CeilingHt() const { return height - wall; }
	double X0(int c) const { return (c - Cols() / 2.0) * cell; }
	double X1(int c) const { return X0(c) + cell; }
	double Y1(int r) const { return (Rows() / 2.0 - r) * cell; }
	double Y0(int r) const { return Y1(r) - cell; }
	double QX(double x) const { return X0(0) + x * cell / 4; }
	double QY(double y) const { return Y1(0) - y * cell / 4; }
};

}
