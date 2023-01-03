/***********************************************************************
 * REDapp - DayWeather.java
 * Copyright (C) 2015-2019 The REDapp Development Team
 * Homepage: http://redapp.org
 * 
 * REDapp is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * REDapp is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with REDapp. If not see <http://www.gnu.org/licenses/>. 
 **********************************************************************/

package ca.wise.weather;


import ca.hss.annotations.Source;
import ca.hss.general.OutVariable;
import ca.hss.math.LinkedListNode;
import ca.hss.times.WTime;
import ca.hss.times.WTimeSpan;

import static ca.hss.math.General.HALF_PI;
import static ca.hss.times.WTime.*;
import static ca.hss.times.WTimeSpan.*;
import static java.lang.Math.*;

import java.util.Arrays;

@Source(project="WeatherCOM", sourceFile="DailyWeather.cpp")
public class DailyWeather extends LinkedListNode {
	public static final int DAY_HOURLY_SPECIFIED = 0x1;
	public static final  int DAY_ORIGIN_FILE	  = 0x2;
	public static final int DAY_ORIGIN_ENSEMBLE   = 0x4;
	public static final  int HOUR_DEWPT_SPECIFIED = 0x4;
	public static final int HOUR_GUST_SPECIFIED = 0x2;

	public WeatherCondition m_weatherCondition;
	public long m_flags;
	public int[] m_hflags = new int[24];
	public WTime m_dayStart,
				   m_SunRise,
				   m_SolarNoon,
				   m_Sunset;
	private double[] m_hourly_temp = new double[24];
	private double[] m_hourly_dewpt_temp = new double[24];
	private double[] m_hourly_rh = new double[24];
	private double[] m_hourly_ws = new double[24];
	private double[] m_hourly_wg = new double[24];
	private double[] m_hourly_precip = new double[24];
	
	private double[] m_hourly_wd = new double[24];
	private boolean[] m_hourly_spec;
    private boolean[] m_hourly_corrected;
	private double m_daily_min_temp,
				   m_daily_max_temp,
				   m_daily_min_ws,
				   m_daily_max_ws,
				   m_daily_min_wg,
				   m_daily_max_wg,
				   m_daily_rh,
				   m_daily_precip;
	private double m_daily_wd,
				   m_calc_gamma,
				   m_calc_min,
				   m_calc_max,
				   m_calc_sunset,
				   m_SunsetTemp,
				   m_dblTempDiff;
	private WTime m_calc_tn,
					m_calc_tx,
					m_calc_ts;

	public DailyWeather getYesterday() {
		DailyWeather dw = (DailyWeather)getPrevious();
		if (dw.getPrevious() != null)
			return dw;
		return null;
	}

	public DailyWeather getTomorrow() {
		DailyWeather dw = (DailyWeather)getNext();
		if (dw.getNext() != null)
			return dw;
		return null;
	}

	public DailyWeather() {
		m_hourly_spec = new boolean[] { true, true, true, true, true, true, true, true, true, true, true, true,
										true, true, true, true, true, true, true, true, true, true, true, true };
		m_hourly_corrected = new boolean[24];
		Arrays.fill(m_hourly_corrected, false);
	}

	public DailyWeather(WeatherCondition cond) {
		this();
		m_dayStart = new WTime(0, cond.m_timeManager);
		m_SunRise = new WTime(0, cond.m_timeManager);
		m_Sunset = new WTime(0, cond.m_timeManager);
		m_SolarNoon = new WTime(0, cond.m_timeManager);
		m_calc_tn = new WTime(0, cond.m_timeManager);
		m_calc_tx = new WTime(0, cond.m_timeManager);
		m_calc_ts = new WTime(0, cond.m_timeManager);

		m_weatherCondition = cond;

		m_flags = 0;
		m_daily_min_temp = m_daily_max_temp = m_daily_min_ws = m_daily_max_ws = m_daily_min_wg = m_daily_max_wg = m_daily_rh = m_daily_precip = 0.0;
		m_daily_wd = 0.0;

		for (short i = 0; i < 24; i++) {
			m_hourly_temp[i] = m_hourly_dewpt_temp[i] = m_hourly_rh[i] = m_hourly_ws[i] = m_hourly_wg[i] = m_hourly_precip[i] = 0.0;
			m_hourly_wd[i] = 0.0;
			m_hflags[i] = 0;
		}

		m_dblTempDiff = 0.0;
	}

