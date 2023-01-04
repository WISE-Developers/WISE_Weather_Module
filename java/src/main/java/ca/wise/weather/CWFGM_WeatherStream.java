/***********************************************************************
 * REDapp - CWFGM_WeatherStream.java
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

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;

import ca.wise.grid.GRID_ATTRIBUTE;
import ca.wise.grid.DFWIData;
import ca.wise.grid.IFWIData;
import ca.wise.grid.IWXData;
import ca.hss.annotations.Source;
import ca.hss.general.ERROR;
import ca.hss.general.OutVariable;
import ca.hss.times.WTime;
import ca.hss.times.WTimeSpan;
import ca.hss.times.SunriseSunsetCalc;
import ca.hss.times.WTimeManager;

/**
 * This object contains weather data for a series of consecutive days, so is such called a weather stream.  It has a start time and duration, and the user
 * can inspect and modify various daily or hourly values.  Each day of weather data is complete unto itself. The user may also specify alpha, beta, and
 * gamma for Judi Beck's diurnal weather calculations. It also supports the standard COM IPersistStream, IPersistStreamInit, and IPersistStorage
 * interfaces.  Any weather stream is automatically saved with the station in order to simplify serialization operations in the client application.\n\n
 * A weather stream must be associated with a weather station to be selected into a scenario.
 * @author Travis Redpath
 *
 */
@Source(sourceFile="CWFGM_WeatherStream.cpp", project="WeatherCOM")
public class CWFGM_WeatherStream implements Serializable, Cloneable {
	public static final long serialVersionUID = 6;
	public boolean m_bRequiresSave;

	private int m_gridCount = 0;
	protected WeatherCondition m_weatherCondition;
	protected String m_loadWarning;
	protected Object m_userData;

	public CWFGM_WeatherStream() {
		m_gridCount = 0;
		m_userData = null;
		m_weatherCondition = new WeatherCondition();
		m_loadWarning = "";
	}

	/**
	 * This method is used to (only) retrieve which weather station a weather stream is associated with.
	 * @return
	 */
	public CWFGM_WeatherStation getWeatherStation() {
		return m_weatherCondition.m_weatherStation;
	}

	public WTimeManager getTimeManager() {
		return m_weatherCondition.m_time.getTimeManager();
	}
	
	public WeatherCondition getWeatherCondition() {
		return m_weatherCondition;
	}

	/**
	 * This method is to only be called by a weather station or a weather grid.  It is used to build associations between these objects.
	 * @param key Internal value.
	 * @param newVal Internal value.
	 * @return
	 */
	boolean setWeatherStation(long key, CWFGM_WeatherStation newVal) {
		if (key == 0xfedcba98)							{ m_gridCount++; return true; };
		if (key == 0x0f1e2d3c)							{ m_gridCount--; return true; };
		if (key != 0x12345678)							return false;
		if (newVal != null) {
			if (key == -1) {
				m_weatherCondition.clearConditions();
				return true;
			}
		}
		if (m_gridCount > 0) return false;

		m_weatherCondition.m_weatherStation = newVal;

		m_weatherCondition.clearConditions();
		return true;
	}

	/**
	 * Creates a new weather stream with all the same properties and data of the object being called, returns a handle to the new object in 'newWeatherStream'.
	 * @return
	 * @throws CloneNotSupportedException 
	 */
	@Override
	public CWFGM_WeatherStream clone() throws CloneNotSupportedException {
		CWFGM_WeatherStream newObj = (CWFGM_WeatherStream)super.clone();
		newObj.m_bRequiresSave = true;
		newObj.m_gridCount = m_gridCount;
		newObj.m_loadWarning = m_loadWarning;
		newObj.m_userData = m_userData;
		newObj.m_weatherCondition = m_weatherCondition;
		return newObj;
	}

