/**
 * WISE_Weather_Module: CWFGM_WeatherGrid.h
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
#include "CWFGM_WeatherGrid.h"
#include "GridCom_ext.h"
#include "results.h"
#include "convert.h"
#include "limits.h"
#include "vectors.h"
#include <vector>


#ifndef DOXYGEN_IGNORE_CODE

CCWFGM_WeatherGrid::CCWFGM_WeatherGrid() : WeatherUtilities(nullptr), m_timeManager(nullptr)
{
	m_idwExponentFWI	= 2.0;
	m_idwExponentTemp	= 2.0;
	m_idwExponentWS		= 2.0;
	m_idwExponentPrecip	= 2.0;

	m_xsize = m_ysize = (std::uint16_t)-1;
	m_converter.setGrid(-1.0, -1.0, -1.0);
}


CCWFGM_WeatherGrid::CCWFGM_WeatherGrid(const CCWFGM_WeatherGrid &toCopy) : WeatherUtilities(toCopy.m_timeManager), m_timeManager(toCopy.m_timeManager) {
	CRWThreadSemaphoreEngage engage(*(CRWThreadSemaphore *)&toCopy.m_lock, SEM_FALSE);

	m_tm = m_timeManager;

	m_idwExponentFWI = toCopy.m_idwExponentFWI;
	m_idwExponentTemp = toCopy.m_idwExponentTemp;
	m_idwExponentWS = toCopy.m_idwExponentWS;
	m_idwExponentPrecip = toCopy.m_idwExponentPrecip;

	m_converter.setGrid(toCopy.m_converter.resolution(), toCopy.m_converter.xllcorner(), toCopy.m_converter.yllcorner());
	m_xsize = toCopy.m_xsize;
	m_ysize = toCopy.m_ysize;

	GStreamNode *n = toCopy.m_streamList.LH_Head();
	while (n->LN_Succ()) {
		GStreamNode *node = new GStreamNode();
		node->m_stream = n->m_stream;
		node->m_stream->put_WeatherStation(0xfedcba98, NULL);
		node->m_elevation = n->m_elevation;
		node->m_location = n->m_location;
		node->m_Pe = n->m_Pe;
		m_streamList.AddTail(node);
		n = (GStreamNode *)n->LN_Succ();
	}
}


CCWFGM_WeatherGrid::~CCWFGM_WeatherGrid() {
	GStreamNode *node;
	while ((node = m_streamList.RemHead()))
		delete node;

	RemoveCache((Layer *)-1, 0);
	RemoveCache((Layer *)-1, 1);
}

#endif


IMPLEMENT_OBJECT_CACHE_MT_NO_TEMPLATE(GStreamNode, GStreamNode, 512 * 1024 / sizeof(GStreamNode), true, 16)


HRESULT CCWFGM_WeatherGrid::MT_Lock(Layer *layerThread, bool exclusive, std::uint16_t obtain) {
	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);
	if (!gridEngine)	{ weak_assert(false); return ERROR_GRID_UNINITIALIZED; }

	HRESULT hr;
	if (obtain == (std::uint16_t)-1) {
		std::int64_t state = m_lock.CurrentState();
		if (!state)				return SUCCESS_STATE_OBJECT_UNLOCKED;
		if (state < 0)			return SUCCESS_STATE_OBJECT_LOCKED_WRITE;
		if (state >= 1000000LL)	return SUCCESS_STATE_OBJECT_LOCKED_SCENARIO;
		return						   SUCCESS_STATE_OBJECT_LOCKED_READ;
	} else if (obtain) {
		if (exclusive)	m_lock.Lock_Write();
		else			m_lock.Lock_Read(1000000LL);

		GStreamNode *node = m_streamList.LH_Head();
		while (node->LN_Succ()) {
			hr = node->m_stream->MT_Lock(exclusive, obtain);
			node = (GStreamNode *)node->LN_Succ();
		}

		hr = gridEngine->MT_Lock(layerThread, exclusive, obtain);
	} else {
		hr = gridEngine->MT_Lock(layerThread, exclusive, obtain);

		GStreamNode *node = m_streamList.LH_Head();
		while (node->LN_Succ()) {
			hr = node->m_stream->MT_Lock(exclusive, obtain);
			node = (GStreamNode *)node->LN_Succ();
		}

		if (exclusive)	m_lock.Unlock();
		else			m_lock.Unlock(1000000LL);
	}
	return hr;
}


HRESULT CCWFGM_WeatherGrid::Clone(boost::intrusive_ptr<ICWFGM_CommonBase> *newObject) const {
	if (!newObject)						return E_POINTER;

	CRWThreadSemaphoreEngage engage(*(CRWThreadSemaphore *)&m_lock, SEM_FALSE);

	try {
		CCWFGM_WeatherGrid *f = new CCWFGM_WeatherGrid(*this);
		*newObject = f;
		return S_OK;
	}
	catch (std::exception& e) {
	}
	return E_FAIL;
}


HRESULT CCWFGM_WeatherGrid::PutGridEngine(Layer *layerThread, ICWFGM_GridEngine *newVal) {
	if (!layerThread) {
		if (newVal) {
			boost::intrusive_ptr<ICWFGM_GridEngine> pGridEngine;
			pGridEngine = dynamic_cast<ICWFGM_GridEngine *>(const_cast<ICWFGM_GridEngine *>(newVal));
			if (pGridEngine.get()) {
				m_rootEngine = pGridEngine;
				fixResolution();
				pGridEngine->GetDimensions(0, &m_xsize, &m_ysize);
				return S_OK;
			}
			return E_FAIL;
		} else {
			m_rootEngine = NULL;
			return S_OK;
		}
	}
	if (!m_layerManager)							{ weak_assert(false); return ERROR_GRID_UNINITIALIZED; }
	HRESULT hr;
	if (SUCCEEDED(hr = m_layerManager->PutGridEngine(layerThread, this, newVal))) {
	}
	
	return hr;
}


HRESULT CCWFGM_WeatherGrid::PutCommonData(Layer* layerThread, ICWFGM_CommonData* pVal) {
	if (!pVal)
		return E_POINTER;
	m_timeManager = pVal->m_timeManager;
	return S_OK;
}


HRESULT CCWFGM_WeatherGrid::GetStreamCount(std::uint32_t *count) {
	if (!count)									return E_POINTER;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_FALSE);
	*count = m_streamList.GetCount();
	return S_OK;
}


HRESULT CCWFGM_WeatherGrid::get_PrimaryStream(boost::intrusive_ptr<CCWFGM_WeatherStream> *stream) {
	if (!stream)									return E_POINTER;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_FALSE);
	*stream = m_primaryStream;
	return S_OK;
}


HRESULT CCWFGM_WeatherGrid::put_PrimaryStream(CCWFGM_WeatherStream *stream) {
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)									return ERROR_SCENARIO_SIMULATION_RUNNING;

	//Verify that the whether station has been added. if not, return fail.
	bool found = false;
	GStreamNode *node = m_streamList.LH_Head();
	if (stream) {
		while (node->LN_Succ()) {
			if (node->m_stream == stream)
			{
				found = true;
				break;
			}
			node = (GStreamNode *)node->LN_Succ();
		}
		if (!found)
			return ERROR_WEATHER_STREAM_UNKNOWN;
	}
	m_primaryStream = stream;
	return S_OK;
}


HRESULT CCWFGM_WeatherGrid::AddStream(CCWFGM_WeatherStream *stream) {
	if (!stream)									return E_POINTER;
//Check whether stream has been added. If so, return fail.

	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)									return ERROR_SCENARIO_SIMULATION_RUNNING;

	GStreamNode *node = m_streamList.LH_Head();
	while (node->LN_Succ()) {
		if (node->m_stream == stream)
											return ERROR_WEATHER_STREAM_ALREADY_ADDED;
		node = (GStreamNode *)node->LN_Succ();
	}

	boost::intrusive_ptr<CCWFGM_WeatherStream> pStream;
	pStream = dynamic_cast<CCWFGM_WeatherStream *>(stream);
	if (pStream) {
		boost::intrusive_ptr<CCWFGM_WeatherStation> pStation;
		if (FAILED(pStream->get_WeatherStation(&pStation)))			return ERROR_WEATHER_STREAM_NOT_ASSIGNED;
		if (!pStation)								return ERROR_WEATHER_STREAM_NOT_ASSIGNED;

		try {
			node = new GStreamNode();
			node->m_stream = pStream;
			node->m_stream->put_WeatherStation(0xfedcba98, NULL);		// increments the grid counter in the stream
			m_streamList.AddTail(node);
		} catch (std::bad_alloc& cme) {
			return E_OUTOFMEMORY;
		}

		return S_OK;
	}
	return E_FAIL;
}


HRESULT CCWFGM_WeatherGrid::RemoveStream(CCWFGM_WeatherStream *stream) {
	if (!stream)									return E_POINTER;

	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)									return ERROR_SCENARIO_SIMULATION_RUNNING;

	GStreamNode *node = m_streamList.LH_Head();
	while (node->LN_Succ()) {
		if (node->m_stream == stream) {
			node->m_stream->put_WeatherStation(0x0f1e2d3c, NULL);		// decrements the grid counter in the stream
			if (stream == m_primaryStream)
				m_primaryStream = NULL;
			m_streamList.Remove(node);
			delete node;
			return S_OK;
		}
		node = (GStreamNode *)node->LN_Succ();
	}
	return ERROR_WEATHER_STREAM_UNKNOWN;
}


HRESULT CCWFGM_WeatherGrid::StreamAtIndex(std::uint32_t index, boost::intrusive_ptr<CCWFGM_WeatherStream> *stream) {
	if (!stream)									return E_POINTER;

	CRWThreadSemaphoreEngage engage(m_lock, SEM_FALSE);
	if (index >= m_streamList.GetCount())						return ERROR_WEATHER_STREAM_UNKNOWN;
	GStreamNode *node = m_streamList.IndexNode(index);
	*stream = node->m_stream;
	return S_OK;
}


HRESULT CCWFGM_WeatherGrid::IndexOfStream(CCWFGM_WeatherStream *stream, std::uint32_t *index) {
	if ((!stream) || (!index))							return E_POINTER;

	CRWThreadSemaphoreEngage engage(m_lock, SEM_FALSE);
	GStreamNode *node = m_streamList.LH_Head();
	std::uint32_t i = 0;
	while (node->LN_Succ()) {
		if (node->m_stream == stream) {
			*index = i;
			return S_OK;
		}
		node = (GStreamNode *)node->LN_Succ();
		i++;
	}
	return ERROR_WEATHER_STREAM_UNKNOWN;
}

#ifndef DOXYGEN_IGNORE_CODE
struct stream_time {
	std::uint64_t start, end;
};
#endif

static int __cdecl stream_order(const void *elem1, const void *elem2) {
	const	stream_time	*s1 = (const stream_time *)elem1,
				*s2 = (const stream_time *)elem2;
	std::int64_t diff = s1->start - s2->start;
	if (diff < 0)	return -1;
	if (diff == 0)	return 0;
	return 1;
}


HRESULT CCWFGM_WeatherGrid::Valid(Layer *layerThread, const HSS_Time::WTime &start_time, const HSS_Time::WTimeSpan &duration, std::uint32_t option, /*[in,out,size_is(24*60*60)]*/std::vector<uint16_t> *application_count) {
	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);

	if (!gridEngine)							{ return ERROR_GRID_UNINITIALIZED; }
	HRESULT hr;

	if (!layerThread)
		hr = gridEngine->Valid(layerThread, start_time, duration, option, application_count);
	else
		hr = ERROR_GRID_WEATHER_INVALID_DATES;

	if (!(option & (~(1 << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE)))) {
		bool exists;
		{
			CRWThreadSemaphoreEngage engage(m_cacheLock, SEM_FALSE);
			if ((exists = CacheExists(layerThread, (option & (1 << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE)) ? true : false)))
				ClearCache(layerThread, (option & (1 << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE)) ? true : false);
										// have to just simply clear this out because most of these objects
										// don't know if weather streams have been added or removed (we could
										// check here but only here) - we can't try to be smart and just
										// compare against (e.g.) a flags field
		}

		if ((hr == ERROR_GRID_WEATHER_NOT_IMPLEMENTED) || (hr == ERROR_GRID_WEATHER_INVALID_DATES)) {

			if (!m_streamList.GetCount())					return ERROR_WEATHER_STREAM_NOT_ASSIGNED;

			if (m_primaryStream == NULL)
			{
				if (m_streamList.GetCount() == 1)
				{
					GStreamNode *streamNode = m_streamList.LH_Head();
					m_primaryStream = streamNode->m_stream;
				}
				else
				{
					return ERROR_GRID_PRIMARY_STREAM_UNSPECIFIED;
				}
			}
			weak_assert(m_primaryStream != NULL);	

			GStreamNode *node = m_streamList.LH_Head(), *n2;
			while (node->LN_Succ()) {
				boost::intrusive_ptr<CCWFGM_WeatherStation> station;
				node->m_stream->get_WeatherStation(&station);

				if (FAILED(hr = station->Valid(start_time, duration)))
					return hr;

				PolymorphicAttribute v;
				gridEngine->GetAttribute(nullptr, CWFGM_GRID_ATTRIBUTE_SPATIALREFERENCE, &v);

				/*POLYMORPHIC CHECK*/
				std::string csProject;
				try { csProject = std::get<std::string>(v); } catch (std::bad_variant_access&) { weak_assert(false); return ERROR_PROJECTION_UNKNOWN; };

				m_converter.SetSourceProjection(csProject.c_str());

				XY_Point loc;
				station->GetLocation(&loc);

#ifdef _DEBUG
HSS_PRAGMA_WARNING_PUSH
HSS_PRAGMA_GCC(GCC diagnostic ignored "-Wunused-but-set-variable")
				double lat, lon, xll, yll, res;
				station->GetAttribute(CWFGM_GRID_ATTRIBUTE_LATITUDE, &v);			VariantToDouble_(v, &lat);
				station->GetAttribute(CWFGM_GRID_ATTRIBUTE_LONGITUDE, &v);			VariantToDouble_(v, &lon);
				gridEngine->GetAttribute(0, CWFGM_GRID_ATTRIBUTE_XLLCORNER, &v);	VariantToDouble_(v, &xll);
				gridEngine->GetAttribute(0, CWFGM_GRID_ATTRIBUTE_YLLCORNER, &v);	VariantToDouble_(v, &yll);
				HSS_PRAGMA_WARNING_POP

				res = m_converter.resolution();

				weak_assert(res > 0.0);

				m_converter.start()
					.fromPoints(lon, lat, 0)
					.asLatLon()
					.startIsRadians()
					.endInUTM()
					.toPoints(&lon, &lat);

				weak_assert(fabs(loc.x - lon) < 1e-3);
				weak_assert(fabs(loc.y - lat) < 1e-3);
#endif

				node->m_location = loc;

				// Cache elevation and atmospheric pressure of this stream's station for fast lookup
				PolymorphicAttribute elev;
				double dElev;
				HRESULT hr1 = station->GetAttribute(CWFGM_GRID_ATTRIBUTE_DEFAULT_ELEVATION, &elev);
				weak_assert(hr1 == S_OK);

				/*POLYMORPHIC CHECK*/
				VariantToDouble_(elev, &dElev);

				station->GetAttribute(CWFGM_GRID_ATTRIBUTE_DEFAULT_ELEVATION_SET, &elev);
				bool bElev;
				
				VariantToBoolean_(elev, &bElev);

				if (bElev)
					node->m_elevation = dElev;// if elevation is unspecified, cache the result as 0, so the extreme elevation doesn't throw off adiabatic lapse rates
				else
					node->m_elevation = 0.0;

				const double L0 = 0.00649;
				const double power = (9.80665 * 0.0289644) / (8.316963 * L0);
				const double P0 = 101.325;
				const double T0 = 288.15;
				if (node->m_elevation == 0.0)
					node->m_Pe = P0;
				else	node->m_Pe = P0 * pow(T0 / (T0 + L0 * node->m_elevation), power);	// equation 10 from Neal's document

				node = (GStreamNode *)node->LN_Succ();
			}

			node = m_streamList.LH_Head();
			while (node->LN_Succ()) {
				boost::intrusive_ptr<CCWFGM_WeatherStation> station = nullptr;
				node->m_stream->get_WeatherStation(&station);
				boost::intrusive_ptr<CCWFGM_WeatherStation> st = station;
				n2 = (GStreamNode *)node->LN_Succ();
				while (n2->LN_Succ()) {
					boost::intrusive_ptr<CCWFGM_WeatherStation> station2;
					n2->m_stream->get_WeatherStation(&station2);
					if (st == station2)
						return ERROR_GRID_WEATHER_STATION_ALREADY_PRESENT;
					weak_assert(m_converter.resolution() > 0.0);
					double dist = n2->m_location.DistanceTo(node->m_location) * m_converter.resolution();
					if (dist <= 100.0) {
						return ERROR_GRID_WEATHERSTATIONS_TOO_CLOSE;
					}
					n2 = (GStreamNode *)n2->LN_Succ();
				}
				node = (GStreamNode *)node->LN_Succ();
			}

			HSS_Time::WTime l_start_time(start_time, m_timeManager);
			node = m_streamList.LH_Head();
			while (node->LN_Succ() != nullptr)
			{
				HSS_Time::WTime start_time2(m_timeManager);
				HSS_Time::WTimeSpan duration2;
				if (FAILED(hr = node->m_stream->GetValidTimeRange(&start_time2, &duration2))
					|| ((start_time2 > l_start_time) || ((l_start_time + duration) > (start_time2 + duration2))))
				{
					return ERROR_GRID_WEATHER_INVALID_DATES;
				}
				node = (GStreamNode*)node->LN_Succ();
			}

			struct stream_time *st = new stream_time[m_streamList.GetCount()];
			HSS_Time::WTime	start(m_timeManager), end(m_timeManager);
			HSS_Time::WTimeSpan	_duration;

			node = m_streamList.LH_Head();
			std::uint32_t i = 0;
			while (node->LN_Succ()) {			// get start & end times for all streams
				hr = node->m_stream->GetValidTimeRange(&start, &_duration);
				st[i].start = start.GetTotalMicroSeconds();
				st[i].end = start.GetTotalMicroSeconds() + _duration.GetTotalMicroSeconds();
				node = (GStreamNode *)node->LN_Succ();
				i++;
			}
			if (m_streamList.GetCount() > 1)
				qsort(st, m_streamList.GetCount(), sizeof(stream_time), stream_order);
									// sort the start & end time pairs in incremental order, based on start times

			WTime	startTime(start_time, m_timeManager),
				endTime(start_time + duration, m_timeManager);

			start = HSS_Time::WTime(st[0].start, m_timeManager, false);
			end = HSS_Time::WTime(st[0].end, m_timeManager, false);
			for (i = 1; i < m_streamList.GetCount(); i++) {	// for all stream start/end pairs...
				if (st[i].start > end.GetTotalMicroSeconds()) {		// if the progression of time isn't contigous, check this range to see if start/end
									// is good

					WTime	minStartTime(start),
						maxEndTime(end);
					if ((startTime >= minStartTime) && (endTime <= maxEndTime)) {
						delete [] st;
						return S_OK;		// it's in range so return success
					}
					start = WTime(st[i].start, m_timeManager, false);		// reset the range delimiters
					end = WTime(st[i].end, m_timeManager, false);
				} else if (end.GetTotalMicroSeconds() < st[i].end)		// bump the end of the range appropriately, when necessary
					end = WTime(st[i].end, m_timeManager, false);
			}
			WTime	minStartTime(start),
				maxEndTime(end);
			if ((startTime >= minStartTime) && (endTime < maxEndTime)) {
				delete [] st;
				return S_OK;				// need to do this check as was done in the for() loop
			}
			delete [] st;
			return ERROR_GRID_WEATHER_INVALID_DATES;	// no range of stream data's included the start/end times given
		}
	}
	return hr;
}


