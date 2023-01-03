/***********************************************************************
 * REDapp - DailyCondition.java
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

import static ca.hss.times.WTime.*;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;

import ca.wise.fwi.Fwi;
import ca.wise.grid.DFWIData;
import ca.wise.grid.IFWIData;
import ca.hss.annotations.Source;
import ca.hss.general.OutVariable;
import ca.hss.times.WTime;
import ca.hss.times.WTimeSpan;

@Source(sourceFile="DayCondition.cpp", project="WeatherCOM")
public class DailyCondition extends DailyWeather implements Serializable {
	public static final long serialVersionUID = 15;
	
	public IFWIData[] m_spec_hr = new IFWIData[24];
	public IFWIData[] m_calc_hr = new IFWIData[24];
	public DFWIData m_spec_day,
				    m_calc_day;
	
	public DailyCondition() { super(); }
	
	public DailyCondition(WeatherCondition wc) {
		super(wc);
		m_spec_day = new DFWIData();
		m_spec_day.dBUI = m_spec_day.dDC = m_spec_day.dDMC = m_spec_day.dFFMC = m_spec_day.dISI = m_spec_day.dFWI = -1.0;
		m_spec_day.dSpecifiedBits = 0;
		
		m_calc_day = new DFWIData();
		m_calc_day.dBUI = m_calc_day.dDC = m_calc_day.dDMC = m_calc_day.dFFMC = m_calc_day.dISI = m_calc_day.dFWI = -1.0;
		m_calc_day.dSpecifiedBits = 0;
		
		for (short i = 0; i < 24; i++) {
			m_spec_hr[i] = new IFWIData();
			m_spec_hr[i].dFFMC = m_spec_hr[i].dFWI = m_spec_hr[i].dISI = -1.0;
			m_spec_hr[i].dSpecifiedBits = 0;

			m_calc_hr[i] = new IFWIData();
			m_calc_hr[i].dFFMC = m_calc_hr[i].dFWI = m_calc_hr[i].dISI = -1.0;
			m_calc_hr[i].dSpecifiedBits = 0;
		}
	}
	
	@Override
	public DailyCondition getNext() { return (DailyCondition)super.getNext(); }
	
	@Override
	public DailyCondition getPrevious() { return (DailyCondition)super.getPrevious(); }
	
	double getHourlyFFMC(WTime time) 			{ int hour = (int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST); return m_calc_hr[hour].dFFMC; };
	public boolean isHourlyFFMCSpecified(WTime time) 		{ int hour = (int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST); return (m_calc_hr[hour].dFFMC >= 0.0) ? true : false; };
	public boolean isInterpolated(WTime time) { return isHourInterpolated(time); }
	public boolean isCorrected(WTime time) { return isHourCorrected(time); }
	public double getISI(WTime time) 				{ int hour = (int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST); return m_calc_hr[hour].dISI; };
	public double getFWI(WTime time) 				{ int hour = (int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST); return m_calc_hr[hour].dFWI; };
	
	public void setHourlyFFMC(WTime time, double ffmc)	{ int hour = (int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST); m_spec_hr[hour].dFFMC = (ffmc >= 0.0) ? ffmc : -1.0; };
	public void setISI(WTime time, double isi) 		{ int hour = (int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST); m_spec_hr[hour].dISI = (isi >= 0.0) ? isi : -1.0; };
	public void setFWI(WTime time, double fwi) 		{ int hour = (int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST); m_spec_hr[hour].dFWI = (fwi >= 0.0) ? fwi : -1.0; };

	public double getDailyFFMC() 					{ return m_calc_day.dFFMC; }
	public double getDailyISI()					{ return m_calc_day.dISI; }
	public double getDailyFWI()					{ return m_calc_day.dFWI; }
	public double getDC() 						{ return m_calc_day.dDC; }
	public double getDMC() 						{ return m_calc_day.dDMC; }
	public double getBUI() 						{ return m_calc_day.dBUI; }
	public boolean isDailyFFMCSpecified() 				{ return (m_spec_day.dFFMC >= 0.0); }
	public boolean isDCSpecified() 					{ return (m_spec_day.dDC >= 0.0); }
	public boolean isDMCSpecified() 					{ return (m_spec_day.dDMC >= 0.0); }
	public boolean isBUISpecified() 					{ return (m_spec_day.dBUI >= 0.0); }

	public void setDailyFFMC(double ffmc) 				{ m_spec_day.dFFMC = (ffmc >= 0.0) ? ffmc : -1.0; }
	public void setDC(double dc) 					{ m_spec_day.dDC = (dc >= 0.0) ? dc : -1.0; }
	public void setDMC(double dmc)					{ m_spec_day.dDMC = (dmc >= 0.0) ? dmc : -1.0; }
	public void setBUI(double bui) 					{ m_spec_day.dBUI = (bui >= 0.0) ? bui : -1.0; }

	public void clearHourlyData(int hour)				{ m_spec_hr[hour].dFFMC = m_spec_hr[hour].dFWI = m_spec_hr[hour].dISI = -1.0; }
	public void clearHourlyData(WTime time)			{ int hour = (int)time.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST); clearHourlyData(hour); }
	public void clearDailyData()						{ m_spec_day.dFFMC = m_spec_day.dDC = m_spec_day.dDMC = m_spec_day.dBUI = m_spec_day.dISI = m_spec_day.dFWI = -1.0; }
	
	public void calculateFWI() {
		calculateDC();
		calculateDMC();
		calculateBUI();
		calculateDailyFFMC();
		calculateHourlyFFMC();
		calculateRemainingFWI();
	}
	
	public boolean anyFWICodesSpecified() {
		if (m_spec_day.dFFMC >= 0.0)	return true;
		if (m_spec_day.dDMC >= 0.0)	return true;
		if (m_spec_day.dDC >= 0.0)	return true;
		if (m_spec_day.dBUI >= 0.0)	return true;
		for (short i = 0; i < 24; i++) {
			if (m_spec_hr[i].dFFMC >= 0.0)	return true;
			if (m_spec_hr[i].dISI >= 0.0)	return true;
			if (m_spec_hr[i].dFWI >= 0.0)	return true;
		}
		return false;
	}
	
	private void calculateDC() {
		if ((m_weatherCondition.m_options & 0x00000004) != 0 && (m_spec_day.dDC >= 0.0))
			m_calc_day.dDC = m_spec_day.dDC;
		else {
			WTime dayNeutral = new WTime(m_dayStart, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)1);
			WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL, (short)-1);
			WTime dayNoon = new WTime(dayLST);
			dayNoon.add(new WTimeSpan(0, 12, 0, 0));

			double rain = getDailyPrecip();

			OutVariable<Boolean> spec = new OutVariable<Boolean>();
			OutVariable<Double> in_dc = new OutVariable<Double>();
			m_weatherCondition.dc(m_dayStart, in_dc, spec);

			m_calc_day.dDC = Fwi.dC(in_dc.value, rain, getHourlyTemp(dayNoon),
			    m_weatherCondition.m_worldLocation.getLatitude(), m_weatherCondition.m_worldLocation.getLongitude(),
			    (int)m_dayStart.getMonth(FORMAT_AS_LOCAL | FORMAT_WITHDST) - 1);
		}
	}
	
	private void calculateDMC() {
		if ((m_weatherCondition.m_options & 0x00000004) != 0 && (m_spec_day.dDMC >= 0.0))
			m_calc_day.dDMC = m_spec_day.dDMC;
		else {
			WTime dayNeutral = new WTime(m_dayStart, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)1);
			WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL, (short)-1);
			WTime dayNoon = new WTime(dayLST);
			dayNoon.add(new WTimeSpan(0, 12, 0, 0));

			double rain = getDailyPrecip();

			OutVariable<Boolean> spec = new OutVariable<Boolean>();
			OutVariable<Double> in_dmc = new OutVariable<Double>();
			m_weatherCondition.dmc(m_dayStart, in_dmc, spec);

			m_calc_day.dDMC = Fwi.dMC(in_dmc.value, rain, getHourlyTemp(dayNoon),
			    m_weatherCondition.m_worldLocation.getLatitude(), m_weatherCondition.m_worldLocation.getLongitude(),
			    (int)m_dayStart.getMonth(FORMAT_AS_LOCAL | FORMAT_WITHDST) - 1, getHourlyRH(dayNoon));
		}
	}
	
	private void calculateBUI() {
		if ((m_weatherCondition.m_options & 0x00000004) != 0 && (m_spec_day.dBUI >= 0.0))
			m_calc_day.dBUI = m_spec_day.dBUI;
		else {
			m_calc_day.dBUI = Fwi.bui(m_calc_day.dDC, m_calc_day.dDMC);
		}
	}
	
	private void calculateDailyFFMC() {
		if ((m_weatherCondition.m_options & 0x00000004) != 0 && (m_spec_day.dFFMC >= 0.0))
			m_calc_day.dFFMC = m_spec_day.dFFMC;
		else {
			WTime dayNeutral = new WTime(m_dayStart, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)1);
			WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL, (short)-1);
			WTime dayNoon = new WTime(dayLST);
			dayNoon.add(new WTimeSpan(0, 12, 0, 0));

			double rain = getDailyPrecip();

			OutVariable<Boolean> spec = new OutVariable<Boolean>();
			OutVariable<Double> in_ffmc = new OutVariable<Double>();
 			m_weatherCondition.dailyFFMC(m_dayStart, in_ffmc, spec);

 			m_calc_day.dFFMC = Fwi.dailyFFMCVanWagner(in_ffmc.value, rain,
			    getHourlyTemp(dayNoon), getHourlyRH(dayNoon), getHourlyWS(dayNoon));
		}
	}
	
	private void calculateHourlyFFMC() {
		double val;
		WTime loop = new WTime(m_dayStart),
			end = new WTime(m_dayStart);
		end.add(new WTimeSpan(0, 23, 0, 0));
		
		WTime streambegin = new WTime(WTime.add(m_weatherCondition.m_time, new WTimeSpan(0, m_weatherCondition.m_firstHour, 0, 0)));
		WTime streamend = new WTime(WTime.add(m_weatherCondition.m_time, new WTimeSpan(m_weatherCondition.m_readings.size() - 1, m_weatherCondition.m_lastHour, 0, 0)));

		WTime dayNeutral = new WTime(m_dayStart, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)1);
		WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL, (short)-1);
		int i;

		if (getYesterday() == null) {
			double in_ffmc;
			if ((WTimeSpan.equal(m_weatherCondition.m_initialHFFMCTime, new WTimeSpan(-1))) || ((m_weatherCondition.m_options & 0x00000003) != 1)) {
				in_ffmc = m_calc_day.dFFMC;
				loop = new WTime(dayLST);
				loop.add(new WTimeSpan(0, 12, 0, 0));
			}
			else {
				//start time will always be hour 0
				loop = new WTime(m_dayStart);
				//TODO maybe call getTomorrow and call this method with a parameter to force checking of
				//the HFFMC value so that initial HFFMC times < the first hour of the stream can be used.
				//for now if the initial HFFMC time is tomorrow use the first hour of the stream instead
				if (m_weatherCondition.m_initialHFFMCTime.getTotalHours() < m_weatherCondition.m_firstHour)
					m_weatherCondition.m_initialHFFMCTime = new WTimeSpan(0, m_weatherCondition.m_firstHour, 0, 0);

				//add the initial HFFMC time to the start time
				loop.add(m_weatherCondition.m_initialHFFMCTime);
				in_ffmc = m_weatherCondition.m_initialHFFMC;
			}

			i = (int)loop.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST);
			boolean calculate = true;
			if ((m_weatherCondition.m_options & 0x00000004L) != 0L) {
				if (m_spec_hr[i].dFFMC >= 0.0)
					m_calc_hr[i].dFFMC = m_spec_hr[i].dFFMC;
				else
					m_calc_hr[i].dFFMC = in_ffmc;
			}
			else
				m_calc_hr[i].dFFMC = in_ffmc;

			if (WTime.lessThan(streambegin, m_dayStart))
				streambegin = m_dayStart;
			for (loop.subtract(WTimeSpan.Hour), i--; greaterThanEqualTo(loop, streambegin) && i >= 0; loop.subtract(WTimeSpan.Hour), i--) {
				calculate = true;
				if ((m_weatherCondition.m_options & 0x00000004) != 0) {
					if (m_spec_hr[i].dFFMC >= 0.0) {
						m_calc_hr[i].dFFMC = m_spec_hr[i].dFFMC;
						calculate = false;
					}
				}

				if (calculate) {
					OutVariable<Double> temp, rh, precip, ws, wg, wd, dew;
					temp = new OutVariable<Double>();
					rh = new OutVariable<Double>();
					precip = new OutVariable<Double>();
					ws = new OutVariable<Double>();
					wg = new OutVariable<Double>();
					wd = new OutVariable<Double>();
					dew = new OutVariable<Double>();
					switch ((int)(m_weatherCondition.m_options & 0x00000003)) {
						case 2:
						case 3:
							OutVariable<Boolean> spec = new OutVariable<Boolean>();
							OutVariable<Double> prev_ffmc = new OutVariable<Double>();
							m_weatherCondition.dailyFFMC(m_dayStart, prev_ffmc, spec);

							hourlyWeather(loop, temp, rh, precip, ws, wg, wd, dew);
							val = Fwi.hourlyFFMCLawsonContiguous(prev_ffmc.value,
							    m_calc_day.dFFMC, rh.value, rh.value, rh.value, subtract(loop, dayLST).getTotalSeconds());

							break;
						default:
							in_ffmc = m_calc_hr[i + 1].dFFMC;
							hourlyWeather(add(loop, WTimeSpan.Hour), temp, rh, precip, ws, wg, wd, dew);
							val = Fwi.hourlyFFMCVanWagnerPrevious(in_ffmc, precip.value, temp.value, rh.value, ws.value);

							break;
					}
					m_calc_hr[i].dFFMC = val;
				}
			}
			if ((WTimeSpan.equal(m_weatherCondition.m_initialHFFMCTime, new WTimeSpan(-1))) || ((m_weatherCondition.m_options & 0x00000003) != 1)) {
				loop = new WTime(dayLST);
				loop.add(new WTimeSpan(0, 12, 0, 0));
			} else {
				loop = new WTime(m_dayStart);
				loop.add(m_weatherCondition.m_initialHFFMCTime);
				loop.add(WTimeSpan.Hour);
			}
		}

		if (WTime.greaterThan(end, streamend))
			end = streamend;
		for (i = (int)loop.getHour(FORMAT_AS_LOCAL | FORMAT_WITHDST); lessThanEqualTo(loop, end) && i < 24; i++, loop.add(WTimeSpan.Hour)) {
			boolean calculate = true;
			if ((m_weatherCondition.m_options & 0x00000004) != 0) {
				if (m_spec_hr[i].dFFMC >= 0.0) {
					m_calc_hr[i].dFFMC = m_spec_hr[i].dFFMC;
					calculate = false;
				}
			}

			if (calculate) {
				OutVariable<Double> temp, rh, precip, ws, wg, wd, dew;
				temp = new OutVariable<Double>();
				rh = new OutVariable<Double>();
				precip = new OutVariable<Double>();
				ws = new OutVariable<Double>();
				wg = new OutVariable<Double>();
				wd = new OutVariable<Double>();
				dew = new OutVariable<Double>();
				hourlyWeather(loop, temp, rh, precip, ws, wg, wd, dew);

				OutVariable<Boolean> spec = new OutVariable<Boolean>();
				OutVariable<Double> prev_ffmc = new OutVariable<Double>();
				OutVariable<Double> prev_hr_ffmc = new OutVariable<Double>();
				switch ((int)(m_weatherCondition.m_options & 0x00000003)) {
					case 2:
						m_weatherCondition.dailyFFMC(m_dayStart, prev_ffmc, spec);
						
						WTime prevLoop = new WTime(loop), loopStop = new WTime(loop);
						prevLoop.subtract(WTimeSpan.Hour);
						loopStop.subtract(new WTimeSpan(0, 48, 0, 0));
						if (i == 0)
							m_weatherCondition.hourlyFFMC(prevLoop, prev_hr_ffmc);
						else
							prev_hr_ffmc.value = m_calc_hr[i - 1].dFFMC;

						double[] rain48 = new double[48];
						rain48[0] = precip.value;
						for (short ii = 1; greaterThan(prevLoop, loopStop); prevLoop.subtract(WTimeSpan.Hour), ii++)
							rain48[ii] = m_weatherCondition.getHourlyRain(prevLoop);

						val = Fwi.hourlyFFMCHybrid(prev_ffmc.value, m_calc_day.dFFMC, prev_hr_ffmc.value, rain48, temp.value, rh.value, ws.value,
						    loop.getTimeOfDay(FORMAT_AS_LOCAL).getTotalSeconds());
						break;
					case 3:
						m_weatherCondition.dailyFFMC(m_dayStart, prev_ffmc, spec);

						val = Fwi.hourlyFFMCLawsonContiguous(prev_ffmc.value,
						    m_calc_day.dFFMC, rh.value, rh.value, rh.value, subtract(loop, dayLST).getTotalSeconds());

						break;
					default:
						OutVariable<Double> in_ffmc = new OutVariable<Double>();
						in_ffmc.value = 0.0;
						if (i == 0)	{
							WTime t = new WTime(loop);
							t.subtract(WTimeSpan.Hour);
							m_weatherCondition.hourlyFFMC(t, in_ffmc);
						}
						else
							in_ffmc.value = m_calc_hr[i - 1].dFFMC;

						val = Fwi.hourlyFFMCVanWagner(in_ffmc.value, precip.value, temp.value, rh.value, ws.value, 60 * 60);
						break;
				}
				m_calc_hr[i].dFFMC = val;
			}
		}
	}
	
	private void calculateRemainingFWI() {
		WTime dayNeutral = new WTime(m_dayStart, FORMAT_AS_LOCAL | FORMAT_WITHDST, (short)1);
		WTime dayLST = new WTime(dayNeutral, FORMAT_AS_LOCAL, (short)-1);
		WTime dayNoon = new WTime(dayLST);
		dayNoon.add(new WTimeSpan(0, 12, 0, 0));
		
		double ws = getHourlyWS(dayNoon);
		m_calc_day.dISI = Fwi.isiFBP(m_calc_day.dFFMC, ws, 24 * 60 * 60);
		m_calc_day.dFWI = Fwi.fwi(m_calc_day.dISI, m_calc_day.dBUI);
		
		WTime loop = new WTime(m_dayStart);
		int i;
		
		int start = m_weatherCondition.firstHourOfDay(m_dayStart);
		int end = m_weatherCondition.lastHourOfDay(m_dayStart);
		if (start != 0)
			loop.add(new WTimeSpan(0, start, 0, 0));
		
		for (i = start; i < end; i++, loop.add(WTimeSpan.Hour)) {
			if (((m_weatherCondition.m_options & 0x00000004) != 0) && (m_spec_hr[i].dISI >= 0.0))
				m_calc_hr[i].dISI = m_spec_hr[i].dISI;
			else {
				double dISI;
				ws = getHourlyWS(loop);
				dISI = Fwi.isiFBP(m_calc_hr[i].dFFMC, ws, 60 * 60);
				m_calc_hr[i].dISI = dISI;
			}
			if ((m_weatherCondition.m_options & 0x00000004) != 0 && (m_spec_hr[i].dFWI >= 0.0))
				m_calc_hr[i].dFWI = m_spec_hr[i].dFWI;
			else {
				double dFWI;
				OutVariable<Double> dBUI = new OutVariable<Double>();
				OutVariable<Boolean> specified = new OutVariable<Boolean>();
				m_weatherCondition.bui(loop, dBUI, specified, false);
				dFWI = Fwi.fwi(m_calc_hr[i].dISI, dBUI.value);
				m_calc_hr[i].dFWI = dFWI;
			}
		}
	}
	
	private void readObject(ObjectInputStream inStream) throws IOException {
		short version = inStream.readShort();
		if ((version < 3) || (version > serialVersionUID)) {
			//AfxThrowArchiveException(CArchiveException::badSchema, "CWFGM Weather Stream: Daily Condition");
		}
		if (version >= 14) {
			if ((version != 14) && (version != 15)) {
				//AfxThrowArchiveException(CArchiveException::badSchema, "CWFGM Weather Stream: Daily Conditions");
			}
			if (version == 14) {
				short m_dailySpecified;
				m_dailySpecified = inStream.readShort();
				if ((m_dailySpecified & DAILYCONDITION_SPECIFIED.DAILYFFMC) != 0) {
					m_spec_day.dFFMC = inStream.readFloat();
				}
				if ((m_dailySpecified & DAILYCONDITION_SPECIFIED.DAILYDC) != 0) {
					m_spec_day.dDC = inStream.readFloat();
				}
				if ((m_dailySpecified & DAILYCONDITION_SPECIFIED.DAILYDMC) != 0) {
					m_spec_day.dDMC = inStream.readFloat();
				}
				for (short i = 0; i < 24; i++) {
					byte m_hourlySpecified = inStream.readByte();
					if ((m_hourlySpecified & DAILYCONDITION_SPECIFIED.HOURLYFFMC) != 0) {
						m_spec_hr[i].dFFMC = inStream.readFloat();
					}
					if ((m_hourlySpecified & DAILYCONDITION_SPECIFIED.HOURLYBUI) != 0) {
						m_spec_day.dBUI = inStream.readFloat();
					}
					if ((m_hourlySpecified & DAILYCONDITION_SPECIFIED.HOURLYISI) != 0) {
						m_spec_hr[i].dISI = inStream.readFloat();
					}
					if ((m_hourlySpecified & DAILYCONDITION_SPECIFIED.HOURLYFWI) != 0) {
						m_spec_hr[i].dFWI = inStream.readFloat();
					}
				}
			}
			else {
				m_spec_day.dFFMC = inStream.readDouble();
				m_spec_day.dDMC = inStream.readDouble();
				m_spec_day.dDC = inStream.readDouble();
				m_spec_day.dBUI = inStream.readDouble();
				for (short i = 0; i < 24; i++) {
					m_spec_hr[i].dFFMC = inStream.readDouble();
					m_spec_hr[i].dFWI = inStream.readDouble();
					m_spec_hr[i].dISI = inStream.readDouble();
				}
			}
		}
		else {
			throw new IOException("Version not supported");
		}
	}
	
	private void writeObject(ObjectOutputStream outStream) throws IOException {
		outStream.writeShort((short)serialVersionUID);
		outStream.writeDouble(m_spec_day.dFFMC);
		outStream.writeDouble(m_spec_day.dDMC);
		outStream.writeDouble(m_spec_day.dDC);
		outStream.writeDouble(m_spec_day.dBUI);
		for (short i = 0; i < 24; i++) {
			outStream.writeDouble(m_spec_hr[i].dFFMC);
			outStream.writeDouble(m_spec_hr[i].dFWI);
			outStream.writeDouble(m_spec_hr[i].dISI);
		}
	}
}
