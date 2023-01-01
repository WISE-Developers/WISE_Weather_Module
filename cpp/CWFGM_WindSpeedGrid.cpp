/**
 * WISE_Weather_Module: CWFGM_WindSpeedGrid.h
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

#include "angles.h"
#include "WeatherCom_ext.h"
#include "FireEngine_ext.h"
#include "propsysreplacement.h"
#include "CWFGM_WeatherGridFilter.h"
#include "CWFGM_WindSpeedGrid.h"
#include "CoordinateConverter.h"


#ifndef DOXYGEN_IGNORE_CODE


CCWFGM_WindSpeedGrid::CCWFGM_WindSpeedGrid() : m_timeManager(nullptr),
		m_defaultSectorData(nullptr),
		m_defaultSectorDataValid(nullptr),
    m_lStartTime((std::uint64_t)0, m_timeManager),
    m_lEndTime((std::uint64_t)0, m_timeManager),
    m_startSpan(0, 0, 0, 0),
    m_endSpan(0, 23, 59, 59) {
	m_bRequiresSave = false;
	m_xsize = m_ysize = (std::uint16_t)-1;
	m_resolution = -1.0;
	m_xllcorner = m_yllcorner = -999999999.0;
	m_flags = 0;
}


CCWFGM_WindSpeedGrid::CCWFGM_WindSpeedGrid(const CCWFGM_WindSpeedGrid &toCopy) : m_timeManager(toCopy.m_timeManager),
	m_lStartTime((std::uint64_t)0, m_timeManager),
	m_lEndTime((std::uint64_t)0, m_timeManager),
	m_startSpan((std::int64_t)0),
	m_endSpan((std::int64_t)0) {
	CRWThreadSemaphoreEngage engage(*(CRWThreadSemaphore *)&toCopy.m_lock, SEM_FALSE);

	m_bRequiresSave = false;

	m_flags = toCopy.m_flags;
	m_xsize = toCopy.m_xsize;
	m_ysize = toCopy.m_ysize;
	m_resolution = toCopy.m_resolution;
	m_xllcorner = toCopy.m_xllcorner;
	m_yllcorner = toCopy.m_yllcorner;

	m_lStartTime = toCopy.m_lStartTime; m_lStartTime.SetTimeManager(m_timeManager);
	m_lEndTime = toCopy.m_lEndTime; m_lEndTime.SetTimeManager(m_timeManager);
	m_startSpan = toCopy.m_startSpan;
	m_endSpan = toCopy.m_endSpan;

	m_defaultSectorFilename = toCopy.m_defaultSectorFilename;
	if (toCopy.m_defaultSectorData) {
		m_defaultSectorData = new std::uint16_t[m_xsize * m_ysize];
		memcpy(m_defaultSectorData, toCopy.m_defaultSectorData, sizeof(std::uint16_t) * m_xsize * m_ysize);
	} else
		m_defaultSectorData = nullptr;

	if (toCopy.m_defaultSectorDataValid) {
		m_defaultSectorDataValid = new bool[m_xsize * m_ysize];
		memcpy(m_defaultSectorDataValid, toCopy.m_defaultSectorDataValid, sizeof(std::uint16_t) * m_xsize * m_ysize);
	} else
		m_defaultSectorDataValid = nullptr;

	for (std::vector<SpeedSector *>::const_iterator it = toCopy.m_sectors.begin(); it != toCopy.m_sectors.end(); it++) {
		SpeedSector *s = new SpeedSector(**it, m_xsize, m_ysize);
		m_sectors.push_back(s);
	}
}


SpeedSector::SpeedSector(const SpeedSector &toCopy, std::uint16_t xsize, std::uint16_t ysize) : Sector(toCopy.m_minAngle, toCopy.m_maxAngle, toCopy.m_label) {
	for (std::vector<speed_entry>::const_iterator it = toCopy.m_entries.begin(); it != toCopy.m_entries.end(); it++) {
		speed_entry s(*it, xsize, ysize);
		m_entries.push_back(s);
	}
}


speed_entry::speed_entry(const speed_entry &toCopy, std::uint16_t xsize, std::uint16_t ysize) {
	filename = toCopy.filename;
	m_speed = toCopy.m_speed;
	if (toCopy.m_data != nullptr) {
		m_data = new std::uint16_t[xsize * ysize];
		memcpy(m_data, toCopy.m_data, sizeof(std::uint16_t) * xsize * ysize);
	}
	if (toCopy.m_datavalid != nullptr) {
		std::uint64_t i, cnt = xsize * ysize;
		m_datavalid = new bool[cnt];
		for (i = 0; i < cnt; i++)
			m_datavalid[i] = toCopy.m_datavalid[i];
	}
}


CCWFGM_WindSpeedGrid::~CCWFGM_WindSpeedGrid()
{
	for (std::uint16_t i=0; i < m_sectors.size(); i++)
	{
		m_sectors[i]->Cleanup();
		delete m_sectors[i];
	}
	if (m_defaultSectorData)		delete [] m_defaultSectorData;
	if (m_defaultSectorDataValid)	delete [] m_defaultSectorDataValid;
}

#endif


HRESULT CCWFGM_WindSpeedGrid::Clone(boost::intrusive_ptr<ICWFGM_CommonBase> *newObject) const
{
	if (!newObject)							return E_POINTER;

	CRWThreadSemaphoreEngage engage(*(CRWThreadSemaphore *)&m_lock, SEM_FALSE);

	try {
		CCWFGM_WindSpeedGrid *f = new CCWFGM_WindSpeedGrid(*this);
		*newObject = f;
		return S_OK;
	}
	catch (std::exception& e) {
	}
	return E_FAIL;
}


HRESULT CCWFGM_WindSpeedGrid::ModifySectorSet(const std::vector<WeatherGridSetModifier> &set_modifiers)
{
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)
		return ERROR_SCENARIO_SIMULATION_RUNNING;
	std::uint32_t numChanges = set_modifiers.size();
	if (numChanges == 0)
		return S_OK;
	if (!m_sectors.size())
		return S_OK;
	std::vector<SpeedSector*> sectorsCopy(m_sectors.size());
	for (std::uint16_t i = 0; i < m_sectors.size(); i++)
		sectorsCopy[i] = m_sectors[i]->ShallowCopy();
	std::uint16_t index;
	std::uint16_t *data;
	bool *datavalid;
	std::string filename;
	std::uint16_t* modded = new std::uint16_t[m_sectors.size()]();
	for (std::uint32_t i = 0; i < numChanges; i++)
	{
		index = sectorsCopy[set_modifiers[i].original_sector]->GetSpeedIndex(set_modifiers[i].original_wind_speed);
		if (index == (std::uint16_t)-1)
			continue;
		struct speed_entry &e = sectorsCopy[set_modifiers[i].original_sector]->m_entries[index];
		data = e.m_data;
		datavalid = e.m_datavalid;
		filename = e.filename;
		e.m_data = nullptr;
		e.m_datavalid = nullptr;
		sectorsCopy[set_modifiers[i].original_sector]->RemoveIndex(index);
		sectorsCopy[set_modifiers[i].new_sector]->AddSpeed(set_modifiers[i].new_wind_speed, filename, data, datavalid);
		modded[set_modifiers[i].new_sector] = 1;
		modded[set_modifiers[i].original_sector] = 1;
	}
	bool valid = true;
	for (std::uint16_t i = 0; i < m_sectors.size(); i++)
	{
		if (modded[i] == 1 && !sectorsCopy[i]->IsValid())
		{
			valid = false;
			break;
		}
	}
	delete[] modded;
	HRESULT retVal = S_OK;
	if (valid)
	{
		for (std::uint16_t i = 0; i < m_sectors.size(); i++)
		{
			m_sectors[i]->CleanupFilenames();
			delete m_sectors[i];
		}
		m_sectors.clear();
		m_sectors = sectorsCopy;
		m_bRequiresSave = true;
	}
	else
	{
		for (std::uint16_t i = 0; i < sectorsCopy.size(); i++)
		{
			sectorsCopy[i]->CleanupFilenames();
			delete sectorsCopy[i];
		}
		sectorsCopy.clear();
		retVal = E_FAIL;
	}
	return retVal;
}


HRESULT CCWFGM_WindSpeedGrid::Remove(const std::uint16_t sector, const double speed)
{
	HRESULT hr = S_OK;

	if (sector == (std::uint16_t)-1) {
		m_defaultSectorFilename.clear();
		if (m_defaultSectorData)		{ delete [] m_defaultSectorData; m_defaultSectorData = nullptr; }
		if (m_defaultSectorDataValid)	{ delete [] m_defaultSectorDataValid; m_defaultSectorDataValid = nullptr; }
		m_bRequiresSave = true;
	} else if ((sector >= m_sectors.size())) {
		hr = ERROR_SECTOR_INVALID_INDEX;
	} else {
		std::uint16_t index = m_sectors[sector]->GetSpeedIndex(speed);
		if (index != (std::uint16_t)-1)
		{
			m_sectors[sector]->RemoveIndex(index);
			m_bRequiresSave = true;
		}
		else
		{
			hr = ERROR_SPEED_OUT_OF_RANGE;
		}
	}

	return hr;
}


HRESULT CCWFGM_WindSpeedGrid::GetAttribute(std::uint16_t option, PolymorphicAttribute *value)
{
	if (!value)						return E_POINTER;

	CRWThreadSemaphoreEngage engage(m_lock, SEM_FALSE);

	std::string empty;
	switch (option) {
		case CWFGM_WEATHER_OPTION_START_TIME:		*value = m_lStartTime; return S_OK;
		case CWFGM_WEATHER_OPTION_END_TIME:			*value = m_lEndTime; return S_OK;
		case CWFGM_WEATHER_OPTION_START_TIMESPAN:	*value = m_startSpan; return S_OK;
		case CWFGM_WEATHER_OPTION_END_TIMESPAN:		*value = m_endSpan; return S_OK;
		case CWFGM_WEATHER_GRID_APPLY_FILE_SECTORS:
		case CWFGM_WEATHER_GRID_APPLY_FILE_DEFAULT:	*value = (m_flags & (1 << (option - 10560))) ? true : false; return S_OK;
		case CWFGM_ATTRIBUTE_LOAD_WARNING:
			{
				*value = empty;
				return S_OK;
			}
	}
	return E_INVALIDARG;
}

HRESULT CCWFGM_WindSpeedGrid::SetAttribute(std::uint16_t option, const PolymorphicAttribute &var)
{
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)								return ERROR_SCENARIO_SIMULATION_RUNNING;

	WTime ullvalue(m_timeManager);
	WTimeSpan llvalue;
	bool	bvalue;

	HRESULT hr = E_INVALIDARG;

	switch (option) {
		case CWFGM_WEATHER_OPTION_START_TIME:
								if (FAILED(hr = VariantToTime_(var, &ullvalue)))		return hr;
								m_lStartTime = ullvalue;
								m_lStartTime.PurgeToSecond(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
								m_bRequiresSave = true;
								return S_OK;

		case CWFGM_WEATHER_OPTION_END_TIME:
								if (FAILED(hr = VariantToTime_(var, &ullvalue)))		return hr;
								m_lEndTime = ullvalue;
								m_lEndTime.PurgeToSecond(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
								m_bRequiresSave = true;
								return S_OK;

		case CWFGM_WEATHER_OPTION_START_TIMESPAN:
								if (FAILED(hr = VariantToTimeSpan_(var, &llvalue)))		return hr;

								m_startSpan = llvalue;
								m_bRequiresSave = true;
								return S_OK;

		case CWFGM_WEATHER_OPTION_END_TIMESPAN:
								if (FAILED(hr = VariantToTimeSpan_(var, &llvalue)))		return hr;

								m_endSpan = llvalue;
								m_bRequiresSave = true;
								return S_OK;

		case CWFGM_WEATHER_GRID_APPLY_FILE_SECTORS:
		case CWFGM_WEATHER_GRID_APPLY_FILE_DEFAULT:
								if (FAILED(hr = VariantToBoolean_(var, &bvalue)))		return hr;
								if (bvalue)	m_flags |= 1 << (option - 10560);
								else		m_flags &= (~(1 << (option - 10560)));
								m_bRequiresSave = true;
								return S_OK;
	}

	weak_assert(0);
	return hr;
}


HRESULT CCWFGM_WindSpeedGrid::GetCount(const std::uint16_t sector, std::uint16_t *count) {
	if (!count)						return E_POINTER;

	if (sector == (std::uint16_t)-1) {
		if (m_defaultSectorData)
			*count = 1;
		else
			*count = 0;
	} else if (sector >= m_sectors.size()) {
		*count = 0;
		return ERROR_SECTOR_INVALID_INDEX;
	} else {
		SpeedSector s = *m_sectors[sector];
		*count = (std::uint16_t)s.m_entries.size();
	}

	return S_OK;
}


HRESULT CCWFGM_WindSpeedGrid::GetWindSpeeds(const std::uint16_t sector, std::uint16_t *count, std::vector<double> *speed_array) {
	if ((!count) || (!speed_array))	return E_POINTER;

	if (sector >= m_sectors.size())
	{
		return ERROR_SECTOR_INVALID_INDEX;
	}
	else
	{
		SpeedSector s = *m_sectors[sector];
		*count = (std::uint16_t)s.m_entries.size();
		if (speed_array->size() < (*count))
			speed_array->resize(*count);
		for (std::uint16_t i = 0; i < *count; i++)
		{
			(*speed_array)[i] = s.m_entries[i].m_speed;
		}
	}
	
	return S_OK;
}


HRESULT CCWFGM_WindSpeedGrid::GetFilenames(const std::uint16_t sector, std::vector<std::string> *filenames)
{
	if (!filenames)					return E_POINTER;

	if (sector == (std::uint16_t)-1) {
		filenames->resize(1);
		(*filenames)[0] = m_defaultSectorFilename;
	} else {
		filenames->resize(m_sectors[sector]->m_entries.size());

		for (std::uint16_t i=0; i<m_sectors[sector]->m_entries.size(); i++)
		{
			(*filenames)[i] = m_sectors[sector]->m_entries[i].filename;
		}
	}
	return S_OK;
}

HRESULT CCWFGM_WindSpeedGrid::GetSectorCount(std::uint16_t *count)
{
	if (!count)					 return E_POINTER;
	*count = (std::uint16_t)m_sectors.size();
	return S_OK;
}

HRESULT CCWFGM_WindSpeedGrid::GetSectorAngles(const std::uint16_t sector, double *min_angle, double *max_angle)
{
	if ((!min_angle) || (!max_angle))		return E_POINTER;
	
	if (sector >= m_sectors.size())
	{
		return ERROR_SECTOR_INVALID_INDEX;
	}
	else
	{
		SpeedSector s = *m_sectors[sector];
		*min_angle = s.m_minAngle;
		*max_angle = s.m_maxAngle;
	}

	return S_OK;
}

HRESULT CCWFGM_WindSpeedGrid::AddSector(const std::string & sector_name, double *min_angle, double *max_angle, std::uint16_t *index)
{
	if ((!min_angle) || (!max_angle) || (!index))		return E_POINTER;

	std::vector<SpeedSector*>::iterator it = m_sectors.begin();
	for(; it < m_sectors.end(); it++)
	{
		if ((*it)->m_label == sector_name)
		{
			return ERROR_NAME_NOT_UNIQUE;
		}
	}

	*min_angle = NORMALIZE_ANGLE_DEGREE(*min_angle);
	*max_angle = NORMALIZE_ANGLE_DEGREE(*max_angle);

	if (EQUAL_ANGLES_APPROXIMATES_RADIAN(DEGREE_TO_RADIAN(*max_angle), DEGREE_TO_RADIAN(*min_angle), DEGREE_TO_RADIAN(MINIMUM_SECTOR_ANGLE)))
	{
		return ERROR_SECTOR_TOO_SMALL;
	}

	SpeedSector *s = new SpeedSector(*min_angle, *max_angle, sector_name);
	
	it = m_sectors.begin();
	for(; it < m_sectors.end(); it++)
	{
		if ((*it)->Overlaps(*s))
		{
			delete s;
			return ERROR_DATA_NOT_UNIQUE;
		}
	}

	m_sectors.push_back(s);
	*index = (std::uint16_t)(m_sectors.size() - 1);
	m_bRequiresSave = true;
	return S_OK;
}

HRESULT CCWFGM_WindSpeedGrid::RemoveSector(const std::uint16_t sector)
{
	HRESULT hr = S_OK;

	if (sector == (std::uint16_t)-1) {
		m_defaultSectorFilename.clear();
		if (m_defaultSectorData)		{ delete [] m_defaultSectorData; m_defaultSectorData = nullptr; }
		if (m_defaultSectorDataValid)	{ delete [] m_defaultSectorDataValid; m_defaultSectorDataValid = nullptr; }
		m_bRequiresSave = true;
	} else if (sector >= m_sectors.size()) {
		hr = ERROR_SECTOR_INVALID_INDEX;
	} else {
		m_sectors[sector]->Cleanup();
		m_sectors.erase(m_sectors.begin() + sector);
		m_bRequiresSave = true;
	}
	return hr;
}

/// Gets the details of a sector of the grid.
/// If option is CWFGM_WINDGRID_BYINDEX, this method populates angle[0], angle[1], and sector_name, with the minimum angle, maximum angle, and name assigned to this sector, respectively.
/// If option is CWFGM_WINDGRID_BYANGLE, this method populates sector and sector_name with the index of, and name assigned, to sector.
/// If option is any other value, no action is taken and the method return S_FALSE.
HRESULT CCWFGM_WindSpeedGrid::GetSector(std::uint16_t option, double *angle, std::uint16_t *sector, std::string *sector_name)
{
	if ((!angle) || (!sector) || (!sector_name))		return E_POINTER;

	HRESULT result = E_INVALIDARG;
	switch (option)
	{
		case CWFGM_WINDGRID_BYINDEX:
			{
				if (*sector < m_sectors.size())
				{
					angle[0] = m_sectors[*sector]->m_minAngle;
					angle[1] = m_sectors[*sector]->m_maxAngle;
					*sector_name = m_sectors[*sector]->m_label;
					result = S_OK;
				}
				else
				{
					result = ERROR_INVALID_INDEX;
				}
			}
		case CWFGM_WINDGRID_BYANGLE:
			{
				if ((*angle >= 0.0) && (*angle < 360.0))
				{
					for (std::uint16_t i=0; i<m_sectors.size(); i++)
					{
						if (m_sectors[i]->ContainsAngle(*angle))
						{
							*sector = i;
							*sector_name = m_sectors[i]->m_label;
							result = S_OK;
							break;
						}
					}
				}
				else
				{
					result = ERROR_INVALID_DATA;
				}
			}
		default:
			{
				weak_assert(false); // invalid option specified
			}
	}
	return result;
}


HRESULT CCWFGM_WindSpeedGrid::MT_Lock(Layer *layerThread, bool exclusive, std::uint16_t obtain) {
	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);
	if (!gridEngine)	{ weak_assert(0); return ERROR_GRID_UNINITIALIZED; }

	HRESULT hr = S_OK;
	if (obtain == (std::uint16_t)-1) {
		std::int64_t state = m_lock.CurrentState();
		if (!state)						return SUCCESS_STATE_OBJECT_UNLOCKED;
		if (state < 0)					return SUCCESS_STATE_OBJECT_LOCKED_WRITE;
		if (state >= 1000000LL)			return SUCCESS_STATE_OBJECT_LOCKED_SCENARIO;
		return								   SUCCESS_STATE_OBJECT_LOCKED_READ;
	} else if (obtain) {
		if (exclusive)	m_lock.Lock_Write();
		else			m_lock.Lock_Read(1000000LL);

		hr = gridEngine->MT_Lock(layerThread, exclusive, obtain);
	} else {
		hr = gridEngine->MT_Lock(layerThread, exclusive, obtain);

		if (exclusive)	m_lock.Unlock();
		else			m_lock.Unlock(1000000LL);
	}
	return hr;
}

HRESULT CCWFGM_WindSpeedGrid::Valid(Layer *layerThread, const HSS_Time::WTime &start_time, const HSS_Time::WTimeSpan &duration, std::uint32_t option, /*[in,out,size_is(24*60*60)]*/std::vector<uint16_t> *application_count)
{
	if (((option & (~(1 << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE)))) && (!application_count))					return E_POINTER;

	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);

	if (!gridEngine)							{ weak_assert(0); return ERROR_GRID_UNINITIALIZED; }
	HRESULT hr = gridEngine->Valid(layerThread, start_time, duration, option, application_count);

	if (!(option & (~(1 << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE)))) {

		if (SUCCEEDED(hr)) {
			if (!m_lStartTime.GetTotalSeconds())
				return ERROR_GRID_TIME_OUT_OF_RANGE;
			if (!m_lEndTime.GetTotalSeconds())
				return ERROR_GRID_TIME_OUT_OF_RANGE;
			if (m_startSpan >= m_endSpan)
				return ERROR_GRID_TIME_OUT_OF_RANGE;
		}
	} else if ((option & (~(1 << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE))) == CWFGM_WEATHER_WXGRID_WS_DIURNALTIMES) {
		if (application_count) {
			if (((std::int64_t)application_count->size()) <= duration.GetTotalSeconds()) {
				application_count->resize(duration.GetTotalSeconds() + 1);
			}
			for (int64_t i = 0; i < duration.GetTotalSeconds(); i++) {
				const HSS_Time::WTime time = start_time + WTimeSpan(i);
				if (((!m_lStartTime.GetTotalMicroSeconds()) && (!m_lEndTime.GetTotalMicroSeconds())) ||
					((time >= m_lStartTime) && (time <= m_lEndTime)))
				{										// if we are in the valid times for this filter, then let's see if the filter changes any data.
					WTime t(time);
					WTimeSpan tod = t.GetTimeOfDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
					if ((tod >= m_startSpan) && (tod <= m_endSpan)) {
						if ((m_flags & (1 << (CWFGM_WEATHER_GRID_APPLY_FILE_DEFAULT - 10560))) && (m_defaultSectorData)) {
							uint16_t cnt = (*application_count)[i];
							(*application_count)[i] = cnt + 1;
						}
						else if (m_flags & (1 << (CWFGM_WEATHER_GRID_APPLY_FILE_SECTORS - 10560)))
							(*application_count)[i] = (*application_count)[i] + 1;
					}
				}
			}
		}
		return S_OK;
	}
	return hr;
}

