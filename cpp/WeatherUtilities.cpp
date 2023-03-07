/**
 * WISE_Weather_Module: WeatherUtilities.cpp
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

#include "WeatherCom_ext.h"
#include "WeatherUtilities.h"
#include "propsysreplacement.h"
#include "points.h"
#include "FireEngine_ext.h"
#include <stdio.h>


WeatherUtilities::WeatherUtilities(WTimeManager *tm) : m_cache(tm) {
	m_tm = tm;
}


void WeatherUtilities::AddCache(Layer *layerThread, std::uint16_t cacheIndex, std::uint16_t x, std::uint16_t y) {
	m_cache.Add(layerThread, cacheIndex, x, y);
}


void WeatherUtilities::RemoveCache(Layer *layerThread, std::uint16_t cacheIndex) {
	m_cache.Remove(layerThread, cacheIndex);
}


void WeatherUtilities::ClearCache(Layer *layerThread, std::uint16_t cacheIndex) {
	m_cache.Clear(layerThread, cacheIndex);
}


bool WeatherUtilities::CacheExists(Layer *layerThread, std::uint16_t cacheIndex) {
	return m_cache.Exists(layerThread, cacheIndex);
}


std::uint32_t WeatherUtilities::IncrementCache(Layer* layerThread, std::uint16_t cacheIndex) {
	return m_cache.Increment(layerThread, cacheIndex);
}


std::uint32_t WeatherUtilities::DecrementCache(Layer* layerThread, std::uint16_t cacheIndex) {
	return m_cache.Decrement(layerThread, cacheIndex);
}


void WeatherUtilities::SetEquilibriumLimit(Layer *layerThread, std::uint16_t cacheIndex, const HSS_Time::WTime &time) {
	m_cache.EquilibriumDepth(layerThread, cacheIndex, time);
}


void WeatherUtilities::PurgeOldCache(Layer *layerThread, std::uint16_t cacheIndex, const HSS_Time::WTime &time) {
	m_cache.PurgeOld(layerThread, cacheIndex, time);	
}


HRESULT WeatherUtilities::GetCalculatedValues(ICWFGM_GridEngine *grid, Layer *layerThread, const XY_Point &pt, WeatherKey &key, WeatherData &data) {
	HRESULT hr;
	const std::uint16_t alternate = (key.interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE)) ? 1 : 0;
	bool use_cache = (key.interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_IGNORE_CACHE) ? false : true);

	WTime time(key.time);
	if (!(key.interpolate_method & CWFGM_GETWEATHER_INTERPOLATE_TEMPORAL))
		time.PurgeToHour(WTIME_FORMAT_AS_LOCAL);

	// simply return results from the cache, if they happen to be there
	if ((use_cache) && (m_cache.Retrieve(alternate, &key, &data, m_tm))) {
		return data.hr;
	}

	if (FAILED(hr = GetRawWxValues(grid, layerThread, time, pt, key.interpolate_method, &data.wx, &data.wx_valid))) {
		weak_assert(false);
		return hr;
	}
	const std::uint32_t bitmask = IWXDATA_OVERRODE_TEMPERATURE | IWXDATA_OVERRODE_RH | IWXDATA_OVERRODE_PRECIPITATION | IWXDATA_OVERRODE_WINDSPEED |
			      IWXDATA_OVERRODEHISTORY_TEMPERATURE | IWXDATA_OVERRODEHISTORY_RH | IWXDATA_OVERRODEHISTORY_PRECIPITATION | IWXDATA_OVERRODEHISTORY_WINDSPEED;

	if ((!(key.interpolate_method & (CWFGM_GETWEATHER_INTERPOLATE_SPATIAL | CWFGM_GETWEATHER_INTERPOLATE_HISTORY))) && (!(data.wx.SpecifiedBits & bitmask))) {
													// if something got changed now, or in the past, AND we've been asked to calculate
													// where effects are cumulative, then we can't continue here and let it simply act
													// as if it's just 1 weather stream feeding data (because it isn't)

		if (FAILED(hr = GetRawDFWIValues(grid, layerThread, time, pt, key.interpolate_method, data.wx.SpecifiedBits, &data.dfwi, &data.wx_valid))) {
			weak_assert(false);
			return hr;
		}
		if (FAILED(hr = GetRawIFWIValues(grid, layerThread, time, pt, key.interpolate_method, data.wx.SpecifiedBits, &data.ifwi, &data.wx_valid))) {
			weak_assert(false);
			return hr;
		}
	} else {
		if (time <= m_cache.EquilibriumDepth(layerThread, alternate))
			hr = CWFGM_WEATHER_INITIAL_VALUES_ONLY;
		if (hr == CWFGM_WEATHER_INITIAL_VALUES_ONLY) {
			WTime yesterday(time);
			yesterday -= WTimeSpan(0, 12, 0, 0);
			yesterday.PurgeToDay(WTIME_FORMAT_AS_LOCAL);
			WTime todayStart(yesterday + WTimeSpan(0, 12, 0, 0));
			if (FAILED(hr = GetRawDFWIValues(grid, layerThread, time, pt, key.interpolate_method & (~(1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_HISTORY)), data.wx.SpecifiedBits, &data.dfwi, &data.wx_valid)))
			{
				weak_assert(false);
				return hr;
			}
			memset(&data.wx, 0, sizeof(IWXData));
			memset(&data.ifwi, 0, sizeof(IFWIData));
		} else {
			// use linearly interpolated lat/lon for the center of the specified grid cell - these values are used later in FWI calculations
			weak_assert(pt.x == m_converter.xllcorner() + m_converter.resolution() * (floor((pt.x - m_converter.xllcorner()) / m_converter.resolution()) + 0.5));
			weak_assert(pt.y == m_converter.yllcorner() + m_converter.resolution() * (floor((pt.y - m_converter.yllcorner()) / m_converter.resolution()) + 0.5));
			double lat = pt.y /*+ 0.5*/, lon = pt.x /*+ 0.5*/;
			if (!m_converter.SourceToLatlon(1, &lon, &lat, nullptr))
			{
				weak_assert(false);
				hr = ERROR_INVALID_DATA;
				return hr;
			}
			lon = DEGREE_TO_RADIAN(lon);
			lat = DEGREE_TO_RADIAN(lat);
			
			// gets yesterday's daily FWI starting codes, spatially interpolated to this location
			if (FAILED(hr = GetCalculatedDFWIValues(grid, layerThread, time, pt, lat, lon, key.interpolate_method, &data.wx, &data.dfwi))) {
				weak_assert(false);
				return hr;
			}

			// computes the instantaneous FWI codes for this location, using spatially interpolated weather and starting codes
			if (FAILED(hr = GetCalculatedIFWIValues(grid, layerThread, key.time, pt, lat, lon, key.interpolate_method, &data.wx, &data.ifwi))) {
				weak_assert(false);
				return hr;
			}
		}
	}
	// store the results in cache
	data.hr = hr;
	if (use_cache)
		m_cache.Store(alternate, &key, &data, m_tm);

	weak_assert(SUCCEEDED(hr));
	return hr;
}

