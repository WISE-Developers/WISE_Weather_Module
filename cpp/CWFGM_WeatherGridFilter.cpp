/**
 * WISE_Weather_Module: CWFGM_WeatherGridFilter.h
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
#include "CWFGM_WeatherGridFilter.h"
#include "XYPoly.h"
#include "results.h"
#include "GridCom_ext.h"
#include "FireEngine_ext.h"
#include "WeatherCom_ext.h"
#include "CoordinateConverter.h"


/////////////////////////////////////////////////////////////////////////////
//

#ifndef DOXYGEN_IGNORE_CODE

CCWFGM_WeatherGridFilter::CCWFGM_WeatherGridFilter() :
    m_timeManager(nullptr),
    m_lStartTime(0ULL, m_timeManager),
    m_lEndTime(0ULL, m_timeManager)
{
	m_bRequiresSave = false;
	m_poly_ws_val = m_poly_wd_val = m_poly_temp_val = m_poly_rh_val = m_poly_precip_val = -1.0;
	m_poly_ws_op = m_poly_wd_op = m_poly_temp_op = m_poly_rh_op = m_poly_precip_op = (std::uint16_t)-1;
	m_resolution = -1.0;
	m_xllcorner = m_yllcorner = -999999999.0;
	m_flags = 0;
}


CCWFGM_WeatherGridFilter::CCWFGM_WeatherGridFilter(const CCWFGM_WeatherGridFilter &toCopy) :
	m_timeManager(toCopy.m_timeManager),
	m_lStartTime(0ULL, m_timeManager),
	m_lEndTime(0ULL, m_timeManager)
{
	CRWThreadSemaphoreEngage engage(*(CRWThreadSemaphore *)&toCopy.m_lock, SEM_FALSE);

	m_bRequiresSave = false;

	m_gisURL = toCopy.m_gisURL;
	m_gisLayer = toCopy.m_gisLayer;
	m_gisUID = toCopy.m_gisUID;
	m_gisPWD = toCopy.m_gisPWD;

	m_resolution = toCopy.m_resolution;
	m_resolution = -1.0;
	m_xllcorner = toCopy.m_xllcorner;
	m_yllcorner = toCopy.m_yllcorner;

	m_poly_wd_val = toCopy.m_poly_wd_val;
	m_poly_ws_val = toCopy.m_poly_ws_val;
	m_poly_temp_val = toCopy.m_poly_temp_val;
	m_poly_rh_val = toCopy.m_poly_rh_val;
	m_poly_precip_val = toCopy.m_poly_precip_val;

	m_poly_ws_op = toCopy.m_poly_ws_op;
	m_poly_wd_op = toCopy.m_poly_wd_op;
	m_poly_temp_op = toCopy.m_poly_temp_op;
	m_poly_rh_op = toCopy.m_poly_rh_op;
	m_poly_precip_op = toCopy.m_poly_precip_op;

	m_flags = toCopy.m_flags;

	m_lStartTime = toCopy.m_lStartTime; m_lStartTime.SetTimeManager(m_timeManager);
	m_lEndTime = toCopy.m_lEndTime; m_lEndTime.SetTimeManager(m_timeManager);

	XY_PolyLL *p = toCopy.m_polySet.LH_Head();
	while (p->LN_Succ()) {
		XY_PolyLL *pp = new XY_PolyLL(*p);
		m_polySet.AddPoly(pp);
		p = p->LN_Succ();
	}
}


CCWFGM_WeatherGridFilter::~CCWFGM_WeatherGridFilter() {
}

#endif


HRESULT CCWFGM_WeatherGridFilter::MT_Lock(Layer *layerThread, bool exclusive, std::uint16_t obtain) {
	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);
	if (!gridEngine)	{ weak_assert(false); return ERROR_GRID_UNINITIALIZED; }

	HRESULT hr;
	if (obtain == (std::uint16_t)-1) {
		std::int64_t state = m_lock.CurrentState();
		if (!state)				return SUCCESS_STATE_OBJECT_UNLOCKED;
		if (state < 0)			return SUCCESS_STATE_OBJECT_LOCKED_WRITE;
		if (state >= 1000000LL)	return SUCCESS_STATE_OBJECT_LOCKED_SCENARIO;
		return						   SUCCESS_STATE_OBJECT_LOCKED_READ;
	} else if (obtain) {
		if (exclusive)	m_lock.Lock_Write();
		else			m_lock.Lock_Read(1000000LL);

		m_calcLock.Lock_Write();
		m_polySet.RescanRanges(false, false);
		m_calcLock.Unlock();

		hr = gridEngine->MT_Lock(layerThread, exclusive, obtain);
	} else {
		hr = gridEngine->MT_Lock(layerThread, exclusive, obtain);

		if (exclusive)	m_lock.Unlock();
		else			m_lock.Unlock(1000000LL);
	}
	return hr;
}


HRESULT CCWFGM_WeatherGridFilter::GetEventTime(Layer *layerThread, const XY_Point& pt, std::uint32_t flags, const HSS_Time::WTime &from_time, HSS_Time::WTime *next_event, bool* event_valid) {
	WTime f_t(from_time, m_timeManager);

	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);
	if (!gridEngine)							{ weak_assert(false); return ERROR_GRID_UNINITIALIZED; }
	if (!next_event)							return E_POINTER;

	if (flags & (CWFGM_GETEVENTTIME_FLAG_SEARCH_SUNRISE | CWFGM_GETEVENTTIME_FLAG_SEARCH_SUNSET)) {
		return gridEngine->GetEventTime(layerThread, pt, flags, from_time, next_event, event_valid);
	}

	HRESULT hr = gridEngine->GetEventTime(layerThread, pt, flags, from_time, next_event, event_valid);
	if (SUCCEEDED(hr)) {
		if (!(flags & (CWFGM_GETEVENTTIME_QUERY_PRIMARY_WX_STREAM | CWFGM_GETEVENTTIME_QUERY_ANY_WX_STREAM))) { // only asking for weather station data
			const WTime ft(from_time, m_timeManager);
			WTime n_e(*next_event, m_timeManager);
			if (!(flags & CWFGM_GETEVENTTIME_FLAG_SEARCH_BACKWARD)) {	// searching forward
				if (ft < m_lStartTime) {
					if (n_e > m_lStartTime)
						n_e = m_lStartTime;
				} else if (ft < m_lEndTime) {
					if (n_e > (m_lEndTime + WTimeSpan(1)))
						n_e = m_lEndTime + WTimeSpan(1);
				}
			} else {							// searching backward
				if (ft > m_lEndTime) {
					if (n_e < (m_lEndTime + WTimeSpan(1)))
						n_e = m_lEndTime + WTimeSpan(1);
				} else if (ft > m_lStartTime) {
					if (n_e < m_lStartTime)
						n_e = m_lStartTime;
				}
			}
			next_event->SetTime(n_e);
		}
	}
	return hr;
}


HRESULT CCWFGM_WeatherGridFilter::GetWeatherData(Layer *layerThread, const XY_Point &pt, const HSS_Time::WTime &time, std::uint64_t interpolate_method,
    IWXData *wx, IFWIData *ifwi, DFWIData *dfwi, bool *wx_valid, XY_Rectangle *bbox_cache) {
	const WTime t(time, m_timeManager);

	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);
	if (!gridEngine)							{ weak_assert(false); return ERROR_GRID_UNINITIALIZED; }

	std::uint16_t x = convertX(pt.x, bbox_cache);
	std::uint16_t y = convertY(pt.y, bbox_cache);
	XY_Point p;
	p.x = invertX(((double)x) + 0.5);
	p.y = invertY(((double)y) + 0.5);
	return getWeatherData(gridEngine.get(), layerThread, p, t, interpolate_method, wx, ifwi, dfwi, wx_valid, bbox_cache);
}


HRESULT CCWFGM_WeatherGridFilter::GetWeatherDataArray(Layer *layerThread, const XY_Point &min_pt, const XY_Point &max_pt, double scale, const HSS_Time::WTime &time, std::uint64_t interpolate_method,
    IWXData_2d *wx, IFWIData_2d *ifwi, DFWIData_2d *dfwi, bool_2d *wx_valid) {
	const WTime t(time, m_timeManager);

	if (scale != m_resolution)							{ weak_assert(false); return ERROR_GRID_UNSUPPORTED_RESOLUTION; }

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
	if (!gridEngine)							{ weak_assert(false); return ERROR_GRID_UNINITIALIZED; }

	IWXData _iwx;
	IFWIData _ifwi;
	DFWIData _dfwi;
	bool _wxv;

	std::uint32_t i = 0;
	std::uint16_t x, y;
	XY_Point pt;
	HRESULT hr = S_OK;
	for (y = y_min; y <= y_max; y++) {			// for every point that was requested...
		for (x = x_min; x <= x_max; x++, i++) {
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

			if (wx)		wxvdata = &_wxv;
			else		wxvdata = nullptr;

			HRESULT hrr = getWeatherData(gridEngine.get(), layerThread, pt, t, interpolate_method, wxdata, ifwidata, dfwidata, wxvdata, nullptr);

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


#ifndef DOXYGEN_IGNORE_CODE

HRESULT CCWFGM_WeatherGridFilter::getWeatherData(ICWFGM_GridEngine *gridEngine, Layer * layerThread, const XY_Point &pt, const HSS_Time::WTime &time, std::uint64_t interpolate_method, IWXData *wx, IFWIData *ifwi, DFWIData *dfwi, bool *wx_valid, XY_Rectangle *bbox_cache) {
	HRESULT hr;

	if ((time >= m_lStartTime) && (time <= m_lEndTime))
	{										// if we are in the valid times for this filter, then let's see if the filter changes any data.
		IWXData c_wx;
		if (!wx)	wx = &c_wx;

		hr = gridEngine->GetWeatherData(layerThread, pt, time, interpolate_method, wx, nullptr, nullptr, wx_valid, bbox_cache);
		if (FAILED(hr))
			if (hr != E_NOTIMPL) {
				*wx_valid = false;
				return hr;
			}
		IWXData t_wx = *wx;
		bool t_wxv;

		if (GetWeatherInfoInPoly(pt, &t_wx, &t_wxv))
		{									// if it's in a polygon (and thus we're in a polygon mode)
			if (!memcmp(wx, &t_wx, sizeof(IWXData))) {
				*wx_valid = true;
				return hr;
			}
			*wx = t_wx;
			*wx_valid = t_wxv;
		}
		else if (m_polySet.NumPolys()) {					// we are a polygon but it wasn't inside the polygon
			*wx_valid = true;
			return hr;
		}
	} else {									// this is the default pass-through that is only called when this filter doesn't apply
		hr = gridEngine->GetWeatherData(layerThread, pt, time, interpolate_method, wx, nullptr, nullptr, wx_valid, bbox_cache);
		if ((time > m_lEndTime) && (time <= (m_lEndTime + WTimeSpan(53 * 24 * 60 * 60)))) {

    #ifdef _DEBUG
			weak_assert(floor(pt.x) == pt.x);
			weak_assert(floor(pt.y) == pt.y);
    #endif

			int32_t inArea;
			bool checked = false;
			if (m_polySet.NumPolys() == 1) {
				XY_PolyLL *p = m_polySet.LH_Head();
				if (p->NumPoints() == 5) {
					checked = true;
					XY_PolyNode *pp = p->LH_Head();
					while (pp->LN_Succ())  {
						if ((pp->x != -1.0) || (pp->y != -1.0)) {
							inArea = 0;
							break;
						}
						pp = pp->LN_Succ();
					}
					inArea = 2;
				}
			}
			if (!checked)
				inArea = m_polySet.PointInArea(XY_Point(pt.x + 0.5, pt.y + 0.5));
			if ((inArea) && (!(inArea & 1))) {
				if (m_poly_temp_op != (std::uint16_t)-1)
					wx->SpecifiedBits |= IWXDATA_OVERRODEHISTORY_TEMPERATURE | IWXDATA_OVERRODEHISTORY_DEWPOINTTEMPERATURE;
				if (m_poly_rh_op != (std::uint16_t)-1)
					wx->SpecifiedBits |= IWXDATA_OVERRODEHISTORY_RH | IWXDATA_OVERRODEHISTORY_DEWPOINTTEMPERATURE;
				if (m_poly_precip_op != (std::uint16_t)-1)
					wx->SpecifiedBits |= IWXDATA_OVERRODEHISTORY_PRECIPITATION;
				if (m_poly_wd_op != (std::uint16_t)-1)
					wx->SpecifiedBits |= IWXDATA_OVERRODEHISTORY_WINDDIRECTION;
				if (m_poly_ws_op != (std::uint16_t)-1)
					wx->SpecifiedBits |= IWXDATA_OVERRODEHISTORY_WINDSPEED;
			}
		}
	}

	return hr;
}

#endif


HRESULT CCWFGM_WeatherGridFilter::GetAttribute(Layer *layerThread, std::uint16_t option, PolymorphicAttribute *value) {
	if (!layerThread) {
		HRESULT hr = GetAttribute(option, value);
		if (SUCCEEDED(hr))
			return hr;
	}

	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);
	if (!gridEngine)							return ERROR_GRID_UNINITIALIZED;
	return gridEngine->GetAttribute(layerThread, option, value);
}


HRESULT CCWFGM_WeatherGridFilter::GetAttribute(std::uint16_t option, PolymorphicAttribute *value) {
	if (!value)								return E_POINTER;

	CRWThreadSemaphoreEngage engage(m_lock, SEM_FALSE);

	switch (option) {
		case CWFGM_WEATHER_OPTION_START_TIME:		*value = m_lStartTime;	return S_OK;
		case CWFGM_WEATHER_OPTION_END_TIME:			*value = m_lEndTime;	return S_OK;
		case CWFGM_ATTRIBUTE_LOAD_WARNING: {
								*value = m_loadWarning;
								return S_OK;
						   }
		case CWFGM_GRID_ATTRIBUTE_GIS_CANRESIZE:
							*value = (m_flags & CCWFGMGRID_ALLOW_GIS) ? true : false;
							return S_OK;

		case CWFGM_GRID_ATTRIBUTE_GIS_URL: {
							*value = m_gisURL;
							return S_OK;
						   }
		case CWFGM_GRID_ATTRIBUTE_GIS_LAYER: {
							*value = m_gisLayer;
							return S_OK;
						   }
		case CWFGM_GRID_ATTRIBUTE_GIS_UID: {
							*value = m_gisUID;
							return S_OK;
						   }
		case CWFGM_GRID_ATTRIBUTE_GIS_PWD: {
							*value = m_gisPWD;
							return S_OK;
						   }
	}
	return E_INVALIDARG;
}


HRESULT CCWFGM_WeatherGridFilter::SetAttribute(std::uint16_t option, const PolymorphicAttribute &var) {
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)								return ERROR_SCENARIO_SIMULATION_RUNNING;

	WTime ullvalue(m_timeManager);
	std::uint32_t old;
	bool bval;
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

		case CWFGM_GRID_ATTRIBUTE_GIS_CANRESIZE:
								try {
									bval = std::get<bool>(var);
								}
								catch (std::bad_variant_access&) {
									weak_assert(false);
									break;
								}
								old = m_flags;
								if (bval)
									m_flags |= CCWFGMGRID_ALLOW_GIS;
								else
									m_flags &= (~(CCWFGMGRID_ALLOW_GIS));
								if (old != m_flags)
									m_bRequiresSave = true;
								return S_OK;

		case CWFGM_GRID_ATTRIBUTE_GIS_URL: {
								std::string str;
								try {
									str = std::get<std::string>(var);
								}
								catch (std::bad_variant_access&) {
									weak_assert(false);
									break;
								}
								if (str.length()) {
									m_gisURL = str;
									m_bRequiresSave = true;
								}
								}
								return S_OK;

		case CWFGM_GRID_ATTRIBUTE_GIS_LAYER: {
								std::string str;
								try {
									str = std::get<std::string>(var);
								}
								catch (std::bad_variant_access&) {
									weak_assert(false);
									break;
								}
								if (str.length()) {
									m_gisLayer = str;
									m_bRequiresSave = true;
								}
								}
								return S_OK;

		case CWFGM_GRID_ATTRIBUTE_GIS_UID: {
								std::string str;
								try {
									str = std::get<std::string>(var);
								}
								catch (std::bad_variant_access&) {
									weak_assert(false);
									break;
								}
								m_gisUID = str;
								m_bRequiresSave = true;
								}
								return S_OK;

		case CWFGM_GRID_ATTRIBUTE_GIS_PWD: {
								std::string str;
								try {
									str = std::get<std::string>(var);
								}
								catch (std::bad_variant_access&) {
									weak_assert(false);
									break;
								}
								m_gisPWD = str;
								m_bRequiresSave = true;
								}
								return S_OK;
	}

	weak_assert(false);
	return hr;
}


HRESULT CCWFGM_WeatherGridFilter::Clone(boost::intrusive_ptr<ICWFGM_CommonBase> *newObject) const {
	if (!newObject)						return E_POINTER;

	CRWThreadSemaphoreEngage engage(*(CRWThreadSemaphore *)&m_lock, SEM_FALSE);

	try {
		CCWFGM_WeatherGridFilter *f = new CCWFGM_WeatherGridFilter(*this);
		*newObject = f;
		return S_OK;
	}
	catch (std::exception& e) {
	}
	return E_FAIL;
}


HRESULT CCWFGM_WeatherGridFilter::AddPolygon(const XY_PolyConst &xy_pairs, std::uint32_t *index) {
	if (!index)												return E_POINTER;

	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)										return ERROR_SCENARIO_SIMULATION_RUNNING;

	XY_PolyLL *poly = (XY_PolyLL *)m_polySet.NewCopy(xy_pairs);
	if (!poly)										return E_OUTOFMEMORY;

	poly->m_publicFlags |= XY_PolyLL::Flags::INTERPRET_POLYGON;
	poly->CleanPoly(0.0, XY_PolyLL::Flags::INTERPRET_POLYGON);

	if (poly->NumPoints() > 2) {
		m_polySet.AddPoly(poly);
		*index = m_polySet.NumPolys() - 1;
	} else {
		m_polySet.Delete(poly);
		return E_FAIL;
	}
	m_bRequiresSave = true;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::get_Temperature(double *pVal) {
	if (!pVal)										return E_POINTER;
	*pVal = m_poly_temp_val;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::put_Temperature(double newVal) {
	m_poly_temp_val = newVal;
	m_bRequiresSave = true;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::get_RH(double *pVal) {
	if (!pVal)										return E_POINTER;
	*pVal = m_poly_rh_val;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::put_RH(double newVal) {
	m_poly_rh_val = newVal;
	m_bRequiresSave = true;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::get_Precipitation(double *pVal) {
	if (!pVal)										return E_POINTER;
	*pVal = m_poly_precip_val;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::put_Precipitation(double newVal) {
	m_poly_precip_val = newVal;
	m_bRequiresSave = true;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::get_WindDirection(double *pVal) {
	if (!pVal)										return E_POINTER;
	if (m_poly_wd_val < 0.0)
		*pVal = m_poly_wd_val;
	else {
		if (m_poly_wd_op == 0)
			*pVal = NORMALIZE_ANGLE_DEGREE(CARTESIAN_TO_COMPASS_DEGREE(RADIAN_TO_DEGREE(m_poly_wd_val)));
		else if (m_poly_wd_op == (std::uint16_t)-1)
			*pVal = -1.0;
		else
			*pVal = NORMALIZE_ANGLE_DEGREE(RADIAN_TO_DEGREE(m_poly_wd_val));
	}
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::put_WindDirection(double newVal) {
	if (newVal < 0.0)
		m_poly_wd_val = -1.0;
	else {
		if (m_poly_wd_op == 0)
			m_poly_wd_val = DEGREE_TO_RADIAN(COMPASS_TO_CARTESIAN_DEGREE(newVal));
		else
			m_poly_wd_val = DEGREE_TO_RADIAN(newVal);
	}
	m_bRequiresSave = true;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::get_WindSpeed(double *pVal) {
	if (!pVal)										return E_POINTER;
	*pVal = m_poly_ws_val;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::put_WindSpeed(double newVal) {
	m_poly_ws_val = newVal;
	m_bRequiresSave = true;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::get_TemperatureOperation(std::uint16_t *pVal) {
	if (!pVal)										return E_POINTER;
	*pVal = m_poly_temp_op;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::put_TemperatureOperation(std::uint16_t newVal) {
	if ((newVal > 4) && (newVal != (std::uint16_t)-1))				return E_INVALIDARG;
	m_poly_temp_op = newVal;
	m_bRequiresSave = true;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::get_RHOperation(std::uint16_t *pVal) {
	if (!pVal)										return E_POINTER;
	*pVal = m_poly_rh_op;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::put_RHOperation(std::uint16_t newVal) {
	if ((newVal > 4) && (newVal != (std::uint16_t)-1))				return E_INVALIDARG;
	m_poly_rh_op = newVal;
	m_bRequiresSave = true;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::get_PrecipitationOperation(std::uint16_t *pVal) {
	if (!pVal)										return E_POINTER;
	*pVal = m_poly_precip_op;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::put_PrecipitationOperation(std::uint16_t newVal) {
	if ((newVal > 4) && (newVal != (std::uint16_t)-1))				return E_INVALIDARG;
	m_poly_precip_op = newVal;
	m_bRequiresSave = true;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::get_WindDirectionOperation(std::uint16_t *pVal) {
	if (!pVal)										return E_POINTER;
	*pVal = m_poly_wd_op;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::put_WindDirectionOperation(std::uint16_t newVal) {
	if ((newVal > 2) && (newVal != (std::uint16_t)-1))				return E_INVALIDARG;
	m_poly_wd_op = newVal;
	m_bRequiresSave = true;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::get_WindSpeedOperation(std::uint16_t *pVal) {
	if (!pVal)										return E_POINTER;
	*pVal = m_poly_ws_op;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::put_WindSpeedOperation(std::uint16_t newVal) {
	if ((newVal > 4) && (newVal != (std::uint16_t)-1))				return E_INVALIDARG;
	m_poly_ws_op = newVal;
	m_bRequiresSave = true;
	return S_OK;
}


#ifndef DOXYGEN_IGNORE_CODE

bool CCWFGM_WeatherGridFilter::GetWeatherInfoInPoly(const XY_Point &pt, IWXData *wx, bool *wx_valid) {
	int32_t inArea;
	if (m_landscape)
		inArea = 2;
	else
		inArea = m_polySet.PointInArea(XY_Point(pt.x, pt.y));
	bool calc_dew = false;
	if ((inArea) && (!(inArea & 1))) {
		switch (m_poly_temp_op) {
			case (std::uint16_t)-1:break;
			case 0:		wx->Temperature = m_poly_temp_val;			wx->SpecifiedBits |= IWXDATA_SPECIFIED_TEMPERATURE | IWXDATA_OVERRODE_TEMPERATURE;				calc_dew = true;	break;
			case 1:		wx->Temperature += m_poly_temp_val;			wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_TEMPERATURE)); wx->SpecifiedBits |= IWXDATA_OVERRODE_TEMPERATURE;	calc_dew = true;	break;
			case 2:		wx->Temperature -= m_poly_temp_val; 			wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_TEMPERATURE)); wx->SpecifiedBits |= IWXDATA_OVERRODE_TEMPERATURE;	calc_dew = true;	break;
			case 3:		wx->Temperature *= fabs(m_poly_temp_val);		wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_TEMPERATURE)); wx->SpecifiedBits |= IWXDATA_OVERRODE_TEMPERATURE;	calc_dew = true;	break;
			case 4:		if (m_poly_temp_val == 0.0) { wx->Temperature = 0.0;	wx->SpecifiedBits |= IWXDATA_SPECIFIED_TEMPERATURE | IWXDATA_OVERRODE_TEMPERATURE; }
					else	 { wx->Temperature /= fabs(m_poly_temp_val);	wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_TEMPERATURE)); wx->SpecifiedBits |= IWXDATA_OVERRODE_TEMPERATURE; }
					calc_dew = true;
					break;
		}

		switch (m_poly_rh_op) {
			case (std::uint16_t)-1:break;
			case 0:		wx->RH = m_poly_rh_val;					wx->SpecifiedBits |= IWXDATA_SPECIFIED_RH | IWXDATA_OVERRODE_RH;						calc_dew = true;	break;
			case 1:		wx->RH += m_poly_rh_val;				wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_RH)); wx->SpecifiedBits |= IWXDATA_OVERRODE_RH;			calc_dew = true;	break;
			case 2:		wx->RH -= m_poly_rh_val; 				wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_RH)); wx->SpecifiedBits |= IWXDATA_OVERRODE_RH;			calc_dew = true;	break;
			case 3:		wx->RH *= fabs(m_poly_rh_val);				wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_RH)); wx->SpecifiedBits |= IWXDATA_OVERRODE_RH;			calc_dew = true;	break;
			case 4:		if (m_poly_rh_val == 0.0) { wx->RH = 0.0;		wx->SpecifiedBits |= IWXDATA_SPECIFIED_TEMPERATURE | IWXDATA_OVERRODE_TEMPERATURE; }
					else	{ wx->RH /= fabs(m_poly_rh_val);		wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_TEMPERATURE)); wx->SpecifiedBits |= IWXDATA_OVERRODE_TEMPERATURE; }
					calc_dew = true;
					break;
		}

		switch (m_poly_precip_op) {
			case (std::uint16_t)-1:break;
			case 0:		wx->Precipitation = m_poly_precip_val;			wx->SpecifiedBits |= IWXDATA_SPECIFIED_PRECIPITATION | IWXDATA_OVERRODE_PRECIPITATION;				break;
			case 1:		wx->Precipitation += m_poly_precip_val;			wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_PRECIPITATION)); wx->SpecifiedBits |= IWXDATA_OVERRODE_PRECIPITATION;	break;
			case 2:		wx->Precipitation -= m_poly_precip_val; 		wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_PRECIPITATION)); wx->SpecifiedBits |= IWXDATA_OVERRODE_PRECIPITATION;	break;
			case 3:		wx->Precipitation *= fabs(m_poly_precip_val);		wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_PRECIPITATION)); wx->SpecifiedBits |= IWXDATA_OVERRODE_PRECIPITATION;	break;
			case 4:		if (m_poly_precip_val == 0.0) { wx->Precipitation = 0.0;wx->SpecifiedBits |= IWXDATA_SPECIFIED_PRECIPITATION | IWXDATA_OVERRODE_PRECIPITATION; }
					else	{ wx->Precipitation /= fabs(m_poly_precip_val);	wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_PRECIPITATION)); wx->SpecifiedBits |= IWXDATA_OVERRODE_PRECIPITATION; }	
					break;
		}

		switch (m_poly_ws_op) {
			case (std::uint16_t)-1:break;
			case 0:		wx->WindSpeed = m_poly_ws_val;				wx->SpecifiedBits |= IWXDATA_SPECIFIED_WINDSPEED | IWXDATA_OVERRODE_WINDSPEED;					break;
			case 1:		wx->WindSpeed += m_poly_ws_val;				wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_WINDSPEED)); wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDSPEED;		break;
			case 2:		wx->WindSpeed -= m_poly_ws_val; 			wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_WINDSPEED)); wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDSPEED;		break;
			case 3:		wx->WindSpeed *= fabs(m_poly_ws_val);			wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_WINDSPEED)); wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDSPEED;		break;
			case 4:		if (m_poly_ws_val == 0.0) { wx->WindSpeed = 0.0;	wx->SpecifiedBits |= IWXDATA_SPECIFIED_WINDSPEED | IWXDATA_OVERRODE_WINDSPEED; }
					else	{ wx->WindSpeed /= fabs(m_poly_ws_val);		wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_WINDSPEED)); wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDSPEED;	}
					break;
		}

		switch (m_poly_wd_op) {
			case (std::uint16_t)-1:break;
			case 0:		wx->WindDirection = NORMALIZE_ANGLE_RADIAN(m_poly_wd_val /*+ Pi*/);
					wx->SpecifiedBits |= IWXDATA_SPECIFIED_WINDDIRECTION | IWXDATA_OVERRODE_WINDDIRECTION;
					break;
			case 2:		wx->WindDirection += m_poly_wd_val; wx->WindDirection = NORMALIZE_ANGLE_RADIAN(wx->WindDirection);
					wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_WINDDIRECTION)); wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDDIRECTION;
					break;
			case 1:		wx->WindDirection -= m_poly_wd_val; wx->WindDirection = NORMALIZE_ANGLE_RADIAN(wx->WindDirection);
					wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_WINDDIRECTION)); wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDDIRECTION;
					break;
		}

		if (calc_dew) {
			double VPs = 0.6112 * pow(10.0, 7.5 * wx->Temperature / (237.7 + wx->Temperature));
			double VP = wx->RH * VPs;
			double dew;
			if (VP > 0.0)
				dew = 237.7 * log10(VP / 0.6112) / (7.5 - log10(VP / 0.6112));
			else	dew = -273.0;
			if (dew != wx->DewPointTemperature) {
				wx->DewPointTemperature = dew;
				wx->SpecifiedBits &= (~(IWXDATA_SPECIFIED_DEWPOINTTEMPERATURE)); wx->SpecifiedBits |= IWXDATA_OVERRODE_DEWPOINTTEMPERATURE;
			}
		}
		*wx_valid = true;
		return true;
	}
	*wx_valid = false;
	return false;
}