	public boolean calculateTimes(int index) {
		m_dayStart = new WTime(m_weatherCondition.m_time);
		m_dayStart.add(new WTimeSpan(index, 0, 0, 0));

		WTime t = new WTime(m_dayStart);
		t.add(new WTimeSpan(0, 12, 0, 0));

		/*m_SunRise = m_weatherCondition.m_worldLocation.getSunrise(t);
		m_Sunset = m_weatherCondition.m_worldLocation.getSunset(t);
		m_SolarNoon = m_weatherCondition.m_worldLocation.getNoon(t);*/
		OutVariable<WTime> rise = new OutVariable<WTime>();
		rise.value = m_SunRise;
		OutVariable<WTime> set = new OutVariable<WTime>();
		set.value = m_Sunset;
		OutVariable<WTime> noon = new OutVariable<WTime>();
		noon.value = m_SolarNoon;
		m_weatherCondition.m_worldLocation.getSunRiseSetNoon(t, rise, set, noon);
		m_SunRise = new WTime(rise.value);
		m_Sunset = new WTime(set.value);
		m_SolarNoon = new WTime(noon.value);
		if (greaterThanEqualTo(subtract(m_Sunset, m_dayStart), WTimeSpan.Day))
			return false;
		return true;
	}

	public boolean calculateHourlyConditions() {
		if ((m_flags & DAY_HOURLY_SPECIFIED) == 0) {
			calculateWD();
			calculatePrecip();
			int lastTemp = calculateTemp();
			//FIXME added range checking
			if (lastTemp < 1 || lastTemp > m_hourly_temp.length)
				lastTemp = 1;
			calculateRH();
			int lastWS = calculateWS();
			int lastGust = calculateGust();
			//FIXME added range checking
			if (lastWS < 1 || lastTemp > m_hourly_ws.length)
				lastWS = 1;

			if ((getNext().getNext()) != null) {
				int i;
				for (i = lastTemp; i < 24; i++) {
					m_hourly_temp[i] = m_hourly_temp[lastTemp - 1];
					m_hourly_rh[i] = m_hourly_rh[lastTemp - 1];
				}
				for (i = lastWS; i < 24; i++)
					m_hourly_ws[i] = m_hourly_ws[lastWS - 1];
				if (lastGust != -1)
					for (i = lastWS; i < 24; i++)
						m_hourly_wg[i] = m_hourly_wg[lastGust - 1];
			}
		}
		return true;
	}

	public void calculateRemainingHourlyConditions() {
		calculateDewPtTemp();
	}

	public void calculateDailyConditions() {
		if ((m_flags & DAY_HOURLY_SPECIFIED) != 0) {
			m_daily_min_temp = getDailyMinTemp();
			m_daily_max_temp = getDailyMaxTemp();
			m_daily_min_ws = getDailyMinWS();
			m_daily_max_ws = getDailyMaxWS();
			m_daily_min_wg = getDailyMinWS();
			m_daily_max_wg = getDailyMaxWS();
			m_daily_rh = getDailyMinRH();
			m_daily_precip = getDailyPrecip();
			m_daily_wd = getDailyWD();
		}
	}

	/**
	 * Get the hourly weather conditions for a specific hour.
	 * @param time The hour to retrieve the weather conditions for.
	 * @param temp The temperature at the requested hour.
	 * @param rh The relative humidity at the requested hour.
	 * @param precip The precipitation at the requested hour.
	 * @param ws The wind speed at the requested hour.
	 * @param wd The wind direction at the requested hour.
	 * @param dew The dew point temperature at the requested hour.
	 * @throws IllegalArgumentException Thrown if any of the OutVariables are null.
	 */
	public void hourlyWeather(WTime time, OutVariable<Double> temp, OutVariable<Double> rh, OutVariable<Double> precip,
			OutVariable<Double> ws, OutVariable<Double> wg, OutVariable<Double> wd, OutVariable<Double> dew) throws IllegalArgumentException {
		if (temp == null || rh == null || precip == null || ws == null || wg == null || wd == null || dew == null)
			throw new IllegalArgumentException();
		int hour = (int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST);
		temp.value = m_hourly_temp[hour];
		rh.value = m_hourly_rh[hour];
		precip.value = m_hourly_precip[hour];
		ws.value = m_hourly_ws[hour];
		if ((m_hflags[hour] & HOUR_GUST_SPECIFIED) != 0)
			wg.value = m_hourly_wg[hour];
		else
			wg.value = -1.0;
		wd.value = m_hourly_wd[hour];
		dew.value = m_hourly_dewpt_temp[hour];
	}

