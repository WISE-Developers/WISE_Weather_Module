/**
 * WISE_Weather_Module: CWFGM_WeatherStation.h
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



#ifndef DOXYGEN_IGNORE_CODE

CCWFGM_WeatherStation::CCWFGM_WeatherStation() {
	m_bRequiresSave = false;
	m_latitude = 0.0;
	m_longitude = 0.0;
	m_location.x = m_location.y = 0.0;
	m_elevation = -9999.0;
	m_elevationSet = m_locationSpecified = m_utmSpecified = m_locationSet = m_utmSet = false;
}


CCWFGM_WeatherStation::CCWFGM_WeatherStation(const CCWFGM_WeatherStation &toCopy) {
	CRWThreadSemaphoreEngage engage(*(CRWThreadSemaphore *)&toCopy.m_lock, SEM_FALSE);

	m_bRequiresSave = false;
	m_latitude = toCopy.m_latitude;
	m_longitude = toCopy.m_longitude;
	m_elevation = toCopy.m_elevation;
	m_location = toCopy.m_location;
	m_locationSpecified = toCopy.m_locationSpecified;
	m_utmSpecified = toCopy.m_utmSpecified;
	m_locationSet = toCopy.m_locationSet;
	m_utmSet = toCopy.m_utmSet;

	std::uint32_t i = 0;
	StreamNode *sn = (StreamNode *)toCopy.m_streamList.LH_Head();
	while (sn->LN_Succ()) {
		boost::intrusive_ptr<ICWFGM_CommonBase> ws;
		sn->m_stream->Clone(&ws);
		CCWFGM_WeatherStream *wws = dynamic_cast<CCWFGM_WeatherStream *>(ws.get());
		if (wws)
			AddStream(wws, i++);
		sn = (StreamNode *)sn->LN_Succ();
	}
}


CCWFGM_WeatherStation::~CCWFGM_WeatherStation() {
	StreamNode *node;
	while ((node = (StreamNode *)m_streamList.RemHead()))
		delete node;
}

#endif

IMPLEMENT_OBJECT_CACHE_MT_NO_TEMPLATE(StreamNode, StreamNode, 16 * 1024 / sizeof(StreamNode), true, 16)


HRESULT CCWFGM_WeatherStation::get_GridEngine(boost::intrusive_ptr<ICWFGM_GridEngine> *pVal) {
	if (!pVal)								return E_POINTER;
	*pVal = m_gridEngine;
	if (!m_gridEngine) { weak_assert(false); return ERROR_WEATHER_STATION_UNINITIALIZED; }
	return S_OK;
}


HRESULT CCWFGM_WeatherStation::put_GridEngine(ICWFGM_GridEngine *newVal) {
	boost::intrusive_ptr<ICWFGM_GridEngine> pGridEngine;
	m_gridEngine = dynamic_cast<ICWFGM_GridEngine*>(const_cast<ICWFGM_GridEngine*>(newVal));
	if (newVal) {
		m_locationSet = m_utmSet = false;
		calculateXY();
		calculateLatLon();
	}
	return S_OK;
}


HRESULT CCWFGM_WeatherStation::put_CommonData(ICWFGM_CommonData* pVal) {
	StreamNode* sn = (StreamNode *)m_streamList.LH_Head();
	while (sn->LN_Succ()) {
		sn->m_stream->put_CommonData(pVal);
		sn = (StreamNode *)sn->LN_Succ();
	}
	return S_OK;
}


HRESULT CCWFGM_WeatherStation::Valid(const HSS_Time::WTime &start_time, const HSS_Time::WTimeSpan &duration) {
	if (!m_gridEngine) { weak_assert(false); return ERROR_WEATHER_STATION_UNINITIALIZED; }
	return S_OK;
}


HRESULT CCWFGM_WeatherStation::MT_Lock(bool exclusive, std::uint16_t obtain) {
	if (obtain == (std::uint16_t)-1) {
		std::int64_t state = m_lock.CurrentState();
		if (!state)				return SUCCESS_STATE_OBJECT_UNLOCKED;
		if (state < 0)			return SUCCESS_STATE_OBJECT_LOCKED_WRITE;
		if (state >= 1000000LL)	return SUCCESS_STATE_OBJECT_LOCKED_SCENARIO;
		return						   SUCCESS_STATE_OBJECT_LOCKED_READ;
	} else if (obtain) {
		if (exclusive)	m_lock.Lock_Write();
		else			m_lock.Lock_Read(1000000LL);
	} else {
		if (exclusive)	m_lock.Unlock();
		else			m_lock.Unlock(1000000LL);
	}
	return S_OK;
}


HRESULT CCWFGM_WeatherStation::Clone(/*[out]*/boost::intrusive_ptr<ICWFGM_CommonBase> *newObject) const {
	if (!newObject)						return E_POINTER;

	CRWThreadSemaphoreEngage engage(*(CRWThreadSemaphore *)&m_lock, SEM_FALSE);

	try {
		CCWFGM_WeatherStation *f = new CCWFGM_WeatherStation(*this);
		*newObject = f;
		return S_OK;
	}
	catch (std::exception& e) {
	}
	return E_FAIL;
}