#endif


HRESULT CCWFGM_WeatherGridFilter::ClearPolygon(std::uint32_t index) {
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)								return ERROR_SCENARIO_SIMULATION_RUNNING;

	if (index == (std::uint32_t)-1) {
		XY_PolyLL *poly;
		while ((poly = m_polySet.RemHead()) != NULL)
			m_polySet.Delete(poly);
	} else {
		if (index >= m_polySet.NumPolys())					return ERROR_FIREBREAK_NOT_FOUND;

		XY_PolyLL *pn = (XY_PolyLL *)m_polySet.GetPoly(index);
		m_polySet.RemovePoly(pn);
		m_polySet.Delete(pn);
	}
	m_bRequiresSave = true;
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::GetPolygonRange(std::uint32_t index, XY_Point* min_pt, XY_Point* max_pt) {
	if ((!min_pt) || (!max_pt))					return E_POINTER;

	CRWThreadSemaphoreEngage engage(m_lock, SEM_FALSE);

	if (index >= m_polySet.NumPolys())							return ERROR_FIREBREAK_NOT_FOUND;

	XY_PolyLL *pn = (XY_PolyLL *)m_polySet.GetPoly(index);

	XY_Rectangle bbox;
	if (pn->BoundingBox(bbox)) {
		min_pt->x = bbox.m_min.x;
		min_pt->y = bbox.m_min.y;
		max_pt->x = bbox.m_max.x;
		max_pt->y = bbox.m_max.y;
		return S_OK;
	}
	return ERROR_NO_DATA | ERROR_SEVERITY_WARNING;
}


