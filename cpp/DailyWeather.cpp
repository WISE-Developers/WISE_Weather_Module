/**
 * WISE_Weather_Module: DailyWeather.h
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

#include "WeatherStream.h"
#include "DayCondition.h"
#include "GridCom_ext.h"
#include <fstream>


#ifdef DEBUG
#include <assert.h>
#endif


DailyWeather::DailyWeather(WeatherCondition *wc)
    : m_DayStart((std::uint64_t)0, &wc->m_timeManager),
      m_SunRise((std::uint64_t)0, &wc->m_timeManager),
      m_SolarNoon((std::uint64_t)0, &wc->m_timeManager),
      m_SunSet((std::uint64_t)0, &wc->m_timeManager),
      m_calc_tn((std::uint64_t)0, &wc->m_timeManager),
      m_calc_tx((std::uint64_t)0, &wc->m_timeManager),
      m_calc_tu((std::uint64_t)0, &wc->m_timeManager),
      m_calc_ts((std::uint64_t)0, &wc->m_timeManager) {
	m_weatherCondition = wc;

	m_flags = 0;
	m_daily_min_temp = m_daily_max_temp = m_daily_min_ws = m_daily_max_ws = m_daily_min_gust = m_daily_max_gust = m_daily_rh = m_daily_precip = 0.0;
	m_daily_wd = 0.0;

	for (std::uint16_t i = 0; i < 24; i++) {
		m_hourly_temp[i] = m_hourly_dewpt_temp[i] = m_hourly_rh[i] = m_hourly_ws[i] = m_hourly_gust[i] = m_hourly_precip[i] = 0.0;
		m_hourly_wd[i] = 0.0;
		m_hflags[i] = 0;
	}

	m_dblTempDiff = 0.0;
}


DailyWeather::DailyWeather(const DailyWeather &toCopy, WeatherCondition *wc)
    : m_DayStart((std::uint64_t)0, &wc->m_timeManager),
      m_SunRise((std::uint64_t)0, &wc->m_timeManager),
      m_SolarNoon((std::uint64_t)0, &wc->m_timeManager),
      m_SunSet((std::uint64_t)0, &wc->m_timeManager),
      m_calc_tn((std::uint64_t)0, &wc->m_timeManager),
      m_calc_tx((std::uint64_t)0, &wc->m_timeManager),
      m_calc_tu((std::uint64_t)0, &wc->m_timeManager),
      m_calc_ts((std::uint64_t)0, &wc->m_timeManager) {
	m_weatherCondition = wc;

	m_DayStart.SetTime(toCopy.m_DayStart);
	m_SunRise.SetTime(toCopy.m_SunRise);
	m_SunSet.SetTime(toCopy.m_SunSet);
	m_SolarNoon.SetTime(toCopy.m_SolarNoon);

	m_flags = toCopy.m_flags;
	if (!(m_flags & DAY_HOURLY_SPECIFIED)) {
		m_daily_min_temp = toCopy.m_daily_min_temp;
		m_daily_max_temp = toCopy.m_daily_max_temp;
		m_daily_min_ws = toCopy.m_daily_min_ws;
		m_daily_max_ws = toCopy.m_daily_max_ws;
		if (m_flags & DAY_GUST_SPECIFIED) {
			m_daily_min_gust = toCopy.m_daily_min_gust;
			m_daily_max_gust = toCopy.m_daily_max_gust;
		}
		m_daily_rh = toCopy.m_daily_rh;
		m_daily_precip = toCopy.m_daily_precip;
		m_daily_wd = toCopy.m_daily_wd;
	}
	else {
		for (std::uint16_t i = 0; i < 24; i++) {
			m_hourly_temp[i] = toCopy.m_hourly_temp[i];
			m_hourly_rh[i] = toCopy.m_hourly_rh[i];
			m_hourly_ws[i] = toCopy.m_hourly_ws[i];
			if (m_hflags[i] & HOUR_GUST_SPECIFIED)
				m_hourly_gust[i] = toCopy.m_hourly_gust[i];
			m_hourly_precip[i] = toCopy.m_hourly_precip[i];
			m_hourly_wd[i] = toCopy.m_hourly_wd[i];
			m_hflags[i] = toCopy.m_hflags[i];
			if (m_hflags[i] & HOUR_DEWPT_SPECIFIED)
				m_hourly_dewpt_temp[i] = toCopy.m_hourly_dewpt_temp[i];
		}
	}
}


void DailyWeather::GetEventTime(std::uint32_t flags, const WTime &from_time, WTime &next_event, bool look_ahead) {
	WTimeSpan time_of_day = from_time.GetTimeOfDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);

	if (flags & CWFGM_GETEVENTTIME_FLAG_SEARCH_BACKWARD) {
		WTime day(from_time);
		day.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
		if (day != from_time) {	// then we aren't at the start of the day, so we can knock off an hour and we're done
			WTime time(from_time);
			time.PurgeToHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
			if (time == from_time)
				time -= WTimeSpan(0, 1, 0, 0);
			if (next_event < time)
				next_event = time;
		} else {
			DailyWeather *yesterday = getYesterday();
			if (yesterday)					// we have a prior day in the list so ask it
				yesterday->GetEventTime(flags, day - WTimeSpan(1), next_event, true);
		}							// otherwise, we're at the start of the stream so can't really go back in time to no event
	} else {
		if (look_ahead) {
			weak_assert(time_of_day == WTimeSpan(0, 23, 59, 59));	// we're being asked for the first event of the day
			if (next_event > m_DayStart)
				next_event = m_DayStart;
		} else if (time_of_day.GetHours() == 23) {			// we're being asked about hour 23 and the next event (we know) will be from the
			WTime day(from_time);					// next day
			day.PurgeToDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
			day += WTimeSpan(0, 23, 59, 59);
			DailyWeather *tomorrow = getTomorrow();
			if (tomorrow)
				tomorrow->GetEventTime(flags, day, next_event, true);	// let 'tomorrow' determine the next event
			else if (next_event > day)
				next_event = day + WTimeSpan(1);		// mark it as the last second of this day
		} else {							// we're asked for the next hour boundary - pretty easy
			WTime day(from_time);
			day.PurgeToHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
			day += WTimeSpan(0, 1, 0, 0);
			if (next_event > day)
				next_event = day;
		}
	}
}


double DailyWeather::dailyMinTemp() const {
	if (!(m_flags & DAY_HOURLY_SPECIFIED))		return m_daily_min_temp;
	uint8_t i = m_weatherCondition->firstHourOfDay(m_DayStart);
	uint8_t j = m_weatherCondition->lastHourOfDay(m_DayStart);
	double t = m_hourly_temp[i];
	for (i++; i <= j; i++)
		if (t > m_hourly_temp[i])
			t = m_hourly_temp[i];
	return t;
}


double DailyWeather::dailyMeanTemp() const {
	uint8_t start = m_weatherCondition->firstHourOfDay(m_DayStart);
	uint8_t j = m_weatherCondition->lastHourOfDay(m_DayStart);
	uint8_t i = start;
	double t = m_hourly_temp[i];
	for (i++; i <= j; i++)
		t += m_hourly_temp[i];
	return t / ((double)(j - start + 1));
}


double DailyWeather::dailyMaxTemp() const {
	if (!(m_flags & DAY_HOURLY_SPECIFIED))		return m_daily_max_temp;
	uint8_t i = m_weatherCondition->firstHourOfDay(m_DayStart);
	uint8_t j = m_weatherCondition->lastHourOfDay(m_DayStart);
	double t = m_hourly_temp[i];
	for (i++; i <= j; i++)
		if (t < m_hourly_temp[i])
			t = m_hourly_temp[i];
	return t;
}


double DailyWeather::dailyMinWS() const {
	if (!(m_flags & DAY_HOURLY_SPECIFIED))		return m_daily_min_ws;
	uint8_t i = m_weatherCondition->firstHourOfDay(m_DayStart);
	uint8_t j = m_weatherCondition->lastHourOfDay(m_DayStart);
	double t = m_hourly_ws[i];
	for (i++; i <= j; i++)
		if (t > m_hourly_ws[i])
			t = m_hourly_ws[i];
	return t;
}


double DailyWeather::dailyMaxWS() const {
	if (!(m_flags & DAY_HOURLY_SPECIFIED))		return m_daily_max_ws;
	uint8_t i = m_weatherCondition->firstHourOfDay(m_DayStart);
	uint8_t j = m_weatherCondition->lastHourOfDay(m_DayStart);
	double t = m_hourly_ws[i];
	for (i++; i <= j; i++)
		if (t < m_hourly_ws[i])
			t = m_hourly_ws[i];
	return t;
}


double DailyWeather::dailyMinGust() const {
	if (!(m_flags & DAY_HOURLY_SPECIFIED)) {
		if (m_flags & DAY_GUST_SPECIFIED)
			return m_daily_min_gust;
		return -1.0;
	}

	uint8_t i = m_weatherCondition->firstHourOfDay(m_DayStart);
	uint8_t j = m_weatherCondition->lastHourOfDay(m_DayStart);
	double t = -1.0;
	m_hourly_gust[i];
	for (i++; i <= j; i++)
		if ((t == -1.0) || (t > m_hourly_gust[i]))
			if (m_hflags[i] & HOUR_GUST_SPECIFIED)
				t = m_hourly_gust[i];
	return t;
}


double DailyWeather::dailyMaxGust() const {
	if (!(m_flags & DAY_HOURLY_SPECIFIED)) {
		if (m_flags & DAY_GUST_SPECIFIED)
			return m_daily_max_gust;
		return -1.0;
	}

	uint8_t i = m_weatherCondition->firstHourOfDay(m_DayStart);
	uint8_t j = m_weatherCondition->lastHourOfDay(m_DayStart);
	double t = -1.0;
	m_hourly_gust[i];
	for (i++; i <= j; i++)
		if ((t == -1.0) || (t < m_hourly_gust[i]))
			if (m_hflags[i] & HOUR_GUST_SPECIFIED)
				t = m_hourly_gust[i];
	return t;
}


double DailyWeather::dailyMinRH() const {
	if (!(m_flags & DAY_HOURLY_SPECIFIED))		return m_daily_rh;
	uint8_t i = m_weatherCondition->firstHourOfDay(m_DayStart);
	uint8_t j = m_weatherCondition->lastHourOfDay(m_DayStart);
	double rh = m_hourly_rh[i];
	for (i++; i <= j; i++)
		if (rh > m_hourly_rh[i])
			rh = m_hourly_rh[i];
	return rh;
}


double DailyWeather::dailyMeanRH() const {
	if (!(m_flags & DAY_HOURLY_SPECIFIED))		return m_daily_rh;
	uint8_t start = m_weatherCondition->firstHourOfDay(m_DayStart);
	uint8_t j = m_weatherCondition->lastHourOfDay(m_DayStart);
	double rh = 0.0;
	for (int i = start; i <= j; i++)
		rh += m_hourly_rh[i];
	return rh / ((double)(j - start + 1));
}


double DailyWeather::dailyMaxRH() const {
	if (!(m_flags & DAY_HOURLY_SPECIFIED))		return m_daily_rh;
	uint8_t i = m_weatherCondition->firstHourOfDay(m_DayStart);
	uint8_t j = m_weatherCondition->lastHourOfDay(m_DayStart);
	double rh = m_hourly_rh[i];
	for (i++; i <= j; i++)
		if (rh < m_hourly_rh[i])
			rh = m_hourly_rh[i];
	return rh;
}


double DailyWeather::dailyPrecip() const {
	if (!(m_flags & DAY_HOURLY_SPECIFIED))		return m_daily_precip;

	const WTime dayNeutral(m_DayStart, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, 1);	// convert to a timezone-neutral time
	const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL, -1);				// gets us the start of the true LST day
	WTime dayNoon(dayLST);
	dayNoon += WTimeSpan(0, 12, 0, 0);

	WTime begin(m_weatherCondition->m_time + WTimeSpan(0, m_weatherCondition->m_firstHour, 0, 0));
	WTime end(m_weatherCondition->m_time + WTimeSpan(m_weatherCondition->m_readings.GetCount() - 1, m_weatherCondition->m_lastHour, 0, 0));

	double rain;
	if (!getYesterday()) {
		rain = m_weatherCondition->m_initialRain;
		WTime loop(m_DayStart);
		if (loop < begin)
			loop = begin;
		if (dayNoon > end)
			dayNoon = end;
		while (loop <= dayNoon) {
			rain += m_weatherCondition->GetHourlyRain(loop);
			loop += WTimeSpan(0, 1, 0, 0);
		}
	}
	else {
		rain = 0.0;
		WTime loop(dayNoon);
		loop -= WTimeSpan(0, 23, 0, 0);
		if (loop < begin)
			loop = begin;
		if (dayNoon > end)
			dayNoon = end;
		for (; loop <= dayNoon; loop += WTimeSpan(0, 1, 0, 0))
			rain += m_weatherCondition->GetHourlyRain(loop);
	}
	return rain;
}


double DailyWeather::dailyWD() const {
	if (!(m_flags & DAY_HOURLY_SPECIFIED))		return m_daily_wd;
	WTime t(m_DayStart);
	t += WTimeSpan(0, 12, 0, 0);
	int endhour = m_weatherCondition->lastHourOfDay(t);
	long th = t.GetHour(WTIME_FORMAT_AS_LOCAL);
	if (th > endhour)
		th = endhour;
	return m_hourly_wd[th];
}


bool DailyWeather::setHourlyWeather(const WTime& time, double temp, double rh, double precip, double ws, double gust, double wd, double dew) {
	if (!(m_flags & DAY_HOURLY_SPECIFIED))
		return false;
	std::int32_t hour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	return setHourlyWeather(hour, temp, rh, precip, ws, gust, wd, dew);
};


bool DailyWeather::setHourlyPrecip(const WTime& time,  double precip) {
	if (!(m_flags & DAY_HOURLY_SPECIFIED))
		return false;
	std::int32_t hour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	return setHourlyPrecip(hour, precip);
};


bool DailyWeather::setHourlyWeather(const std::int32_t hour, double temp, double rh, double precip, double ws, double gust, double wd, double dew) {
	if (!(m_flags & DAY_HOURLY_SPECIFIED))
		return false;
	m_hourly_temp[hour] = (float)temp;
	m_hourly_rh[hour] = (float)rh;
	m_hourly_precip[hour] = (float)precip;
	m_hourly_ws[hour] = (float)ws;

	if (gust >= 0.0) {
		m_hourly_gust[hour] = (float)gust;
		m_hflags[hour] |= HOUR_GUST_SPECIFIED;
	} else
		m_hflags[hour] &= (~(HOUR_GUST_SPECIFIED));

	m_hourly_wd[hour] = wd;

	if (dew > -300.0) {
		m_hourly_dewpt_temp[hour] = (float)dew;
		m_hflags[hour] |= HOUR_DEWPT_SPECIFIED;
	}
	else
		m_hflags[hour] &= (~(HOUR_DEWPT_SPECIFIED));

	return true;
}


bool DailyWeather::setHourlyPrecip(const std::int32_t hour, double precip) {
	if (!(m_flags & DAY_HOURLY_SPECIFIED))
		return false;
	m_hourly_precip[hour] = (float)precip;
	return true;
}


bool DailyWeather::calculateTimes(std::uint16_t i) {
	m_DayStart = m_weatherCondition->m_time;
	m_DayStart += WTimeSpan(i, 0, 0, 0);

	WTime t(m_DayStart);
	t += WTimeSpan(0, 12, 0, 0);

	std::int16_t success = m_weatherCondition->m_worldLocation.m_sun_rise_set(t, &m_SunRise, &m_SunSet, &m_SolarNoon);
	if (success & NO_SUNRISE)
		m_SunRise = m_DayStart;
	if (success & NO_SUNSET)
		m_SunSet = m_DayStart + WTimeSpan(0, 23, 59, 59);
	if ((m_SunSet - m_DayStart) >= WTimeSpan(1, 0, 0, 0))
		return false;					// sun set is more than 24 hours away from start of day so indicates a bad time zone
	return true;
}


void DailyWeather::calculateDailyConditions() {
	if (m_flags & DAY_HOURLY_SPECIFIED/*m_isHourlySpecified*/) {
		m_daily_min_temp = (float)dailyMinTemp();
		m_daily_max_temp = (float)dailyMaxTemp();
		m_daily_min_ws = (float)dailyMinWS();
		m_daily_max_ws = (float)dailyMaxWS();
		m_daily_min_gust = (float)dailyMinGust();
		m_daily_max_gust = (float)dailyMaxGust();
		if ((m_daily_min_gust >= 0.0) && (m_daily_max_gust > 0.0))
			m_flags |= DAY_GUST_SPECIFIED;
		m_daily_rh = (float)dailyMeanRH();
		m_daily_precip = (float)dailyPrecip();
		m_daily_wd = (float)dailyWD();
	}
}


