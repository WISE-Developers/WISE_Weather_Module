/**
 * WISE_Weather_Module: DayCondition.cpp
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

#include "WeatherStream.h"
#include "DayCondition.h"
#include "WeatherCom_ext.h"
#include <fstream>
#include "str_printf.h"
#include "doubleBuilder.h"
#include "macros.h"

#define DAILYCONDITION_BINARY_VERSION		16		// substantial restructuring, change from float to double, and now load/save dBUI

#ifdef DEBUG
#include <assert.h>
#endif


DailyCondition::DailyCondition(WeatherCondition *wc) : DailyWeather(wc), m_interpolated(0) {
	m_spec_day.dBUI = m_spec_day.dDC = m_spec_day.dDMC = m_spec_day.dFFMC = m_spec_day.dISI = m_spec_day.dFWI = -1.0;
	m_spec_day.SpecifiedBits = 0;

	m_calc_day.dBUI = m_calc_day.dDC = m_calc_day.dDMC = m_calc_day.dFFMC = m_spec_day.dISI = m_spec_day.dFWI = -1.0;
	m_calc_day.SpecifiedBits = 0;

	for (std::uint16_t i = 0; i < 24; i++) {
		m_spec_hr[i].FFMC = m_spec_hr[i].FWI = m_spec_hr[i].ISI = -1.0;
		m_spec_hr[i].SpecifiedBits = 0;

		m_calc_hr[i].FFMC = m_calc_hr[i].FWI = m_calc_hr[i].ISI = -1.0;
		m_calc_hr[i].SpecifiedBits = 0;
	}
}


DailyCondition::DailyCondition(const DailyCondition &toCopy, WeatherCondition *wc) : DailyWeather(toCopy, wc) {
	m_interpolated = toCopy.m_interpolated;
	m_spec_day = toCopy.m_spec_day;
	m_calc_day = toCopy.m_calc_day;
	for (uint16_t i = 0; i < 24; i++) {
		m_spec_hr[i] = toCopy.m_spec_hr[i];
		m_calc_hr[i] = toCopy.m_calc_hr[i];
	}
}


void DailyCondition::calculateDC() {
	if ((m_weatherCondition->m_options & WeatherCondition::USER_SPECIFIED) && (m_spec_day.dDC >= 0.0))
		m_calc_day.dDC = m_spec_day.dDC;
	else {
		HRESULT hr;
		double in_dc;
		const WTime dayNeutral(m_DayStart, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, 1);	// convert to a timezone-neutral time
		const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL, -1);				// gets us the start of the true LST day
		WTime dayNoon(dayLST);
		dayNoon += WTimeSpan(0, 12, 0, 0);

// From Beck et. al: the standard daily FFMC is computed from ... temperature, relative humidity, 10-m open wind speed, and 24-hr accumulated rainfall
// taken at solar noon or 1300 DST
// we don't have values at solar noon, but we do at noon LST which should be close enough by the sounds of it

		double rain = dailyPrecip(), val;

		bool spec;
		m_weatherCondition->DC(m_DayStart, &in_dc, &spec);

		if (SUCCEEDED(hr = m_weatherCondition->m_fwi->DC(in_dc, rain, hourlyTemp(dayNoon),
		    m_weatherCondition->m_worldLocation.m_latitude(), m_weatherCondition->m_worldLocation.m_longitude(),
		    (std::uint16_t)m_DayStart.GetMonth(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST) - 1, &val)))
			m_calc_day.dDC = val;
	}
}


void DailyCondition::calculateBUI() {
	if ((m_weatherCondition->m_options & WeatherCondition::USER_SPECIFIED) && (m_spec_day.dBUI >= 0.0))
		m_calc_day.dBUI = m_spec_day.dBUI;
	else {
		double val;
		if (SUCCEEDED(m_weatherCondition->m_fwi->BUI(m_calc_day.dDC, m_calc_day.dDMC, &val)))
			m_calc_day.dBUI = val;
	}
}


void DailyCondition::calculateDMC() {
	if ((m_weatherCondition->m_options & WeatherCondition::USER_SPECIFIED) && (m_spec_day.dDMC >= 0.0))
		m_calc_day.dDMC = m_spec_day.dDMC;
	else {
		HRESULT hr;
		double in_dmc;
		const WTime dayNeutral(m_DayStart, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, 1);	// convert to a timezone-neutral time
		const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL, -1);				// gets us the start of the true LST day
		WTime dayNoon(dayLST);
		dayNoon += WTimeSpan(0, 12, 0, 0);

// From Beck et. al: the standard daily FFMC is computed from ... temperature, relative humidity, 10-m open wind speed, and 24-hr accumulated rainfall
// taken at solar noon or 1300 DST
// we don't have values at solar noon, but we do at noon LST which should be close enough by the sounds of it

		double rain = dailyPrecip(), val;

		bool spec;
		m_weatherCondition->DMC(m_DayStart, &in_dmc, &spec);

		if (SUCCEEDED(hr = m_weatherCondition->m_fwi->DMC(in_dmc, rain, hourlyTemp(dayNoon),
		    m_weatherCondition->m_worldLocation.m_latitude(), m_weatherCondition->m_worldLocation.m_longitude(),
		    (std::uint16_t)m_DayStart.GetMonth(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST) - 1, hourlyRH(dayNoon), &val)))
			m_calc_day.dDMC = val;
	}
}


void DailyCondition::calculateDailyFFMC() {
	if ((m_weatherCondition->m_options & WeatherCondition::USER_SPECIFIED) && (m_spec_day.dFFMC >= 0.0))
		m_calc_day.dFFMC = m_spec_day.dFFMC;
	else {
			HRESULT hr;										// we calculate a daily FFMC if we have a previous daily FFMC, OR if we are using the Lawson
														// techniques (init FFMC value is then treated as the previous daily FFMC)
			const WTime dayNeutral(m_DayStart, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, 1);	// convert to a timezone-neutral time
			const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL, -1);				// gets us the start of the true LST day
			WTime dayNoon(dayLST);
			dayNoon += WTimeSpan(0, 12, 0, 0);

				// From Beck et. al: the standard daily FFMC is computed from ... temperature, relative humidity, 10-m open wind speed, and 24-hr accumulated rainfall
				// taken at solar noon or 1300 DST
				// we don't have values at solar noon, but we do at noon LST which should be close enough by the sounds of it

			double rain = dailyPrecip(), in_ffmc, val;

			bool spec;
 			m_weatherCondition->DailyFFMC(m_DayStart, &in_ffmc, &spec);

			if (SUCCEEDED(hr = m_weatherCondition->m_fwi->DailyFFMC_VanWagner(in_ffmc, rain,
			    hourlyTemp(dayNoon), hourlyRH(dayNoon), hourlyWS(dayNoon), &val)))
				m_calc_day.dFFMC = val;
	}
}


void DailyCondition::calculateHourlyFFMC() {
	double val;
	WTime loop(m_DayStart),
		end(m_DayStart);
	end += WTimeSpan(0, 23, 0, 0);

	WTime streambegin(m_weatherCondition->m_time + WTimeSpan(0, m_weatherCondition->m_firstHour, 0, 0));
	WTime streamend(m_weatherCondition->m_time + WTimeSpan(m_weatherCondition->m_readings.GetCount() - 1, m_weatherCondition->m_lastHour, 0, 0));

	const WTime dayNeutral(m_DayStart, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, 1);	// convert to a timezone-neutral time
	const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL, -1);					// gets us the start of the true LST day
	std::uint16_t i;

	if (!getYesterday()) {										// this is the first day so we will to calculate backwards
													// here, we have to assume that the daily FFMC is the same as the hourly FFMC at noon LST
		double in_ffmc;
		if ((m_weatherCondition->m_initialHFFMCTime == WTimeSpan(-1)) || ((m_weatherCondition->m_options & WeatherCondition::FFMC_MASK) != WeatherCondition::FFMC_VAN_WAGNER)) {
													// our specified FFMC was the previous day's standard FFMC
			in_ffmc = m_calc_day.dFFMC;							// ...so grab our standard FFMC for today
			loop = dayLST;
			loop += WTimeSpan(0, 12, 0, 0);						// and plan to assign it to our LST noon (or DST 1300) entry
		} else {
			loop = m_DayStart;								// our specified start FFMC is an hourly FFMC at a particular hour (LST or LDT)
			loop += m_weatherCondition->m_initialHFFMCTime;
			in_ffmc = m_weatherCondition->m_initialHFFMC;
		}

													// this next block of code actually seeds this initial FFMC value into our array to kick-start our following loop
		i = (std::uint16_t)loop.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);

		bool calculate = true;
		if (m_weatherCondition->m_options & WeatherCondition::USER_SPECIFIED) {
			if (m_spec_hr[i].FFMC >= 0.0)
				m_calc_hr[i].FFMC = m_spec_hr[i].FFMC;
			else	m_calc_hr[i].FFMC = in_ffmc;
		} else		m_calc_hr[i].FFMC = in_ffmc;

		if (streambegin < m_DayStart)
			streambegin = m_DayStart;
		for (loop -= WTimeSpan(0, 1, 0, 0), i--; loop >= streambegin; loop -= WTimeSpan(0, 1, 0, 0), i--) {
													// loop backwards from the previous-hour-for-our-initial-FFMC backwards to the start of our true day (LST or LDT)
			calculate = true;				
			if (m_weatherCondition->m_options & WeatherCondition::USER_SPECIFIED) {
				if (m_spec_hr[i].FFMC >= 0.0) {
					m_calc_hr[i].FFMC = m_spec_hr[i].FFMC;
					calculate = false;
				}
			}

			if (calculate) {
				double temp, rh, precip, ws, gust, wd, dew;
				switch (m_weatherCondition->m_options & WeatherCondition::FFMC_MASK) {
					case WeatherCondition::FFMC_LAWSON:		{	double prev_ffmc;
								bool spec;
								m_weatherCondition->DailyFFMC(m_DayStart , &prev_ffmc, &spec);

								hourlyWeather(loop, &temp, &rh, &precip, &ws, &gust, &wd, &dew);
								m_weatherCondition->m_fwi->HourlyFFMC_Lawson_Contiguous(prev_ffmc,//((DailyCondition *)getYesterday())->dailyFFMC(),
								    m_calc_day.dFFMC, precip, temp, rh, rh, rh, ws, (std::uint32_t)(loop - dayLST)./*(std::uint32_t)loop.GetTimeOfDay(WTIME_FORMAT_AS_LOCAL).*/GetTotalSeconds(), &val);

								break;
							}
					default :	in_ffmc = m_calc_hr[(std::uint16_t)(i + 1)].FFMC;
								hourlyWeather(loop + WTimeSpan(0, 1, 0, 0), &temp, &rh, &precip, &ws, &gust, &wd, &dew);
								m_weatherCondition->m_fwi->HourlyFFMC_VanWagner_Previous(in_ffmc, precip, temp, rh, ws, &val);

								break;
				}
				m_calc_hr[i].FFMC = val;
			}
		}
													// now that we're done looping backwards, let's reseed our loop starting points to loop forwards
		if ((m_weatherCondition->m_initialHFFMCTime == WTimeSpan(-1)) || ((m_weatherCondition->m_options & WeatherCondition::FFMC_MASK) != WeatherCondition::FFMC_VAN_WAGNER)) {
			loop = dayLST;
			loop += WTimeSpan(0, 12, 0, 0);
		} else {
			loop = m_DayStart;
			loop += m_weatherCondition->m_initialHFFMCTime;
			loop += WTimeSpan(0, 1, 0, 0);			// this avoids re-calcing the provided value
		}
	}

    #ifdef _DEBUG
	di = (std::uint16_t)loop.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
    #endif

	if (end > streamend)
		end = streamend;
	for (i = (std::uint16_t)loop.GetHour(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST); loop <= end; i++, loop += WTimeSpan(0, 1, 0, 0)) {
		bool calculate = true;
		if (m_weatherCondition->m_options & WeatherCondition::USER_SPECIFIED) {	// if we're told to use user-specified override FFMC values
			if (m_spec_hr[i].FFMC >= 0.0) {
				m_calc_hr[i].FFMC = m_spec_hr[i].FFMC;
				calculate = false;
			}
		}

		if (calculate) {
			double temp, rh, precip, ws, gust, wd, dew;
			hourlyWeather(loop, &temp, &rh, &precip, &ws, &gust, &wd, &dew);

			switch (m_weatherCondition->m_options & WeatherCondition::FFMC_MASK) {
				case WeatherCondition::FFMC_LAWSON:		{	double prev_ffmc;
							bool spec;
							m_weatherCondition->DailyFFMC(m_DayStart , &prev_ffmc, &spec);

							m_weatherCondition->m_fwi->HourlyFFMC_Lawson_Contiguous(prev_ffmc,
							    m_calc_day.dFFMC, precip, temp, rh, rh, rh, ws, (std::uint32_t)(loop - dayLST).GetTotalSeconds(), &val);

							break;
						}
				default : {	double in_ffmc;
							if (!i)	{
								WTime t(loop);
								t -= WTimeSpan(0, 1, 0, 0);
								m_weatherCondition->HourlyFFMC(t, &in_ffmc);
							} else	in_ffmc = m_calc_hr[(std::uint16_t)(i - 1)].FFMC;

							m_weatherCondition->m_fwi->HourlyFFMC_VanWagner(in_ffmc, precip, temp, rh, ws, 60 * 60, &val);
							break;
						}
			}
			m_calc_hr[i].FFMC = val;
		}
	}
}


