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

#pragma once

#include "WTime.h"
#include "linklist.h"
#include "hssconfig/config.h"

using namespace HSS_Time;

#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(push, 8)
#endif

class DailyWeather : public MinNode {
	friend class CWFGM_WeatherStreamHelper;
public:
	DailyWeather *LN_Succ() const			{ return (DailyWeather *)MinNode::LN_Succ(); };
	DailyWeather *LN_Pred() const			{ return (DailyWeather *)MinNode::LN_Pred(); };

	DailyWeather(WeatherCondition *wc);
	DailyWeather(const DailyWeather &toCopy, WeatherCondition *wc);

	class WeatherCondition *m_weatherCondition;		// pointer to its owner, so we can ask for values for a different time, which
								// may be in a different day
	DailyWeather *getYesterday() const		{ DailyWeather *dw = LN_Pred(); if (dw->LN_Pred()) return dw; return nullptr; };
	DailyWeather *getTomorrow() const		{ DailyWeather *dw = LN_Succ(); if (dw->LN_Succ()) return dw; return nullptr; };

	void GetEventTime(std::uint32_t flags, const WTime &from_time, WTime &next_event, bool look_ahead = false);

	std::uint32_t	m_flags;
	std::uint8_t	m_hflags[24];
		#define DAY_HOURLY_SPECIFIED		0x00000001
		#define DAY_ORIGIN_FILE				0x00000002
		#define DAY_ORIGIN_ENSEMBLE			0x00000004
		#define DAY_ORIGIN_MODIFIED			0x00000008
		#define DAY_GUST_SPECIFIED			0x00000010

		#define HOUR_DEWPT_SPECIFIED		0x04
		#define HOUR_GUST_SPECIFIED			0x02

	WTime	m_DayStart,
		m_SunRise,					// variables used in calculateHourlyConditions()
		m_SolarNoon,
		m_SunSet;

public:
	bool calculateTimes(std::uint16_t index);

    private:
	float	m_hourly_temp[24],
		m_hourly_dewpt_temp[24],
		m_hourly_rh[24],
		m_hourly_ws[24],
			m_hourly_gust[24],
		m_hourly_precip[24];
	double	m_hourly_wd[24];				// hourly conditions that may be given or calculated from daily values

	float	m_daily_min_temp,
		m_daily_max_temp,
		m_daily_min_ws,
		m_daily_max_ws,
			m_daily_min_gust,
			m_daily_max_gust,
		m_daily_rh,
		m_daily_precip;
	double	m_daily_wd;					// daily conditions for us to work with

protected:
	void hourlyWeather_Serialize(const std::uint32_t hour, double *temp, double *rh, double *precip, double *ws, double *wd, double *dew) const {
		*temp = m_hourly_temp[hour];
		*rh = m_hourly_rh[hour];
		*precip = m_hourly_precip[hour];
		*ws = m_hourly_ws[hour];
		if (gust) {
			if (m_hflags[hour] & HOUR_GUST_SPECIFIED)
				*gust = m_hourly_gust[hour];
			else
				*gust = -1.0;
		}
		*wd = m_hourly_wd[hour];
		*dew = m_hourly_dewpt_temp[hour];
	};


public:
	bool	calculateHourlyConditions();
	void	calculateRemainingHourlyConditions();
	void	calculateDailyConditions();

	void hourlyWeather(const WTime &time, double *temp, double *rh, double *precip, double *ws, double *gust, double *wd, double *dew) const {
										  std::int32_t hour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
										  *temp = m_hourly_temp[hour];
										  *rh = m_hourly_rh[hour];
										  *precip = m_hourly_precip[hour];
										  *ws = m_hourly_ws[hour];
										  if (gust) {
											  if (m_hflags[hour] & HOUR_GUST_SPECIFIED)
												  *gust = m_hourly_gust[hour];
											  else
												  *gust = -1.0;
										  }
										  *wd = m_hourly_wd[hour];
										  *dew = m_hourly_dewpt_temp[hour];
										};
	double hourlyTemp(const WTime &time) const		{ return m_hourly_temp[time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)]; };
	double hourlyDewPtTemp(const WTime &t) const	{ return m_hourly_dewpt_temp[t.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)]; };
	double hourlyRH(const WTime &time) const		{ return m_hourly_rh[time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)]; };
	double hourlyWS(const WTime &time) const		{ return m_hourly_ws[time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)]; };
	double hourlyGust(const WTime& time) const		{ return m_hourly_gust[time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)]; };
	double hourlyPrecip(const WTime &time) const	{ return m_hourly_precip[time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)]; };
	double hourlyWD(const WTime &time) const		{
								  return m_hourly_wd[time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)];
								};

	bool setHourlyWeather(const WTime &time, double temp, double rh, double precip, double ws, double gust, double wd, double dew);
	bool setHourlyPrecip(const WTime& time, double precip);

protected:
	bool setHourlyWeather(std::int32_t hour, double temp, double rh, double precip, double ws, double gust, double wd, double dew);
	bool setHourlyPrecip(std::int32_t hour, double precip);

public:
	double dailyMinTemp() const;
	double dailyMeanTemp() const;
	double dailyMaxTemp() const;

	double dailyMinWS() const;
	double dailyMaxWS() const;

	double dailyMinGust() const;
	double dailyMaxGust() const;

	double dailyMinRH() const;
	double dailyMeanRH() const;
	double dailyMaxRH() const;

	double dailyPrecip() const;

	double dailyWD() const;

	void getDailyWeather(double *min_temp, double *max_temp, double *min_ws, double *max_ws, double* min_gust, double* max_gust, double *rh, double *precip, double *wd) const {
										  *min_temp = dailyMinTemp();
										  *max_temp = dailyMaxTemp();
										  *min_ws = dailyMinWS();
										  *max_ws = dailyMaxWS();
										  *min_gust = dailyMinGust();
										  *max_gust = dailyMaxGust();
										  *rh = dailyMinRH();
										  *precip = dailyPrecip();
										  *wd = dailyWD();
										};
	bool setDailyWeather(double min_temp, double max_temp, double min_ws, double max_ws, double min_gust, double max_gust, double rh, double precip, double wd) {
										  if (m_flags & DAY_HOURLY_SPECIFIED)
											  return false;
										  m_daily_min_temp = (float)min_temp;
										  m_daily_max_temp = (float)max_temp;
										  m_daily_min_ws = (float)min_ws;
										  m_daily_max_ws = (float)max_ws;
										  m_daily_min_gust = (float)min_gust;
										  m_daily_max_gust = (float)max_gust;
										  m_daily_rh = (float)rh;
										  m_daily_precip = (float)precip;
										  m_daily_wd = wd;
										  return true;
										};

    private:
	double	m_calc_gamma;					// temporary variables for hourly calculations
	WTime	m_calc_tn,
		m_calc_tx,
		m_calc_tu,
		m_calc_ts;
	double	m_calc_min,
		m_calc_max,
		m_calc_user,
		m_calc_sunset;

	double	m_SunsetTemp;
	double m_dblTempDiff;

	double sin_function(const WTime &t) const;

	double exp_function(const WTime &t) const;
	double exp_WindFunc(const WTime &t) const;


	double QT0(double vpt0, double MaxTemp)		{ return (217.0 * vpt0)/(273.17+MaxTemp); };

	void calculateWD();
	void calculatePrecip();
	std::uint16_t calculateTemp();
	void calculateRH();
	std::uint16_t calculateWS();
	void calculateDewPtTemp();
};

#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(pop)
#endif