bool DailyWeather::calculateHourlyConditions() {
	if (!(m_flags & DAY_HOURLY_SPECIFIED)) {

    #ifdef DEBUG
		weak_assert(!m_DayStart.GetTimeOfDay(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST).GetTotalSeconds());
    #endif
									// calcultaes hourly readings for a given day from daily readings
									// daily values are expected to have been set, as are all alpha, beta and gamma values

		calculateWD();
		calculatePrecip();
		std::uint16_t lastTemp = calculateTemp();
		calculateRH();
		std::uint16_t lastWS = calculateWS();
		std::uint16_t lastGust = calculateGust();

		if (!(LN_Succ()->LN_Succ())) {					// if we're the last then just pad out the values,
			std::uint16_t i;
			for (i = lastTemp; i < 24; i++) {			// this is easier for now than trying to record what
				m_hourly_temp[i] = m_hourly_temp[lastTemp - 1];	// the last valid hour for a part of a day is
				m_hourly_rh[i] = m_hourly_rh[lastTemp - 1];
			}
			for (i = lastWS; i < 24; i++)
				m_hourly_ws[i] = m_hourly_ws[lastWS - 1];
			if (lastGust != (std::uint16_t)-1)
				for (i = lastWS; i < 24; i++)
					m_hourly_gust[i] = m_hourly_gust[lastGust - 1];
		}
	}
	return true;

}