void DailyCondition::calculateRemainingFWI() {
	const WTime dayNeutral(m_DayStart, WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST, 1);	// convert to a timezone-neutral time
	const WTime dayLST(dayNeutral, WTIME_FORMAT_AS_LOCAL, -1);				// gets us the start of the true LST day
	WTime dayNoon(dayLST);
	dayNoon += WTimeSpan(0, 12, 0, 0);

	double DISI, DFWI, ws = hourlyWS(dayNoon);
	m_weatherCondition->m_fwi->ISI_FBP(m_calc_day.dFFMC, ws, 24 * 60 * 60, &DISI);
	m_calc_day.dISI = DISI;
	m_weatherCondition->m_fwi->FWI(DISI, m_calc_day.dBUI, &DFWI);
	m_calc_day.dFWI = DFWI;

	WTime loop(m_DayStart);
	std::uint16_t i;

	std::uint16_t start = m_weatherCondition->firstHourOfDay(m_DayStart);
	std::uint16_t end = m_weatherCondition->lastHourOfDay(m_DayStart);
	loop += WTimeSpan(0, start, 0, 0);

	for (i = start; i <= end; i++, loop += WTimeSpan(0, 1, 0, 0)) {
		if ((m_weatherCondition->m_options & WeatherCondition::USER_SPECIFIED) && (m_spec_hr[i].ISI >= 0.0))
			m_calc_hr[i].ISI = m_spec_hr[i].ISI;
		else {
			double dISI, ws1 = hourlyWS(loop);
			m_weatherCondition->m_fwi->ISI_FBP(m_calc_hr[i].FFMC, ws1, 60 * 60, &dISI);
			m_calc_hr[i].ISI = dISI;
		}
		if ((m_weatherCondition->m_options & WeatherCondition::USER_SPECIFIED) && (m_spec_hr[i].FWI >= 0.0))
			m_calc_hr[i].FWI = m_spec_hr[i].FWI;
		else {
			double dFWI, dBUI;
			bool specified;
			m_weatherCondition->BUI(loop, &dBUI, &specified, false);
			m_weatherCondition->m_fwi->FWI(m_calc_hr[i].ISI, dBUI, &dFWI);
			m_calc_hr[i].FWI = dFWI;
		}
	}
}