HRESULT WeatherUtilities::GetCalculatedDFWIValues(ICWFGM_GridEngine *grid, Layer *layerThread, const HSS_Time::WTime &time, const XY_Point &pt, double lat, double lon, std::uint64_t interpolate_method, const IWXData *wx, DFWIData *t_dfwi, DFWIData *p_dfwi)
{
	HRESULT hr = S_OK;
	bool wx_valid;
	DFWIData dfwi2; // temp variables
	if (!t_dfwi) { weak_assert(false); return E_POINTER; }
	if (!p_dfwi) { p_dfwi = &dfwi2; }

	WTime yesterday(time);
	yesterday -= WTimeSpan(0, 12, 0, 0);
	yesterday.PurgeToDay(WTIME_FORMAT_AS_LOCAL);
	WTime todayStart = yesterday + WTimeSpan(0, 12, 0, 0);
	yesterday -= WTimeSpan(0, 12, 0, 0);

	if ((!(interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL))) || (!(interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_CALCFWI)))) {
		if (FAILED(hr = GetRawDFWIValues(grid, layerThread, todayStart, pt, interpolate_method, wx->SpecifiedBits, t_dfwi, &wx_valid)) || (!wx_valid))
		{
			weak_assert(false);
		}
		p_dfwi = NULL;
		return hr;
	}

	// get yesterday's spatially interpolated FWI starting codes for this location
	if (FAILED(hr = GetRawDFWIValues(grid, layerThread, yesterday, pt, interpolate_method, wx->SpecifiedBits, p_dfwi, &wx_valid)) || (!wx_valid))
	{
		// this happens whenever at least one weather stream has no daily codes specified for yesterday
		if (FAILED(hr = GetRawDFWIValues(grid, layerThread, todayStart, pt, interpolate_method & (~(1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_HISTORY)), wx->SpecifiedBits, t_dfwi, &wx_valid)) || (!wx_valid))
		{
			weak_assert(false);
		}
		p_dfwi = NULL;
		return hr;
	}
	
	// get todays spatially interpolated weather conditions (at the start of the FWI day)
	IWXData wx1, wx2;
	if (FAILED(hr = GetRawWxValues(grid, layerThread, todayStart, pt, interpolate_method, &wx1, &wx_valid)) || (!wx_valid))
	{
		weak_assert(false);
		return hr;
	}
	else if (hr == CWFGM_WEATHER_INITIAL_VALUES_ONLY)
	{
		// this happens when at least one weather stream is missing wx data from the start of the fwi day
		if (FAILED(hr = GetRawDFWIValues(grid, layerThread, todayStart, pt, interpolate_method & (~(1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_HISTORY)), wx1.SpecifiedBits, t_dfwi, &wx_valid)) || (!wx_valid))
		{
			weak_assert(false);
		}
		p_dfwi = NULL;
		return hr;
	}

	double rain = wx1.Precipitation;
	WTime loop(todayStart);
	for (loop -= WTimeSpan(0, 1, 0, 0); loop > yesterday; loop -= WTimeSpan(0, 1, 0, 0)) {
		if (SUCCEEDED(hr = GetRawWxValues(grid, layerThread, loop, pt, interpolate_method, &wx2, &wx_valid)) || (!wx_valid))
			rain += wx2.Precipitation;
		else
			break;				// ran out of weather data
	}

	std::uint16_t fwiMonth = (std::uint16_t)todayStart.GetMonth(WTIME_FORMAT_AS_LOCAL) - 1;

	double dd;
	// Compute new value for dFFMC, dDMC and dDC for time using the values found in wx and the p_dfwi, and place the result
	// in the return variable for todays daily FWI codes (t_dfwi).
	if (FAILED(hr = m_fwi.DailyFFMC_VanWagner(p_dfwi->dFFMC, rain, wx1.Temperature, wx1.RH, wx1.WindSpeed, &dd)))
	{
		weak_assert(false);
		return hr;
	}
	if (dd != t_dfwi->dFFMC) {
		t_dfwi->dFFMC = dd;
		t_dfwi->SpecifiedBits |= DFWIDATA_OVERRODE_FFMC;
	}

	if (FAILED(hr = m_fwi.ISI_FBP(t_dfwi->dFFMC, wx1.WindSpeed, 24 * 60 * 60, &dd)))
	{
		weak_assert(false);
		return hr;
	}
	if (dd != t_dfwi->dISI) {
		t_dfwi->dISI = dd;
		t_dfwi->SpecifiedBits |= DFWIDATA_OVERRODE_ISI;
	}

	if (FAILED(hr = m_fwi.DMC(p_dfwi->dDMC, rain, wx1.Temperature, lat, lon, fwiMonth, wx1.RH, &dd)))
	{
		weak_assert(false);
		return hr;
	}
	if (dd != t_dfwi->dDMC) {
		t_dfwi->dDMC = dd;
		t_dfwi->SpecifiedBits |= DFWIDATA_OVERRODE_DMC;
	}

	if (FAILED(hr = m_fwi.DC(p_dfwi->dDC, rain, wx1.Temperature, lat, lon, fwiMonth, &dd)))
	{
		weak_assert(false);
		return hr;
	}
	if (dd != t_dfwi->dDC) {
		t_dfwi->dDC = dd;
		t_dfwi->SpecifiedBits |= DFWIDATA_OVERRODE_DC;
	}

	// Compute dBUI from new values for dFFMC, dDMC and dDC
	if (FAILED(hr = m_fwi.BUI(t_dfwi->dDC, t_dfwi->dDMC, &dd)))
	{
		weak_assert(false);
		return hr;
	}
	if (dd != t_dfwi->dBUI) {
		t_dfwi->dBUI = dd;
		t_dfwi->SpecifiedBits |= DFWIDATA_OVERRODE_BUI;
	}

	if (FAILED(hr = m_fwi.FWI(t_dfwi->dISI, t_dfwi->dBUI, &dd)))
	{
		weak_assert(false);
		return hr;
	}
	if (dd != t_dfwi->dFWI) {
		t_dfwi->dFWI = dd;
		t_dfwi->SpecifiedBits |= DFWIDATA_OVERRODE_BUI;
	}

	weak_assert(hr == S_OK);
	return hr;
}


