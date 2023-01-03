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

#pragma once

#include "WeatherCOM.h"
#include "linklist.h"
#include "WTime.h"
#include "semaphore.h"
#include "FireEngine_ext.h"
#include "WeatherUtilities.h"
#include "valuecache_mt.h"
#include <map>

#include "FwiCom.h"
#include "CWFGM_WeatherStream.h"

#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#ifndef DOXYGEN_IGNORE_CODE

class GStreamNode : public MinNode {
    public:
	XY_Point m_location; // location of the weatherstation containing this stream, expressed in grid units
	double m_elevation; // elevation of the weatherstation containing this stream
	double m_Pe;		// atmospheric pressure at the station
	boost::intrusive_ptr<CCWFGM_WeatherStream> m_stream;

	DECLARE_OBJECT_CACHE_MT(GStreamNode, GStreamNode);
};

#endif


/**
	This object manages a collection weather data for a simulation engine's scenario.  It implements the GridEngine interface for communication with the simulation engine.
	Since it only handles weather data, its GridEngine property should refer to an object that can provide fuel type and elevation data, latitude, longitude, etc.
	In addition to the GridEngine interface, it also implements its own interface so that a client application may specify a collection of weather streams for use in a simulation.
	This object manages the spatial aspect of merging data together from multiple weather streams.  \n\n Only one weather stream from a weather station may be associated with a weather
	grid at a time when spatial weather interpolation is turned off.  This rule is imposed to avoid confusing situations to merge data from multiple streams.
	A stream cannot be attached to a weather grid until it is associated with a weather station, to ensure that it has a location.
*/
class WEATHERCOM_API CCWFGM_WeatherGrid : public ICWFGM_GridEngine, protected WeatherUtilities {

#ifndef DOXYGEN_IGNORE_CODE
public:
	CCWFGM_WeatherGrid();
	CCWFGM_WeatherGrid(const CCWFGM_WeatherGrid &toCopy);
	~CCWFGM_WeatherGrid();

#endif
public:
	virtual NO_THROW HRESULT Clone(boost::intrusive_ptr<ICWFGM_CommonBase> *newObject) const override;
	/**
		Returns the number of weather streams associated with this weather grid.
		\param	count	Number of streams.
		\retval	E_POINTER	The address provided for count is invalid.
		\retval	S_OK	Successful.
	*/
	virtual NO_THROW HRESULT GetStreamCount(std::uint32_t *count);
	/**
		Adds a weather stream to this weather grid.
		\param	stream	Weather stream object.
		\retval	E_POINTER	The address provided for stream is invalid, or the object trying to be added doesn't support the correct interfaces.
		\retval	S_OK	Successful.
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Cannot add a stream if the simulation is running.
		\retval	ERROR_WEATHER_STREAM_ALREADY_ADDED	The stream being added is already associated with this WeatherGrid object.
		\retval	ERROR_WEATHER_STREAM_NOT_ASSIGNED	The stream is not attached to a weather station (a stream must be associated with a station before it may be added to a weather grid).
		\retval	ERROR_WEATHER_STATION_ALREADY_PRESENT	There is already a stream from the new stream's weather station attached to this weather grid.
		\retval	E_OUTOFMEMORY	Insufficient memory.
		\retval	E_NOINTERFACE	Interface not supported.
	*/
	virtual NO_THROW HRESULT AddStream(CCWFGM_WeatherStream *stream);
	/**
		Removes an association between a weather stream and this weather grid.
		\param	stream	Weather stream object.
		\retval	E_POINTER	The address provided for stream is invalid.
		\retval	S_OK	Successful.
		\retval	ERROR_WEATHER_STREAM_UNKNOWN	The client program tries to remove a fire object that is unknown to this scenario object.
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Cannot remove a stream if the scenario simulation is running.
	*/
	virtual NO_THROW HRESULT RemoveStream(CCWFGM_WeatherStream *stream);
	/**
		Given an index value, returns a pointer to a stream associated with this grid.  Note that this value is expected to be different from the stream's index for its station.
		\param	index	Index to a weather stream
		\param	stream	Address to contain the specific requested weather stream.
		\retval	E_POINTER	The address provided for stream is invalid.
		\retval	S_OK	Successful.
		\retval	ERROR_WEATHER_STREAM_UNKNOWN	The index is out of range.
	*/
	virtual NO_THROW HRESULT StreamAtIndex(std::uint32_t index, boost::intrusive_ptr<CCWFGM_WeatherStream> *stream);
	/**
		Returns the index of 'stream' in this grid's set of weather streams.
		\param	stream	Weather stream object.
		\param	index	Index to the stream.
		\retval	E_POINTER	The address provided for stream or index is invalid.
		\retval	S_OK	Successful.
		\retval	ERROR_WEATHER_STREAM_UNKNOWN	The index is out of range.
	*/
	virtual NO_THROW HRESULT IndexOfStream(CCWFGM_WeatherStream *stream, std::uint32_t *index);
	/**
		Polymorphic.  This routine retrieves an attribute/option value given the attribute/option index.
		\param option	The attribute of interest.  Valid attributes are:
		<ul>
		<li><code>CWFGM_WEATHER_OPTION_ADIABATIC_IDW_EXPONENT_TEMP</code> 64-bit floating point.  Used when <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code> is set.  IDW power for interpolating temperature and dew point values.
		<li><code>CWFGM_WEATHER_OPTION_IDW_EXPONENT_WS</code> 64-bit floating point.  Used when <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND</code> and/or <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND_VECTOR</code> is set.  IDW power for interpolating WS values.
		<li><code>CWFGM_WEATHER_OPTION_IDW_EXPONENT_PRECIP</code>  64-bit floating point.  Used when <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_PRECIP</code> is set.  IDW power for interpolating precip values.
		<li><code>CWFGM_WEATHER_OPTION_IDW_EXPONENT_FWI</code>  64-bit floating point.  Used when <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code> is set.  IDW power for interpolating FWI values.
		<li><code>CWFGM_WEATHER_OPTION_FFMC_VANWAGNER</code>		Boolean.  Use the Van Wagner approach to calculating HFFMC values
		<li><code>CWFGM_WEATHER_OPTION_FFMC_LAWSON</code>		Boolean.  Use the Lawson approach to calculating HFFMC values
		</ul>
		\param value	Location for the retrieved value to be placed.
		\retval E_POINTER	value is NULL
		\retval E_INVALIDARG	unknown requested option
		\retval S_OK Success
	*/
	virtual NO_THROW HRESULT GetAttribute(std::uint16_t option,  PolymorphicAttribute *value);