HRESULT CCWFGM_WeatherGrid::GetEventTime(Layer *layerThread, const XY_Point& pt, std::uint32_t flags, const HSS_Time::WTime &from_time, HSS_Time::WTime *next_event, bool* event_valid) {
	if (!next_event)							return E_POINTER;

	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);
	if (!gridEngine)							{ weak_assert(false); return ERROR_GRID_UNINITIALIZED; }
	HRESULT hr = S_OK;

	if (flags & (CWFGM_GETEVENTTIME_FLAG_SEARCH_SUNRISE | CWFGM_GETEVENTTIME_FLAG_SEARCH_SUNSET)) {
		return gridEngine->GetEventTime(layerThread, pt, flags, from_time, next_event, event_valid);
	}

	if (!(flags & (CWFGM_GETEVENTTIME_QUERY_PRIMARY_WX_STREAM | CWFGM_GETEVENTTIME_QUERY_ANY_WX_STREAM)))
		hr = gridEngine->GetEventTime(layerThread, pt, flags, from_time, next_event, event_valid);

	WTime next_event1(m_timeManager);
	std::uint32_t cnt = 0;
	GStreamNode *node = m_streamList.LH_Head();
	while (node->LN_Succ() != NULL)
	{
		HSS_Time::WTime n_event(*next_event, m_timeManager);
		if (!(flags & CWFGM_GETEVENTTIME_QUERY_PRIMARY_WX_STREAM) || (node->m_stream == m_primaryStream))
		{
			hr = node->m_stream->GetEventTime(flags, from_time, &n_event);
		}
		if (!cnt) {
			next_event1 = n_event;
			cnt++;
		} 
		else {
			if (flags & CWFGM_GETEVENTTIME_FLAG_SEARCH_BACKWARD) {
				if (n_event > next_event1)
					next_event1 = n_event;
			} else {
				if (n_event < next_event1)
					next_event1 = n_event;
			}
		}
		node = (GStreamNode *)node->LN_Succ();
	}

	next_event->SetTime(next_event1);
	return hr;
}