HRESULT CCWFGM_WindSpeedGrid::GetAttribute(Layer *layerThread, std::uint16_t option, PolymorphicAttribute *value)
{
	if (!layerThread) {
		HRESULT hr = GetAttribute(option, value);
		if (SUCCEEDED(hr))
			return hr;
	}

	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);
	if (!gridEngine)							{ weak_assert(0); return ERROR_GRID_UNINITIALIZED; }
	return gridEngine->GetAttribute(layerThread, option, value);
}

HRESULT CCWFGM_WindSpeedGrid::GetWeatherData(Layer *layerThread, const XY_Point &pt, const HSS_Time::WTime &time, std::uint64_t interpolate_method, IWXData *wx, IFWIData *ifwi, DFWIData *dfwi, bool *wx_valid, XY_Rectangle *bbox_cache)
{
	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);
	if (!gridEngine)							{ weak_assert(0); return ERROR_GRID_UNINITIALIZED; }

	std::uint16_t x = convertX(pt.x, bbox_cache);
	std::uint16_t y = convertY(pt.y, bbox_cache);
	XY_Point pt1;
	pt1.x = invertX(((double)x) + 0.5);
	pt1.y = invertY(((double)y) + 0.5);
;	return getWeatherData(gridEngine.get(), layerThread, pt1, time, interpolate_method, wx, ifwi, dfwi, wx_valid, bbox_cache);
}