	/**
		Changes the state of the object with respect to access rights.  When the object is used by an active simulation, it must not be modified.
		When the object is somehow modified, it must be done so in an atomic action to prevent concerns with arising from multithreading.
		Note that these locks are primarily needed to ensure that data contributing during a simulation is not modified while the simulation is executing.\n\n All routines in the
		ICWFGM_GridEngine interface are necessarily NOT multithreading safe (for performance) but other interfaces for a given COM object
		implementing this interface must be by specification.  Locking request is forwarded to the next lower object in the 'layerThread' layering, in
		additional to all attached ICWFGM_WeatherStream objects.\n\n
		In the event of an error, then locking is undone to reflect an error state.
		\param	layerThread		Handle for scenario layering/stack access, allocated from an ICWFGM_LayerManager COM object.  Needed.  It is designed to allow nested layering analogous to the GIS layers.
		\param	exclusive	true if the requester wants a write lock, false for read/shared access
		\param	obtain	true to obtain the lock, false to release the lock.  If this is false, then the 'exclusive' parameter must match the initial call used to obtain the lock.
		\sa ICWFGM_GridEngine::MT_Lock
		\retval	SUCCESS_STATE_OBJECT_UNLOCKED	Lock was released.
		\retval	SUCCESS_STATE_OBJECT_LOCKED_WRITE	Exclusive/write lock obtained.
		\retval	SUCCESS_STATE_OBJECT_LOCKED_SCENARIO	A scenario successfully required a lock for purposes of simulating.
		\retval	SUCCESS_STATE_OBJECT_LOCKED_READ	Shared/read lock obtained.
		\retval	S_OK	Successful
		\retval	ERROR_GRID_UNINITIALIZED	No path via layerThread can be determined to further determine successful locks.
	*/
	virtual NO_THROW HRESULT MT_Lock(Layer *layerThread,  bool exclusive,  std::uint16_t obtain) override;
	/**
		This filter object validates all associated weather streams with this simulation, and if successful, forwards the call to the next lower GIS layer determined by layerThread.
		It also ensures that the selection of weather streams is valid (no 2 streams from the same weather station, weather stations are appropriately spaced from each other, etc.)
		and all information needed to correctly initialize weather streams (e.g. latitude, longitude, timezone) is available and has been set.
		\param	layerThread		Handle for scenario layering/stack access, allocated from an ICWFGM_LayerManager COM object.  Needed.  It is designed to allow nested layering analogous to the GIS layers.
		\param	start_time	Start time of the simulation for which valid data must be available
		\param	duration	Duration of the simulation time for which valid data must be available
		\param option	Determines type of Valid request.
		\param application_count	Optional (dependent on option).  Array of counts for how often a particular type of grid occurs in the set of ICWFGM_GridEngine objects associated with a scenario.
		\retval S_OK		Successful.
		\retval	ERROR_GRID_UNINITIALIZED	Grid not initialized.
		\retval	ERROR_SEVERITY_WARNING	Grid validation failed based on options provided.
		\retval ERROR_GRID_WEATHER_INVALID_DATES	Invalid layerThread.
		\retval ERROR_WEATHER_STREAM_NOT_ASSIGNED	No weather streams are assigned to this object.
		\retval ERROR_GRID_PRIMARY_STREAM_UNSPECIFIED	Multiple weather streams exist, but none have been identified as the primary weather stream.
		\retval ERROR_GRID_WEATHER_STATION_ALREADY_PRESENT Muliple weather streams from the same weather station exists; invalid.
		\retval ERROR_GRID_WEATHERSTATIONS_TOO_CLOSE At least 2 weather stations are within 100m of each other
		\retval ERROR_GRID_WEATHER_INVALID_DATES At least one weather station cannot provide weather data for the time, duration specified
	*/
	virtual NO_THROW HRESULT Valid(Layer *layerThread,  const HSS_Time::WTime &start_time,  const HSS_Time::WTimeSpan &duration, std::uint32_t option, std::vector<uint16_t> *application_count) override;

