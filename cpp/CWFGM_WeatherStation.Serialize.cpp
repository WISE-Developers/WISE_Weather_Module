/**
 * WISE_Weather_Module: CWFGM_WeatherStation.Serialize.cpp
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

#include "angles.h"
#include "propsysreplacement.h"
#include "WeatherCom_ext.h"
#include "CWFGM_WeatherStation.h"
#include "CWFGM_WeatherStream.h"
#include "results.h"
#include "GridCom_ext.h"
#include "geo_poly.h"
#include "str_printf.h"
#include "doubleBuilder.h"


#include <cpl_string.h>
#include "CoordinateConverter.h"


void CCWFGM_WeatherStation::calculateXY() {
	if (!m_gridEngine)
		return;
	if (m_utmSet)
		return;
	if (!m_locationSpecified)
		return;

	double latitude = RADIAN_TO_DEGREE(m_latitude), longitude = RADIAN_TO_DEGREE(m_longitude);
	std::uint16_t xdim, ydim;
	HRESULT hr;
	if (FAILED(hr = m_gridEngine->GetDimensions(0, &xdim, &ydim))) { weak_assert(false); return; }

	/*POLYMORPHIC CHECK*/
	PolymorphicAttribute var;

	if (FAILED(hr = m_gridEngine->GetAttribute(0, CWFGM_GRID_ATTRIBUTE_SPATIALREFERENCE, &var))) { weak_assert(false); return; }
	std::string projection;
	
	try { projection = std::get<std::string>(var); } catch (std::bad_variant_access&) { weak_assert(false); return; };

	CCoordinateConverter cc;
	cc.SetSourceProjection(projection.c_str());

	XY_Point loc = cc.start()
		.fromPoints(longitude, latitude, 0.0)
		.asLatLon()
		.endInUTM()
		.to2DPoint();
	m_location = loc;
	m_utmSet = true;
	m_utmSpecified = true;	// forcibly switch to native storage of UTM
}


void CCWFGM_WeatherStation::calculateLatLon() {
	if (!m_gridEngine)
		return;
	if (m_locationSet)
		return;
	if (!m_utmSpecified)
		return;
	if (m_locationSpecified)
		return;

	std::uint16_t xdim, ydim;
	HRESULT hr;
	if (FAILED(hr = m_gridEngine->GetDimensions(0, &xdim, &ydim))) { weak_assert(false); return; }

	PolymorphicAttribute var;

	if (FAILED(hr = m_gridEngine->GetAttribute(0, CWFGM_GRID_ATTRIBUTE_SPATIALREFERENCE, &var))) { weak_assert(false); return; }
	std::string projection;
	
	try { projection = std::get<std::string>(var); } catch (std::bad_variant_access&) { weak_assert(false); return; };

	CCoordinateConverter cc;
	cc.SetSourceProjection(projection.c_str());

	XY_Point loc(m_location);
	cc.start()
		.fromPoint(loc)
		.asSource()
		.endInRadians()
		.toPoints(&m_longitude, &m_latitude);
	m_locationSet = true;
}


std::int32_t CCWFGM_WeatherStation::serialVersionUid(const SerializeProtoOptions& options) const noexcept {
	return options.fileVersion();
}


WISE::WeatherProto::CwfgmWeatherStation* CCWFGM_WeatherStation::serialize(const SerializeProtoOptions& options) {
	auto station = new WISE::WeatherProto::CwfgmWeatherStation();
	station->set_version(serialVersionUid(options));

	XY_Point location(m_location);


	GeoPoint geo(location);
	geo.setStoredUnits(GeoPoint::UTM);
	station->set_allocated_location(geo.getProtobuf(options.useVerboseFloats()));

	if (m_elevationSet)
		station->set_allocated_elevation(DoubleBuilder().withValue(m_elevation).forProtobuf(options.useVerboseFloats()));

	if (options.useVerboseOutput())
	{
		StreamNode *sn = (StreamNode *)m_streamList.LH_Head();
		while (sn->LN_Succ())
		{
			auto stream = sn->m_stream->serialize(options);
			station->add_streams()->Swap(stream);
			delete stream;

			sn = (StreamNode *)sn->LN_Succ();
		}
	}

	return station;
}