HRESULT CCWFGM_WindSpeedGrid::GetWeatherDataArray(Layer *layerThread, const XY_Point &min_pt, const XY_Point &max_pt, double scale, const HSS_Time::WTime &time, std::uint64_t interpolate_method,
	IWXData_2d *wx, IFWIData_2d *ifwi, DFWIData_2d *dfwi, bool_2d *wx_valid) {

	if (scale != m_resolution) { weak_assert(0); return ERROR_GRID_UNSUPPORTED_RESOLUTION; }

	std::uint16_t x_min = convertX(min_pt.x, nullptr), y_min = convertY(min_pt.y, nullptr);
	std::uint16_t x_max = convertX(max_pt.x, nullptr), y_max = convertY(max_pt.y, nullptr);
	std::uint32_t xdim = x_max - x_min + 1;
	std::uint32_t ydim = y_max - y_min + 1;

	if (wx)
	{
		const IWXData_2d::size_type *dims = wx->shape();
		if ((dims[0] < xdim) || (dims[1] < ydim)) return E_INVALIDARG;
	}
	if (ifwi)
	{
		const IFWIData_2d::size_type *dims = ifwi->shape();
		if ((dims[0] < xdim) || (dims[1] < ydim)) return E_INVALIDARG;
	}
	if (dfwi)
	{
		const DFWIData_2d::size_type *dims = dfwi->shape();
		if ((dims[0] < xdim) || (dims[1] < ydim)) return E_INVALIDARG;
	}
	if (wx_valid)
	{
		const bool_2d::size_type *dims = wx_valid->shape();
		if ((dims[0] < xdim) || (dims[1] < ydim)) return E_INVALIDARG;
	}

	if (x_min > x_max)							return E_INVALIDARG;
	if (y_min > y_max)							return E_INVALIDARG;

	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);
	if (!gridEngine)							{ weak_assert(0); return ERROR_GRID_UNINITIALIZED; }
	
	IWXData _iwx;
	IFWIData _ifwi;
	DFWIData _dfwi;
	bool _wxv;

	std::uint32_t i = 0;
	XY_Point pt;
	std::uint16_t x, y;
	HRESULT hr = S_OK;
	std::uint32_t indY, indX;
	for (y = y_min, indY = 0; y <= y_max; y++, indY++) {			// for every point that was requested...
		for (x = x_min, indX = 0; x <= x_max; x++, i++, indX++) {
			pt.x = invertX(((double)x) + 0.5);
			pt.y = invertY(((double)y) + 0.5);
			IWXData *wxdata;
			IFWIData *ifwidata;
			DFWIData *dfwidata;
			bool *wxvdata;

			if (wx)		wxdata = &_iwx;
			else		wxdata = nullptr;

			if (ifwi)	ifwidata = &_ifwi;
			else		ifwidata = nullptr;

			if (dfwi)	dfwidata = &_dfwi;
			else		dfwidata = nullptr;

			if (wx_valid)	wxvdata = &_wxv;
			else			wxvdata = nullptr;

			HRESULT hrr = getWeatherData(gridEngine.get(), layerThread, pt, time, interpolate_method, wxdata, ifwidata, dfwidata, wxvdata, nullptr);

			if (SUCCEEDED(hrr)) {
				if (!i)
					hr = hrr;
				if (wxdata)
					(*wx)[x - x_min][y - y_min] = _iwx;
				if (ifwidata)
					(*ifwi)[x - x_min][y - y_min] = _ifwi;
				if (dfwidata)
					(*dfwi)[x - x_min][y - y_min] = _dfwi;
				if (wxvdata)
					(*wx_valid)[x - x_min][y - y_min] = _wxv;
			}
		}
	}

	return hr;
}