	public double getHourlyTemp(WTime time) { return m_hourly_temp[(int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)]; }
	public double getHourlyDewPtTemp(WTime time) { return m_hourly_dewpt_temp[(int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)]; }
	public double getHourlyRH(WTime time) { return m_hourly_rh[(int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)]; }
	public double getHourlyWS(WTime time) { return m_hourly_ws[(int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)]; }
	public double getHourlyWG(WTime time) { return m_hourly_wg[(int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)]; }
	public double getHourlyWD(WTime time) { return m_hourly_wd[(int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)]; }
	public double getHourlyPrecip(WTime time) { return m_hourly_precip[(int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)]; }
	public boolean isHourInterpolated(WTime time) { return !m_hourly_spec[(int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)]; }
    public boolean isHourCorrected(WTime time) { return m_hourly_corrected[(int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)]; }

	public boolean setHourlyInterpolation(int hour) {
		if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
			return false;
		m_hourly_spec[hour] = false;
		return true;
	}
	
	public boolean clearHourInterpolated(int hour) {
		if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
			return false;
		m_hourly_spec[hour] = true;
		return true;
	}

    public boolean setHourlyCorrected(int hour) {
        if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
            return false;
        m_hourly_corrected[hour] = true;
        return true;
    }
    
    public boolean clearHourCorrected(int hour) {
        if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
            return false;
        m_hourly_corrected[hour] = false;
        return true;
    }
    
    /**
     * Were any of the hourly weather values corrected when they were imported.
     * They will have been corrected if the values were out of bounds and
     * fix invalid values was specified in the import options.
     */
    public boolean hasAnyCorrected() {
        for (boolean b : m_hourly_corrected) {
            if (b)
                return true;
        }
        return false;
    }
	
	public boolean setHourlyWeather(WTime time, double temp, double rh, double precip, double ws, double wg, double wd, double dew) {
		if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
			return false;
		int hour = (int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST);
		m_hourly_temp[hour] = temp;
		m_hourly_rh[hour] = rh;
		
		m_hourly_precip[hour] = precip;
		
		m_hourly_ws[hour] = ws;

		if (wg >= 0.0) {
			m_hourly_wg[hour] = (float)wg;
			m_hflags[hour] |= HOUR_GUST_SPECIFIED;
		} else
			m_hflags[hour] &= (~(HOUR_GUST_SPECIFIED));

		m_hourly_wd[hour] = wd;

		if (dew > -300) {
			m_hourly_dewpt_temp[hour] = dew;
			m_hflags[hour] |= HOUR_DEWPT_SPECIFIED;
		}
		else
			m_hflags[hour] &= (~(HOUR_DEWPT_SPECIFIED));

		return true;
	}

	public double getDailyMinTemp() {
		if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
			return m_daily_min_temp;
		int i = m_weatherCondition.firstHourOfDay(m_dayStart);
		int j = m_weatherCondition.lastHourOfDay(m_dayStart);
		double t = m_hourly_temp[i];
		for (i++; i <= j; i++)
			if (t > m_hourly_temp[i])
				t = m_hourly_temp[i];
		return t;
	}

	public double getDailyMeanTemp() {
		int start = m_weatherCondition.firstHourOfDay(m_dayStart);
		int j = m_weatherCondition.lastHourOfDay(m_dayStart);
		int i = start;
		double t = m_hourly_temp[i];
		for (i++; i <= j; i++)
			t += m_hourly_temp[i];
		return t / ((double)(j - start + 1));
	}

	public double getDailyMaxTemp() {
		if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
			return m_daily_max_temp;
		int i = m_weatherCondition.firstHourOfDay(m_dayStart);
		int j = m_weatherCondition.lastHourOfDay(m_dayStart);
		double t = m_hourly_temp[i];
		for (i++; i <= j; i++)
			if (t < m_hourly_temp[i])
				t = m_hourly_temp[i];
		return t;
	}

	public double getDailyMinWS() {
		if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
			return m_daily_min_ws;
		int i = m_weatherCondition.firstHourOfDay(m_dayStart);
		int j = m_weatherCondition.lastHourOfDay(m_dayStart);
		double t = m_hourly_ws[i];
		for (i++; i <= j; i++)
			if (t > m_hourly_ws[i])
				t = m_hourly_ws[i];
		return t;
	}

	public double getDailyMaxWS() {
		if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
			return m_daily_max_ws;
		int i = m_weatherCondition.firstHourOfDay(m_dayStart);
		int j = m_weatherCondition.lastHourOfDay(m_dayStart);
		double t = m_hourly_ws[i];
		for (i++; i <= j; i++)
			if (t < m_hourly_ws[i])
				t = m_hourly_ws[i];
		return t;
	}