CCWFGM_WeatherStation* CCWFGM_WeatherStation::deserialize(const google::protobuf::Message& message, std::shared_ptr<validation::validation_object> valid, const std::string& name)
{
	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine;
	if (!m_gridEngine) {
		if (valid)
			valid->add_child_validation("WISE.WeatherProto.CcwfgmWeatherStation", name, validation::error_level::WARNING, validation::id::initialization_incomplete, "gridEngine");
		weak_assert(false);
		m_loadWarning = "Error: WISE.WeatherProto.CcwfgmWeatherStation: No grid engine";
		throw ISerializeProto::DeserializeError("WISE.GridProto.CcwfgmWeatherStation: Incomplete initialization");
	}

	auto proto = dynamic_cast_assert<const WISE::WeatherProto::CwfgmWeatherStation*>(&message);

	if (!proto)
	{
		if (valid)
			/// <summary>
			/// The object passed as a weather station is invalid. An incorrect object type was passed to the parser.
			/// </summary>
			/// <type>internal</type>
			valid->add_child_validation("WISE.WeatherProto.CwfgmWeatherStation", name, validation::error_level::SEVERE,
				validation::id::object_invalid, message.GetDescriptor()->name());
		weak_assert(false);
		m_loadWarning = "Error: WISE.WeatherProto.CcwfgmWeatherStation: Protobuf object invalid";
		throw ISerializeProto::DeserializeError("WISE.WeatherProto.CcwfgmWeatherStation: Protobuf object invalid", ERROR_PROTOBUF_OBJECT_INVALID);
	}

	if ((proto->version() != 1) && (proto->version() != 2))
	{
		if (valid)
			/// <summary>
			/// The object version is not supported. The weather station is not supported by this version of Prometheus.
			/// </summary>
			/// <type>user</type>
			valid->add_child_validation("WISE.WeatherProto.CwfgmWeatherStation", name, validation::error_level::SEVERE,
				validation::id::version_mismatch, std::to_string(proto->version()));
		weak_assert(false);
		m_loadWarning = "Error: WISE.WeatherProto.CcwfgmWeatherStation: Version is invalid";
		throw ISerializeProto::DeserializeError("WISE.WeatherProto.CcwfgmWeatherStation: Version is invalid", ERROR_PROTOBUF_OBJECT_VERSION_INVALID);
	}

	/// <summary>
	/// Child validations for weather stations.
	/// </summary>
	auto vt = validation::conditional_make_object(valid, "WISE.WeatherProto.CwfgmWeatherStation", name);
	auto myValid = vt.lock();

	if (proto->has_location())
	{
		CCoordinateConverter cc;

		PolymorphicAttribute var;
		if (FAILED(m_gridEngine->GetAttribute(0, CWFGM_GRID_ATTRIBUTE_SPATIALREFERENCE, &var))) { 
			if (myValid)
				/// <summary>
				/// The projection is not readable but should be by this time in deserialization.
				/// </summary>
				/// <type>internal</type>
				myValid->add_child_validation("WISE.WeatherProto.CwfgmWeatherStation", name, validation::error_level::SEVERE,
					validation::id::initialization_incomplete, "projection");
			m_loadWarning = "Error: WISE.WeatherProto.CcwfgmWeatherStation: Incomplete initialization";
			weak_assert(false);
			throw ISerializeProto::DeserializeError("WISE.GridProto.CcwfgmWeatherStation: Incomplete initialization");
		}
		if (std::holds_alternative<std::string>(var))
		{
			std::string projection = std::get<std::string>(var);
			cc.SetSourceProjection(projection.c_str());
		}

		GeoPoint geo(proto->location());
		geo.setStoredUnits(GeoPoint::UTM);
		geo.setConverter([&cc](std::uint8_t type, double x, double y, double z) -> std::tuple<double, double, double>
		{
			XY_Point loc = cc.start()
				.fromPoints(x, y, z)
				.asLatLon()
				.endInUTM()
				.to2DPoint();
			return std::make_tuple(loc.x, loc.y, 0.0);
		});
		m_location = XY_Point(geo.getPoint(myValid, "location"));
		m_locationSet = false;
		m_utmSpecified = true;
		if (proto->has_elevation()) {
			m_elevation = DoubleBuilder().withProtobuf(proto->elevation(), myValid, "elevation").getValue();
			m_elevationSet = true;
		}
		else
			m_elevationSet = false;
		calculateLatLon();
	}

	double elev = DoubleBuilder().withProtobuf(proto->elevation()).getValue();
	SetAttribute(CWFGM_GRID_ATTRIBUTE_DEFAULT_ELEVATION, elev);

	if (!proto->has_skipstream() || !proto->skipstream().value())
	{
		for (int i = 0; i < proto->streams_size(); i++)
		{
			auto s = proto->streams(i);
			CCWFGM_WeatherStream* stream = nullptr;
			try {
				stream = new CCWFGM_WeatherStream();
			}
			catch (std::exception& e) {
				weak_assert(false);
				m_loadWarning = "Error: WISE.GridProto.CcwfgmWeatherStation: No more memory";
				/// <summary>
				/// The COM object could not be instantiated.
				/// </summary>
				/// <type>internal</type>
				if (myValid)
					myValid->add_child_validation("WISE.WeatherProto.CwfgmWeatherStation", strprintf("streams[%d]", i), validation::error_level::SEVERE, validation::id::cannot_allocate, "CLSID_CWFGM_WeatherStream");
				throw std::bad_alloc();
			}
			if (stream->deserialize(s, myValid, strprintf("streams[%d]", i)))
				AddStream(stream, (std::uint32_t)-1);
			else {
				m_loadWarning = "Error: WISE.WeatherProto.CcwfgmWeatherStation: Incomplete initialization";
				return nullptr;
			}
		}
	}

	return this;
}
