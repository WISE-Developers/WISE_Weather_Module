/**
 * WISE_Weather_Module: WeatherStream.h
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

#include "FwiCom.h"
#include "WTime.h"
#if __has_include(<mathimf.h>)
#include <mathimf.h>
#else
#include <cmath>
#endif
#include "angles.h"
#include "linklist.h"
#include "CWFGM_WeatherStation.h"
#include "ISerializeProto.h"
#include "weatherStream.pb.h"
#include "hssconfig/config.h"

using namespace HSS_Time;

#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(push, 8)
#endif

class WeatherCondition : public ISerializeProto {
    friend class DailyWeather;
    friend class DailyCondition;
	friend class CWFGM_WeatherStreamHelper;

public:
	static constexpr std::uint32_t USER_SPECIFIED = 0b100;
	static constexpr std::uint32_t FFMC_VAN_WAGNER = 0x00000001;
	static constexpr std::uint32_t FFMC_LAWSON = 0x00000003;
	static constexpr std::uint32_t FFMC_MASK = 0x00000003;
	static constexpr std::uint32_t FROM_FILE = 0x00000020;
	static constexpr std::uint32_t FROM_ENSEMBLE = 0x00000040;

public:
	WTimeManager		m_timeManager;
	WorldLocation		m_worldLocation;
	WTime				m_time;				// start time of this weather stream, in GMT defined by lat, long, timezone in

	WTimeSpan			m_initialHFFMCTime;		// if this is (-1), then m_initialHFFMC is treated as not set

	std::uint32_t		m_options;					// different settings, like which FFMC calculation to use

	double	m_initialRain;
	DFWIData m_spec_day;
	double	m_initialHFFMC;		// use m_spec_day.dBUI instead of m_initialBUI

	double	m_temp_alpha, m_temp_beta, m_temp_gamma,
			m_wind_alpha, m_wind_beta, m_wind_gamma;	// Judy Beck's values for calculating hourly values from daily observations/predictions
	
	///
	/// <summary>The last hour of the last day of the stream.</summary>
	///
	uint8_t m_lastHour;
	///
	/// <summray>The first hour of the first day of the stream.</summary>
	///
	uint8_t m_firstHour;

	boost::intrusive_ptr<CCWFGM_WeatherStation> m_weatherStation;

private:
	bool	m_isCalculatedValuesValid;			// if each day's calculated values (hourly observations, FWI values, etc.) are valid, or
								// if they have to be recalculated

protected:
	ICWFGM_FWI		*m_fwi;

	MinListTempl<class DailyCondition>	m_readings;					// each day of data

	class DailyCondition *getDCReading(const WTime &time, bool add);
								// method to retrieve a day given a time, it may add a new day if given the option

public:
	WeatherCondition();
	WeatherCondition(const WeatherCondition &toCopy);
	virtual ~WeatherCondition();

	WeatherCondition &operator=(const WeatherCondition &toCopy);

	bool GetDailyWeatherValues(const WTime &time, double *min_temp, double *max_temp, double *min_ws, double *max_ws, double* min_gust, double* max_gust, double *rh, double *precip, double *wd);
	bool SetDailyWeatherValues(const WTime &time, double min_temp, double max_temp, double min_ws, double max_ws, double min_gust, double max_gust, double rh, double precip, double wd);
								// values to get/set if it's a "daily observation mode"
	double GetHourlyRain(const WTime &time);
	bool SetHourlyWeatherValues(const WTime &time, double temp, double rh, double precip, double ws, double gust, double wd, double dew);
	bool SetHourlyWeatherValues(const WTime &time, double temp, double rh, double precip, double ws, double gust, double wd, double dew, bool interp);
	bool SetHourlyWeatherValues(const WTime& time, double temp, double rh, double precip, double ws, double gust, double wd, double dew, bool interp, bool ensemble);
	// values to get/set if it's an "hourly observation mode"
	bool MakeHourlyObservations(const WTime &time);
	bool MakeDailyObservations(const WTime &time);
	std::uint16_t IsHourlyObservations(const WTime &time);
	HRESULT IsAnyDailyObservations() const;
	HRESULT IsAnyModified() const;
	std::uint16_t IsOriginFile(const WTime &time);
	std::uint16_t IsOriginEnsemble(const WTime& time);
	std::uint16_t IsModified(const WTime& time);

	HRESULT SetValidTimeRange(const HSS_Time::WTime& start, const HSS_Time::WTimeSpan& duration, const bool correctInitialPrecip);

	std::int16_t WarnOnSunRiseSet();

	///
	/// <summary>Get the first hour of the given day that the stream has data for.</summary>
	///
	uint8_t firstHourOfDay(const WTime& time);
	///
	/// <summary>Get the last hour of the given day that the stream has data for.</summary>
	///
	uint8_t lastHourOfDay(const WTime& time);

	bool GetInstantaneousValues(const WTime &time, std::uint32_t method, IWXData *wx, IFWIData *ifwi, DFWIData *dfwi);

	bool HourlyFFMC(const WTime &time, double *ffmc);
	bool DailyFFMC(const WTime &time, double *ffmc, bool *specified);
	bool DC(const WTime &time, double *dc, bool *specified);
	bool DMC(const WTime &time, double *dmc, bool *specified);
	bool BUI(const WTime &time, double *bui, bool *specified, bool recalculate = true);
	bool HourlyISI(const WTime &time, double *isi);
	bool DailyISI(const WTime &time, double *isi);
	bool HourlyFWI(const WTime &time, double *fwi);
	bool DailyFWI(const WTime &time, double *fwi);

	bool AnyFWICodesSpecified();		// returns whether there are any FWI codes (daily or hourly) that have been specified by the user (e.g. during file load) or not
	void ClearConditions();
	void ClearWeatherData();
	bool CumulativePrecip(const WTime &time, const WTimeSpan &duration, double *rain);

	void GetEventTime(std::uint32_t flags, const WTime &from_time, WTime &next_event);

	HRESULT Import(const TCHAR *fileName, std::uint16_t options, std::shared_ptr<validation::validation_object> valid);
	void SetEndTime(WTime &endTime);
	void GetEndTime(WTime &EndTime);

	__INLINE std::uint32_t NumDays() const				{ return m_readings.GetCount(); };

	void calculateValues();

private:
	void DistributeDailyValue(std::vector<std::string> &header, int index, double value, double *min_temp, double *max_temp, double *rh, double *precip, double *min_ws, double *max_ws, double *min_gust, double *max_gust, double *wd);
	void FillDailyLineValue(std::vector<std::string> &header, char *line, char *file_type,double *min_temp, double *max_temp, double *rh, double *precip, double *min_ws, double *max_ws, double *min_gust, double *max_gust, double *wd);
	int GetWord(std::string *source, std::string *strWord);
	void ProcessHeader(TCHAR *line, std::vector<std::string> &header);
	void CopyDailyCondition(WTime &source, WTime &dest);
	void DecreaseConditions(WTime &currentEndTime, std::uint32_t days);
	void IncreaseConditions(WTime &currentEndTime, std::uint32_t days);
	bool isSupportedFormat(char *line, std::vector<std::string> &header);

public:
	virtual std::int32_t serialVersionUid(const SerializeProtoOptions& options) const noexcept override;
	virtual WISE::WeatherProto::WeatherStream* serialize(const SerializeProtoOptions& options) override;
	virtual WeatherCondition *deserialize(const google::protobuf::Message& proto, std::shared_ptr<validation::validation_object> valid, const std::string& name) override;
	virtual std::optional<bool> isdirty(void) const noexcept override { return std::nullopt; }
};

#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(pop)
#endif
