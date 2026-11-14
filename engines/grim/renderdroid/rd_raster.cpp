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

#include "engines/grim/renderdroid/rd.h"

namespace Grim {

void RenderDroid::RasterModule::startup() {
	_oneOverNFlex[0] = 3.4e38f;
	_oneOverNFixed[0] = 0x7FFFFFFF;

	for (int i = 1; i < 2048; i++) {
		_oneOverNFlex[i] = (float)(1.0 / (double)i);
		_oneOverNFixed[i] = (int)(1.0 / (double)i * 65536.0);
	}

	for (int i = 0; i < 16; i++) {
		double product = (double)_oneOverNFlex[i] * _fixedScale;
		float value;

		if (product > std::numeric_limits<float>::max())
			value = std::numeric_limits<float>::infinity();
		else
			value = (float)product;

		if (i == 0) {
			if (!_rd->_remastered)
				memcpy(&_oneOverNFixed[2047], &value, sizeof(float));
		} else
			_oneOverNFixedF[i - 1] = value;
	}
}

} // End of namespace Grim
