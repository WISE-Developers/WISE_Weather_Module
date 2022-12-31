/**
 * WISE_Weather_Module: WeatherCondition.h
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

#ifndef WEATHER_CONDITION_H
#define WEATHER_CONDITION_H


#include "WTime.h"
#include "FwiCom.h"
#ifdef _MSC_VER
#include <atlbase.h>
#endif
#include <cmath>
#include "angles.h"
#include "linklist.h"

class WeatherCondition : public CObject 
{
    friend class DailyCondition;
    public:
	TimeManager		m_timeManager;
	WorldLocation		m_worldLocation;
	Time	m_time;						// start time of this weather stream, in GMT defined by lat, long, timezone in

	TimeSpan m_initialFFMCTime;
	TimeSpan m_initialTempTime;
	TimeSpan m_initialWSTime;

	ULONG	m_options;					// different settings, like which FFMC calculation to use

	double	m_initialTemp, m_initialWS, m_initialFFMC, 
		m_initialDC, m_initialDMC;	// initial values that are needed to kick-start values
	double	m_temp_alpha, m_temp_beta, m_temp_gamma,
		m_wind_alpha, m_wind_beta, m_wind_gamma;	// Judy Beck's values for calculating hourly values from daily observations/predictions

	BOOL	m_isCalculatedValuesValid;			// if each day's calculated values (hourly observations, FWI values, etc.) are valid, or
								// if they have to be recalculated

    protected:
	CComPtr<ICWFGM_FWI>	m_fwi;				// our "FWI" module

	MinList	m_readings;					// each day of data

	class DailyCondition *getReading(const Time &time, BOOL add = FALSE);
								// method to retrieve a day given a time, it may add a new day if given the option
public:
	double m_dblStationLongitude;
	double m_dblStationLatitude;
	void ClearUserFWI(const Time &t);
	void ClearUserISI(const Time &t);
	void ClearUserBUI(const Time &t);
	BOOL IsDCSpecified();
	BOOL IsDMCSpecified();
	BOOL IsHourlyFFMCSpecified();
	BOOL DailyStandardFFMC(long time, double *ffmc);
	void InitialRain(double rainValue);
	void GetDayBurnCondition(const Time &time, BOOL *bEffective, int *StartTime, int *EndTime, double *MaxWS, double *MinRH);
	void SetDayBurnCondition(const Time &time,BOOL bEffective, int StartTime, int EndTime, double MaxWS, double MinRH);
	void SetBurnCondition(BOOL bEffective, int StartTime, int EndTime, double MaxWS, double MinRH);
	BOOL CanBurn(const Time &time, int bEffective, int StartHour, int EndHour, double MaxWS, double MinRH);
	double ExtinguishMinRH();
	double ExtinguishMaxWS();
	int BurnEndHour();
	int BurnStartHour();
	BOOL BurnConditionEffective();
	CStringArray m_arHeader;
	int DailyConditionSource(const Time &time);
	BOOL m_bDaylightSaving;
	void SetDaylightSaving(int iDaylightSaving);
	BOOL ImportHourly(const char *fileName);
	BOOL ImportDaily(const char *fileName);
	double FWI(long timeVal, double bui, double isi);
	double ISI(long timeVal, double FFMC, double ws);
	int m_iUseWS;
	int m_iUseTemp;
	int m_iDiurnalMethod;
	void SetEndTime(Time &endTime);
	double InitialDC();
	double InitialDMC();
	double InitialFFMC();
	void GetEndTime(Time &EndTime);
	Time m_EndTime;
	double BUI(long timeVal, double dmc, double dc);
	WeatherCondition();
	~WeatherCondition();

	void calculateValues();

	BOOL Import(CString fname);
	virtual void Serialize(CArchive& ar);

	inline int NumDays() const				{ return m_readings.GetCount(); };
	inline int IndexOfDay(const class DailyCondition *dc){ return m_readings.NodeIndex((MinNode *)dc); }

	BOOL SetDailyValues( DailyCondition *dc, double min_temp, double max_temp, double min_ws, double max_ws, double rh, double precip, double wd); 
	BOOL GetDailyValues(const Time &time, double *min_temp, double *max_temp, double *min_ws, double *max_ws, double *rh, double *precip, double *wd);
	BOOL SetDailyValues(const Time &time, double min_temp, double max_temp, double min_ws, double max_ws, double rh, double precip, double wd);
								// values to get/set if it's a "daily observation mode"
	BOOL GetHourlyValues(const Time &time, double *temp, double *rh, double *precip, double *ws, double *wd);
	BOOL SetHourlyValues(const Time &time, double temp, double rh, double precip, double ws, double wd);
	BOOL SetHourlyValues(const Time &time, double temp, double rh, double precip, double ws, double wd, double ffmc);
								// values to get/set if it's an "hourly observation mode"
	BOOL MakeHourlyObservations(const Time &time);
	BOOL MakeDailyObservations(const Time &time);

	BOOL GetSimulationValues(const Time &time, double *ffmc, double *dc, double *dmc, double *ws, double *wd);
								// values for the fire simulation
	BOOL FFMC(const Time &time, double *ffmc);
	BOOL Status(const Time &time, int *val);
	BOOL SetStatus(const Time &time, int val);
	BOOL SetStatus(long time, int val);
	BOOL FFMC(long time, double *ffmc);
	BOOL Status(long time, int *stat);
	BOOL DC(const Time &time, double *dc);
	BOOL DC(long timeValue, double *dc);
	BOOL DMC(const Time &time, double *dmc);
	BOOL DMC(long timeValue, double *dmc);

	inline void ClearUserFFMC();
	void ClearUserFFMC(const Time &time);
	
	USHORT IsDailyUsed(const Time &time);
	double m_dblInitialRain;
private:
	BOOL SetHourlyFWI(const Time &time, double fwi);
	BOOL SetHourlyISI(const Time &time, double isi);
	BOOL SetHourlyBUI(const Time &time,double bui);
	void SetAllDailyBurnCondition();
	BOOL ValuesAreOriginal();
	BOOL m_bBurnConditionEffective;
	double m_dblExtinguishMinRH;
	double m_dblExtinguishMaxWS;
	int m_iBurnEndHour;
	int m_iBurnStartHour;
	void DistributeDailyValue(int index, double value, double *min_temp, double *max_temp, double *rh, double *precip, double *min_ws, double *max_ws, double *wd);
	void FillDailyLineValue(char *line, char *file_type,double *min_temp, double *max_temp, double *rh, double *precip, double *min_ws, double *max_ws, double *wd);
	void DistributeValue(int index, double value, int *hour, double *temp, double *rh, double *wd, double *ws, double *precip, double *ffmc, double *dmc, double *dc, double *bui, double *isi, double *fwi);
	void FillLineValues(char *line, char *file_type,int *hour, double *temp, double *rh, double *wd, double *ws, double *precip, double *ffmc, double *dmc, double *dc, double *bui, double *isi, double *fwi);
	int GetWord(CString *source, CString *strWord);
	void ProcessHeader(char *line);
	void CopyDailyCondition(Time &source, Time &dest);
	void DecreaseConditions(int days);
	void IncreaseConditions(int days);
};


#endif