HRESULT CCWFGM_WeatherStation::GetStreamCount(std::uint32_t *count) {
	if (!count)									return E_POINTER;
	CRWThreadSemaphoreEngage _semaphore_engage(m_lock, SEM_FALSE);
	*count = m_streamList.GetCount();
	return S_OK;
}


HRESULT CCWFGM_WeatherStation::AddStream(CCWFGM_WeatherStream *stream, std::uint32_t index) {
	if (!stream)									return E_POINTER;

	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)									return ERROR_SCENARIO_SIMULATION_RUNNING;

	StreamNode *node = (StreamNode *)m_streamList.LH_Head();
	while (node->LN_Succ()) {
		if (node->m_stream == stream)
			return ERROR_WEATHER_STREAM_ALREADY_ADDED;
		node = (StreamNode *)node->LN_Succ();
	}

	if ((index != (std::uint32_t)-1) && (index > m_streamList.GetCount()))
		return ERROR_WEATHER_STREAM_UNKNOWN;

	boost::intrusive_ptr<CCWFGM_WeatherStream> pStream;
	pStream = dynamic_cast<CCWFGM_WeatherStream *>(stream);
	if (pStream) {
		boost::intrusive_ptr<CCWFGM_WeatherStation> pStation;
		if (SUCCEEDED(pStream->get_WeatherStation(&pStation)))			return ERROR_WEATHER_STREAM_ALREADY_ASSIGNED;
		if (pStation)								return ERROR_WEATHER_STREAM_ALREADY_ASSIGNED;
		try {
			node = new StreamNode();
			node->m_stream = pStream;
			pStream->put_WeatherStation(0x12345678, this);
			if ((index == (std::uint32_t)-1) || (m_streamList.IsEmpty()))
				m_streamList.AddTail(node);
			else if (!index)
				m_streamList.AddHead(node);
			else {
				MinNode *mn = m_streamList.IndexNode(index - 1);
				m_streamList.Insert(node, mn);
			}
			m_bRequiresSave = true;
		} catch (std::bad_alloc& cme) {
			return E_OUTOFMEMORY;
		}
	}
	return S_OK;
}


HRESULT CCWFGM_WeatherStation::RemoveStream(CCWFGM_WeatherStream *stream) {
	if (!stream)								return E_POINTER;

	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)								return ERROR_SCENARIO_SIMULATION_RUNNING;

	StreamNode *node = (StreamNode *)m_streamList.LH_Head();
	while (node->LN_Succ()) {
		if (node->m_stream == stream) {
			node->m_stream->put_WeatherStation(0x12345678, NULL);
			m_streamList.Remove(node);
			delete node;
			m_bRequiresSave = true;
			return S_OK;
		}
		node = (StreamNode *)node->LN_Succ();
	}
	return ERROR_WEATHER_STREAM_UNKNOWN;
}


HRESULT CCWFGM_WeatherStation::StreamAtIndex(std::uint32_t index, boost::intrusive_ptr<CCWFGM_WeatherStream> *stream) {
	if (!stream)								return E_POINTER;
	CRWThreadSemaphoreEngage _semaphore_engage(m_lock, SEM_FALSE);
	if (index >= m_streamList.GetCount())					return ERROR_WEATHER_STREAM_UNKNOWN;
	StreamNode *node = (StreamNode *)m_streamList.IndexNode(index);
	*stream = node->m_stream;
	return S_OK;
}


HRESULT CCWFGM_WeatherStation::IndexOfStream(CCWFGM_WeatherStream *stream, std::uint32_t *index) {
	if ((!stream) || (!index))						return E_POINTER;

	CRWThreadSemaphoreEngage _semaphore_engage(m_lock, SEM_FALSE);
	StreamNode *node = (StreamNode *)m_streamList.LH_Head();
	std::uint32_t i = 0;
	while (node->LN_Succ()) {
		if (node->m_stream == stream) {
			*index = i;
			return S_OK;
		}
		node = (StreamNode *)node->LN_Succ();
		i++;
	}
	return ERROR_WEATHER_STREAM_UNKNOWN;
}