HRESULT CCWFGM_WindSpeedGrid::GetEventTime(Layer *layerThread, const XY_Point& pt, std::uint32_t flags, const HSS_Time::WTime &from_time, HSS_Time::WTime *next_event, bool *event_valid) {
	if (!next_event)							return E_POINTER;

	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);
	if (!gridEngine) { weak_assert(0); return ERROR_GRID_UNINITIALIZED; }

	if (flags & (CWFGM_GETEVENTTIME_FLAG_SEARCH_SUNRISE | CWFGM_GETEVENTTIME_FLAG_SEARCH_SUNSET)) {
		return gridEngine->GetEventTime(layerThread, pt, flags, from_time, next_event, event_valid);
	}

	HRESULT hr = gridEngine->GetEventTime(layerThread, pt, flags, from_time, next_event, event_valid);
	if (SUCCEEDED(hr) && (m_lStartTime.GetTime(0)) && (m_lEndTime.GetTime(0))) {
		if (!(flags & (CWFGM_GETEVENTTIME_QUERY_PRIMARY_WX_STREAM | CWFGM_GETEVENTTIME_QUERY_ANY_WX_STREAM))) { // only asking for weather station data
			const WTime ft(from_time, m_timeManager);
			WTime n_e(*next_event, m_timeManager);
			WTime *events[6], e0(m_timeManager), e1(m_timeManager), e2(m_timeManager), e3(m_timeManager), e4(m_timeManager), e5(m_timeManager);
			events[0] = &e0; events[1] = &e1; events[2] = &e2; events[3] = &e3; events[4] = &e4; events[5] = &e5;

			WTime day(from_time, m_timeManager), d2(m_timeManager);
			day.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
			d2 = day + m_startSpan;						*(events[0]) = d2;		// get events for the current day
			d2 = day + m_endSpan + WTimeSpan(1);		*(events[1]) = d2;
			day -= WTimeSpan(1, 0, 0, 0);									// get events for the prior day
			*(events[2]) = m_lStartTime;
			d2 = day + m_endSpan + WTimeSpan(1);		*(events[3]) = d2;
			day += WTimeSpan(2, 0, 0, 0);									// get events for the next day
			d2 = day + m_startSpan;						*(events[4]) = d2;
			*(events[5]) = m_lEndTime + WTimeSpan(1);

			for (std::uint16_t i = 0; i < 6; i++) {
				if ((*(events[i]) >= m_lStartTime) && (*(events[i]) <= (m_lEndTime + WTimeSpan(1)))) {
					if (!(flags & CWFGM_GETEVENTTIME_FLAG_SEARCH_BACKWARD)) {	// searching forward
						if (ft < *(events[i])) {
							if (n_e > *(events[i]))
								n_e = *(events[i]);
						}
					} else {							// searching backward
						if (ft > *(events[i])) {
							if (n_e < *(events[i]))
								n_e = *(events[i]);
						}
					}
				}
			}
			next_event->SetTime(n_e);
		}
	}
	return hr;
}


