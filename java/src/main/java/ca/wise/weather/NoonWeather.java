/***********************************************************************
 * REDapp - NoonWeather.java
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

import static ca.hss.times.WTime.FORMAT_AS_LOCAL;
import static ca.hss.times.WTime.FORMAT_WITHDST;
import static ca.hss.times.WTime.subtract;
import static ca.hss.times.WTimeSpan.greaterThanEqualTo;
import static java.lang.Math.log10;
import static java.lang.Math.pow;

import ca.wise.fwi.Fwi;
import ca.hss.general.OutVariable;
import ca.hss.math.LinkedListNode;
import ca.hss.times.WTime;
import ca.hss.times.WTimeSpan;

/**
 * Contains information about a days noon weather.
 */
public class NoonWeather extends LinkedListNode {
	/**
	 * The temperature at noon (decC).
	 */
	public double m_temperature;
	/**
	 * The calculated dew point temperature (decC);
	 */
	public double m_dewpoint;
	/**
	 * The relative humidity at noon (%).
	 */
	public double m_rh;
	/**
	 * The 24 hour cumulative precipitation at noon (mm).
	 */
	public double m_precip;
	/**
	 * The wind speed at noon (km/h).
	 */
	public double m_ws;
	/**
	 * The wind gust at noon (km/h).
	 */
	public double m_wg;
	/**
	 * The wind direction at noon (radians).
	 */
	public double m_wd;
	/**
	 * The time the weather was recorded at.
	 */
	public WTime m_time;
	public NoonWeatherCondition m_condition;
	public WTime m_dayStart,
	   m_SunRise,
	   m_SolarNoon,
	   m_Sunset;
	
	/**
	 * The computed drought code.
	 */
	private double m_dc;
	/**
	 * The computed duff moisture code.
	 */
	private double m_dmc;
	/**
	 * The computed fine fuel moisture code.
	 */
	private double m_ffmc;
	/**
	 * The computed buildup index.
	 */
	private double m_bui;
	/**
	 * The computed initial spread index.
	 */
	private double m_isi;
	/**
	 * The computed fire weather index.
	 */
	private double m_fwi;
	
	public double getDailyFFMC() 					{ return m_ffmc; }
	public double getDC() 						{ return m_dc; }
	public double getDMC() 						{ return m_dmc; }
	public double getBUI() 						{ return m_bui; }
	public double getISI()						{ return m_isi; }
	public double getFWI()						{ return m_fwi; }

	public NoonWeather getYesterday() {
		NoonWeather dw = getPrevious();
		if (dw.getPrevious() != null)
			return dw;
		return null;
	}

	public NoonWeather getTomorrow() {
		NoonWeather dw = getNext();
		if (dw.getNext() != null)
			return dw;
		return null;
	}
	
	public NoonWeather() {
		super();
	}
	
	public NoonWeather(NoonWeatherCondition parent) {
		this();
		
		m_condition = parent;
		m_dayStart = new WTime(0, m_condition.m_timeManager);
		m_SunRise = new WTime(0, m_condition.m_timeManager);
		m_Sunset = new WTime(0, m_condition.m_timeManager);
		m_SolarNoon = new WTime(0, m_condition.m_timeManager);
	}
	
	public void setNoonWeather(double temp, double rh, double ws, double wg, double wd, double precip) {
		this.m_temperature = temp;
		this.m_rh = rh;
		this.m_ws = ws;
		this.m_wg = wg;
		this.m_wd = wd;
		this.m_precip = precip;
	}
	
	public void getNoonWeather(OutVariable<Double> temp, OutVariable<Double> dew, OutVariable<Double> rh, OutVariable<Double> ws, OutVariable<Double> wg, OutVariable<Double> wd, OutVariable<Double> precip) {
		temp.value = m_temperature;
		dew.value = m_dewpoint;
		rh.value = m_rh;
		ws.value = m_ws;
		wg.value = m_wg;
		wd.value = m_wd;
		precip.value = m_precip;
	}
	