void DailyWeather::calculateRemainingHourlyConditions() {
	calculateDewPtTemp();
}


void DailyWeather::calculateDewPtTemp() {
	uint8_t i = m_weatherCondition->firstHourOfDay(m_DayStart);
	uint8_t j = m_weatherCondition->lastHourOfDay(m_DayStart);
	for (; i <= j; i++) {
		if (!(m_hflags[i] & HOUR_DEWPT_SPECIFIED)) {
			double VPs = 0.6112 * pow(10.0, 7.5 * m_hourly_temp[i] / (237.7 + m_hourly_temp[i]));
			double VP = m_hourly_rh[i] * VPs;
			if (VP > 0.0)
				m_hourly_dewpt_temp[i] = 237.7 * log10(VP / 0.6112) / (7.5 - log10(VP / 0.6112));
			else	m_hourly_dewpt_temp[i] = -273.0;
		}
	}
}


void DailyWeather::calculateWD() {
	for (std::uint8_t i = 0; i < 24; i++)
        	m_hourly_wd[i] = m_daily_wd;
}


void DailyWeather::calculatePrecip() {
	const WTime dayNeutral(m_DayStart, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, 1);	// convert to a timezone-neutral time
	const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL, -1);				// gets us the start of the true LST day
	WTime dayNoon(dayLST);
	dayNoon += WTimeSpan(0, 12, 0, 0);

	std::int32_t hour = dayNoon.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);

	for (std::uint8_t i = 0; i < 24; i++)
       		m_hourly_precip[i] = (float)0.0;
	m_hourly_precip[hour] = m_daily_precip;
}


