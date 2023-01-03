/**
 * WISE_Weather_Module: CWFGM_WeatherStream.Serialize.h
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

#include "propsysreplacement.h"
#include "CWFGM_WeatherStream.h"
#include "WeatherCom_ext.h"
#include "GridCom_ext.h"
#include "results.h"
#include "DayCondition.h"
#include <errno.h>

#ifdef DEBUG
#include <assert.h>
#endif


HRESULT CCWFGM_WeatherStream::newCondition(DailyCondition** cond)
{
	*cond = new DailyCondition(&m_weatherCondition);
	return S_OK;
}


HRESULT CCWFGM_WeatherStream::Import(const std::string & fileName, std::uint16_t options) {
	if (!fileName.length())							return E_INVALIDARG;

	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)							return ERROR_SCENARIO_SIMULATION_RUNNING;

	if (m_weatherCondition.NumDays()) {
		std::uint16_t purge = options & CWFGM_WEATHERSTREAM_IMPORT_PURGE;
		std::uint16_t overwrite_append = options & (CWFGM_WEATHERSTREAM_IMPORT_SUPPORT_APPEND | CWFGM_WEATHERSTREAM_IMPORT_SUPPORT_OVERWRITE);
		if (purge && overwrite_append)
			return E_INVALIDARG;
		if (options & (~(CWFGM_WEATHERSTREAM_IMPORT_PURGE | CWFGM_WEATHERSTREAM_IMPORT_SUPPORT_APPEND | CWFGM_WEATHERSTREAM_IMPORT_SUPPORT_OVERWRITE)))
			return E_INVALIDARG;
		if (!options)
			return E_INVALIDARG;
	}

	HRESULT success = m_weatherCondition.Import(fileName.c_str(), options, nullptr);
	m_bRequiresSave = true;
	m_weatherCondition.m_options |= 0x00000020;
	m_cache.Clear();
	return success;
}


std::int32_t CCWFGM_WeatherStream::serialVersionUid(const SerializeProtoOptions& options) const noexcept {
	return options.fileVersion();
}


WISE::WeatherProto::CwfgmWeatherStream* CCWFGM_WeatherStream::serialize(const SerializeProtoOptions& options) {
	auto stream = new WISE::WeatherProto::CwfgmWeatherStream();
	stream->set_version(serialVersionUid(options));

	stream->set_allocated_condition(m_weatherCondition.serialize(options));

	return stream;
}


CCWFGM_WeatherStream* CCWFGM_WeatherStream::deserialize(const google::protobuf::Message& message, std::shared_ptr<validation::validation_object> valid, const std::string& name) {
	auto stream = dynamic_cast_assert<const WISE::WeatherProto::CwfgmWeatherStream*>(&message);

	if (!stream)
	{
		if (valid)
			valid->add_child_validation("WISE.WeatherProto.CwfgmWeatherStream", name, validation::error_level::SEVERE, validation::id::object_invalid, message.GetDescriptor()->name());
		weak_assert(false);
		throw ISerializeProto::DeserializeError("WISE.WeatherProto.CwfgmWeatherStation: Protobuf object invalid", ERROR_PROTOBUF_OBJECT_INVALID);
	}
	if ((stream->version() != 1) && (stream->version() != 2))
	{
		if (valid)
			valid->add_child_validation("WISE.WeatherProto.CwfgmWeatherStream", name, validation::error_level::SEVERE, validation::id::version_mismatch, message.GetDescriptor()->name());
		weak_assert(false);
		throw ISerializeProto::DeserializeError("WISE.WeatherProto.CwfgmWeatherStation: Version is invalid", ERROR_PROTOBUF_OBJECT_VERSION_INVALID);
	}

	auto vt = validation::conditional_make_object(valid, "WISE.WeatherProto.CwfgmWeatherStream", name);
	auto v = vt.lock();

	try
	{
		m_weatherCondition.deserialize(stream->condition(), v, "condition");
	}
	catch (std::exception & e)
	{
		m_loadWarning = e.what();
		throw e;
	}

	return this;
}
