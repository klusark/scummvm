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

int RenderDroid::ClipModule::calcOutcode2(RdCanvas *canvas, int x, int y) {
	int outcode = 0;

	if (x >= canvas->xStart) {
		if (x > canvas->xEnd)
			outcode = 0x10;
	} else {
		outcode = 1;
	}

	if (y >= canvas->yStart) {
		if (y > canvas->yEnd)
			outcode |= 0x100;
	} else {
		outcode |= 0x1000;
	}

	return outcode;
}

int RenderDroid::ClipModule::line2(RdCanvas *canvas, int *x0, int *y0, int *x1, int *y1) {
	int outcode0 = calcOutcode2(canvas, *x0, *y0);
	int outcode1 = calcOutcode2(canvas, *x1, *y1);

	if (!outcode0 && !outcode1)
		return 1;

	if ((outcode0 & outcode1) != 0)
		return 0;

	float fx0 = (float)*x0;
	float fx1 = (float)*x1;
	float fy0 = (float)*y0;
	float fy1 = (float)*y1;
	bool vertical = (double)*x0 == fx1;
	bool horizontal = fy0 == fy1;

	while (true) {
		int outcode = outcode0 ? outcode0 : outcode1;
		double newX;
		double newY;

		if ((outcode & 0x1000) != 0) {
			if (vertical)
				newX = fx0;
			else
				newX = ((double)fx1 - fx0) / ((double)fy1 - fy0) * ((double)canvas->yStart - fy0) + fx0;
			newY = (double)canvas->yStart;
		} else if ((outcode & 0x100) != 0) {
			if (vertical)
				newX = fx0;
			else
				newX = ((double)fx1 - fx0) / ((double)fy1 - fy0) * ((double)canvas->yEnd - fy0) + fx0;
			newY = (double)canvas->yEnd;
		} else {
			float clippedY;

			if ((outcode & 0x10) != 0) {
				if (horizontal)
					clippedY = fy0;
				else
					clippedY = (float)(((double)fy1 - fy0) / ((double)fx1 - fx0) * ((double)canvas->xEnd - fx0) + fy0);
				newX = (double)canvas->xEnd;
			} else {
				if (horizontal)
					clippedY = fy0;
				else
					clippedY = (float)(((double)fy1 - fy0) / ((double)fx1 - fx0) * ((double)canvas->xStart - fx0) + fy0);
				newX = (double)canvas->xStart;
			}

			newY = clippedY;
		}

		if (outcode == outcode0) {
			fx0 = (float)newX;
			fy0 = (float)newY;
			outcode0 = calcOutcode2(canvas, (int)newX, (int)newY);
		} else {
			fx1 = (float)newX;
			fy1 = (float)newY;
			outcode1 = calcOutcode2(canvas, (int)newX, (int)newY);
		}

		if (!outcode0 && !outcode1)
			break;

		if ((outcode0 & outcode1) != 0)
			return 0;
	}

	*x0 = (int)fx0;
	*y0 = (int)fy0;
	*x1 = (int)fx1;
	*y1 = (int)fy1;

	return 1;
}

int RenderDroid::ClipModule::point3Remastered(RdClipFrustum *clip, RdVector3 pt) {
	int projectType = _rd->_camera.getCurrent()->projectType;
	float limit;

	if (pt[1] < clip->zNear)
		return 0;

	if (clip->clipFar && clip->zFar < pt[1])
		return 0;

	if (projectType == 1)
		limit = (float)((double)clip->left * pt[1]);
	else
		limit = clip->orthoLeft;
	if (limit > pt[0])
		return 0;

	if (projectType == 1)
		limit = (float)((double)clip->right * pt[1]);
	else
		limit = clip->orthoRight;
	if (limit < pt[0])
		return 0;

	if (projectType == 1)
		limit = (float)((double)clip->top * pt[1]);
	else
		limit = clip->orthoTop;
	if (limit < pt[2])
		return 0;

	if (projectType == 1)
		limit = (float)((double)clip->bottom * pt[1]);
	else
		limit = clip->orthoBottom;

	return !(limit > pt[2]);
}