bool DailyCondition::calculateFWI() {
	calculateDC();
	calculateDMC();
	calculateBUI();
	calculateDailyFFMC();
	calculateHourlyFFMC();
	calculateRemainingFWI();
	return true;
}



		#define DAILYCONDITION_DAILYFFMC_SPECIFIED		0x0010
		#define DAILYCONDITION_DAILYDC_SPECIFIED		0x0020
		#define DAILYCONDITION_DAILYDMC_SPECIFIED		0x0040

		#define DAILYCONDITION_HOURLYFFMC_SPECIFIED		0x01
		#define DAILYCONDITION_HOURLYBUI_SPECIFIED		0x02
		#define DAILYCONDITION_HOURLYISI_SPECIFIED		0x04
		#define DAILYCONDITION_HOURLYFWI_SPECIFIED		0x08


bool DailyCondition::AnyFWICodesSpecified() {
	if (m_spec_day.dFFMC >= 0.0)		return true;
	if (m_spec_day.dDMC >= 0.0)			return true;
	if (m_spec_day.dDC >= 0.0)			return true;
	if (m_spec_day.dBUI >= 0.0)			return true;
	for (std::uint16_t i = 0; i < 24; i++) {
		if (m_spec_hr[i].FFMC >= 0.0)	return true;
		if (m_spec_hr[i].ISI >= 0.0)	return true;
		if (m_spec_hr[i].FWI >= 0.0)	return true;
	}
	return false;
}


std::int32_t DailyCondition::serialVersionUid(const SerializeProtoOptions& options) const noexcept {
	return 1;
}


