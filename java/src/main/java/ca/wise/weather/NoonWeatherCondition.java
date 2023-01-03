/***********************************************************************
 * REDapp - NoonWeatherCondition.java
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
import static ca.hss.times.WTime.FORMAT_AS_LOCAL;
import static ca.hss.times.WTime.FORMAT_WITHDST;
import static ca.hss.times.WTime.greaterThan;
import static ca.hss.times.WTime.lessThan;
import static ca.hss.times.WTime.notEqual;
import static ca.hss.times.WTime.subtract;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.StringTokenizer;

import ca.wise.grid.GRID_ATTRIBUTE;
import ca.wise.grid.DFWIData;
import ca.hss.general.ERROR;
import ca.hss.general.OutVariable;
import ca.hss.math.LinkedList;
import ca.hss.text.StringExtensions;
import ca.hss.times.WTime;
import ca.hss.times.WTimeManager;
import ca.hss.times.WTimeSpan;
import ca.hss.times.WorldLocation;

public class NoonWeatherCondition {
	LinkedList<NoonWeather> m_readings;
	public WTimeManager m_timeManager;
	public WorldLocation m_worldLocation;
	public WTime m_time;
	public boolean m_isCalculatedValuesValid = false;
	DFWIData m_spec_day;
	
	public NoonWeatherCondition(WTimeManager timeManager) {
		m_timeManager = timeManager;
		m_worldLocation = m_timeManager.getWorldLocation();
		m_time = new WTime(0L, m_timeManager);
		m_readings = new LinkedList<>(NoonWeather.class);
		m_spec_day = new DFWIData();
		m_spec_day.dFFMC = -1.0;
		m_spec_day.dDMC = -1.0;
		m_spec_day.dDC = -1.0;
		m_spec_day.dBUI = -1.0;
	}
	
	public WTimeManager getTimeManager() {
		return m_timeManager;
	}

	/**
	 * Gets the value of an "option" and saves it in the "value" variable provided.
	 * @param option The weather option of interest.
	 * @return
	 */
	public Object getAttribute(int option) {
		switch (option) {
		case WEATHER_OPTION.INITIAL_FFMC:
			return Double.valueOf(m_spec_day.dFFMC);
		case WEATHER_OPTION.INITIAL_DC:
			return Double.valueOf(m_spec_day.dDC);
		case WEATHER_OPTION.INITIAL_DMC:
			return Double.valueOf(m_spec_day.dDMC);
		case WEATHER_OPTION.INITIAL_BUI:
			return Double.valueOf(m_spec_day.dBUI);
		case GRID_ATTRIBUTE.LATITUDE:
			return Double.valueOf(m_worldLocation.getLatitude());
		case GRID_ATTRIBUTE.LONGITUDE:
			return Double.valueOf(m_worldLocation.getLongitude());
		case GRID_ATTRIBUTE.TIMEZONE:
			return Long.valueOf(m_worldLocation.getTimezoneOffset().getTotalSeconds());
		case GRID_ATTRIBUTE.DAYLIGHT_SAVINGS:
			return Long.valueOf(m_worldLocation.getDSTAmount().getTotalSeconds());
		case GRID_ATTRIBUTE.DST_START:
			return Long.valueOf(m_worldLocation.getStartDST().getTotalSeconds());
		case GRID_ATTRIBUTE.DST_END:
			return Long.valueOf(m_worldLocation.getEndDST().getTotalSeconds());
		case WEATHER_OPTION.START_TIME:
			return Long.valueOf(m_time.getTime((short)0));
		case WEATHER_OPTION.END_TIME:
			return Long.valueOf(getEndTime().getTime((short)0));
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
		Double dValue;
		Long llvalue;
		WTimeSpan ts;
		if (value == null)
			throw new IllegalArgumentException();
		switch (option) {
		case WEATHER_OPTION.INITIAL_FFMC:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property INITIAL_FFMC must be passed a Double");
			dValue = (Double)value;
			if (dValue < 0.0)	return;
			if (dValue > 101.0)	return;
			if (m_spec_day.dFFMC != dValue) {
				clearConditions();
				m_spec_day.dFFMC = dValue;
			}
			break;
		case WEATHER_OPTION.INITIAL_DC:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property INITIAL_DC must be passed a Double");
			dValue = (Double)value;
			if (dValue < 0.0)	return;
			if (dValue > 1500.0)	return;
			if (m_spec_day.dDC != dValue) {
				clearConditions();
				m_spec_day.dDC = dValue;
			}
			break;
		case WEATHER_OPTION.INITIAL_DMC:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property INITIAL_DMC must be passed a Double");
			dValue = (Double)value;
			if (dValue < 0.0)	return;
			if (dValue > 500.0)	return;
			if (m_spec_day.dDMC != dValue) {
				clearConditions();
				m_spec_day.dDMC = dValue;
			}
			break;
		case GRID_ATTRIBUTE.LATITUDE:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property LATITUDE must be passed a Double");
			dValue = (Double)value;
			if (m_worldLocation.getLatitude() != dValue) {
				clearConditions();
				m_worldLocation.setLatitude(dValue);
			}
			break;
		case GRID_ATTRIBUTE.LONGITUDE:
			if (!(value instanceof Double))
				throw new IllegalArgumentException("The property LONGITUDE must be passed a Double");
			dValue = (Double)value;
			if (m_worldLocation.getLongitude() != dValue) {
				clearConditions();
				m_worldLocation.setLongitude(dValue);
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
			if (WTimeSpan.equal(m_worldLocation.getTimezoneOffset(), ts))
				return;
			clearConditions();
			WTime t1 = new WTime(m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);

			m_worldLocation.setTimezoneOffset(ts);
			WTime t2 = new WTime(t1, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)-1);
			m_time = t2;
			break;
		case GRID_ATTRIBUTE.DAYLIGHT_SAVINGS:
			if (!(value instanceof Long))
				throw new IllegalArgumentException("The property DAYLIGHT_SAVINGS must be passed a Long");
			llvalue = (Long)value;
			ts = new WTimeSpan(llvalue);
			if (llvalue > (2 * 60 * 60))						return;
			if (llvalue < (-2 * 60 * 60))						return;
			if ((llvalue > 0) && (ts.getSeconds() > 0))			return;
			if (WTimeSpan.equal(m_worldLocation.getDSTAmount(), ts))
				return;
			clearConditions();
			t1 = new WTime(m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);

			m_worldLocation.setDSTAmount(ts);
			t2 = new WTime(t1, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)-1);
			m_time = t2;
			break;
		case GRID_ATTRIBUTE.DST_START:
			if (!(value instanceof Long))
				throw new IllegalArgumentException("The property DST_START must be passed a Long");
			llvalue = (Long)value;
			ts = new WTimeSpan(llvalue);
			if (llvalue > (24 * 60 * 60 * 366))		return;
			if (ts.getSeconds() > 0 || ts.getMinutes() > 0 || ts.getHours() > 0)
				return;
			if (WTimeSpan.equal(m_worldLocation.getStartDST(), ts))
				return;
			clearConditions();
			t1 = new WTime(m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);

			m_worldLocation.setStartDST(ts);
			t2 = new WTime(t1, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)-1);
			m_time = t2;
			break;
		case GRID_ATTRIBUTE.DST_END:
			if (!(value instanceof Long))
				throw new IllegalArgumentException("The property DST_END must be passed a Long");
			llvalue = (Long)value;
			ts = new WTimeSpan(llvalue);
			if (llvalue > (24 * 60 * 60 * 366))		return;
			if (ts.getSeconds() > 0 || ts.getMinutes() > 0 || ts.getHours() > 0)
				return;
			if (WTimeSpan.equal(m_worldLocation.getEndDST(), ts))
				return;
			clearConditions();
			t1 = new WTime(m_time, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)1);

			m_worldLocation.setEndDST(ts);
			t2 = new WTime(t1, WTime.FORMAT_AS_LOCAL | WTime.FORMAT_WITHDST, (short)-1);
			m_time = t2;
			break;
		}
	}

	public long importFile(String fileName, int options) throws IOException {
		List<String> header = new ArrayList<String>();
		NoonWeather nw;
		long hr = ERROR.S_OK;
		boolean can_append = (options & WEATHERSTREAM_IMPORT.SUPPORT_APPEND) != 0;

		if (m_readings.isEmpty())
			can_append = true;

		if ((options & WEATHERSTREAM_IMPORT.PURGE) != 0) {
			clearWeatherData();
			can_append = true;
		}
		
		long lines = 0;
		File f = new File(fileName);
		if (f != null && f.exists()) {
			try (BufferedReader rdr = new BufferedReader(new FileReader(f))) {
				String line;
				String file_type;
	
				if ((line = rdr.readLine()) == null) {
					rdr.close();
					return ERROR.READ_FAULT | ERROR.SEVERITY_WARNING;
				}
				processHeader(line, header);
	
				WTime lastTime = new WTime(0L, m_timeManager);
				if (m_readings.size() > 0)
					lastTime = add(m_time, new WTimeSpan(m_readings.size(), 0, 0, 0));
	
				while ((line = rdr.readLine()) != null) {
					for(int i = line.length() - 1; line.charAt(i) == ' ' || line.charAt(i) == 0x0a || line.charAt(i) == 0x0d ||
							line.charAt(i) == ',' || line.charAt(i) == '\t' || line.charAt(i) == ';' || line.charAt(i) == 0x22; i--)
						line = line.substring(0, line.length() - 1);
					if (line.length() == 0)
						continue;
					OutVariable<Double> temp = new OutVariable<Double>();
					temp.value = -100.0;
					OutVariable<Double> rh = new OutVariable<Double>();
					rh .value = -100.0;
					OutVariable<Double> ws = new OutVariable<Double>();
					ws.value =  -100.0;
					OutVariable<Double> wg = new OutVariable<Double>();
					wg.value =  -100.0;
					OutVariable<Double> wd = new OutVariable<Double>();
					wd.value = -100.0;
					OutVariable<Double> precip = new OutVariable<Double>();
					precip.value = -100.0;
	
					file_type = fillDailyLineValue(header, line, temp, rh, ws, wg, wd, precip);
	
					if ((wd.value < 0.0) || (wd.value > 360.0) ||
					    (ws.value < 0.0) ||
					    (rh.value < 0.0) || (rh.value > 100.0) ||
					    (precip.value < 0.0) ||
					    (temp.value < -50.0) || (temp.value > 60.0)) {
						hr = ERROR.INVALID_DATA | ERROR.SEVERITY_WARNING;
						break;
					}
	
					wd.value = ca.hss.math.General.DEGREE_TO_RADIAN(ca.hss.math.General.COMPASS_TO_CARTESIAN_DEGREE(wd.value));
					if ((ws.value > 0.0) && (wd.value == 0.0))
						wd.value = ca.hss.math.General.TWO_PI;
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
	
					nw = getNWReading(t, can_append);
					if (nw == null) {
						hr = ERROR.ATTEMPT_APPEND;
						break;
					}
					nw.setNoonWeather(temp.value, rh.value, ws.value, wg.value, wd.value, precip.value);
					lines++;
				}
			}
		}

		m_isCalculatedValuesValid = false;
		return hr;
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

	private void distributeDailyValue(List<String> header, int index, double value, OutVariable<Double> temp, OutVariable<Double> rh,
			OutVariable<Double> ws, OutVariable<Double> wg, OutVariable<Double> wd, OutVariable<Double> precip) {
		assert temp != null;
		assert rh != null;
		assert ws != null;
		assert wg != null;
		assert wd != null;
		assert precip != null;
		if(index > header.size() - 1 || index<0)
			return;
		String str = header.get(index);
		if (str.compareToIgnoreCase("temp") == 0)
			temp.value = value;
		else if ((str.compareToIgnoreCase("rh") == 0)  || (str.compareToIgnoreCase("min_rh") == 0) || str.compareToIgnoreCase("relative_humidity") == 0)
			rh.value = value;
		else if (str.compareToIgnoreCase("ws") == 0)
			ws.value = value;
		else if (str.compareToIgnoreCase("wg") == 0 || str.compareToIgnoreCase("gust") == 0 || str.compareToIgnoreCase("gusting") == 0 || str.compareToIgnoreCase("wind_gust") == 0 || str.compareToIgnoreCase("windgust") == 0)
			wg.value = value;
		else if (str.compareToIgnoreCase("wd") == 0 || str.compareToIgnoreCase("dir") == 0 || str.compareToIgnoreCase("wind_direction") == 0)
			wd.value = value;
		else if ((str.compareToIgnoreCase("precip") == 0) || (str.compareToIgnoreCase("rain") == 0) || str.compareToIgnoreCase("precipitation") == 0)
			precip.value = value;
	}

	private String fillDailyLineValue(List<String> header, String line, OutVariable<Double> temp, OutVariable<Double> rh,
			OutVariable<Double> ws, OutVariable<Double> wg, OutVariable<Double> wd, OutVariable<Double> precip) {
		assert temp != null;
		assert rh != null;
		assert ws != null;
		assert wg != null;
		assert wd != null;
		assert precip != null;
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
			distributeDailyValue(header, ++i, ReadIn, temp, rh, ws, wg, wd, precip);
		}
		return retVal;
	}

	public WTime getEndTime() {
		long count = m_readings.size();
		WTimeSpan timeSpan = new WTimeSpan(count, 0, 0, 0);
		WTime EndTime = new WTime(m_time);
		EndTime.add(timeSpan);
		EndTime.subtract(new WTimeSpan(0, 0, 0, 1));
		return EndTime;
	}

	public NoonWeather getNWReading(WTime time, boolean add) {
		NoonWeather dc;

		WTimeSpan index = subtract(time, m_time);

		if (index.getTotalSeconds() < 0) {
			return null;
	 	}
		dc = m_readings.get((long)index.getDays());
		if ((dc == null) && (add)) {
			if (m_readings.size() > 0) {
				if (index.getDays() != m_readings.size())
					return null;
			}
			dc = new NoonWeather(this);
            m_readings.addLast(dc);
			clearConditions();
		}
		return dc;
	}

	public boolean getNoonWeatherValues(WTime time, OutVariable<Double> temp, OutVariable<Double> dew,
			OutVariable<Double> rh, OutVariable<Double> ws, OutVariable<Double> wg, OutVariable<Double> wd, OutVariable<Double> precip) {
		assert temp != null;
		assert rh != null;
		assert ws != null;
		assert wg != null;
		assert wd != null;
		assert precip != null;
		NoonWeather nw = getNWReading(time, false);
		if (nw != null) {
			calculateValues();
			nw.getNoonWeather(temp, dew, rh, ws, wg, wd, precip);
		}
		return (nw != null);
	}
	
	public boolean getNoonFWI(WTime time, OutVariable<Double> dc, OutVariable<Double> dmc, OutVariable<Double> ffmc, OutVariable<Double> bui,
			OutVariable<Double> isi, OutVariable<Double> fwi) {
		assert dc != null;
		assert dmc != null;
		assert ffmc != null;
		assert bui != null;
		assert isi != null;
		assert fwi != null;
		NoonWeather nw = getNWReading(time, false);
		if (nw != null) {
			calculateValues();
			nw.getNoonFWI(dc, dmc, ffmc, bui, isi, fwi);
		}
		return (nw != null);
	}

	public boolean setNoonWeatherValues(WTime time, double temp, double rh, double ws, double wg, double wd, double precip) {
		NoonWeather dc = getNWReading(time, true);
		if (dc != null) {
			dc.setNoonWeather(temp, rh, ws, wg, wd, precip);
			clearConditions();
		}
		return (dc != null);
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

	public void clearConditions() {
		m_isCalculatedValuesValid = false;
	}

	public void clearWeatherData() {
		while (m_readings.removeFirst() != null);
	}

	/**
	 * Returns the start time and duration for the data stored in this weather stream object.
	 * These objects return this data in GMT, where local time is set by SetAttribute().
	 * @param start Start time for the weather stream, specified in GMT time zone, as count of seconds since January 1, 1900.
	 * @param duration Duration of the weather stream, provided as a count of seconds.
	 */
	public void getValidTimeRange(OutVariable<WTime> start, OutVariable<WTimeSpan> duration) {
		start.value = new WTime(m_time);
		if (getNumDays() > 0)
			duration.value = new WTimeSpan(getNumDays(), 0, 0, 0);
		else
			duration.value = null;
	}

	public long getNumDays()				{ return m_readings.size(); }

	public void calculateValues() {
		if (m_isCalculatedValuesValid)
			return;
		m_isCalculatedValuesValid = true;

		if (m_readings.isEmpty())
			return;

		NoonWeather nw;
		int i = 0;
		nw = m_readings.getFirst();
		while (nw.getNext() != null) {
			nw.calculateTimes(i++);
			nw = nw.getNext();
		}
		nw = m_readings.getFirst();
		while (nw.getNext() != null) {
			nw.calculateFWI();
			nw = nw.getNext();
		}
	}

	public boolean dc(WTime time, OutVariable<Double> dc) {
		assert dc != null;
		WTime dayNeutral = new WTime(time, FORMAT_AS_LOCAL, (short)1);
		WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)-1);
		WTime dayNoon = new WTime(dayLST);
		dayNoon.subtract(new WTimeSpan(0, 12, 0, 0));
		dayNoon.purgeToDay(FORMAT_AS_LOCAL | FORMAT_WITHDST);
		NoonWeather _dc = getNWReading(dayNoon, false);
		if (_dc != null) {
			calculateValues();
			dc.value = _dc.getDC();
		} else if (lessThan(dayNoon, m_time)) {
			dc.value = m_spec_day.dDC;
			return true;
		}
		return (_dc != null);
	}

	public boolean dmc(WTime time, OutVariable<Double> dmc) {
		assert dmc != null;
		WTime dayNeutral = new WTime(time, FORMAT_AS_LOCAL, (short)1);
		WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)-1);
		WTime dayNoon = new WTime(dayLST);
		dayNoon.subtract(new WTimeSpan(0, 12, 0, 0));
		dayNoon.purgeToDay(FORMAT_AS_LOCAL | FORMAT_WITHDST);
		NoonWeather _dc = getNWReading(dayNoon, false);
		if (_dc != null) {
			calculateValues();
			dmc.value = _dc.getDMC();
		} else if (lessThan(dayNoon, m_time)) {
			dmc.value = m_spec_day.dDMC;
			return true;
		}
		return (_dc != null);
	}

	public boolean dailyFFMC(WTime time, OutVariable<Double> ffmc) {
		assert ffmc != null;
		WTime dayNeutral = new WTime(time, FORMAT_AS_LOCAL, (short)1);
		WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)-1);
		WTime dayNoon = new WTime(dayLST);
		dayNoon.subtract(new WTimeSpan(0, 12, 0, 0));
		dayNoon.purgeToDay(FORMAT_AS_LOCAL | FORMAT_WITHDST);
		NoonWeather _dc = getNWReading(dayNoon, false);
		if (_dc != null) {
			calculateValues();
			ffmc.value = _dc.getDailyFFMC();
		}
		else if (lessThan(dayNoon, m_time)) {
			ffmc.value = m_spec_day.dFFMC;
			return true;
		}
		return (_dc != null);
	}
}