int RenderDroid::ClipModule::point3(RdClipFrustum *clip, RdVector3 pt) {
	if (_rd->_remastered)
		return point3Remastered(clip, pt);

	RdCamera *camera = _rd->_camera.getCurrent();
	int projectType = camera->projectType;

	if (pt[1] < (double)clip->zNear)
		return 0;

	if (clip->clipFar && pt[1] > (double)clip->zFar)
		return 0;

	double limit;

	if (projectType == 1)
		limit = (double)clip->left * pt[1];
	else
		limit = clip->orthoLeft;
	if (limit > pt[0])
		return 0;

	if (projectType == 1)
		limit = (double)clip->right * pt[1];
	else
		limit = clip->orthoRight;
	if (limit < pt[0])
		return 0;

	if (projectType == 1)
		limit = (double)clip->top * pt[1];
	else
		limit = clip->orthoTop;
	if (limit < pt[2])
		return 0;

	if (projectType == 1)
		limit = (double)clip->bottom * pt[1];
	else
		limit = clip->orthoBottom;

	return limit <= pt[2];
}

int RenderDroid::ClipModule::line3(RdClipFrustum *clip, RdVector3 v0, RdVector3 v1, int *clipped0, int *clipped1) {
	if (v0[1] < (double)clip->zNear && v1[1] < (double)clip->zNear)
		return 0;

	if (v0[1] >= (double)clip->zNear) {
		if (v1[1] < (double)clip->zNear) {
			double t = ((double)clip->zNear - v0[1]) / ((double)v1[1] - v0[1]);
			float x = v1[0];

			v1[1] = clip->zNear;
			v1[2] = (float)(((double)v1[2] - v0[2]) * t + v0[2]);
			v1[0] = (float)(((double)x - v0[0]) * t + v0[0]);
			if (clipped1)
				*clipped1 = 1;
		}
	} else {
		double t = ((double)clip->zNear - v0[1]) / ((double)v1[1] - v0[1]);

		v0[1] = clip->zNear;
		v0[2] = (float)(((double)v1[2] - v0[2]) * t + v0[2]);
		v0[0] = (float)(((double)v1[0] - v0[0]) * t + v0[0]);
		if (clipped0)
			*clipped0 = 1;
	}

	if (clip->clipFar) {
		if (v0[1] > (double)clip->zFar && v1[1] > (double)clip->zFar)
			return 0;

		if (v0[1] <= (double)clip->zFar) {
			if (v1[1] > (double)clip->zFar) {
				double t = ((double)clip->zFar - v0[1]) / ((double)v1[1] - v0[1]);
				float x = v1[0];

				v1[1] = clip->zFar;
				v1[2] = (float)(((double)v1[2] - v0[2]) * t + v0[2]);
				v1[0] = (float)(((double)x - v0[0]) * t + v0[0]);
				if (clipped1)
					*clipped1 = 1;
			}
		} else {
			double t = ((double)clip->zFar - v0[1]) / ((double)v1[1] - v0[1]);

			v0[1] = clip->zFar;
			v0[2] = (float)(((double)v1[2] - v0[2]) * t + v0[2]);
			v0[0] = (float)(((double)v1[0] - v0[0]) * t + v0[0]);
			if (clipped0)
				*clipped0 = 1;
		}
	}

	float leftLimit = (float)((double)clip->left * v1[1]);

	if (v0[0] < (double)clip->left * v0[1] && v1[0] < (double)leftLimit)
		return 0;

	if ((double)clip->left * v0[1] <= v0[0]) {
		if (v1[0] < (double)leftLimit) {
			float dy = (float)((double)v1[1] - v0[1]);
			float dx = (float)((double)v1[0] - v0[0]);
			double c = (double)v1[1] * v0[0] - (double)v0[1] * v1[0];
			double denominator = (double)clip->left * dy - dx;

			if (denominator != 0.0)
				c = c / (float)denominator;

			double s = (double)clip->left * c;
			double t;

			if (fabs((double)dy) <= fabs((double)dx))
				t = (s - v0[0]) / dx;
			else
				t = (c - v0[1]) / dy;

			float z = v1[2];
			v1[0] = (float)s;
			v1[1] = (float)c;
			v1[2] = (float)(((double)z - v0[2]) * t + v0[2]);
			if (clipped1)
				*clipped1 = 1;
		}
	} else {
		float dy = (float)((double)v1[1] - v0[1]);
		float dx = (float)((double)v1[0] - v0[0]);
		double c = (double)v1[1] * v0[0] - (double)v0[1] * v1[0];
		double denominator = (double)clip->left * dy - dx;

		if (denominator != 0.0)
			c = c / (float)denominator;

		float s = (float)((double)clip->left * c);
		double t;

		if (fabs((double)dy) <= fabs((double)dx))
			t = ((double)s - v0[0]) / dx;
		else
			t = (c - v0[1]) / dy;

		v0[0] = s;
		v0[1] = (float)c;
		v0[2] = (float)(((double)v1[2] - v0[2]) * t + v0[2]);
		if (clipped0)
			*clipped0 = 1;
	}

	float rightLimit = (float)((double)clip->right * v1[1]);

	if (v0[0] > (double)clip->right * v0[1] && v1[0] > (double)rightLimit)
		return 0;

	if ((double)clip->right * v0[1] >= v0[0]) {
		if (v1[0] > (double)rightLimit) {
			float dy = (float)((double)v1[1] - v0[1]);
			float dx = (float)((double)v1[0] - v0[0]);
			double c = (double)v1[1] * v0[0] - (double)v0[1] * v1[0];
			double denominator = (double)clip->right * dy - dx;

			if (denominator != 0.0)
				c = c / (float)denominator;

			double s = (double)clip->right * c;
			double t;

			if (fabs((double)dy) <= fabs((double)dx))
				t = (s - v0[0]) / dx;
			else
				t = (c - v0[1]) / dy;

			float z = v1[2];
			v1[0] = (float)s;
			v1[1] = (float)c;
			v1[2] = (float)(((double)z - v0[2]) * t + v0[2]);
			if (clipped1)
				*clipped1 = 1;
		}
	} else {
		float dy = (float)((double)v1[1] - v0[1]);
		float dx = (float)((double)v1[0] - v0[0]);
		double c = (double)v1[1] * v0[0] - (double)v0[1] * v1[0];
		double denominator = (double)clip->right * dy - dx;

		if (denominator != 0.0)
			c = c / (float)denominator;

		float s = (float)((double)clip->right * c);
		double t;

		if (fabs((double)dy) <= fabs((double)dx))
			t = ((double)s - v0[0]) / dx;
		else
			t = (c - v0[1]) / dy;

		v0[0] = s;
		v0[1] = (float)c;
		v0[2] = (float)(((double)v1[2] - v0[2]) * t + v0[2]);
		if (clipped0)
			*clipped0 = 1;
	}

	float topLimit = (float)((double)clip->top * v1[1]);

	if (v0[2] > (double)clip->top * v0[1] && v1[2] > (double)topLimit)
		return 0;

	if ((double)clip->top * v0[1] >= v0[2]) {
		if (v1[2] > (double)topLimit) {
			float dy = (float)((double)v1[1] - v0[1]);
			float dz = (float)((double)v1[2] - v0[2]);
			double denominator = (double)clip->top * dy - dz;
			double c = (double)v1[1] * v0[2] - (double)v0[1] * v1[2];

			if (denominator != 0.0)
				c = c / (float)denominator;

			double s = (double)clip->top * c;
			double t;

			if (fabs((double)dy) <= fabs((double)dz))
				t = (s - v0[2]) / dz;
			else
				t = (c - v0[1]) / dy;

			double x = ((double)v1[0] - v0[0]) * t + v0[0];
			v1[1] = (float)c;
			v1[2] = (float)s;
			v1[0] = (float)x;
			if (clipped1)
				*clipped1 = 1;
		}
	} else {
		float dy = (float)((double)v1[1] - v0[1]);
		float dz = (float)((double)v1[2] - v0[2]);
		double denominator = (double)clip->top * dy - dz;
		double c = (double)v1[1] * v0[2] - (double)v0[1] * v1[2];

		if (denominator != 0.0)
			c = c / (float)denominator;

		double s = (double)clip->top * c;
		double t;

		if (fabs((double)dy) <= fabs((double)dz))
			t = (s - v0[2]) / dz;
		else
			t = (c - v0[1]) / dy;

		double x = ((double)v1[0] - v0[0]) * t;
		v0[1] = (float)c;
		x = x + v0[0];
		v0[2] = (float)s;
		v0[0] = (float)x;
		if (clipped0)
			*clipped0 = 1;
	}

	float bottomLimit = (float)((double)clip->bottom * v1[1]);

	if (v0[2] < (double)clip->bottom * v0[1] && v1[2] < (double)bottomLimit)
		return 0;

	if ((double)clip->bottom * v0[1] <= v0[2]) {
		if (v1[2] < (double)bottomLimit) {
			float dy = (float)((double)v1[1] - v0[1]);
			float dz = (float)((double)v1[2] - v0[2]);
			double denominator = (double)clip->bottom * dy - dz;
			double c = (double)v1[1] * v0[2] - (double)v0[1] * v1[2];

			if (denominator != 0.0)
				c = c / (float)denominator;

			double s = (double)clip->bottom * c;
			double t;

			if (fabs((double)dy) <= fabs((double)dz))
				t = (s - v0[2]) / dz;
			else
				t = (c - v0[1]) / dy;

			double x = ((double)v1[0] - v0[0]) * t + v0[0];
			v1[1] = (float)c;
			v1[2] = (float)s;
			v1[0] = (float)x;
			if (clipped1)
				*clipped1 = 1;
		}
	} else {
		float dy = (float)((double)v1[1] - v0[1]);
		float dz = (float)((double)v1[2] - v0[2]);
		double denominator = (double)clip->bottom * dy - dz;
		double c = (double)v1[1] * v0[2] - (double)v0[1] * v1[2];

		if (denominator != 0.0)
			c = c / (float)denominator;

		double s = (double)clip->bottom * c;
		double t;

		if (fabs((double)dy) <= fabs((double)dz))
			t = (s - v0[2]) / dz;
		else
			t = (c - v0[1]) / dy;

		double x = ((double)v1[0] - v0[0]) * t;
		v0[1] = (float)c;
		x = x + v0[0];
		v0[2] = (float)s;
		v0[0] = (float)x;
		if (clipped0)
			*clipped0 = 1;
	}

	return 1;
}