	public double getDailyMinGust() {
		if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
			return m_daily_min_wg;
		int i = m_weatherCondition.firstHourOfDay(m_dayStart);
		int j = m_weatherCondition.lastHourOfDay(m_dayStart);
		double t = m_hourly_wg[i];
		for (i++; i <= j; i++)
			if (t > m_hourly_wg[i])
				t = m_hourly_wg[i];
		return t;
	}

	public double getDailyMaxGust() {
		if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
			return m_daily_max_wg;
		int i = m_weatherCondition.firstHourOfDay(m_dayStart);
		int j = m_weatherCondition.lastHourOfDay(m_dayStart);
		double t = m_hourly_wg[i];
		for (i++; i <= j; i++)
			if (t < m_hourly_wg[i])
				t = m_hourly_wg[i];
		return t;
	}

	public double getDailyMinRH() {
		if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
			return m_daily_rh;
		int i = m_weatherCondition.firstHourOfDay(m_dayStart);
		int j = m_weatherCondition.lastHourOfDay(m_dayStart);
		double rh = m_hourly_rh[i];
		for (i++; i <= j; i++)
			if (rh > m_hourly_rh[i])
				rh = m_hourly_rh[i];
		return rh;
	}

	public double getDailyMeanRH() {
		if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
			return m_daily_rh;
		int start = m_weatherCondition.firstHourOfDay(m_dayStart);
		int j = m_weatherCondition.lastHourOfDay(m_dayStart);
		double rh = 0.0;
		for (int i = start; i <= j; i++)
			rh += m_hourly_rh[i];
		return rh / ((double)(j - start + 1));
	}

	public double getDailyMaxRH() {
		if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
			return m_daily_rh;
		int i = m_weatherCondition.firstHourOfDay(m_dayStart);
		int j = m_weatherCondition.lastHourOfDay(m_dayStart);
		double rh = m_hourly_rh[i];
		for (i++; i <= j; i++)
			if (rh < m_hourly_rh[i])
				rh = m_hourly_rh[i];
		return rh;
	}

	public double getDailyPrecip() {
		if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
			return m_daily_precip;

		WTime dayNeutral = new WTime(m_dayStart, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);
		WTime dayLST = new WTime(dayNeutral, WTime.FORMAT_AS_LOCAL, (short)-1);
		WTime dayNoon = new WTime(dayLST);
		dayNoon.add(new WTimeSpan(0, 12, 0, 0));
		
		WTime begin = new WTime(WTime.add(m_weatherCondition.m_time, new WTimeSpan(0, m_weatherCondition.firstHourOfDay(m_dayStart), 0, 0)));
		WTime end = new WTime(WTime.add(m_weatherCondition.m_time,
		        new WTimeSpan(m_weatherCondition.m_readings.size() - 1, m_weatherCondition.lastHourOfDay(m_dayStart), 0, 0)));

		double rain;
		
		if (getYesterday() == null) {
			rain = m_weatherCondition.m_initialRain;
			WTime loop = new WTime(m_dayStart);
			if (WTime.lessThan(loop, begin))
				loop = begin;
			if (WTime.greaterThan(dayNoon, end))
				dayNoon = end;
			while (WTime.lessThanEqualTo(loop, dayNoon)) {
				rain += m_weatherCondition.getHourlyRain(loop);
				loop.add(WTimeSpan.Hour);
			}
		}
		else {
			rain = 0.0;
			WTime loop = new WTime(dayNoon);
			loop.subtract(new WTimeSpan(0, 23, 0, 0));
			if (WTime.lessThan(loop, begin))
				loop = begin;
			if (WTime.greaterThan(dayNoon, end))
				dayNoon = end;
			for (; WTime.lessThanEqualTo(loop, dayNoon); loop.add(WTimeSpan.Hour))
				rain += m_weatherCondition.getHourlyRain(loop);
		}
		
		return rain;
	}

	public double getDailyWD() {
		if ((m_flags & DAY_HOURLY_SPECIFIED) == 0)
			return m_daily_wd;
		WTime t = new WTime(m_dayStart);
		t.add(new WTimeSpan(0, 12, 0, 0));
		int end = m_weatherCondition.lastHourOfDay(t);
		int th = (int)t.getHour(FORMAT_AS_LOCAL);
		if (th > end)
			th = end;
		return m_hourly_wd[th];
	}