#ifndef DOXYGEN_IGNORE_CODE

HRESULT CCWFGM_WindSpeedGrid::getWeatherData(ICWFGM_GridEngine *gridEngine, Layer *layerThread, const XY_Point &pt, const HSS_Time::WTime &time, std::uint32_t interpolate_method, IWXData *wx, IFWIData *ifwi, DFWIData *dfwi, bool *wx_valid, XY_Rectangle *bbox_cache)
{
	HRESULT hr;

	hr = gridEngine->GetWeatherData(layerThread, pt, time, interpolate_method, wx, nullptr, nullptr, wx_valid, bbox_cache);
	if (FAILED(hr))
		if (hr != E_NOTIMPL)
			return hr;

	const WTime t(time, m_timeManager);
	if ((wx) && (!(interpolate_method & CWFGM_GETEVENTTIME_QUERY_PRIMARY_WX_STREAM))) {
		if (((!m_lStartTime.GetTotalMicroSeconds()) && (!m_lEndTime.GetTotalMicroSeconds())) ||
			((t >= m_lStartTime) && (t <= m_lEndTime)))
		{										// if we are in the valid times for this filter, then let's see if the filter changes any data.
			WTimeSpan tod = t.GetTimeOfDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
			std::uint16_t x = convertX(pt.x, bbox_cache);
			std::uint16_t y = convertY(pt.y, bbox_cache);
			if ((tod >= m_startSpan) && (tod <= m_endSpan)) {
				if ((m_flags & (1 << (CWFGM_WEATHER_GRID_APPLY_FILE_DEFAULT - 10560))) && (m_defaultSectorData)) {
					std::uint16_t *data = m_defaultSectorData;
					bool *valid = m_defaultSectorDataValid;
					if (valid[this->ArrayIndex(x, y)]) {
						wx->WindSpeed = ((double)data[this->ArrayIndex(x, y)]) / 10.0;
						wx->SpecifiedBits |= IWXDATA_SPECIFIED_WINDSPEED | IWXDATA_OVERRODE_WINDSPEED;
					}
				}
				if (m_flags & (1 << (CWFGM_WEATHER_GRID_APPLY_FILE_SECTORS - 10560))) {
					IWXData m_wx;
					gridEngine->GetWeatherData(layerThread, pt, time, interpolate_method | CWFGM_GETEVENTTIME_QUERY_PRIMARY_WX_STREAM, &m_wx, nullptr, nullptr, wx_valid, bbox_cache);
					double direction = CARTESIAN_TO_COMPASS_DEGREE(RADIAN_TO_DEGREE(m_wx.WindDirection));
					std::uint16_t count = (std::uint16_t)m_sectors.size();
					for (std::uint16_t i = 0; i < count; i++) {
						if (m_sectors[i]->ContainsAngle(direction)) {
							std::uint16_t index;
							if (m_sectors[i]->m_entries.size() == 0) {
							} else if (m_sectors[i]->m_entries.size() == 1) {
								index = 0;
								goto LOOKUP;
							} else if ((index = m_sectors[i]->GetSpeedIndex(m_wx.WindSpeed)) == (std::uint16_t)-1) {
								std::uint16_t lower = m_sectors[i]->GetLowerSpeedIndex(m_wx.WindSpeed);
								std::uint16_t higher = m_sectors[i]->GetHigherSpeedIndex(m_wx.WindSpeed);
								if ((lower == (std::uint16_t)-1) && (higher != (std::uint16_t)-1)) {
									index = higher;
									goto LOOKUP;
								} else if ((lower != (std::uint16_t)-1) && (higher == (std::uint16_t)-1)) {
									index = lower;
									goto LOOKUP;
								} else {
									weak_assert(lower != (std::uint16_t)-1);
									weak_assert(higher != (std::uint16_t)-1);
									double ws1, ws2;

									std::uint16_t *data = m_sectors[i]->m_entries[lower].m_data;
									bool *valid = m_sectors[i]->m_entries[lower].m_datavalid;
									if (valid[this->ArrayIndex(x, y)])
										ws1 = (((double)data[this->ArrayIndex(x, y)]) / 10.0);
									else	ws1 = -1.0;

									data = m_sectors[i]->m_entries[higher].m_data;
									valid = m_sectors[i]->m_entries[higher].m_datavalid;
									if (valid[this->ArrayIndex(x, y)])
										ws2 = (((double)data[this->ArrayIndex(x, y)]) / 10.0);
									else	ws2 = -1.0;
								
									if ((ws1 == -1.0) && (ws2 != -1.0)) {
										index = higher;
										goto LOOKUP;
									} else if ((ws1 != -1.0) && (ws2 == -1.0)) {
										index = lower;
										goto LOOKUP;
									} else if ((ws1 != -1.0) && (ws2 != -1.0)) {
										double ds1 = m_sectors[i]->m_entries[higher].m_speed - m_sectors[i]->m_entries[lower].m_speed;
										double ds2 = m_wx.WindSpeed - m_sectors[i]->m_entries[lower].m_speed;
										wx->WindSpeed = (ws2 - ws1) / ds1 * ds2 + m_sectors[i]->m_entries[lower].m_speed;
										wx->SpecifiedBits |= IWXDATA_SPECIFIED_WINDSPEED | IWXDATA_OVERRODE_WINDSPEED;
									}
								}
							} else {
								LOOKUP:
								if (m_sectors[i]->m_entries[index].m_data) {
									std::uint16_t *data = m_sectors[i]->m_entries[index].m_data;
									bool *valid = m_sectors[i]->m_entries[index].m_datavalid;
									if (valid[this->ArrayIndex(x, y)]) {
										double speed = m_sectors[i]->m_entries[index].m_speed;
										double scale;
										if (speed > 0.0)
											scale = m_wx.WindSpeed / speed;
										else	scale = 1.0;
										double in_speed = (((double)data[this->ArrayIndex(x, y)]) / 10.0);
										wx->WindSpeed = scale * in_speed;
										wx->SpecifiedBits |= IWXDATA_SPECIFIED_WINDSPEED | IWXDATA_OVERRODE_WINDSPEED;
									}
								} else {
									weak_assert(false); // this shouldn't be the case
								}
								break;
							}
						}
					}
				} else if ((t >= (m_lStartTime + m_startSpan)) && (t <= (m_lEndTime + WTimeSpan(53 * 24 * 60 * 60))))
					wx->SpecifiedBits |= IWXDATA_OVERRODEHISTORY_WINDSPEED;
			} else if ((!((!m_lStartTime.GetTime(0)) && (!m_lEndTime.GetTime(0)))) &&
				((t > m_lEndTime) && (t <= (m_lEndTime + WTimeSpan(53 * 24 * 60 * 60)))))
			{										// if we are in the valid times for this filter, then let's see if the filter changes any data.
				wx->SpecifiedBits |= IWXDATA_OVERRODEHISTORY_WINDSPEED;
			}
		}
	}
	return hr;
}