WISE::WeatherProto::DailyConditions* DailyCondition::serialize(const SerializeProtoOptions& options) {
	auto conditions = new WISE::WeatherProto::DailyConditions();
	conditions->set_version(serialVersionUid(options));

	auto fwi = new WISE::WeatherProto::DailyFwi();
	fwi->set_version(1);

	if (m_flags & DAY_ORIGIN_FILE)
		conditions->set_allocated_fromfile(createProtobufObject(true));
	if (m_flags & DAY_ORIGIN_ENSEMBLE)
		conditions->set_allocated_fromensemble(createProtobufObject(true));
	if (m_flags & DAY_ORIGIN_MODIFIED)
		conditions->set_allocated_ismodified(createProtobufObject(true));

	if (!(m_flags & DAY_HOURLY_SPECIFIED))
	{
		auto day = new WISE::WeatherProto::DailyConditions_DayWeather();

		day->set_allocated_mintemp(DoubleBuilder().withValue(dailyMinTemp()).forProtobuf(options.useVerboseFloats()));
		day->set_allocated_maxtemp(DoubleBuilder().withValue(dailyMaxTemp()).forProtobuf(options.useVerboseFloats()));
		day->set_allocated_minws(DoubleBuilder().withValue(dailyMinWS()).forProtobuf(options.useVerboseFloats()));
		day->set_allocated_maxws(DoubleBuilder().withValue(dailyMaxWS()).forProtobuf(options.useVerboseFloats()));
		if (m_flags & DAY_GUST_SPECIFIED) {
			day->set_allocated_mingust(DoubleBuilder().withValue(dailyMinWS()).forProtobuf(options.useVerboseFloats()));
			day->set_allocated_maxgust(DoubleBuilder().withValue(dailyMaxWS()).forProtobuf(options.useVerboseFloats()));
		}
		day->set_allocated_rh(DoubleBuilder().withValue(dailyMeanRH() * 100.0).forProtobuf(options.useVerboseFloats()));
		day->set_allocated_precip(DoubleBuilder().withValue(dailyPrecip()).forProtobuf(options.useVerboseFloats()));
		day->set_allocated_wd(DoubleBuilder().withValue(ROUND_DECIMAL(CARTESIAN_TO_COMPASS_DEGREE(RADIAN_TO_DEGREE(dailyWD())), 6)).forProtobuf(options.useVerboseFloats()));

		conditions->set_allocated_dayweather(day);

		if (dailyFFMCSpecified())
			fwi->set_allocated_ffmc(DoubleBuilder().withValue(m_spec_day.dFFMC).forProtobuf(options.useVerboseFloats()));
		if (DMCSpecified())
			fwi->set_allocated_dmc(DoubleBuilder().withValue(m_spec_day.dDMC).forProtobuf(options.useVerboseFloats()));
		if (DCSpecified())
			fwi->set_allocated_dc(DoubleBuilder().withValue(m_spec_day.dDC).forProtobuf(options.useVerboseFloats()));
		if (BUISpecified())
			fwi->set_allocated_bui(DoubleBuilder().withValue(m_spec_day.dBUI).forProtobuf(options.useVerboseFloats()));
		conditions->set_allocated_fwi(fwi);
	}
	else if ((m_flags & DAY_HOURLY_SPECIFIED))
	{
		std::uint32_t start = 0, end = 23;
		if (!LN_Pred()->LN_Pred())
			start = m_weatherCondition->m_firstHour;
		if (!LN_Succ()->LN_Succ())
			end = m_weatherCondition->m_lastHour;

		WTime time(m_DayStart);
		auto dayHourly = new WISE::WeatherProto::DailyConditions_DayHourWeather();
		for (std::uint32_t i = start; i <= end; i++)
		{
			auto hour = dayHourly->add_hours();

			double temp, rh, precip, ws, gust, wd, dew;
			hourlyWeather_Serialize(i, &temp, &rh, &precip, &ws, &gust, &wd, &dew);

			hour->set_allocated_temp(DoubleBuilder().withValue(temp).forProtobuf(options.useVerboseFloats()));
			hour->set_allocated_rh(DoubleBuilder().withValue(rh * 100.0).forProtobuf(options.useVerboseFloats()));
			hour->set_allocated_ws(DoubleBuilder().withValue(ws).forProtobuf(options.useVerboseFloats()));
			if ((m_hflags[i] & HOUR_GUST_SPECIFIED) && (gust >= 0.0))
				hour->set_allocated_gust(DoubleBuilder().withValue(gust).forProtobuf(options.useVerboseFloats()));
			hour->set_allocated_precip(DoubleBuilder().withValue(precip).forProtobuf(options.useVerboseFloats()));
			hour->set_allocated_wd(DoubleBuilder().withValue(ROUND_DECIMAL(CARTESIAN_TO_COMPASS_DEGREE(RADIAN_TO_DEGREE(wd)), 6)).forProtobuf(options.useVerboseFloats()));
			if (m_hflags[i] & HOUR_DEWPT_SPECIFIED)
				hour->set_allocated_dewpoint(DoubleBuilder().withValue(dew).forProtobuf(options.useVerboseFloats()));
			hour->set_interpolated(isHourIterpolated(i));

			time += WTimeSpan(0, 1, 0, 0);
		}
		conditions->set_allocated_hourweather(dayHourly);

		for (std::uint32_t i = start; i <= end; i++)
		{
			auto spec = conditions->add_spechour();

			if (m_spec_hr[i].FFMC != -1)
				spec->set_allocated_ffmc(DoubleBuilder().withValue(m_spec_hr[i].FFMC).forProtobuf(options.useVerboseFloats()));
			if (m_spec_hr[i].FWI != -1)
				spec->set_allocated_fwi(DoubleBuilder().withValue(m_spec_hr[i].FWI).forProtobuf(options.useVerboseFloats()));
			if (m_spec_hr[i].ISI != -1)
				spec->set_allocated_isi(DoubleBuilder().withValue(m_spec_hr[i].ISI).forProtobuf(options.useVerboseFloats()));
		}
	}

	return conditions;
}


DailyCondition* DailyCondition::deserialize(const google::protobuf::Message& proto, std::shared_ptr<validation::validation_object> valid, const std::string& name) {
	weak_assert(false);
	return deserialize(proto, valid, name, 0, 23);
}