int RenderDroid::ClipModule::sphereInFrustrum(RdClipFrustum *clip, RdVector3 center, float radius) {
	RdCamera *camera = _rd->_camera.getCurrent();
	float farEdge = (float)((double)radius + center[1]);
	float nearEdge = (float)((double)center[1] - radius);

	if (farEdge < (double)clip->zNear)
		return 2;

	int inside = nearEdge >= (double)clip->zNear;

	if (clip->clipFar) {
		if (nearEdge > (double)clip->zFar)
			return 2;

		if (farEdge > (double)clip->zFar)
			inside = 0;
	}

	int projectType = camera->projectType;
	float topEdge = (float)((double)radius + center[2]);
	float bottomEdge = (float)((double)center[2] - radius);
	double limitNear;
	float limitFar;

	if (projectType == 1) {
		limitNear = (double)clip->top * nearEdge;
		limitFar = (float)((double)clip->top * farEdge);
	} else {
		limitNear = clip->orthoTop;
		limitFar = clip->orthoTop;
	}

	if (bottomEdge > limitNear && bottomEdge > (double)limitFar)
		return 2;

	if (topEdge > limitNear || topEdge > (double)limitFar)
		inside = 0;

	if (projectType == 1) {
		limitNear = (double)clip->bottom * nearEdge;
		limitFar = (float)((double)clip->bottom * farEdge);
	} else {
		limitNear = clip->orthoBottom;
		limitFar = clip->orthoBottom;
	}

	if (topEdge < limitNear && topEdge < (double)limitFar)
		return 2;

	if (bottomEdge < limitNear || bottomEdge < (double)limitFar)
		inside = 0;

	float rightEdge = (float)((double)radius + center[0]);
	float leftEdge = (float)((double)center[0] - radius);

	if (projectType == 1) {
		limitNear = (double)clip->left * nearEdge;
		limitFar = (float)((double)clip->left * farEdge);
	} else {
		limitNear = clip->orthoLeft;
		limitFar = clip->orthoLeft;
	}

	if (rightEdge < limitNear && rightEdge < (double)limitFar)
		return 2;

	if (leftEdge < limitNear || leftEdge < (double)limitFar)
		inside = 0;

	if (projectType == 1) {
		limitNear = (double)clip->right * nearEdge;
		limitFar = (float)((double)clip->right * farEdge);
	} else {
		limitNear = clip->orthoRight;
		limitFar = clip->orthoRight;
	}

	if (leftEdge > limitNear && leftEdge > (double)limitFar)
		return 2;

	if (rightEdge > limitNear || rightEdge > (double)limitFar)
		inside = 0;

	return !inside;
}