	/**
	 * Gets the value of an "option" and saves it in the "value" variable provided.
	 * @param option The weather option of interest.
	 * @return
	 */
	public Object getAttribute(int option) {
		switch (option) {
		case GRID_ATTRIBUTE.LOAD_WARNING:
			return m_loadWarning;
		case WEATHER_OPTION.WARNONSUNRISE:
			return Boolean.valueOf((m_weatherCondition.getWarnOnSunRiseSet() & SunriseSunsetCalc.NO_SUNRISE) != 0);
		case WEATHER_OPTION.WARNONSUNSET:
			return Boolean.valueOf((m_weatherCondition.getWarnOnSunRiseSet() & SunriseSunsetCalc.NO_SUNSET) != 0);
		case WEATHER_OPTION.FFMC_VANWAGNER:
			return Boolean.valueOf((m_weatherCondition.m_options & 0x00000003) == 1);
		case WEATHER_OPTION.FFMC_HYBRID:
			return Boolean.valueOf(false);
		case WEATHER_OPTION.FFMC_LAWSON:
			return Boolean.valueOf((m_weatherCondition.m_options & 0x00000003) == 3);
		case WEATHER_OPTION.FWI_USE_SPECIFIED:
			return Boolean.valueOf((m_weatherCondition.m_options & 0x00000004) != 0);
		case WEATHER_OPTION.ORIGIN_FILE:
			return Boolean.valueOf((m_weatherCondition.m_options & 0x00000020) != 0);
		case WEATHER_OPTION.FWI_ANY_SPECIFIED:
			return Boolean.valueOf(m_weatherCondition.anyFWICodesSpecified());
		case WEATHER_OPTION.TEMP_ALPHA:
			return Double.valueOf(m_weatherCondition.m_temp_alpha);
		case WEATHER_OPTION.TEMP_BETA:
			return Double.valueOf(m_weatherCondition.m_temp_beta);
		case WEATHER_OPTION.TEMP_GAMMA:
			return Double.valueOf(m_weatherCondition.m_temp_gamma);
		case WEATHER_OPTION.WIND_ALPHA:
			return Double.valueOf(m_weatherCondition.m_wind_alpha);
		case WEATHER_OPTION.WIND_BETA:
			return Double.valueOf(m_weatherCondition.m_wind_beta);
		case WEATHER_OPTION.WIND_GAMMA:
			return Double.valueOf(m_weatherCondition.m_wind_gamma);
		case WEATHER_OPTION.INITIAL_FFMC:
			return Double.valueOf(m_weatherCondition.m_spec_day.dFFMC);
		case WEATHER_OPTION.INITIAL_HFFMC:
			return Double.valueOf(m_weatherCondition.m_initialHFFMC);
		case WEATHER_OPTION.INITIAL_DC:
			return Double.valueOf(m_weatherCondition.m_spec_day.dDC);
		case WEATHER_OPTION.INITIAL_DMC:
			return Double.valueOf(m_weatherCondition.m_spec_day.dDMC);
		case WEATHER_OPTION.INITIAL_BUI:
			return Double.valueOf(m_weatherCondition.m_spec_day.dBUI);
		case WEATHER_OPTION.INITIAL_RAIN:
			return Double.valueOf(m_weatherCondition.m_initialRain);
		case GRID_ATTRIBUTE.LATITUDE:
			return Double.valueOf(m_weatherCondition.m_worldLocation.getLatitude());
		case GRID_ATTRIBUTE.LONGITUDE:
			return Double.valueOf(m_weatherCondition.m_worldLocation.getLongitude());
		case WEATHER_OPTION.INITIAL_HFFMCTIME:
			return Long.valueOf(m_weatherCondition.m_initialHFFMCTime.getTotalSeconds());
		case GRID_ATTRIBUTE.TIMEZONE:
			return Long.valueOf(m_weatherCondition.m_worldLocation.getTimezoneOffset().getTotalSeconds());
		case GRID_ATTRIBUTE.DAYLIGHT_SAVINGS:
			return Long.valueOf(m_weatherCondition.m_worldLocation.getDSTAmount().getTotalSeconds());
		case GRID_ATTRIBUTE.DST_START:
			return Long.valueOf(m_weatherCondition.m_worldLocation.getStartDST().getTotalSeconds());
		case GRID_ATTRIBUTE.DST_END:
			return Long.valueOf(m_weatherCondition.m_worldLocation.getEndDST().getTotalSeconds());
		case WEATHER_OPTION.START_TIME:
			return Long.valueOf(m_weatherCondition.m_time.getTime((short)0));
		case WEATHER_OPTION.END_TIME:
			return Long.valueOf(m_weatherCondition.getEndTime().getTime((short)0));
		}
		return null;
	}

