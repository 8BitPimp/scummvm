/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef TITANIC_STAR_POINTS1_H
#define TITANIC_STAR_POINTS1_H

#include "common/array.h"
#include "titanic/star_control/surface_area.h"
#include "titanic/star_control/fvector.h"

namespace Titanic {

class CStarCamera;

class CStarPoints1 {
	struct CStarPointEntry : public FVector {
		bool _flag;
		CStarPointEntry() : FVector(), _flag(false) {}
	};
private:
	Common::Array<CStarPointEntry> _data;
public:
	CStarPoints1();

	/**
	 * Initialize the array
	 */
	bool initialize();

	/**
	 * Draw the starfield points
	 */
	void draw(CSurfaceArea *surface, CStarCamera *camera);
};

} // End of namespace Titanic

#endif /* TITANIC_STAR_POINTS1_H */