std::uint16_t DailyWeather::calculateTemp() {					// *****************************************************************//
									// Temperature Calculations
	DailyWeather *yesterday = getYesterday();

	m_calc_gamma = m_weatherCondition->m_temp_gamma;		// gamma decay parameter for exponential function
	m_calc_min = m_daily_min_temp;					// today's minimum temp
	m_calc_max = m_daily_max_temp;					// today's maximum temp

    m_calc_tn = m_SunRise + WTimeSpan((std::int64_t)(m_weatherCondition->m_temp_alpha * 60.0 * 60.0));	// time of minimum temperature
    m_calc_tx = m_SolarNoon + WTimeSpan((std::int64_t)(m_weatherCondition->m_temp_beta * 60.0 * 60.0));	// time of maximum temperature

	m_SunsetTemp = sin_function(m_SunSet);				// set sunset temperature for today may be needed as a guess for yesterday's value

	if (yesterday != NULL) {
		WTime daily_time((std::uint64_t)0, m_weatherCondition->m_time.GetTimeManager());
		int i;
		/* yesterday's sunset time*/
		m_calc_ts = yesterday->m_SunSet;
		/* yesterday's sunset temperature */
			// is yesterday specified in hours ?
		if (yesterday->m_flags & DAY_HOURLY_SPECIFIED)
					// interpolate from hourly values
			m_calc_sunset =
						 // last hour before sunset plus
						 yesterday->m_hourly_temp[m_calc_ts.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)]
						   // the difference between first hour after sunset and last hour before sunset
						   + (yesterday->m_hourly_temp[m_calc_ts.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)+1] - yesterday->m_hourly_temp[m_calc_ts.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)])
						   // prorated by number of minutes after the hour that the sun went down
						   * ((double)m_calc_ts.GetMinute(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)/60.0);
		else
				// recall calculated value from yesterday
			m_calc_sunset = yesterday->m_SunsetTemp;

		// first function - sunset yesterday to time tn today

		// should we fill in yesterdays values ?

		if (!(yesterday->m_flags & DAY_HOURLY_SPECIFIED)/*m_isHourlySpecified*/) {
				//***************************************
				// set up variables for yesterday's RH
		// calculate saturated vapour pressure at max temp
			double svpt0 = 6.108 * exp((yesterday->m_daily_max_temp + yesterday->m_dblTempDiff)*17.27/((yesterday->m_daily_max_temp + yesterday->m_dblTempDiff)+237.3));
		// calculate vapour pressure at max temp
			double vpt0 = svpt0 * yesterday->m_daily_rh/*/100.0*/;
			// calculate absolute humidity (qt0) from Max temperature and vapour pressure
			double qt0 = QT0(vpt0,yesterday->m_daily_max_temp+yesterday->m_dblTempDiff);

			double RH_const = 100.0*qt0/(6.108*217.0);

					//*********************************
					// Calculate hourly Temperature and RH values after sunset yesterday until midnight yesterday
			for (i = m_calc_ts.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST) + 1,
			    daily_time = m_calc_ts + WTimeSpan(0,1,-m_calc_ts.GetMinute(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST),
			    -m_calc_ts.GetSecond(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST));
					daily_time < m_DayStart;daily_time += WTimeSpan(0, 1, 0, 0), i++   )
			{
				double tempValue=exp_function(daily_time);
				yesterday->m_hourly_temp[i] = (float)tempValue;
				yesterday->m_hourly_rh[i] = (float)(RH_const
							* (273.17 + yesterday->m_hourly_temp[i])
					/ exp (17.27*yesterday->m_hourly_temp[i]/(yesterday->m_hourly_temp[i]+237.3)) * 0.01);
				if (yesterday->m_hourly_rh[i] > 1.0)
					yesterday->m_hourly_rh[i] = 1.0;
				else if (yesterday->m_hourly_rh[i] < 0.0)
					yesterday->m_hourly_rh[i] = 0.0;
			}
		}
	} else {   // there is no yesterday, so use todays value as a guess
		WTime t(m_DayStart), SunRise(m_DayStart), SunSet(m_DayStart), SolarNoon(m_DayStart);
		t -= WTimeSpan(0, 12, 0, 0);
		std::int16_t success = m_weatherCondition->m_worldLocation.m_sun_rise_set(t, &SunRise, &SunSet, &SolarNoon);
		if (success & NO_SUNSET)
			m_calc_ts = m_SunSet - WTimeSpan(1, 0, 0, 0);
		else
			m_calc_ts = SunSet;
		m_calc_sunset = m_SunsetTemp;	// we'll just use today's for now because don't have anything else
	}

	WTime daily_time((std::uint64_t)0, m_weatherCondition->m_time.GetTimeManager());
	std::uint16_t i;
	for (i=0, daily_time = m_DayStart; daily_time < m_calc_tn; daily_time += WTimeSpan(0, 1, 0, 0))
		m_hourly_temp[i++] = (float)exp_function(daily_time);
	// second function - time tn to sunset today
	for (; daily_time <= m_SunSet; daily_time += WTimeSpan (0, 1, 0, 0))
		m_hourly_temp[i++] = (float)sin_function(daily_time);

	return i;
}