	/**
		Polymorphic.  If layerThread is non-zero, then this filter object simply forwards the call to the next lower GIS
		layer determined by layerThread.  If layerthread is zero, then this object will interpret the call.
		\param	layerThread		Handle for scenario layering/stack access, allocated from an ICWFGM_LayerManager COM object.  Needed.  It is designed to allow nested layering analogous to the GIS layers.
		\param option	The attribute of interest.  Valid attributes are:
		<ul>
		<li><code>CWFGM_WEATHER_OPTION_ADIABATIC_IDW_EXPONENT_TEMP</code> 64-bit floating point.  Used when <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code> is set.  IDW power for interpolating temperature and dew point values.
		<li><code>CWFGM_WEATHER_OPTION_IDW_EXPONENT_WS</code> 64-bit floating point.  Used when <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND</code> and/or <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND_VECTOR</code> is set.  IDW power for interpolating WS values.
		<li><code>CWFGM_WEATHER_OPTION_IDW_EXPONENT_PRECIP</code>  64-bit floating point.  Used when <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_PRECIP</code> is set.  IDW power for interpolating precip values.
		<li><code>CWFGM_WEATHER_OPTION_IDW_EXPONENT_FWI</code>  64-bit floating point.  Used when <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code> is set.  IDW power for interpolating FWI values.
		<li><code>CWFGM_WEATHER_OPTION_FFMC_VANWAGNER</code>		Boolean.  Use the Van Wagner approach to calculating HFFMC values
		<li><code>CWFGM_WEATHER_OPTION_FFMC_LAWSON</code>		Boolean.  Use the Lawson approach to calculating HFFMC values
		</ul>
		\param value	Location for the retrieved value to be placed.
		\retval E_POINTER	value is NULL
		\retval	ERROR_GRID_UNINITIALIZED	No object in the grid layering to forward the request to.
		\retval S_OK Success
	*/
	virtual NO_THROW HRESULT GetAttribute(Layer *layerThread,  std::uint16_t option,  PolymorphicAttribute *value) override;
	/**
		Sets the value of an "option" to the value of the "value" variable provided.  Supported values for IDW exponents are from (0.0 to 10.0].  If 0.0 is provided for either wind
		or precipitation, then voronoi regions / theissen polygons are used instead of the IDW approach.
		\param	option	The weather option of interest.  Valid options are:
		<ul>
		<li><code>CWFGM_WEATHER_OPTION_ADIABATIC_IDW_EXPONENT_TEMP</code> 64-bit floating point.  Used when <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code> is set.  IDW power for interpolating temperature and dew point values.
		<li><code>CWFGM_WEATHER_OPTION_IDW_EXPONENT_WS</code> 64-bit floating point.  Used when <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND</code> and/or <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND_VECTOR</code> is set.  IDW power for interpolating WS values.
		<li><code>CWFGM_WEATHER_OPTION_IDW_EXPONENT_PRECIP</code>  64-bit floating point.  Used when <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_PRECIP</code> is set.  IDW power for interpolating precip values.
		<li><code>CWFGM_WEATHER_OPTION_IDW_EXPONENT_FWI</code>  64-bit floating point.  Used when <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code> is set.  IDW power for interpolating FWI values.
		</ul>
		\param	value	The value to set the option to.
		\retval	S_OK	Successful.
		\retval	ERROR_WEATHER_OPTION_INVALID	The weather option referred to is unknown.
		\retval	E_INVALIDARG	If value's value is incorrect.	
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Cannot set an attribute if the simulation is running.
		\retval	E_FAIL 	Failed.
	*/
	virtual NO_THROW HRESULT SetAttribute(std::uint16_t option,  const PolymorphicAttribute &value);
	virtual NO_THROW HRESULT GetAttributeData(Layer* layerThread, const XY_Point& pt, const HSS_Time::WTime& time, const HSS_Time::WTimeSpan& timeSpan, std::uint16_t option, std::uint64_t optionFlags, NumericVariant* attribute, grid::AttributeValue* attribute_valid, XY_Rectangle* cache_bbox);
	virtual NO_THROW HRESULT SetCache(Layer *layerThread, unsigned short cache, bool enable);
	/**
		This object calculates weather at location (x, y) at time 'time'.  'interpolate_method' determines various rules for how these calculations take place: if weather is to be temporally
		interpolated, spatially interpolated (and how), etc.  All weather and fwi calculations are performed (as requested and determined).  Note that some modes require a potentially long-
		duration recursive calculation to take place, which may take some time (and stack space) to perform.
		\param	layerThread		Handle for scenario layering/stack access, allocated from an ICWFGM_LayerManager COM object.  Needed.  It is designed to allow nested layering analogous to the GIS layers.
		\param	pt			Location.
		\param	time	A GMT time.
		\param	interpolate_method		Interpolation method identifier.  Valid bit-flag identifiers are:
		<ul>
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code> Boolean.  When true, spatial weather interpolation is turned on.  This option applies to both WX and FWI values, and will work whether there is 1 or more weather stations assigned to the scenario.  If false, then there should only be one weather stream.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_PRECIP</code> Boolean.  Conditional on <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code>.  When true, precipitation is spatially interpolated using IDW.  When false, precipitation from the primary weather stream is used.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND</code> Boolean.  Conditional on <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code>.  When true, wind is spatially interpolated (WS is defined using IDW, WD is chosen from the closest station); when false, wind from the primary weather stream is used.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND_VECTOR</code> Boolean.  Conditional on <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND</code>.  When true, wind is spatially interpolated (WS is defined using IDW, WD is chosen from the closest station); when false, wind from the primary weather stream is used.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_TEMP_RH</code> Boolean.  Conditional on <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code>.  When true, temperature, dew point temperature, and RH are calculated spatially from adiabatic lapse rates; when false, values from the primary weather stream are used.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_CALCFWI</code> Boolean.  If false, then the current FWI values are returned (possibly interpolated).  If true, then the current FWI values are calculated from the prior FWI values and the current weather values (likely spatially interpolated).
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_HISTORY</code> Boolean.  Conditional on <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code> and <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_CALCFWI</code>.  If true, then historical FWI values are calculated to try to attain equilibrium on FWI values.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_TEMPORAL</code> Boolean.  When true, temporal weather interpolation is turned on, for all of the WX and hourly/instantantaneous FWI calculations.
		</ul>
		\param	wx			Weather information.
		\param	ifwi		IFWI Information.
		\param	dfwi		DFWI Information.
		\retval	CWFGM_WEATHER_INITIAL_VALUES_ONLY	Given time, only initial weather values (used for starting FWI calculations) could be returned.	
		\retval ERROR_INVALID_DATA			Invalid latitude/longitude available for calculations.
		\retval E_INVALIDARG				Error in calculations (likely FWI calculations).
		\retval E_POINTER				Invalid wx, ifwi, or dfwi pointers.
		\retval S_OK					Calculations are successful
		\retval (ERROR_INVALID_STATE | ERROR_SEVERITY_WARNING)	No weather streams available, or no primary weather stream set, for calculations.
	*/
	virtual NO_THROW HRESULT GetWeatherData(Layer *layerThread,  const XY_Point & pt, const HSS_Time::WTime &time, std::uint64_t interpolate_method,
	    IWXData *wx, IFWIData *ifwi, DFWIData *dfwi, bool *wx_valid, XY_Rectangle *bbox_cache) override;