int RenderDroid::ClipModule::face3W(RdClipFrustum *clip, RdVector3 *verts, int numVerts) {
	return clipFace(clip, verts, nullptr, nullptr, numVerts, false, false);
}

int RenderDroid::ClipModule::face3WOrtho(RdClipFrustum *clip, RdVector3 *verts, int numVerts) {
	return clipFace(clip, verts, nullptr, nullptr, numVerts, false, true);
}

int RenderDroid::ClipModule::face3S(RdClipFrustum *clip, RdVector3 *verts, int numVerts) {
	return clipFace(clip, verts, nullptr, nullptr, numVerts, true, false);
}

int RenderDroid::ClipModule::face3SOrtho(RdClipFrustum *clip, RdVector3 *verts, int numVerts) {
	return clipFace(clip, verts, nullptr, nullptr, numVerts, true, true);
}

int RenderDroid::ClipModule::face3GS(RdClipFrustum *clip, RdVector3 *verts, float *intensities, int numVerts) {
	return clipFace(clip, verts, nullptr, intensities, numVerts, true, false);
}

int RenderDroid::ClipModule::face3GSOrtho(RdClipFrustum *clip, RdVector3 *verts, float *intensities, int numVerts) {
	return clipFace(clip, verts, nullptr, intensities, numVerts, true, true);
}