	public void getNoonFWI(OutVariable<Double> dc, OutVariable<Double> dmc, OutVariable<Double> ffmc, OutVariable<Double> bui,
			OutVariable<Double> isi, OutVariable<Double> fwi) {
		dc.value = m_dc;
		dmc.value = m_dmc;
		ffmc.value = m_ffmc;
		bui.value = m_bui;
		isi.value = m_isi;
		fwi.value = m_fwi;
	}
	
	@Override
	public NoonWeather getNext() { return (NoonWeather)super.getNext(); }
	
	@Override
	public NoonWeather getPrevious() { return (NoonWeather)super.getPrevious(); }

	public boolean calculateTimes(int index) {
		m_dayStart = new WTime(m_condition.m_time);
		m_dayStart.add(new WTimeSpan(index, 0, 0, 0));

		WTime t = new WTime(m_dayStart);
		t.add(new WTimeSpan(0, 12, 0, 0));

		OutVariable<WTime> rise = new OutVariable<WTime>();
		rise.value = m_SunRise;
		OutVariable<WTime> set = new OutVariable<WTime>();
		set.value = m_Sunset;
		OutVariable<WTime> noon = new OutVariable<WTime>();
		noon.value = m_SolarNoon;
		m_condition.m_worldLocation.getSunRiseSetNoon(t, rise, set, noon);
		m_SunRise = new WTime(rise.value);
		m_Sunset = new WTime(set.value);
		m_SolarNoon = new WTime(noon.value);
		if (greaterThanEqualTo(subtract(m_Sunset, m_dayStart), WTimeSpan.Day))
			return false;
		return true;
	}
	
	public void calculateFWI() {
		calculateDewPtTemp();
		calculateDC();
		calculateDMC();
		calculateBUI();
		calculateDailyFFMC();
		calculateRemainingFWI();
	}

	private void calculateDewPtTemp() {
		double VPs = 0.6112 * pow(10.0, 7.5 * m_temperature / (237.7 + m_temperature));
		double VP = m_rh * VPs;
		if (VP > 0.0)
			m_dewpoint = 237.7 * log10(VP / 0.6112) / (7.5 - log10(VP / 0.6112));
		else	m_dewpoint = -273.0;
	}
	
	private void calculateDC() {
		OutVariable<Double> in_dc = new OutVariable<Double>();
		m_condition.dc(m_dayStart, in_dc);

		m_dc = Fwi.dC(in_dc.value, m_precip, m_temperature,
				m_condition.getTimeManager().getWorldLocation().getLatitude(), m_condition.getTimeManager().getWorldLocation().getLongitude(),
		    (int)m_dayStart.getMonth(FORMAT_AS_LOCAL | FORMAT_WITHDST) - 1);
	}
	
	private void calculateDMC() {
		OutVariable<Double> in_dmc = new OutVariable<Double>();
		m_condition.dmc(m_dayStart, in_dmc);

		m_dmc = Fwi.dMC(in_dmc.value, m_precip, m_temperature,
				m_condition.getTimeManager().getWorldLocation().getLatitude(), m_condition.getTimeManager().getWorldLocation().getLongitude(),
		    (int)m_dayStart.getMonth(FORMAT_AS_LOCAL | FORMAT_WITHDST) - 1, m_rh);
	}
	
	private void calculateBUI() {
		m_bui = Fwi.bui(m_dc, m_dmc);
	}
	
	private void calculateDailyFFMC() {
		OutVariable<Double> in_ffmc = new OutVariable<Double>();
		m_condition.dailyFFMC(m_dayStart, in_ffmc);

		m_ffmc = Fwi.dailyFFMCVanWagner(in_ffmc.value, m_precip, m_temperature, m_rh, m_ws);
	}
	
	private void calculateRemainingFWI() {
		
		m_isi = Fwi.isiFWI(m_ffmc, m_ws, 24 * 60 * 60);
		m_fwi = Fwi.fwi(m_isi, m_bui);
	}
}
