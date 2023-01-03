/***********************************************************************
 * REDapp - WeatherCondition.java
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

import static ca.hss.times.WTime.add;
import static ca.hss.times.WTime.greaterThan;
import static ca.hss.times.WTime.lessThan;
import static ca.hss.times.WTime.lessThanEqualTo;
import static ca.hss.times.WTime.FORMAT_AS_LOCAL;
import static ca.hss.times.WTime.FORMAT_WITHDST;
import static ca.hss.times.WTime.notEqual;
import static ca.hss.times.WTime.subtract;
import static ca.hss.times.WTimeSpan.equal;
import static java.lang.Math.*;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.StringTokenizer;

import ca.wise.fwi.Fwi;
import ca.wise.grid.GETWEATHER_INTERPOLATE;
import ca.wise.grid.GRID_ATTRIBUTE;
import ca.wise.grid.DFWIData;
import ca.wise.grid.IFWIData;
import ca.wise.grid.IWXData;
import ca.hss.annotations.Source;
import ca.hss.general.CollectionExtensions;
import ca.hss.general.ERROR;
import ca.hss.general.OutVariable;
import ca.hss.text.StringExtensions;
import ca.hss.math.General;
import ca.hss.math.LinkedList;
import ca.hss.times.WTime;
import ca.hss.times.WTimeSpan;
import ca.hss.times.SunriseSunsetCalc;
import ca.hss.times.WTimeManager;
import ca.hss.times.WorldLocation;
import ca.wise.weather.interp.Interpolator;
import ca.wise.weather.interp.Interpolator.HourValue;

import static ca.hss.math.General.*;

@Source(sourceFile="WeatherStream.cpp", project="WeatherCOM")
public class WeatherCondition {
	
	public static final long serialVersionUID = 17;
	
	public static final int INVALID_FAILURE = 0;
	public static final int INVALID_ALLOW = 1;
	public static final int INVALID_FIX = 2;

	public WTimeManager m_timeManager;
	public WorldLocation m_worldLocation;
	public WTime m_time;
	public WTimeSpan m_initialHFFMCTime,
					   m_initialTempTime,
					   m_initialWSTime;
	public long m_options;
	
	private int mode;
	
	public double m_initialTemp,
				  m_initialWS,
				  m_initialRain,
				  m_initialHFFMC,
				  m_initialBUI,
				  m_temp_alpha,
				  m_temp_beta,
				  m_temp_gamma,
				  m_wind_alpha,
				  m_wind_beta,
				  m_wind_gamma;
	DFWIData m_spec_day;
	CWFGM_WeatherStation m_weatherStation;
	public int m_lastHour;
	public int m_firstHour;

	boolean m_isCalculatedValuesValid;
	LinkedList<DailyCondition> m_readings;

	public WeatherCondition() {
		m_worldLocation = new WorldLocation();
		m_timeManager = new WTimeManager(m_worldLocation);
		m_time = new WTime(0L, m_timeManager);
		m_spec_day = new DFWIData();
		m_readings = new LinkedList<DailyCondition>(DailyCondition.class);
		m_temp_alpha = -0.77;
		m_temp_beta = 2.80;
		m_temp_gamma = -2.20;
		m_wind_alpha = 1.00;
		m_wind_beta = 1.24;
		m_wind_gamma = -3.59;
		m_spec_day.dFFMC = -1.0;
		m_initialHFFMC = 0.0;
		m_initialHFFMCTime = new WTimeSpan(-1);
		m_spec_day.dDMC = -1.0;
		m_spec_day.dDC = -1.0;
		m_spec_day.dBUI = -1.0;
		m_spec_day.dISI = -1.0;
		m_spec_day.dFWI = -1.0;
		m_initialRain = 0.0;
		m_initialTemp = m_initialWS = 0;

		m_isCalculatedValuesValid = false;

		m_options = 0x00000001;
	}

	/**
	 * Restrict a double value to a certain range.
	 * @param value The value to constrain.
	 * @param min The minimum allowed value.
	 * @param max The maximum allowed value.
	 * @return min if value < min, max if max < value, otherwise value.
	 */
	private static double constrainToRange(double value, double min, double max) {
	    return Math.min(Math.max(value, min), max);
	}

	public DailyCondition getDCReading(WTime time, boolean add) {
		DailyCondition dc;

		WTimeSpan index = subtract(time, m_time);

		if (index.getTotalSeconds() < 0) {
			return null;
	 	}
		dc = m_readings.get((long)index.getDays());
		if ((dc == null) && (add)) {
			if (m_readings.size() > 0) {
				if (m_lastHour != 23 || (index.getDays() != m_readings.size()))
					return null;
			}
			dc = new DailyCondition(this);
            m_readings.addLast(dc);
			clearConditions();
		}
		return dc;
	}
    
    /**
     * Were any of the hourly weather values corrected when they were imported.
     * They will have been corrected if the values were out of bounds and
     * fix invalid values was specified in the import options.
     */
    public boolean hasAnyCorrected() {
        for (DailyCondition d : m_readings) {
            if (d.hasAnyCorrected())
                return true;
        }
        return false;
    }

	public boolean getDailyWeatherValues(WTime time, OutVariable<Double> min_temp, OutVariable<Double> max_temp, OutVariable<Double> min_ws, OutVariable<Double> max_ws, OutVariable<Double> min_wg, OutVariable<Double> max_wg,
			OutVariable<Double> rh, OutVariable<Double> precip, OutVariable<Double> wd) {
		assert min_temp != null;
		assert max_temp != null;
		assert min_ws != null;
		assert max_ws != null;
		assert min_wg != null;
		assert max_wg != null;
		assert rh != null;
		assert precip != null;
		assert wd != null;
		DailyCondition dc = getDCReading(time, false);
		if (dc != null) {
			calculateValues();
			dc.getDailyWeather(min_temp, max_temp, min_ws, max_ws, min_wg, max_wg, rh, precip, wd);
		}
		return (dc != null);
	}

	public boolean setDailyWeatherValues(WTime time, double min_temp, double max_temp, double min_ws, double max_ws, double min_wg, double max_wg, double rh, double precip, double wd) {
		DailyCondition dc = getDCReading(time, true);
		if (dc != null) {
			if ((dc.m_flags & DailyWeather.DAY_HOURLY_SPECIFIED) != 0)
				return false;
			
			OutVariable<Double> min_temp2 = new OutVariable<>();
			OutVariable<Double> max_temp2 = new OutVariable<>();
			OutVariable<Double> min_ws2 = new OutVariable<>();
			OutVariable<Double> max_ws2 = new OutVariable<>();
			OutVariable<Double> min_wg2 = new OutVariable<>();
			OutVariable<Double> max_wg2 = new OutVariable<>();
			OutVariable<Double> min_rh2 = new OutVariable<>();
			OutVariable<Double> precip2 = new OutVariable<>();
			OutVariable<Double> wd2 = new OutVariable<>();
			OutVariable<Double> min_temp3 = new OutVariable<>();
			OutVariable<Double> max_temp3 = new OutVariable<>();
			OutVariable<Double> min_ws3 = new OutVariable<>();
			OutVariable<Double> max_ws3 = new OutVariable<>();
			OutVariable<Double> min_wg3 = new OutVariable<>();
			OutVariable<Double> max_wg3 = new OutVariable<>();
			OutVariable<Double> min_rh3 = new OutVariable<>();
			OutVariable<Double> precip3 = new OutVariable<>();
			OutVariable<Double> wd3 = new OutVariable<>();
			dc.getDailyWeather(min_temp2, max_temp2, min_ws2, max_ws2, min_wg2, max_wg2, min_rh2, precip2, wd2);
			dc.setDailyWeather(min_temp, max_temp, min_ws, max_ws, min_wg, max_wg, rh, precip, wd);
			dc.getDailyWeather(min_temp3, max_temp3, min_ws3, max_ws3, min_wg2, max_wg2, min_rh3, precip3, wd3);
			//if any of the weather values have been modified clear any loaded FWI values 
			if (abs(min_temp2.value - min_temp3.value) >= 1e-5 ||
				abs(max_temp2.value - max_temp3.value) >= 1e-5 ||
				abs(min_ws2.value - min_ws3.value) >= 1e-5 ||
				abs(max_ws2.value - max_ws3.value) >= 1e-5 ||
				abs(min_wg2.value - min_wg3.value) >= 1e-5 ||
				abs(max_wg2.value - max_wg3.value) >= 1e-5 ||
				abs(min_rh2.value - min_rh3.value) >= 1e-5 ||
				abs(precip2.value - precip3.value) >= 1e-5 ||
				abs(wd2.value - wd3.value) >= 1e-5)
				m_options = m_options & (~0x0000004);
			clearConditions();
		}
		return (dc != null);
	}

	public double getHourlyRain(WTime time) {
		DailyCondition _dc = getDCReading(time, false);
		if (_dc != null)
			return _dc.getHourlyPrecip(time);
		return 0.0;
	}

	public boolean setHourlyWeatherValues(WTime time, double temp, double rh, double precip, double ws, double gust, double wd, double dew) {
		DailyCondition dc = getDCReading(time, true);
		if (dc != null) {
			if ((dc.m_flags & DailyWeather.DAY_HOURLY_SPECIFIED) == 0)
				return false;
			WTime tm = new WTime(m_time);
			tm = add(tm, new WTimeSpan(m_readings.size(), -(24 - m_lastHour), 0, 0));
			WTimeSpan diff = subtract(time, tm);
			if (diff.getTotalHours() > 1)
				return false;
			else if (diff.getTotalHours() == 1) {
				m_lastHour += diff.getTotalHours();
				m_lastHour = m_lastHour % 24;
			}
			OutVariable<Double> temp2 = new OutVariable<>();
			OutVariable<Double> rh2 = new OutVariable<>();
			OutVariable<Double> precip2 = new OutVariable<>();
			OutVariable<Double> ws2 = new OutVariable<>();
			OutVariable<Double> wg2 = new OutVariable<>();
			OutVariable<Double> wd2 = new OutVariable<>();
			OutVariable<Double> dew2 = new OutVariable<>();
			OutVariable<Double> temp3 = new OutVariable<>();
			OutVariable<Double> rh3 = new OutVariable<>();
			OutVariable<Double> precip3 = new OutVariable<>();
			OutVariable<Double> ws3 = new OutVariable<>();
			OutVariable<Double> wg3 = new OutVariable<>();
			OutVariable<Double> wd3 = new OutVariable<>();
			OutVariable<Double> dew3 = new OutVariable<>();
			dc.hourlyWeather(time, temp2, rh2, precip2, ws2, wg2, wd2, dew2);
			dc.setHourlyWeather(time, temp, rh, precip, ws, gust, wd, dew);
			dc.hourlyWeather(time, temp3, rh3, precip3, ws3, wg3, wd3, dew3);
			if (abs(temp2.value - temp3.value) >= 1e-5 ||
				abs(rh2.value - rh3.value) >= 1e-5 ||
				abs(precip2.value - precip3.value) >= 1e-5 ||
				abs(ws2.value - ws3.value) >= 1e-5 ||
				abs(wg2.value - wg3.value) >= 1e-5 ||
				abs(wd2.value - wd3.value) >= 1e-5 ||
				abs(dew2.value - dew3.value) >= 1e-5)
				m_options = m_options & (~0x0000004);
			dc.clearHourInterpolated((int)time.getHour(WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST));
			dc.clearHourCorrected((int)time.getHour(WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST));
			clearConditions();
		}
		return (dc != null);
	}

	public boolean makeHourlyObservations(WTime time) {
		DailyCondition dc = getDCReading(time, true);
		if (dc != null) {
			if ((dc.m_flags & DailyWeather.DAY_HOURLY_SPECIFIED) == 0) {
				dc.m_flags |= DailyWeather.DAY_HOURLY_SPECIFIED;
				clearConditions();
			}
		}
		return (dc != null);
	}

	public boolean makeDailyObservations(WTime time) {
		DailyCondition dc = getDCReading(time, true);
		if (dc != null) {
			if ((dc.m_flags & DailyWeather.DAY_HOURLY_SPECIFIED) != 0) {
				dc.m_flags &= (~(DailyWeather.DAY_HOURLY_SPECIFIED));
				clearConditions();
			}
		}
		return (dc != null);
	}

	public int isHourlyObservations(WTime time) {
		DailyCondition dc = getDCReading(time, false);
		if (dc != null) {
			if ((dc.m_flags & DailyWeather.DAY_HOURLY_SPECIFIED) != 0)
				return 1;
			return 0;
		}
		return 2;
	}

	public int isOriginFile(WTime time) {
		DailyCondition dc = getDCReading(time, false);
		if (dc != null) {
			if ((dc.m_flags & DailyWeather.DAY_ORIGIN_FILE) != 0)
				return 0;
			return 1;
		}
		return 2;
	}
	
	public int firstHourOfDay(WTime time) {
		WTime temp = new WTime(time);
		temp.purgeToDay(WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST);
		WTime start = new WTime(m_time);
		start.purgeToDay(WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST);
		if (lessThan(temp, start))
			return -1;
		if (WTime.equal(start, temp))
			return m_firstHour;
		return 0;
	}
	
	public int lastHourOfDay(WTime time) {
		WTime temp = new WTime(time);
		temp.purgeToDay(WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST);
		WTime end = new WTime(m_time);
		end = WTime.add(end, new WTimeSpan(getNumDays(), -(23 - m_lastHour) - m_firstHour, 0, 0));
		end.purgeToDay(WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST);
		if (WTime.greaterThan(temp, end))
			return -1;
		if (WTime.equal(end, temp))
			return m_lastHour;
		return 23;
	}

	public short getWarnOnSunRiseSet() {
		short retval = 0;
		DailyCondition dc = m_readings.getFirst();
		while (dc != null) {
			if ((dc.m_flags & DailyWeather.DAY_HOURLY_SPECIFIED) == 0) {
				if (dc.m_dayStart == dc.m_SunRise)
					retval |= SunriseSunsetCalc.NO_SUNRISE;
				if (add(dc.m_dayStart, new WTimeSpan(0, 23, 59, 59)) == dc.m_Sunset)
					retval |= SunriseSunsetCalc.NO_SUNSET;
			}
			dc = (DailyCondition)dc.getNext();
		}
		return retval;
	}
	
	public void setLatitude(double latitude) {
		clearConditions();
		m_worldLocation.setLatitude(latitude);
	}
	
	public void setLongitude(double longitude) {
		clearConditions();
		m_worldLocation.setLongitude(longitude);
	}
	
	public void setTimezone(long val) {
		WTimeSpan ts = new WTimeSpan(val);
		if (val > (12 * 60 * 60))						return;
		if (val < (-12 * 60 * 60))						return;
		if ((val > 0) && (ts.getSeconds() > 0))			return;
		if (WTimeSpan.equal(m_worldLocation.getTimezoneOffset(), ts))
			return;
		clearConditions();
		WTime t1 = new WTime(m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);

		m_worldLocation.setTimezoneOffset(ts);
		WTime t2 = new WTime(t1, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)-1);
		m_time = t2;
	}
	
	public void setDaylightSavings(long llvalue) {
		WTimeSpan ts = new WTimeSpan(llvalue);
		if (llvalue > (12 * 60 * 60))						return;
		if (llvalue < (-12 * 60 * 60))						return;
		if ((llvalue > 0) && (ts.getSeconds() > 0))			return;
		if (WTimeSpan.equal(m_worldLocation.getDSTAmount(), ts))
			return;
		clearConditions();
		WTime t1 = new WTime(m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);

		m_worldLocation.setDSTAmount(ts);
		WTime t2 = new WTime(t1, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)-1);
		m_time = t2;
	}
	
	public void setDaylightSavingsStart(long llvalue) {
		WTimeSpan ts = new WTimeSpan(llvalue);
		if (llvalue > (24 * 60 * 60 * 366))		return;
		if (ts.getSeconds() > 0 || ts.getMinutes() > 0 || ts.getHours() > 0)
			return;
		if (WTimeSpan.equal(m_worldLocation.getStartDST(), ts))
			return;
		clearConditions();
		WTime t1 = new WTime(m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);

		m_worldLocation.setStartDST(ts);
		WTime t2 = new WTime(t1, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)-1);
		m_time = t2;
	}
	
	public void setDaylightSavingsEnd(long llvalue) {
		WTimeSpan ts = new WTimeSpan(llvalue);
		if (llvalue > (24 * 60 * 60 * 366))		return;
		if (ts.getSeconds() > 0 || ts.getMinutes() > 0 || ts.getHours() > 0)
			return;
		if (WTimeSpan.equal(m_worldLocation.getEndDST(), ts))
			return;
		clearConditions();
		WTime t1 = new WTime(m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);

		m_worldLocation.setEndDST(ts);
		WTime t2 = new WTime(t1, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)-1);
		m_time = t2;
	}

	public boolean getInstantaneousValues(WTime time, long method, OutVariable<IWXData> wx, OutVariable<IFWIData> ifwi, OutVariable<DFWIData> dfwi) {
		assert wx != null;
		WTime nt1 = new WTime(time);
		nt1.purgeToHour(FORMAT_AS_LOCAL | FORMAT_WITHDST);
		calculateValues();

		WTime nt2 = new WTime(nt1);
		nt2.add(new WTimeSpan(0, 1, 0, 0));
		DailyCondition dc1 = getDCReading(nt1, false),
					   dc2 = getDCReading(nt2, false);

		OutVariable<Double> d1 = new OutVariable<Double>();
		OutVariable<Double> d2 = new OutVariable<Double>();
		OutVariable<Double> d3 = new OutVariable<Double>();
		OutVariable<Double> d4 = new OutVariable<Double>();
		OutVariable<Double> d7 = new OutVariable<Double>();
		OutVariable<Double> d5 = new OutVariable<Double>();
		OutVariable<Double> d6 = new OutVariable<Double>();
		OutVariable<Boolean> b1 = new OutVariable<Boolean>();

		double perc1 = 0, perc2 = 0;
		double rh1, rh2;

		if ((dc1 == null) || (dc2 == null) || (WTime.equal(nt2, time)) || ((method & GETWEATHER_INTERPOLATE.TEMPORAL)) == 0) {
			if (dc1 != null) {
				if (wx.value != null) {
					dc1.hourlyWeather(time, d1, d2, d3, d4, d7, d5, d6);
					wx.value.temperature = d1.value;
					wx.value.rh = d2.value;
					wx.value.precipitation = d3.value;
					wx.value.windSpeed = d4.value;
					wx.value.windGust = d7.value;
					wx.value.windDirection = d5.value;
					wx.value.dewPointTemperature = d6.value;
					if ((method & GETWEATHER_INTERPOLATE.TEMPORAL) != 0)
						if ((dc2 == null) && (nt1 != time))
							wx.value.precipitation = 0.0;
					wx.value.specifiedBits = IWXData.SPECIFIED.TEMPERATURE | IWXData.SPECIFIED.RH | IWXData.SPECIFIED.PRECIPITATION | IWXData.SPECIFIED.WINDSPEED | IWXData.SPECIFIED.WINDDIRECTION;
					if (wx.value.windGust >= 0.0)
						wx.value.specifiedBits |= IWXData.SPECIFIED.WINDGUST;
					if (dc1.isInterpolated(time))
						wx.value.specifiedBits |= IWXData.SPECIFIED.INTERPOLATED;
					if (dc1.isCorrected(time))
					    wx.value.specifiedBits |= IWXData.SPECIFIED.INVALID_DATA;
				}
			}
			if ((dc1 == null) || (nt1 == time) || ((method & GETWEATHER_INTERPOLATE.TEMPORAL) == 0)) {
				if (dc1 != null) {
					if (ifwi != null && ifwi.value != null) {
						ifwi.value.dFFMC	= dc1.getHourlyFFMC(time);
						ifwi.value.dISI	= dc1.getISI(time);
						ifwi.value.dFWI	= dc1.getFWI(time);
						if (dc1.isHourlyFFMCSpecified(time))	ifwi.value.dSpecifiedBits = IFWIData.SPECIFIED.FWI;
						else									ifwi.value.dSpecifiedBits = 0;
						ifwi.value.dSpecifiedBits |= (m_options & 3) << 16;
					}
				}
				if (dfwi != null && dfwi.value != null) {
					dfwi.value.dSpecifiedBits = 0;
					dailyFFMC(time, d1, b1);
					dfwi.value.dFFMC = d1.value;
					if (b1.value)
						dfwi.value.dSpecifiedBits |= DFWIData.SPECIFIED.FFMC;
					dc(time, d1, b1);
					dfwi.value.dDC = d1.value;
					if (b1.value)
						dfwi.value.dSpecifiedBits |= DFWIData.SPECIFIED.DC;
					dmc(time, d1, b1);
					dfwi.value.dDMC = d1.value;
					if (b1.value)
						dfwi.value.dSpecifiedBits |= DFWIData.SPECIFIED.DMC;
					bui(time, d1, b1);
					dfwi.value.dBUI = d1.value;
					if (b1.value)
						dfwi.value.dSpecifiedBits |= DFWIData.SPECIFIED.BUI;
					getDailyISI(time, d1);
					dfwi.value.dISI = d1.value;
					getDailyFWI(time, d1);
					dfwi.value.dFWI = d1.value;
					if ((dfwi.value.dFFMC >= 0.0) && (dfwi.value.dISI == -1.0) && (dc1 != null)) {
						assert dc1.getPrevious().getPrevious() == null;
						WTime dayNeutral = new WTime(dc1.m_dayStart, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)1);
						WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL, (short)-1);
						WTime dayNoon = new WTime(dayLST);
						dayNoon.add(new WTimeSpan(0, 12, 0, 0));
						double ws = dc1.getHourlyWS(dayNoon);
						dfwi.value.dISI = Fwi.isiFBP(dfwi.value.dFFMC, ws, 24 * 60 * 60);
						dfwi.value.dFWI = Fwi.fwi(dfwi.value.dISI, dfwi.value.dBUI);
					}
				}
				return (dc1 != null);
			}
			rh1 = rh2 = wx.value.rh;
		}
		else {
			double t1, ws1, gust1, wd1, dew1;
			double t2, p2, ws2, gust2, wd2, dew2;
			dc1.hourlyWeather(nt1, d1, d2, d3, d4, d7, d5, d6);
			t1 = d1.value;
			rh1 = d2.value;
			ws1 = d4.value;
			gust1 = d7.value;
			wd1 = d5.value;
			dew1 = d6.value;
			dc2.hourlyWeather(nt2, d1, d2, d3, d4, d7, d5, d6);
			t2 = d1.value;
			rh2 = d2.value;
			p2 = d3.value;
			ws2 = d4.value;
			gust2 = d7.value;
			wd2 = d5.value;
			dew2 = d6.value;
			perc2 = ((double)(time.getTime((short)0) - nt1.getTime((short)0))) / 3600.0;
			perc1 = 1.0 - perc2;

			if (wx.value == null)
				wx.value = new IWXData();

			wx.value.temperature = t1 * perc1 + t2 * perc2;
			wx.value.dewPointTemperature = dew1 * perc1 + dew2 * perc2;
			wx.value.rh = rh1 * perc1 + rh2 * perc2;
			wx.value.precipitation = p2 * perc2;

			boolean bb1 = ((ws1 < 0.0001) && (wd1 < 0.0001));
			boolean bb2 = ((ws2 < 0.0001) && (wd2 < 0.0001));
			double wd_diff = NORMALIZE_ANGLE_RADIAN(wd2 - wd1);

			if (bb1)		wx.value.windDirection = wd2;
			else if (bb2)	wx.value.windDirection = wd1;
			else {
				if ((ws1 >= 0.0001) && (ws2 >= 0.0001) && ((wd_diff < DEGREE_TO_RADIAN(181.0)) && (wd_diff > DEGREE_TO_RADIAN(179.0)))) {
					WTimeSpan ts = subtract(nt2, nt1);
					ts.divide(2);
					if (lessThanEqualTo(time, add(nt1, ts)))
						wx.value.windDirection = wd1;
					else
						wx.value.windDirection = wd2;
				}
				else {
					if (wd_diff > General.PI)
						wd_diff -= TWO_PI;
					wx.value.windDirection = NORMALIZE_ANGLE_RADIAN(wd2 - perc1 * wd_diff);
				}
			}

			if ((ws1 >= 0.0001) && (ws2 >= 0.0001) && ((wd_diff < DEGREE_TO_RADIAN(181.0)) && (wd_diff > DEGREE_TO_RADIAN(179.0)))) {
				WTimeSpan ts = subtract(nt2, nt1);
				ts.divide(2);
				if (lessThanEqualTo(time, add(nt1, ts)))
					wx.value.windSpeed = ws1;
				else
					wx.value.windSpeed = ws2;
			}
			else
				wx.value.windSpeed = ws1 * perc1 + ws2 * perc2;
			wx.value.specifiedBits = 0;
			if (dc1.isInterpolated(time))
				wx.value.specifiedBits |= IWXData.SPECIFIED.INTERPOLATED;
		}

		if (dfwi != null) {
			if (dfwi.value == null)
				dfwi.value = new DFWIData();

			dfwi.value.dSpecifiedBits = 0;
			dailyFFMC(time, d1, b1);
			if (b1.value) {
				dfwi.value.dFFMC = d1.value;
				dfwi.value.dSpecifiedBits |= DFWIData.SPECIFIED.FFMC;
			}
			dc(time, d1, b1);
			if (b1.value) {
				dfwi.value.dDC = d1.value;
				dfwi.value.dSpecifiedBits |= DFWIData.SPECIFIED.DC;
			}
			dmc(time, d1, b1);
			if (b1.value) {
				dfwi.value.dDMC = d1.value;
				dfwi.value.dSpecifiedBits |= DFWIData.SPECIFIED.DMC;
			}
			bui(time, d1, b1);
			if (b1.value) {
				dfwi.value.dBUI = d1.value;
				dfwi.value.dSpecifiedBits |= DFWIData.SPECIFIED.BUI;
			}
			getDailyISI(time, d1);
			dfwi.value.dISI = d1.value;
			getDailyFWI(time, d1);
			dfwi.value.dFWI = d1.value;
			
			if ((dfwi.value.dFFMC >= 0.0) && (dfwi.value.dISI == -1.0) && (dc1 != null)) {
				assert dc1.getPrevious().getPrevious() == null;
				WTime dayNeutral = new WTime(dc1.m_dayStart, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)1);
				WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL, (short)-1);
				WTime dayNoon = new WTime(dayLST);
				dayNoon.add(new WTimeSpan(0, 12, 0, 0));
				double ws = dc1.getHourlyWS(dayNoon);
				dfwi.value.dISI = Fwi.isiFBP(dfwi.value.dFFMC, ws, 24 * 60 * 60);
				dfwi.value.dFWI = Fwi.fwi(dfwi.value.dISI, dfwi.value.dBUI);
			}
		}

		if (ifwi != null) {
			if (ifwi.value == null)
				ifwi.value = new IFWIData();

			if (ifwi.value != null) {
				ifwi.value.dSpecifiedBits = 0;
				double	ffmc1 = dc1.getHourlyFFMC(nt1),
						ffmc2 = (dc2 != null) ? dc2.getHourlyFFMC(nt2) : ffmc1;
				boolean	fs2 = (dc2 != null) ? dc2.isHourlyFFMCSpecified(nt2) : false;

				if (fs2) {
					ifwi.value.dSpecifiedBits |= IFWIData.SPECIFIED.FWI;
					ifwi.value.dFFMC = ffmc1 * perc1 + ffmc2 * perc2;
				}
				else {
					switch ((int)(m_options & 0x00000003)) {
					case 2:
						WTime dayStart = new WTime(time);
						dayStart.purgeToDay(FORMAT_AS_LOCAL | FORMAT_WITHDST);
						double prev_hr_ffmc;
						dailyFFMC(dayStart , d1, b1);

						dailyFFMC(add(dayStart, new WTimeSpan(0, 18, 0, 0)), d2, b1);

						WTime prevLoop = new WTime(nt2), loopStop = new WTime(nt2);
						loopStop.subtract(new WTimeSpan(0, 48, 0, 0));

						prev_hr_ffmc = ffmc1;

						double[] rain48 = new double[48];
						for (short ii = 0; greaterThan(prevLoop, loopStop); prevLoop.subtract(WTimeSpan.Hour), ii++)
							rain48[ii] = getHourlyRain(prevLoop);

						ifwi.value.dFFMC = Fwi.hourlyFFMCHybrid(d1.value, d2.value, prev_hr_ffmc, rain48, wx.value.temperature, wx.value.rh, wx.value.windSpeed,
						    time.getTimeOfDay(FORMAT_AS_LOCAL).getTotalSeconds());
						break;

					case 3:
						dayStart = new WTime(time);
						dayStart.purgeToDay(FORMAT_AS_LOCAL | FORMAT_WITHDST);
						WTime dayNeutral = new WTime(dayStart, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)1);
						WTime dayStartLst = new WTime(dayNeutral, FORMAT_AS_LOCAL, (short)-1);

						dailyFFMC(dayStart , d1, b1);
						dailyFFMC(add(dayStart, new WTimeSpan(0, 18, 0, 0)), d2, b1);

						ifwi.value.dFFMC = Fwi.hourlyFFMCLawsonContiguous(d1.value, d2.value, rh1, wx.value.rh, rh2, subtract(time, dayStartLst).getTotalSeconds());
						break;

					default:
						double in_ffmc = ffmc1;
						ifwi.value.dFFMC = Fwi.hourlyFFMCVanWagner(in_ffmc, wx.value.precipitation, wx.value.temperature, wx.value.rh, wx.value.windSpeed,
						    subtract(time, nt1).getTotalSeconds());
						break;
					}
				}

				ifwi.value.dISI = Fwi.isiFBP(ifwi.value.dFFMC, wx.value.windSpeed, time.getTotalSeconds());
				ifwi.value.dFWI = Fwi.fwi(ifwi.value.dISI, dfwi.value.dBUI);
			}
		}
		return true;
	}

	public boolean hourlyFFMC(WTime time, OutVariable<Double> ffmc) {
		assert ffmc != null;
		DailyCondition _dc = getDCReading(time, false);
		if (_dc != null) {
			calculateValues();
			ffmc.value = _dc.getHourlyFFMC(time);
		}
		else if (lessThan(time, m_time) && equal(m_initialHFFMCTime, new WTimeSpan(-1 * 60 * 60))) {
			ffmc.value = m_initialHFFMC;
			return true;
		}
		return (_dc != null);
	}

	public boolean dailyFFMC(WTime time, OutVariable<Double> ffmc, OutVariable<Boolean> specified) {
		assert ffmc != null;
		assert specified != null;
		WTime dayNeutral = new WTime(time, FORMAT_AS_LOCAL, (short)1);
		WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)-1);
		WTime dayNoon = new WTime(dayLST);
		dayNoon.subtract(new WTimeSpan(0, 12, 0, 0));
		dayNoon.purgeToDay(FORMAT_AS_LOCAL | FORMAT_WITHDST);
		DailyCondition _dc = getDCReading(dayNoon, false);
		if (_dc != null) {
			calculateValues();
			ffmc.value = _dc.getDailyFFMC();
			specified.value = _dc.isDailyFFMCSpecified();
		}
		else if (lessThan(dayNoon, m_time)) {
			ffmc.value = m_spec_day.dFFMC;
			specified.value = true;
			return true;
		}
		return (_dc != null);
	}

	public boolean dc(WTime time, OutVariable<Double> dc, OutVariable<Boolean> specified) {
		assert dc != null;
		assert specified != null;
		WTime dayNeutral = new WTime(time, FORMAT_AS_LOCAL, (short)1);
		WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)-1);
		WTime dayNoon = new WTime(dayLST);
		dayNoon.subtract(new WTimeSpan(0, 12, 0, 0));
		dayNoon.purgeToDay(FORMAT_AS_LOCAL | FORMAT_WITHDST);
		DailyCondition _dc = getDCReading(dayNoon, false);
		if (_dc != null) {
			calculateValues();
			dc.value = _dc.getDC();
			specified.value = _dc.isDCSpecified();
		} else if (lessThan(dayNoon, m_time)) {
			dc.value = m_spec_day.dDC;
			specified.value = true;
			return true;
		}
		return (_dc != null);
	}

	public boolean dmc(WTime time, OutVariable<Double> dmc, OutVariable<Boolean> specified) {
		assert dmc != null;
		assert specified != null;
		WTime dayNeutral = new WTime(time, FORMAT_AS_LOCAL, (short)1);
		WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)-1);
		WTime dayNoon = new WTime(dayLST);
		dayNoon.subtract(new WTimeSpan(0, 12, 0, 0));
		dayNoon.purgeToDay(FORMAT_AS_LOCAL | FORMAT_WITHDST);
		DailyCondition _dc = getDCReading(dayNoon, false);
		if (_dc != null) {
			calculateValues();
			dmc.value = _dc.getDMC();
			specified.value = _dc.isDMCSpecified();
		} else if (lessThan(dayNoon, m_time)) {
			dmc.value = m_spec_day.dDMC;
			specified.value = true;
			return true;
		}
		return (_dc != null);
	}

	public boolean bui(WTime time, OutVariable<Double> bui, OutVariable<Boolean> specified) {
		return bui(time, bui, specified, true);
	}

	public boolean bui(WTime time, OutVariable<Double> bui, OutVariable<Boolean> specified, boolean recalculate) {
		assert bui != null;
		assert specified != null;
		WTime dayNeutral = new WTime(time, FORMAT_AS_LOCAL, (short)1);
		WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)-1);
		WTime dayNoon = new WTime(dayLST);
		dayNoon.subtract(new WTimeSpan(0, 12, 0, 0));
		dayNoon.purgeToDay(FORMAT_AS_LOCAL | FORMAT_WITHDST);
		DailyCondition _dc = getDCReading(dayNoon, false);
		if (_dc != null) {
			if (recalculate)
				calculateValues();
			bui.value = _dc.getBUI();
			specified.value = _dc.isBUISpecified();
		}
		else if (lessThan(dayNoon, m_time)) {
			if (m_spec_day.dBUI < 0.0) {
				bui.value = Fwi.bui(m_spec_day.dDC, m_spec_day.dDMC);
				specified.value = false;
			}
			else {
				bui.value = m_spec_day.dBUI;
				specified.value = true;
			}
			return true;
		}
		return (_dc != null);
	}

	public boolean getHourlyISI(WTime time, OutVariable<Double> isi) {
		assert isi != null;
		DailyCondition _dc = getDCReading(time, false);
		if (_dc != null) {
			calculateValues();
			isi.value = _dc.getISI(time);
			return true;
		}
		return false;
	}
	
	public boolean getDailyISI(WTime time, OutVariable<Double> isi) {
		WTime dayNeutral = new WTime(time, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)1);
		WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL, (short)-1);
		WTime dayNoon = new WTime(dayLST);
		dayNoon.subtract(new WTimeSpan(0, 12, 0, 0));
		dayNoon.purgeToDay(WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST);
		DailyCondition _dc = getDCReading(dayNoon, false);
		if (_dc != null) {
			calculateValues();
			isi.value = _dc.getDailyISI();
			return true;
		}
		else if (lessThan(dayNoon, m_time)) {
			isi.value = -1.0;
			return true;
		}
		return false;
	}

	public boolean getHourlyFWI(WTime time, OutVariable<Double> fwi) {
		assert fwi != null;
		DailyCondition _dc = getDCReading(time, false);
		if (_dc != null) {
			calculateValues();
			fwi.value = _dc.getFWI(time);
			return true;
		}
		return false;
	}
	
	public boolean getDailyFWI(WTime time, OutVariable<Double> fwi) {
		WTime dayNeutral = new WTime(time, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)1);
		WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL, (short)-1);
		WTime dayNoon = new WTime(dayLST);
		dayNoon.subtract(new WTimeSpan(0, 12, 0, 0));
		dayNoon.purgeToDay(WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST);
		DailyCondition _dc = getDCReading(dayNoon, false);
		if (_dc != null) {
			calculateValues();
			fwi.value = _dc.getDailyFWI();
			return true;
		}
		else if (lessThan(dayNoon, m_time)) {
			fwi.value = -1.0;
			return true;
		}
		return false;
	}

	public boolean anyFWICodesSpecified() {
		DailyCondition dc = m_readings.getFirst();
		while (dc != null) {
			if (dc.anyFWICodesSpecified())
				return true;
			dc = (DailyCondition)dc.getNext();
		}
		return false;
	}

	public void clearConditions() {
		m_isCalculatedValuesValid = false;
	}

	public void clearWeatherData() {
		while (m_readings.removeFirst() != null);
	}
	
	static class WeatherCollection {
		public double hour;
		public long epoch;
		public double temp;
		public double rh;
		public double wd;
		public double ws;
		public double wg;
		public double precip;
		public double ffmc;
		public double DMC;
		public double DC;
		public double BUI;
		public double ISI;
		public double FWI;
		public int options;
		
		public WeatherCollection() {
			hour = 0;
			epoch = 0;
			temp = 0;
			rh = 0;
			wd = 0;
			ws = 0;
			wg = -1.0;
			precip = 0;
			ffmc = -1.0;
			DMC = -1.0;
			DC = -1.0;
			BUI = -1.0;
			ISI = -1.0;
			FWI = -1.0;
			options = 0;
		}

		public boolean isValid() {
			if ((wd < 0.0) || (wd > 360.0) ||
				(ws < 0.0) ||
				(rh < 0.0) || (rh > 100.0) ||
				(precip < 0.0) ||
				(temp < -50.0) || (temp > 60.0) ||
				((DMC < 0.0) && (DMC != -1.0)) || ((DC < 0.0) && (DC != -1.0)) ||
				(DMC > 500.0) || (DC > 1500.0))
				return false;
			return true;
		}
		
		public void fix() {
		    wd = constrainToRange(wd, 0.0, 360.0);
		    ws = constrainToRange(ws, 0.0, Double.MAX_VALUE);
			wg = constrainToRange(wg, 0.0, Double.MAX_VALUE);
		    rh = constrainToRange(rh, 0.0, 100.0);
		    precip = constrainToRange(precip, 0.0, Double.MAX_VALUE);
		    temp = constrainToRange(temp, -50.0, 60.0);
		    if (DMC != -1.0)
		        DMC = constrainToRange(DMC, 0.0, 500.0);
		    if (DC != -1.0)
		        DC = constrainToRange(DC, 0.0, 1500.0);
		}
	}
    
    public List<WeatherCollection> importHourly(String fileName, OutVariable<Long> hr) throws IOException {
        return importHourly(fileName, hr, INVALID_FAILURE);
    }
	
	public List<WeatherCollection> importHourly(String fileName, OutVariable<Long> hr, int invalidHandle) throws IOException {
		List<WeatherCollection> retval = new ArrayList<WeatherCondition.WeatherCollection>();
		OutVariable<Integer> hour = new OutVariable<Integer>();
		List<String> header = new ArrayList<String>();

		WTime dayNoon = new WTime(m_timeManager);
		hour.value = 0;
		
		if (m_readings.size() != 0) {
			WTime dayNeutral = new WTime(m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);
			WTime dayLST = new WTime(dayNeutral, WTime.FORMAT_AS_LOCAL, (short)-1);
			dayNoon = dayLST;
			dayNoon.add(new WTimeSpan(0, 12, 0, 0));
		}
		
		Long hr1 = null;

		File f = new File(fileName);
		BufferedReader rdr = null;
		try {
			if (f != null && f.exists()) {
				rdr = new BufferedReader(new FileReader(f));
				String file_type;
				String line;
				
				do {
					if ((line = rdr.readLine()) == null) {
						rdr.close();
						hr.value = ERROR.READ_FAULT | ERROR.SEVERITY_WARNING;
						return null;
					}
					header.clear();
					processHeader(line, header);
				} while (!header.get(0).equalsIgnoreCase("hourly") && !header.get(0).equalsIgnoreCase("date"));
			
				int lasthour = -1;
				List<Integer> missinghours = new ArrayList<Integer>();
				boolean startTimeSpecified = !m_readings.isEmpty();
		
				while ((line = rdr.readLine()) != null) {
					if (line.length() == 0)
						continue;
					for (int i = line.length() - 1; line.charAt(i) == ' ' || line.charAt(i) == 0x0a || line.charAt(i) == 0x0d || line.charAt(i) == ',' ||
							line.charAt(i) == '\t' || line.charAt(i) == ';' || line.charAt(i) == 0x22; i--)
						line = line.substring(0, line.length() - 1);
					if (line.length() == 0)
						continue;
					WeatherCollection coll = new WeatherCollection();
					OutVariable<Integer> min = new OutVariable<Integer>();
					min.value = 0;
					OutVariable<Integer> sec = new OutVariable<Integer>();
					sec.value = 0;
					OutVariable<Double> rh = new OutVariable<Double>();
					rh.value = 0.0;
					OutVariable<Double> precip = new OutVariable<Double>();
					precip .value = 0.0;
					OutVariable<Double> wd = new OutVariable<Double>();
					wd.value =  0.0;
					OutVariable<Double> temp = new OutVariable<Double>();
					temp.value = 0.0;
					OutVariable<Double> ws = new OutVariable<Double>();
					ws.value = 0.0;
					OutVariable<Double> wg = new OutVariable<Double>();
					wg.value = -1.0;
					OutVariable<Double> ffmc = new OutVariable<Double>();
					ffmc.value = -1.0;
					OutVariable<Double> DMC = new OutVariable<Double>();
					DMC.value = -1.0;
					OutVariable<Double> DC = new OutVariable<Double>();
					DC.value = -1.0;
					OutVariable<Double> BUI = new OutVariable<Double>();
					BUI.value = -1.0;
					OutVariable<Double> ISI = new OutVariable<Double>();
					ISI.value = -1.0;
					OutVariable<Double> FWI = new OutVariable<Double>();
					FWI.value = -1.0;
					file_type = fillLineValues(header, line, hour, min, sec, temp, rh, wd, ws, wg, precip, ffmc, DMC, DC, BUI, ISI, FWI);
					coll.temp = temp.value;
					coll.rh = rh.value;
					coll.wd = wd.value;
					coll.ws = ws.value;
					coll.wg = wg.value;
					coll.precip = precip.value;
					coll.ffmc = ffmc.value;
					coll.DMC = DMC.value;
					coll.DC = DC.value;
					coll.BUI = BUI.value;
					coll.ISI = ISI.value;
					coll.FWI = FWI.value;
		
					if (!coll.isValid()) {
                        hr1 = ERROR.INVALID_DATA;
                        if (invalidHandle == INVALID_FAILURE) {
                            hr1 |= ERROR.SEVERITY_WARNING;
                            break;
                        }
                        else if (invalidHandle == INVALID_FIX) {
                            coll.fix();
                            coll.options |= IWXData.SPECIFIED.INVALID_DATA;
                        }
                        else
                            coll.options |= IWXData.SPECIFIED.INVALID_DATA;
					}
					if ((hour.value < 0) || (hour.value > 23)) {
						hr1 = ERROR.INVALID_DATA | ERROR.SEVERITY_WARNING;
						break;
					}

					coll.wd = DEGREE_TO_RADIAN(COMPASS_TO_CARTESIAN_DEGREE(coll.wd));
					coll.rh *= 0.01;
					
					WTime t = new WTime(m_time);
					t.parseDateTime(file_type, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST);
					t.add(new WTimeSpan(0, hour.value, 0, 0));
					t.add(new WTimeSpan(0, 0, min.value, 0));
					t.add(new WTimeSpan(0, 0, 0, sec.value));
					
					coll.epoch = t.getTotalMicroSeconds();
					
					retval.add(coll);
				}
				
				if (retval.size() > 0) {
					Collections.sort(retval, new Comparator<WeatherCollection>() {
						@Override
						public int compare(WeatherCollection arg0, WeatherCollection arg1) {
							return (int)((arg0.epoch / 1000000L) - (arg1.epoch / 1000000L));
						}
					});
					
					if (!startTimeSpecified) {
						m_time = new WTime(retval.get(0).epoch, m_timeManager, false);
						WTime testTime = new WTime(m_time);
						m_time.purgeToDay(WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST);
						
						WTime dayNeutral = new WTime(m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);
						WTime dayLST = new WTime(dayNeutral, WTime.FORMAT_AS_LOCAL, (short)-1);
						dayNoon = dayLST;
						dayNoon.add(new WTimeSpan(0, 12, 0, 0));
						
						if (greaterThan(testTime, dayNoon)) {
							hr.value = ERROR.START_AFTER_NOON | ERROR.SEVERITY_WARNING;
						}
					}

					if (hr.value == 0) {
						boolean firstloop = true;
						for (int i = 0; i < retval.size(); i++) {
							WTime t = new WTime(retval.get(i).epoch, m_timeManager, false);
							if (lessThan(t, m_time)) {
								hr.value = ERROR.ATTEMPT_PREPEND;
								break;
							}
							WTimeSpan s = subtract(t, m_time);
							retval.get(i).hour = ((double)s.getTotalSeconds()) / 3600.0;
							if (firstloop) {
								lasthour = (int)(retval.get(i).hour) - 1;
								firstloop = false;
							}
							int count = 0;
							//if there were missing hours add them to the list.
							while (lasthour < (retval.get(i).hour - 1)) {
								lasthour++;
								count++;
								missinghours.add(lasthour);
							}
							//if there are more than 5 in a row reject the data set.
							if (count > 5) {
								hr.value = ERROR.INVALID_DATA | ERROR.SEVERITY_WARNING;
								break;
							}
							lasthour = (int)Math.floor(retval.get(i).hour);
						}
						
						if (hr.value == 0) {
							int noonlst = 12;
							if (m_timeManager.getWorldLocation().getEndDST().getTotalSeconds() > 0)
								noonlst = 13;
							//error on start days with no data for noon LST and for non-hour first times.
							if ((((int)retval.get(0).hour) % 24) > noonlst || (retval.get(0).hour != Math.floor(retval.get(0).hour))) {
								hr.value = ERROR.INVALID_DATA | ERROR.SEVERITY_WARNING;
							}
							else {
								//if there are missing hours, interpolate their values.
								if (missinghours.size() > 0) {
									hr.value = ERROR.INTERPOLATE;
									HourValue[] temps = new HourValue[retval.size()];
									HourValue[] rhs = new HourValue[retval.size()];
									HourValue[] wss = new HourValue[retval.size()];
									HourValue[] wgs = new HourValue[retval.size()];
									for (int i = 0; i < retval.size(); i++) {
										temps[i] = new HourValue();
										temps[i].houroffset = retval.get(i).hour;
										temps[i].value = retval.get(i).temp;
										rhs[i] = new HourValue();
										rhs[i].houroffset = retval.get(i).hour;
										rhs[i].value = retval.get(i).rh;
										wss[i] = new HourValue();
										wss[i].houroffset = retval.get(i).hour;
										wss[i].value = retval.get(i).ws;
										wgs[i] = new HourValue();
										wgs[i].houroffset = retval.get(i).hour;
										wgs[i].value = retval.get(i).wg;
									}
									Interpolator inter = new Interpolator();
									HourValue[] newtemps = inter.splineInterpolate(temps);
									HourValue[] newrhs = inter.splineInterpolate(rhs);
									HourValue[] newwss = inter.splineInterpolate(wss);
									HourValue[] newwgs = inter.splineInterpolate(wgs);
									
									//get rid of unneeded times
									for (int i = retval.size() - 1; i >= 0; i--) {
										if (retval.get(i).hour != Math.floor(retval.get(i).hour))
											retval.remove(i);
									}
									
									//add the interpolated hours to the list of data.
									for (final int h : missinghours) {
										WeatherCollection coll = new WeatherCollection();
										HourValue index2 = CollectionExtensions.find(newtemps, new CollectionExtensions.Selector<HourValue>() {
											@Override
											public boolean testEqual(HourValue value) {
												return value.houroffset == h;
											}
										});
										if (index2 == null)
											continue;
										int index = (int)(index2.houroffset - retval.get(0).hour);
										WTime t = new WTime(retval.get(0).epoch, m_timeManager, false);
										t.add(new WTimeSpan(0, index, 0, 0));
										coll.options |= IWXData.SPECIFIED.INTERPOLATED;
										coll.hour = h;
										coll.temp = newtemps[index].value;
										coll.precip = 0;
										coll.rh = newrhs[index].value;
										coll.ws = newwss[index].value;
										coll.wg = newwgs[index].value;
										coll.wd = retval.get(index - 1).wd;
										coll.DMC = -1;
										coll.DC = -1;
										coll.BUI = -1;
										coll.ISI = -1;
										coll.FWI = -1;
										coll.ffmc = -1;
										coll.fix();
										retval.add(index, coll);
									}
								}
							}
						}
					}
				}
				
				rdr.close();
			}
		}
		catch (Exception e) {
			hr1 = ERROR.INVALID_DATA;
			if (rdr != null) {
				try {
					rdr.close();
				}
				catch (IOException ex) {
					ex.printStackTrace();
				}
			}
		}
		
		if ((hr.value == 0 || hr.value == ERROR.INTERPOLATE) && hr1 != null) {
			if (hr.value == 0)
				hr.value = hr1;
			else
				hr.value = ERROR.INTERPOLATE_BEFORE_INVALID_DATA;
		}
		
		return retval;
	}

	public long importFile(String fileName, int options) throws IOException {
		List<String> header = new ArrayList<String>();
		DailyCondition dc;
		long hr = ERROR.S_OK;
		boolean can_append = (options & WEATHERSTREAM_IMPORT.SUPPORT_APPEND) != 0;

		OutVariable<Integer> hour = new OutVariable<Integer>();

		if (m_readings.isEmpty())
			can_append = true;

		if ((options & WEATHERSTREAM_IMPORT.PURGE) != 0) {
			clearWeatherData();
			can_append = true;
		}
		
		mode = 0;
		
		long lines = 0;
		File f = new File(fileName);
		BufferedReader rdr = null;
		if (f.exists()) {
			try {
				rdr = new BufferedReader(new FileReader(f));
				String file_type;
				String line;
	
				do {
					if ((line = rdr.readLine()) == null) {
						rdr.close();
						return ERROR.READ_FAULT | ERROR.SEVERITY_WARNING;
					}
					
					header.clear();
					processHeader(line, header);
					String flagStr;
					flagStr = header.get(0);
					if (flagStr.compareToIgnoreCase("daily")==0)
						mode = 1;
					else if (flagStr.compareToIgnoreCase("hourly")==0)
						mode = 2;
					else if (flagStr.compareToIgnoreCase("date") == 0) {
						mode = 1;
						for (int ii = 1; ii < header.size(); ii++) {
							String str = header.get(ii);
							if (str.compareToIgnoreCase("hour") == 0 || str.compareToIgnoreCase("Time(CST)") == 0) {
								mode = 2;
								break;
							}
						}
					}
					else if (isSupportedFormat(line, header))
						mode = 3;
				} while (mode == 0);
	
				if (mode == 1) {
					WTime lastTime = new WTime(0L, m_timeManager);
					if (m_readings.size() > 0)
						lastTime = add(m_time, new WTimeSpan(m_readings.size(), 0, 0, 0));
					m_firstHour = 0;
					m_lastHour = 23;
	
					while ((line = rdr.readLine()) != null) {
						for(int i = line.length() - 1; line.charAt(i) == ' ' || line.charAt(i) == 0x0a || line.charAt(i) == 0x0d ||
								line.charAt(i) == ',' || line.charAt(i) == '\t' || line.charAt(i) == ';' || line.charAt(i) == 0x22; i--)
							line = line.substring(0, line.length() - 1);
						if (line.length() == 0)
							continue;
						OutVariable<Double> min_temp = new OutVariable<Double>();
						min_temp.value = -100.0;
						OutVariable<Double> max_temp = new OutVariable<Double>();
						max_temp .value = -100.0;
						OutVariable<Double> min_ws = new OutVariable<Double>();
						min_ws .value =  -100.0;
						OutVariable<Double> max_ws = new OutVariable<Double>();
						max_ws.value = -100.0;
						OutVariable<Double> min_wg = new OutVariable<Double>();
						min_wg .value =  -100.0;
						OutVariable<Double> max_wg = new OutVariable<Double>();
						max_wg.value = -100.0;
						OutVariable<Double> rh = new OutVariable<Double>();
						rh.value = -100.0;
						OutVariable<Double> precip = new OutVariable<Double>();
						precip.value = -100.0;
						OutVariable<Double> wd = new OutVariable<Double>();
						wd.value = -100.0;
	
						file_type = fillDailyLineValue(header, line, min_temp, max_temp, rh, precip, min_ws, max_ws, min_wg, max_wg, wd);
	
						if ((wd.value < 0.0) || (wd.value > 360.0) ||
						    (min_ws.value < 0.0) || (max_ws.value < 0.0) ||
						    (rh.value < 0.0) || (rh.value > 100.0) ||
						    (precip.value < 0.0) ||
						    (min_temp.value < -50.0) || (min_temp.value > 60.0) ||
						    (max_temp.value < -50.0) || (max_temp.value > 60.0)) {
							hr = ERROR.INVALID_DATA | ERROR.SEVERITY_WARNING;
							break;
						}
	
						wd.value = DEGREE_TO_RADIAN(COMPASS_TO_CARTESIAN_DEGREE(wd.value));
						if ((max_ws.value > 0.0) && (wd.value == 0.0))
							wd.value = TWO_PI;
						rh.value *= 0.01;
	
						if (m_readings.isEmpty())
							m_time.parseDateTime(file_type, FORMAT_AS_LOCAL | FORMAT_WITHDST);
	
						WTime t = new WTime(m_time);
						t.parseDateTime(file_type, FORMAT_AS_LOCAL | FORMAT_WITHDST);
	
						WTime endTime = new WTime(m_time);
						endTime = getEndTime();
						endTime.add(WTimeSpan.Second);
	
						if (lessThan(t, m_time)) {
							hr = ERROR.ATTEMPT_PREPEND;
							break;
						}
	
						if (lastTime.getTotalSeconds() > 0) {
							if (lessThan(t, lastTime) && ((options & WEATHERSTREAM_IMPORT.SUPPORT_OVERWRITE) == 0)) {
								hr = ERROR.ATTEMPT_OVERWRITE;
								break;
							}
							if (((lines == 0) && greaterThan(t, lastTime)) || ((lines != 0) && (notEqual(t, add(lastTime, WTimeSpan.Day))))) {
								hr = ERROR.INVALID_TIME | ERROR.SEVERITY_WARNING;
								break;
							}
						}
	
						lastTime = t;
	
						dc = getDCReading(t, can_append);
						if (dc == null) {
							hr = ERROR.ATTEMPT_APPEND;
							break;
						}
						makeDailyObservations(t);
						dc.m_flags |= DailyWeather.DAY_ORIGIN_FILE;
	
						if (min_temp.value > max_temp.value)
						{
							double ttt = min_temp.value;
							min_temp.value = max_temp.value;
							max_temp.value = ttt;
						}
						if (min_ws.value > max_ws.value)
						{
							double ttt = min_ws.value;
							min_ws.value = max_ws.value;
							max_ws.value = ttt;
						}
						if (min_wg.value > max_wg.value)
						{
							double ttt = min_wg.value;
							min_wg.value = max_wg.value;
							max_wg.value = ttt;
						}
	
						dc.setDailyWeather(min_temp.value, max_temp.value, min_ws.value, max_ws.value, min_wg.value, max_wg.value, rh.value, precip.value, wd.value);
						lines++;
					}
				}
				else if (mode == 2) {
					rdr.close();
					OutVariable<Long> h = new OutVariable<Long>();
					h.value = ERROR.S_OK;
					int errorHandling = INVALID_FAILURE;
					if ((options & WEATHERSTREAM_IMPORT.INVALID_ALLOW) != 0) {
					    errorHandling = INVALID_ALLOW;
					}
					else if ((options & WEATHERSTREAM_IMPORT.INVALID_FIX) != 0) {
					    errorHandling = INVALID_FIX;
					}
					List<WeatherCollection> weather = importHourly(fileName, h, errorHandling);
					hr = h.value;
					int noonhour = -1;
					
					if ((hr & ERROR.SEVERITY_WARNING) == 0)
					{
						WTime dayNoon = new WTime(m_timeManager);
						WTime lastTime = new WTime(0, m_timeManager);
						
						if (m_readings.size() != 0) {
							lastTime = add(m_time, new WTimeSpan(m_readings.size(), -(24 - m_lastHour), 0, 0));
							
							WTime dayNeutral = new WTime(m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);
							WTime dayLST = new WTime(dayNeutral, WTime.FORMAT_AS_LOCAL, (short)-1);
							dayNoon = dayLST;
							dayNoon.add(new WTimeSpan(0, 12, 0, 0));
							noonhour = (int)dayNoon.getHour(WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST);
						}
						else {
							m_time = new WTime(weather.get(0).epoch, m_timeManager, false);
							m_time.purgeToDay(WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST);
							m_firstHour = (int) weather.get(0).hour;
							
							WTime dayNeutral = new WTime(m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);
							WTime dayLST = new WTime(dayNeutral, WTime.FORMAT_AS_LOCAL, (short)-1);
							dayNoon = dayLST;
							dayNoon.add(new WTimeSpan(0, 12, 0, 0));
							
							noonhour = (int)dayNoon.getHour(WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST);
						}
						
						lines = 0;
						//finally add all of the weather to the stream.
						for (WeatherCollection w : weather) {
							WTime t = new WTime(m_time);
							t.add(new WTimeSpan(0, (int)w.hour, 0, 0));
							hour.value = ((int)w.hour) % 24;
							
							if (lastTime.getTotalSeconds() > 0) {
								if ((WTime.lessThan(t, lastTime) && ((options & WEATHERSTREAM_IMPORT.SUPPORT_OVERWRITE) == 0))) {
									hr = ERROR.ATTEMPT_OVERWRITE;
									break;
								}
								if (((lines == 0) && (WTime.greaterThan(t, lastTime))) || ((lines != 0) && (WTime.notEqual(t, lastTime.add(new WTimeSpan(0, 1, 0, 0)))))) {
									hr = ERROR.INVALID_TIME | ERROR.SEVERITY_WARNING;
									break;
								}
							}
							
							lastTime = t;
							dc = getDCReading(t, can_append);
							if (dc == null) {
								hr = ERROR.ATTEMPT_APPEND;
								break;
							}
							makeHourlyObservations(t);
							dc.m_flags |= DailyWeather.DAY_ORIGIN_FILE;
	
							if ((w.options & IWXData.SPECIFIED.INTERPOLATED) != 0) {
								dc.setHourlyInterpolation(hour.value);
							}
			                else if ((w.options & IWXData.SPECIFIED.INVALID_DATA) != 0) {
			                    dc.setHourlyCorrected(hour.value);
			                }
							if (w.DMC >= 0.0) {
								if ((WTime.equal(lastTime, m_time) && (hour.value == 0)) || lines == 0)
									m_spec_day.dDMC = w.DMC;
								else if (hour.value == noonhour)
									dc.setDMC(w.DMC);
								m_options |= 0x00000004;
							}
							if (w.DC >= 0.0) {
								if ((WTime.equal(lastTime, m_time) && (hour.value == 0)) || lines == 0)
									m_spec_day.dDC = w.DC;
								else if (hour.value == noonhour)
									dc.setDC(w.DC);
								m_options |= 0x00000004;
							}
							if (w.BUI >= 0.0) {
								if ((WTime.equal(lastTime, m_time) && (hour.value == 0)) || lines == 0)
									m_spec_day.dBUI = w.BUI;
								dc.setBUI(w.BUI);
								m_options |= 0x00000004;
							}
							if (w.ISI >= 0.0) {
								dc.setISI(t, w.ISI);
								m_options |= 0x00000004;
							}
							if (w.FWI >= 0.0) {
								dc.setFWI(t, w.FWI);
								m_options |= 0x00000004;
							}
							if (w.ffmc >= 0.0) {
								dc.setHourlyFFMC(t, w.ffmc);
								m_options |= 0x00000004;
		
								if ((noonhour + 4) == hour.value) {
									dc.setDailyFFMC(w.ffmc);
									if (t.getDay(FORMAT_AS_LOCAL | FORMAT_WITHDST) == m_time.getDay(FORMAT_AS_LOCAL | FORMAT_WITHDST)) {	// same for Equilibrium and Van Wagner
										m_initialHFFMC = w.ffmc;
										m_initialHFFMCTime = subtract(add(dayNoon, new WTimeSpan(0, 4, 0, 0)), m_time);
									}
								}
							}
		
							m_lastHour = hour.value;
							dc.setHourlyWeather(t, w.temp, w.rh, w.precip, w.ws, w.wg, w.wd, -300.0);
							lines++;
						}
		
						if ((hr & ERROR.SEVERITY_WARNING) == 0) {
							m_isCalculatedValuesValid = false;
							calculateValues();
						}
					}
				}
				else {
					rdr.close();
					return ERROR.BAD_FILE_TYPE | ERROR.SEVERITY_WARNING;
				}
				if (rdr != null) {
					try {
						rdr.close();
					}
					catch (Exception e) {
						e.printStackTrace();
					}
				}
			}
			catch (Exception e) {
				if (rdr != null) {
					try {
						rdr.close();
					}
					catch (IOException ex) {
						ex.printStackTrace();
					}
				}
			}
		}
		else
			return ERROR.FILE_NOT_FOUND | ERROR.SEVERITY_WARNING;

		m_isCalculatedValuesValid = false;
		return hr;
	}

	public void setEndTime(WTime endTime) {
		WTime currentEndTime = new WTime(m_time);
		WTimeSpan ts;

		boolean IncFlag = false;
		long oldDays = getNumDays();

		if (oldDays==0) {
			return;
		}
		currentEndTime = add(m_time, new WTimeSpan(oldDays - 1, 0, 0, 0));
		if (greaterThan(endTime, currentEndTime)) {
			ts = subtract(endTime, currentEndTime);
			IncFlag = true;
		}
		else {
			ts = subtract(currentEndTime, endTime);
			IncFlag = false;
		}
		long days = ts.getDays();
		if (days >= oldDays && IncFlag == false)
			days = oldDays;
		if (IncFlag == true)
			increaseConditions(currentEndTime, days);
		else
			decreaseConditions(currentEndTime, days);
	}

	public WTime getEndTime() {
		long count = m_readings.size();
		WTimeSpan timeSpan = new WTimeSpan(count, 0, 0, 0);
		WTime EndTime = new WTime(m_time);
		EndTime.add(timeSpan);
		EndTime.subtract(new WTimeSpan(0, 0, 0, 1));
		return EndTime;
	}

	public long getNumDays()				{ return m_readings.size(); }

	public void calculateValues() {
		if (m_isCalculatedValuesValid)
			return;
		m_isCalculatedValuesValid = true;

		if (m_weatherStation != null) {
			Object v;
			v = m_weatherStation.getAttribute(GRID_ATTRIBUTE.LATITUDE);
			assert v instanceof Double;
			m_worldLocation.setLatitude(((Double)v).doubleValue());
			v = m_weatherStation.getAttribute(GRID_ATTRIBUTE.LONGITUDE);
			assert v instanceof Double;
			m_worldLocation.setLongitude(((Double)v).doubleValue());
		}
		if (m_readings.isEmpty())
			return;

		DailyCondition fakeLast = null, dc = m_readings.getLast();
		if ((dc.m_flags & DailyWeather.DAY_HOURLY_SPECIFIED) == 0) {
			fakeLast = new DailyCondition(this);
			m_readings.addLast(fakeLast);
			fakeLast.setDailyWeather(dc.getDailyMinTemp(), dc.getDailyMaxTemp(), dc.getDailyMinWS(), dc.getDailyMaxWS(), dc.getDailyMinGust(), dc.getDailyMaxGust(), dc.getDailyMeanRH(), dc.getDailyPrecip(), dc.getDailyWD());
		}
		dc = m_readings.getFirst();

		int i = 0;
		dc = m_readings.getFirst();
		while (dc.getNext() != null) {
			dc.calculateTimes(i++);
			dc = dc.getNext();
		}
		dc = m_readings.getFirst();
		while (dc.getNext() != null) {
			dc.calculateHourlyConditions();
			dc.calculateDailyConditions();
			dc = dc.getNext();
		}
		dc = m_readings.getFirst();
		while (dc.getNext() != null) {
			dc.calculateRemainingHourlyConditions();
			dc = dc.getNext();
		}

		dc = m_readings.getLast();
		if ((dc.m_flags & DailyWeather.DAY_HOURLY_SPECIFIED) == 0 && fakeLast != null) {
			m_readings.remove(fakeLast);
		}

		dc = m_readings.getFirst();
		while (dc.getNext() != null) {
			dc.calculateFWI();
			dc.m_flags |= DailyWeather.DAY_HOURLY_SPECIFIED;
			dc = dc.getNext();
		}
	}

	private void distributeDailyValue(List<String> header, int index, double value, OutVariable<Double> min_temp, OutVariable<Double> max_temp,
			OutVariable<Double> rh, OutVariable<Double> precip, OutVariable<Double> min_ws, OutVariable<Double> max_ws, OutVariable<Double> min_wg, OutVariable<Double> max_wg, OutVariable<Double> wd) {
		assert min_temp != null;
		assert max_temp != null;
		assert min_ws != null;
		assert max_ws != null;
		assert min_wg != null;
		assert max_wg != null;
		assert rh != null;
		assert precip != null;
		assert wd != null;
		if(index > header.size() - 1 || index<0)
			return;
		String str = header.get(index);
		if (str.compareToIgnoreCase("min_temp") == 0)
			min_temp.value = value;
		else if (str.compareToIgnoreCase("max_temp") == 0)
			max_temp.value = value;
		else if ((str.compareToIgnoreCase("rh") == 0)  || (str.compareToIgnoreCase("min_rh") == 0) || str.compareToIgnoreCase("relative_humidity") == 0)
			rh.value = value;
		else if (str.compareToIgnoreCase("wd") == 0 || str.compareToIgnoreCase("dir") == 0 || str.compareToIgnoreCase("wind_direction") == 0)
			wd.value = value;
		else if (str.compareToIgnoreCase("min_ws") == 0)
			min_ws.value = value;
		else if (str.compareToIgnoreCase("max_ws") == 0)
			max_ws.value = value;
		else if (str.compareToIgnoreCase("min_wg") == 0)
			min_wg.value = value;
		else if (str.compareToIgnoreCase("max_wg") == 0)
			max_wg.value = value;
		else if ((str.compareToIgnoreCase("precip") == 0) || (str.compareToIgnoreCase("rain") == 0) || str.compareToIgnoreCase("precipitation") == 0)
			precip.value = value;
	}

	private void distributeValue(List<String> header, int index, double value, OutVariable<Integer> hour,
			OutVariable<Integer> minute, OutVariable<Integer> second,
			OutVariable<Double> temp, OutVariable<Double> rh, OutVariable<Double> wd, OutVariable<Double> ws, OutVariable<Double> wg,
			OutVariable<Double> precip, OutVariable<Double> ffmc, OutVariable<Double> dmc, OutVariable<Double> dc,
			OutVariable<Double> bui, OutVariable<Double> isi, OutVariable<Double> fwi) {
		assert hour != null;
		assert temp != null;
		assert ws != null;
		assert wg != null;
		assert precip != null;
		assert rh != null;
		assert wd != null;
		assert ffmc != null;
		assert dmc != null;
		assert dc != null;
		assert bui != null;
		assert isi != null;
		assert fwi != null;
		if(index > header.size() - 1 || index < 0)
			return;
		String str = header.get(index);
		if (str.compareToIgnoreCase("hour") == 0)
			hour.value = (int)value;
		else if (str.compareToIgnoreCase("temp") == 0 || str.compareToIgnoreCase("temperature") == 0)
			temp.value = value;
		else if (str.compareToIgnoreCase("rh") == 0 || str.compareToIgnoreCase("relative_humidity") == 0 || str.compareToIgnoreCase("min_rh") == 0 || str.compareToIgnoreCase("min rh") == 0)
			rh.value = value;
		else if (str.compareToIgnoreCase("wd") == 0 || str.compareToIgnoreCase("dir") == 0 || str.compareToIgnoreCase("wind_direction") == 0 || str.compareToIgnoreCase("direction") == 0)
			wd.value = value;
		else if (str.compareToIgnoreCase("ws") == 0 || str.compareToIgnoreCase("wspd") == 0 || str.compareToIgnoreCase("wind_speed") == 0 || str.compareToIgnoreCase("windspeed") == 0)
			ws.value = value;
		else if (str.compareToIgnoreCase("wg") == 0 || str.compareToIgnoreCase("gust") == 0 || str.compareToIgnoreCase("gusting") == 0 || str.compareToIgnoreCase("wind_gust") == 0 || str.compareToIgnoreCase("windgust") == 0)
			wg.value = value;
		else if ((str.compareToIgnoreCase("precip") == 0) || (str.compareToIgnoreCase("rain") == 0) || str.compareToIgnoreCase("precipitation") == 0 || str.compareToIgnoreCase("prec") == 0)
			precip.value = value;
		else if ((str.compareToIgnoreCase("ffmc") == 0) || (str.compareToIgnoreCase("hffmc") == 0))
			ffmc.value = value;
		else if (str.compareToIgnoreCase("dmc") == 0)
			dmc.value = value;
		else if (str.compareToIgnoreCase("dc") == 0)
			dc.value = value;
//		else if (str.compareToIgnoreCase("bui") == 0)
//			bui.value = value;
//		else if ((str.compareToIgnoreCase("hisi") == 0) || (str.compareToIgnoreCase("isi") == 0))
//			isi.value = value;
//		else if ((str.compareToIgnoreCase("hfwi") == 0) || (str.compareToIgnoreCase("fwi") == 0))
//			fwi.value = value;
	}

	private String fillDailyLineValue(List<String> header, String line, OutVariable<Double> min_temp, OutVariable<Double> max_temp,
			OutVariable<Double> rh, OutVariable<Double> precip, OutVariable<Double> min_ws, OutVariable<Double> max_ws, OutVariable<Double> min_wg, OutVariable<Double> max_wg, OutVariable<Double> wd) {
		assert min_temp != null;
		assert max_temp != null;
		assert min_ws != null;
		assert max_ws != null;
		assert min_wg != null;
		assert max_wg != null;
		assert rh != null;
		assert precip != null;
		assert wd != null;
		int i=0;
		OutVariable<StringTokenizer> context = new OutVariable<StringTokenizer>();
		String dat = StringExtensions.strtok_s(line, ", ;\t", context);

		String retVal = dat.replaceAll("[\"\']+", "");
		
		while(dat != null)
		{
			dat = StringExtensions.strtok_s(null, ", ;\t", context);
			if(dat == null)
				break;

			dat = dat.replaceAll("[\"\']+", "");
			
			double ReadIn = Double.parseDouble(dat);
			distributeDailyValue(header, ++i, ReadIn, min_temp, max_temp, rh, precip, min_ws, max_ws, min_wg, max_wg, wd);
		}
		return retVal;
	}
	
	/**
	 * Parses time from a string in any of the following format: 'h:mm:ss', 'h:mm', 'h'.
	 * @param dat The string to parse the time from.
	 * @param hour A variable to store the hour in. Can't be null.
	 * @param minute A variable to store the minutes in. Can't be null.
	 * @param second A variable to store the seconds in. Can't be null.
	 */
	private void parseTime(String dat, OutVariable<Integer> hour, OutVariable<Integer> minute, OutVariable<Integer> second) {
		int colonIndex = dat.indexOf(":");
		hour.value = Integer.parseInt(dat.substring(0, colonIndex));
		minute.value = Integer.parseInt(dat.substring(colonIndex + 1, dat.length()));
		second.value = 0;
	}

	private String fillLineValues(List<String> header, String line, OutVariable<Integer> hour, OutVariable<Integer> minute, OutVariable<Integer> second,
			OutVariable<Double> temp, OutVariable<Double> rh, OutVariable<Double> wd, OutVariable<Double> ws, OutVariable<Double> wg,
			OutVariable<Double> precip, OutVariable<Double> ffmc, OutVariable<Double> dmc, OutVariable<Double> dc, 
			OutVariable<Double> bui, OutVariable<Double> isi, OutVariable<Double> fwi) {
		assert hour != null;
		assert temp != null;
		assert ws != null;
		assert wg != null;
		assert precip != null;
		assert rh != null;
		assert precip != null;
		assert wd != null;
		assert ffmc != null;
		assert dmc != null;
		assert dc != null;
		assert bui != null;
		assert isi != null;
		assert fwi != null;
		int i=0;
		OutVariable<StringTokenizer> context = new OutVariable<StringTokenizer>();
		String dat = StringExtensions.strtok_s(line, ", ;\t", context);

		String retVal = dat.replaceAll("[\"\']+", "");

		while(dat != null)
		{
			dat = StringExtensions.strtok_s(null, ", ;\t", context);
			if(dat == null)
				break;

			dat = dat.replaceAll("[\"\']+", "");
			
			if (dat.indexOf(":") >= 0) {
				String str = header.get(++i);
				
				if (str.compareToIgnoreCase("hour") == 0) {
					parseTime(dat, hour, minute, second);
					dat = StringExtensions.strtok_s(null, ", ;\t", context).replaceAll("[\"\']+", "");
				} else if (str.compareToIgnoreCase("Sunrise") == 0 || str.compareToIgnoreCase("Solar_Noon") == 0 || str.compareToIgnoreCase("Sunset") == 0)
					break;
			}
			double ReadIn = Double.parseDouble(dat);
			distributeValue(header, ++i, ReadIn, hour, minute, second, temp, rh, wd, ws, wg, precip, ffmc, dmc, dc, bui, isi, fwi);
		}
		return retVal;
	}

	private int getWord(OutVariable<String> source, OutVariable<String> strWord) {
		assert source != null;
		assert strWord != null;
		if(source.value.length() == 0)
			return 0;
		char c = source.value.charAt(0);

		while(c == ',' || c == ' ' || c == ';' || c == '\t' || c == 0x0a || c == 0x0d || c == 0x22)
		{
			source.value = source.value.substring(1);
			if(source.value.length() == 0)
				return 0;
			c = source.value.charAt(0);
		}
		strWord.value = "";

		for(int i = 0; i < source.value.length(); i++)
		{
			c = source.value.charAt(i);
			if(c != ',' && c != ' ' && c != ';' && c != '\t' && c != 0x0a && c != 0x0d && c != 0x22)
				strWord.value += c;
			else
				break;
		}
		source.value = source.value.substring(strWord.value.length());

		return strWord.value.length();
	}

	private void processHeader(String line, List<String> header) {
		OutVariable<String> colName = new OutVariable<String>();
		OutVariable<String> source = new OutVariable<String>();
		source.value = line;
		int res;
		res = getWord(source, colName);

		while(res != 0)
		{
			header.add(colName.value);
			colName.value = "";
			res = getWord(source, colName);
		}
	}

	private void copyDailyCondition(WTime source, WTime dest) {
		OutVariable<Double> min_temp = new OutVariable<Double>();
		OutVariable<Double> max_temp = new OutVariable<Double>();
		OutVariable<Double> min_ws = new OutVariable<Double>();
		OutVariable<Double> max_ws = new OutVariable<Double>();
		OutVariable<Double> min_wg = new OutVariable<Double>();
		OutVariable<Double> max_wg = new OutVariable<Double>();
		OutVariable<Double> rh = new OutVariable<Double>();
		OutVariable<Double> precip = new OutVariable<Double>();
		OutVariable<Double> wd = new OutVariable<Double>();
		getDailyWeatherValues(source, min_temp, max_temp, min_ws, max_ws, min_wg, max_wg, rh, precip, wd);

		setDailyWeatherValues(dest, min_temp.value, max_temp.value, min_ws.value, max_ws.value, min_wg.value, max_wg.value, rh.value, precip.value, wd.value);
	}

	private void decreaseConditions(WTime currentEndTime, long days) {
		for(long i = 0; i < days; i++) {
			m_readings.removeLast();
		}
	}

	private void increaseConditions(WTime currentEndTime, long days) {
		WTime tempTime = new WTime(currentEndTime);
		tempTime.add(new WTimeSpan(1, 0, 0, 0));
		WTime start = new WTime(tempTime.getYear(0), tempTime.getMonth(0), tempTime.getDay(0), 0, 0, 0, currentEndTime.getTimeManager());
		for (int i = 0; i < days; i++) {
			copyDailyCondition(currentEndTime, start);
			start.add(new WTimeSpan(1, 0, 0, 0));
		}
	}

	private boolean isSupportedFormat(String line, List<String> header) {
		OutVariable<String> colName = new OutVariable<String>();
		OutVariable<String> source = new OutVariable<String>();
		source.value = line;
		int res;
		res = getWord(source, colName);

		while(res != 0)
		{
			header.add(colName.value);
			colName.value = "";
			res = getWord(source, colName);
		}
		String v = header.get(0);
		if (v.equals("Name") || v.equals("StationID") || v.equals("weather_date"))
			return true;
		else
			return false;
	}

	void serialize(ObjectOutputStream outStream, String m_loadWarning) throws IOException {
		outStream.writeInt((int)serialVersionUID);
		outStream.writeObject(m_worldLocation);
		outStream.writeObject(m_time);
		outStream.writeDouble(m_spec_day.dFFMC);
		outStream.writeDouble(m_spec_day.dDC);
		outStream.writeDouble(m_spec_day.dDMC);
		outStream.writeObject(m_initialHFFMCTime);
		outStream.writeDouble(m_initialHFFMC);
		outStream.writeDouble(m_initialRain);
		outStream.writeDouble(m_initialTemp);
		outStream.writeObject(m_initialTempTime);
		outStream.writeDouble(m_initialWS);
		outStream.writeObject(m_initialWSTime);
		outStream.writeDouble(m_temp_alpha);
		outStream.writeDouble(m_temp_beta);
		outStream.writeDouble(m_temp_gamma);
		outStream.writeDouble(m_wind_alpha);
		outStream.writeDouble(m_wind_beta);
		outStream.writeDouble(m_wind_gamma);
		outStream.writeLong(m_options);
		long cnt = m_readings.size();
		outStream.writeLong(cnt);
		DailyCondition dc = m_readings.getFirst();
		while (dc != null) {
			outStream.writeObject(dc);
			dc = dc.getNext();
		}
	}

	void serialize(ObjectInputStream inStream, String m_loadWarning) throws IOException, ClassNotFoundException {
		int version = inStream.readInt();
		if (version == 0) {
			m_loadWarning += "Weather Stream: Invalid version.\n";
			//AfxThrowArchiveException(CArchiveException::badSchema, "CWFGM Weather Stream");
		}
		if (version > serialVersionUID) {
			m_loadWarning += "Weather Stream: Version too new.\n";
			//AfxThrowArchiveException(CArchiveException::badSchema, "CWFGM Weather Stream");
		}
		m_worldLocation = (WorldLocation)inStream.readObject();

		if (version < 9) {
			throw new IOException("Version not supported");
		}
		else
			m_time = (WTime)inStream.readObject();

		WTime t = new WTime(m_time);
		m_time.purgeToDay(FORMAT_AS_LOCAL | FORMAT_WITHDST);

		if (notEqual(t, m_time))
			m_loadWarning += "Weather Stream: Start time had to be reset to correct for DST / timezone errors.  Please review.\n";

		m_spec_day.dFFMC = inStream.readDouble();
		m_spec_day.dDC = inStream.readDouble();
		m_spec_day.dDMC = inStream.readDouble();
		if (version > 2) {
			if (version < 9) {
				throw new IOException("Version not supported");
			}
			else
				m_initialHFFMCTime = (WTimeSpan)inStream.readObject();
		}
		else
			m_initialHFFMCTime = new WTimeSpan(-1);

		if (version < 16) {
			m_initialHFFMC = m_spec_day.dFFMC;
			m_initialHFFMCTime = new WTimeSpan(0, 12, 0, 0);
		}
		else
			m_initialHFFMC = inStream.readDouble();

		if (version >= 17)
			m_initialRain = inStream.readDouble();

		if (version > 3) {
			if (version < 9) {
				throw new IOException("Version not supported");
			}
			else {
				m_initialTemp = inStream.readDouble();
				m_initialTempTime = (WTimeSpan)inStream.readObject();
				m_initialWS = inStream.readDouble();
				m_initialWSTime = (WTimeSpan)inStream.readObject();
			}
		}
		else {
			m_initialTemp = m_initialWS = 0;
			m_initialTempTime = m_initialWSTime = new WTimeSpan(0);
		}

		if ((version > 4) && (version < 9)) {
			inStream.readInt();
			inStream.readInt();
			inStream.readInt();
		}
		if (version < 16)
			m_options &= (~(0x00000018));

		m_temp_alpha = inStream.readDouble();
		m_temp_beta = inStream.readDouble();
		m_temp_gamma = inStream.readDouble();
		m_wind_alpha = inStream.readDouble();
		m_wind_beta = inStream.readDouble();
		m_wind_gamma = inStream.readDouble();

		if (version > 1) {
			long options = m_options;
			m_options = inStream.readLong();
			if ((version > 4) && (version < 9))
				m_options |= options;
		}
		if (version < 9) {
			m_options |= ((m_options & 0x000000003) + 1) & 0x00000003;
			m_loadWarning += "Weather Stream: Please review selection of FFMC calculation.  An error in the file has been detected and corrected.\n";
		}

		long cnt;
		if ((version > 9) && (version < 14)) {
			cnt = inStream.readShort();
		}
		else
			cnt = inStream.readLong();
		while (cnt > 0) {
			DailyCondition dc = (DailyCondition)inStream.readObject();
			m_readings.addLast(dc);
			cnt--;
		}

		if ((version >= 6) && (version < 9)) {
			boolean m_bDaylightSaving = inStream.readBoolean();
			if (m_bDaylightSaving)	m_options |= 0x00000020;
		}

		if ((version >= 7) && (version < 10)) {
			inStream.readBoolean();
			inStream.readDouble();
			inStream.readDouble();
			inStream.readInt();
			inStream.readInt();
		}

		if ((version >= 8) && (version < 12)) {
			inStream.readDouble();
			inStream.readDouble();
		}

		if (version <= 12)
			if ((m_options & 0x00000003) == 2) {
				m_options &= (~(0x00000003));
				m_options |= 0x00000001;
			}
		if (version < 15)
			m_loadWarning += "Dew point temperatures did not exist in this file version and will be calculated from provided data.\n";
		if (version < 16)
			m_loadWarning += "HFFMC and FFMC starting values have been set to be equal and should be reviewed.\n";
	}
	
	public int getMode() {
		return mode;
	}
}