int RenderDroid::ClipModule::face3T(RdClipFrustum *clip, RdVector3 *verts, RdVector2 *texVerts, int numVerts) {
	return clipFace(clip, verts, texVerts, nullptr, numVerts, true, false);
}

int RenderDroid::ClipModule::face3TOrtho(RdClipFrustum *clip, RdVector3 *verts, RdVector2 *texVerts, int numVerts) {
	return clipFace(clip, verts, texVerts, nullptr, numVerts, true, true);
}

int RenderDroid::ClipModule::face3GT(RdClipFrustum *clip, RdVector3 *verts, RdVector2 *texVerts, float *intensities, int numVerts) {
	return clipFace(clip, verts, texVerts, intensities, numVerts, true, false);
}

int RenderDroid::ClipModule::face3GTOrtho(RdClipFrustum *clip, RdVector3 *verts, RdVector2 *texVerts, float *intensities, int numVerts) {
	return clipFace(clip, verts, texVerts, intensities, numVerts, true, true);
}

int RenderDroid::ClipModule::clipFace(RdClipFrustum *clip, RdVector3 *verts, RdVector2 *texVerts, float *intensities, int numVerts, bool guard, bool ortho) {
	RdClipPoly caller;
	RdClipPoly work;
	RdClipPoly *src = &caller;
	RdClipPoly *dst = &work;
	RdClipPoly *temp;
	int count;

	_faceStatus = 0;

	if (numVerts > 32) {
		warning("RenderDroid::ClipModule::clipFace(): too many vertices (%d)", numVerts);
		return 0;
	}

	caller.verts = verts;
	caller.texVerts = texVerts;
	caller.intensities = intensities;
	work.verts = _vertBufferA;
	work.texVerts = texVerts ? _workTVerts : nullptr;
	work.intensities = intensities ? _workIVerts : nullptr;

	if (ortho)
		count = clipOrtho(src, numVerts, dst, 0, clip->orthoLeft, true, 0x10);
	else
		count = clipPerspectiveX(src, numVerts, dst, guard ? clip->guardLeft : clip->left, true, 0x10);

	if (count < 3)
		return count;

	temp = src;
	src = dst;
	dst = temp;

	if (ortho)
		count = clipOrtho(src, count, dst, 0, clip->orthoRight, false, 0x20);
	else
		count = clipPerspectiveX(src, count, dst, clip->right, false, 0x20);

	if (count < 3)
		return count;

	temp = src;
	src = dst;
	dst = temp;

	if (ortho)
		count = clipOrtho(src, count, dst, 2, clip->orthoTop, false, 4);
	else
		count = clipPerspectiveZ(src, count, dst, guard ? clip->guardTop : clip->top, false, 4);

	if (count < 3)
		return count;

	temp = src;
	src = dst;
	dst = temp;

	if (ortho)
		count = clipOrtho(src, count, dst, 2, clip->orthoBottom, true, 8);
	else
		count = clipPerspectiveZ(src, count, dst, clip->bottom, true, 8);

	if (count < 3)
		return count;

	temp = src;
	src = dst;
	dst = temp;

	count = clipDepth(src, count, dst, clip->zNear, true, 1);

	if (count < 3) {
		_faceStatus |= 0x40;
		return count;
	}

	if (clip->clipFar) {
		temp = src;
		src = dst;
		dst = temp;

		count = clipDepth(src, count, dst, clip->zFar, false, 2);

		if (count < 3)
			return count;
	}

	if (dst->verts != verts) {
		memcpy(verts, dst->verts, sizeof(RdVector3) * count);

		if (texVerts)
			memcpy(texVerts, dst->texVerts, sizeof(RdVector2) * count);

		if (intensities)
			memcpy(intensities, dst->intensities, sizeof(float) * count);
	}

	return count;
}