	public void getDailyWeather(OutVariable<Double> min_temp, OutVariable<Double> max_temp, OutVariable<Double> min_ws, OutVariable<Double> max_ws, OutVariable<Double> min_wg, OutVariable<Double> max_wg,
								OutVariable<Double> rh, OutVariable<Double> precip, OutVariable<Double> wd) throws IllegalArgumentException {
		if (min_temp == null || max_temp == null || min_ws == null || max_ws == null || rh == null || precip == null || wd == null)
			throw new IllegalArgumentException();
		min_temp.value = getDailyMinTemp();
		max_temp.value = getDailyMaxTemp();
		min_ws.value = getDailyMinWS();
		max_ws.value = getDailyMaxWS();
		min_wg.value = getDailyMinGust();
		max_wg.value = getDailyMaxGust();
		rh.value = getDailyMinRH();
		precip.value = getDailyPrecip();
		wd.value = getDailyWD();
	}

	public boolean setDailyWeather(double min_temp, double max_temp, double min_ws, double max_ws, double min_wg, double max_wg, double rh, double precip, double wd) {
		if ((m_flags & DAY_HOURLY_SPECIFIED) != 0)
			return false;
		m_daily_min_temp = min_temp;
		m_daily_max_temp = max_temp;
		m_daily_min_ws = min_ws;
		m_daily_max_ws = max_ws;
		m_daily_min_wg = min_wg;
		m_daily_max_wg = max_wg;
		m_daily_rh = rh;
		m_daily_precip = precip;
		m_daily_wd = wd;
		return true;
	}

	private void calculateWD() {
		for (short i = 0; i < 24; i++)
        	m_hourly_wd[i] = m_daily_wd;
	}

	private void calculatePrecip() {
		WTime dayNeutral = new WTime(m_dayStart, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);
		WTime dayLST = new WTime(dayNeutral, WTime.FORMAT_AS_LOCAL, (short)-1);
		WTime dayNoon = new WTime(dayLST);
		dayNoon.add(new WTimeSpan(0, 12, 0, 0));
		int hour = (int)dayNoon.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST);

