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

#pragma once

#include "WeatherCOM.h"
#include "linklist.h"
#include "semaphore.h"
#include "points.h"


class CCWFGM_WeatherStream;
#include "ICWFGM_GridEngine.h"
#include "ICWFGM_Weather.h"
#include "ISerializeProto.h"
#include "cwfgmWeatherStation.pb.h"


#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#ifndef DOXYGEN_IGNORE_CODE
class StreamNode : public MinNode {
    public:
	boost::intrusive_ptr<CCWFGM_WeatherStream> m_stream;

	DECLARE_OBJECT_CACHE_MT(StreamNode, StreamNode);
};
#endif

/**
	This object maintains a collection of weather streams.  A variety of streams may co-exist to provide a useful comparison of results in different scenarios.
	It also supports the standard COM IPersistStream, IPersistStreamInit, and IPersistStorage interfaces.  Any weather stream is automatically saved with the station in order to simplify serialization operations in the client application.
	\n\n
	A weather stream must be associated with a station before being attached to a scenario.
*/
class WEATHERCOM_API CCWFGM_WeatherStation : public ICWFGM_CommonBase, public ISerializeProto {

public:
#ifndef DOXYGEN_IGNORE_CODE
	CCWFGM_WeatherStation();
	CCWFGM_WeatherStation(const CCWFGM_WeatherStation &toCopy);
	~CCWFGM_WeatherStation();

#endif
	/**
		Creates a new weather station with all the same properties of the object being called, returns a handle to the new object in 'newWeatherStation'.  Any weather streams associated with this object are also duplicated for the new station.
		\param	newWeatherStation	A weather station object.
		\sa ICWFGM_WeatherStation::Clone
		\retval	S_OK	Successful.
		\retval	E_POINTER	The address provided for newWeatherStation is invalid.
		\retval	E_OUTOFMEMORY	Insufficient memory.
		\retval	ERROR_SEVERITY_WARNING	File error or an unspecified failure.
		\retval	E_NOINTERFACE	Correct interface is not supported.
	*/
	virtual NO_THROW HRESULT Clone(boost::intrusive_ptr<ICWFGM_CommonBase> *newObject) const;
	/**
		Changes the state of the object with respect to access rights.  When the object is used by an active simulation, it must not be modified.
		When the object is somehow modified, it must be done so in an atomic action to prevent concerns with arising from multithreading.
		Note that these locks are primarily needed to ensure that data contributing during a simulation is not modified while the simulation is executing.\n\n All routines in the
		ICWFGM_GridEngine interface are necessarily NOT multithreading safe (for performance) but other interfaces for a given COM object
		implementing this interface must be by specification.\n\n
		In the event of an error, then locking is undone to reflect an error state.
		\param	exclusive	true if the requester wants a write lock, false for read/shared access
		\param	obtain	true to obtain the lock, false to release the lock.  If this is false, then the 'exclusive' parameter must match the initial call used to obtain the lock.
		\sa ICWFGM_WeatherStation::MT_Lock
		\retval	SUCCESS_STATE_OBJECT_UNLOCKED	Lock was released.
		\retval	SUCCESS_STATE_OBJECT_LOCKED_WRITE	Exclusive/write lock obtained.
		\retval	SUCCESS_STATE_OBJECT_LOCKED_SCENARIO	A scenario successfully required a lock for purposes of simulating.
		\retval	SUCCESS_STATE_OBJECT_LOCKED_READ	Shared/read lock obtained.
		\retval	S_OK	Successful
	*/
	virtual NO_THROW HRESULT MT_Lock(bool exclusive, std::uint16_t obtain);
	virtual NO_THROW HRESULT Valid(const HSS_Time::WTime &start_time, const HSS_Time::WTimeSpan &duration);
	/**
		Returns the number of streams associated with this weather station.  Any number of streams can be associated with a given weather station and be assigned different values, options, and time ranges.
		However, only one stream from a station can be associated with a scenario at a given time.
		\param	count	Number of streams.
		\sa ICWFGM_WeatherStation::GetStreamCount
		\retval	E_POINTER	The address provided for count is invalid.
		\retval	S_OK	Successful.
	*/
	virtual NO_THROW HRESULT GetStreamCount(std::uint32_t *count);
	/**
		Adds a weather stream to this weather station.  This stream may be in any state (initialized, newly created, or containing data).  Any calculated data in the weather stream will be marked invalid to use the location and time zone of this weather station.
		\param	stream	A weather stream object.
		\param index Index (0-based) for where to insert the stream into the set.
		\sa ICWFGM_WeatherStation::AddStream
		\retval	S_OK	Successful.
		\retval	E_OUTOFMEMORY
		\retval	Insufficient memory.
		\retval	E_NOINTERFACE	The object trying to be added (stream) doesn't support the correct interfaces, or if the pointer is invalid.
		\retval	ERROR_WEATHER_STREAM_ALREADY_ADDED	If the stream being added is already associated with this station.
		\retval	ERROR_WEATHER_STREAM_ALREADY_ASSIGNED	The stream is already attached to a weather station (a stream can only be associated with a single weather station at a time).
		\retval	ERROR_WEATHER_STREAM_UNKNOWN	The weather stream indexed could not be found. 
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Cannot add stream while the simulation is running.
	*/
	virtual NO_THROW HRESULT AddStream(CCWFGM_WeatherStream *stream, std::uint32_t index);
	/**
		Removes an association between a weather stream and this weather station.
		\param	stream	A weather stream object.
		\sa ICWFGM_WeatherStation::RemoveStream
		\retval	E_POINTER	The address provided for stream is invalid.
		\retval	S_OK	Successful.
		\retval	ERROR_WEATHER_STREAM_UNKNOWN	The client program tried to remove a stream object that is unknown to this station object.
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Cannot remove a stream while it is used in a running scenario.
	*/
	virtual NO_THROW HRESULT RemoveStream(CCWFGM_WeatherStream *stream);
	/**
		Given an index value, returns a pointer to a specific stream associated with this station.
		\param	index	Index to a weather stream
		\param	stream	Address to contain the specific requested weather stream.
		\sa ICWFGM_WeatherStation::StreamAtIndex
		\retval	E_POINTER	The address provided for stream is invalid.
		\retval	S_OK	Successful.
		\retval	ERROR_WEATHER_STREAM_UNKNOWN	The index is out of range.
	*/
	virtual NO_THROW HRESULT StreamAtIndex(std::uint32_t index, boost::intrusive_ptr<CCWFGM_WeatherStream> *stream);
	/**
		Returns the index of 'stream' in this station's set of streams.
		\param	stream	A weather stream object.
		\param	index	The index of stream.
		\sa ICWFGM_WeatherStation::IndexOfStream
		\retval	E_POINTER	The address provided for stream or index is invalid.
		\retval	S_OK	Successful.
		\retval	ERROR_WEATHER_STREAM_UNKNOWN	Stream is unknown to this weather station.
	*/
	virtual NO_THROW HRESULT IndexOfStream(CCWFGM_WeatherStream *stream, std::uint32_t *index);
	virtual NO_THROW HRESULT SetLocation(const XY_Point &pt);
	virtual NO_THROW HRESULT GetLocation(XY_Point *pt);
	/**
		Gets the value of an "option" and saves it in the "value" variable provided.
		\param	option	The option that you want the value of (Longitude, Latitude or Elevation).  Valid values are:
		<ul>
		<li><code>CWFGM_GRID_ATTRIBUTE_LATITUDE</code> 64-bit floating point, radians.
		<li><code>CWFGM_GRID_ATTRIBUTE_LONGITUDE</code> 64-bit floating point, radians.
		<li><code>CWFGM_GRID_ATTRIBUTE_DEFAULT_ELEVATION </code> 64-bit floating point, metres.  User specified elevation to use when there is no grid elevation available for at a requested grid location.
		<li><code>CWFGM_ATTRIBUTE_LOAD_WARNING</code>	BSTR.  Any warnings generated by the COM object when deserializating.
		</ul>
		\param	value	A pointer to a variable that you want the value to be saved in.
		\sa ICWFGM_WeatherStation::GetAttribute
		\retval	E_POINTER	The address provided for value is invalid.
		\retval	S_OK	Successful.
		\retval	E_INVALIDARG	Invalid arguments were passed.
	*/
	virtual NO_THROW HRESULT GetAttribute(std::uint16_t option, PolymorphicAttribute *value);
	/**
		Sets the value of an "option" to the value of the "value" variable provided.
		\param	option	The option that you want the value of (Longitude, Latitude or  Default Elevation).  Valid values are:
		<ul>
		<li><code>CWFGM_GRID_ATTRIBUTE_DEFAULT_ELEVATION</code> 64-bit floating point, metres.  User specified elevation to use when there is no grid elevation available for at a requested grid location.
		</ul>
		\param	value	The new value for the "option"
		\sa ICWFGM_WeatherStation::SetAttribute
		\retval	E_POINTER	The address provided for value is invalid.
		\retval	S_OK	Successful.
		\retval	E_INVALIDARG	Invalid arguments were passed.
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Cannot change attributes while a scenario is running.
		\retval	E_FAIL	Failed.
	*/
	virtual NO_THROW HRESULT SetAttribute(std::uint16_t option, const PolymorphicAttribute &value);
	/**
	Retrieves the object exposing the GridEngine interface that this weather station object may refer to, to use for tasks such as bounds clipping, etc.
	\param	pVal	Value of GridEngine.
	\sa ICWFGM_VectorEngine::GridEngine
	\retval	E_POINTER	The address provided for pVal is invalid, or upon setting pVal the pointer doesn't appear to belong to an object exposing the ICWFGM_GridEngine interface.
	\retval	S_OK	Successful.
	\retval	ERROR_WEATHER_STATION_UNINITIALIZED	The Grid Engine property has not be set.
	*/
	virtual NO_THROW HRESULT get_GridEngine(boost::intrusive_ptr<ICWFGM_GridEngine> *pVal);
	/**
	Sets the object exposing the GridEngine interface that this weather station object may refer to, to use for tasks such as bounds clipping, etc.
	\param	newVal	Replacement value for GridEngine.
	\sa ICWFGM_VectorEngine::GridEngine
	\retval	E_POINTER	The address provided for pVal is invalid, or upon setting pVal the pointer doesn't appear to belong to an object exposing the ICWFGM_GridEngine interface.
	\retval	S_OK	Successful.
	\retval	ERROR_WEATHER_STATION_UNINITIALIZED	The Grid Engine property has not be set.
	\retval	E_NOINTERFACE	The object provided does not implement the ICWFGM_GridEngine interface.
	*/
	virtual NO_THROW HRESULT put_GridEngine(ICWFGM_GridEngine * newVal);
	virtual NO_THROW HRESULT put_CommonData(ICWFGM_CommonData* pVal);

public:

#ifndef DOXYGEN_IGNORE_CODE
public:
	virtual std::int32_t serialVersionUid(const SerializeProtoOptions& options) const noexcept override;
	virtual WISE::WeatherProto::CwfgmWeatherStation* serialize(const SerializeProtoOptions& options) override;
	virtual CCWFGM_WeatherStation *deserialize(const google::protobuf::Message& proto, std::shared_ptr<validation::validation_object> valid, const std::string& name) override;
	virtual std::optional<bool> isdirty(void) const noexcept override { return m_bRequiresSave; }
	 
protected:
	boost::intrusive_ptr<ICWFGM_GridEngine>	m_gridEngine;
	double				m_latitude,
						m_longitude,
						m_elevation;
	XY_Point			m_location;
	bool				m_locationSet, m_utmSet, m_elevationSet,
						m_locationSpecified, m_utmSpecified;
	MinList				m_streamList;
	std::string			m_loadWarning;
	CRWThreadSemaphore	m_lock;
	bool				m_bRequiresSave;
	
	void resetStreams();

	void calculateXY();
	void calculateLatLon();
#endif

};

#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(pop)
#endif