	/**
		This object calculates weather at the specified location for storage in the provided array(s) at time 'time'.  'interpolate_method' determines various rules for how these calculations take place: if weather is to be temporally
		interpolated, spatially interpolated (and how), etc.  All weather and fwi calculations are performed (as requested and determined).  Note that some modes require a potentially long-
		duration recursive calculation to take place, which may take some time (and stack space) to perform.
		\param	layerThread		Handle for scenario layering/stack access, allocated from an ICWFGM_LayerManager COM object.  Needed.  It is designed to allow nested layering analogous to the GIS layers.
		\param	min_pt		Minimum value (inclusive).
		\param	max_pt		Maximum value (inclusive).
		\param	scale		Scale (meters) that the array is defined for
		\param	time	A GMT time.
		\param	interpolate_method		Interpolation method identifier.  Valid bit-flag identifiers are:
		<ul>
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code> Boolean.  When true, spatial weather interpolation is turned on.  This option applies to both WX and FWI values, and will work whether there is 1 or more weather stations assigned to the scenario.  If false, then there should only be one weather stream.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_PRECIP</code> Boolean.  Conditional on <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code>.  When true, precipitation is spatially interpolated using IDW.  When false, precipitation from the primary weather stream is used.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND</code> Boolean.  Conditional on <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code>.  When true, wind is spatially interpolated (WS is defined using IDW, WD is chosen from the closest station); when false, wind from the primary weather stream is used.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND_VECTOR</code> Boolean.  Conditional on <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_WIND</code>.  When true, wind is spatially interpolated (WS is defined using IDW, WD is chosen from the closest station); when false, wind from the primary weather stream is used.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_TEMP_RH</code> Boolean.  Conditional on <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code>.  When true, temperature, dew point temperature, and RH are calculated spatially from adiabatic lapse rates; when false, values from the primary weather stream are used.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_CALCFWI</code> Boolean.  If false, then the current FWI values are returned (possibly interpolated).  If true, then the current FWI values are calculated from the prior FWI values and the current weather values (likely spatially interpolated).
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_HISTORY</code> Boolean.  Conditional on <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code> and <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_CALCFWI</code>.  If true, then historical FWI values are calculated to try to attain equilibrium on FWI values.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_TEMPORAL</code> Boolean.  When true, temporal weather interpolation is turned on, for all of the WX and hourly/instantantaneous FWI calculations.
		</ul>
		\param	wx		Array of Weather information.
		\param	ifwi		Array of Instantaneous FWI codes.
		\param	dfwi		Array of Daily FWI codes.
		\retval	ERROR_GRID_UNINITIALIZED	No object in the grid layering to forward the request to.
		\retval E_INVALIDARG	The array is not 2D, or is insufficient in size to contain the requested data
	*/
	virtual NO_THROW HRESULT GetWeatherDataArray( Layer *layerThread, const XY_Point &min_pt, const XY_Point &max_pt, double scale,const HSS_Time::WTime &time, std::uint64_t interpolate_method, 
	    IWXData_2d *wx, IFWIData_2d *ifwi, DFWIData_2d *dfwi, bool_2d *wx_valid) override;
	/**
		This method will query all valid, associated weather streams for the next time at which a specified weather event (recorded change in weather data) occurs.
		This filter object it will then forward the call to the next lower GIS layer determined by layerThread and combine results.
		\param	layerThread		Handle for scenario layering/stack access, allocated from an ICWFGM_LayerManager COM object.  Needed.  It is designed to allow nested layering analogous to the GIS layers.
		\param	flags	Calculation flags.  Valid bit-flag identifiers are:
		<ul>
		<li><code>CWFGM_GETEVENTTIME_FLAG_SEARCH_FORWARD</code> search forward in time
		<li><code>CWFGM_GETEVENTTIME_FLAG_SEARCH_BACKWARD</code> search backwards in time
		<li><code>CWFGM_GETEVENTTIME_QUERY_PRIMARY_WX_STREAM</code> only the primary weather stream may respond
		<li><code>CWFGM_GETEVENTTIME_QUERY_ANY_WX_STREAM</code> only weather streams may respond
		</ul>
		\param	from_time	A GMT time provided as seconds since January 1st, 1600.
		\param	next_event	A GMT time provided as seconds since January 1st, 1600, representing the time for the next event, based on 'flags'.
		\retval	ERROR_GRID_UNINITIALIZED	No object in the grid layering to forward the request to.
	*/
	virtual NO_THROW HRESULT GetEventTime(Layer *layerThread,  const XY_Point& pt, std::uint32_t flags, const HSS_Time::WTime &from_time, HSS_Time::WTime *next_event, bool* event_valid) override;
	/**
		This filter object handles caching of spatial weather data, then forwards the call to the next lower GIS layer determined by layerThread.
		\param	layerThread		Handle for scenario layering/stack access, allocated from an ICWFGM_LayerManager COM object.  Needed.  It is designed to allow nested layering analogous to the GIS layers.
		\param	time	A GMT time.
		\param	mode	Calculation mode.
		\param	parms	parameters to send and receive values from/to the simulation and grid objects, may be NULL
		\retval	ERROR_GRID_UNINITIALIZED	No object in the grid layering to forward the request to.
	*/
	virtual NO_THROW HRESULT PreCalculationEvent(Layer *layerThread, const HSS_Time::WTime &time, std::uint32_t mode, CalculationEventParms *parms) override;
	/**
		This filter object handles caching of spatial weather data, then forwards the call to the next lower GIS layer determined by layerThread.
		\param	layerThread		Handle for scenario layering/stack access, allocated from an ICWFGM_LayerManager COM object.  Needed.  It is designed to allow nested layering analogous to the GIS layers.
		\param	time	A GMT time.
		\param	mode	Calculation mode.
		\param	parms	parameters to send and receive values from/to the simulation and grid objects, may be NULL
		\retval	ERROR_GRID_UNINITIALIZED	No object in the grid layering to forward the request to.
	*/
	virtual NO_THROW HRESULT PostCalculationEvent(Layer *layerThread, const HSS_Time::WTime &time,  std::uint32_t mode, CalculationEventParms *parms) override;
	/**
		This method sets the next lower GIS layer for layerThread, used to determine the stacking and order of objects associated to a scenario.
		\param	layerThread		Handle for scenario layering/stack access, allocated from an ICWFGM_LayerManager COM object.  Needed.  It is designed to allow nested layering analogous to the GIS layers.
		\param	newVal	Pointer to new grid engine.
		\retval	E_NOINTERFACE Provided object (newVal) does not support the required ICWFGM_GridEngine interface (mis-cast)
		\retval S_OK Mapping assignment between layerThread and newval was successful.
	*/
	virtual NO_THROW HRESULT PutGridEngine(Layer *layerThread, ICWFGM_GridEngine * newVal) override;
	
