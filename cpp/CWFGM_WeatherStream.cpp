/**
 * WISE_Weather_Module: CWFGM_WeatherStream.cpp
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

#include "propsysreplacement.h"
#include "CWFGM_WeatherStream.h"
#include "WeatherCom_ext.h"
#include "GridCom_ext.h"
#include "results.h"
#include <errno.h>

#ifdef DEBUG
#include <assert.h>
#endif


CCWFGM_WeatherStream::CCWFGM_WeatherStream() {
	m_gridCount = 0;
	m_bRequiresSave = false;
}


CCWFGM_WeatherStream::CCWFGM_WeatherStream(const CCWFGM_WeatherStream &toCopy) {
	CRWThreadSemaphoreEngage engage(*(CRWThreadSemaphore *)&toCopy.m_lock, SEM_FALSE);

	m_weatherCondition = toCopy.m_weatherCondition;
	m_gridCount = 0;
	m_bRequiresSave = false;
}


CCWFGM_WeatherStream::~CCWFGM_WeatherStream() {
}


HRESULT CCWFGM_WeatherStream::get_WeatherStation(boost::intrusive_ptr<CCWFGM_WeatherStation> *pVal) {
	if (!pVal)								return E_POINTER;
	CRWThreadSemaphoreEngage _semaphore_engage(m_lock, SEM_FALSE);
	*pVal = m_weatherCondition.m_weatherStation;
	if (!m_weatherCondition.m_weatherStation)				return ERROR_WEATHER_STREAM_NOT_ASSIGNED;
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::put_WeatherStation(/*DWORD*/long key, CCWFGM_WeatherStation *newVal) {
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)								return ERROR_SCENARIO_SIMULATION_RUNNING;

	if (key == 0xfedcba98)							{ m_gridCount++; return S_OK; };
	if (key == 0x0f1e2d3c)							{ m_gridCount--; return S_OK; };
	if (key != 0x12345678)							return E_NOINTERFACE;
	if (newVal) {
		if (newVal == (CCWFGM_WeatherStation *)-1) {			// special flag to say we have to re-calc
			m_cache.Clear();
			m_weatherCondition.ClearConditions();
			return S_OK;
		}
	}
	if (m_gridCount)							return ERROR_WEATHER_STREAM_ALREADY_ASSIGNED;

	HRESULT retval;
	if (newVal) {
		boost::intrusive_ptr<CCWFGM_WeatherStation> pWeatherStation;
		pWeatherStation = dynamic_cast<CCWFGM_WeatherStation *>(newVal);
		if (pWeatherStation) {
			m_weatherCondition.m_weatherStation = pWeatherStation;
			retval = S_OK;
		}
		else
			retval = E_FAIL;
	}
	else {
		m_weatherCondition.m_weatherStation = newVal;
		retval = S_OK;
	}

	m_cache.Clear();
	m_weatherCondition.ClearConditions();
	return retval;
}