HRESULT CCWFGM_WeatherGrid::PreCalculationEvent(Layer *layerThread, const HSS_Time::WTime &time, std::uint32_t mode, /*[in, out]*/ CalculationEventParms *parms) {
	std::uint32_t *cnt;
	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread, &cnt);
	if (!gridEngine)							{ weak_assert(false); return ERROR_GRID_UNINITIALIZED; }

	weak_assert(m_converter.resolution() != -1.0);

	if ((mode & (~(1 << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE))) == 1) {
		(*cnt)++;
		CRWThreadSemaphoreEngage engage(m_cacheLock, SEM_FALSE);
		WTime t(time, m_timeManager);
		SetEquilibriumLimit(layerThread, (mode & (1 << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE)) ? true : false, t);
	}
	else {
		ClearCache(layerThread, (mode & (1 << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE)) ? true : false);
	}
	HRESULT hr = gridEngine->PreCalculationEvent(layerThread, time, mode, parms);
	return hr;
}


HRESULT CCWFGM_WeatherGrid::PostCalculationEvent(Layer *layerThread, const HSS_Time::WTime &time, std::uint32_t mode, /*[in, out]*/ CalculationEventParms *parms) {
	std::uint32_t *cnt;
	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread, &cnt);
	if (!gridEngine)							{ weak_assert(false); return ERROR_GRID_UNINITIALIZED; }

	if ((mode & (~(1 << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE))) == 1) {
		WTime t(time, m_timeManager);
		CRWThreadSemaphoreEngage engage(m_cacheLock, SEM_FALSE);
		PurgeOldCache(layerThread, (mode & (1 << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE)) ? true : false, t);
		(*cnt)--;
	} else {
		CRWThreadSemaphoreEngage engage(m_cacheLock, SEM_FALSE);
		ClearCache(layerThread, (mode & (1 << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE)) ? true : false);
	}

	HRESULT hr = gridEngine->PostCalculationEvent(layerThread, time, mode, parms);
	return hr;
}


HRESULT CCWFGM_WeatherGrid::GetAttribute(Layer *layerThread, std::uint16_t option, /*unsigned*/ PolymorphicAttribute *value) {
	HRESULT hr = GetAttribute(option, value);
	if (SUCCEEDED(hr))
		return hr;
	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);
	if (!gridEngine) { weak_assert(false); return ERROR_GRID_UNINITIALIZED; }

	return gridEngine->GetAttribute(layerThread, option, value);
}


HRESULT CCWFGM_WeatherGrid::GetAttributeData(Layer* layerThread, const XY_Point& pt, const HSS_Time::WTime& time, const HSS_Time::WTimeSpan& timeSpan, std::uint16_t option,
	std::uint64_t optionFlags, NumericVariant* attribute, grid::AttributeValue* attribute_valid, XY_Rectangle* cache_bbox) {
	if (option == CWFGM_WEATHER_OPTION_CUMULATIVE_RAIN) {
		boost::intrusive_ptr<CCWFGM_WeatherStream> s;
		if (m_streamList.GetCount() == 1)
			s = m_streamList.LH_Head()->m_stream;
		else
			s = m_primaryStream;
		double rain;
		HRESULT hr = s->GetCumulativePrecip(time, timeSpan, &rain);
		if (SUCCEEDED(hr)) {
			*attribute = rain;
			*attribute_valid = grid::AttributeValue::SET;
		}
		return hr;
	}
	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);
	if (!gridEngine) { weak_assert(false); return ERROR_GRID_UNINITIALIZED; }
	return gridEngine->GetAttributeData(layerThread, pt, time, timeSpan, option, optionFlags, attribute, attribute_valid, cache_bbox);
}


