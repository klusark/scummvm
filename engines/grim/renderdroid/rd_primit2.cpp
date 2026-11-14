/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/textconsole.h"

#include "engines/grim/renderdroid/rd.h"

namespace Grim {

void RenderDroid::Primit2Module::plot(RdCanvas *canvas, int x, int y, uint16 color) {
	StdVBuffer *vBuffer = canvas->vBuffer;

	if (x < 0 || y < 0 || x >= vBuffer->tex.width || y >= vBuffer->tex.height) {
		warning("RenderDroid::Primit2Module::plot(): pixel outside the buffer");
		return;
	}

	if (vBuffer->tex.format.bpp == 8)
		vBuffer->pixels[y * vBuffer->tex.pitchInBytes + x] = (byte)color;
	else
		*(uint16 *)&vBuffer->pixels[2 * x + 2 * y * vBuffer->tex.pitchInPixels] = color;
}

void RenderDroid::Primit2Module::drawBresenham(RdCanvas *canvas, int x0, int y0, int x1, int y1, uint16 color, uint pattern) {
	uint patternBit = 0x80000000;
	int x = x0;
	int y = y0;
	int dx = x1 - x0;
	int dy = y1 - y0;
	int stepX = (dx <= 0) ? -1 : 1;
	int stepY = (dy <= 0) ? -1 : 1;

	if (stepX < 0)
		dy = -dy;

	if (stepY > 0)
		dx = -dx;

	if ((pattern & 0x80000000) != 0)
		plot(canvas, x, y, color);

	int error = 0;

	while (x != x1 || y != y1) {
		patternBit >>= 1;
		if (!patternBit)
			patternBit = 0x80000000;

		int errorY = error + dy;
		error += dx;

		if (abs(errorY) >= abs(error)) {
			y += stepY;
		} else {
			x += stepX;
			error = errorY;
		}

		if ((patternBit & pattern) != 0)
			plot(canvas, x, y, color);
	}
}

void RenderDroid::Primit2Module::drawLine(RdCanvas *canvas, int x0, int y0, int x1, int y1, uint16 color, uint pattern) {
	drawBresenham(canvas, x0, y0, x1, y1, color, pattern);
}

int RenderDroid::Primit2Module::drawClippedLine(RdCanvas *canvas, int x0, int y0, int x1, int y1, uint16 color, uint pattern) {
	if (!_rd->_clip.line2(canvas, &x0, &y0, &x1, &y1))
		return 0;

	drawBresenham(canvas, x0, y0, x1, y1, color, pattern);

	return 1;
}

void RenderDroid::Primit2Module::drawCircle(RdCanvas *canvas, int cx, int cy, float radius, float angleStep, uint16 color, uint pattern) {
	int radiusInt = (int)((double)radius + 0.5);

	if (radiusInt + cx >= canvas->xStart && cx - radiusInt <= canvas->xEnd && radiusInt + cy >= canvas->yStart && cy - radiusInt <= canvas->yEnd) {
		float sinValue;
		float cosValue;

		_rd->_stdMath->sinCos(0.0f, &sinValue, &cosValue);

		int x = cx + (int)((double)cosValue * radius + 0.5);
		int y = cy + (int)((double)sinValue * radius + 0.5);
		float angle = angleStep;

		if (angleStep <= 360.0) {
			while (true) {
				_rd->_stdMath->sinCos(angle, &sinValue, &cosValue);

				int nextX = cx + (int)((double)cosValue * radius + 0.5);
				int nextY = cy + (int)((double)sinValue * radius + 0.5);

				drawClippedLine(canvas, x, y, nextX, nextY, color, pattern);

				double nextAngle = (double)angle + angleStep;
				x = nextX;
				y = nextY;
				angle = (float)nextAngle;

				if (_rd->_remastered) {
					if (!(angle <= 360.0))
						break;
				} else if (nextAngle > 360.0) {
					break;
				}
			}
		}
	}
}

void RenderDroid::Primit2Module::drawRectangle(RdCanvas *canvas, int x0, int y0, int x1, int y1, uint16 color, uint pattern) {
	drawClippedLine(canvas, x0, y0, x1, y0, color, pattern);
	drawClippedLine(canvas, x1, y0, x1, y1, color, pattern);
	drawClippedLine(canvas, x0, y0, x0, y1, color, pattern);
	drawClippedLine(canvas, x0, y1, x1, y1, color, pattern);
}

} // End of namespace Grim