int RenderDroid::ClipModule::faceToPlane(RdVector3 norm, RdVector3 pt, RdVector3 *verts, RdVector2 *texVerts, float *intensities, int numVerts) {
	int outCount = 0;

	if (numVerts <= 0 || numVerts > 32) {
		warning("RenderDroid::ClipModule::faceToPlane(): invalid vertex count (%d)", numVerts);
		return 0;
	}

	const float *prev = verts[numVerts - 1];
	const float *prevTex = texVerts ? texVerts[numVerts - 1] : nullptr;
	float prevIntensity = intensities ? intensities[numVerts - 1] : 0.0f;
	float prevDist = (float)(((double)prev[2] - pt[2]) * norm[2] + ((double)prev[0] - pt[0]) * norm[0] + ((double)prev[1] - pt[1]) * norm[1]);

	for (int i = 0; i < numVerts; i++) {
		const float *cur = verts[i];
		const float *curTex = texVerts ? texVerts[i] : nullptr;
		float curIntensity = intensities ? intensities[i] : 0.0f;
		double dist = ((double)cur[2] - pt[2]) * norm[2] + ((double)cur[0] - pt[0]) * norm[0] + ((double)cur[1] - pt[1]) * norm[1];

		if (prevDist < 0.0f || dist < 0.0) {
			if (prevDist >= 0.0f || dist >= 0.0) {
				double t = -((double)prevDist / (dist - prevDist));

				if (!canAddVertex(outCount))
					return 0;

				_vertBufferA[outCount][0] = (float)(((double)cur[0] - prev[0]) * t + prev[0]);
				_vertBufferA[outCount][1] = (float)(((double)cur[1] - prev[1]) * t + prev[1]);
				_vertBufferA[outCount][2] = (float)(((double)cur[2] - prev[2]) * t + prev[2]);

				if (texVerts) {
					_workTVerts[outCount][0] = (float)(((double)curTex[0] - prevTex[0]) * t + prevTex[0]);
					_workTVerts[outCount][1] = (float)(((double)curTex[1] - prevTex[1]) * t + prevTex[1]);
				}

				if (intensities)
					_workIVerts[outCount] = (float)(((double)curIntensity - prevIntensity) * t + prevIntensity);

				outCount++;

				if (prevDist < 0.0f) {
					if (!canAddVertex(outCount))
						return 0;

					_vertBufferA[outCount][0] = cur[0];
					_vertBufferA[outCount][1] = cur[1];
					_vertBufferA[outCount][2] = cur[2];

					if (texVerts) {
						_workTVerts[outCount][0] = curTex[0];
						_workTVerts[outCount][1] = curTex[1];
					}

					if (intensities)
						_workIVerts[outCount] = curIntensity;

					outCount++;
				}
			}
		} else {
			if (!canAddVertex(outCount))
				return 0;

			_vertBufferA[outCount][0] = cur[0];
			_vertBufferA[outCount][1] = cur[1];
			_vertBufferA[outCount][2] = cur[2];

			if (texVerts) {
				_workTVerts[outCount][0] = curTex[0];
				_workTVerts[outCount][1] = curTex[1];
			}

			if (intensities)
				_workIVerts[outCount] = curIntensity;

			outCount++;
		}

		prev = cur;
		prevTex = curTex;
		prevIntensity = curIntensity;
		prevDist = (float)dist;
	}

	for (int i = 0; i < outCount; i++) {
		verts[i][0] = _vertBufferA[i][0];
		verts[i][1] = _vertBufferA[i][1];
		verts[i][2] = _vertBufferA[i][2];

		if (texVerts) {
			texVerts[i][0] = _workTVerts[i][0];
			texVerts[i][1] = _workTVerts[i][1];
		}

		if (intensities)
			intensities[i] = _workIVerts[i];
	}

	return outCount;
}

bool RenderDroid::ClipModule::canAddVertex(int count) {
	if (count >= 32) {
		warning("RenderDroid::ClipModule::canAddVertex(): clipped face has too many vertices");
		return false;
	}

	return true;
}

void RenderDroid::ClipModule::copyVertex(const RdClipPoly *src, int srcIdx, RdClipPoly *dst, int dstIdx) {
	dst->verts[dstIdx][0] = src->verts[srcIdx][0];
	dst->verts[dstIdx][1] = src->verts[srcIdx][1];
	dst->verts[dstIdx][2] = src->verts[srcIdx][2];

	if (src->texVerts) {
		dst->texVerts[dstIdx][0] = src->texVerts[srcIdx][0];
		dst->texVerts[dstIdx][1] = src->texVerts[srcIdx][1];
	}

	if (src->intensities)
		dst->intensities[dstIdx] = src->intensities[srcIdx];
}