HRESULT CCWFGM_WeatherGrid::GetAttribute(std::uint16_t option, PolymorphicAttribute *var) {
	if (!var)								return E_POINTER;

	CRWThreadSemaphoreEngage engage(m_lock, SEM_FALSE);

	HRESULT hr = E_INVALIDARG;

	switch (option) {
		case CWFGM_WEATHER_OPTION_ADIABATIC_IDW_EXPONENT_TEMP:
			*var = m_idwExponentTemp;
			return S_OK;
		case CWFGM_WEATHER_OPTION_IDW_EXPONENT_WS:
			*var = m_idwExponentWS;
			return S_OK;
		case CWFGM_WEATHER_OPTION_IDW_EXPONENT_PRECIP:
			*var = m_idwExponentPrecip;
			return S_OK;
		case CWFGM_WEATHER_OPTION_IDW_EXPONENT_FWI:
			*var = m_idwExponentFWI;
			return S_OK;
		case CWFGM_WEATHER_OPTION_FFMC_VANWAGNER:
		case CWFGM_WEATHER_OPTION_FFMC_LAWSON:
			{
				boost::intrusive_ptr<CCWFGM_WeatherStream> s;
				if (m_streamList.GetCount() == 1)
					s = m_streamList.LH_Head()->m_stream;
				else
					s = m_primaryStream;
				return s->GetAttribute(option, var);
			}
	}

	return hr;
}


HRESULT CCWFGM_WeatherGrid::SetAttribute(std::uint16_t option, const PolymorphicAttribute &var) {
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)								return ERROR_SCENARIO_SIMULATION_RUNNING;

	double dValue;
	HRESULT hr = E_INVALIDARG;

	switch (option) {
		case CWFGM_WEATHER_OPTION_ADIABATIC_IDW_EXPONENT_TEMP:
			if (FAILED(hr = VariantToDouble_(var, &dValue)))					break;
			if ((dValue <= 0.0) || (dValue > 10.0))
				return ERROR_INVALID_PARAMETER;
			this->m_idwExponentTemp = dValue;
			return S_OK;
		case CWFGM_WEATHER_OPTION_IDW_EXPONENT_WS:
			if (FAILED(hr = VariantToDouble_(var, &dValue)))					break;
			if ((dValue < 0.0) || (dValue > 10.0))
				return ERROR_INVALID_PARAMETER;
			this->m_idwExponentWS = dValue;
			return S_OK;
		case CWFGM_WEATHER_OPTION_IDW_EXPONENT_PRECIP:
			if (FAILED(hr = VariantToDouble_(var, &dValue)))					break;
			if ((dValue < 0.0) || (dValue > 10.0))
				return ERROR_INVALID_PARAMETER;
			this->m_idwExponentPrecip = dValue;
			return S_OK;
		case CWFGM_WEATHER_OPTION_IDW_EXPONENT_FWI:
			if (FAILED(hr = VariantToDouble_(var, &dValue)))					break;
			if ((dValue <= 0.0) || (dValue > 10.0))
				return ERROR_INVALID_PARAMETER;
			this->m_idwExponentFWI = dValue;
			return S_OK;
	}

	weak_assert(false);
	return hr;
}
 

HRESULT CCWFGM_WeatherGrid::SetCache(Layer *layerThread, unsigned short cache, bool enable) {
	CRWThreadSemaphoreEngage engage(m_cacheLock, SEM_TRUE);

	if ((cache != 0) && (cache != 1))
		return E_INVALIDARG;

	if (enable) {
		if ((m_xsize != (std::uint16_t)-1) && (m_ysize != (std::uint16_t)-1)) {
			if (CacheExists(layerThread, cache)) {
				IncrementCache(layerThread, cache);
				return SUCCESS_CACHE_ALREADY_EXISTS;
			}
			AddCache(layerThread, cache, m_xsize, m_ysize);
			IncrementCache(layerThread, cache);
		}
	}
	else {
		if (!CacheExists(layerThread, cache))
			return ERROR_CACHE_NOT_FOUND;
		std::uint32_t cnt = DecrementCache(layerThread, cache);
		weak_assert(cnt < (std::uint32_t)-3);
		if (!cnt)
			RemoveCache(layerThread, cache);
	}
	return S_OK;
}


HRESULT CCWFGM_WeatherGrid::GetWeatherData(Layer *layerThread, const XY_Point &pt, const HSS_Time::WTime &time, std::uint64_t interpolate_method,
    IWXData *wx, IFWIData *ifwi, DFWIData *dfwi, bool *wx_valid, XY_Rectangle *bbox_cache) {
	std::uint16_t x = convertX(pt.x, bbox_cache);
	std::uint16_t y = convertY(pt.y, bbox_cache);
	if (x >= m_xsize)							return ERROR_GRID_LOCATION_OUT_OF_RANGE;
	if (y >= m_ysize)							return ERROR_GRID_LOCATION_OUT_OF_RANGE;

	IWXData wx2; IFWIData ifwi2; DFWIData dfwi2; bool wxv2;
	if (!wx)		wx = &wx2;
	if (!ifwi)		ifwi = &ifwi2;
	if (!dfwi)		dfwi = &dfwi2;
	if (!wx_valid)	wx_valid = &wxv2;

	HRESULT hr;
	weak_assert(m_streamList.GetCount());

	if (interpolate_method & (CWFGM_GETEVENTTIME_QUERY_PRIMARY_WX_STREAM)) {
		boost::intrusive_ptr<CCWFGM_WeatherStream> s = m_primaryStream;
		if (!s) {
			if (m_streamList.GetCount() != 1) {
				weak_assert(false);
				return ERROR_INVALID_STATE | ERROR_SEVERITY_WARNING;	// there's no primary weather stream!
			}
			s = m_streamList.LH_Head()->m_stream;
		}
		hr = s->GetInstantaneousValues(time, interpolate_method, wx, ifwi, dfwi);
		*wx_valid = SUCCEEDED(hr);
	} else {		// if spatial interpolating is enabled
		WTime tm(time, m_timeManager);
		WeatherKey key(x, y, tm, interpolate_method, layerThread);
		WeatherData data = {0};
		XY_Point p(pt);
		p.x = invertX(((double)x) + 0.5);
		p.y = invertY(((double)y) + 0.5);
		hr = GetCalculatedValues(this, layerThread, p, key, data);
		*wx = data.wx;
		*ifwi = data.ifwi;
		*dfwi = data.dfwi;
		*wx_valid = data.wx_valid;
	}

	return hr;
}


HRESULT CCWFGM_WeatherGrid::GetWeatherDataArray(Layer *layerThread, const XY_Point &min_pt, const XY_Point &max_pt, double scale, const HSS_Time::WTime &time, std::uint64_t interpolate_method, 
    IWXData_2d *wx, IFWIData_2d *ifwi, DFWIData_2d *dfwi, bool_2d *wx_valid) {

	std::uint16_t x_min = convertX(min_pt.x, nullptr), y_min = convertY(min_pt.y, nullptr);
	std::uint16_t x_max = convertX(max_pt.x, nullptr), y_max = convertY(max_pt.y, nullptr);
	if (x_min >= m_xsize)								return ERROR_GRID_LOCATION_OUT_OF_RANGE;
	if (y_min >= m_ysize)								return ERROR_GRID_LOCATION_OUT_OF_RANGE;
	if (x_max >= m_xsize)								return ERROR_GRID_LOCATION_OUT_OF_RANGE;
	if (y_max >= m_ysize)								return ERROR_GRID_LOCATION_OUT_OF_RANGE;
	if (min_pt.x > max_pt.x)							return E_INVALIDARG;
	if (min_pt.y > max_pt.y)							return E_INVALIDARG;

	std::uint32_t xdim = x_max - x_min + 1;
	std::uint32_t ydim = y_max - y_min + 1;
	if (wx)
	{
		const IWXData_2d::size_type *dims = wx->shape();
		if ((dims[0] < xdim) || (dims[1] < ydim)) return E_INVALIDARG;
	}
	if (ifwi)
	{
		const IFWIData_2d::size_type *dims = ifwi->shape();
		if ((dims[0] < xdim) || (dims[1] < ydim)) return E_INVALIDARG;
	}
	if (dfwi)
	{
		const DFWIData_2d::size_type *dims = dfwi->shape();
		if ((dims[0] < xdim) || (dims[1] < ydim)) return E_INVALIDARG;
	}
	if (wx_valid)
	{
		const bool_2d::size_type *dims = wx_valid->shape();
		if ((dims[0] < xdim) || (dims[1] < ydim)) return E_INVALIDARG;
	}

	GStreamNode *sn = m_streamList.LH_Head();
	if (!sn->LN_Succ())							return ERROR_INVALID_STATE | ERROR_SEVERITY_WARNING;

	std::uint32_t i = 0;
	std::uint16_t x, y;
	HRESULT hr = S_OK;

	if (interpolate_method & (CWFGM_GETEVENTTIME_QUERY_PRIMARY_WX_STREAM)) {
		boost::intrusive_ptr<CCWFGM_WeatherStream> s = m_primaryStream;
		if (!s) {
			if (m_streamList.GetCount() != 1) {
				weak_assert(false);
				return ERROR_INVALID_STATE | ERROR_SEVERITY_WARNING;	// there's no primary weather stream!
			}
			s = m_streamList.LH_Head()->m_stream;
		}

		IWXData _iwx;
		IFWIData _ifwi;
		DFWIData _dfwi;
		hr = s->GetInstantaneousValues(time, interpolate_method, &_iwx, &_ifwi, &_dfwi);

		if (SUCCEEDED(hr)) {
			for (y = y_min; y <= y_max; y++) {			// for every point that was requested...
				for (x = x_min; x <= x_max; x++, i++) {
					if (wx)			(*wx)[x - x_min][y - y_min] = _iwx;
					if (ifwi)		(*ifwi)[x - x_min][y - y_min] = _ifwi;
					if (dfwi)		(*dfwi)[x - x_min][y - y_min] = _dfwi;
					if (wx_valid)	(*wx_valid)[x - x_min][y - y_min] = true;
				}
			}
		}
	} else {
		// interpolation is turned on - we must compute each point individually
		for (y = y_min; y <= y_max; y++)	// for every point that was requested...
		{
			for (x = x_min; x <= x_max; x++, i++)
			{
				// get the interpolated instantaneous values for this grid cell
				XY_Point pt;
				pt.x = invertX(((double)x) + 0.5);
				pt.y = invertY(((double)y) + 0.5);
				WeatherKey key(x, y, time, interpolate_method, layerThread);
				WeatherData data = {0};
				hr = GetCalculatedValues(this, layerThread, pt, key, data);
				if (FAILED(hr))
				{
					return hr;
				}
				else
				{
					if (wx)			(*wx)[x - x_min][y - y_min] = data.wx;
					if (ifwi)		(*ifwi)[x - x_min][y - y_min] = data.ifwi;
					if (dfwi)		(*dfwi)[x - x_min][y - y_min] = data.dfwi;
					if (wx_valid)	(*wx_valid)[x - x_min][y - y_min] = data.wx_valid;
				}
			}
		}
	}
	return hr;

}