void DailyWeather::calculateRH() {					// *****************************************************************//
									// RH Calculations
	double svpt0 = 6.108 * exp(m_daily_max_temp*17.27/(m_daily_max_temp+237.3));	// calculate saturated vapour pressure at max temp
	double vpt0 = svpt0 * m_daily_rh;					// calculate vapour pressure at max temp
	double qt0 = (217.0 * vpt0)/(273.17+m_daily_max_temp);				// calculate absolute humidity (qt0) from Max temperature and vapour pressure

	double temp = 100.0*qt0/(6.108*217.0);

	for (std::uint16_t i = 0; i <= m_SunSet.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST); i++) {
			m_hourly_rh[i] = (float)(temp
                			* (273.17 + m_hourly_temp[i])
					/ exp (17.27*m_hourly_temp[i]/(m_hourly_temp[i]+237.3)) * 0.01);
			if (m_hourly_rh[i] > 1.0)
				m_hourly_rh[i] = 1.0;
			else if (m_hourly_rh[i] < 0.0)
				m_hourly_rh[i] = 0.0;
		}
}


std::uint16_t DailyWeather::calculateWS() {					// *****************************************************************//
									// Windspeed Calculations
	DailyWeather *yesterday = getYesterday();

	m_calc_gamma = m_weatherCondition->m_wind_gamma;		// gamma decay parameter for exponential function
	m_calc_min = m_daily_min_ws;					// today's minimum wind speed
	m_calc_max = m_daily_max_ws;					// today's maximum wind speed

        m_calc_tn = m_SunRise + WTimeSpan((std::int64_t)(m_weatherCondition->m_wind_alpha * 60.0 * 60.0));	// time of minimum wind
        m_calc_tx = m_SolarNoon + WTimeSpan((std::int64_t)(m_weatherCondition->m_wind_beta * 60.0 * 60.0));	// time of maximum wind

	if (yesterday != NULL) {
		m_calc_ts = yesterday->m_SunSet;
		m_calc_tx = yesterday->m_SolarNoon + WTimeSpan((std::int64_t)(m_weatherCondition->m_wind_beta * 60.0 * 60.0));	// time of maximum wind
	    /* yesterday's maximum windspeed */
            // is yesterday specified in hours ?
		if (yesterday->m_flags & DAY_HOURLY_SPECIFIED)
                    // interpolate from hourly values
			m_calc_sunset =
                         // last hour before sunset plus
                         yesterday->m_hourly_ws[m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)]
                           // the difference between first hour after sunset and last hour before sunset
                           + (yesterday->m_hourly_ws[m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)+1] - yesterday->m_hourly_ws[m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)])
                           // prorated by number of minutes after the hour that the sun went down
                           * (m_calc_tx.GetMinute(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)/60.0);
		else
			// recall calculated value from yesterday
			m_calc_sunset = yesterday->m_daily_max_ws;

	    // first function - sunset yesterday to time tn today

	    // should we fill in yesterdays values ?

        if (!(yesterday->m_flags & DAY_HOURLY_SPECIFIED))
        {
                // yes do hourly values after maximum yesterday until midnight yesterday
			int i;
			WTime daily_time((std::uint64_t)0, m_weatherCondition->m_time.GetTimeManager());

			if (yesterday->m_flags & DAY_HOURLY_SPECIFIED)
						// interpolate from hourly values
				m_calc_sunset =
							 // last hour before sunset plus
							 yesterday->m_hourly_ws[m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)]
							   // the difference between first hour after sunset and last hour before sunset
							   + (yesterday->m_hourly_ws[m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)+1] - yesterday->m_hourly_temp[m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)])
							   // prorated by number of minutes after the hour that the sun went down
							   * ((double)m_calc_tx.GetMinute(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)/60.0);
			else
					// recall calculated value from yesterday
				m_calc_sunset = yesterday->m_hourly_ws[m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)];

		    for(i = m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)+1,
			daily_time = m_calc_tx + WTimeSpan(0,1,-m_calc_tx.GetMinute(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST),
			-m_calc_tx.GetSecond(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST));
			    daily_time < m_DayStart ;
			    daily_time += WTimeSpan(0, 1, 0, 0), i++
		       )
		    {
				double tempValue=exp_WindFunc(daily_time);
					if(tempValue<0)
						tempValue=0;
				yesterday->m_hourly_ws[i] = (float)tempValue;
		    }

	}
    } // if yesterday != NULL
    else   // there is no yesterday, so use todays value as a guess
    {
		WTime t(m_DayStart), SunRise(m_DayStart), SunSet(m_DayStart), SolarNoon(m_DayStart);
		t -= WTimeSpan(0, 12, 0, 0);
		std::int16_t success = m_weatherCondition->m_worldLocation.m_sun_rise_set(t, &SunRise, &SunSet, &SolarNoon);
		if (success & NO_SUNSET)
			m_calc_ts = m_SunSet - WTimeSpan(1, 0, 0, 0); 
		else	m_calc_ts = SunSet;
		m_calc_tx = SolarNoon + WTimeSpan((std::int64_t)(m_weatherCondition->m_wind_beta * 60.0 * 60.0));	// time of maximum wind
		m_calc_sunset = m_daily_max_ws;
    }

	WTime daily_time((std::uint64_t)0, m_weatherCondition->m_time.GetTimeManager());
	std::uint16_t i;
	for(i=0, daily_time = m_DayStart; daily_time < m_calc_tn; daily_time += WTimeSpan(0, 1, 0, 0))
	{
		double tempValue;
		tempValue=exp_WindFunc(daily_time);
		if(tempValue<0)
			tempValue=0;
		m_hourly_ws[i++] = (float)tempValue;
	}
	// second function - time tn to tx today

        m_calc_tx = m_SolarNoon + WTimeSpan((std::int64_t)(m_weatherCondition->m_wind_beta * 60.0 * 60.0));	// time of maximum wind

	for( ; daily_time <= m_calc_tx; daily_time += WTimeSpan (0, 1, 0, 0) )	//Originally is m_calc_ts
	{
		double tempValue;
		tempValue=sin_function(daily_time);
		if(tempValue<0)
			tempValue=0;
		m_hourly_ws[i++] = (float)tempValue;
	}

	return i;
}