		for (short i = 0; i < 24; i++)
       		m_hourly_precip[i] = 0.0;
		m_hourly_precip[hour] = m_daily_precip;
	}

	private double QT0(double vpt0, double MaxTemp) { return (217.0 * vpt0) / (273.17 + MaxTemp); }

	private int calculateTemp() {
		DailyWeather yesterday = getYesterday();

		m_calc_gamma = m_weatherCondition.m_temp_gamma;
		m_calc_min = m_daily_min_temp;
		m_calc_max = m_daily_max_temp;

        m_calc_tn = add(m_SunRise, new WTimeSpan((long)(m_weatherCondition.m_temp_alpha * 60.0 * 60.0)));
        m_calc_tx = add(m_SolarNoon, new WTimeSpan((long)(m_weatherCondition.m_temp_beta * 60.0 * 60.0)));

		m_SunsetTemp = sin_function(m_Sunset);

		if (yesterday != null) {
			WTime daily_time = new WTime(0L, m_weatherCondition.m_time.getTimeManager());
			int i;
			m_calc_ts = yesterday.m_Sunset;

			if ((yesterday.m_flags & DAY_HOURLY_SPECIFIED) != 0)
				m_calc_sunset = yesterday.m_hourly_temp[(int)m_calc_ts.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)] +
							   (yesterday.m_hourly_temp[(int)m_calc_ts.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST) + 1] -
									   yesterday.m_hourly_temp[(int)m_calc_ts.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)]) *
							   ((double)m_calc_ts.getMinute(FORMAT_AS_LOCAL | FORMAT_WITHDST) / 60.0);
			else
				m_calc_sunset = yesterday.m_SunsetTemp;

			if ((yesterday.m_flags & DAY_HOURLY_SPECIFIED) == 0) {
				double svpt0 = 6.108 * exp((yesterday.m_daily_max_temp + yesterday.m_dblTempDiff) * 17.27/((yesterday.m_daily_max_temp + yesterday.m_dblTempDiff) + 237.3));

				double vpt0 = svpt0 * yesterday.m_daily_rh;

				double qt0 = QT0(vpt0,yesterday.m_daily_max_temp + yesterday.m_dblTempDiff);

				double RH_const = 100.0*qt0/(6.108*217.0);

				daily_time = add(m_calc_ts, new WTimeSpan(0, 1, -m_calc_ts.getMinute(FORMAT_AS_LOCAL | FORMAT_WITHDST), -m_calc_ts.getSecond(FORMAT_AS_LOCAL | FORMAT_WITHDST)));
				//FIXME added i < 24
				for (i = (int)m_calc_ts.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST) + 1;
						lessThan(daily_time, m_dayStart) && i < 24; daily_time.add(new WTimeSpan(0, 1, 0, 0)), i++   )
				{
					double tempValue = exp_function(daily_time);
					yesterday.m_hourly_temp[i] = tempValue;

					yesterday.m_hourly_rh[i] = (RH_const * (273.17 + yesterday.m_hourly_temp[i]) / exp(17.27 * yesterday.m_hourly_temp[i] / (yesterday.m_hourly_temp[i] + 237.3)) * 0.01);
					if (yesterday.m_hourly_rh[i] > 1.0)
						yesterday.m_hourly_rh[i] = 1.0;
					else if (yesterday.m_hourly_rh[i] < 0.0)
						yesterday.m_hourly_rh[i] = 0.0;
				}
			}
		}
		else {
			m_calc_ts = subtract(m_Sunset, new WTimeSpan(1, 0, 0, 0));
			m_calc_sunset = m_SunsetTemp;
		}

		WTime daily_time;

		int i;
		for (i=0, daily_time = new WTime(m_dayStart); lessThan(daily_time, m_calc_tn); daily_time.add(new WTimeSpan(0, 1, 0, 0)))
			m_hourly_temp[i++] = exp_function(daily_time);

		for (; lessThanEqualTo(daily_time, m_Sunset); daily_time.add(new WTimeSpan(0, 1, 0, 0)))
			m_hourly_temp[i++] = sin_function(daily_time);

		return i;
	}

	private void calculateRH() {
		double svpt0 = 6.108 * exp(m_daily_max_temp * 17.27 / (m_daily_max_temp + 237.3));
		double vpt0 = svpt0 * m_daily_rh;
		double qt0 = (217.0 * vpt0) / (273.17 + m_daily_max_temp);
		double temp = 100.0 * qt0 / (6.108 * 217.0);

		for (int i = 0; i <= m_Sunset.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST); i++) {
			m_hourly_rh[i] = (temp * (273.17 + m_hourly_temp[i]) / exp(17.27 * m_hourly_temp[i] / (m_hourly_temp[i] + 237.3)) * 0.01);
			if (m_hourly_rh[i] > 1.0)
				m_hourly_rh[i] = 1.0;
			else if (m_hourly_rh[i] < 0.0)
				m_hourly_rh[i] = 0.0;
		}
	}

	private int calculateWS() {
		DailyWeather yesterday = getYesterday();

		m_calc_gamma = m_weatherCondition.m_wind_gamma;
		m_calc_min = m_daily_min_ws;
		m_calc_max = m_daily_max_ws;

        m_calc_tn = add(m_SunRise, new WTimeSpan((long)(m_weatherCondition.m_wind_alpha * 60.0 * 60.0)));
        m_calc_tx = add(m_SolarNoon, new WTimeSpan((long)(m_weatherCondition.m_wind_beta * 60.0 * 60.0)));

		if (yesterday != null) {
			m_calc_ts = new WTime(yesterday.m_Sunset);
			m_calc_tx = add(yesterday.m_SolarNoon, new WTimeSpan((long)(m_weatherCondition.m_wind_beta * 60.0 * 60.0)));

			if ((yesterday.m_flags & DAY_HOURLY_SPECIFIED) == 0)
				m_calc_sunset = yesterday.m_hourly_ws[(int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)] +
	                            (yesterday.m_hourly_ws[(int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)+1] -
	                        		   yesterday.m_hourly_ws[(int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)])
	                            *(m_calc_tx.getMinute(FORMAT_AS_LOCAL | FORMAT_WITHDST)/60.0);
			else
				m_calc_sunset = yesterday.m_daily_max_ws;

	        if ((yesterday.m_flags & DAY_HOURLY_SPECIFIED) == 0)
	        {
				int i;
				WTime daily_time = new WTime(0L, m_weatherCondition.m_time.getTimeManager());

				if ((yesterday.m_flags & DAY_HOURLY_SPECIFIED) != 0)
					m_calc_sunset = yesterday.m_hourly_ws[(int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)] +
								    (yesterday.m_hourly_ws[(int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST) + 1] -
										   yesterday.m_hourly_temp[(int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)])
								    *((double)m_calc_tx.getMinute(FORMAT_AS_LOCAL | FORMAT_WITHDST) / 60.0);
				else
					m_calc_sunset = yesterday.m_hourly_ws[(int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)];

				daily_time = add(m_calc_tx, new WTimeSpan(0,1,-m_calc_tx.getMinute(FORMAT_AS_LOCAL | FORMAT_WITHDST),
						-m_calc_tx.getSecond(FORMAT_AS_LOCAL | FORMAT_WITHDST)));
				//FIXME added i < 24
			    for(i = (int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST) + 1;
				    lessThan(daily_time, m_dayStart) && i < 24;
				    daily_time.add(WTimeSpan.Hour), i++
			       )
			    {
					double tempValue = exp_WindFunc(daily_time);
					if(tempValue < 0)
						tempValue = 0;
					yesterday.m_hourly_ws[i] = tempValue;
			    }
	        }
	    }
	    else
	    {
	    	OutVariable<WTime> SunRise = new OutVariable<WTime>();
	    	OutVariable<WTime> SunSet = new OutVariable<WTime>();
	    	OutVariable<WTime> SolarNoon = new OutVariable<WTime>();
			WTime t = new WTime(m_dayStart);
			SunRise.value = new WTime(m_dayStart);
			SunSet.value = new WTime(m_dayStart);
			SolarNoon.value = new WTime(m_dayStart);
			t.subtract(new WTimeSpan(0, 12, 0, 0));
			int success = m_weatherCondition.m_worldLocation.getSunRiseSetNoon(t, SunRise, SunSet, SolarNoon);
			if ((success & ca.hss.times.SunriseSunsetCalc.NO_SUNSET) != 0)
				m_calc_ts = WTime.subtract(m_Sunset, new WTimeSpan(1, 0, 0, 0));
			else
				m_calc_ts = new WTime(SunSet.value);
			m_calc_tx = WTime.add(SolarNoon.value, new WTimeSpan((long)(m_weatherCondition.m_wind_beta * 60.0 * 60.0)));
			m_calc_sunset = m_daily_max_ws;
	    }

		WTime daily_time;
		int i;
		for(i = 0, daily_time = new WTime(m_dayStart); lessThan(daily_time, m_calc_tn); daily_time.add(WTimeSpan.Hour))
		{
			double tempValue = exp_WindFunc(daily_time);
			if(tempValue < 0)
				tempValue = 0;
			m_hourly_ws[i++] = tempValue;
		}

	    m_calc_tx = add(m_SolarNoon, new WTimeSpan((long)(m_weatherCondition.m_wind_beta * 60.0 * 60.0)));

		for( ; lessThanEqualTo(daily_time, m_calc_tx); daily_time.add(new WTimeSpan(0,1,0,0)))
		{
			double tempValue;
			tempValue=sin_function(daily_time);
			if(tempValue<0)
				tempValue=0;
			m_hourly_ws[i++] = (float)tempValue;
		}

	    return i;
	}

	private int calculateGust() {
		DailyWeather yesterday = getYesterday();

		m_calc_gamma = m_weatherCondition.m_wind_gamma;
		m_calc_min = m_daily_min_wg;
		m_calc_max = m_daily_max_wg;

        m_calc_tn = add(m_SunRise, new WTimeSpan((long)(m_weatherCondition.m_wind_alpha * 60.0 * 60.0)));
        m_calc_tx = add(m_SolarNoon, new WTimeSpan((long)(m_weatherCondition.m_wind_beta * 60.0 * 60.0)));

		if (yesterday != null) {
			m_calc_ts = new WTime(yesterday.m_Sunset);
			m_calc_tx = add(yesterday.m_SolarNoon, new WTimeSpan((long)(m_weatherCondition.m_wind_beta * 60.0 * 60.0)));

			if ((yesterday.m_flags & DAY_HOURLY_SPECIFIED) == 0)
				m_calc_sunset = yesterday.m_hourly_wg[(int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)] +
	                            (yesterday.m_hourly_wg[(int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)+1] -
	                        		   yesterday.m_hourly_wg[(int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)])
	                            *(m_calc_tx.getMinute(FORMAT_AS_LOCAL | FORMAT_WITHDST)/60.0);
			else
				m_calc_sunset = yesterday.m_daily_max_wg;

	        if ((yesterday.m_flags & DAY_HOURLY_SPECIFIED) == 0)
	        {
				int i;
				WTime daily_time = new WTime(0L, m_weatherCondition.m_time.getTimeManager());

				if ((yesterday.m_flags & DAY_HOURLY_SPECIFIED) != 0)
					m_calc_sunset = yesterday.m_hourly_wg[(int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)] +
								    (yesterday.m_hourly_wg[(int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST) + 1] -
										   yesterday.m_hourly_temp[(int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)])
								    *((double)m_calc_tx.getMinute(FORMAT_AS_LOCAL | FORMAT_WITHDST) / 60.0);
				else
					m_calc_sunset = yesterday.m_hourly_wg[(int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST)];

				daily_time = add(m_calc_tx, new WTimeSpan(0,1,-m_calc_tx.getMinute(FORMAT_AS_LOCAL | FORMAT_WITHDST),
						-m_calc_tx.getSecond(FORMAT_AS_LOCAL | FORMAT_WITHDST)));
				//FIXME added i < 24
			    for(i = (int)m_calc_tx.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST) + 1;
				    lessThan(daily_time, m_dayStart) && i < 24;
				    daily_time.add(WTimeSpan.Hour), i++
			       )
			    {
					double tempValue = exp_WindFunc(daily_time);
					if(tempValue < 0)
						tempValue = 0;
					yesterday.m_hourly_wg[i] = tempValue;
			    }
	        }
	    }
	    else
	    {
	    	OutVariable<WTime> SunRise = new OutVariable<WTime>();
	    	OutVariable<WTime> SunSet = new OutVariable<WTime>();
	    	OutVariable<WTime> SolarNoon = new OutVariable<WTime>();
			WTime t = new WTime(m_dayStart);
			SunRise.value = new WTime(m_dayStart);
			SunSet.value = new WTime(m_dayStart);
			SolarNoon.value = new WTime(m_dayStart);
			t.subtract(new WTimeSpan(0, 12, 0, 0));
			int success = m_weatherCondition.m_worldLocation.getSunRiseSetNoon(t, SunRise, SunSet, SolarNoon);
			if ((success & ca.hss.times.SunriseSunsetCalc.NO_SUNSET) != 0)
				m_calc_ts = WTime.subtract(m_Sunset, new WTimeSpan(1, 0, 0, 0));
			else
				m_calc_ts = new WTime(SunSet.value);
			m_calc_tx = WTime.add(SolarNoon.value, new WTimeSpan((long)(m_weatherCondition.m_wind_beta * 60.0 * 60.0)));
			m_calc_sunset = m_daily_max_wg;
	    }

		WTime daily_time;
		int i;
		for(i = 0, daily_time = new WTime(m_dayStart); lessThan(daily_time, m_calc_tn); daily_time.add(WTimeSpan.Hour))
		{
			double tempValue = exp_WindFunc(daily_time);
			if(tempValue < 0)
				tempValue = 0;
			m_hourly_wg[i++] = tempValue;
		}

	    m_calc_tx = add(m_SolarNoon, new WTimeSpan((long)(m_weatherCondition.m_wind_beta * 60.0 * 60.0)));

		for( ; lessThanEqualTo(daily_time, m_calc_tx); daily_time.add(new WTimeSpan(0,1,0,0)))
		{
			double tempValue;
			tempValue=sin_function(daily_time);
			if(tempValue<0)
				tempValue=0;
			m_hourly_wg[i++] = (float)tempValue;
		}

	    return i;
	}

	private void calculateDewPtTemp() {
		int i = m_weatherCondition.firstHourOfDay(m_dayStart);
		int j = m_weatherCondition.lastHourOfDay(m_dayStart);
		for (; i <= j; i++) {
			if ((m_hflags[i] & HOUR_DEWPT_SPECIFIED) == 0) {
				double VPs = 0.6112 * pow(10.0, 7.5 * m_hourly_temp[i] / (237.7 + m_hourly_temp[i]));
				double VP = m_hourly_rh[i] * VPs;
				if (VP > 0.0)
					m_hourly_dewpt_temp[i] = 237.7 * log10(VP / 0.6112) / (7.5 - log10(VP / 0.6112));
				else	m_hourly_dewpt_temp[i] = -273.0;
			}
		}
	}

	private double sin_function(WTime t) {
		WTimeSpan numerator = subtract(t, m_calc_tn);
		WTimeSpan denominator = subtract(m_calc_tx, m_calc_tn);
		double fraction = ((double)numerator.getTotalSeconds()) / ((double)denominator.getTotalSeconds());
		return m_calc_min + (m_calc_max - m_calc_min) * sin(fraction * HALF_PI);
	}

	private double exp_function(WTime t) {
		WTimeSpan numerator = subtract(t, m_calc_ts);
		WTimeSpan denominator = subtract(m_calc_tn, m_calc_ts);
		double fraction = ((double)numerator.getTotalSeconds()) / ((double)denominator.getTotalSeconds());
		return m_calc_min + (m_calc_sunset - m_calc_min) * exp(fraction * m_calc_gamma);
	}

	private double exp_WindFunc(WTime t) {
		WTimeSpan numerator = subtract(t, m_calc_tx);
		WTimeSpan denominator = subtract(m_calc_tn, m_calc_tx);
		double fraction = ((double)numerator.getTotalSeconds()) / ((double)denominator.getTotalSeconds());
		return m_calc_sunset - (m_calc_sunset - m_calc_min) * sin(fraction * HALF_PI);
	}
}