#ifndef DOXYGEN_IGNORE_CODE

// this routine returns spatially interpolated weather data for the specified time and location
HRESULT CCWFGM_WeatherGrid::GetRawWxValues(ICWFGM_GridEngine *grid, Layer *layerThread, const HSS_Time::WTime &time, const XY_Point &pt, std::uint64_t interpolate_method, IWXData *wx, bool *wx_valid) {
	std:uint16_t const alternate = (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE)) ? 1 : 0;
	const bool use_cache = (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_IGNORE_CACHE) ? false : true);

    #ifdef DEBUG
	WTime t(time);
	std::string theTime = t.ToString(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST | WTIME_FORMAT_ABBREV | WTIME_FORMAT_DATE | WTIME_FORMAT_TIME);
    #endif

	std::uint16_t x = convertX(pt.x, nullptr);
	std::uint16_t y = convertY(pt.y, nullptr);
	WeatherKey key(x, y, time, interpolate_method, layerThread);

	HIWXData iwx;
	if ((use_cache) && (m_cache.Retrieve(alternate, &key, &iwx, m_timeManager))) {
		*wx = iwx.wx;
		*wx_valid = iwx.wx_valid;
		return iwx.hr;
	}

	HRESULT hr = S_OK;

	double nearest_d = DBL_MAX; // the distance to the active weather station nearest this point
	double nearest_precip = 0.0; // the active weather station nearest this point
	double nearest_wd = 0.0, nearest_ws = 0.0, nearest_gust = 0.0;
	double elev = 0, slope_factor, slope_azimuth; // slope_factor, slope_azimuth are unused
	grid::TerrainValue elev_valid, terrain_valid;
	double weight_temp = 0.0, weight_ws = 0.0, weight_gust = 0.0, weight_precip = 0.0, d, ww; // the cumulative weight, and individual weights, and distances for each weather station, respectively
	XY_Vector wind_vector(0.0, 0.0), gust_vector(0.0, 0.0);

	XY_Point pt2(pt.x, pt.y);
	IWXData wx2;
	GStreamNode *sn = m_streamList.LH_Head();

	if (!m_primaryStream) {
		weak_assert(false);
		hr = ERROR_INVALID_STATE | ERROR_SEVERITY_WARNING;	// there's no primary weather stream!
		iwx.hr = hr;
		if (use_cache)
			m_cache.Store(alternate, &key, &iwx, m_timeManager);

		*wx_valid = false;
		return hr;
	}

	hr = m_primaryStream->GetInstantaneousValues(time, interpolate_method, wx, NULL, NULL);
	if ((FAILED(hr) || (hr == CWFGM_WEATHER_INITIAL_VALUES_ONLY))) {
		weak_assert(SUCCEEDED(hr));
		iwx.wx = *wx;
		iwx.wx_valid = SUCCEEDED(hr);
		iwx.hr = hr;
		if (use_cache)
			m_cache.Store(alternate, &key, &iwx, m_timeManager);

		*wx_valid = iwx.wx_valid;
		return hr;
	}

	if (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL)) {
		double wx_WindSpeed, wx_WindGust;
		std::uint32_t wind_cnt = 0, gust_cnt = 0;

		if (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_TEMP_RH)) {
			wx->Temperature = 0.0;
			wx->DewPointTemperature = 0.0;
		}

		if (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND)) {
			weak_assert(m_idwExponentWS == 2.0);			// RWB: for testing changes in #811 for Prometheus only, 2013/12/10

			if (m_idwExponentWS != 0.0) {
				wx_WindSpeed = 0.0;
				wx_WindGust = 0.0;
			}
		}

		if (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_PRECIP)) {
			weak_assert(m_idwExponentPrecip == 2.0);		// RWB: for testing changes in #811 for Prometheus only, 2013/12/10

			if (m_idwExponentPrecip != 0.0)
				wx->Precipitation = 0.0;
		}

		double wx__UALR = 0.0, wx__SALR = 0.0;

		while (sn->LN_Succ()) {
			d = sn->m_location.DistanceToSquared(pt2);	// we use DistanceToSquared so need to halve the power in the pow() calls below
			ww = (d > 1.0) ? (1.0 / d) : 5.0;

			// Lookup instantaneous weather conditions at this weather station
			if (FAILED(hr = sn->m_stream->GetInstantaneousValues(time, interpolate_method, &wx2, NULL, NULL)))
			{
				weak_assert(false);
				iwx.wx = *wx;
				iwx.wx_valid = false;
				iwx.hr = hr;
				if (use_cache)
					m_cache.Store(alternate, &key, &iwx, m_timeManager);

				*wx_valid = iwx.wx_valid;
				return hr;
			}

			if (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_TEMP_RH)) {

				double VPs = 0.6112 * pow(10.0, 7.5 * wx2.Temperature / (237.7 + wx2.Temperature));
				double VP = wx2.RH * VPs;

				double Rv = 0.622 * VP / (sn->m_Pe - VP);
				double Rvs = 0.622 * VPs / (sn->m_Pe - VPs);

				const double Lv = 2501000.0;
				const double R = 287.0;
				const double g = -9.80665;
				const double Cpd = 1005.7;
				const double e = 0.621885157;
				double temp_kelvin = UnitConvert::convertUnit(wx2.Temperature, STORAGE_FORMAT_KELVIN, STORAGE_FORMAT_CELSIUS);
				double numerator = 1.0 + (Lv * Rv) / (R * temp_kelvin);
				double denominator = Cpd + (Lv * Lv * Rv * e) / (R * (temp_kelvin * temp_kelvin));
				double UALR = g * numerator / denominator;

				numerator = 1.0 + (Lv * Rvs) / (R * temp_kelvin);
				denominator = Cpd + (Lv * Lv * Rvs * e) / (R * (temp_kelvin * temp_kelvin));
				double SALR = g * numerator / denominator;

				wx2.Temperature -= (UALR * sn->m_elevation);
				wx2.DewPointTemperature -= (SALR * sn->m_elevation);	// new math from Neal is K/m not K/km

				// Accumulate value for numerator and denominator used in IDW interpolation
				double ww_temp;		// if distance > 1.0 meter then IDW, if it's <= 1m, then bias (arbitrarily) hugely to this point
				if (m_idwExponentTemp != 0.0) {
					if (m_idwExponentTemp != 2.0)
						ww_temp = pow(ww, m_idwExponentTemp * 0.5);
					else
						ww_temp = ww;
				} else		ww_temp = 0.0;
				
				wx->Temperature += ww_temp * wx2.Temperature;
				wx->DewPointTemperature += ww_temp * wx2.DewPointTemperature;

				wx__UALR += ww_temp * UALR;
				wx__SALR += ww_temp * SALR;

				weight_temp += ww_temp;
			}

			if (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND)) {
				double ww_ws;
				if (m_idwExponentWS != 0.0) {
					if (m_idwExponentWS != 2.0)
						ww_ws = pow(ww, m_idwExponentWS * 0.5);
					else
						ww_ws = ww;
				} else
					ww_ws = 0.0;
			
				if (m_idwExponentWS != 0.0) {
					if (interpolate_method & (1ull << (CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND_VECTOR))) {
						double sin_wd, cos_wd;
						::sincos(wx2.WindDirection, &sin_wd, &cos_wd);
						wind_vector.x += cos_wd * wx2.WindSpeed * ww_ws;
						wind_vector.y += sin_wd * wx2.WindSpeed * ww_ws;
						if (wx2.SpecifiedBits & IWXDATA_SPECIFIED_WINDGUST) {
							gust_vector.x += cos_wd * wx2.WindGust * ww_ws;
							gust_vector.y += sin_wd * wx2.WindGust * ww_ws;
							gust_cnt++;
							weight_gust += ww_ws;
						}
					}
					else {
						if (wx2.WindSpeed != 0.0)
							wx_WindSpeed += ww_ws * wx2.WindSpeed;
						if (wx2.SpecifiedBits & IWXDATA_SPECIFIED_WINDGUST) {
							weak_assert(wx2.WindGust > 0.0);
							wx_WindGust += ww_ws * wx2.WindGust;
							gust_cnt++;
							weight_gust += ww_ws;
						}
					}
					weight_ws += ww_ws;
					wind_cnt++;
				}
			}

			if (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_PRECIP)) {
				double ww_precip;
				if (m_idwExponentPrecip != 0.0) {
					if (m_idwExponentPrecip != 2.0)
						ww_precip = pow(ww, m_idwExponentPrecip * 0.5);
					else
						ww_precip = ww;
				} else		ww_precip = 0.0;

				if (m_idwExponentPrecip != 0.0) {
					if (wx2.Precipitation != 0.0)
						wx->Precipitation += ww_precip * wx2.Precipitation;
					weight_precip += ww_precip;
				}
			}

			// Keep track of the nearest weather station so that we can use it for precipitation data later on
			if (d < nearest_d) // note: w = (1000000.0 / distance squared); hence, smaller distances yield larger values of w.
			{
				nearest_d = d;
				nearest_precip = wx2.Precipitation;
				nearest_wd = wx2.WindDirection;
				nearest_ws = wx2.WindSpeed;
				if (wx2.SpecifiedBits & IWXDATA_SPECIFIED_WINDGUST)
					nearest_gust = wx2.WindGust;
			}
		
			sn = (GStreamNode*)sn->LN_Succ();
		}

		// Apply IDW to get (dew point) temperature normalized to sea level
		// then use this coordinates' elevation and lapse rate to determine (dew point) temperature at the actual elevation
		if (FAILED(hr = this->GetElevationData(0, pt, true, &elev, &slope_factor, &slope_azimuth, &elev_valid, &terrain_valid, nullptr)) ||
			(elev_valid == grid::TerrainValue::NOT_SET) || (terrain_valid == grid::TerrainValue::NOT_SET))
		{
			weak_assert(false);
			iwx.wx = *wx;
			iwx.wx_valid = false;
			iwx.hr = hr;
			if (use_cache)
				m_cache.Store(alternate, &key, &iwx, m_timeManager);

			*wx_valid = iwx.wx_valid;
			return hr;
		}

		if (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_TEMP_RH)) {
			if (weight_temp != 0.0) {
				wx->Temperature		/= weight_temp; // get normalized, interpolated temperature
				wx->DewPointTemperature	/= weight_temp; // get normalized, interpolated dew point temperature
				wx__UALR		/= weight_temp;
				wx__SALR		/= weight_temp;
			}
			wx->Temperature		+= (wx__UALR * elev /* / 1000.0 */ ); // adjust for adiabatic lapse rate
			wx->DewPointTemperature	+= (wx__SALR * elev /* / 1000.0 */ ); // adjust for adiabatic lapse rate
		}

		if (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND)) {
			bool set_wd = false;
			if (wind_cnt > 1) {
				if (m_idwExponentWS != 0.0) {
					if (interpolate_method & (1ull << (CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND_VECTOR))) {
						double wd = wind_vector.atan();
						double ws = wind_vector.Length() / weight_ws;
						double gust = gust_vector.Length() / weight_gust;
						if (fabs(ws - wx->WindSpeed) > 1e-7) {
							wx->WindSpeed = ws;
							wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDSPEED;
						}
						if (gust_cnt > 0) {
							if (fabs(gust - wx->WindGust) > 1e-7) {
								wx->WindGust = ws;
								wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDGUST;
							}
						}
						set_wd = true;
						if (fabs(wx->WindDirection - wd) > 1e-7) {
							wx->WindDirection = wd; // use instantaneous wd from the nearest wx stream
							wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDDIRECTION;
						}
					}
					else {
						if ((wx_WindSpeed != 0.0) && (weight_ws != 0.0))
							wx_WindSpeed /= weight_ws;
						if ((wx_WindGust != 0.0) && (weight_gust != 0.0))
							wx_WindGust /= weight_gust;
						if (fabs(wx_WindSpeed - wx->WindSpeed) > 1e-7) {
							wx->WindSpeed = wx_WindSpeed;
							wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDSPEED;
						}
						if (fabs(wx_WindGust - wx->WindGust) > 1e-7) {
							wx->WindGust = wx_WindGust;
							wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDGUST;
						}
					}
				} else {
					if (wx->WindSpeed != nearest_ws) {
						wx->WindSpeed = nearest_ws;
						wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDSPEED;
					}
					if (wx->WindGust != nearest_gust) {
						wx->WindGust = nearest_gust;
						wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDGUST;
					}
				}

    #ifdef _DEBUG
			} else {
				if (m_idwExponentWS != 0.0) {
					if (interpolate_method & (1ull << (CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND_VECTOR))) {
						double wd = NORMALIZE_ANGLE_RADIAN(wind_vector.atan());
						double ws = wind_vector.Length() / weight_ws;
						double gust = gust_vector.Length() / weight_gust;
						if (fabs(ws - wx->WindSpeed) > 1e-7) {
							weak_assert(false);
							wx->WindSpeed = ws;
							wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDSPEED;
						}
						if (fabs(gust - wx->WindGust) > 1e-7) {
							weak_assert(false);;
							wx->WindGust = gust;
							wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDGUST;
						}
						set_wd = true;
						if (fabs(wx->WindDirection - wd) > 1e-7) {
							weak_assert(false);
							wx->WindDirection = wd; // use instantaneous wd from the nearest wx stream
							wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDDIRECTION;
						}
					}
					else {
						if ((wx_WindSpeed != 0.0) && (weight_ws != 0.0))
							wx_WindSpeed /= weight_ws;
						if ((wx_WindGust != 0.0) && (weight_gust != 0.0))
							wx_WindGust /= weight_gust;
						if (fabs(wx_WindSpeed - wx->WindSpeed) > 1e-7) {
							weak_assert(false);
							wx->WindSpeed = wx_WindSpeed;
							wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDSPEED;
						}
						if (fabs(wx_WindGust - wx->WindGust) > 1e-7) {
							wx->WindGust = wx_WindGust;
							wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDGUST;
						}
					}
				} else {
					if (wx->WindSpeed != nearest_ws) {
						weak_assert(false);
						wx->WindSpeed = nearest_ws;
						wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDSPEED;
					}
					if (wx->WindGust != nearest_gust) {
						weak_assert(false);;
						wx->WindGust = nearest_gust;
						wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDGUST;
					}
				}
    #endif

			}

			if (!set_wd) {
				weak_assert(nearest_d != DBL_MAX);	// there is always at least one stream, so some stream must be nearest!
				if (fabs(wx->WindDirection - nearest_wd) > 1e-7) {
					wx->WindDirection = nearest_wd; // use instantaneous wd from the nearest wx stream
					wx->SpecifiedBits |= IWXDATA_OVERRODE_WINDDIRECTION;
				}
			}
		}

		if (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_PRECIP)) {
			if (m_idwExponentPrecip != 0.0) {
				if (wx->Precipitation != 0.0)
					wx->Precipitation	/= weight_precip;
				wx->SpecifiedBits |= IWXDATA_OVERRODE_PRECIPITATION;
			} else {
				weak_assert(nearest_d != DBL_MAX); // there is always at least one stream, so some stream must be nearest!
				if (fabs(wx->Precipitation - nearest_precip) > 1e-7) {
					wx->Precipitation = nearest_precip; // use instantaneous precip from the nearest wx stream
					wx->SpecifiedBits |= IWXDATA_OVERRODE_PRECIPITATION;
				}
			}
		}

		if (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_TEMP_RH)) {
			// using interpolated "local" temp and dew point temp, compute corresponding relative humidity
			//
			// Eq1. VP  = 6.112 * 10 ^ (7.5 * Tdp / (237.7 + Tdp))
			// Eq2. VPs = 6.112 * 10 ^ (7.5 * T   / (237.7 + T))
			// Eq3. RH  = (VP / VPs) * 100%
			//
			// where,	RH  = relative humidity			(% )
			//			T   = temperature				(C)
			//			Tdp = dew point temperature		(C)
			//			VP  = actual vapor pressure		(millibars)
			//			VPs = saturation vapor pressure	(millibars)
			double VP  = 0.6112 * pow(10.0, 7.5 * wx->DewPointTemperature / (237.7 + wx->DewPointTemperature)); // actual vapor pressure (millibars)
			double VPs = 0.6112 * pow(10.0, 7.5 * wx->Temperature / (237.7 + wx->Temperature)); // saturation vapor pressure (millibars)
			double rh = (VP / VPs) * 1.0;
			wx->RH = (rh < 0.0) ? 0.0 : (rh > 1.0) ? 1.0 : rh; // rh is clipped to be between 0.0 and 1.0 (i.e., 0% and 100%)

			wx->SpecifiedBits |= IWXDATA_OVERRODE_TEMPERATURE | IWXDATA_OVERRODE_DEWPOINTTEMPERATURE | IWXDATA_OVERRODE_RH;
		}			// these weather inputs have been changed now
	}

	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine = m_gridEngine(layerThread);
	if (!gridEngine) {
		weak_assert(false);
		hr = ERROR_GRID_UNINITIALIZED;
		iwx.wx = *wx;
		iwx.wx_valid = false;
		iwx.hr = hr;
		if (use_cache)
			m_cache.Store(alternate, &key, &iwx, m_timeManager);

    #ifdef DEBUG_CACHE
		if (verify)
			weak_assert(iwx.hr == iwx2.hr);
    #endif

		*wx_valid = iwx.wx_valid;
		return hr;
	}

	HRESULT hr1 = gridEngine->GetWeatherData(layerThread, pt, time, interpolate_method, wx, NULL, NULL, wx_valid, nullptr);
	if (FAILED(hr1))
		if (hr1 != E_NOTIMPL) {
			iwx.wx = *wx;
			iwx.wx_valid = *wx_valid;
			iwx.hr = hr1;
			if (use_cache)
				m_cache.Store(alternate, &key, &iwx, m_timeManager);

    #ifdef DEBUG_CACHE
			if (verify)
				weak_assert(iwx.hr == iwx2.hr);
    #endif

			return hr1;
		}

	weak_assert(hr == S_OK);
	iwx.wx = *wx;
	iwx.wx_valid = true;
	iwx.hr = hr;
	if (use_cache)
		m_cache.Store(alternate, &key, &iwx, m_timeManager);

    #ifdef DEBUG_CACHE
	if (verify)
		weak_assert(!memcmp(&iwx, &iwx2, sizeof(iwx)));
    #endif

	*wx_valid = iwx.wx_valid;
	return hr;
}