HRESULT CCWFGM_WeatherStream::put_CommonData(ICWFGM_CommonData* pVal) {
	if (!pVal)
		return E_POINTER;

	if (pVal->m_timeManager->m_worldLocation.m_timezoneInfo())
		m_weatherCondition.m_worldLocation.m_timezoneInfo(pVal->m_timeManager->m_worldLocation.m_timezoneInfo());
	else {
		m_weatherCondition.m_worldLocation.m_timezone(pVal->m_timeManager->m_worldLocation.m_timezone());
		m_weatherCondition.m_worldLocation.m_startDST(pVal->m_timeManager->m_worldLocation.m_startDST());
		m_weatherCondition.m_worldLocation.m_amtDST(pVal->m_timeManager->m_worldLocation.m_amtDST());
		m_weatherCondition.m_worldLocation.m_endDST(pVal->m_timeManager->m_worldLocation.m_endDST());
	}
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::MT_Lock(bool exclusive, std::uint16_t obtain) {
	HRESULT hr;
	if (obtain == (std::uint16_t)-1) {
		std::int64_t state = m_lock.CurrentState();
		if (!state)				return SUCCESS_STATE_OBJECT_UNLOCKED;
		if (state < 0)			return SUCCESS_STATE_OBJECT_LOCKED_WRITE;
		if (state >= 1000000LL)	return SUCCESS_STATE_OBJECT_LOCKED_SCENARIO;
		return						   SUCCESS_STATE_OBJECT_LOCKED_READ;
	} else if (obtain) {
		if (SUCCEEDED(hr = m_weatherCondition.m_weatherStation->MT_Lock(exclusive, obtain))) {
			if (exclusive)	m_lock.Lock_Write();
			else		m_lock.Lock_Read(1000000LL);

			m_mt_calc_lock.Lock_Write();
			m_weatherCondition.calculateValues();
			m_mt_calc_lock.Unlock();
		}
	} else {
		if (exclusive)	m_lock.Unlock();
		else			m_lock.Unlock(1000000LL);

		hr = m_weatherCondition.m_weatherStation->MT_Lock(exclusive, obtain);
	}
	return hr;
}


HRESULT CCWFGM_WeatherStream::Clone(boost::intrusive_ptr<ICWFGM_CommonBase> *newObject) const {
	if (!newObject)							return E_POINTER;

	CRWThreadSemaphoreEngage engage(*(CRWThreadSemaphore *)&m_lock, SEM_FALSE);

	try {
		CCWFGM_WeatherStream *f = new CCWFGM_WeatherStream(*this);
		*newObject = f;
		return S_OK;
	}
	catch (std::exception& e) {
	}
	return E_FAIL;
}


HRESULT CCWFGM_WeatherStream::GetAttribute(std::uint16_t option, PolymorphicAttribute *value) {
	if (!value)							return E_POINTER;

	CRWThreadSemaphoreEngage _semaphore_engage(m_lock, SEM_FALSE);

	switch (option) {
		case CWFGM_ATTRIBUTE_LOAD_WARNING: {
							*value = m_loadWarning;
							return S_OK;
						   }

		case CWFGM_WEATHER_OPTION_WARNONSUNRISE:	*value = (m_weatherCondition.WarnOnSunRiseSet() & NO_SUNRISE) ? true : false;	return S_OK;
		case CWFGM_WEATHER_OPTION_WARNONSUNSET:		*value = (m_weatherCondition.WarnOnSunRiseSet() & NO_SUNSET) ? true : false;	return S_OK;

		case CWFGM_WEATHER_OPTION_FFMC_VANWAGNER:	*value = ((m_weatherCondition.m_options & WeatherCondition::FFMC_MASK) == WeatherCondition::FFMC_VAN_WAGNER)	? true : false;	return S_OK;
		case CWFGM_WEATHER_OPTION_FFMC_LAWSON:		*value = ((m_weatherCondition.m_options & WeatherCondition::FFMC_MASK) == WeatherCondition::FFMC_LAWSON)		? true : false;	return S_OK;
		case CWFGM_WEATHER_OPTION_FWI_USE_SPECIFIED:*value = (m_weatherCondition.m_options & WeatherCondition::USER_SPECIFIED)	? true : false;	return S_OK;
		case CWFGM_WEATHER_OPTION_ORIGIN_FILE:		*value = (m_weatherCondition.m_options & WeatherCondition::FROM_FILE)		? true : false; return S_OK;
		case CWFGM_WEATHER_OPTION_ORIGIN_ENSEMBLE:	*value = (m_weatherCondition.m_options & WeatherCondition::FROM_ENSEMBLE)	? true : false; return S_OK;
		case CWFGM_WEATHER_OPTION_FWI_ANY_SPECIFIED:*value = m_weatherCondition.AnyFWICodesSpecified();				return S_OK;

		case CWFGM_WEATHER_OPTION_TEMP_ALPHA:		*value = m_weatherCondition.m_temp_alpha;					return S_OK;
		case CWFGM_WEATHER_OPTION_TEMP_BETA:		*value = m_weatherCondition.m_temp_beta;					return S_OK;
		case CWFGM_WEATHER_OPTION_TEMP_GAMMA:		*value = m_weatherCondition.m_temp_gamma;					return S_OK;
		case CWFGM_WEATHER_OPTION_WIND_ALPHA:		*value = m_weatherCondition.m_wind_alpha;					return S_OK;
		case CWFGM_WEATHER_OPTION_WIND_BETA:		*value = m_weatherCondition.m_wind_beta;					return S_OK;
		case CWFGM_WEATHER_OPTION_WIND_GAMMA:		*value = m_weatherCondition.m_wind_gamma;					return S_OK;
		case CWFGM_WEATHER_OPTION_INITIAL_FFMC:		*value = m_weatherCondition.m_spec_day.dFFMC;				return S_OK;
		case CWFGM_WEATHER_OPTION_INITIAL_HFFMC:	*value = m_weatherCondition.m_initialHFFMC;					return S_OK;
		case CWFGM_WEATHER_OPTION_INITIAL_DC:		*value = m_weatherCondition.m_spec_day.dDC;					return S_OK;
		case CWFGM_WEATHER_OPTION_INITIAL_DMC:		*value = m_weatherCondition.m_spec_day.dDMC;				return S_OK;
		case CWFGM_WEATHER_OPTION_INITIAL_BUI:		*value = m_weatherCondition.m_spec_day.dBUI;				return S_OK;
		case CWFGM_WEATHER_OPTION_INITIAL_RAIN:		*value = m_weatherCondition.m_initialRain;					return S_OK;
		case CWFGM_GRID_ATTRIBUTE_LATITUDE:			*value = m_weatherCondition.m_worldLocation.m_latitude();	return S_OK;
		case CWFGM_GRID_ATTRIBUTE_LONGITUDE:		*value = m_weatherCondition.m_worldLocation.m_longitude();	return S_OK;

		case CWFGM_WEATHER_OPTION_INITIAL_HFFMCTIME:	*value = m_weatherCondition.m_initialHFFMCTime;			return S_OK;
		case CWFGM_WEATHER_OPTION_START_TIME:
			*value = m_weatherCondition.m_time + WTimeSpan((std::int32_t)0, (std::int32_t)m_weatherCondition.m_firstHour, (std::int32_t)0, (std::int32_t)0);
			return S_OK;
		case CWFGM_WEATHER_OPTION_END_TIME:
			{
				WTime t(m_weatherCondition.m_time);
				m_weatherCondition.GetEndTime(t);
				*value = t;
				return S_OK;
			}
	}
			
    #ifdef DEBUG
	weak_assert(false);
    #endif

	return ERROR_WEATHER_OPTION_INVALID;
}


HRESULT CCWFGM_WeatherStream::SetAttribute(std::uint16_t option, const PolymorphicAttribute &v_value) {
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)						return ERROR_SCENARIO_SIMULATION_RUNNING;

	bool value;
	double dvalue;
	HRESULT hr;
	std::uint32_t lvalue;
	HSS_Time::WTimeSpan llvalue;
	HSS_Time::WTime ullvalue(0ULL, &m_weatherCondition.m_timeManager);
	switch (option) {
		case CWFGM_WEATHER_OPTION_FFMC_VANWAGNER:
								if (FAILED(hr = VariantToBoolean_(v_value, &value))) return hr;
								if ((value) && ((m_weatherCondition.m_options & 0x00000007) != WeatherCondition::FFMC_VAN_WAGNER)) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_options &= (~(0x0000007));	// also turn off the user override
									m_weatherCondition.m_options |= WeatherCondition::FFMC_VAN_WAGNER;
									m_bRequiresSave = true;
								}
								return S_OK;

		case CWFGM_WEATHER_OPTION_FFMC_LAWSON:
								if (FAILED(hr = VariantToBoolean_(v_value, &value))) return hr;
								if ((value) && ((m_weatherCondition.m_options & 0x00000007) != WeatherCondition::FFMC_LAWSON)) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_options &= (~(0x0000007));	// also turn off the user override
									m_weatherCondition.m_options |= WeatherCondition::FFMC_LAWSON;
									m_bRequiresSave = true;
								}
								return S_OK;

		case CWFGM_WEATHER_OPTION_FWI_USE_SPECIFIED:
								if (FAILED(hr = VariantToBoolean_(v_value, &value))) return hr;
								m_cache.Clear();
								m_weatherCondition.ClearConditions();
								if (!value)	m_weatherCondition.m_options &= (~(WeatherCondition::USER_SPECIFIED));
								else		m_weatherCondition.m_options |= WeatherCondition::USER_SPECIFIED;
								m_bRequiresSave = true;
								return S_OK;

		case CWFGM_WEATHER_OPTION_TEMP_ALPHA:
								if (FAILED(hr = VariantToDouble_(v_value, &dvalue))) return hr;
								if (m_weatherCondition.m_temp_alpha != dvalue) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_temp_alpha = dvalue;
									m_bRequiresSave = true;
								}
								return S_OK;
		case CWFGM_WEATHER_OPTION_TEMP_BETA:
								if (FAILED(hr = VariantToDouble_(v_value, &dvalue))) return hr;
								if (m_weatherCondition.m_temp_beta != dvalue) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_temp_beta = dvalue;
									m_bRequiresSave = true;
								}
								return S_OK;
		case CWFGM_WEATHER_OPTION_TEMP_GAMMA:
								if (FAILED(hr = VariantToDouble_(v_value, &dvalue))) return hr;
								if (m_weatherCondition.m_temp_gamma != dvalue) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_temp_gamma = dvalue;
									m_bRequiresSave = true;
								}
								return S_OK;
		case CWFGM_WEATHER_OPTION_WIND_ALPHA:
								if (FAILED(hr = VariantToDouble_(v_value, &dvalue))) return hr;
								if (m_weatherCondition.m_wind_alpha != dvalue) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_wind_alpha = dvalue;
									m_bRequiresSave = true;
								}
								return S_OK;
		case CWFGM_WEATHER_OPTION_WIND_BETA:
								if (FAILED(hr = VariantToDouble_(v_value, &dvalue))) return hr;
								if (m_weatherCondition.m_wind_beta != dvalue) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_wind_beta = dvalue;
									m_bRequiresSave = true;
								}
								return S_OK;
		case CWFGM_WEATHER_OPTION_WIND_GAMMA:
								if (FAILED(hr = VariantToDouble_(v_value, &dvalue))) return hr;
								if (m_weatherCondition.m_wind_gamma != dvalue) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_wind_gamma = dvalue;
									m_bRequiresSave = true;
								}
								return S_OK;

		case CWFGM_WEATHER_OPTION_INITIAL_FFMC:
								if (FAILED(hr = VariantToDouble_(v_value, &dvalue))) return hr;
								if (dvalue < 0.0)	return E_INVALIDARG;
								if (dvalue > 101.0)	return E_INVALIDARG;
								if (m_weatherCondition.m_spec_day.dFFMC != dvalue) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_spec_day.dFFMC = dvalue;
									m_bRequiresSave = true;
								}
								return S_OK;
		case CWFGM_WEATHER_OPTION_INITIAL_HFFMC:
								if (FAILED(hr = VariantToDouble_(v_value, &dvalue))) return hr;
								if (dvalue < 0.0)	return E_INVALIDARG;
								if (dvalue > 101.0)	return E_INVALIDARG;
								if ((m_weatherCondition.m_initialHFFMC != dvalue) && (m_weatherCondition.m_initialHFFMCTime != WTimeSpan(-1))) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_initialHFFMC = dvalue;
									m_bRequiresSave = true;
								}
								return S_OK;
		case CWFGM_WEATHER_OPTION_INITIAL_RAIN:
								if (FAILED(hr = VariantToDouble_(v_value, &dvalue))) return hr;
								if (m_weatherCondition.m_initialRain != dvalue) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_initialRain = dvalue;
									m_bRequiresSave = true;
								}
								return S_OK;

		case CWFGM_WEATHER_OPTION_INITIAL_DC:
								if (FAILED(hr = VariantToDouble_(v_value, &dvalue))) return hr;
								if (dvalue < 0.0)	return E_INVALIDARG;
								if (dvalue > 1500.0)	return E_INVALIDARG;
								if (m_weatherCondition.m_spec_day.dDC != dvalue) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_spec_day.dDC = dvalue;
									m_bRequiresSave = true;
								}
								return S_OK;

		case CWFGM_WEATHER_OPTION_INITIAL_DMC:
								if (FAILED(hr = VariantToDouble_(v_value, &dvalue))) return hr;
								if (dvalue < 0.0)	return E_INVALIDARG;
								if (dvalue > 500.0)	return E_INVALIDARG;
								if (m_weatherCondition.m_spec_day.dDMC != dvalue) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_spec_day.dDMC = dvalue;
									m_bRequiresSave = true;
								}
								return S_OK;
		case CWFGM_WEATHER_OPTION_INITIAL_BUI:
								if (FAILED(hr = VariantToDouble_(v_value, &dvalue))) return hr;
								if ((dvalue < 0.0) && (dvalue != -99.0))	return E_INVALIDARG;	// use -99 to clear out a specified value
								m_weatherCondition.m_spec_day.dBUI = dvalue;
								return S_OK;
		case CWFGM_GRID_ATTRIBUTE_LATITUDE:
								if (FAILED(hr = VariantToDouble_(v_value, &dvalue))) return hr;
								if (dvalue < DEGREE_TO_RADIAN(-90.0))					{ weak_assert(false); return E_INVALIDARG; }
								if (dvalue > DEGREE_TO_RADIAN(90.0))					{ weak_assert(false); return E_INVALIDARG; }
								if (m_weatherCondition.m_worldLocation.m_latitude() != dvalue) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_worldLocation.m_latitude(dvalue);
									m_bRequiresSave = true;
								}
								return S_OK;
		case CWFGM_GRID_ATTRIBUTE_LONGITUDE:
								if (FAILED(hr = VariantToDouble_(v_value, &dvalue))) return hr;
								if (dvalue < DEGREE_TO_RADIAN(-180.0))					{ weak_assert(false); return E_INVALIDARG; }
								if (dvalue > DEGREE_TO_RADIAN(180.0))					{ weak_assert(false); return E_INVALIDARG; }
								if (m_weatherCondition.m_worldLocation.m_longitude() != dvalue) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_worldLocation.m_longitude(dvalue);
									m_bRequiresSave = true;
								}
								return S_OK;

		case CWFGM_WEATHER_OPTION_INITIAL_HFFMCTIME:
								if (FAILED(hr = VariantToTimeSpan_(v_value, &llvalue)))		return hr;
								if (llvalue >= WTimeSpan(24 * 60 * 60))					return E_INVALIDARG;
								if ((llvalue < WTimeSpan(-1)) && (llvalue != WTimeSpan(-1 * 60 * 60)))		return E_INVALIDARG;
								if ((llvalue > WTimeSpan(0)) && (llvalue.GetSeconds() || llvalue.GetMinutes()))	return E_INVALIDARG;
								if (m_weatherCondition.m_initialHFFMCTime != llvalue) {
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_weatherCondition.m_initialHFFMCTime = llvalue;
									m_bRequiresSave = true;
								}
								return S_OK;
		case CWFGM_WEATHER_OPTION_START_TIME:
							{
								if (FAILED(hr = VariantToTime_(v_value, &ullvalue)))		return hr;
								WTime t(ullvalue);
								int hour = t.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
								t.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
								if ((m_weatherCondition.m_time != t) || (hour != m_weatherCondition.m_firstHour)) {
									m_weatherCondition.m_time = t;
									m_weatherCondition.m_firstHour = (std::uint8_t)hour;
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_bRequiresSave = true;
								}
								return S_OK;
							}
		case CWFGM_WEATHER_OPTION_END_TIME:
							{
								if (FAILED(hr = VariantToTime_(v_value, &ullvalue)))		return hr;
								WTime endTime(0ULL, m_weatherCondition.m_time.GetTimeManager()),
									t(ullvalue);
								int hour = t.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
								t.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
								m_weatherCondition.GetEndTime(endTime);
								if ((endTime != t) || (hour != m_weatherCondition.m_lastHour)) {
									m_weatherCondition.SetEndTime(t);
									m_cache.Clear();
									m_weatherCondition.ClearConditions();
									m_bRequiresSave = true;
								}
								return S_OK;
							}
	}
			
	weak_assert(false);
	return ERROR_WEATHER_OPTION_INVALID;
}