	virtual NO_THROW HRESULT PutCommonData(/* [in] */ Layer* layerThread, /* [in] */ ICWFGM_CommonData* pVal) override;
	/**
		This property is very important for various aspects and options for spatial weather modelling.  It must be specified any time more than one stream is associated with this grid
		(and thus, a scenario).  Its affects will vary according to scenario options.  If only one stream is attached, then this value will automatically be set to that scenario.
		\param	stream	Value of PrimaryStream.
		\retval	E_POINTER	The address provided for pVal is invalid.
		\retval	S_OK	Successful.
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Cannot change PrimarySyream while scenario is running.
	*/
	virtual NO_THROW HRESULT get_PrimaryStream(boost::intrusive_ptr<CCWFGM_WeatherStream> *stream);
	/**
		This property is very important for various aspects and options for spatial weather modelling.  It must be specified any time more than one stream is associated with this grid
		(and thus, a scenario).  Its affects will vary according to scenario options.  If only one stream is attached, then this value will automatically be set to that scenario.
		\param	stream	Replacement value for PrimaryStream.
		\retval	E_POINTER	The address provided for pVal is invalid.
		\retval	S_OK	Successful.
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Cannot change PrimarySyream while scenario is running.
	*/
	virtual NO_THROW HRESULT put_PrimaryStream(CCWFGM_WeatherStream *stream);

#ifndef DOXYGEN_IGNORE_CODE
protected:
	boost::intrusive_ptr<CCWFGM_WeatherStream> m_primaryStream;	// RH: 20/08/2008: added
	CRWThreadSemaphore		m_lock, m_cacheLock;