DailyCondition* DailyCondition::deserialize(const google::protobuf::Message& proto, std::shared_ptr<validation::validation_object> valid, const std::string& name, std::uint16_t firstHour, std::uint16_t lastHour)
{
	auto conditions = dynamic_cast_assert<const WISE::WeatherProto::DailyConditions*>(&proto);

	if (!conditions)
	{
		if (valid)
			/// <summary>
			/// The object passed as a daily condition is invalid. An incorrect object type was passed to the parser.
			/// </summary>
			/// <type>internal</type>
			valid->add_child_validation("WISE.WeatherProto.DailyConditions", name, validation::error_level::SEVERE,
				validation::id::object_invalid, proto.GetDescriptor()->name());
		weak_assert(false);
		throw ISerializeProto::DeserializeError("DailyCondition: Protobuf object invalid", ERROR_PROTOBUF_OBJECT_INVALID);
	}
	if (conditions->version() != 1)
	{
		if (valid)
			/// <summary>
			/// The object version is not supported. The daily condition is not supported by this version of Prometheus.
			/// </summary>
			/// <type>user</type>
			valid->add_child_validation("WISE.WeatherProto.DailyConditions", name, validation::error_level::SEVERE,
				validation::id::version_mismatch, std::to_string(conditions->version()));
		weak_assert(false);
		throw ISerializeProto::DeserializeError("DailyCondition: Version is invalid", ERROR_PROTOBUF_OBJECT_VERSION_INVALID);
	}

	/// <summary>
	/// Child validations for daily conditions.
	/// </summary>
	auto vt = validation::conditional_make_object(valid, "WISE.WeatherProto.DailyConditions", name);
	auto myValid = vt.lock();

	if (conditions->has_fromfile() && conditions->fromfile().value())
		m_flags = DAY_ORIGIN_FILE;
	else
		m_flags = 0;
	if (conditions->has_fromensemble() && conditions->fromensemble().value())
		m_flags |= DAY_ORIGIN_ENSEMBLE;
	if (conditions->has_ismodified() && conditions->ismodified().value())
		m_flags |= DAY_ORIGIN_MODIFIED;

	if (conditions->has_dayweather())
	{
		auto vt2 = validation::conditional_make_object(myValid, "WISE.WeatherProto.DailyConditions.DayWeather", "dayWeather");
		auto myValid2 = vt2.lock();

		m_flags = m_flags & ~DAY_HOURLY_SPECIFIED;
		auto day = conditions->dayweather();

		double minTemp, maxTemp, minWs, maxWs, minGust, maxGust, rh, precip, wd;

		if (day.has_mintemp()) {
			minTemp = DoubleBuilder().withProtobuf(day.mintemp(), myValid2, "minTemp").getValue();
			if ((minTemp < -50.0) || (minTemp > 60.0)) {
				if (myValid2)
					/// <summary>
					/// The minimum temperature is out of range of acceptable values.
					/// </summary>
					/// <type>user</type>
					myValid2->add_child_validation("Math.Double", "minTemp", validation::error_level::WARNING, validation::id::value_invalid, std::to_string(minTemp), { true, -50.0 }, { true, 60.0 }, "C");
				if (minTemp < -50.0)
					minTemp = -50.0;
				else if (minTemp > 60.0)
					minTemp = 60.0;
			}
		}
		else {
			minTemp = 0.0;
			if (myValid2)
				/// <summary>
				/// The minimum temperature is not found.
				/// </summary>
				/// <type>user</type>
				myValid2->add_child_validation("Math.Double", "minTemp", validation::error_level::INFORMATION, validation::id::missing_daily_weather_data, "minTemp");
		}

		if (day.has_maxtemp()) {
			maxTemp = DoubleBuilder().withProtobuf(day.maxtemp(), myValid2, "maxTemp").getValue();
			if ((maxTemp < -50.0) || (maxTemp > 60.0)) {
				if (myValid2)
					/// <summary>
					/// The maximum temperature is out of range of acceptable values.
					/// </summary>
					/// <type>user</type>
					myValid2->add_child_validation("Math.Double", "minTemp", validation::error_level::WARNING, validation::id::value_invalid, std::to_string(minTemp), { true, -50.0 }, { true, 60.0 }, "C");
				if (maxTemp < -50.0)
					maxTemp = -50.0;
				else if (maxTemp > 60.0)
					maxTemp = 60.0;
			}
		}
		else {
			if (myValid2)
				/// <summary>
				/// The maximum temperature is not found.
				/// </summary>
				/// <type>user</type>
				myValid2->add_child_validation("Math.Double", "maxTemp", validation::error_level::SEVERE, validation::id::missing_daily_weather_data, "maxTemp");
			else
				throw std::invalid_argument("Error: WISE.WeatherProto.DailyConditions.DayWeather: Missing maxTemp value");
		}

		if (minTemp > maxTemp) {
			std::swap(minTemp, maxTemp);
			if (myValid)
				myValid->add_child_validation("Math.Double", { "minTemp", "maxTemp" }, validation::error_level::INFORMATION, validation::id::value_invalid, { std::to_string(minTemp), std::to_string(maxTemp) }, "C");
		}

		if (day.has_minws()) {
			minWs = DoubleBuilder().withProtobuf(day.minws(), myValid2, "minWs").getValue();
			if ((minWs < 0.0) || (minWs > 200.0)) {
				if (myValid2)
					/// <summary>
					/// The minimum wind speed is out of range of acceptable values.
					/// </summary>
					/// <type>user</type>
					myValid2->add_child_validation("Math.Double", "minWs", (minWs > 200.0) ? validation::error_level::INFORMATION : validation::error_level::WARNING, validation::id::value_invalid, std::to_string(minWs), { true, 0.0 }, { true, 200.0 });
				if (minWs < 0.0)
					minWs = 0.0;
				else if (minWs > 200.0)
					minWs = 200.0;
			}
		}
		else {
			minWs = 0.0;
			if (myValid2)
				/// <summary>
				/// The minimum wind speed is not found.
				/// </summary>
				/// <type>user</type>
				myValid2->add_child_validation("Math.Double", "minWs", validation::error_level::INFORMATION, validation::id::missing_daily_weather_data, "minWs");
		}

		if (day.has_maxws()) {
			maxWs = DoubleBuilder().withProtobuf(day.maxws(), myValid2, "maxWs").getValue();
			if ((maxWs < 0.0) || (maxWs > 200.0)) {
				if (myValid2)
					/// <summary>
					/// The maximum wind speed is out of range of acceptable values.
					/// </summary>
					/// <type>user</type>
					myValid2->add_child_validation("Math.Double", "maxWs", (maxWs > 200.0) ? validation::error_level::INFORMATION : validation::error_level::WARNING, validation::id::value_invalid, std::to_string(maxWs), { true, 0.0 }, { true, 200.0 });
				if (maxWs < 0.0)
					maxWs = 0.0;
				else if (maxWs > 200.0)
					maxWs = 200.0;
			}
		}
		else {
			if (myValid2)
				/// <summary>
				/// The maximum wind speed is not found.
				/// </summary>
				/// <type>user</type>
				myValid2->add_child_validation("Math.Double", "maxWs", validation::error_level::SEVERE, validation::id::missing_daily_weather_data, "maxWs");
			else
				throw std::invalid_argument("Error: WISE.WeatherProto.DailyConditions.DayWeather: Missing maxWs value");
		}

		if (minWs > maxWs) {
			std::swap(minWs, maxWs);
			if (myValid) {
				myValid->add_child_validation("Math.Double", { "minWs", "maxWs" }, validation::error_level::INFORMATION, validation::id::value_invalid, { std::to_string(minWs), std::to_string(maxWs) }, "C");
			}
		}

		if (day.has_mingust()) {
			minGust = DoubleBuilder().withProtobuf(day.mingust(), myValid2, "minGust").getValue();
			if ((minGust < 0.0) || (minGust > 200.0)) {
				if (myValid2)
					/// <summary>
					/// The minimum wind speed is out of range of acceptable values.
					/// </summary>
					/// <type>user</type>
					myValid2->add_child_validation("Math.Double", "minGust", (minGust > 200.0) ? validation::error_level::INFORMATION : validation::error_level::WARNING, validation::id::value_invalid, std::to_string(minGust), { true, 0.0 }, { true, 200.0 });
				if (minGust < 0.0)
					minGust = 0.0;
				else if (minGust > 200.0)
					minGust = 200.0;
			}
		}
		else {
			minGust = -1.0;
			if (myValid2)
				/// <summary>
				/// The minimum wind speed is not found.
				/// </summary>
				/// <type>user</type>
				myValid2->add_child_validation("Math.Double", "minGust", validation::error_level::INFORMATION, validation::id::missing_daily_weather_data, "minGust");
		}

		if (day.has_maxgust()) {
			maxGust = DoubleBuilder().withProtobuf(day.maxgust(), myValid2, "maxGust").getValue();
			if ((maxGust < 0.0) || (maxGust > 200.0)) {
				if (myValid2)
					/// <summary>
					/// The maximum wind speed is out of range of acceptable values.
					/// </summary>
					/// <type>user</type>
					myValid2->add_child_validation("Math.Double", "maxGust", (maxGust > 200.0) ? validation::error_level::INFORMATION : validation::error_level::WARNING, validation::id::value_invalid, std::to_string(maxGust), { true, 0.0 }, { true, 200.0 });
				if (maxGust < 0.0)
					maxGust = 0.0;
				else if (maxGust > 200.0)
					maxGust = 200.0;
			}
		}
		else {
			maxGust = -1.0;
			if (myValid2)
				/// <summary>
				/// The maximum wind speed is not found.
				/// </summary>
				/// <type>user</type>
				myValid2->add_child_validation("Math.Double", "maxGust", validation::error_level::INFORMATION, validation::id::missing_daily_weather_data, "maxGust");
		}

		if (minGust > maxGust) {
			std::swap(minGust, maxGust);
			if (myValid) {
				myValid->add_child_validation("Math.Double", { "minGust", "maxGust" }, validation::error_level::INFORMATION, validation::id::value_invalid, { std::to_string(minGust), std::to_string(maxGust) }, "C");
			}
		}

		if (day.has_rh()) {
			rh = DoubleBuilder().withProtobuf(day.rh(), myValid2, "rh").getValue() * 0.01;
			if ((rh < 0.0) || (rh > 100.0)) {
				if (myValid2)
					/// <summary>
					/// Relative humidity is out of range of acceptable values.
					/// </summary>
					/// <type>user</type>
					myValid2->add_child_validation("Math.Double", "rh", validation::error_level::WARNING, validation::id::value_invalid, std::to_string(rh), { true, 0.0 }, { true, 100.0 });
				if (rh < 0.0)
					rh = 0.0;
				else if (rh > 200.0)
					rh = 200.0;
			}
		}
		else {
			if (myValid2)
				/// <summary>
				/// Relative humidity is not found.
				/// </summary>
				/// <type>user</type>
				myValid2->add_child_validation("Math.Double", "rh", validation::error_level::SEVERE, validation::id::missing_daily_weather_data, "rh");
			else
				throw std::invalid_argument("Error: WISE.WeatherProto.DailyConditions.DayWeather: Missing rh value");
		}

		if (day.has_precip()) {
			precip = DoubleBuilder().withProtobuf(day.precip(), myValid2, "precip").getValue();
			if ((precip < 0.0) || (precip > 300.0)) {
				if (myValid2)
					/// <summary>
					/// Precipitation is out of range of acceptable values.
					/// </summary>
					/// <type>user</type>
					myValid2->add_child_validation("Math.Double", "precip", (precip > 300.0) ? validation::error_level::INFORMATION : validation::error_level::WARNING, validation::id::value_invalid, std::to_string(precip), { true, 0.0 }, { true, 300.0 });
				if (precip < 0.0)
					precip = 0.0;
				else if (precip > 300.0)
					precip = 300.0;
			}
		}
		else {
			precip = 0.0;
			if (myValid2)
				/// <summary>
				/// Precipitation is not found.
				/// </summary>
				/// <type>user</type>
				myValid2->add_child_validation("Math.Double", "precip", validation::error_level::INFORMATION, validation::id::missing_daily_weather_data, "precip");
		}

		if (day.has_wd()) {
			wd = COMPASS_TO_CARTESIAN_RADIAN(DEGREE_TO_RADIAN(DoubleBuilder().withProtobuf(day.wd(), myValid2, "wd").getValue()));
			if ((wd < 0.0) || (wd > 360.0)) {
				if (myValid2)
					/// <summary>
					/// The wind direction is out of range of acceptable values.
					/// </summary>
					/// <type>user</type>
					myValid2->add_child_validation("Math.Double", "precip", validation::error_level::WARNING, validation::id::value_invalid, std::to_string(wd), { true, 0.0 }, { true, 360.0 });
				if (wd < 0.0)
					wd = 0.0;
				else if (wd > 360.0)
					wd = 360.0;
			}
		}
		else {
			if (myValid2)
				/// <summary>
				/// The wind direction is not found.
				/// </summary>
				/// <type>user</type>
				myValid2->add_child_validation("Math.Double", "wd", validation::error_level::SEVERE, validation::id::missing_daily_weather_data, "wd");
			else
				throw std::invalid_argument("Error: WISE.WeatherProto.DailyConditions.DayWeather: Missing wd value");
		}

		setDailyWeather(minTemp, maxTemp, minWs, maxWs, minGust, maxGust, rh, precip, wd);

		if (conditions->has_fwi())
		{
			auto vt3 = validation::conditional_make_object(myValid, "WISE.WeatherProto.DailyFwi", "fwi");
			auto myValid2 = vt3.lock();

			if (conditions->fwi().has_ffmc())
			{
				m_spec_day.SpecifiedBits |= DFWIDATA_SPECIFIED_FFMC;
				m_spec_day.dFFMC = DoubleBuilder().withProtobuf(conditions->fwi().ffmc(), myValid2, "ffmc").getValue();
				if (m_spec_day.dFFMC < 0.0 || m_spec_day.dFFMC > 101.0)
				{
					if (myValid2)
						/// <summary>
						/// The FFMC is out of range of acceptable values.
						/// </summary>
						/// <type>user</type>
						myValid2->add_child_validation("Math.Double", "ffmc", validation::error_level::SEVERE,
							validation::id::ffmc_invalid, std::to_string(m_spec_day.dFFMC),
							{ true, 0.0 }, { true, 101.0 });
					else
						throw std::invalid_argument("Error: WISE.WeatherProto.DailyFwi: Invalid FFMC value");
				}
			}
			else
				m_spec_day.dFFMC = -1.0;

			if (conditions->fwi().has_isi())
			{
				m_spec_day.SpecifiedBits |= DFWIDATA_SPECIFIED_ISI;
				m_spec_day.dISI = DoubleBuilder().withProtobuf(conditions->fwi().isi(), myValid2, "isi").getValue();
			}
			else
				m_spec_day.dISI = -1.0;

			if (conditions->fwi().has_fwi())
			{
				m_spec_day.SpecifiedBits |= DFWIDATA_SPECIFIED_FWI;
				m_spec_day.dFWI = DoubleBuilder().withProtobuf(conditions->fwi().fwi(), myValid2, "fwi").getValue();
			}
			else
				m_spec_day.dFWI = -1.0;

			if (conditions->fwi().has_dmc())
			{
				m_spec_day.SpecifiedBits |= DFWIDATA_SPECIFIED_DMC;
				m_spec_day.dDMC = DoubleBuilder().withProtobuf(conditions->fwi().dmc(), myValid2, "dmc").getValue();
				if (m_spec_day.dDMC < 0.0 || m_spec_day.dDMC > 500.0)
				{
					if (myValid2)
						/// <summary>
						/// The DMC is out of range of acceptable values.
						/// </summary>
						/// <type>user</type>
						myValid2->add_child_validation("Math.Double", "dmc", validation::error_level::SEVERE,
							validation::id::dmc_invalid, std::to_string(m_spec_day.dFFMC),
							{ true, 0.0 }, { true, 101.0 });
					else
						throw std::invalid_argument("Error: WISE.WeatherProto.DailyFwi: Invalid DMC value");
				}
			}
			else
				m_spec_day.dDMC = -1.0;

			if (conditions->fwi().has_dc())
			{
				m_spec_day.SpecifiedBits |= DFWIDATA_SPECIFIED_DC;
				m_spec_day.dDC = DoubleBuilder().withProtobuf(conditions->fwi().dc(), myValid2, "dc").getValue();
				if (m_spec_day.dDC < 0.0 || m_spec_day.dDC > 1500.0)
				{
					if (myValid2)
						/// <summary>
						/// The DC is out of range of acceptable values.
						/// </summary>
						/// <type>user</type>
						myValid2->add_child_validation("Math.Double", "dc", validation::error_level::SEVERE,
							validation::id::dc_invalid, std::to_string(m_spec_day.dFFMC),
							{ true, 0.0 }, { true, 101.0 });
					else
						throw std::invalid_argument("Error: WISE.WeatherProto.DailyFwi: Invalid DC value");
				}
			}
			else
				m_spec_day.dDC = -1.0;

			if (conditions->fwi().has_bui())
			{
				m_spec_day.SpecifiedBits |= DFWIDATA_SPECIFIED_BUI;
				m_spec_day.dBUI = DoubleBuilder().withProtobuf(conditions->fwi().bui(), myValid2, "bui").getValue();
				if (m_spec_day.dBUI < 1.0 && ((m_spec_day.dBUI != -99.0) && (m_spec_day.dBUI != -1.0)))
				{
					if (myValid2)
						myValid2->add_child_validation("Math.Double", "bui", validation::error_level::SEVERE,
							validation::id::bui_invalid, std::to_string(m_spec_day.dBUI),
							{ true, 1.0 }, { true, 300.0 });
					else
						throw std::invalid_argument("Error: WISE.WeatherProto.DailyFwi: Invalid BUI value");
				}
			}
			else
				m_spec_day.dBUI = -1.0;
		}
	}
	else if (conditions->has_hourweather() && conditions->hourweather().hours_size() == (lastHour - firstHour + 1))
	{
		std::uint32_t start = firstHour, end = lastHour;

#ifdef _DEBUG
		if (!LN_Pred()->LN_Pred()) {
			weak_assert(firstHour == m_weatherCondition->m_firstHour);
		}
#endif

		m_flags |= DAY_HOURLY_SPECIFIED;

		for (std::uint32_t i = start; i <= end; i++)
		{
			/// <summary>
			/// Child validations for hourly weather data.
			/// </summary>
			auto vt2 = validation::conditional_make_object(myValid, "WISE.WeatherProto.DailyConditions.HourWeather", strprintf("hours[%d]", i));
			auto hourValid = vt2.lock();

			auto hour = conditions->hourweather().hours(i - start);

			double temp, rh, precip, ws, gust, wd, dew;

			if (hour.has_temp()) {
				temp = DoubleBuilder().withProtobuf(hour.temp(), hourValid, "temp").getValue();
				if ((temp < -50.0) || (temp > 60.0)) {
					if (hourValid)
						/// <summary>
						/// The temperature is out of range of acceptable values.
						/// </summary>
						/// <type>user</type>
						hourValid->add_child_validation("Math.Double", "minTemp", validation::error_level::WARNING, validation::id::value_invalid, std::to_string(temp), { true, -50.0 }, { true, 60.0 });
					if (temp < -50.0)
						temp = -50.0;
					else if (temp > 60.0)
						temp = 60.0;
				}
			}

			if (hour.has_rh()) {
				rh = DoubleBuilder().withProtobuf(hour.rh(), hourValid, "rh").getValue() * 0.01;
				if ((rh < 0.0) || (rh > 100.0)) {
					if (hourValid)
						/// <summary>
						/// The relative humidity is out of range of acceptable values.
						/// </summary>
						/// <type>user</type>
						hourValid->add_child_validation("Math.Double", "rh", validation::error_level::WARNING, validation::id::value_invalid, std::to_string(rh), { true, 0.0 }, { true, 100.0 });
					if (rh < 0.0)
						rh = 0.0;
					else if (rh > 200.0)
						rh = 200.0;
				}
			}

			if (hour.has_precip()) {
				precip = DoubleBuilder().withProtobuf(hour.precip(), hourValid, "precip").getValue();
				if ((precip < 0.0) || (precip > 300.0)) {
					if (hourValid)
						/// <summary>
						/// The precipitation is out of range of acceptable values.
						/// </summary>
						/// <type>user</type>
						hourValid->add_child_validation("Math.Double", "precip", (precip > 300.0) ? validation::error_level::INFORMATION : validation::error_level::WARNING, validation::id::value_invalid, std::to_string(precip), { true, 0.0 }, { true, 300.0 });
					if (precip < 0.0)
						precip = 0.0;
					else if (precip > 300.0)
						precip = 300.0;
				}
			}

			if (hour.has_ws()) {
				ws = DoubleBuilder().withProtobuf(hour.ws(), hourValid, "ws").getValue();
				if ((ws < 0.0) || (ws > 200.0)) {
					if (hourValid)
						/// <summary>
						/// The minimum wind speed is out of range of acceptable values.
						/// </summary>
						/// <type>user</type>
						hourValid->add_child_validation("Math.Double", "ws", (ws > 200.0) ? validation::error_level::INFORMATION : validation::error_level::WARNING, validation::id::value_invalid, std::to_string(ws), { true, 0.0 }, { true, 200.0 });
					if (ws < 0.0)
						ws = 0.0;
					else if (ws > 200.0)
						ws = 200.0;
				}
			}

			if (hour.has_gust()) {
				gust = DoubleBuilder().withProtobuf(hour.gust(), hourValid, "gust").getValue();
				if ((gust < 0.0) || (gust > 200.0)) {
					if (hourValid)
						/// <summary>
						/// The minimum wind speed is out of range of acceptable values.
						/// </summary>
						/// <type>user</type>
						hourValid->add_child_validation("Math.Double", "gust", (gust > 200.0) ? validation::error_level::INFORMATION : validation::error_level::WARNING, validation::id::value_invalid, std::to_string(ws), { true, 0.0 }, { true, 200.0 });
					if (gust < 0.0)
						gust = 0.0;
					else if (gust > 200.0)
						gust = 200.0;
				}
			}
			else
				gust = -1.0;		// this states that no gust was applied

			if (hour.has_wd()) {
				wd = COMPASS_TO_CARTESIAN_RADIAN(DEGREE_TO_RADIAN(DoubleBuilder().withProtobuf(hour.wd(), hourValid, "wd").getValue()));
				if ((wd < 0.0) || (wd > 360.0)) {
					if (hourValid)
						/// <summary>
						/// The wind direction is out of range of acceptable values.
						/// </summary>
						/// <type>user</type>
						hourValid->add_child_validation("Math.Double", "wd", validation::error_level::WARNING, validation::id::value_invalid, std::to_string(wd), { true, 0.0 }, { true, 360.0 });
					if (wd < 0.0)
						wd = 0.0;
					else if (wd > 360.0)
						wd = 360.0;
				}
			}

			if (hour.has_dewpoint())
			{
				dew = DoubleBuilder().withProtobuf(hour.dewpoint(), hourValid, "dewPoint").getValue();
				m_hflags[i] |= HOUR_DEWPT_SPECIFIED;
			}
			else
			{
				dew = -400.0;
				m_hflags[i] &= ~HOUR_DEWPT_SPECIFIED;
			}

			setHourlyWeather(i, temp, rh, precip, ws, gust, wd, dew);
			if (hour.interpolated())
				setHourInterpolated(i);

			for (std::uint32_t ii = start; (ii < conditions->spechour_size()) && (ii <= end); ii++)
			{
				/// <summary>
				/// Child validations for hourly FFMC specifications.
				/// </summary>
				auto vt3 = validation::conditional_make_object(myValid, "WISE.WeatherProto.DailyConditions.SpecHour", strprintf("spechour[%d]", i));
				auto specValid = vt3.lock();

				auto spec = conditions->spechour(ii - start);

				if (spec.has_ffmc()) {
					m_spec_hr[ii].FFMC = DoubleBuilder().withProtobuf(spec.ffmc(), specValid, "ffmc").getValue();
					if (m_spec_hr[ii].FFMC < 0.0 || m_spec_hr[ii].FFMC > 101.0)
					{
						if (specValid)
							specValid->add_child_validation("Math.Double", "ffmc", validation::error_level::SEVERE,
								validation::id::ffmc_invalid, std::to_string(m_spec_hr[ii].FFMC),
								{ true, 0.0 }, { true, 101.0 });
						throw std::invalid_argument("Error: WISE.WeatherProto.WeatherCondition: Invalid FFMC value");
					}
				}
				else
					m_spec_hr[ii].FFMC = -1.0;
				if (spec.has_fwi())
					m_spec_hr[ii].FWI = DoubleBuilder().withProtobuf(spec.fwi(), specValid, "fwi").getValue();
				else
					m_spec_hr[ii].FWI = -1.0;
				if (spec.has_isi())
					m_spec_hr[ii].ISI = DoubleBuilder().withProtobuf(spec.isi(), specValid, "isi").getValue();
				else
					m_spec_hr[ii].ISI = -1.0;
			}
		}
	}
	else {
		if (myValid)
			/// <summary>
			/// The number of hourly readings is not expected.
			/// </summary>
			/// <type>internal</type>
			myValid->add_child_validation("WISE.WeatherProto.DailyConditions", name, validation::error_level::SEVERE,
				validation::id::incorrect_amt_weather_data, proto.GetDescriptor()->name());
		weak_assert(false);
		throw std::invalid_argument("DailyCondition: Invalid number of hourly readings");
	}

	return this;
}