HRESULT CCWFGM_WeatherStream::ClearWeatherData() {
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)							return ERROR_SCENARIO_SIMULATION_RUNNING;

	m_weatherCondition.ClearWeatherData();
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::GetValidTimeRange(HSS_Time::WTime *start, HSS_Time::WTimeSpan *duration) {
	if ((!start) || (!duration))					return E_POINTER;
	CRWThreadSemaphoreEngage _semaphore_engage(m_lock, SEM_FALSE);
	WTime t(m_weatherCondition.m_time);
	t += WTimeSpan(0, m_weatherCondition.m_firstHour, 0, 0);
	start->SetTime(t);
	if (m_weatherCondition.NumDays())
		*duration = WTimeSpan(m_weatherCondition.NumDays(), -(23 - m_weatherCondition.m_lastHour) - m_weatherCondition.m_firstHour, 0, 0);
	else
		*duration = WTimeSpan(0);
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::SetValidTimeRange(const HSS_Time::WTime& start, const HSS_Time::WTimeSpan& duration, const bool correctInitialPrecip) {
	CRWThreadSemaphoreEngage _semaphore_engage(m_lock, SEM_TRUE);
	WTime s(start, &m_weatherCondition.m_timeManager);

	return m_weatherCondition.SetValidTimeRange(s, duration, correctInitialPrecip);
}


HRESULT CCWFGM_WeatherStream::GetFirstHourOfDay(const HSS_Time::WTime &time, unsigned char *hour)
{
	if (!hour) return E_POINTER;
	CRWThreadSemaphoreEngage _semaphore_engage(m_lock, SEM_FALSE);
	WTime t(time, &m_weatherCondition.m_timeManager);
	*hour = m_weatherCondition.firstHourOfDay(t);
	if (*hour != (unsigned char)-1)
		return S_OK;
	return ERROR_GRID_WEATHER_INVALID_DATES;
}

HRESULT CCWFGM_WeatherStream::GetLastHourOfDay(const HSS_Time::WTime &time, unsigned char *hour)
{
	if (!hour) return E_POINTER;
	CRWThreadSemaphoreEngage _semaphore_engage(m_lock, SEM_FALSE);
	WTime t(time, &m_weatherCondition.m_timeManager);
	*hour = m_weatherCondition.lastHourOfDay(t);
	if (*hour != (unsigned char)-1)
		return S_OK;
	return ERROR_GRID_WEATHER_INVALID_DATES;
}



HRESULT CCWFGM_WeatherStream::GetEventTime(std::uint32_t flags, const HSS_Time::WTime &from_time, HSS_Time::WTime *next_event) {
	if (!next_event)						return E_POINTER;

	CRWThreadSemaphoreEngage _semaphore_engage(m_lock, SEM_FALSE);
	WTime ft(from_time, &m_weatherCondition.m_timeManager);
	WTime ne(*next_event, &m_weatherCondition.m_timeManager);

	m_weatherCondition.GetEventTime(flags, ft, ne);
	next_event->SetTime(ne);
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::MakeHourlyObservations(const HSS_Time::WTime &time) {
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)						return ERROR_SCENARIO_SIMULATION_RUNNING;

	WTime t(time, &m_weatherCondition.m_timeManager);
	if (!m_weatherCondition.MakeHourlyObservations(t))	return ERROR_SEVERITY_WARNING;
	m_cache.Clear();
	m_bRequiresSave = true;
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::MakeDailyObservations(const HSS_Time::WTime &time) {
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)						return ERROR_SCENARIO_SIMULATION_RUNNING;

	WTime t(time, &m_weatherCondition.m_timeManager);
	if (!m_weatherCondition.MakeDailyObservations(t))	return ERROR_SEVERITY_WARNING;
	m_cache.Clear();
	m_bRequiresSave = true;
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::IsDailyObservations(const HSS_Time::WTime &time) {
	CRWThreadSemaphoreEngage _semaphore_engage(m_lock, SEM_FALSE);

	WTime t(time, &m_weatherCondition.m_timeManager);
	std::uint16_t val = m_weatherCondition.IsHourlyObservations(t);
	if (val == 1)								return ERROR_SEVERITY_WARNING;
	if (val == 2)								return ERROR_SEVERITY_WARNING | ERROR_INVALID_TIME;
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::IsModified(const HSS_Time::WTime &time) {
	CRWThreadSemaphoreEngage _semaphore_engage(m_lock, SEM_FALSE);

	WTime t(time, &m_weatherCondition.m_timeManager);
	std::uint16_t val = m_weatherCondition.IsModified(t);
	if (val == 1)								return ERROR_SEVERITY_WARNING;
	if (val == 2)								return ERROR_SEVERITY_WARNING | ERROR_INVALID_TIME;
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::IsAnyDailyObservations() {
	CRWThreadSemaphoreEngage _semaphore_engage(m_lock, SEM_FALSE);
	return m_weatherCondition.IsAnyDailyObservations();
}


HRESULT CCWFGM_WeatherStream::IsAnyModified() {
	CRWThreadSemaphoreEngage _semaphore_engage(m_lock, SEM_FALSE);
	return m_weatherCondition.IsAnyModified();
}


HRESULT CCWFGM_WeatherStream::GetDailyValues(const HSS_Time::WTime &time, double *min_temp, double *max_temp,
    double *min_ws, double *max_ws, double *min_gust, double *max_gust, double *min_rh, double *precip, double *wa) {
	if ((!min_temp) || (!max_temp) || (!min_ws) || (!max_ws) || (!min_rh) || (!precip) || (!wa))	return E_POINTER;

	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged);
	if (!engaged)
		engage.Lock(m_lock.CurrentState() < 1000000LL);

	double MIN_TEMP, MAX_TEMP, MIN_WS, MAX_WS, MIN_GUST, MAX_GUST, RH, PRECIP;
	WTime t(time, &m_weatherCondition.m_timeManager);
	bool b = m_weatherCondition.GetDailyWeatherValues(t, &MIN_TEMP, &MAX_TEMP, &MIN_WS, &MAX_WS, &MIN_GUST, &MAX_GUST, &RH, &PRECIP, wa);
	if (!b)									return ERROR_SEVERITY_WARNING;
	*min_temp = MIN_TEMP;
	*max_temp = MAX_TEMP;
	*min_ws = MIN_WS;
	*max_ws = MAX_WS;
	*min_gust = MIN_GUST;
	*max_gust = MAX_GUST;
	*min_rh = RH;
	*precip = PRECIP;
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::GetCumulativePrecip(const HSS_Time::WTime& time, const HSS_Time::WTimeSpan &duration, double* rain) {
	if (!rain)	return E_POINTER;

	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged);
	if (!engaged)
		engage.Lock(m_lock.CurrentState() < 1000000LL);

	double RAIN;
	WTime t(time, &m_weatherCondition.m_timeManager);
	bool b = m_weatherCondition.CumulativePrecip(t, duration, &RAIN);
	if (!b)									return ERROR_SEVERITY_WARNING;
	*rain = RAIN;
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::SetDailyValues(const HSS_Time::WTime &time, double min_temp, double max_temp, double min_ws,
    double max_ws, double min_gust, double max_gust, double min_rh, double precip, double wa) {
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)								return ERROR_SCENARIO_SIMULATION_RUNNING;

	WTime t(time, &m_weatherCondition.m_timeManager);
	bool b = m_weatherCondition.SetDailyWeatherValues(t, min_temp, max_temp, min_ws, max_ws, min_gust, max_gust, min_rh, precip, wa);
	if (!b)
		return ERROR_SEVERITY_WARNING;

	m_cache.Clear();
	m_bRequiresSave = true;
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::GetInstantaneousValues(const HSS_Time::WTime &time, std::uint64_t interpolation_method, IWXData *wx, IFWIData *ifwi, DFWIData *dfwi) {
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged);
	if (!engaged)
		engage.Lock(m_lock.CurrentState() < 1000000LL);

	WTime t(time, &m_weatherCondition.m_timeManager);

	WeatherKeyBase key(time);
	key.interpolate_method = interpolation_method & CWFGM_GETWEATHER_INTERPOLATE_TEMPORAL;
	{
		WeatherData *result, r;
		if ((result = m_cache.Retrieve(&key, &r, m_weatherCondition.m_time.GetTimeManager()))) {
			if (wx)		memcpy(wx, &result->wx, sizeof(IWXData));
			if (ifwi)	memcpy(ifwi, &result->ifwi, sizeof(IFWIData));
			if (dfwi)	memcpy(dfwi, &result->dfwi, sizeof(DFWIData));
			return result->hr;
		}
	}
	{
		WeatherData result;
		bool b = m_weatherCondition.GetInstantaneousValues(t, interpolation_method, &result.wx, &result.ifwi, &result.dfwi);
		if (b)
			result.hr = S_OK;
		else {
			memset(&result.wx, 0, sizeof(IWXData));
			memset(&result.ifwi, 0, sizeof(IFWIData));
			result.hr = CWFGM_WEATHER_INITIAL_VALUES_ONLY;
		}
		m_cache.Store(&key, &result, m_weatherCondition.m_time.GetTimeManager());
		if (wx)		memcpy(wx, &result.wx, sizeof(IWXData));
		if (ifwi)	memcpy(ifwi, &result.ifwi, sizeof(IFWIData));
		if (dfwi)	memcpy(dfwi, &result.dfwi, sizeof(DFWIData));
		return result.hr;

	}
}


HRESULT CCWFGM_WeatherStream::SetInstantaneousValues(const HSS_Time::WTime &time, IWXData *wx) {
	if (!wx)								return E_POINTER;
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)								return ERROR_SCENARIO_SIMULATION_RUNNING;

	WTime t(time, &m_weatherCondition.m_timeManager);

	IWXData curr_wx;
	m_weatherCondition.GetInstantaneousValues(t, 0, &curr_wx, NULL, NULL);
	if (!memcmp(wx, &curr_wx, sizeof(IWXData)))
		return S_OK;
	if (wx->DewPointTemperature <= -300.0 && m_weatherCondition.IsHourlyObservations(t) == 1) {
		if ((wx->Temperature == curr_wx.Temperature) &&
		    (wx->RH == curr_wx.RH) &&
		    (wx->Precipitation == curr_wx.Precipitation) &&
		    (wx->WindDirection == curr_wx.WindDirection) &&
			(wx->WindGust == curr_wx.WindGust) &&
		    (wx->WindSpeed == curr_wx.WindSpeed))
			return S_OK;
	}

	bool interp = false;
	if (wx->SpecifiedBits & IWXDATA_SPECIFIED_INTERPOLATED)
		interp = true;
	bool ensemble = false;
	if (wx->SpecifiedBits & IWXDATA_SPECIFIED_ENSEMBLE)
		ensemble = true;
	bool b = m_weatherCondition.SetHourlyWeatherValues(t, wx->Temperature, wx->RH, wx->Precipitation, wx->WindSpeed, wx->WindGust, wx->WindDirection, wx->DewPointTemperature, interp, ensemble);
	if (!b)
		return ERROR_SEVERITY_WARNING;

	m_cache.Clear();
	m_bRequiresSave = true;
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::IsImportedFromFile(const HSS_Time::WTime &time) {
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged);
	if (!engaged)
		engage.Lock(m_lock.CurrentState() < 1000000LL);

	if (!time.GetTotalSeconds())
		return (m_weatherCondition.m_options & WeatherCondition::FROM_FILE) ? S_OK : ERROR_SEVERITY_WARNING;

	WTime t(time, &m_weatherCondition.m_timeManager);
	std::uint16_t val = m_weatherCondition.IsOriginFile(t);
	if (val == 1)								return ERROR_SEVERITY_WARNING;
	if (val == 2)								return ERROR_SEVERITY_WARNING | ERROR_INVALID_TIME;
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::IsImportedFromEnsemble(const HSS_Time::WTime& time) {
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, TRUE, &engaged);
	if (!engaged)
		engage.Lock(m_lock.CurrentState() < 1000000LL);

	if (!time.GetTotalSeconds())
		return (m_weatherCondition.m_options & 0x00000040) ? S_OK : ERROR_SEVERITY_WARNING;

	WTime t(time, &m_weatherCondition.m_timeManager);
	std::uint16_t val = m_weatherCondition.IsOriginEnsemble(t);
	if (val == 1)								return ERROR_SEVERITY_WARNING;
	if (val == 2)								return ERROR_SEVERITY_WARNING | ERROR_INVALID_TIME;
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::DailyStandardFFMC(const HSS_Time::WTime &time, double *ffmc) {
	if (!ffmc)								return E_POINTER;

	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged);
	if (!engaged)
		engage.Lock(m_lock.CurrentState() < 1000000LL);

	WTime t(time, &m_weatherCondition.m_timeManager);
	bool spec, valid_date = m_weatherCondition.DailyFFMC(t, ffmc, &spec);
	if (!valid_date)								return ERROR_SEVERITY_WARNING | ERROR_INVALID_TIME;
	if (*ffmc < 0.0)								return ERROR_SEVERITY_WARNING;
	return S_OK;
}