void RenderDroid::ClipModule::interpolateAttributes(const RdClipPoly *src, int prevIdx, int curIdx, RdClipPoly *dst, int dstIdx, double t) {
	if (src->texVerts) {
		dst->texVerts[dstIdx][0] = (float)(((double)src->texVerts[curIdx][0] - src->texVerts[prevIdx][0]) * t + src->texVerts[prevIdx][0]);
		dst->texVerts[dstIdx][1] = (float)(((double)src->texVerts[curIdx][1] - src->texVerts[prevIdx][1]) * t + src->texVerts[prevIdx][1]);
	}

	if (src->intensities)
		dst->intensities[dstIdx] = (float)(((double)src->intensities[curIdx] - src->intensities[prevIdx]) * t + src->intensities[prevIdx]);
}

int RenderDroid::ClipModule::clipPerspectiveX(const RdClipPoly *src, int count, RdClipPoly *dst, float slope, bool keepGreater, uint status) {
	int outCount = 0;
	int prevIdx = count - 1;

	for (int curIdx = 0; curIdx < count; curIdx++) {
		const float *prev = src->verts[prevIdx];
		const float *cur = src->verts[curIdx];

		float prevBound = (float)((double)prev[1] * slope);
		double curBound = (double)cur[1] * slope;
		bool prevInside = keepGreater ? !(prev[0] < prevBound) : (prev[0] <= prevBound);
		bool curInside = keepGreater ? (curBound <= cur[0]) : (curBound >= cur[0]);

		if (prevInside || curInside) {
			bool prevOutside = keepGreater ? (prev[0] < (double)prevBound) : (prev[0] > (double)prevBound);
			bool curOutside = keepGreater ? (curBound > cur[0]) : (curBound < cur[0]);

			if (prev[0] != prevBound && curBound != cur[0] && (prevOutside || curOutside)) {
				float dy = (float)((double)cur[1] - prev[1]);
				float dx = (float)((double)cur[0] - prev[0]);
				double c = (double)cur[1] * prev[0] - (double)prev[1] * cur[0];
				double denominator = (double)slope * dy - dx;

				if (denominator != 0.0)
					c = c / (float)denominator;

				float x = (float)((double)slope * c);
				double t;

				if (fabs((double)dy) <= fabs((double)dx))
					t = ((double)x - prev[0]) / dx;
				else
					t = (c - prev[1]) / dy;

				if (!canAddVertex(outCount))
					return 0;

				dst->verts[outCount][0] = x;
				dst->verts[outCount][1] = (float)c;
				dst->verts[outCount][2] = (float)(((double)cur[2] - prev[2]) * t + prev[2]);
				interpolateAttributes(src, prevIdx, curIdx, dst, outCount, t);
				outCount++;
				_faceStatus |= status;
			}

			if (curInside) {
				if (!canAddVertex(outCount))
					return 0;

				copyVertex(src, curIdx, dst, outCount);
				outCount++;
			}
		}

		prevIdx = curIdx;
	}

	return outCount;
}

int RenderDroid::ClipModule::clipPerspectiveZ(const RdClipPoly *src, int count, RdClipPoly *dst, float slope, bool keepGreater, uint status) {
	int outCount = 0;
	int prevIdx = count - 1;

	for (int curIdx = 0; curIdx < count; curIdx++) {
		const float *prev = src->verts[prevIdx];
		const float *cur = src->verts[curIdx];

		float prevBound = (float)((double)slope * prev[1]);
		double curBound = (double)cur[1] * slope;
		bool prevInside = keepGreater ? !(prev[2] < prevBound) : (prev[2] <= prevBound);
		bool curInside = keepGreater ? (curBound <= cur[2]) : (curBound >= cur[2]);

		if (prevInside || curInside) {
			bool prevOutside = keepGreater ? (prev[2] < (double)prevBound) : (prev[2] > (double)prevBound);
			bool curOutside = keepGreater ? (curBound > cur[2]) : (curBound < cur[2]);

			if (prev[2] != prevBound && curBound != cur[2] && (prevOutside || curOutside)) {
				float dy = (float)((double)cur[1] - prev[1]);
				float dz = (float)((double)cur[2] - prev[2]);
				double denominator = (double)slope * dy - dz;
				double c = (double)cur[1] * prev[2] - (double)cur[2] * prev[1];

				if (denominator != 0.0)
					c = c / (float)denominator;

				double z = (double)slope * c;
				double t;

				// The versions with texture or intensities keep z as a float
				if (src->texVerts || src->intensities)
					z = (float)z;

				if (fabs((double)dy) <= fabs((double)dz))
					t = (z - prev[2]) / dz;
				else
					t = (c - prev[1]) / dy;

				if (!canAddVertex(outCount))
					return 0;

				dst->verts[outCount][0] = (float)(((double)cur[0] - prev[0]) * t + prev[0]);
				dst->verts[outCount][1] = (float)c;
				dst->verts[outCount][2] = (float)z;
				interpolateAttributes(src, prevIdx, curIdx, dst, outCount, t);
				outCount++;
				_faceStatus |= status;
			}

			if (curInside) {
				if (!canAddVertex(outCount))
					return 0;

				copyVertex(src, curIdx, dst, outCount);
				outCount++;
			}
		}

		prevIdx = curIdx;
	}

	return outCount;
}