std::uint16_t DailyWeather::calculateGust() {					// *****************************************************************//
	// same as WS other than the starting conditional
									// Windspeed Calculations
	if (!(m_flags & DAY_GUST_SPECIFIED))
		return -1;

	DailyWeather* yesterday = getYesterday();

	m_calc_gamma = m_weatherCondition->m_wind_gamma;		// gamma decay parameter for exponential function
	m_calc_min = m_daily_min_gust;							// today's minimum wind speed
	m_calc_max = m_daily_max_gust;							// today's maximum wind speed

	m_calc_tn = m_SunRise + WTimeSpan((std::int64_t)(m_weatherCondition->m_wind_alpha * 60.0 * 60.0));	// time of minimum wind
	m_calc_tx = m_SolarNoon + WTimeSpan((std::int64_t)(m_weatherCondition->m_wind_beta * 60.0 * 60.0));	// time of maximum wind

	if (yesterday != NULL) {
		m_calc_ts = yesterday->m_SunSet;
		m_calc_tx = yesterday->m_SolarNoon + WTimeSpan((std::int64_t)(m_weatherCondition->m_wind_beta * 60.0 * 60.0));	// time of maximum wind
		/* yesterday's maximum windspeed */
			// is yesterday specified in hours ?
		if (yesterday->m_flags & DAY_HOURLY_SPECIFIED)
			// interpolate from hourly values
			m_calc_sunset =
				// last hour before sunset plus
				yesterday->m_hourly_gust[m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)]
				// the difference between first hour after sunset and last hour before sunset
				+ (yesterday->m_hourly_gust[m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST) + 1] - yesterday->m_hourly_gust[m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)])
				// prorated by number of minutes after the hour that the sun went down
				* (m_calc_tx.GetMinute(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST) / 60.0);
		else
			// recall calculated value from yesterday
			m_calc_sunset = yesterday->m_daily_max_gust;

		if (!(yesterday->m_flags & DAY_HOURLY_SPECIFIED))
		{
			// yes do hourly values after maximum yesterday until midnight yesterday
			int i;
			WTime daily_time((std::uint64_t)0, m_weatherCondition->m_time.GetTimeManager());

			if (yesterday->m_flags & DAY_HOURLY_SPECIFIED)
				// interpolate from hourly values
				m_calc_sunset =
				// last hour before sunset plus
				yesterday->m_hourly_gust[m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)]
				// the difference between first hour after sunset and last hour before sunset
				+ (yesterday->m_hourly_gust[m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST) + 1] - yesterday->m_hourly_temp[m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)])
				// prorated by number of minutes after the hour that the sun went down
				* ((double)m_calc_tx.GetMinute(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST) / 60.0);
			else
				// recall calculated value from yesterday
				m_calc_sunset = yesterday->m_hourly_gust[m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)];

			for (i = m_calc_tx.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST) + 1,
				daily_time = m_calc_tx + WTimeSpan(0, 1, -m_calc_tx.GetMinute(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST),
					-m_calc_tx.GetSecond(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST));
				daily_time < m_DayStart;
				daily_time += WTimeSpan(0, 1, 0, 0), i++
				)
			{// At first it starts from m_calc_ts; Actually it should start with max time;
				double tempValue = exp_WindFunc(daily_time);
				if (tempValue < 0)
					tempValue = 0;
				yesterday->m_hourly_gust[i] = (float)tempValue;
			}
		}
	} // if yesterday != NULL
	else   // there is no yesterday, so use todays value as a guess
	{
		WTime t(m_DayStart), SunRise(m_DayStart), SunSet(m_DayStart), SolarNoon(m_DayStart);
		t -= WTimeSpan(0, 12, 0, 0);
		std::int16_t success = m_weatherCondition->m_worldLocation.m_sun_rise_set(t, &SunRise, &SunSet, &SolarNoon);
		if (success & NO_SUNSET)
			m_calc_ts = m_SunSet - WTimeSpan(1, 0, 0, 0);
		else	m_calc_ts = SunSet;
		m_calc_tx = SolarNoon + WTimeSpan((std::int64_t)(m_weatherCondition->m_wind_beta * 60.0 * 60.0));	// time of maximum wind
		m_calc_sunset = m_daily_max_gust;
	}

	WTime daily_time((std::uint64_t)0, m_weatherCondition->m_time.GetTimeManager());
	std::uint16_t i;
	for (i = 0, daily_time = m_DayStart; daily_time < m_calc_tn; daily_time += WTimeSpan(0, 1, 0, 0))
	{
		double tempValue;
		tempValue = exp_WindFunc(daily_time);
		if (tempValue < 0)
			tempValue = 0;
		m_hourly_gust[i++] = (float)tempValue;
	}
	// second function - time tn to tx today

	m_calc_tx = m_SolarNoon + WTimeSpan((std::int64_t)(m_weatherCondition->m_wind_beta * 60.0 * 60.0));	// time of maximum wind

	for (; daily_time <= m_calc_tx; daily_time += WTimeSpan(0, 1, 0, 0))	//Originally is m_calc_ts
	{
		double tempValue;
		tempValue = sin_function(daily_time);
		if (tempValue < 0)
			tempValue = 0;
		m_hourly_gust[i++] = (float)tempValue;
	}

	return i;
}