// this routine gets spatially interpolated daily starting codes for the day specified by the parameter time.
// it works as follows: user specifies a time - we find the previous day and the start of the current day
// we lookup the daily starting codes from the previous day
// we get the spatially interpolated local weather conditions for this location, at start of the current day.
// we use yesterdays interpolated starting codes and todays interpolated weather to compute todays starting codes
HRESULT CCWFGM_WeatherGrid::GetRawDFWIValues(ICWFGM_GridEngine * /*grid*/, Layer *layerThread, const HSS_Time::WTime &time, const XY_Point &pt, std::uint64_t interpolate_method, std::uint32_t WX_SpecifiedBits, DFWIData *p_dfwi, bool *wx_valid) {
	const std::uint16_t alternate = (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE)) ? true : false;
	const bool use_cache = (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_IGNORE_CACHE) ? false : true);

    #ifdef DEBUG
	WTime t(time);
	std::string theTime = t.ToString(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST | WTIME_FORMAT_ABBREV | WTIME_FORMAT_DATE | WTIME_FORMAT_TIME);
    #endif

	const std::uint32_t bitmask = IWXDATA_OVERRODE_TEMPERATURE | IWXDATA_OVERRODE_RH | IWXDATA_OVERRODE_PRECIPITATION | IWXDATA_OVERRODE_WINDSPEED |
			      IWXDATA_OVERRODEHISTORY_TEMPERATURE | IWXDATA_OVERRODEHISTORY_RH | IWXDATA_OVERRODEHISTORY_PRECIPITATION | IWXDATA_OVERRODEHISTORY_WINDSPEED;

	if (WX_SpecifiedBits & bitmask)					// this lets us know if anything at all has been changed from the data provided by the weather stream - set if true
									// (and will thus be set when spatial wx is turned on anyway)
		if ((interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_CALCFWI)) || (!(interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL))))
			if ((interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_HISTORY)) && (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL)))
				return this->GetWeatherData(layerThread, pt, time, interpolate_method, NULL, NULL, p_dfwi, wx_valid, nullptr);

	HRESULT hr = S_OK;

	std::uint16_t x = convertX(pt.x, nullptr);
	std::uint16_t y = convertY(pt.y, nullptr);
	WeatherKey key(x, y, time, interpolate_method, layerThread);

	HDFWIData iwx;

    #ifdef DEBUG_CACHE
	HDFWIData iwx2;
	BOOL verify = false;
    #endif

	if ((use_cache) && (m_cache.Retrieve(alternate, &key, &iwx, m_timeManager))) {

    #ifdef DEBUG_CACHE
		iwx2 = iwx;
		verify = true;
    #else
		*p_dfwi = iwx.dfwi;
		*wx_valid = iwx.wx_valid;
		return iwx.hr;
    #endif

	}

	if ((m_streamList.GetCount() > 1) && (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL))) {

		double weight = 0.0, w, d; // the cumulative weight, individual weights, and distances for each weather station, respectively
		double res, res2; // the plot resolution and plot resolution squared

		XY_Point pt2(pt.x, pt.y);
		DFWIData dfwi2; // temp variables

		// clear out any garbage data
		p_dfwi->dBUI = 0.0;
		p_dfwi->dDC = 0.0;
		p_dfwi->dDMC = 0.0;
		p_dfwi->dFFMC = 0.0;
		p_dfwi->SpecifiedBits = 0;

		res = m_converter.resolution();
		weak_assert(res > 0.0);
		res2 = res * res;

		GStreamNode *sn = m_streamList.LH_Head();

		while (sn->LN_Succ() != NULL)
		{
			// Lookup daily starting codes
			if (FAILED(hr = sn->m_stream->GetInstantaneousValues(time, interpolate_method, NULL, NULL, &dfwi2)))
			{

				weak_assert(false);
				iwx.dfwi = *p_dfwi;
				iwx.wx_valid = false;
				iwx.hr = hr;
				m_cache.Store(alternate, &key, &iwx, m_timeManager);

				*wx_valid = iwx.wx_valid;
				return hr;
			}

			// Accumulate value for numerator and denominator used in IDW interpolation
			d = sn->m_location.DistanceToSquared(pt2) * res2;
			w = (d > 1.0) ? (1.0 / d) : 5.0;
			if (m_idwExponentFWI != 2.0)
				w = pow(w, m_idwExponentFWI * 0.5);

			p_dfwi->dFFMC	+= w * dfwi2.dFFMC;
			p_dfwi->dDMC	+= w * dfwi2.dDMC;
			p_dfwi->dDC	+= w * dfwi2.dDC;
			weight		+= w;

			sn = (GStreamNode*)sn->LN_Succ();
		}

		weak_assert(weight > 0.0);
		p_dfwi->dFFMC	/= weight; // get spatially interpolated FFMC
		p_dfwi->dDMC	/= weight; // get spatially interpolated DMC
		p_dfwi->dDC	/= weight; // get spatially interpolated DC
		if (FAILED(hr = m_fwi.BUI(p_dfwi->dDC, p_dfwi->dDMC, &(p_dfwi->dBUI)))) // get spatially interpolated BUI
		{
			weak_assert(false);
			iwx.dfwi = *p_dfwi;
			iwx.wx_valid = false;
			iwx.hr = hr;
			m_cache.Store(alternate, &key, &iwx, m_timeManager);

			return hr;
		}
	} else
		hr = m_primaryStream->GetInstantaneousValues(time, interpolate_method, NULL, NULL, p_dfwi);

	weak_assert(hr == S_OK || hr == CWFGM_WEATHER_INITIAL_VALUES_ONLY);
	iwx.dfwi = *p_dfwi;
	iwx.wx_valid = true;
	iwx.hr = hr;
	m_cache.Store(alternate, &key, &iwx, m_timeManager);

	*wx_valid = iwx.wx_valid;
	return hr;
}