int RenderDroid::ClipModule::clipOrtho(const RdClipPoly *src, int count, RdClipPoly *dst, int axis, float bound, bool keepGreater, uint status) {
	int outCount = 0;
	int prevIdx = count - 1;

	for (int curIdx = 0; curIdx < count; curIdx++) {
		const float *prev = src->verts[prevIdx];
		const float *cur = src->verts[curIdx];

		bool prevInside = keepGreater ? !(prev[axis] < bound) : (prev[axis] <= bound);
		bool curInside = keepGreater ? (bound <= cur[axis]) : (bound >= cur[axis]);

		if (prevInside || curInside) {
			bool prevOutside = keepGreater ? (prev[axis] < (double)bound) : (prev[axis] > (double)bound);
			bool curOutside = keepGreater ? (bound > cur[axis]) : (bound < cur[axis]);

			if (prev[axis] != bound && bound != cur[axis] && (prevOutside || curOutside)) {
				double t = ((double)bound - prev[axis]) / ((double)cur[axis] - prev[axis]);

				if (!canAddVertex(outCount))
					return 0;

				for (int k = 0; k < 3; k++) {
					if (k == axis)
						dst->verts[outCount][k] = bound;
					else
						dst->verts[outCount][k] = (float)(((double)cur[k] - prev[k]) * t + prev[k]);
				}

				interpolateAttributes(src, prevIdx, curIdx, dst, outCount, t);
				outCount++;
				_faceStatus |= status;
			}

			if (curInside) {
				if (!canAddVertex(outCount))
					return 0;

				copyVertex(src, curIdx, dst, outCount);
				outCount++;
			}
		}

		prevIdx = curIdx;
	}

	return outCount;
}

int RenderDroid::ClipModule::clipDepth(const RdClipPoly *src, int count, RdClipPoly *dst, float plane, bool keepGreater, uint status) {
	int outCount = 0;
	int prevIdx = count - 1;

	for (int curIdx = 0; curIdx < count; curIdx++) {
		const float *prev = src->verts[prevIdx];
		const float *cur = src->verts[curIdx];

		bool prevInside = keepGreater ? (prev[1] >= (double)plane) : (prev[1] <= (double)plane);
		bool curInside = keepGreater ? (cur[1] >= (double)plane) : (cur[1] <= (double)plane);

		if (prevInside || curInside) {
			bool prevOutside = keepGreater ? (prev[1] < (double)plane) : (prev[1] > (double)plane);
			bool curOutside = keepGreater ? (cur[1] < (double)plane) : (cur[1] > (double)plane);

			if (prev[1] != plane && cur[1] != plane && (prevOutside || curOutside)) {
				double t = ((double)plane - prev[1]) / ((double)cur[1] - prev[1]);

				if (!canAddVertex(outCount))
					return 0;

				dst->verts[outCount][1] = plane;
				dst->verts[outCount][2] = (float)(((double)cur[2] - prev[2]) * t + prev[2]);
				dst->verts[outCount][0] = (float)(((double)cur[0] - prev[0]) * t + prev[0]);
				interpolateAttributes(src, prevIdx, curIdx, dst, outCount, t);
				outCount++;
				_faceStatus |= status;
			}

			if (curInside) {
				if (!canAddVertex(outCount))
					return 0;

				copyVertex(src, curIdx, dst, outCount);
				outCount++;
			}
		}

		prevIdx = curIdx;
	}

	return outCount;
}

} // End of namespace Grim