double DailyWeather::sin_function(const WTime &t) const {
	WTimeSpan	numerator = t - m_calc_tn,
				denominator = m_calc_tx - m_calc_tn;
	double fraction = (double)numerator.GetTotalSeconds() / (double)denominator.GetTotalSeconds();		// equation 3
	return m_calc_min+(m_calc_max-m_calc_min)*sin(fraction * CONSTANTS_NAMESPACE::HalfPi<double>());
}


double DailyWeather::exp_function(const WTime &t) const {
	WTimeSpan	numerator = t - m_calc_ts,
				denominator = m_calc_tn - m_calc_ts;
	double fraction = (double)numerator.GetTotalSeconds() / (double)denominator.GetTotalSeconds();		// equation 4
	weak_assert(m_calc_tn > m_calc_ts);
	weak_assert(t >= m_calc_ts);
	weak_assert(m_calc_tn >= m_calc_ts);
	return m_calc_min+(m_calc_sunset-m_calc_min)*exp(fraction * m_calc_gamma);
}


double DailyWeather::exp_WindFunc(const WTime &t) const {
	WTimeSpan	numerator = t - m_calc_tx,
				denominator = m_calc_tn - m_calc_tx;
	double fraction = (double)numerator.GetTotalSeconds() / (double)denominator.GetTotalSeconds();
	weak_assert(m_calc_tn > m_calc_tx);
	weak_assert(t >= m_calc_tx);
	weak_assert(m_calc_tn >= t);
	return m_calc_sunset - (m_calc_sunset - m_calc_min) * sin(fraction * CONSTANTS_NAMESPACE::HalfPi<double>());
}
