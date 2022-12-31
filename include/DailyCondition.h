/**
 * WISE_Weather_Module: DailyCondition.h
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

#ifndef __DAILY_CONDITION_H
#define __DAILY_CONDITION_H

#include "WTime.h"
#include "fwicom.h"
#include <atlbase.h>
#include <math.h>
#include "angles.h"
#include "linklist.h"

#define DAILY_CONDITION_FROM_MANUAL	0
#define DAILY_CONDITION_FROM_HOURLY_FILE	1
#define DAILY_CONDITION_FROM_DAILY_FILE		2

class DailyCondition : public MinNode 
{
public:
	BOOL m_bBurnConditionEffective;
	double m_dblExtinguishMinRH;
	double m_dblExtinguishMaxWS;
	int m_iBurnEndHour;
	int m_iBurnStartHour;
	    BOOL m_bDCSpecified;
	    BOOL m_bDMCSpecified;
	    BOOL m_bFFMCSpecified;
	inline DailyCondition *LN_Succ() const			{ return (DailyCondition *)MinNode::LN_Succ(); };
	inline DailyCondition *LN_Pred() const			{ return (DailyCondition *)MinNode::LN_Pred(); };

	WeatherCondition *m_weatherCondition;			// pointer to its owner, so we can ask for values for a different time, which
								// may be in a different day
	BOOL	m_isHourlySpecified;				// if FALSE then daily conditions are specified and hourly conditions are
								// calculated, if TRUE then hourly conditions are specified and daily
								// conditions aren't used
protected:
	int m_iDaylightSaving;
	double	m_dailyMinTemp,
		m_dailyMaxTemp,
		m_initialTemp,
		m_dailyMinWS,
		m_dailyMaxWS,
		m_initialWS,
		m_dailyRH,
		m_dailyPrecip;
	double	m_dailyWD;					// daily conditions for us to work with

    Time	m_Sunrise,					// variables used in calculateHourlyConditions()
		m_SolarNoon,
		m_InitialTempTime,
		m_InitialWSTime,
		m_Sunset;
    double	m_SunsetTemp;

	double	m_calc_gamma;					// temporary variables for hourly calculations
    Time	m_calc_tn,
		m_calc_tx,
		m_calc_tu,
		m_calc_ts;
    double	m_calc_min,
		m_calc_max,
		m_calc_user,
		m_calc_sunset;

	double	m_hourlyTemp[24],
		m_hourlyRH[24],
		m_hourlyWS[24],
		m_hourlyPrecip[24];
	double	m_hourlyWD[24];					// hourly conditions that may be given or calculated from daily values

	double	m_FFMC[24],
		m_DC,
		m_DMC;						// FWI values that are calculated

	double	m_specifiedFFMC[24];				// specified FFMC values from a file
	UCHAR	m_specifiedFFMC_valid[24];			// which data is valid


	double	m_specifiedBUI[24];				// specified BUI values from a file
	BOOL	m_specifiedBUI_valid[24];			// which data is valid
	double	m_specifiedFWI[24];				// specified FWI values from a file
	BOOL	m_specifiedFWI_valid[24];			// which data is valid
	double	m_specifiedISI[24];				// specified ISI values from a file
	BOOL	m_specifiedISI_valid[24];			// which data is valid

	// calculate sin part of diurnal weather trend as per Beck & Trevitt
	inline double sin_function(Time t) 	{ return m_calc_min+(m_calc_max-m_calc_min)*sin((t-m_calc_tn)/(m_calc_tx-m_calc_tn)*HALF_PI); }
	double sin_RiseUser(Time t);
	double SinFuncBetween(Time t, Time start, double startVal, Time end, double endVal);
	inline double sin_UserSet(Time t) 		{ return m_calc_user+(m_calc_max-m_calc_user)*sin((m_calc_tx-t)/(m_calc_tx-m_calc_tu)*HALF_PI); }

	// calculate exp part of diurnal weather trend as per Beck & Trevitt
	inline double exp_function(Time t)		{ return m_calc_min+(m_calc_sunset-m_calc_min)*exp((t-m_calc_ts)/(m_calc_tn-m_calc_ts)*m_calc_gamma); }
	double ExpFuncBetween(Time t, Time start, double startVal, Time end, double endVal);
	inline double exp_WindFunc(Time t)
	{
		double Ws;
		Ws=sin_function(t);
		return m_calc_min+(Ws-m_calc_min)*exp((t-m_calc_ts)/(m_calc_tn-m_calc_ts)*m_calc_gamma); 
	}
	inline double exp_SetUser(Time t)		{ return m_calc_user+(m_calc_sunset-m_calc_user)*exp((t-m_calc_ts)/(m_calc_tu-m_calc_ts)*m_calc_gamma); }
	inline double exp_UserRise(Time t)		{ return m_calc_min+(m_calc_user-m_calc_min)*exp((t-m_calc_tu)/(m_calc_tn-m_calc_tu)*m_calc_gamma); }
	double QT0(double vpt0, double MaxTemp);

public:
	BOOL IsDCSpecified();
	BOOL IsDMCSpecified();
	BOOL IsHourlyFFMCSpecified();
	double SpecifiedFWI(const Time &time);
	double SpecifiedISI(const Time &time);
	double SpecifiedBUI(const Time &time);
	BOOL SpecifyFWI(const Time &time, double fwi);
	BOOL SpecifyISI(const Time &time, double isi);
	BOOL SpecifyBUI(const Time &time,double bui);
	double DailyStandardFFMC(long time);
	double m_dblDailyStandardFFMC;
	void ClearUserFFMC(const Time & time);
	void GetDayBurnCondition(BOOL *bEffective, int *StartTime, int *EndTime, double *MaxWS, double *MinRH);
	void SetBurnCondition(BOOL bEffective, int StartTime, int EndTime, double MaxWS, double MinRH);
	BOOL CanBurn(const Time &time, int bEffective, int StartHour, int EndHour, double MaxWS, double MinRH);
	int m_iHourlySet[24];
	void SetDailyConditionSource(int iSource);
	int m_iDataSource;
	BOOL IsDaylightSaving();
	void SetDaylightSaving(BOOL IsDaylightSaving);
	void SetDMC(double dmc);
	void SetDC(double dc);
	BOOL UseWS();
	BOOL UseTemp();
	void SetApplyWSFlag(BOOL value);
	void SetApplyTempFlag(BOOL value);
	double m_dblWSDiff;
	double m_dblTempDiff;
	bool m_bAdjustWS;
	bool m_bAdjustTemp;
	void SetInitialTemp(double temp);
	void SetInitialTempTime(TimeSpan &TempTime);
	void SetInitialWS(double temp);
	void SetInitialWSTime(TimeSpan &TempTime);
	bool IsFirstDay();
	void SetFirstDayFlag(bool bSetAs=true);
	DailyCondition(WeatherCondition *wc);
				// ***** input/output...
	friend CArchive& operator>>(CArchive& is, DailyCondition &dc);
	friend CArchive& operator<<(CArchive& os, const DailyCondition &dc);

	BOOL	calculateHourlyConditions();
	BOOL	calculateFWI();

	inline BOOL IsDailyUsed() const				{ return !m_isHourlySpecified; };
	inline void MakeHourlyObservations()		
	{ 
		m_weatherCondition->m_isCalculatedValuesValid=FALSE;
		m_weatherCondition->calculateValues(); 
		m_isHourlySpecified = TRUE; 
	};
	void MakeDailyObservations();
								// functions to retrieve data or set data
	double DailyMinTemp() const;
	double DailyMaxTemp() const;
	double DailyMinWS() const;
	double DailyMaxWS() const;
	double DailyRH() const;
	double DailyMinRH() const;
	double DailyMaxRH() const;
	double DailyPrecip() const;
	double DailyWD() const;

	BOOL DailyMinTemp(double temp);
	BOOL DailyMaxTemp(double temp);
	BOOL DailyMinWS(double ws);
	BOOL DailyMaxWS(double ws);
	BOOL DailyRH(double rh);
	BOOL DailyPrecip(double p);
	BOOL DailyWD(double wd);

	inline double Temperature(const Time &time)		{ return m_hourlyTemp[time.GetLocalHour()]; };
	inline double RH(const Time &time)			{ return m_hourlyRH[time.GetLocalHour()]; };
	inline double WS(const Time &time)			{ return m_hourlyWS[time.GetLocalHour()]; };
	inline double Precip(const Time &time)			{ return m_hourlyPrecip[time.GetLocalHour()]; };
	inline double WD(const Time &time)			{ return m_hourlyWD[time.GetLocalHour()]; };
	inline int Status(const Time &time)			{ return m_iHourlySet[time.GetLocalHour()]; };
	inline void SetStatus(const Time &time, int val)			
	{ m_iHourlySet[time.GetLocalHour()]=val==0?0:1; };

	double Temperature(const Time &time, double temp);
	double RH(const Time &time, double rh);
	double WS(const Time &time, double ws);
	double Precip(const Time &time, double p);
	double WD(const Time &time, double wd);

	void SpecifiedFFMC(const Time &time, double ffmc);

	double FFMC(const Time &time);
	inline double FFMC(long timeHour)			{ return m_FFMC[timeHour]; };
	inline double DC()					{ return m_DC; };
	inline double DMC()					{ return m_DMC; };

	inline void ClearUserFFMC()				{ for (int i = 0; i < 24; i++) m_specifiedFFMC_valid[i] = 0; m_bFFMCSpecified=FALSE;};
	void ClearUserFWI(const Time &t);
	void ClearUserISI(const Time &t);
	void ClearUserBUI(const Time &t);
private:
	void CheckAndAdjustPeakTime();
	void CheckAndAdjustSunriseSunset();
	void ClearUserDC();
	double GetPreviousActualStandardFFMC();
	void CalcFirstDayLawsonFFMC();
	double GetMorningRH();
	double CollectRainFrom13To13();
	double GetTodayStandardFFMC();
	double GetPreviousStandardFFMC();
	double GetPreviousDayFFMC(int hour);
	double GetHourlyFFMCNext(int method, int i, double in_ffmc, double precip, double temp, double rh, double ws);
	double GetHourlyFFMCPrevious(int method, int i, double in_ffmc, double precip, double temp, double rh, double ws);
	void CalcBackwardFFMC(int j);
	void CalcForwardFFMC(int j);
	BOOL ValuesAreOriginal();
	BOOL m_bUseWS;
	BOOL m_bUseTemp;
	void CalcYesterdayWS(DailyCondition *yesterday, Time &time);
	void CalcTodayWS( Time &time, int *lastWS);
	double SVPT0(double MaxTemp);
	void FillSunsetToMidnight(int lastTemp, int lastWS);
	void CalcFirstDayTemp(Time &time, int *lastTemp);
	void CalcHourlyTemp(Time &time, int *lastTemp);
	bool m_bIsFirstDay;
	void CalcHourlyWS(DailyCondition *yesterday, int *lastWS, Time &time);
	void CalcYesterdayTemp(DailyCondition *yesterday,Time *time);
	void CalcHourlyTempAndRH(DailyCondition *yesterday,int *lastTemp,Time &time);
	void CalcHourlyFFMC(Time &time,double average_temp);
};

#endif