HRESULT CCWFGM_WeatherStation::GetAttribute(std::uint16_t option, PolymorphicAttribute *value) {
	if (!value)								return E_POINTER;

	CRWThreadSemaphoreEngage _semaphore_engage(m_lock, SEM_FALSE);

	switch (option) {
		case CWFGM_GRID_ATTRIBUTE_LATITUDE:					*value = m_latitude; return S_OK;
		case CWFGM_GRID_ATTRIBUTE_LONGITUDE:				*value = m_longitude; return S_OK;
		case CWFGM_GRID_ATTRIBUTE_DEFAULT_ELEVATION:		*value = m_elevation; if (!m_elevationSet) return ERROR_SEVERITY_WARNING; return S_OK;
		case CWFGM_GRID_ATTRIBUTE_DEFAULT_ELEVATION_SET:	*value = m_elevationSet; return S_OK;
		case CWFGM_ATTRIBUTE_LOAD_WARNING: {
							*value = m_loadWarning;
							return S_OK;
						   }
	}

	weak_assert(false);
	return E_INVALIDARG;
}


HRESULT CCWFGM_WeatherStation::SetAttribute(std::uint16_t option, const PolymorphicAttribute &var) {
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)								return ERROR_SCENARIO_SIMULATION_RUNNING;

	double value;
	HRESULT hr = E_INVALIDARG;

	switch (option) {
		case CWFGM_GRID_ATTRIBUTE_LATITUDE:
										if (FAILED(hr = VariantToDouble_(var, &value)))			break;
										if (value < DEGREE_TO_RADIAN(-90.0))					{ weak_assert(false); return E_INVALIDARG; }
										if (value > DEGREE_TO_RADIAN(90.0))						{ weak_assert(false); return E_INVALIDARG; }
										if (m_latitude != value) {
											m_latitude = value;
											resetStreams();
											m_locationSet = m_utmSet = false;
											m_locationSpecified = true;
											calculateXY();
											m_bRequiresSave = true;
										}
										return S_OK;

		case CWFGM_GRID_ATTRIBUTE_LONGITUDE:
										if (FAILED(hr = VariantToDouble_(var, &value)))			break;				
										if (value < DEGREE_TO_RADIAN(-180.0))					{ weak_assert(false); return E_INVALIDARG; }
										if (value > DEGREE_TO_RADIAN(180.0))					{ weak_assert(false); return E_INVALIDARG; }
										if (m_longitude != value) {
											m_longitude = value;
											resetStreams();
											m_locationSet = m_utmSet = false;
											m_locationSpecified = true;
											calculateXY();
										}
										m_bRequiresSave = true;
										return S_OK;

		case CWFGM_GRID_ATTRIBUTE_DEFAULT_ELEVATION:
										if (FAILED(hr = VariantToDouble_(var, &value)))			break;
										if (value != m_elevation) {
											m_elevation = value;
											m_elevationSet = true;
											resetStreams();
											m_bRequiresSave = true;
										}
										return S_OK;
	}

	weak_assert(false);
	return hr;
}


HRESULT CCWFGM_WeatherStation::SetLocation(const XY_Point &location) {
	if ((location.x != m_location.x) || (location.y != m_location.y)) {
		m_bRequiresSave = true;
		m_location.x = location.x;
		m_location.y = location.y;
		m_utmSpecified = true;
		m_locationSpecified = m_locationSet = false;
		calculateLatLon();
		if (m_locationSet)
			resetStreams();
	}
	return S_OK;
}


HRESULT CCWFGM_WeatherStation::GetLocation(XY_Point *location) {
	if (!location)					return E_POINTER;
	location->x = m_location.x;
	location->y = m_location.y;
	return (m_utmSet || m_utmSpecified) ? S_OK : E_FAIL;
}

#ifndef DOXYGEN_IGNORE_CODE

void CCWFGM_WeatherStation::resetStreams() {
	StreamNode *sn = (StreamNode *)m_streamList.LH_Head();
	while (sn->LN_Succ()) {
		sn->m_stream->put_WeatherStation(0x12345678, (CCWFGM_WeatherStation *)-1);
		sn = (StreamNode *)sn->LN_Succ();
	}
}

#endif