	/**
	 * Sets the value of an "option" to the value of the "value" variable provided.
	 * It is important to set the starting codes of the weather stream before importing the weather data.
	 * Changing the starting codes after importing mean Prometheus will recalculate everything.
	 * @param option
	 * @param value
	 */
	public void setAttribute(int option,  Object value) {
		Boolean bValue;
		Double dValue;
		Long llvalue;
		if (value == null)
			throw new IllegalArgumentException();
		switch (option) {
		case WEATHER_OPTION.FFMC_VANWAGNER:
			if (!(value instanceof Boolean))
				throw new IllegalArgumentException("The property FFMC_VANWAGNER must be passed a Boolean");
			bValue = (Boolean)value;
			if ((m_weatherCondition.m_options & 0x00000003) != 1) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_options &= (~(0x0000007));
				m_weatherCondition.m_options |= 0x00000001;
				m_bRequiresSave = true;
			}
			break;
		case WEATHER_OPTION.FFMC_LAWSON:
			if (!(value instanceof Boolean))
				throw new IllegalArgumentException("The property FFMC_LAWSON must be passed a Boolean");
			bValue = (Boolean)value;
			if ((m_weatherCondition.m_options & 0x00000003) != 3) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_options &= (~(0x0000007));
				m_weatherCondition.m_options |= 0x00000003;
				m_bRequiresSave = true;
			}
			break;
		case WEATHER_OPTION.FWI_USE_SPECIFIED:
			if (!(value instanceof Boolean))
				throw new IllegalArgumentException("The property FWI_USE_SPECIFIED must be passed a Boolean");
			bValue = (Boolean)value;
			m_weatherCondition.clearConditions();
			if (!bValue)	m_weatherCondition.m_options &= (~(0x00000004));
			else			m_weatherCondition.m_options |= 0x00000004;
			m_bRequiresSave = true;
			break;
		case WEATHER_OPTION.TEMP_ALPHA:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property TEMP_ALPHA must be passed a Double");
			dValue = (Double)value;
			if (m_weatherCondition.m_temp_alpha != dValue) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_temp_alpha = dValue;
				m_bRequiresSave = true;
			}
			break;
		case WEATHER_OPTION.TEMP_BETA:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property TEMP_BETA must be passed a Double");
			dValue = (Double)value;
			if (m_weatherCondition.m_temp_beta != dValue) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_temp_beta = dValue;
				m_bRequiresSave = true;
			}
			break;
		case WEATHER_OPTION.TEMP_GAMMA:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property TEMP_GAMMA must be passed a Double");
			dValue = (Double)value;
			if (m_weatherCondition.m_temp_gamma != dValue) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_temp_gamma = dValue;
				m_bRequiresSave = true;
			}
			break;
		case WEATHER_OPTION.WIND_ALPHA:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property WIND_ALPHA must be passed a Double");
			dValue = (Double)value;
			if (m_weatherCondition.m_wind_alpha != dValue) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_wind_alpha = dValue;
				m_bRequiresSave = true;
			}
			break;
		case WEATHER_OPTION.WIND_BETA:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property WIND_BETA must be passed a Double");
			dValue = (Double)value;
			if (m_weatherCondition.m_wind_beta != dValue) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_wind_beta = dValue;
				m_bRequiresSave = true;
			}
			break;
		case WEATHER_OPTION.WIND_GAMMA:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property WIND_GAMMA must be passed a Double");
			dValue = (Double)value;
			if (m_weatherCondition.m_wind_gamma != dValue) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_wind_gamma = dValue;
				m_bRequiresSave = true;
			}
			break;
		case WEATHER_OPTION.INITIAL_FFMC:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property INITIAL_FFMC must be passed a Double");
			dValue = (Double)value;
			if (dValue < 0.0)	return;
			if (dValue > 101.0)	return;
			if (m_weatherCondition.m_spec_day.dFFMC != dValue) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_spec_day.dFFMC = dValue;
				m_bRequiresSave = true;
			}
			break;
		case WEATHER_OPTION.INITIAL_HFFMC:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property INITIAL_HFFMC must be passed a Double");
			dValue = (Double)value;
			if (dValue < 0.0)	return;
			if (dValue > 101.0)	return;
			if ((m_weatherCondition.m_initialHFFMC != dValue) && (WTimeSpan.notEqual(m_weatherCondition.m_initialHFFMCTime, new WTimeSpan(-1)))) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_initialHFFMC = dValue;
				m_bRequiresSave = true;
			}
			break;
		case WEATHER_OPTION.INITIAL_DC:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property INITIAL_DC must be passed a Double");
			dValue = (Double)value;
			if (dValue < 0.0)	return;
			if (dValue > 1500.0)	return;
			if (m_weatherCondition.m_spec_day.dDC != dValue) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_spec_day.dDC = dValue;
				m_bRequiresSave = true;
			}
			break;
		case WEATHER_OPTION.INITIAL_DMC:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property INITIAL_DMC must be passed a Double");
			dValue = (Double)value;
			if (dValue < 0.0)	return;
			if (dValue > 500.0)	return;
			if (m_weatherCondition.m_spec_day.dDMC != dValue) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_spec_day.dDMC = dValue;
				m_bRequiresSave = true;
			}
			break;
		case WEATHER_OPTION.INITIAL_BUI:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property INITIAL_BUI must be passed a Double");
			dValue = (Double)value;
			if (dValue < 0.0)	return;
			m_weatherCondition.m_initialBUI = dValue;
			break;
		case WEATHER_OPTION.INITIAL_RAIN:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property INITIAL_RAIN must be passed a Double");
			dValue = (Double)value;
			if (m_weatherCondition.m_initialRain != dValue) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_initialRain = dValue;
				m_bRequiresSave = true;
			}
			break;
		case GRID_ATTRIBUTE.LATITUDE:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property LATITUDE must be passed a Double");
			dValue = (Double)value;
			if (m_weatherCondition.m_worldLocation.getLatitude() != dValue) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_worldLocation.setLatitude(dValue);
				m_bRequiresSave = true;
			}
			break;
		case GRID_ATTRIBUTE.LONGITUDE:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property LONGITUDE must be passed a Double");
			dValue = (Double)value;
			if (m_weatherCondition.m_worldLocation.getLongitude() != dValue) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_worldLocation.setLongitude(dValue);
				m_bRequiresSave = true;
			}
			break;
		case WEATHER_OPTION.INITIAL_HFFMCTIME:
			if (!(value instanceof Long))
				throw new IllegalArgumentException("The property INITIAL_HFFMCTIME must be passed a Long");
			llvalue = (Long)value;
			WTimeSpan ts = new WTimeSpan(llvalue);
			if (llvalue >= (24 * 60 * 60)) 										return;
			if ((llvalue < -1) && (llvalue != (-1 * 60 * 60)))					return;
			if ((llvalue > 0) && (ts.getSeconds() > 0 || ts.getMinutes() > 0))	return;
			if (m_weatherCondition.m_initialHFFMCTime != ts) {
				m_weatherCondition.clearConditions();
				m_weatherCondition.m_initialHFFMCTime = ts;
				m_bRequiresSave = true;
			}
			break;
		case GRID_ATTRIBUTE.TIMEZONE:
			if (!(value instanceof Long))
				throw new IllegalArgumentException("The property TIMEZONE must be passed a Long");
			llvalue = (Long)value;
			ts = new WTimeSpan(llvalue);
			if (llvalue > (12 * 60 * 60))						return;
			if (llvalue < (-12 * 60 * 60))						return;
			if ((llvalue > 0) && (ts.getSeconds() > 0))			return;
			if (WTimeSpan.equal(m_weatherCondition.m_worldLocation.getTimezoneOffset(), ts))
				return;
			m_weatherCondition.clearConditions();
			WTime t1 = new WTime(m_weatherCondition.m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);

			m_weatherCondition.m_worldLocation.setTimezoneOffset(ts);
			WTime t2 = new WTime(t1, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)-1);
			m_weatherCondition.m_time = t2;
			m_bRequiresSave = true;
			break;
		case GRID_ATTRIBUTE.DAYLIGHT_SAVINGS:
			if (!(value instanceof Long))
				throw new IllegalArgumentException("The property DAYLIGHT_SAVINGS must be passed a Long");
			llvalue = (Long)value;
			ts = new WTimeSpan(llvalue);
			if (llvalue > (2 * 60 * 60))						return;
			if (llvalue < (-2 * 60 * 60))						return;
			if ((llvalue > 0) && (ts.getSeconds() > 0))			return;
			if (WTimeSpan.equal(m_weatherCondition.m_worldLocation.getDSTAmount(), ts))
				return;
			m_weatherCondition.clearConditions();
			t1 = new WTime(m_weatherCondition.m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);

			m_weatherCondition.m_worldLocation.setDSTAmount(ts);
			t2 = new WTime(t1, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)-1);
			m_weatherCondition.m_time = t2;
			m_bRequiresSave = true;
			break;
		case GRID_ATTRIBUTE.DST_START:
			if (!(value instanceof Long))
				throw new IllegalArgumentException("The property DST_START must be passed a Long");
			llvalue = (Long)value;
			ts = new WTimeSpan(llvalue);
			if (llvalue > (24 * 60 * 60 * 366))		return;
			if (ts.getSeconds() > 0 || ts.getMinutes() > 0 || ts.getHours() > 0)
				return;
			if (WTimeSpan.equal(m_weatherCondition.m_worldLocation.getStartDST(), ts))
				return;
			m_weatherCondition.clearConditions();
			t1 = new WTime(m_weatherCondition.m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);

			m_weatherCondition.m_worldLocation.setStartDST(ts);
			t2 = new WTime(t1, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)-1);
			m_weatherCondition.m_time = t2;
			m_bRequiresSave = true;
			break;
		case GRID_ATTRIBUTE.DST_END:
			if (!(value instanceof Long))
				throw new IllegalArgumentException("The property DST_END must be passed a Long");
			llvalue = (Long)value;
			ts = new WTimeSpan(llvalue);
			if (llvalue > (24 * 60 * 60 * 366))		return;
			if (ts.getSeconds() > 0 || ts.getMinutes() > 0 || ts.getHours() > 0)
				return;
			if (WTimeSpan.equal(m_weatherCondition.m_worldLocation.getEndDST(), ts))
				return;
			m_weatherCondition.clearConditions();
			t1 = new WTime(m_weatherCondition.m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);

			m_weatherCondition.m_worldLocation.setEndDST(ts);
			t2 = new WTime(t1, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)-1);
			m_weatherCondition.m_time = t2;
			m_bRequiresSave = true;
			break;
		case WEATHER_OPTION.START_TIME:
			if (!(value instanceof Long))
				throw new IllegalArgumentException("The property START_TIME must be passed a Long");
			llvalue = (Long)value;
			ts = new WTimeSpan(llvalue);
			WTime t = new WTime(llvalue, m_weatherCondition.m_time.getTimeManager());
			t.purgeToDay(WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST);
			if (WTime.notEqual(m_weatherCondition.m_time, t)) {
				m_weatherCondition.m_time = t;
				m_weatherCondition.clearConditions();
				m_bRequiresSave = true;
			}
			break;
		case WEATHER_OPTION.END_TIME:
			if (!(value instanceof Long))
				throw new IllegalArgumentException("The property END_TIME must be passed a Long");
			llvalue = (Long)value;
			ts = new WTimeSpan(llvalue);
			t = new WTime(llvalue, m_weatherCondition.m_time.getTimeManager());
			t.purgeToDay(WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST);
			WTime endTime = m_weatherCondition.getEndTime();
			if (WTime.notEqual(endTime, t)) {
				m_weatherCondition.setEndTime(t);
				m_weatherCondition.clearConditions();
				m_bRequiresSave = true;
			}
			break;
		}
	}
	
	public int firstHourOfDay(WTime time) {
		return m_weatherCondition.firstHourOfDay(time);
	}
	
	public int lastHourOfDay(WTime time) {
		return m_weatherCondition.lastHourOfDay(time);
	}

	/**
	 * Imports a weather stream with hourly data or daily data depending on the file.
	 * @param filename Path/file of weather stream data.
	 * @param options Determines rules for (re)importing data.
	 * @return
	 * @throws IOException
	 */
	public long importFile(String filename, int options) throws IOException {
		if (m_weatherCondition.getNumDays() > 0) {
			long purge = options & WEATHERSTREAM_IMPORT.PURGE;
			long overwrite_append = options & (WEATHERSTREAM_IMPORT.SUPPORT_APPEND | WEATHERSTREAM_IMPORT.SUPPORT_OVERWRITE);
			if ((purge != 0) && (overwrite_append != 0))
				return ERROR.INVALIDARG;
			if ((options & (~(WEATHERSTREAM_IMPORT.PURGE | WEATHERSTREAM_IMPORT.SUPPORT_APPEND | WEATHERSTREAM_IMPORT.SUPPORT_OVERWRITE))) != 0)
				return ERROR.INVALIDARG;
			if (options == 0)
				return ERROR.INVALIDARG;
		}

		long success = m_weatherCondition.importFile(filename, options);
		m_bRequiresSave = true;
		m_weatherCondition.m_options |= 0x00000020;
		return success;
	}
    
    /**
     * Were any of the hourly weather values corrected when they were imported.
     * They will have been corrected if the values were out of bounds and
     * fix invalid values was specified in the import options.
     */
    public boolean hasAnyCorrected() {
        return m_weatherCondition.hasAnyCorrected();
    }

	/**
	 * Deletes all hourly and daily weather data in the file.  Does not reset start time, starting codes, etc.
	 */
	public void clearWeatherData() {
		m_weatherCondition.clearWeatherData();
	}

	/**
	 * Returns the start time and duration for the data stored in this weather stream object.
	 * These objects return this data in GMT, where local time is set by SetAttribute().
	 * @param start Start time for the weather stream, specified in GMT time zone, as count of seconds since January 1, 1900.
	 * @param duration Duration of the weather stream, provided as a count of seconds.
	 */
	public void getValidTimeRange(OutVariable<WTime> start, OutVariable<WTimeSpan> duration) {
		start.value = new WTime(m_weatherCondition.m_time);
		start.value = WTime.add(start.value, new WTimeSpan(0, m_weatherCondition.m_firstHour, 0, 0));
		if (m_weatherCondition.getNumDays() > 0)
			duration.value = new WTimeSpan(m_weatherCondition.getNumDays(), -(23 - m_weatherCondition.m_lastHour) - m_weatherCondition.m_firstHour, 0, 0);
		else
			duration.value = null;
	}

	/**
	 * There are two basic formats supported by this weather stream object: a day's values can be specified as hourly conditions or daily observations.  If daily observations are
	 * provided, then hourly conditions are subsequently calculated.  If the day's values are already specified as hourly conditions, then there is no change.  This method may also
	 * append a preceding or following day to existing data.
	 * @param time Time identifying the day to modify, provided as a count of seconds since Midnight January 1, 1900 GMT time.
	 */
	public boolean makeHourlyObservations(WTime time) {
		if (!m_weatherCondition.makeHourlyObservations(time))
			return false;
		m_bRequiresSave = true;
		return true;
	}

	/**
	 * There are two basic formats supported by this weather stream object: a day's values can be specified as hourly conditions or daily observations.If hourly conditions are
	 * provided, then daily observations are subsequently calculated.  If the day's values are already specified as daily observations, then there is no change.
	 * This method may also
	 * append a preceding or following day to existing data.
	 * @param time Time identifying the day to modify, provided as a count of seconds since Midnight January 1, 1900 GMT time.
	 */
	public boolean makeDailyObservations(WTime time) {
		if (!m_weatherCondition.makeDailyObservations(time))
			return false;
		m_bRequiresSave = true;
		return true;
	}

	/**
	 * Returns the representation of a given day.
	 * @param time Time identifying the day to inspect, provided as a count of seconds since Midnight January 1, 1900 GMT time.
	 * @return
	 */
	public short isDailyObservations(WTime time) {
		return (short)m_weatherCondition.isHourlyObservations(time);
	}

	/**
	 * Returns the daily observations for the specified day.  If the day is represented as daily observations, this method returns these specified values.  If
	 * the day is represented as hourly readings, then it returns values calculated from the hourly readings.
	 * @param time Time identifying the day to inspect, provided as a count of seconds since Midnight January 1, 1900 GMT time.
	 * @param min_temp Returned minimum temperature.
	 * @param max_temp Returned maximum temperature.
	 * @param min_ws Returned minimum windspeed.
	 * @param max_ws Returned maximum windspeed.
	 * @param min_wg Returned minimum wind gust.
	 * @param max_wg Returned maximum wind gust.
	 * @param rh Returned relative humidity.
	 * @param precip Returned precipitation for the day.
	 * @param wd Returned mean wind direction, provided in Cartesian radians.
	 */
	public void getDailyValues(WTime time, OutVariable<Double> min_temp, OutVariable<Double> max_temp, OutVariable<Double> min_ws,
			OutVariable<Double> max_ws, OutVariable<Double> min_wg,
			OutVariable<Double> max_wg, OutVariable<Double> rh, OutVariable<Double> precip, OutVariable<Double> wd) {
		m_weatherCondition.getDailyWeatherValues(time, min_temp, max_temp, min_ws, max_ws, min_wg, max_wg, rh, precip, wd);
	}

	/**
	 * Sets the daily observations for the specified day.  If the day is represented as hourly readings, then this method will fail.
	 * @param time Time identifying the day to set, provided as a count of seconds since Midnight January 1, 1900 GMT time.
	 * @param min_temp Minimum temperature.
	 * @param max_temp Maximum temperature.
	 * @param min_ws Minimum windspeed.
	 * @param max_ws Maximum windspeed.
	 * @param min_wg Minimum wind gust.
	 * @param max_wg Maximum wind gust.
	 * @param rh Relative humidity.
	 * @param precip Daily precipitation.
	 * @param wd Mean wind direction.
	 */
	public void setDailyValues(WTime time, double min_temp, double max_temp, double min_ws, double max_ws, double min_wg, double max_wg,
	    double rh, double precip, double wd) {
		m_weatherCondition.setDailyWeatherValues(time, min_temp, max_temp, min_ws, max_ws, min_wg, max_wg, rh, precip, wd);
	}

	/**
	 * Gets the instantaneous values for Temperature, DewPointTemperature, RH, Precipitation,
	 * WindSpeed and WindDirection (and saves these values in the data structure wx). It
	 * also gets FFMC, ISI and FWI (and saves theses values in the data structure ifwi) and similarly gets the values
	 * for dFFMC, dDMC, dDC and dBUI (and saves them to the data structure dfwi).
	 * @param time Time identifying the day and hour to inspect, provided as a count of seconds since Midnight January 1, 1900 GMT time.
	 * @param interpolation_method Can only be CWFGM_GETWEATHER_INTERPOLATE.TEMPORAL.
	 * @param wx The data structure that stores the values for Temperature, Dew DewPointTemperature, RH, Precipitation, WindSpeed and WindDirection.
	 * @param ifwi The data structure that stores the values for FFMC, ISI and FWI.
	 * @param dfwi The data structure that stores the values for dFFMC, dDMC, dDC and dBUI.
	 */
	public void getInstantaneousValues(WTime time, long interpolation_method, OutVariable<IWXData> wx, OutVariable<IFWIData> ifwi, OutVariable<DFWIData> dfwi) {
		m_weatherCondition.getInstantaneousValues(time, interpolation_method, wx, ifwi, dfwi);
	}

	/**
	 * Sets the instantaneous values for Temperature, DewPointTemperature, RH, Precipitation, WindSpeed and WindDirection via the IWXData data structure.
	 * @param time Time identifying the day and hour to inspect, provided as a count of seconds since Midnight January 1, 1900 GMT time.
	 * @param wx The data structure that stores the values for Temperature, Dew DewPointTemperature, RH, Precipitation, WindSpeed and WindDirection.
	 */
	public void setInstantaneousValues(WTime time, IWXData wx) {
		OutVariable<IWXData> curr_wx = new OutVariable<IWXData>();
		curr_wx.value = new IWXData();
		m_weatherCondition.getInstantaneousValues(time, 0, curr_wx, null, null);
		if (curr_wx != null) {
			if (wx.dewPointTemperature <= -300.0) {
				if ((wx.temperature == curr_wx.value.temperature) &&
				    (wx.rh == curr_wx.value.rh) &&
				    (wx.precipitation == curr_wx.value.precipitation) &&
				    (wx.windDirection == curr_wx.value.windDirection) &&
				    (wx.windSpeed == curr_wx.value.windSpeed) &&
					(wx.windGust == curr_wx.value.windGust))
					return;
			}
		}

		boolean b = m_weatherCondition.setHourlyWeatherValues(time, wx.temperature, wx.rh, wx.precipitation, wx.windSpeed, wx.windGust, wx.windDirection, wx.dewPointTemperature);
		if (!b)
			return;
		m_bRequiresSave = true;
	}

	/**
	 * This property is unused by this object, and is available for exclusive use by the client code.  It is a VARIANT value to ensure
	 * that the client code can store a pointer value (if it chooses) for use in manual subclassing this object.  This value is not loaded or
	 * saved during serialization operations, and it is the responsibility of the client code to manage any value or object stored here.
	 * @return
	 */
	public Object getUserData() {
		return m_userData;
	}

	/**
	 * This property is unused by this object, and is available for exclusive use by the client code.  It is a VARIANT value to ensure
	 * that the client code can store a pointer value (if it chooses) for use in manual subclassing this object.  This value is not loaded or
	 * saved during serialization operations, and it is the responsibility of the client code to manage any value or object stored here.
	 * @param newVal
	 */
	public void setUserData(Object newVal) {
		m_userData = newVal;
	}

	/**
	 * Retrieves the daily standard (Van Wagner) FFMC for the specified day.
	 * Note that other functions typically return the hourly FFMC value.  Note that daily FFMC values change at noon LST, not at midnight.
	 * @param time A GMT time provided as seconds since January 1st, 1900.
	 * @return
	 */
	public Double dailyStandardFFMC(WTime time) {
		OutVariable<Boolean> spec = new OutVariable<Boolean>();
		OutVariable<Double> ffmc = new OutVariable<Double>();
		boolean valid_date = m_weatherCondition.dailyFFMC(time, ffmc, spec);
		if (!valid_date)		return null;
		if (ffmc.value < 0.0)	return null;
		return ffmc.value;
	}

	/**
	 * Retrieves information regarding whether a particular day's data was loaded from a file.
	 * @param time Time identifying the day and hour to inspect, provided as a count of seconds since Midnight January 1, 1900 GMT time.
	 * @return
	 */
	public boolean isImportedFromFile(WTime time) {
		if (time.getTotalSeconds() == 0)
			return (m_weatherCondition.m_options & 0x00000020) != 0;

		int val = m_weatherCondition.isOriginFile(time);
		if (val == 1)								return false;
		if (val == 2)								return false;
		return true;
	}

	private void readObject(ObjectInputStream inStream) throws IOException, ClassNotFoundException {
		int version = inStream.readInt();
		if (version < 5) {
			m_loadWarning += "Weather Stream: Invalid version.\n";
			//AfxThrowArchiveException(CArchiveException::badSchema, "CWFGM Weather Stream");
		}
		if (version > serialVersionUID) {
			m_loadWarning += "Weather Stream: Version too new.\n";
			//AfxThrowArchiveException(CArchiveException::badSchema, "CWFGM Weather Stream");
		}
		if (version > 1)
			m_weatherCondition.serialize(inStream, m_loadWarning);
		if ((version >= 4) && (version < 6)) {
			boolean m_bImportedFromFile = inStream.readBoolean();
			if (m_bImportedFromFile)
				m_weatherCondition.m_options |= 0x00000020;
		}
	}

	private void writeObject(ObjectOutputStream outStream) throws IOException {
		outStream.writeInt((int)serialVersionUID);
		m_weatherCondition.serialize(outStream, m_loadWarning);
	}
	
	public int getWeatherMode() {
		return m_weatherCondition.getMode();
	}
}
