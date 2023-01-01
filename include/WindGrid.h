/**
 * WISE_Weather_Module: WindGrid.h
 * Copyright (C) 2023  WISE
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <vector> // This is included for convenience; members of the Sector class are typically stored in a vector
#include <map>

#include "results.h"
#include "propsysreplacement.h"

#include "GridCom_ext.h"
#include "GridCOM.h"
#include "angles.h"

#define MINIMUM_SECTOR_ANGLE		1.0	// the minimum size (angle) of a sector

#define CWFGM_WINDGRID_BYINDEX		1	// sector specified by index
#define CWFGM_WINDGRID_BYANGLE		2	// sector specified by angle

HSS_PRAGMA_WARNING_PUSH
HSS_PRAGMA_GCC(GCC diagnostic ignored "-Wunused-variable")
static const char
    *directions_8[] = { _T("North"), _T("Northeast"), _T("East"), _T("Southeast"), _T("South"), _T("Southwest"), _T("West"), _T("Northwest") };
static const char
    *directions_8l[] = { _T("N"), _T("NE"), _T("E"), _T("SE"), _T("S"), _T("SW"), _T("W"), _T("NW") };
HSS_PRAGMA_WARNING_POP

class Sector
{
    public:
		double m_minAngle; // the minimum angle for this section
		double m_maxAngle; // the maximum angle for this section
		std::string m_label; // the name/label assigned to this section

		Sector(double mn, double mx, const std::string &l) : m_label(l)
		{
			this->m_minAngle = mn;
			this->m_maxAngle = mx;
		}
		virtual ~Sector() {}
		virtual void Cleanup() {}

		bool ContainsAngle(double angle)
		{
			if (!BETWEEN_ANGLES_DEGREE(angle, m_minAngle, m_maxAngle))
				return false;
			if (NORMALIZE_ANGLE_DEGREE(angle) == m_maxAngle)
				return false;	// can be >= start but < end
			if (NORMALIZE_ANGLE_DEGREE(angle) == m_minAngle)
				return false;	// can be >= start but < end - logic will dictate how to handle == properly
			return true;
		}

		bool Overlaps(const Sector &sector)
		{
			return Overlaps(sector.m_minAngle, sector.m_maxAngle);
		}

		bool Overlaps(double min_angle, double max_angle)
		{
			return ((ContainsAngle(min_angle)) || (ContainsAngle(max_angle)));
		}

		bool IsEmpty()
		{
			weak_assert(!(m_minAngle > m_maxAngle));
			return !(m_minAngle < m_maxAngle);
		}
};
