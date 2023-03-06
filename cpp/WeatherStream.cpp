/**
 * WISE_Weather_Module: WeatherStream.cpp
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

#include "types.h"
#include "weatherStream.pb.h"
#include "WTime.h"
#include "WeatherStream.h"
#include "WeatherCom_ext.h"
#include "results.h"
#include "DayCondition.h"
#include <fstream>
#include "propsysreplacement.h"
#include "GridCom_ext.h"
#include "misc.h"
#include "comcodes.h"

#include "REDappWrapper.h"

#include "doubleBuilder.h"

#include <vector>
#include <cstdint>
#include <algorithm>
#include "filesystem.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include "str_printf.h"

#ifdef DEBUG
#include <assert.h>
#endif


WeatherCondition::WeatherCondition() : m_timeManager(m_worldLocation), m_time((std::uint64_t)0, &m_timeManager) {
	m_temp_alpha = -0.77;
	m_temp_beta = 2.80;			// temp alpha, beta defaults changed to Cdn averages 060619
	m_temp_gamma = -2.20;
	m_wind_alpha = 1.00;
	m_wind_beta = 1.24;
	m_wind_gamma = -3.59;			// Denver defaults, "Beck & Trevitt, Forecasting diurnal variations in meteorological parameters for
						// predicting fire behavior", 1989
	m_spec_day.dFFMC = -1.0;
	m_initialHFFMC = 0.0;
	m_initialHFFMCTime = WTimeSpan(-1);	// this means that we haven't specified an initial HFFMC value or time for that value
	m_spec_day.dDMC = -1.0;
	m_spec_day.dDC = -1.0;
	m_spec_day.dBUI = -1.0;
	m_spec_day.dISI = -1.0;
	m_spec_day.dFWI = -1.0;
	m_initialRain = 0.0;

	m_fwi = new CCWFGM_FWI;

	m_isCalculatedValuesValid = false;

	m_options = FFMC_LAWSON;
	m_firstHour = 0;
	m_lastHour = 23;
}


WeatherCondition::WeatherCondition(const WeatherCondition &toCopy) : m_timeManager(m_worldLocation), m_time((std::uint64_t)0, &m_timeManager) {
	*this = toCopy;
}


WeatherCondition &WeatherCondition::operator=(const WeatherCondition &toCopy) {
	if (this == &toCopy)
		return *this;

	m_worldLocation = toCopy.m_worldLocation;
	m_time.SetTime(toCopy.m_time);

	m_temp_alpha = toCopy.m_temp_alpha;
	m_temp_beta = toCopy.m_temp_beta;
	m_temp_gamma = toCopy.m_temp_gamma;
	m_wind_alpha = toCopy.m_wind_alpha;
	m_wind_beta = toCopy.m_wind_beta;
	m_wind_gamma = toCopy.m_wind_gamma;

	m_spec_day = toCopy.m_spec_day;
	m_initialHFFMC = toCopy.m_initialHFFMC;
	m_initialHFFMCTime = toCopy.m_initialHFFMCTime;
	m_initialRain = toCopy.m_initialRain;
	m_options = toCopy.m_options;
	m_firstHour = toCopy.m_firstHour;
	m_lastHour = toCopy.m_lastHour;
	
	m_fwi = new CCWFGM_FWI;

	DailyCondition *dc = toCopy.m_readings.LH_Head();
	while (dc->LN_Succ()) {
		DailyCondition *ndc = new DailyCondition(*dc, this);
		m_readings.AddTail(ndc);
		dc = dc->LN_Succ();
	}

	m_isCalculatedValuesValid = toCopy.m_isCalculatedValuesValid;
	return *this;
}


WeatherCondition::~WeatherCondition() {
	ClearWeatherData();
}


DailyCondition * WeatherCondition::getDCReading(const WTime &time, bool add) {
	DailyCondition *dc;

	WTimeSpan index = time - m_time;

	if (index.GetTotalSeconds() < 0) {
		//if not allowed to add
		if (!add)
			return nullptr;
		//if adding the day before the current first specified day and that day has data to hour 0
		if (index.GetDays() == -1 && m_firstHour == 0) {
			dc = new DailyCondition(this);
			m_readings.AddHead(dc);
			ClearConditions();
			m_time -= WTimeSpan(1, 0, 0, 0);
            return dc;
		}
		return nullptr;
 	}
	dc = m_readings.IndexNode((std::uint32_t)index.GetDays());
	if ((!dc) && (add)) {
		if (m_readings.GetCount() > 0)
		{
			//no appending if the last day doesn't end on hour 23 and only append the day after the last day of the stream
			if (m_lastHour != 23 || (index.GetDays() != m_readings.GetCount()))
				return nullptr;
		}
		dc = new DailyCondition(this);
                m_readings.AddTail(dc);
		ClearConditions();
	}
	return dc;
}


void WeatherCondition::calculateValues() {
	if (m_isCalculatedValuesValid)
		return;
	m_isCalculatedValuesValid = true;

	if (m_weatherStation) {
		PolymorphicAttribute v;
		double temp;
		m_weatherStation->GetAttribute(CWFGM_GRID_ATTRIBUTE_LATITUDE, &v);
		VariantToDouble_(v, &temp);
		m_worldLocation.m_latitude(temp);
		m_weatherStation->GetAttribute(CWFGM_GRID_ATTRIBUTE_LONGITUDE, &v);
		VariantToDouble_(v, &temp);
		m_worldLocation.m_longitude(temp);
	}
	if (m_readings.IsEmpty())				// the stream has no data associated with it so abort now
		return;

	DailyCondition *fakeLast, *dc = m_readings.LH_Tail();	// this will take the diurnal curves and finish them off for the last day
	if (!(dc->m_flags & DAY_HOURLY_SPECIFIED)) {
		fakeLast = new DailyCondition(this);
		m_readings.AddTail(fakeLast);
		fakeLast->setDailyWeather(dc->dailyMinTemp(), dc->dailyMaxTemp(), dc->dailyMinWS(), dc->dailyMaxWS(), dc->dailyMinGust(), dc->dailyMaxGust(), dc->dailyMeanRH(), dc->dailyPrecip(), dc->dailyWD());
	}

	std::uint16_t i = 0;
	dc = m_readings.LH_Head();
	while (dc->LN_Succ()) {
		dc->calculateTimes(i++);
		dc = dc->LN_Succ();
	}
	dc = m_readings.LH_Head();
	while (dc->LN_Succ()) {
		dc->calculateHourlyConditions();
		dc->calculateDailyConditions();
		dc = dc->LN_Succ();
	}
	dc = m_readings.LH_Head();
	while (dc->LN_Succ()) {
		dc->calculateRemainingHourlyConditions();
		dc = dc->LN_Succ();
	}

	dc = m_readings.LH_Tail();		// this will take the diurnal curves and finish them off for the last day
	if (!(dc->m_flags & DAY_HOURLY_SPECIFIED)) {
		m_readings.Remove(fakeLast);			// then clean up
HSS_PRAGMA_WARNING_PUSH
HSS_PRAGMA_GCC(GCC diagnostic ignored "-Wdelete-non-virtual-dtor")
		delete fakeLast;
HSS_PRAGMA_WARNING_POP
	}

	dc = m_readings.LH_Head();
	while (dc->LN_Succ()) {
		dc->calculateFWI();
		dc->m_flags |= DAY_HOURLY_SPECIFIED;	// #359 - Neal wants to just use the Beck/Trevitt work as a calculator now, and not store both types of wx conditions
		dc = dc->LN_Succ();
	}
}


bool WeatherCondition::GetDailyWeatherValues(const WTime &time, double *min_temp, double *max_temp, double *min_ws, double *max_ws, double* min_gust, double* max_gust, double *min_rh, double *precip, double *wd) {
	DailyCondition *dc = getDCReading(time, false);
	if (dc) {
		calculateValues();
		dc->getDailyWeather(min_temp, max_temp, min_ws, max_ws, min_gust, max_gust, min_rh, precip, wd);
	}
	return (dc != NULL);
}


bool WeatherCondition::SetDailyWeatherValues(const WTime &time, double min_temp, double max_temp, double min_ws, double max_ws, double min_gust, double max_gust, double min_rh, double precip, double wd) {

	DailyCondition *dc = getDCReading(time, true);
	if (dc) {
		if (dc->m_flags & DAY_HOURLY_SPECIFIED)
			return false;

		double min_temp2, max_temp2, min_ws2, max_ws2, min_gust2, max_gust2, min_rh2, precip2, wd2;
		double min_temp3, max_temp3, min_ws3, max_ws3, min_gust3, max_gust3, min_rh3, precip3, wd3;
		dc->getDailyWeather(&min_temp2, &max_temp2, &min_ws2, &max_ws2, &min_gust2, &max_gust2, &min_rh2, &precip2, &wd2);
		dc->setDailyWeather(min_temp, max_temp, min_ws, max_ws, min_gust, max_gust, min_rh, precip, wd);
		dc->getDailyWeather(&min_temp3, &max_temp3, &min_ws3, &max_ws3, &min_gust3, &max_gust3, &min_rh3, &precip3, &wd3);	// we are converting between data types so I'm trying to deal with rounding errors from those conversions
		if ((fabs(min_temp2 - min_temp3) > 1e-5) ||
			(fabs(max_temp2 - max_temp3) > 1e-5) ||
			(fabs(min_ws2 - min_ws3) > 1e-5) ||
			(fabs(max_ws2 - max_ws3) > 1e-5) ||
			(fabs(min_gust2 - min_gust3) > 1e-5) ||
			(fabs(max_gust2 - max_gust3) > 1e-5) ||
			(fabs(min_rh2 - min_rh3) > 1e-5) ||
			(fabs(precip2 - precip3) > 1e-5) ||
			(fabs(wd2 - wd3) > 1e-5))
			m_options &= (~(USER_SPECIFIED));
		ClearConditions();
	}
	return (dc != NULL);
}


double WeatherCondition::GetHourlyRain(const WTime &time) {
	DailyCondition *_dc = getDCReading(time, false);
	if (_dc)
		return _dc->hourlyPrecip(time);
	return 0.0;
}


bool WeatherCondition::SetHourlyWeatherValues(const WTime &time, double temp, double rh, double precip, double ws, double gust, double wd, double dew) {
	return SetHourlyWeatherValues(time, temp, rh, precip, ws, gust, wd, dew, false);
}


bool WeatherCondition::SetHourlyWeatherValues(const WTime& time, double temp, double rh, double precip, double ws, double gust, double wd, double dew, bool interp) {
	return SetHourlyWeatherValues(time, temp, rh, precip, ws, gust, wd, dew, interp, false);
}


bool WeatherCondition::SetHourlyWeatherValues(const WTime &time, double temp, double rh, double precip, double ws, double gust, double wd, double dew, bool interp, bool ensemble) {
	DailyCondition *dc = getDCReading(time, true);
	if (dc) {
		if (!(dc->m_flags & DAY_HOURLY_SPECIFIED))
			return false;
		WTime tm(m_time);
		tm += WTimeSpan(m_readings.GetCount(), -(24 - m_lastHour), 0, 0);
		WTimeSpan diff = time - tm;
		if (diff.GetTotalHours() > 1)
			return false;
		else if (diff.GetTotalHours() == 1) {
			m_lastHour += diff.GetTotalHours();
			m_lastHour = m_lastHour % 24;
		}
		else if (time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST) < m_lastHour && (m_readings.GetCount() < 2 || diff.GetTotalHours() == -23))
			m_lastHour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);

		double temp2, rh2, precip2, ws2, gust2, wd2, dew2;
		double temp3, rh3, precip3, ws3, gust3, wd3, dew3;
		dc->hourlyWeather(time, &temp2, &rh2, &precip2, &ws2, &gust2, &wd2, &dew2);
		dc->setHourlyWeather(time, temp, rh, precip, ws, gust, wd, dew);
		dc->hourlyWeather(time, &temp3, &rh3, &precip3, &ws3, &gust3, &wd3, &dew3);
		if ((fabs(temp2 - temp3) > 1e-5) ||
			(fabs(rh2 - rh3) > 1e-5) ||
			(fabs(precip2 - precip3) > 1e-5) ||
			(fabs(ws2 - ws3) > 1e-5) ||
			(fabs(gust2 - gust3) > 1e-5) ||
			(fabs(wd2 - wd3) > 1e-5) ||
			(fabs(dew2 - dew3) > 1e-5)) {
			m_options &= (~(USER_SPECIFIED));
			if (ensemble) {
				dc->m_flags |= DAY_ORIGIN_ENSEMBLE;
				m_options |= 0x00000040;
			}
			else
				dc->m_flags |= DAY_ORIGIN_MODIFIED;
		}
		if (interp) {
			dc->setHourInterpolated(time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST));
		}
		else {
		dc->clearHourInterpolated(time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST));
		}
		ClearConditions();
	}
	return (dc != NULL);
}


bool WeatherCondition::MakeHourlyObservations(const WTime &time) {
	DailyCondition *dc = getDCReading(time, true);
	if (dc) {
		if (!(dc->m_flags & DAY_HOURLY_SPECIFIED)) {
			dc->m_flags |= DAY_HOURLY_SPECIFIED;
			ClearConditions();
		}
	}
	return (dc != NULL);
}


bool WeatherCondition::MakeDailyObservations(const WTime &time) {
	DailyCondition *dc = getDCReading(time, true);
	if (dc) {
		if (dc->m_flags & DAY_HOURLY_SPECIFIED) {
			dc->m_flags &= (~(DAY_HOURLY_SPECIFIED));
			ClearConditions();
		}
	}
	return (dc != NULL);
}


std::int16_t WeatherCondition::WarnOnSunRiseSet() {
	std::int16_t retval = 0;
	DailyCondition *dc = m_readings.LH_Head();
	while (dc->LN_Succ()) {
		if (!(dc->m_flags & DAY_HOURLY_SPECIFIED)) {
			if (dc->m_DayStart == dc->m_SunRise)
				retval |= NO_SUNRISE;
			if ((dc->m_DayStart + WTimeSpan(0, 23, 59, 59)) == dc->m_SunSet)
				retval |= NO_SUNSET;
		}
		dc = dc->LN_Succ();
	}
	return retval;
}


std::uint16_t WeatherCondition::IsHourlyObservations(const WTime &time) {
	DailyCondition *dc = getDCReading(time, false);
	if (dc) {
		if (dc->m_flags & DAY_HOURLY_SPECIFIED)
		{
			int32_t hour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
			if (hour < firstHourOfDay(time) || hour > lastHourOfDay(time))
				return 2;
			return 1;
		}
		return 0;
	}
	return 2;
}


HRESULT WeatherCondition::IsAnyDailyObservations() const {
	DailyCondition* dc = m_readings.LH_Head();
	while (dc->LN_Succ()) {
		if (!(dc->m_flags & DAY_HOURLY_SPECIFIED))
			return S_OK;
		dc = dc->LN_Succ();
	}
	return ERROR_SEVERITY_WARNING;
}


std::uint16_t WeatherCondition::IsModified(const WTime& time) {
	DailyCondition* dc = getDCReading(time, false);
	if (dc) {
		if (dc->m_flags & DAY_ORIGIN_MODIFIED) {
			int32_t hour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
			if (hour < firstHourOfDay(time) || hour > lastHourOfDay(time))
				return 2;
			return 1;
		}
		return 0;
	}
	return 2;
}


HRESULT WeatherCondition::IsAnyModified() const {
	DailyCondition* dc = m_readings.LH_Head();
	while (dc->LN_Succ()) {
		if (dc->m_flags & DAY_ORIGIN_MODIFIED)
			return S_OK;
		dc = dc->LN_Succ();
	}
	return ERROR_SEVERITY_WARNING;
}


std::uint16_t WeatherCondition::IsOriginFile(const WTime &time) {
	DailyCondition *dc = getDCReading(time, false);
	if (dc) {
		if (dc->m_flags & DAY_ORIGIN_FILE)
			return 0;				// it's from a file (originally, could have been modified since then)
		return 1;					// it's not CREATED from a file (created by the user)
	}
	return 2;						// time/date doesn't exist
}


std::uint16_t WeatherCondition::IsOriginEnsemble(const WTime& time) {
	DailyCondition* dc = getDCReading(time, false);
	if (dc) {
		if (dc->m_flags & DAY_ORIGIN_ENSEMBLE)
			return 0;				// it's from a file (originally, could have been modified since then)
		return 1;					// it's not CREATED from a file (created by the user)
	}
	return 2;						// time/date doesn't exist
}


uint8_t WeatherCondition::firstHourOfDay(const WTime& time)
{
	WTime temp(time);
	temp.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	WTime start(m_time);
	start.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	WTime end(start);
	end += WTimeSpan(m_readings.GetCount(), 0, 0, 0);
	if ((temp < start) || (temp > end))
		return (uint8_t)-1;
	if (start == temp)
		return (uint8_t)m_firstHour;
	return (uint8_t)0;
}


uint8_t WeatherCondition::lastHourOfDay(const WTime& time)
{
	WTime temp(time);
	temp.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	WTime start(m_time);
	start.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	WTime end(start);
	end += WTimeSpan(m_readings.GetCount() - 1, m_lastHour, 0, 0);
	end.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	if ((temp < start) || (temp > end))
		return (uint8_t)-1;
	if (end == temp)
		return (uint8_t)m_lastHour;
	return (uint8_t)23;
}


bool WeatherCondition::GetInstantaneousValues(const WTime &time, std::uint32_t method, IWXData *wx, IFWIData *ifwi, DFWIData *dfwi) {
	WTime nt1(time);
	nt1.PurgeToHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	calculateValues();

	WTime nt2(nt1);
	nt2 += WTimeSpan(0, 1, 0, 0);
	DailyCondition	*dc1 = getDCReading(nt1, false),
			*dc2 = getDCReading(nt2, false);

	double perc1, perc2;
	double rh1, rh2;

	if ((!dc1) || (!dc2) || (nt1 == time) || (!(method & CWFGM_GETWEATHER_INTERPOLATE_TEMPORAL))) {
		if (dc1) {
			if (wx) {
				dc1->hourlyWeather(time, &wx->Temperature, &wx->RH, &wx->Precipitation, &wx->WindSpeed, &wx->WindGust, &wx->WindDirection, &wx->DewPointTemperature);
				if (method & CWFGM_GETWEATHER_INTERPOLATE_TEMPORAL)		// if temporal interp is turned on...
					if ((!dc2) && (nt1 != time))				// and we don't have a reading for the next day AND we were asking for some time other than 11pm
						wx->Precipitation = 0.0;
				wx->SpecifiedBits = IWXDATA_SPECIFIED_TEMPERATURE | IWXDATA_SPECIFIED_RH | IWXDATA_SPECIFIED_PRECIPITATION | IWXDATA_SPECIFIED_WINDSPEED | IWXDATA_SPECIFIED_WINDDIRECTION | IWXDATA_SPECIFIED_DEWPOINTTEMPERATURE;
				if (wx->WindGust >= 0.0)
					wx->SpecifiedBits |= IWXDATA_SPECIFIED_WINDGUST;
				if (dc1->isTimeInterpolated(time))
					wx->SpecifiedBits |= IWXDATA_SPECIFIED_INTERPOLATED;
			}
		}
		if ((!dc1) || (nt1 == time) || (!(method & CWFGM_GETWEATHER_INTERPOLATE_TEMPORAL))) {
			if (dc1) {
				if (ifwi) {
					ifwi->FFMC = dc1->hourlyFFMC(time);
					ifwi->ISI = dc1->ISI(time);
					ifwi->FWI = dc1->FWI(time);
					if (dc1->isHourlyFFMCSpecified(time))	ifwi->SpecifiedBits = IFWIDATA_SPECIFIED_FWI;
					else					ifwi->SpecifiedBits = 0;
					ifwi->SpecifiedBits |= (m_options & FFMC_MASK) << 16;
				}
			}
			if (dfwi) {
				dfwi->SpecifiedBits = 0;
				bool spec;
				DailyFFMC(time, &dfwi->dFFMC, &spec);	if (spec)	dfwi->SpecifiedBits |= DFWIDATA_SPECIFIED_FFMC;
				DC(time, &dfwi->dDC, &spec);		if (spec)	dfwi->SpecifiedBits |= DFWIDATA_SPECIFIED_DC;
				DMC(time, &dfwi->dDMC, &spec);		if (spec)	dfwi->SpecifiedBits |= DFWIDATA_SPECIFIED_DMC;
				BUI(time, &dfwi->dBUI, &spec);		if (spec)	dfwi->SpecifiedBits |= DFWIDATA_SPECIFIED_BUI;
				DailyISI(time, &dfwi->dISI);
				DailyFWI(time, &dfwi->dFWI);
				if ((dfwi->dFFMC >= 0.0) && (dfwi->dISI == -1.0) && (dc1)) {
					weak_assert(!dc1->LN_Pred()->LN_Pred());
					const WTime dayNeutral(dc1->m_DayStart, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, 1);	// convert to a timezone-neutral time
					const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL, -1);				// gets us the start of the true LST day
					WTime dayNoon(dayLST);
					dayNoon += WTimeSpan(0, 12, 0, 0);
					double ws = dc1->hourlyWS(dayNoon);
					m_fwi->ISI_FBP(dfwi->dFFMC, ws, 24 * 60 * 60, &dfwi->dISI);
					m_fwi->FWI(dfwi->dISI, dfwi->dBUI, &dfwi->dFWI);
				}
			}
			if (dc1) {
				if (!dc1->LN_Pred()->LN_Pred()) {	// if the first day...
					std::int32_t hour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
					if (hour < m_firstHour)
						return false;
				}
			}
			if ((dc2) && (dc2 == dc1)) {
				if (!dc2->LN_Succ()->LN_Succ()) {	// if the last day....
					std::int32_t hour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
					if (hour > m_lastHour)
						return false;
				}
			}
			return (dc1 != nullptr);
		}
		rh1 = rh2 = wx->RH;
	} else {
		weak_assert(dc1);
		double t1, p1, ws1, wd1, gust1, dew1;
		double t2, p2, ws2, wd2, gust2, dew2;
		dc1->hourlyWeather(nt1, &t1, &rh1, &p1, &ws1, &gust1, &wd1, &dew1);
		dc2->hourlyWeather(nt2, &t2, &rh2, &p2, &ws2, &gust2, &wd2, &dew2);
		perc2 = ((double)(time.GetTime(0) - nt1.GetTime(0))) / 3600.0;
		perc1 = 1.0 - perc2;

		IWXData dwx;
		if (!wx)
			wx = &dwx;

		wx->Temperature = t1 * perc1 + t2 * perc2;
		wx->DewPointTemperature = dew1 * perc1 + dew2 * perc2;
		wx->RH = rh1 * perc1 + rh2 * perc2;
		wx->Precipitation = p2 * perc2;

		bool bb1 = ((ws1 < 0.0001) && (wd1 < 0.0001));
		bool bb2 = ((ws2 < 0.0001) && (wd2 < 0.0001));
		double wd_diff = NORMALIZE_ANGLE_RADIAN(wd2 - wd1);

		if (bb1)	wx->WindDirection = wd2;				// ws/wd at start of hour is dead calm so no interp on wd
		else if (bb2)	wx->WindDirection = wd1;				// ws/wd at start of hour is dead calm so no interp on wd
		else {							// interp wd linearly
			if ((ws1 >= 0.0001) && (ws2 >= 0.0001) && ((wd_diff < DEGREE_TO_RADIAN(181.0)) && (wd_diff > DEGREE_TO_RADIAN(179.0)))) {
				WTimeSpan ts = nt2 - nt1;
				ts /= 2;
				if (time <= (nt1 + ts))
					wx->WindDirection = wd1;
				else	wx->WindDirection = wd2;
			} else {
				if (wd_diff > CONSTANTS_NAMESPACE::Pi<double>())
					wd_diff -= CONSTANTS_NAMESPACE::TwoPi<double>();
				wx->WindDirection = NORMALIZE_ANGLE_RADIAN(wd2 - perc1 * wd_diff);
			}
		}

		if ((ws1 >= 0.0001) && (ws2 >= 0.0001) && ((wd_diff < DEGREE_TO_RADIAN(181.0)) && (wd_diff > DEGREE_TO_RADIAN(179.0)))) {
			WTimeSpan ts = nt2 - nt1;
			ts /= 2;
			if (time <= (nt1 + ts))
				wx->WindSpeed = ws1;
			else	wx->WindSpeed = ws2;
		} else	wx->WindSpeed = ws1 * perc1 + ws2 * perc2;		// interp ws linearly
		wx->SpecifiedBits = 0;
		if (dc1->isTimeInterpolated(time))
			wx->SpecifiedBits |= IWXDATA_SPECIFIED_INTERPOLATED;
	}

	DFWIData ddfwi;
	if (!dfwi)
		dfwi = &ddfwi;

	dfwi->SpecifiedBits = 0;
	bool spec;
	DailyFFMC(time, &dfwi->dFFMC, &spec);	if (spec)	dfwi->SpecifiedBits |= DFWIDATA_SPECIFIED_FFMC;
	DC(time, &dfwi->dDC, &spec);		if (spec)	dfwi->SpecifiedBits |= DFWIDATA_SPECIFIED_DC;
	DMC(time, &dfwi->dDMC, &spec);		if (spec)	dfwi->SpecifiedBits |= DFWIDATA_SPECIFIED_DMC;
	BUI(time, &dfwi->dBUI, &spec);		if (spec)	dfwi->SpecifiedBits |= DFWIDATA_SPECIFIED_BUI;
	DailyISI(time, &dfwi->dISI);
	DailyFWI(time, &dfwi->dFWI);
	if ((dfwi->dFFMC >= 0.0) && (dfwi->dISI == -1.0)) {
		weak_assert(!dc1->LN_Pred()->LN_Pred());
		const WTime dayNeutral(dc1->m_DayStart, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, 1);	// convert to a timezone-neutral time
		const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL, -1);				// gets us the start of the true LST day
		WTime dayNoon(dayLST);
		dayNoon += WTimeSpan(0, 12, 0, 0);
		double ws = dc1->hourlyWS(dayNoon);
		m_fwi->ISI_FBP(dfwi->dFFMC, ws, 24 * 60 * 60, &dfwi->dISI);
		m_fwi->FWI(dfwi->dISI, dfwi->dBUI, &dfwi->dFWI);
	}

	if (ifwi) {
		ifwi->SpecifiedBits = 0;
		double	ffmc1 = dc1->hourlyFFMC(nt1),
			ffmc2 = (dc2) ? dc2->hourlyFFMC(nt2) : ffmc1;
		bool	fs2 = (dc2) ? dc2->isHourlyFFMCSpecified(nt2) : false;

		if (fs2) {
			ifwi->SpecifiedBits |= IFWIDATA_SPECIFIED_FWI;
			ifwi->FFMC = ffmc1 * perc1 + ffmc2 * perc2;	// RWB: 080203: if the target FFMC value was specified by the user, then we should linearly interpolate to it to make sure we are consistent
		} else {						// but because using ffmc1 as a starting code (below) we aren't concerned about checking fs1
				switch (m_options & FFMC_MASK) {
					case FFMC_LAWSON:		{	double prev_ffmc, today_ffmc;

								WTime dayStart(time);
								dayStart.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
								const WTime dayNeutral(dayStart, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, 1);	// convert to a timezone-neutral time
								const WTime dayStartLst(dayNeutral, WTIME_FORMAT_AS_LOCAL, -1);					// gets us the start of the true LST day

								DailyFFMC(dayStart , &prev_ffmc, &spec);
								DailyFFMC(dayStart + WTimeSpan(0, 18, 0, 0), &today_ffmc, &spec);

								m_fwi->HourlyFFMC_Lawson_Contiguous(prev_ffmc,
								    today_ffmc, wx->Precipitation, wx->Temperature, rh1, wx->RH, rh2, wx->WindSpeed,
								    (std::uint32_t)(time - dayStartLst).GetTotalSeconds(), &ifwi->FFMC);

								break;
							}
					default: {	double in_ffmc = ffmc1;

								m_fwi->HourlyFFMC_VanWagner(in_ffmc, wx->Precipitation, wx->Temperature, wx->RH, wx->WindSpeed,
								    WTimeSpan(time - nt1).GetTotalSeconds(), &ifwi->FFMC);

								break;
							}
				}
		}

		m_fwi->ISI_FBP(ifwi->FFMC, wx->WindSpeed, time.GetTotalSeconds(), &ifwi->ISI);
		m_fwi->FWI(ifwi->ISI, dfwi->dBUI, &ifwi->FWI);
	}
	return true;
}


bool WeatherCondition::HourlyFFMC(const WTime &time, double *ffmc) {
	DailyCondition *_dc = getDCReading(time, false);
	if (_dc) {
		calculateValues();
		*ffmc = _dc->hourlyFFMC(time);
	} else if ((time < m_time) && (m_initialHFFMCTime == WTimeSpan(-1 * 60 * 60))) {
		*ffmc = m_initialHFFMC;
		return true;
	} else {
		weak_assert(false);
	}
	return (_dc != NULL);
}


bool WeatherCondition::DailyFFMC(const WTime &time, double *ffmc, bool *specified) {
	const WTime dayNeutral(time, WTIME_FORMAT_AS_LOCAL, 1);	// convert to a timezone-neutral time
	const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, -1);				// gets us the start of the true LST day
	WTime dayNoon(dayLST);
	dayNoon -= WTimeSpan(0, 12, 0, 0);
	dayNoon.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	DailyCondition *_dc = getDCReading(dayNoon, false);
	if (_dc) {
		calculateValues();
		*ffmc = _dc->dailyFFMC();
		*specified = _dc->dailyFFMCSpecified();
	} else if (dayNoon < m_time) {
		*ffmc = m_spec_day.dFFMC;
		*specified = true;
		return true;
	}
	return (_dc != NULL);
}


bool WeatherCondition::DailyISI(const WTime &time, double *isi) {
	const WTime dayNeutral(time, WTIME_FORMAT_AS_LOCAL, 1);	// convert to a timezone-neutral time
	const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, -1);				// gets us the start of the true LST day
	WTime dayNoon(dayLST);
	dayNoon -= WTimeSpan(0, 12, 0, 0);
	dayNoon.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	DailyCondition *_dc = getDCReading(dayNoon, false);
	if (_dc) {
		calculateValues();
		*isi = _dc->dailyISI();
	}
	else if (dayNoon < m_time) {
		*isi = -1.0;
		return true;
	}
	return (_dc != nullptr);
}


bool WeatherCondition::DailyFWI(const WTime &time, double *fwi) {
	const WTime dayNeutral(time, WTIME_FORMAT_AS_LOCAL, 1);	// convert to a timezone-neutral time
	const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, -1);				// gets us the start of the true LST day
	WTime dayNoon(dayLST);
	dayNoon -= WTimeSpan(0, 12, 0, 0);
	dayNoon.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	DailyCondition *_dc = getDCReading(dayNoon, false);
	if (_dc) {
		calculateValues();
		*fwi = _dc->dailyFWI();
	}
	else if (dayNoon < m_time) {
		*fwi = -1.0;
		return true;
	}
	return (_dc != nullptr);
}


bool WeatherCondition::DC(const WTime &time, double *dc, bool *specified) {
	const WTime dayNeutral(time, WTIME_FORMAT_AS_LOCAL, 1);	// convert to a timezone-neutral time
	const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, -1);				// gets us the start of the true LST day
	WTime dayNoon(dayLST);
	dayNoon -= WTimeSpan(0, 12, 0, 0);
	dayNoon.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	DailyCondition *_dc = getDCReading(dayNoon, false);
	if (_dc) {
		calculateValues();
		*dc = _dc->DC();
		*specified = _dc->DCSpecified();
	} else if (dayNoon < m_time) {
		*dc = m_spec_day.dDC;
		*specified = true;
		return true;
	}
	return (_dc != nullptr);
}


bool WeatherCondition::DMC(const WTime &time, double *dmc, bool *specified) {
	const WTime dayNeutral(time, WTIME_FORMAT_AS_LOCAL, 1);	// convert to a timezone-neutral time
	const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, -1);				// gets us the start of the true LST day
	WTime dayNoon(dayLST);
	dayNoon -= WTimeSpan(0, 12, 0, 0);
	dayNoon.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	DailyCondition *_dc = getDCReading(dayNoon, false);
	if (_dc) {
		calculateValues();
		*dmc = _dc->DMC();
		*specified = _dc->DMCSpecified();
	} else if (dayNoon < m_time) {
		*dmc = m_spec_day.dDMC;
		*specified = true;
		return true;
	}
	return (_dc != nullptr);
}


bool WeatherCondition::BUI(const WTime &time, double *bui, bool *specified, bool recalculate) {
	const WTime dayNeutral(time, WTIME_FORMAT_AS_LOCAL, 1);	// convert to a timezone-neutral time
	const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, -1);				// gets us the start of the true LST day
	WTime dayNoon(dayLST);
	dayNoon -= WTimeSpan(0, 12, 0, 0);
	dayNoon.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	DailyCondition *_dc = getDCReading(dayNoon, false);
	if (_dc) {
		if (recalculate)
		calculateValues();
		*bui = _dc->BUI();
		*specified = _dc->BUISpecified();
	} else if (dayNoon < m_time) {
		if (m_spec_day.dBUI < 0.0) {
			m_fwi->BUI(m_spec_day.dDC, m_spec_day.dDMC, bui);
			*specified = false;
		}
		else {
			*bui = m_spec_day.dBUI;
			*specified = true;
		}
		return true;
	}
	return (_dc != nullptr);
}


bool WeatherCondition::CumulativePrecip(const WTime &time, const WTimeSpan &duration, double *rain) {
			// this function needs more work, since it assumes both 'time' and 'duration' are on even hour increments
	WTime t(time, &m_timeManager);
	WTime end_t = m_time + WTimeSpan((std::int32_t)0, (std::int32_t)m_firstHour, (std::int32_t)0, (std::int32_t)0);
	t.PurgeToHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);

	std::uint32_t hrs = duration.GetTotalHours();
	*rain = 0.0;
	std::uint16_t i;
	for (i = 0; i < hrs; i++) {
		if (t <= end_t)
			break;
		*rain += GetHourlyRain(t);
		t -= WTimeSpan(0, 1, 0, 0);
	}
	if (i < hrs)
		*rain += m_initialRain;
	return true;
}


bool WeatherCondition::HourlyISI(const WTime &time, double *isi) {
	DailyCondition *_dc = getDCReading(time, false);
	if (_dc) {
		calculateValues();
		*isi = _dc->ISI(time);
		return true;
	}
	return false;
}


bool WeatherCondition::HourlyFWI(const WTime &time, double *fwi) {
	DailyCondition *_dc = getDCReading(time, false);
	if (_dc) {
		calculateValues();
		*fwi = _dc->FWI(time);
		return true;
	}
	return false;
}


void WeatherCondition::ClearConditions() {
	m_isCalculatedValuesValid = false;
}


bool WeatherCondition::AnyFWICodesSpecified() {
	DailyCondition *dc = m_readings.LH_Head();
	while (dc->LN_Succ()) {
		if (dc->AnyFWICodesSpecified())
			return true;
		dc = dc->LN_Succ();
	}
	return false;
}


void WeatherCondition::GetEndTime(WTime &EndTime)
{
	int count=m_readings.GetCount() - 1;
	WTimeSpan timeSpan(count, 0, 0, 0);
	EndTime=WTime(m_time);
	EndTime+=timeSpan;
	EndTime += WTimeSpan(0, m_lastHour, 59, 59);
}


void WeatherCondition::SetEndTime(WTime &endTime) {
	WTime currentEndTime(m_time);
	WTimeSpan ts;

	bool IncFlag = false;
	std::uint32_t oldDays = NumDays();

	if (oldDays==0) {
		return;
	}
	currentEndTime = m_time + WTimeSpan(oldDays - 1, 0, 0, 0);
	if (endTime > currentEndTime) {
		ts = endTime - currentEndTime;
		IncFlag = true;
	} else {
		ts = currentEndTime - endTime;
		IncFlag = false;
	}
	std::uint32_t days = (std::uint32_t)ts.GetDays();
	if (days >= oldDays && IncFlag == false)
		days = oldDays;
	if (IncFlag)
		IncreaseConditions(currentEndTime, days);
	else
		DecreaseConditions(currentEndTime, days);
}


void WeatherCondition::IncreaseConditions(WTime &currentEndTime, std::uint32_t days) {
	WTime tempTime(currentEndTime);
	tempTime += WTimeSpan(1, 0, 0, 0);
	WTime start(tempTime.GetYear(0), tempTime.GetMonth(0), tempTime.GetDay(0), 0, 0, 0, currentEndTime.GetTimeManager());
	for (std::uint32_t i = 0; i < days; i++) {
		CopyDailyCondition(currentEndTime, start);
		start += WTimeSpan(1, 0, 0, 0);
	}
}


void WeatherCondition::DecreaseConditions(WTime &currentEndTime, std::uint32_t days) {
	DailyCondition *dc;
	for(std::uint32_t i=0;i<days;i++) {
		dc = m_readings.RemTail();
HSS_PRAGMA_WARNING_PUSH
HSS_PRAGMA_GCC(GCC diagnostic ignored "-Wdelete-non-virtual-dtor")
		delete dc;
HSS_PRAGMA_WARNING_POP
	}
}


void WeatherCondition::CopyDailyCondition(WTime &source, WTime &dest)
{
	double min_temp;
	double max_temp;
	double min_ws;
	double max_ws;
	double min_gust;
	double max_gust;
	double rh;
	double precip;
	double wd;
	GetDailyWeatherValues(source, &min_temp, &max_temp, &min_ws, &max_ws, &min_gust, &max_gust, &rh, &precip, &wd);
	SetDailyWeatherValues(dest,min_temp,max_temp,min_ws,max_ws,min_gust,max_gust,rh,precip,wd);
}


void WeatherCondition::ClearWeatherData() {
	DailyCondition *dc;
HSS_PRAGMA_WARNING_PUSH
HSS_PRAGMA_GCC(GCC diagnostic ignored "-Wdelete-non-virtual-dtor")
	while ((dc = m_readings.RemHead()))
		delete dc;
HSS_PRAGMA_WARNING_POP
}


bool isWeatherCollectionValid(const WeatherCollection &collection)
{
	if ((collection.wd < 0.0) || (collection.wd > 360.0) ||
		(collection.ws < 0.0) ||
		(collection.rh < 0.0) || (collection.rh > 100.0) ||
		(collection.precip < 0.0) ||
		(collection.temp < -50.0) || (collection.temp > 60.0) ||
		((collection.DMC < 0.0) && (collection.DMC != -1.0)) || ((collection.DC < 0.0) && (collection.DC != -1.0)) ||
		(collection.DMC > 500.0) || (collection.DC > 1500.0))
		return false;
	return true;
}


HRESULT WeatherCondition::Import(const TCHAR *fileName, std::uint16_t options, std::shared_ptr<validation::validation_object> valid) {
	std::vector<std::string> header;
	DailyCondition *dc;
	HRESULT hr = S_OK;
	bool can_append = (options & CWFGM_WEATHERSTREAM_IMPORT_SUPPORT_APPEND) ? true : false;

	if (m_readings.IsEmpty())
		can_append = true;

	if (options & CWFGM_WEATHERSTREAM_IMPORT_PURGE) {
		ClearWeatherData();
		can_append = true;
	}

	WeatherCollection *wc = nullptr;
	int hour = 0, noonhour = -1;
	std::uint16_t mode = 0;
	std::uint32_t lines = 0;
	FILE *f = NULL;
HSS_PRAGMA_WARNING_PUSH
HSS_PRAGMA_GCC(GCC diagnostic ignored "-Wunused-value")
	_tfopen_s(&f, fileName, _T("r"));
HSS_PRAGMA_WARNING_POP
	if (f) {
		TCHAR file_type[40], line[256];
		if (!_fgetts(line, 255, f)) { fclose(f); return ERROR_READ_FAULT | ERROR_SEVERITY_WARNING; }
		ProcessHeader(line, header);
		std::string flagStr;
		flagStr = header[0];
		if (boost::iequals(flagStr, _T("daily")))
			mode = 1;
		else if (boost::iequals(flagStr, _T("hourly")))
			mode = 2;
		else if (boost::iequals(flagStr, _T("date"))) {
			mode = 1;
			for (size_t ii = 1; ii < header.size(); ii++) {
				std::string str = header[ii];
				if (boost::iequals(str, _T("hour")) || boost::iequals(str, _T("Time(CST)"))) {
					mode = 2;
					break;
				}
			}
		}
		else if (isSupportedFormat(line, header)) // this currently just returns true, later this will parse the header and check if the required columns exist in the file or not
			mode=3; // all other formats
		else {
			fclose(f);
			return ERROR_BAD_FILE_TYPE | ERROR_SEVERITY_WARNING;
		}

		if (mode == 1) {
			WTime lastTime((std::uint64_t)0, &m_timeManager);
			if (m_readings.GetCount())
				lastTime = m_time + WTimeSpan(m_readings.GetCount(), 0, 0, 0);
			m_firstHour = 0;
			m_lastHour = 23;

			while (_fgetts(line, 255, f)) {
				for(size_t i=_tcslen(line)-1;line[i]==' '||line[i]==0x0a || line[i]==0x0d || line[i]==',' || line[i]=='\t' || line[i]==';' || line[i]==0x22;i--)
					line[i]='\0';
				if (!(line[0]))
					continue;
				_tcscat_s(line, 255, _T("\n"));
				double min_temp=-100, max_temp=-100, min_ws=-100, max_ws=-100, min_gust = -100, max_gust = -100, rh=-100, precip=-100, wd=-100;

				FillDailyLineValue(header, line,file_type, &min_temp,&max_temp,&rh,&precip,&min_ws,&max_ws,&min_gust,&max_gust,&wd);

				if ((wd < 0.0) || (wd > 360.0) ||
				    (min_ws < 0.0) || (max_ws < 0.0) ||
					(rh < 0.0) || (rh > 100.0) ||
				    (precip < 0.0) ||
				    (min_temp < -50.0) || (min_temp > 60.0) ||
				    (max_temp < -50.0) || (max_temp > 60.0)) {
					hr = ERROR_INVALID_DATA | ERROR_SEVERITY_WARNING;
					goto DONE;
				}

				wd = DEGREE_TO_RADIAN(COMPASS_TO_CARTESIAN_DEGREE(wd));
				if ((max_ws > 0.0) && (wd == 0.0))
					wd = CONSTANTS_NAMESPACE::TwoPi<double>();
				rh *= 0.01;	// go from 0..100 to 0..1

				if (m_readings.IsEmpty())
					m_time.ParseDateTime(std::string(file_type), WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);

				WTime t(m_time);
				t.ParseDateTime(std::string(file_type), WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);

				WTime endTime(m_time);
				GetEndTime(endTime);
				endTime += WTimeSpan(0, 0, 0, 1);

				if (t < m_time) {						// can't prepend data
					hr = ERROR_WEATHER_STREAM_ATTEMPT_PREPEND;
					goto DONE;
				}

				if (lastTime.GetTotalSeconds()) {				// if there's some data around (either already loaded, or from an earlier iteration...)
					if ((t < lastTime) && (!(options & CWFGM_WEATHERSTREAM_IMPORT_SUPPORT_OVERWRITE))) {
												// don't overwrite if we aren't allowed to
						hr = ERROR_WEATHER_STREAM_ATTEMPT_OVERWRITE;
						goto DONE;
					}
					if (((!lines) && (t > lastTime)) || ((lines) && (t != (lastTime + WTimeSpan(1, 0, 0, 0))))) {
																// make sure things are in order in the file, and not missing any data
						hr = ERROR_INVALID_TIME | ERROR_SEVERITY_WARNING;
						goto DONE;
					}
				}

				lastTime = t;

				dc = getDCReading(t, can_append);
				if (!dc) {
					hr = ERROR_WEATHER_STREAM_ATTEMPT_APPEND;
					goto DONE;
				}
				MakeDailyObservations(t);
				dc->m_flags |= DAY_ORIGIN_FILE;

				if (min_temp > max_temp)
				{
					double ttt = min_temp;
					min_temp = max_temp;
					max_temp = ttt;
				}
				if (min_ws > max_ws)
				{
					double ttt = min_ws;
					min_ws = max_ws;
					max_ws = ttt;
				}
				if (min_gust > max_gust)
				{
					double ttt = min_gust;
					min_gust = max_gust;
					max_gust = ttt;
				}

				dc->setDailyWeather(min_temp, max_temp, min_ws, max_ws, min_gust, max_gust, rh, precip, wd);
				lines++;
			}
		} else if (mode == 2) {
			WTime dayNoon(&m_timeManager);
			WTime lastTime((std::uint64_t)0, &m_timeManager);
			bool startTimeSpecified = !m_readings.IsEmpty();
			//if there is previous data get the last hour that was imported.
			if (m_readings.GetCount()) {
				lastTime = m_time + WTimeSpan(m_readings.GetCount() - 1, m_lastHour, 0, 0);
				lines = ((m_readings.GetCount() - 1) * 24) - m_firstHour + m_lastHour;

				const WTime dayNeutral(m_time, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, 1);	// convert to a timezone-neutral time
				const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL, -1);				// gets us the start of the true LST day
				dayNoon = dayLST;
				dayNoon += WTimeSpan(0, 12, 0, 0);

				noonhour = dayNoon.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
			}
			else {
				lines = 0;
			}

			REDapp::JavaWeatherStream wStream;
			wStream.setLatitude(m_worldLocation.m_latitude());
			wStream.setLongitude(m_worldLocation.m_longitude());
			wStream.setTimezone(m_worldLocation.m_timezone().GetTotalSeconds());
			wStream.setDaylightSavingsStart(m_worldLocation.m_startDST().GetTotalSeconds());
			wStream.setDaylightSavings(m_worldLocation.m_amtDST().GetTotalSeconds());
			wStream.setDaylightSavingsEnd(m_worldLocation.m_endDST().GetTotalSeconds());

			if (valid != nullptr)
				wStream.setAllowInvalid(REDapp::JavaWeatherStream::InvalidHandler::ALLOW);
			else
				wStream.setAllowInvalid(REDapp::JavaWeatherStream::InvalidHandler::FAILURE);
			
			size_t size;
            std::string fn = fileName;
			wc = wStream.importHourly(fn, &hr, &size);
			std::vector<WeatherCollection> weather(wc, wc + size);
			if (weather.size() == 0)
				goto DONE;
			if ((hr != S_OK) && 
				 (hr != (ERROR_INVALID_DATA | ERROR_SEVERITY_WARNING)) &&
				 (hr != ERROR_INVALID_DATA) &&
				 (hr != WARNING_WEATHER_STREAM_INTERPOLATE) &&
				 (hr != WARNING_WEATHER_STREAM_INTERPOLATE_BEFORE_INVALID_DATA))
			{
				goto DONE;
			}
			
			WTime streamStartTime(weather[0].epoch, &m_timeManager, false);
			streamStartTime.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
			if (!startTimeSpecified)
			{
				m_time = WTime(streamStartTime);
				m_time.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
				m_firstHour = (int) weather[0].hour;

				const WTime dayNeutral(m_time, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, 1);	// convert to a timezone-neutral time
				const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL, -1);				// gets us the start of the true LST day
				dayNoon = dayLST;
				dayNoon += WTimeSpan(0, 12, 0, 0);

				noonhour = dayNoon.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
			}

			auto tempValid = validation::conditional_make_object(valid, "WISE.WeatherProto.HourlyWeather", "hourly");
			auto weatherValid = tempValid.lock();

			//finally, add all of the weather to the stream.
			for (auto w : weather)
			{
				WTime t(streamStartTime);
				t += WTimeSpan(0, 1, 0, 0) * w.hour;
				hour = ((uint_fast64_t)w.hour) % 24;

				if (lastTime.GetTotalSeconds())
				{
					// don't overwrite if we aren't allowed to
					if ((t < lastTime) && (!(options & CWFGM_WEATHERSTREAM_IMPORT_SUPPORT_OVERWRITE)))
					{
						hr = ERROR_WEATHER_STREAM_ATTEMPT_OVERWRITE;
						goto DONE;
					}
					// make sure things are in order in the file, and not missing any data
					if (((!lines) && (t > lastTime)) || ((lines) && (t > (lastTime + WTimeSpan(0, 1, 0, 0)))))
					{
						hr = ERROR_INVALID_TIME | ERROR_SEVERITY_WARNING;
						goto DONE;
					}
				}

				lastTime = t;
				dc = getDCReading(t, can_append);
				if (!dc)
				{
					hr = ERROR_WEATHER_STREAM_ATTEMPT_APPEND;
					goto DONE;
				}
				MakeHourlyObservations(t);
				dc->m_flags |= DAY_ORIGIN_FILE;

				if ((w.options & IWXDATA_SPECIFIED_INTERPOLATED))
					dc->setHourInterpolated(hour);
				else if ((w.options & IWXDATA_SPECIFIED_INVALID_DATA))
				{
					if (weatherValid)
					{
						weatherValid->add_child_validation("WISE.WeatherProto.HourlyWeather", strprintf("hour[%d]", (int)w.hour),
							validation::error_level::SEVERE, validation::id::invalid_weather, t.ToString(WTIME_FORMAT_STRING_ISO8601));
					}
				}
				if (w.DMC >= 0.0)
				{
					if (((lastTime == m_time) && (!w.hour)) || (!lines))
						m_spec_day.dDMC = w.DMC;
					if (hour == noonhour)
						dc->specificDMC(w.DMC);
					m_options |= USER_SPECIFIED;					//turn on user-specified automatically
				}
				if (w.DC >= 0.0)
				{
					if (((lastTime == m_time) && (!w.hour)) || (!lines))
						m_spec_day.dDC = w.DC;
					if (hour == noonhour)
						dc->specificDC(w.DC);
					m_options |= USER_SPECIFIED;					//turn on user-specified automatically
				}
				if (w.BUI >= 0.0)
				{
					if (((lastTime == m_time) && (!w.hour)) || (!lines))
						m_spec_day.dBUI = w.BUI;
					dc->specificBUI(w.BUI);
					m_options |= USER_SPECIFIED;					//turn on user-specified automatically
				}
				if (w.ISI >= 0.0)
				{
					dc->specificISI(t, w.ISI);
					m_options |= USER_SPECIFIED;					//turn on user-specified automatically
				}
				if (w.FWI >= 0.0)
				{
					dc->specificFWI(t, w.FWI);
					m_options |= USER_SPECIFIED;					//turn on user-specified automatically
				}
				if (w.ffmc >= 0.0)
				{
					dc->specificHourlyFFMC(t, w.ffmc);
					m_options |= USER_SPECIFIED;					//turn on user-specified automatically

					if ((noonhour + 4) == w.hour)
					{
						dc->specificDailyFFMC(w.ffmc);
						if (t.GetDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST) == m_time.GetDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST))
						{
							m_initialHFFMC = w.ffmc;
							m_initialHFFMCTime = dayNoon + WTimeSpan(0, 4, 0, 0) - m_time;
						}
					}
				}
				
				m_lastHour = hour;
				dc->setHourlyWeather(t, w.temp, w.rh, w.precip, w.ws, w.wg, w.wd, -300.0);
				lines++;
			}

			m_isCalculatedValuesValid = false;
			calculateValues();
		}
		else
		{
			fclose(f);
			return ERROR_BAD_FILE_TYPE | ERROR_SEVERITY_WARNING;
		}
	}
	else
        return ComError(ENOENT);

DONE:
	fclose(f);

	if (wc)
		delete[] wc;
	m_isCalculatedValuesValid = false;
	return hr;
}


bool WeatherCondition::isSupportedFormat(char *line, std::vector<std::string> &header)
{
	std::string colName;
	std::string source=line;
	int res;
	res=GetWord(&source,&colName);

	while(res!=0)
	{
		header.push_back(colName);
		colName.clear();
		res=GetWord(&source,&colName);
	}
	if (header[0] == "Name" || header[0] == "StationID" || header[0] == "weather_date")
		return true;
	else
		return false;
}

void WeatherCondition::ProcessHeader(TCHAR *line, std::vector<std::string> &header)
{
	std::string colName;
	std::string source=line;
	int res;
	res=GetWord(&source,&colName);

	while(res!=0)
	{
		header.push_back(colName);
		colName.clear();
		res=GetWord(&source,&colName);
	}
}

int WeatherCondition::GetWord(std::string *source, std::string *strWord)
{
	if(source->length()==0)
		return 0;
	char c=(*source)[0];
	while(c==',' || c==' ' || c==';' || c=='\t' || c==0x0a || c==0x0d || c==0x22 ) // last three are CR, LF and "
	{
		*source=source->substr(1, source->length()-1);
		if(source->length()==0)
			return 0;
		c=(*source)[0];
	}
	for(size_t i=0;i<source->length();i++)
	{
		c=(*source)[i];
		if(c!=',' && c!=' ' && c!=';' && c!='\t' && c!=0x0a && c!=0x0d && c!=0x22)
			*strWord+=c;
		else
			break;
	}
	*source=source->substr(strWord->length(), source->length() - strWord->length());
	if(strWord->length()!=0)
		return strWord->length();
	return 0;
}


void WeatherCondition::FillDailyLineValue(std::vector<std::string> &header, char *line, char *file_type,double *min_temp, double *max_temp, double *rh, double *precip, double *min_ws, double *max_ws, double* min_gust, double* max_gust, double *wd)
{
	int i=0;
	char *context;
	char *dat = _tcstok_s(line, _T(", ;\t"), &context);
	strcpy_strip_s(file_type, 40, dat, "\"\'");
	while(dat)
	{
		dat=_tcstok_s(NULL, _T(", ;\t"), &context);
		if(dat==NULL)
			break;

		strcpy_strip_s(dat, _tcslen(dat) + 1, dat, _T("\"\'"));
		double ReadIn=_tstof(dat);
		DistributeDailyValue(header, ++i,ReadIn,min_temp,max_temp,rh,precip,min_ws,max_ws,min_gust,max_gust,wd);
	}
}

void WeatherCondition::DistributeDailyValue(std::vector<std::string> &header, int index, double value, double *min_temp, double *max_temp, double *rh, double *precip, double *min_ws, double *max_ws, double *min_gust, double *max_gust, double *wd)
{
HSS_PRAGMA_WARNING_PUSH
HSS_PRAGMA_GCC(GCC diagnostic ignored "-Wsign-compare")
	if(index > header.size()-1 || index<0)
		return;
HSS_PRAGMA_WARNING_POP
	std::string str = header[index];
	if (boost::iequals(str, _T("min_temp")))
		*min_temp=value;
	else if (boost::iequals(str, _T("max_temp")))
		*max_temp=value;
	else if ((boost::iequals(str, _T("rh")))  || (boost::iequals(str, _T("min_rh"))) || boost::iequals(str, _T("relative_humidity")))
		*rh=value;
	else if (boost::iequals(str, _T("wd")) || boost::iequals(str, _T("dir")) || boost::iequals(str, _T("wind_direction")))
		*wd=value;
	else if (boost::iequals(str, _T("min_ws")))
		*min_ws=value;
	else if (boost::iequals(str, _T("max_ws")))
		*max_ws=value;
	else if (boost::iequals(str, _T("min_gust")))
		*min_gust = value;
	else if (boost::iequals(str, _T("max_gust")))
		*max_gust = value;
	else if ((boost::iequals(str, _T("precip"))) || (boost::iequals(str, _T("rain"))) || boost::iequals(str, _T("precipitation")))
		*precip=value;
}


void WeatherCondition::GetEventTime(std::uint32_t flags, const WTime &from_time, WTime &next_event) {
	DailyCondition *dc = getDCReading(from_time, false);
	if (dc) {
		calculateValues();
		dc->GetEventTime(flags, from_time, next_event);
	}
}


HRESULT WeatherCondition::SetValidTimeRange(const HSS_Time::WTime& start, const HSS_Time::WTimeSpan& duration, const bool correctInitialPrecip) {
	if (correctInitialPrecip)
		calculateValues();

	WTimeSpan d;
	if (NumDays())
		d = WTimeSpan(NumDays(), -(23 - m_lastHour) - m_firstHour, 0, 0);
	else
		d = WTimeSpan(0);

	if (duration < WTimeSpan(0))
		return E_INVALIDARG;
	if (start < (m_time + WTimeSpan(0, m_firstHour, 0, 0)))
		return E_INVALIDARG;				// can't prepend data
	if ((start + duration) > (m_time + d))
		return E_INVALIDARG;				// can't append data
	if ((start == (m_time + WTimeSpan(0, m_firstHour, 0, 0))) &&
		(start + duration) == (m_time + d))
		return S_OK;						// no change

	WTime ds(start), dsl(start);
	ds.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	dsl.PurgeToDay(WTIME_FORMAT_AS_LOCAL);

	int phr;
	if (dsl != ds)
		phr = 13;
	else
		phr = 12;


	WTimeSpan hr = start - ds;
	d = duration;
	d += WTimeSpan(0, m_firstHour, 0, 0);
	m_firstHour = hr.GetHours();
	m_lastHour = d.GetHours();

	double precip;
	while (ds > (m_time + WTimeSpan(0, 23, 0, 0))) {
		m_time += WTimeSpan(1, 0, 0, 0);
		DailyCondition* dc = m_readings.RemHead();
		if (correctInitialPrecip) {
			precip = 0.0;
			for (int i = phr; i < 24; i++)
				precip += dc->hourlyPrecip(m_time + WTimeSpan(0, i, 0, 0));
		}
		delete dc;
	}

	int days = d.GetDays();
	DailyCondition* dc = m_readings.IndexNode(days);
	if (dc)
		while (m_readings.LH_Tail() != dc) {
			DailyCondition* dc = m_readings.RemTail();
			delete dc;
		}

	if (correctInitialPrecip) {
		for (int i = 0; i <= m_firstHour; i++)
			precip += m_readings.LH_Head()->hourlyPrecip(m_time + WTimeSpan(0, i, 0, 0));
		m_initialRain = 0.0;
		m_readings.LH_Head()->setHourlyPrecip(m_time + WTimeSpan(0, m_firstHour, 0, 0), precip);
	}

	return S_OK;
}


std::int32_t WeatherCondition::serialVersionUid(const SerializeProtoOptions& options) const noexcept {
	return 1;
}


WISE::WeatherProto::WeatherStream* WeatherCondition::serialize(const SerializeProtoOptions& options) {
	auto stream = new WISE::WeatherProto::WeatherStream;
	stream->set_version(serialVersionUid(options));
	if (m_firstHour != 0)
		stream->set_allocated_starthour(createProtobufObject((int32_t)m_firstHour));

	if (m_lastHour != 23)
		stream->set_allocated_endhour(createProtobufObject((int32_t)m_lastHour));

	stream->set_dataimportedfromfile(m_options & FROM_FILE);
	if (m_options & FROM_ENSEMBLE)
		stream->set_allocated_dataimportedfromensemble(createProtobufObject((m_options & FROM_ENSEMBLE) ? true : false));
	stream->set_allocated_hffmcusespecified(createProtobufObject((m_options & USER_SPECIFIED) ? true : false));

	WTime lstTime(m_time, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, 1);
	WTime otime(lstTime, nullptr);
	stream->set_allocated_starttime(HSS_Time::Serialization::TimeSerializer().serializeTime(otime, options.fileVersion()));
	if (m_initialHFFMCTime.GetTotalSeconds() >= 0)
		stream->set_allocated_hffmctime(HSS_Time::Serialization::TimeSerializer().serializeTimeSpan(m_initialHFFMCTime));
	stream->set_allocated_hffmc(DoubleBuilder().withValue(m_initialHFFMC).forProtobuf(options.useVerboseFloats()));
	switch (m_options & FFMC_MASK)
	{
	case FFMC_LAWSON:
		stream->set_hffmcmethod(WISE::WeatherProto::WeatherStream_FFMCMethod_LAWSON);
		break;
	case FFMC_VAN_WAGNER:
	default:
		stream->set_hffmcmethod(WISE::WeatherProto::WeatherStream_FFMCMethod_VAN_WAGNER);
		break;
	}

	auto temps = new WISE::WeatherProto::WeatherStream_ABC();
	temps->set_allocated_alpha(DoubleBuilder().withValue(m_temp_alpha).forProtobuf(options.useVerboseFloats()));
	temps->set_allocated_beta(DoubleBuilder().withValue(m_temp_beta).forProtobuf(options.useVerboseFloats()));
	temps->set_allocated_gamma(DoubleBuilder().withValue(m_temp_gamma).forProtobuf(options.useVerboseFloats()));
	stream->set_allocated_temperature(temps);

	auto winds = new WISE::WeatherProto::WeatherStream_ABC();
	winds->set_allocated_alpha(DoubleBuilder().withValue(m_wind_alpha).forProtobuf(options.useVerboseFloats()));
	winds->set_allocated_beta(DoubleBuilder().withValue(m_wind_beta).forProtobuf(options.useVerboseFloats()));
	winds->set_allocated_gamma(DoubleBuilder().withValue(m_wind_gamma).forProtobuf(options.useVerboseFloats()));
	stream->set_allocated_wind(winds);

	auto start = new WISE::WeatherProto::WeatherStream_StartingCodes();
	start->set_allocated_ffmc(DoubleBuilder().withValue(m_spec_day.dFFMC).forProtobuf(options.useVerboseFloats()));
	start->set_allocated_dmc(DoubleBuilder().withValue(m_spec_day.dDMC).forProtobuf(options.useVerboseFloats()));
	start->set_allocated_dc(DoubleBuilder().withValue(m_spec_day.dDC).forProtobuf(options.useVerboseFloats()));
	start->set_allocated_bui(DoubleBuilder().withValue(m_spec_day.dBUI).forProtobuf(options.useVerboseFloats()));
	start->set_allocated_precipitation(DoubleBuilder().withValue(m_initialRain).forProtobuf(options.useVerboseFloats()));
	stream->set_allocated_startingcodes(start);

	auto conditions = new WISE::WeatherProto::WeatherStream_ConditionList();
	auto dc = m_readings.LH_Head();
	while (dc->LN_Succ())
	{
		auto serialized = dc->serialize(options);
		auto created = conditions->add_dailyconditions();
		created->Swap(serialized);
		delete serialized;
		dc = dc->LN_Succ();
	}
	stream->set_allocated_dailyconditions(conditions);

	return stream;
}

WeatherCondition* WeatherCondition::deserialize(const google::protobuf::Message& proto, std::shared_ptr<validation::validation_object> valid, const std::string& name)
{
	auto conditions = dynamic_cast_assert<const WISE::WeatherProto::WeatherStream*>(&proto);

	if (!conditions)
	{
		if (valid)
			/// <summary>
			/// The object passed as a weather stream is invalid. An incorrect object type was passed to the parser.
			/// </summary>
			/// <type>internal</type>
			valid->add_child_validation("WISE.WeatherProto.WeatherStream", name, validation::error_level::SEVERE,
				validation::id::object_invalid, proto.GetDescriptor()->name());
		weak_assert(false);
		throw ISerializeProto::DeserializeError("Error: WISE.WeatherProto.WeatherCondition: Protobuf object invalid", ERROR_PROTOBUF_OBJECT_INVALID);
	}

	if (conditions->version() != 1)
	{
		if (valid)
			/// <summary>
			/// The object version is not supported. The weather stream is not supported by this version of Prometheus.
			/// </summary>
			/// <type>user</type>
			valid->add_child_validation("WISE.WeatherProto.WeatherStream", name, validation::error_level::SEVERE,
				validation::id::version_mismatch, std::to_string(conditions->version()));
		weak_assert(false);
		throw ISerializeProto::DeserializeError("Error: WISE.WeatherProto.WeatherCondition: Version is invalid", ERROR_PROTOBUF_OBJECT_VERSION_INVALID);
	}

	/// <summary>
	/// Child validations for a weather stream.
	/// </summary>
	auto vt = validation::conditional_make_object(valid, "WISE.WeatherProto.WeatherStream", name);
	auto myValid = vt.lock();

	if (conditions->has_starthour()) {
		m_firstHour = conditions->starthour().value();
		if ((m_firstHour > 23) && (myValid))
			myValid->add_child_validation("int32", "startHour", validation::error_level::WARNING, validation::id::value_invalid, std::to_string(m_firstHour), { true, 0 }, { true, 32 });
	}
	if (conditions->has_endhour()) {
		m_lastHour = conditions->endhour().value();
		if ((m_lastHour > 23) && (myValid))
			myValid->add_child_validation("int32", "endHour", validation::error_level::WARNING, validation::id::value_invalid, std::to_string(m_lastHour), { true, 0 }, { true, 32 });
	}

	if (conditions->has_starttime())
	{
		auto time = HSS_Time::Serialization::TimeSerializer().deserializeTime(conditions->starttime(), nullptr, myValid, "startTime");
		if (time) {
			WTime lstTime(*time, &m_timeManager);
			m_time = HSS_Time::WTime(lstTime, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, -1);

			delete time;
		}
	}

	if (conditions->has_hffmctime())
	{
		auto timespan = HSS_Time::Serialization::TimeSerializer().deserializeTimeSpan(conditions->hffmctime(), myValid, "hffmcTime");
		m_initialHFFMCTime = HSS_Time::WTimeSpan(*timespan);
		delete timespan;
	}

	double dValue = DoubleBuilder().withProtobuf(conditions->hffmc()).getValue();
	if (dValue < 0.0 || dValue > 101.0)
	{
		if (myValid)
			myValid->add_child_validation("WISE.WeatherProto.WeatherStream", "hffmc", validation::error_level::SEVERE,
				validation::id::ffmc_invalid, std::to_string(dValue),
				{ true, 0.0 }, { true, 101.0 });
		throw std::invalid_argument("Error: WISE.WeatherProto.WeatherCondition: Invalid hourly FFMC value");
	}
	m_initialHFFMC = dValue;

	m_options = (m_options & ~FFMC_MASK) | (conditions->hffmcmethod() + 1);

	if (conditions->has_temperature())
	{
		auto vt2 = validation::conditional_make_object(myValid, "WISE.WeatherProto.WeatherStream.ABC", "temperature");
		auto myValid2 = vt2.lock();

		m_temp_alpha = DoubleBuilder().withProtobuf(conditions->temperature().alpha(), myValid2, "alpha").getValue();
		m_temp_beta = DoubleBuilder().withProtobuf(conditions->temperature().beta(), myValid2, "beta").getValue();
		m_temp_gamma = DoubleBuilder().withProtobuf(conditions->temperature().gamma(), myValid2, "gamma").getValue();
	}

	if (conditions->has_wind())
	{
		auto vt2 = validation::conditional_make_object(myValid, "WISE.WeatherProto.WeatherStream.ABC", "wind");
		auto myValid2 = vt2.lock();

		m_wind_alpha = DoubleBuilder().withProtobuf(conditions->wind().alpha(), myValid2, "alpha").getValue();
		m_wind_beta = DoubleBuilder().withProtobuf(conditions->wind().beta(), myValid2, "beta").getValue();
		m_wind_gamma = DoubleBuilder().withProtobuf(conditions->wind().gamma(), myValid2, "gamma").getValue();
	}

	if (conditions->has_startingcodes())
	{
		auto vt2 = validation::conditional_make_object(myValid, "WISE.WeatherProto.WeatherStream.StartingCodes", "startingCodes");
		auto myValid2 = vt2.lock();

		dValue = DoubleBuilder().withProtobuf(conditions->startingcodes().ffmc(), myValid, "ffmc").getValue();
		if (dValue < 0.0 || dValue > 101.0)
		{
			if (myValid2)
				myValid2->add_child_validation("Math.Double", "ffmc", validation::error_level::SEVERE,
					validation::id::ffmc_invalid, std::to_string(dValue),
					{ true, 0.0 }, { true, 101.0 });
			else
				throw std::invalid_argument("Error: WISE.WeatherProto.WeatherCondition: Invalid daily FFMC value");
		}
		m_spec_day.dFFMC = dValue;

		dValue = DoubleBuilder().withProtobuf(conditions->startingcodes().dmc(), myValid2, "dmc").getValue();
		if (dValue < 0.0 || dValue > 500.0)
		{
			if (myValid2)
				myValid2->add_child_validation("Math.Double", "dmc", validation::error_level::SEVERE,
					validation::id::dmc_invalid, std::to_string(dValue),
					{ true, 0.0 }, { true, 500.0 });
			else
				throw std::invalid_argument("Error: WISE.WeatherProto.WeatherCondition: Invalid DMC value");
		}
		m_spec_day.dDMC = dValue;

		dValue = DoubleBuilder().withProtobuf(conditions->startingcodes().dc(), myValid2, "dc").getValue();
		if (dValue < 0.0 || dValue > 1500.0)
		{
			if (myValid2)
				myValid2->add_child_validation("Math.Double", "dc", validation::error_level::SEVERE,
					validation::id::dc_invalid, std::to_string(dValue),
					{ true, 0.0 }, { true, 1500.0 });
			else
				throw std::invalid_argument("Error: WISE.WeatherProto.WeatherCondition: Invalid DC value");
		}
		m_spec_day.dDC = dValue;

		if (conditions->startingcodes().has_bui()) {
			dValue = DoubleBuilder().withProtobuf(conditions->startingcodes().bui(), myValid2, "bui").getValue();
			if ((dValue < 0.0) && (dValue != -99.0) && (dValue != -1.0))
			{
				if (myValid2)
					myValid2->add_child_validation("Math.Double", "bui", validation::error_level::SEVERE,
						validation::id::bui_invalid, std::to_string(dValue),
						{ true, 0.0 }, { false, std::numeric_limits<double>::infinity() });
				else
					throw std::invalid_argument("Error: WISE.WeatherProto.WeatherCondition: Invalid BUI value");
			}
			m_spec_day.dBUI = dValue;
		}

		if (conditions->startingcodes().has_precipitation())
			m_initialRain = DoubleBuilder().withProtobuf(conditions->startingcodes().precipitation(), myValid2, "startingcodes.precip").getValue();
		else
			m_initialRain = 0.0;
	}

	if (conditions->dataimportedfromfile())
		m_options |= FROM_FILE;
	if (conditions->has_dataimportedfromensemble())
		if (conditions->dataimportedfromensemble().value())
			m_options |= FROM_ENSEMBLE;
	if (conditions->has_hffmcusespecified() && conditions->hffmcusespecified().value())
		m_options |= USER_SPECIFIED;

	if (conditions->data_case() == WISE::WeatherProto::WeatherStream::kDailyConditions)
	{
		for (int i = 0; i < conditions->dailyconditions().dailyconditions_size(); i++)
		{
			auto day = conditions->dailyconditions().dailyconditions(i);

			auto deserialized = new DailyCondition(this);
			m_readings.AddTail(deserialized);
			if (!deserialized->deserialize(day, myValid, strprintf("dailyconditions[%d]", i), (i == 0) ? m_firstHour : 0, (i == (conditions->dailyconditions().dailyconditions_size() - 1) ? m_lastHour : 23)))
				throw std::invalid_argument("Error: WISE.WeatherProto.WeatherCondition: Incomplete initialization");
		}
	}
	else if (conditions->data_case() == WISE::WeatherProto::WeatherStream::kFilename)
	{
		if (fs::exists(fs::relative(conditions->filename())))
		{
			auto vt2 = validation::conditional_make_object(myValid, "WISE.WeatherProto.WeatherStream.filename", "import");
			auto myValid2 = vt2.lock();

			HRESULT hr = Import(conditions->filename().c_str(), CWFGM_WEATHERSTREAM_IMPORT_PURGE, myValid2);
			if (FAILED(hr) || hr == ERROR_INVALID_DATA)
				throw ISerializeProto::DeserializeError((boost::format("The import weather stream operation has failed. Unable to import \"%1%\"")
					% conditions->filename()).str());
		}
		else if (myValid)
			/// <summary>
			/// The specified weather stream file could not be found on the filesystem.
			/// </summary>
			/// <type>user</type>
			myValid->add_child_validation("WISE.WeatherProto.WeatherCondition", "file", validation::error_level::WARNING,
				validation::id::missing_file, conditions->filename());
	}

	if (!m_time.GetTotalMicroSeconds())
	{
		if (myValid)
			/// <summary>
			/// No weather data was found associated with this set of daily conditions.
			/// </summary>
			/// <type>internal</type>
			myValid->add_child_validation("WISE.WeatherProto.WeatherCondition", "stream", validation::error_level::WARNING,
				validation::id::missing_weather_data, "stream");
		else
			throw ISerializeProto::DeserializeError((boost::format("The import weather stream operation has failed. Unable to import \"%1%\"") % conditions->filename()).str());
	}
	if (m_time.GetMicroSeconds(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)) {
		if (myValid)
			/// <summary>
			/// The start time contains fractions of seconds.
			/// </summary>
			/// <type>user</type>
			myValid->add_child_validation("HSS.Times.WTime", "startTime", validation::error_level::WARNING, validation::id::time_invalid, m_time.ToString(WTIME_FORMAT_STRING_ISO8601), "Fractions of seconds will be purged.");
		m_time.PurgeToSecond(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	}

	if (m_readings.GetCount() == 1) {
		if (m_firstHour >= m_lastHour) {
			if (myValid)
				myValid->add_child_validation("int32", "startHour:endHour", validation::error_level::WARNING, validation::id::value_invalid, std::to_string(m_lastHour), "Condition not met: startHour < endHour");
			else
				throw ISerializeProto::DeserializeError("The import weather stream operation has failed - start time is after end time.");
		}
	}

	return this;
}