HRESULT WeatherUtilities::GetCalculatedIFWIValues(ICWFGM_GridEngine *gridEngine, Layer *layerThread, const HSS_Time::WTime &time, const XY_Point &pt, double lat, double lon, std::uint64_t interpolate_method, const IWXData *wx, IFWIData *ifwi)
{
	HRESULT hr = S_OK;
	bool wx_valid;
	if (!wx)	{ weak_assert(false); return E_POINTER; }
	if (!ifwi)	{ weak_assert(false); return E_POINTER; }

	IFWIData ofwi = *ifwi;
	// clear out any garbage data
	ifwi->FFMC = 0.0;
	ifwi->FWI = 0.0;
	ifwi->ISI = 0.0;
	ifwi->SpecifiedBits = 0;

	WTime ttime(time);
	WTime daytime(ttime);
	if (!(interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_TEMPORAL))) {
		daytime.PurgeToHour(WTIME_FORMAT_AS_LOCAL);
		ttime = daytime;
	}

	if ((!(interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL))) || (!(interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_CALCFWI)))) {
		if (FAILED(hr = GetRawIFWIValues(gridEngine, layerThread, ttime, pt, interpolate_method, wx->SpecifiedBits, ifwi, &wx_valid)) || (!wx_valid))
		{
			weak_assert(false);
		}
		if (!(wx->SpecifiedBits & IWXDATA_OVERRODE_WINDSPEED))	// if the windspeed hasn't changed, then we shouldn't have to recalculate ISI from FFMC and wind speed (GetRawIFWIValues interpolates ISI too)
			return hr;
	} else {
		WTimeSpan secs = daytime.GetTimeOfDay(WTIME_FORMAT_AS_LOCAL);

		PolymorphicAttribute iFFMC_Method;
		std::uint32_t uiFFMC_Method;
		bool lawson;
		gridEngine->GetAttribute(layerThread, CWFGM_WEATHER_OPTION_FFMC_LAWSON, &iFFMC_Method);
		VariantToBoolean_(iFFMC_Method, &lawson);
		if (lawson)	uiFFMC_Method = 3 << 16;
		else		uiFFMC_Method = 1 << 16;

		switch ((uiFFMC_Method >> 16) & 3)
		{
			case 2:
				{
					weak_assert(false);
					return ERROR_SEVERITY_WARNING;
				}
			case 3: // this means we're using Lawson contiguous - need to work with ld0 and ld-1
				{
					IWXData wh0, wh1;
					WTime lh0(ttime);
					lh0.PurgeToHour(WTIME_FORMAT_AS_LOCAL);
					WTime lh1(lh0);
					lh1 += WTimeSpan(60 * 60);
					if (FAILED(this->GetRawWxValues(gridEngine, layerThread, lh0, pt, interpolate_method, &wh0, &wx_valid)) || (!wx_valid))
						wh0.RH = wx->RH;
					if (FAILED(this->GetRawWxValues(gridEngine, layerThread, lh1, pt, interpolate_method, &wh1, &wx_valid)) || (!wx_valid))
						wh1.RH = wx->RH;

					WTime ld0(ttime);
					ld0.PurgeToDay(WTIME_FORMAT_AS_LOCAL);
					ld0 += WTimeSpan(0, 12, 0, 0);
					DFWIData p_dfwi, t_dfwi;
					// get yesterday's and today's spatially interpolated daily FWI starting codes
					if (FAILED(hr = this->GetCalculatedDFWIValues(gridEngine, layerThread, ld0, pt, lat, lon, interpolate_method, wx, &t_dfwi, &p_dfwi)))
					{									// RWB: ***** above line - parameters for FWI stuff needed reversed
						weak_assert(false);
						return hr;
					}
					// compute spatially interpolated instantaneous FWI values for the current time
					if (FAILED(hr = m_fwi.HourlyFFMC_Lawson_Contiguous(p_dfwi.dFFMC, t_dfwi.dFFMC, wx->Precipitation, wx->Temperature, wh0.RH, wx->RH, wh1.RH, wx->WindSpeed, (unsigned long)secs.GetTotalSeconds(), &ifwi->FFMC)))
					{
						weak_assert(false);
						return hr;
					}
					if (ifwi->FFMC != ofwi.FFMC)
						ifwi->SpecifiedBits |= IFWIDATA_OVERRODE_FFMC;
					break;
				}
			default: // this means we're using Van Wagner - need to work with most recent event time
				{
					WTime prev_time(ttime - WTimeSpan(0, 1, 0, 1));	// we know there will be an event between now and 1-hr-and-1-sec before now
					bool event_valid;
					if (FAILED(hr = gridEngine->GetEventTime(layerThread, pt, CWFGM_GETEVENTTIME_FLAG_SEARCH_BACKWARD | CWFGM_GETEVENTTIME_QUERY_ANY_WX_STREAM, ttime, &prev_time, &event_valid)))
					{
						weak_assert(false);
						return hr;
					}

					// get spatially interpolated instantaneous FWI values for the recent event time
					if (prev_time == (ttime - WTimeSpan(0, 1, 0, 1)))
					{
						// no previous event!
						if (FAILED(hr = GetRawIFWIValues(gridEngine, layerThread, ttime, pt, interpolate_method & (~(1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_HISTORY)), wx->SpecifiedBits, ifwi, &wx_valid)) || (!wx_valid))
						{
							weak_assert(false);
							return hr;
						}
					}
					else
					{
						weak_assert((ttime - prev_time) <= WTimeSpan(60 * 60));
						HSS_Time::WTimeSpan duration = ttime - prev_time;
						if (FAILED(hr = GetRawIFWIValues(gridEngine, layerThread, prev_time, pt, interpolate_method, wx->SpecifiedBits, ifwi, &wx_valid)) || (!wx_valid))
						{
							weak_assert(false);
							return hr;
						}

						// compute spatially interpolated instantaneous FWI values for the current time
						if (FAILED(hr = m_fwi.HourlyFFMC_VanWagner(ifwi->FFMC, wx->Precipitation, wx->Temperature, wx->RH, wx->WindSpeed, duration.GetTotalSeconds(), &ifwi->FFMC)))
						{
							weak_assert(false);
							return hr;
						}
						if (ifwi->FFMC != ofwi.FFMC)
							ifwi->SpecifiedBits |= IFWIDATA_OVERRODE_FFMC;
					}
				}
		}
	}
	// compute ISI using new FFMC
	WTime tt(ttime);
	tt.PurgeToHour(WTIME_FORMAT_AS_LOCAL);
	if (FAILED(hr = m_fwi.ISI_FBP(ifwi->FFMC, wx->WindSpeed, (ttime - tt).GetTotalSeconds(), &(ifwi->ISI))))
	{
		weak_assert(false);
		return hr;
	}
	if (ifwi->ISI != ofwi.ISI)
		ifwi->SpecifiedBits |= IFWIDATA_OVERRODE_ISI;

	// get todays daily FWI starting codes
	DFWIData t_dfwi = {0};
	if (FAILED(hr = GetCalculatedDFWIValues(gridEngine, layerThread, ttime, pt, lat, lon, interpolate_method, wx, &t_dfwi)))
	{
		weak_assert(false);
		return hr;
	}

	// compute FWI using new ISI
	if (FAILED(hr = m_fwi.FWI(ifwi->ISI, t_dfwi.dBUI, &(ifwi->FWI))))
	{
		weak_assert(false);
		return hr;
	}
	if (ifwi->FWI != ofwi.FWI)
		ifwi->SpecifiedBits |= IFWIDATA_OVERRODE_FWI;

	weak_assert(hr == S_OK);
	return hr;
}