HRESULT CCWFGM_WeatherGrid::GetRawIFWIValues(ICWFGM_GridEngine * /*gridEngine*/, Layer *layerThread, const HSS_Time::WTime &time, const XY_Point &pt, std::uint64_t interpolate_method, std::uint32_t WX_SpecifiedBits, IFWIData *ifwi, bool *wx_valid) {
	const std::uint16_t alternate = (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_ALTERNATE_CACHE)) ? true : false;
	const bool use_cache = (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_IGNORE_CACHE) ? false : true);

	const std::uint32_t bitmask = IWXDATA_OVERRODE_TEMPERATURE | IWXDATA_OVERRODE_RH | IWXDATA_OVERRODE_PRECIPITATION | IWXDATA_OVERRODE_WINDSPEED |
			      IWXDATA_OVERRODEHISTORY_TEMPERATURE | IWXDATA_OVERRODEHISTORY_RH | IWXDATA_OVERRODEHISTORY_PRECIPITATION | IWXDATA_OVERRODEHISTORY_WINDSPEED;

	if (WX_SpecifiedBits & bitmask)					// this lets us know if anything at all has been changed from the data provided by the weather stream - set if true
									// (and will thus be set when spatial wx is turned on anyway)
		if ((interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_CALCFWI)) || (!(interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL))))
			if ((interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_HISTORY)) && (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL)))
				return this->GetWeatherData(layerThread, pt, time, interpolate_method, NULL, ifwi, NULL, wx_valid, nullptr);

	std::uint16_t x = convertX(pt.x, nullptr);
	std::uint16_t y = convertY(pt.y, nullptr);
	WeatherKey key(x, y, time, interpolate_method, layerThread);

	HIFWIData iwx;

	if ((use_cache) && (m_cache.Retrieve(alternate, &key, &iwx, m_timeManager))) {

		*ifwi = iwx.ifwi;
		*wx_valid = iwx.wx_valid;
		return iwx.hr;
	}

	HRESULT hr;
	if ((m_streamList.GetCount() > 1) && (interpolate_method & (1ull << CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL))) {
		GStreamNode *sn = m_streamList.LH_Head();
		if (!sn->LN_Succ())
		{
			weak_assert(false);
			hr = (ERROR_INVALID_STATE | ERROR_SEVERITY_WARNING); // there are no streams in the stream list!
			iwx.ifwi = *ifwi;
			iwx.wx_valid = false;
			iwx.hr = hr;
			m_cache.Store(alternate, &key, &iwx, m_timeManager);

			*wx_valid = iwx.wx_valid;
			return hr;
		}
		double weight = 0.0, w, d; // the cumulative weight, and individual weights, and distances for each weather station, respectively

		XY_Point pt2(pt.x, pt.y);

		// clear out any garbage values
		ifwi->FFMC = 0.0;
		ifwi->FWI = 0.0;
		ifwi->ISI = 0.0;
		ifwi->SpecifiedBits = 0;

		IFWIData ifwi2;

		while (sn->LN_Succ() != NULL)
		{
			// Lookup hourly starting codes
			if (FAILED(hr = sn->m_stream->GetInstantaneousValues(time, interpolate_method, NULL, &ifwi2, NULL)))
			{
				weak_assert(false);
				iwx.ifwi = *ifwi;
				iwx.wx_valid = false;
				iwx.hr = hr;
				m_cache.Store(alternate, &key, &iwx, m_timeManager);

				*wx_valid = iwx.wx_valid;
				return hr;
			}

			// Accumulate value for numerator and denominator used in IDW interpolation
			d = sn->m_location.DistanceToSquared(pt2);// * res2;
			w =	(d > 1.0) ? (1.0 / d) : 5.0;
			if (m_idwExponentFWI != 1.0)
				w = pow(w, m_idwExponentFWI);
			
			ifwi->FFMC	+= w * ifwi2.FFMC;
			ifwi->FWI	+= w * ifwi2.FWI;
			ifwi->ISI	+= w * ifwi2.ISI;
			weight += w;

			sn = (GStreamNode*)sn->LN_Succ();
		}

		weak_assert(weight > 0.0);
		ifwi->FFMC	/= weight; // get spatially interpolated FFMC
		ifwi->FWI	/= weight; // get spatially interpolated FWI
		ifwi->ISI	/= weight; // get spatially interpolated ISI
	}
	else {
		hr = m_primaryStream->GetInstantaneousValues(time, interpolate_method, NULL, ifwi, NULL);

#ifdef _DEBUG
		weak_assert(ifwi->FFMC > 0.0);
		if (ifwi->FFMC <= 0.0) {
			hr = m_primaryStream->GetInstantaneousValues(time, interpolate_method, NULL, ifwi, NULL);
		}
#endif

	}

	weak_assert(hr == S_OK || hr == CWFGM_WEATHER_INITIAL_VALUES_ONLY);
	iwx.ifwi = *ifwi;
	iwx.wx_valid = true;
	iwx.hr = hr;
	m_cache.Store(alternate, &key, &iwx, m_timeManager);

	*wx_valid = iwx.wx_valid;
	return hr;
}