	WTimeManager			*m_timeManager;
	MinListTempl<GStreamNode>	m_streamList;
	double				m_idwExponentFWI;
	double				m_idwExponentTemp; // exponent for IDW calculations in spatial interpolation
	double				m_idwExponentWS;
	double				m_idwExponentPrecip;
	std::uint16_t		m_xsize, m_ysize;

	std::uint16_t convertX(double x, XY_Rectangle *bbox);
	std::uint16_t convertY(double y, XY_Rectangle *bbox);
	double invertX(double x);
	double invertY(double y);
	double revertX(double x);
	double revertY(double y);
	HRESULT fixResolution();

private:
	virtual HRESULT GetRawWxValues(ICWFGM_GridEngine *grid, Layer *layerThread, const HSS_Time::WTime &time, const XY_Point &pt, std::uint64_t interpolate_method, IWXData *wx, bool *wx_valid);
	virtual HRESULT GetRawIFWIValues(ICWFGM_GridEngine *gridEngine, Layer *layerThread, const HSS_Time::WTime &time, const XY_Point &pt, std::uint64_t interpolate_method, std::uint32_t WX_SpecifiedBits, IFWIData *ifwi, bool *wx_valid);
	virtual HRESULT GetRawDFWIValues(ICWFGM_GridEngine *gridEngine, Layer *layerThread, const HSS_Time::WTime &time, const XY_Point &pt, std::uint64_t interpolate_method, std::uint32_t WX_SpecifiedBits, DFWIData *dfwi, bool *wx_valid);
#endif
};

#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(pop)
#endif