HRESULT CCWFGM_WeatherGridFilter::GetPolygon(std::uint32_t index, /*[in,out]*/std::uint32_t *size, /*[in,out,size_is(*size)]double*/XY_Poly *xy_pairs) {
	if ((!size) || (!xy_pairs))									return E_POINTER;

	CRWThreadSemaphoreEngage engage(m_lock, SEM_FALSE);

	if (index >= m_polySet.NumPolys())							return ERROR_FIREBREAK_NOT_FOUND;

	XY_PolyLL *pn = (XY_PolyLL *)m_polySet.GetPoly(index);

	XY_PolyNode *n;
	xy_pairs->SetNumPoints(pn->NumPoints());

	n = pn->LH_Head();
	std::uint32_t cnt = 0;
	while (n->LN_Succ()) {
		xy_pairs->SetPoint(cnt++, *n);
		n = n->LN_Succ();
	}
	*size = pn->NumPoints();
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::GetPolygonCount(/*[out, retval]*/std::uint32_t *count) {
	if (!count)								return E_POINTER;

	CRWThreadSemaphoreEngage engage(m_lock, SEM_FALSE);

	*count = m_polySet.NumPolys();
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::GetPolygonSize(std::uint32_t index, /*[out, retval]*/std::uint32_t *size) {
	if (!size)								return E_POINTER;
	*size = 0;

	CRWThreadSemaphoreEngage engage(m_lock, SEM_FALSE);

	if (index == (std::uint32_t)-1) {
		*size = 0;
		XY_PolyLL *pn = (XY_PolyLL *)m_polySet.LH_Head();
		while (pn->LN_Succ()) {
			if (pn->NumPoints() > *size)
				*size = pn->NumPoints();
			pn = pn->LN_Succ();
		}
	} else {
		if (index >= m_polySet.NumPolys())				return ERROR_FIREBREAK_NOT_FOUND;

		XY_PolyLL *pn = (XY_PolyLL *)m_polySet.GetPoly(index);
		weak_assert(pn);
		if (pn)
			*size = pn->NumPoints();
	}
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::GetArea(double *area) {
	if (!area)								return E_POINTER;

	CRWThreadSemaphoreEngage engage(m_lock, SEM_FALSE);

	*area = m_polySet.Area();
	return S_OK;
}


#ifndef DOXYGEN_IGNORE_CODE

HRESULT CCWFGM_WeatherGridFilter::PutGridEngine(Layer *layerThread, ICWFGM_GridEngine *newVal) {
	HRESULT hr = ICWFGM_GridEngine::PutGridEngine(layerThread, newVal);
	if (SUCCEEDED(hr) && m_gridEngine(nullptr)) {
		HRESULT hr = fixResolution();
		weak_assert(SUCCEEDED(hr));
	}
	return hr;
}


HRESULT CCWFGM_WeatherGridFilter::PutCommonData(Layer* layerThread, ICWFGM_CommonData* pVal) {
	if (!pVal)
		return E_POINTER;
	m_timeManager = pVal->m_timeManager;
	m_lStartTime.SetTimeManager(m_timeManager);
	m_lEndTime.SetTimeManager(m_timeManager);
	return S_OK;
}


HRESULT CCWFGM_WeatherGridFilter::fixResolution() {
	HRESULT hr;
	double gridResolution, gridXLL, gridYLL;
	PolymorphicAttribute var;

	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine;
	if (!(gridEngine = m_gridEngine(nullptr)))					{ weak_assert(false); return ERROR_GRID_UNINITIALIZED; }

	/*POLYMORPHIC CHECK*/
	try {
		if (!m_timeManager) {
			weak_assert(false);
			ICWFGM_CommonData* data;
			if (FAILED(hr = gridEngine->GetCommonData(nullptr, &data)) || (!data)) return hr;
			m_timeManager = data->m_timeManager;
		}
		if (FAILED(hr = gridEngine->GetAttribute(nullptr, CWFGM_GRID_ATTRIBUTE_PLOTRESOLUTION, &var))) return hr;
		gridResolution = std::get<double>(var);
		if (FAILED(hr = gridEngine->GetAttribute(nullptr, CWFGM_GRID_ATTRIBUTE_XLLCORNER, &var))) return hr;
		gridXLL = std::get<double>(var);
		if (FAILED(hr = gridEngine->GetAttribute(nullptr, CWFGM_GRID_ATTRIBUTE_YLLCORNER, &var))) return hr;
		gridYLL = std::get<double>(var);
	}
	catch (std::bad_variant_access&) {
		weak_assert(false);
		return ERROR_GRID_UNINITIALIZED;
	}

	m_resolution = gridResolution;
	m_xllcorner = gridXLL;
	m_yllcorner = gridYLL;

	m_polySet.SetCacheScale(m_resolution);
	return S_OK;
}

#endif


std::uint16_t CCWFGM_WeatherGridFilter::convertX(double x, XY_Rectangle* bbox) {
	double lx = x - m_xllcorner;
	double cx = floor(lx / m_resolution);
	if (bbox) {
		bbox->m_min.x = cx * m_resolution + m_xllcorner;
		bbox->m_max.x = bbox->m_min.x + m_resolution;
	}
	return (std::uint16_t)cx;
}


std::uint16_t CCWFGM_WeatherGridFilter::convertY(double y, XY_Rectangle* bbox) {
	double ly = y - m_yllcorner;
	double cy = floor(ly / m_resolution);
	if (bbox) {
		bbox->m_min.y = cy * m_resolution + m_yllcorner;
		bbox->m_max.y = bbox->m_min.y + m_resolution;
	}
	return (std::uint16_t)cy;
}