HRESULT CCWFGM_WindSpeedGrid::PutGridEngine(Layer *layerThread, ICWFGM_GridEngine *newVal) {
	HRESULT hr = ICWFGM_GridEngine::PutGridEngine(layerThread, newVal);
	if (SUCCEEDED(hr) && m_gridEngine(nullptr)) {
		HRESULT hr = fixResolution();
		weak_assert(SUCCEEDED(hr));
	}
	return hr;
}


HRESULT CCWFGM_WindSpeedGrid::PutCommonData(/* [in] */ Layer* layerThread, /* [in] */ ICWFGM_CommonData* pVal) {
	if (!pVal)
		return E_POINTER;
	m_timeManager = pVal->m_timeManager;
	m_lStartTime.SetTimeManager(m_timeManager);
	m_lEndTime.SetTimeManager(m_timeManager);
	return S_OK;
}


HRESULT CCWFGM_WindSpeedGrid::fixResolution() {
	HRESULT hr;
	double gridResolution, gridXLL, gridYLL, temp;
	PolymorphicAttribute var;

	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine;
	if (!(gridEngine = m_gridEngine(nullptr)))					{ weak_assert(0); return ERROR_GRID_UNINITIALIZED; }

	if (!m_timeManager) {
		weak_assert(0);
		ICWFGM_CommonData* data;
		if (FAILED(hr = gridEngine->GetCommonData(nullptr, &data)) || (!data)) return hr;
		m_timeManager = data->m_timeManager;
	}
	if (FAILED(hr = gridEngine->GetAttribute(nullptr, CWFGM_GRID_ATTRIBUTE_PLOTRESOLUTION, &var))) return hr; VariantToDouble_(var, &gridResolution);
	if (FAILED(hr = gridEngine->GetAttribute(nullptr, CWFGM_GRID_ATTRIBUTE_XLLCORNER, &var))) return hr; VariantToDouble_(var, &gridXLL);
	if (FAILED(hr = gridEngine->GetAttribute(nullptr, CWFGM_GRID_ATTRIBUTE_YLLCORNER, &var))) return hr; VariantToDouble_(var, &gridYLL);

	m_resolution = gridResolution;
	m_xllcorner = gridXLL;
	m_yllcorner = gridYLL;

	return S_OK;
}

#endif


std::uint16_t CCWFGM_WindSpeedGrid::convertX(double x, XY_Rectangle* bbox) {
	double lx = x - m_xllcorner;
	double cx = floor(lx / m_resolution);
	if (bbox) {
		bbox->m_min.x = cx * m_resolution + m_xllcorner;
		bbox->m_max.x = bbox->m_min.x + m_resolution;
	}
	return (std::uint16_t)cx;
}


std::uint16_t CCWFGM_WindSpeedGrid::convertY(double y, XY_Rectangle* bbox) {
	double ly = y - m_yllcorner;
	double cy = floor(ly / m_resolution);
	if (bbox) {
		bbox->m_min.y = cy * m_resolution + m_yllcorner;
		bbox->m_max.y = bbox->m_min.y + m_resolution;
	}
	return (std::uint16_t)cy;
}