#endif


#ifndef DOXYGEN_IGNORE_CODE

HRESULT CCWFGM_WeatherGrid::fixResolution() {
	HRESULT hr;
	double gridResolution, gridXLL, gridYLL, temp;
	PolymorphicAttribute var;

	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine;
	if (!(gridEngine = m_gridEngine(nullptr)))					{ weak_assert(false); return ERROR_GRID_UNINITIALIZED; }

	/*POLYMORPHIC CHECK*/
	try {
		if (!m_timeManager) {
			weak_assert(false);
			ICWFGM_CommonData* data;
			if (FAILED(hr = gridEngine->GetCommonData(nullptr, &data))) return hr;
			m_timeManager = data->m_timeManager;
			m_tm = data->m_timeManager;
			m_cache.SetTimeManager(m_tm);
		}
		if (FAILED(hr = gridEngine->GetAttribute(nullptr, CWFGM_GRID_ATTRIBUTE_PLOTRESOLUTION, &var))) return hr;
		VariantToDouble_(var, &gridResolution);
		if (FAILED(hr = gridEngine->GetAttribute(nullptr, CWFGM_GRID_ATTRIBUTE_XLLCORNER, &var))) return hr;
		VariantToDouble_(var, &gridXLL);
		if (FAILED(hr = gridEngine->GetAttribute(nullptr, CWFGM_GRID_ATTRIBUTE_YLLCORNER, &var))) return hr;
		VariantToDouble_(var, &gridYLL);
	}
	catch (std::bad_variant_access&) {
		weak_assert(false);
		return ERROR_GRID_UNINITIALIZED;
	}

	m_converter.setGrid(gridResolution, gridXLL, gridYLL);

	return S_OK;
}

#endif

std::uint16_t CCWFGM_WeatherGrid::convertX(double x, XY_Rectangle *bbox) {
	double lx = x - m_converter.xllcorner();
	std::uint16_t cx = (std::uint16_t)(lx / m_converter.resolution());
	if (bbox) {
		double bx = cx * m_converter.resolution() + m_converter.xllcorner();
		if (bbox->m_min.x < bx)		bbox->m_min.x = bx;
		bx += m_converter.resolution();
		if (bbox->m_max.x > bx)		bbox->m_max.x = bx;
	}
	return cx;
}


std::uint16_t CCWFGM_WeatherGrid::convertY(double y, XY_Rectangle *bbox) {
	double ly = y - m_converter.yllcorner();
	std::uint16_t cy = (std::uint16_t)(ly / m_converter.resolution());
	if (bbox) {
		double by = cy * m_converter.resolution() + m_converter.yllcorner();
		if (bbox->m_min.y < by)		bbox->m_min.y = by;
		by += m_converter.resolution();
		if (bbox->m_max.y > by)		bbox->m_max.y = by;
	}
	return cy;
}


double CCWFGM_WeatherGrid::invertX(double x) { return x * m_converter.resolution() + m_converter.xllcorner(); }
double CCWFGM_WeatherGrid::invertY(double y) { return y * m_converter.resolution() + m_converter.yllcorner(); }
double CCWFGM_WeatherGrid::revertX(double x) { return (x - m_converter.xllcorner()) / m_converter.resolution(); }
double CCWFGM_WeatherGrid::revertY(double y) { return (y - m_converter.yllcorner()) / m_converter.resolution(); }
