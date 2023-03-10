/**
 * WISE_Weather_Module: DayCondition.h
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

#include "DailyWeather.h"

#include "ISerializeProto.h"
#include "dailyConditions.pb.h"
#include "validation_object.h"

#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(push, 8)
#endif

class DailyCondition : public DailyWeather, public ISerializeProto {
	friend class CWFGM_WeatherStreamHelper;

    public:
	DailyCondition *LN_Succ() const				{ return (DailyCondition *)DailyWeather::LN_Succ(); };
	DailyCondition *LN_Pred() const				{ return (DailyCondition *)DailyWeather::LN_Pred(); };

    protected:
	IFWIData	m_spec_hr[24], m_calc_hr[24];
	DFWIData	m_spec_day, m_calc_day;
	int32_t		m_interpolated;

    public:
	double hourlyFFMC(const WTime &time) const					{ std::int32_t hour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST); return m_calc_hr[hour].FFMC; };
	bool isHourlyFFMCSpecified(const WTime &time) const		{ std::int32_t hour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST); return (m_calc_hr[hour].FFMC >= 0.0) ? true : false; };
	double ISI(const WTime &time) const						{ std::int32_t hour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST); return m_calc_hr[hour].ISI; };
	double FWI(const WTime &time) const						{ std::int32_t hour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST); return m_calc_hr[hour].FWI; };

	void specificHourlyFFMC(const WTime &time, double ffmc)	{ std::int32_t hour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST); m_spec_hr[hour].FFMC = (ffmc >= 0.0) ? ffmc : -1.0; };
	void specificISI(const WTime &time, double isi) 			{ std::int32_t hour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST); m_spec_hr[hour].ISI = (isi >= 0.0) ? isi : -1.0; };
	void specificFWI(const WTime &time, double fwi) 			{ std::int32_t hour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST); m_spec_hr[hour].FWI = (fwi >= 0.0) ? fwi : -1.0; };

	double dailyFFMC() const									{ return m_calc_day.dFFMC; };
	double dailyISI() const									{ return m_calc_day.dISI; };
	double dailyFWI() const									{ return m_calc_day.dFWI; };
	double DC() const											{ return m_calc_day.dDC; };
	double DMC() const											{ return m_calc_day.dDMC; };
	double BUI() const											{ return m_calc_day.dBUI; };
	bool dailyFFMCSpecified() const							{ return (m_spec_day.dFFMC >= 0.0); };
	bool DCSpecified() const									{ return (m_spec_day.dDC >= 0.0); };
	bool DMCSpecified() const									{ return (m_spec_day.dDMC >= 0.0); };
	bool BUISpecified() const									{ return (m_spec_day.dBUI >= 0.0); };

	void specificDailyFFMC(double ffmc) 						{ m_spec_day.dFFMC = (ffmc >= 0.0) ? ffmc : -1.0; };
	void specificDC(double dc) 								{ m_spec_day.dDC = (dc >= 0.0) ? dc : -1.0; };
	void specificDMC(double dmc)								{ m_spec_day.dDMC = (dmc >= 0.0) ? dmc : -1.0; };
	void specificBUI(double bui) 								{ m_spec_day.dBUI = (bui >= 0.0) ? bui : -1.0; };

	void setHourInterpolated(std::int32_t hour)				{ m_interpolated |= (1 << hour); }
	void clearHourInterpolated(std::int32_t hour)				{ m_interpolated &= ~(1 << hour); }
	bool isHourIterpolated(std::int32_t hour) const			{ return 0x1 & (m_interpolated >> hour); }
	bool isTimeInterpolated(WTime const &time) const			{ return isHourIterpolated(time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)); }

	void clearHourlyData(std::int32_t hour)					{ m_spec_hr[hour].FFMC = m_spec_hr[hour].FWI = m_spec_hr[hour].ISI = -1.0; }
	void clearHourlyData(const WTime &time)					{ std::int32_t hour = time.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST); clearHourlyData(hour); }
	void clearDailyData()										{ m_spec_day.dFFMC = m_spec_day.dDC = m_spec_day.dDMC = m_spec_day.dBUI = m_spec_day.dISI = m_spec_day.dFWI = -1.0; }

    public:
	DailyCondition(WeatherCondition *wc);
	DailyCondition(const DailyCondition &toCOpy, WeatherCondition *wc);

	bool	calculateFWI();
	bool	AnyFWICodesSpecified();

    private:
	void calculateDC();
	void calculateDMC();
	void calculateBUI();
	void calculateDailyFFMC();
	void calculateHourlyFFMC();
	void calculateRemainingFWI();

public:
	virtual std::int32_t serialVersionUid(const SerializeProtoOptions& options) const noexcept override;
	virtual WISE::WeatherProto::DailyConditions* serialize(const SerializeProtoOptions& options) override;
	virtual DailyCondition* deserialize(const google::protobuf::Message& proto, std::shared_ptr<validation::validation_object> valid, const std::string& name) override;
	virtual DailyCondition *deserialize(const google::protobuf::Message& proto, std::shared_ptr<validation::validation_object> valid, const std::string& name, std::uint16_t firstHour, std::uint16_t lastHour);
	virtual std::optional<bool> isdirty(void) const noexcept override { return std::nullopt; }
};

#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(pop)
#endif
