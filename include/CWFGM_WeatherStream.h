/**
 * WISE_Weather_Module: CWFGM_WeatherStream.h
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

class CCWFGM_WeatherStream;

#include "ICWFGM_CommonBase.h"
#include "ICWFGM_Weather.h"
#include "linklist.h"
#include "WeatherStream.h"
#include "semaphore.h"
#include "valuecache_mt.h"
#include "WeatherUtilities.h"
#include "ISerializeProto.h"
#include "cwfgmWeatherStream.pb.h"

#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(push, 8)
#endif

class DailyCondition;


/**
	This object contains weather data for a series of consecutive days, so is such called a weather stream.  It has a start time and duration, and the user
	can inspect and modify various daily or hourly values.  Each day of weather data is complete unto itself. The user may also specify alpha, beta, and
	gamma for Judi Beck's diurnal weather calculations. It also supports the standard COM IPersistStream, IPersistStreamInit, and IPersistStorage
	interfaces.  Any weather stream is automatically saved with the station in order to simplify serialization operations in the client application.\n\n
	A weather stream must be associated with a weather station to be selected into a scenario.
*/
class WEATHERCOM_API CCWFGM_WeatherStream : public ICWFGM_CommonBase, public ISerializeProto {
public:
#ifndef DOXYGEN_IGNORE_CODE
	CCWFGM_WeatherStream();
	CCWFGM_WeatherStream(const CCWFGM_WeatherStream &toCopy);
	~CCWFGM_WeatherStream();
		friend class CWFGM_WeatherStreamHelper;
#endif

public:
	/**
		This method is used to (only) retrieve which weather station a weather stream is associated with.
		\param	pVal	A weather station object.
		\sa ICWFGM_WeatherStream::WeatherStation
		\retval	E_POINTER	The address provided for pVal is invalid.
		\retval	S_OK	Successful.
		\retval	ERROR_WEATHER_STREAM_NOT_ASSIGNED	The stream is not associated with any station (pVal is set to NULL).
	*/
	virtual NO_THROW HRESULT get_WeatherStation(boost::intrusive_ptr<CCWFGM_WeatherStation> *pVal);
	/**
		This method is a private method to only be called by a weather station or a weather grid.  It is used to build associations between these objects.
		\param	key	Internal value.
		\param	newVal	Internal value.
		\sa ICWFGM_WeatherStream::WeatherStation
		\retval	E_NOINTERFACE	This is returned if the 'put' method should not be called directly or if any of the parameters are invalid.
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Association could not be built as the simulation is currently running.
		\retval	ERROR_WEATHER_STREAM_ALREADY_ASSIGNED	Association already exists.
		\retval	S_OK	Successful.
	*/
	virtual NO_THROW HRESULT put_WeatherStation(long key, CCWFGM_WeatherStation * newVal);
	virtual NO_THROW HRESULT put_CommonData(ICWFGM_CommonData* pVal);
	/**
		Changes the state of the object with respect to access rights.  When the object is used by an active simulation, it must not be modified.
		When the object is somehow modified, it must be done so in an atomic action to prevent concerns with arising from multithreading.
		Note that these locks are primarily needed to ensure that data contributing during a simulation is not modified while the simulation is executing.\n\n
		Locking request is forwarded to the attached ICWFGM_WeatherStation object.\n\n
		In the event of an error, then locking is undone to reflect an error state.
		\param	exclusive	true if the requester wants a write lock, false for read/shared access
		\param	obtain	true to obtain the lock, false to release the lock.  If this is false, then the 'exclusive' parameter must match the initial call used to obtain the lock.
		\sa ICWFGM_WeatherStream::MT_Lock
		\sa ICWFGM_WeatherStation::MT_Lock
		\retval	SUCCESS_STATE_OBJECT_UNLOCKED	Lock was released.
		\retval	SUCCESS_STATE_OBJECT_LOCKED_WRITE	Exclusive/write lock obtained.
		\retval	SUCCESS_STATE_OBJECT_LOCKED_SCENARIO	A scenario successfully required a lock for purposes of simulating.
		\retval	SUCCESS_STATE_OBJECT_LOCKED_READ	Shared/read lock obtained.
		\retval	S_OK	Successful
	*/
	virtual NO_THROW HRESULT MT_Lock(bool exclusive, std::uint16_t obtain);
	/**
		Creates a new weather stream with all the same properties and data of the object being called, returns a handle to the new object in 'newWeatherStream'.
		\param	newWeatherStream	A weather stream object.
		\sa ICWFGM_WeatherStream::Clone
		\retval	E_POINTER	The address provided for "newWeatherStream" is invalid.
		\retval	S_OK	Successful.
		\retval	E_OUTOFMEMORY	Insufficient memory.
		\retval	ERROR_SEVERITY_WARNING	Unspecified failure.
		\retval	E_NOINTERFACE	Internal serialization issue.
	*/
	virtual NO_THROW HRESULT Clone(boost::intrusive_ptr<ICWFGM_CommonBase> *newObject) const;
	/**
		Gets the value of an "option" and saves it in the "value" variable provided.
		\param	option	The weather option of interest.  Valid values are:
		<ul>
		<li><code>CWFGM_ATTRIBUTE_LOAD_WARNING</code>	BSTR.  Any warnings generated by the COM object when deserializating.
		<li><code>CWFGM_WEATHER_OPTION_WARNONSUNRISE</code>		Boolean.  true if sunrise could not be calculated.
		<li><code>CWFGM_WEATHER_OPTION_WARNONSUNSET</code>		Boolean.  true if sunset could not be calculated.
		<li><code>CWFGM_WEATHER_OPTION_FFMC_VANWAGNER</code>		Boolean.  Use the Van Wagner approach to calculating HFFMC values.  Also forces the object to ignore any provided FWI values (for consistency).
		<li><code>CWFGM_WEATHER_OPTION_FFMC_LAWSON</code>		Boolean.  Use the Lawson approach to calculating HFFMC values.  Also forces the object to ignore any provided FWI values (for consistency).
		<li><code>CWFGM_WEATHER_OPTION_FWI_USE_SPECIFIED</code>		Boolean.  Use any/all FWI values provided in the input file.
		<li><code>CWFGM_WEATHER_OPTION_ORIGIN_FILE</code>		Boolean.  Indicates whether the weather stream data origininated from an imported file.
		<li><code>CWFGM_WEATHER_OPTION_FWI_ANY_SPECIFIED</code>		Boolean.  Indicates whether at least one entry in the inputted weather file had any FWI codes specified.
		<li><code>CWFGM_WEATHER_OPTION_TEMP_ALPHA</code>		64-bit floating point.  The 'alpha' value for Beck et. al. diurnal equations for temperature.
		<li><code>CWFGM_WEATHER_OPTION_TEMP_BETA</code>			64-bit floating point.  The 'beta' value for Beck et. al. diurnal equations for temperature.
		<li><code>CWFGM_WEATHER_OPTION_TEMP_GAMMA</code>		64-bit floating point.  The 'gamma' value for Beck et. al. diurnal equations for temperature.
		<li><code>CWFGM_WEATHER_OPTION_WIND_ALPHA</code>		64-bit floating point.  The 'alpha' value for Beck et. al. diurnal equations for wind.
		<li><code>CWFGM_WEATHER_OPTION_WIND_BETA</code>			64-bit floating point.  The 'beta' value for Beck et. al. diurnal equations for wind.
		<li><code>CWFGM_WEATHER_OPTION_WIND_GAMMA</code>		64-bit floating point.  The 'gamma' value for Beck et. al. diurnal equations for wind.
		<li><code>CWFGM_WEATHER_OPTION_INITIAL_FFMC</code>		64-bit floating point.  Initial FFMC value for daily FWI calculations.
		<li><code>CWFGM_WEATHER_OPTION_INITIAL_HFFMC</code>		64-bit floating point.  Initial hourly FFMC value for hourly FWI calculations.
		<li><code>CWFGM_WEATHER_OPTION_INITIAL_DC</code>		64-bit floating point.  Initial DC value for daily FWI calculations.
		<li><code>CWFGM_WEATHER_OPTION_INITIAL_DMC</code>		64-bit floating point.  Initial DMC value for daily FWI calculations.
		<li><code>CWFGM_WEATHER_OPTION_INITIAL_BUI</code>		64-bit floating point.  Optional initial BUI value.  If not provided, then BUI is calculated from the initial DC, DMC values.  Use -99.0 to clear the specified value.
		<li><code>CWFGM_WEATHER_OPTION_INITIAL_RAIN</code>		64-bit floating point.  Initial rain for the prior 11/12 hours.  Only needed for Van Wagner HFFMC calculations.
		<li><code>CWFGM_GRID_ATTRIBUTE_LATITUDE</code> 64-bit floating point, radians.
		<li><code>CWFGM_GRID_ATTRIBUTE_LONGITUDE</code> 64-bit floating point, radians.
		<li><code>CWFGM_WEATHER_OPTION_INITIAL_HFFMCTIME</code>	64-bit signed integer.  Units are in seconds.  Hour at which the initial HFFMC value is observed.  HFFMC values will be calculated forward and back in time from this value.
		<li><code>CWFGM_GRID_ATTRIBUTE_TIMEZONE_ID</code>	32-bit unsigned integer.  A unique ID for a pre-defined set of timezone settings. The timezone information can be retrieved using <code>WorldLocation::TimeZoneFromId</code>.
		<li><code>CWFGM_GRID_ATTRIBUTE_TIMEZONE</code>		64-bit signed integer.  Units are in seconds, relative to GMT.  For example, MST (Mountain Standard Time) would be -6 * 60 * 60 seconds.  Valid values are from -12 hours to +12 hours.
		<li><code>CWFGM_GRID_ATTRIBUTE_DAYLIGHT_SAVINGS</code>	64-bit signed integer.  Units are in seconds.  Amount of correction to apply for daylight savings time.
		<li><code>CWFGM_GRID_ATTRIBUTE_DST_START</code>	64-bit unsigned integer.  Units are in seconds.  Julian date determining when daylight savings starts within the calendar year.
		<li><code>CWFGM_GRID_ATTRIBUTE_DST_END</code>	64-bit unsigned integer.  Units are in seconds.  Julian date determining when daylight savings ends within the calendar year.
		<li><code>CWFGM_WEATHER_OPTION_START_TIME</code>	64-bit unsigned integer.  GMT time provided as seconds since Midnight January 1, 1600
		<li><code>CWFGM_WEATHER_OPTION_END_TIME</code>		64-bit unsigned integer.  GMT time provided as seconds since Midnight January 1, 1600
		<li><code>CWFGM_WEATHER_OPTION_START_HOUR</code>	32-bit signed integer. The first hour of the weather stream [0-12) LST.
		<li><code>CWFGM_WEATHER_OPTION_END_HOUR</code>		32-bit signed integer. The lsat hour of the weather stream [0-23].
		</ul>
		\param	value	The value of the option.
		\retval	E_POINTER	The address provided for value is invalid.
		\retval	S_OK	Successful.
		\retval	ERROR_WEATHER_OPTION_INVALID	The weather option referred to is unknown.
	*/
	virtual NO_THROW HRESULT GetAttribute(std::uint16_t option, PolymorphicAttribute *value);
	/**
		Sets the value of an "option" to the value of the "value" variable provided.  It is important to set the starting codes of the weather stream before importing the weather data.  Changing the starting codes after importing mean Prometheus will recalculate everything.
		\param	option	The weather option of interest.  Valid values are:
		<ul>
		<li><code>CWFGM_WEATHER_OPTION_FFMC_VANWAGNER</code>		Boolean.  Use the Van Wagner approach to calculating HFFMC values
		<li><code>CWFGM_WEATHER_OPTION_FFMC_LAWSON</code>		Boolean.  Use the Lawson approach to calculating HFFMC values
		<li><code>CWFGM_WEATHER_OPTION_FWI_USE_SPECIFIED</code>	Boolean.  Use any/all FWI values provided in the input file
		<li><code>CWFGM_WEATHER_OPTION_TEMP_ALPHA</code>		64-bit floating point.  The 'alpha' value for Beck et. al. diurnal equations for temperature.
		<li><code>CWFGM_WEATHER_OPTION_TEMP_BETA</code>			64-bit floating point.  The 'beta' value for Beck et. al. diurnal equations for temperature.
		<li><code>CWFGM_WEATHER_OPTION_TEMP_GAMMA</code>		64-bit floating point.  The 'gamma' value for Beck et. al. diurnal equations for temperature.
		<li><code>CWFGM_WEATHER_OPTION_WIND_ALPHA</code>		64-bit floating point.  The 'alpha' value for Beck et. al. diurnal equations for wind.
		<li><code>CWFGM_WEATHER_OPTION_WIND_BETA</code>			64-bit floating point.  The 'beta' value for Beck et. al. diurnal equations for wind.
		<li><code>CWFGM_WEATHER_OPTION_WIND_GAMMA</code>		64-bit floating point.  The 'gamma' value for Beck et. al. diurnal equations for wind.
		<li><code>CWFGM_WEATHER_OPTION_INITIAL_FFMC</code>		64-bit floating point.  Initial FFMC value for daily FWI calculations.
		<li><code>CWFGM_WEATHER_OPTION_INITIAL_HFFMC</code>		64-bit floating point.  Initial hourly FFMC value for hourly FWI calculations.
		<li><code>CWFGM_WEATHER_OPTION_INITIAL_DC</code>		64-bit floating point.  Initial DC value for daily FWI calculations.
		<li><code>CWFGM_WEATHER_OPTION_INITIAL_DMC</code>		64-bit floating point.  Initial DMC value for daily FWI calculations.
		<li><code>CWFGM_WEATHER_OPTION_INITIAL_BUI</code>		64-bit floating point.  Optional initial BUI value.  If not provided, then BUI is calculated from the initial DC, DMC values.  Use -99.0 to clear the specified value.
		<li><code>CWFGM_WEATHER_OPTION_INITIAL_RAIN</code>		64-bit floating point.  Initial rain for the prior 11/12 hours.  Only needed for Van Wagner HFFMC calculations.
		<li><code>CWFGM_GRID_ATTRIBUTE_LATITUDE</code> 64-bit floating point, radians.
		<li><code>CWFGM_GRID_ATTRIBUTE_LONGITUDE</code> 64-bit floating point, radians.
		<li><code>CWFGM_WEATHER_OPTION_INITIAL_HFFMCTIME</code>	64-bit signed integer.  Units are in seconds.  Hour at which the initial HFFMC value is observed.  HFFMC values will be calculated forward and back in time from this value.
		<li><code>CWFGM_GRID_ATTRIBUTE_TIMEZONE_ID</code>	32-bit unsigned integer.  A unique ID for a pre-defined set of timezone settings. The timezone information can be retrieved using <code>WorldLocation::TimeZoneFromId</code>.
		<li><code>CWFGM_GRID_ATTRIBUTE_TIMEZONE</code>		64-bit signed integer.  Units are in seconds, relative to GMT.  For example, MST (Mountain Standard Time) would be -6 * 60 * 60 seconds.  Valid values are from -12 hours to +12 hours.
		<li><code>CWFGM_GRID_ATTRIBUTE_DAYLIGHT_SAVINGS</code>	64-bit signed integer.  Units are in seconds.  Amount of correction to apply for daylight savings time.
		<li><code>CWFGM_GRID_ATTRIBUTE_DST_START</code>	64-bit unsigned integer.  Units are in seconds.  Julian date determining when daylight savings starts within the calendar year.
		<li><code>CWFGM_GRID_ATTRIBUTE_DST_END</code>	64-bit unsigned integer.  Units are in seconds.  Julian date determining when daylight savings ends within the calendar year.
		<li><code>CWFGM_WEATHER_OPTION_START_TIME</code>	64-bit unsigned integer.  GMT time provided as seconds since Midnight January 1, 1600
		<li><code>CWFGM_WEATHER_OPTION_END_TIME</code>		64-bit unsigned integer.  GMT time provided as seconds since Midnight January 1, 1600
		</ul>
		\param	value	The value to set the option to.
		\sa ICWFGM_WeatherStream::SetAttribute
		\retval	S_OK	Successful.
		\retval	ERROR_WEATHER_OPTION_INVALID	The weather option referred to is unknown.
		\retval	E_INVALIDARG	If value's value is incorrect.
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Cannot set an attribute if the simulation is running.
		\retval	E_FAIL 	Failed.
	*/
	virtual NO_THROW HRESULT SetAttribute(std::uint16_t option, const PolymorphicAttribute &value);
	/**
		Imports a weather stream with hourly data or daily data depending on the file.
		For importing a weather stream with hourly data the file must be of a specific format.  The first line contains the header determining the file layout.  Necessary
		columns will be identified as
		<ul>
		<li>"hour" (synonyms are "Time(CST)")
		<li>"temp" (synonyms are "temperature", "temp(celsius)")
		<li>"rh" (synonyms are "relative_humidity")
		<li>"wd" (synonyms are "dir(degrees)", "dir", "wind_direction")
		<li>"ws" (synonyms are "wspd", "wind_speed", "spd(kph)")
		<li>"precip" (synonyms are "rain", "precipitation", "raintot", "rn_1", "rn24").
		</ul>
		Optional columns will be identified as
		<ul>
		<li>"ffmc" (synonyms are "hffmc", "ffmc(h)")
		<li>"dmc"
		<li>"dc"
		<li>"bui"
		<li>"isi" (synonyms are "hisi", "isi(h)")
		<li>"fwi" (synonyms are "hfwi", "fwi(h)").
		</ul>
		It is advised to contact the support team or visit the web site for an example file.\n\n
		For importing a weather stream with daily data the file must be of a specific format.  The first line contains the header determining the file layout.  Column
		headers may be
		<ul>
		<li>"min_temp"
		<li>"max_temp"
		<li>"rh" (synonyms are "min_rh", "relative_humidity")
		<li>"wd" (synonyms are "dir(degrees)", "dir", "wind_direction")
		<li>"min_ws"
		<li>"max_ws"
		<li>"precip" (synonyms are "rain", "precipitation", "raintot", "rn_1", "rn24").
		</ul>
		It is advised to contact the support team or visit the web site for an example file.
		\param	file_name	Path/file of weather stream data.
		\param	options		Determines rules for (re)importing data.  Valid options are:
		<ul>
		<li><code>0</code> Only valid for importing data into a weather stream with no data.
		<li><code>CWFGM_WEATHERSTREAM_IMPORT_PURGE</code> When there is existing data in the weather stream, this option removes this data before performing the import.
		<li><code>CWFGM_WEATHERSTREAM_IMPORT_SUPPORT_APPEND</code> When there is existing data in the weather stream, this option will append data to the existing data.  Times must be valid.
		<li><code>CWFGM_WEATHERSTREAM_IMPORT_SUPPORT_OVERWRITE</code> When there is existing data in the weather stream, this option will conditionally overwrite existing data, depending on times.
		<li><code>CWFGM_WEATHERSTREAM_IMPORT_PURGE</code> is mutually exclusive to the other two options.  No option supports prepending data.  Options are ignored when the weather stream has no existing data in it.
		</ul>
		\sa ICWFGM_WeatherStream::Import
		\retval	S_OK	Successful.
		\retval E_INVALIDARG Invalid options.
		\retval	ERROR_FILE_NOT_FOUND File cannot be found in the filesystem.
		\retval	ERROR_ACCESS_DENIED File cannot be opened.
		\retval	ERROR_SEVERITY_WARNING	File cannot be found in the filesystem.
		\retval	ERROR_READ_FAULT | ERROR_SEVERITY_WARNING	Cannot read any data from the file.
		\retval ERROR_BAD_FILE_TYPE | ERROR_SEVERITY_WARNING	The file format is unrecognized, or there is an error in the file format.
		\retval ERROR_INVALID_DATA | ERROR_SEVERITY_WARNING	Data in the file is out of range.
		\retval ERROR_INVALID_TIME | ERROR_SEVERITY_WARNING	Time sequence is invalid (out of order or missing entries, etc.).
		\retval	ERROR_SEVERITY_WARNING	File cannot be opened.
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	The weather stream data cannot be imported while a scenario simulation is still running.
		\retval	ERROR_INVALID_HANDLE	Generic file I/O error.
		\retval	ERROR_FILE_EXISTS	The file does not exist.
		\retval	ERROR_INVALID_PARAMETER
		\retval	ERROR_TOO_MANY_OPEN_FILES	Too many files are open.
		\retval	ERROR_HANDLE_DISK_FULL Disk full
	*/
	virtual NO_THROW HRESULT Import(const std::string & file_name, std::uint16_t options);
	/**
		Deletes all hourly and daily weather data in the file.  Does not reset start time, starting codes, etc.
		\sa ICWFGM_WeatherStream::ClearWeatherData
		\retval S_OK Successful
	*/
	virtual NO_THROW HRESULT ClearWeatherData();
	/**
		Returns the start time and duration for the data stored in this weather stream object.  These objects return this data in GMT, where local time is set by SetAttribute().
		\param	start	Start time for the weather stream, specified in GMT time zone, as count of seconds since Midnight January 1, 1600.
		\param	duration	Duration of the weather stream, provided as a count of seconds.
		\sa ICWFGM_WeatherStream::GetValidTimeRange
		\retval	E_POINTER	The address provided for start or duration is invalid.
		\retval	S_OK	Successful.
	*/
	virtual NO_THROW HRESULT GetValidTimeRange(HSS_Time::WTime *start, HSS_Time::WTimeSpan *duration);
	virtual NO_THROW HRESULT SetValidTimeRange(const HSS_Time::WTime &start, const HSS_Time::WTimeSpan &duration, const bool correctInitialPrecip);
	virtual NO_THROW HRESULT GetFirstHourOfDay(const HSS_Time::WTime &time, unsigned char *hour);
	virtual NO_THROW HRESULT GetLastHourOfDay(const HSS_Time::WTime &time, unsigned char *hour);
	/**
		Gets the time of the next event by searching forwards or backwards (depending on the value of "flags" parameter) and saves the value of the next event to the "next_time" variable.
		\param	flags	To search backwards or forwards.  Valid bit-flag identifiers are:
		<ul>
		<li><code>CWFGM_GETEVENTTIME_FLAG_SEARCH_FORWARD</code> search forward in time
		<li><code>CWFGM_GETEVENTTIME_FLAG_SEARCH_BACKWARD</code> search backwards in time
		</ul>
		\param	from_time	A GMT time provided as seconds since January 1st, 1600.
		\param	next_event	A GMT time provided as seconds since January 1st, 1600, representing the time for the next event, based on 'flags'.
		\sa ICWFGM_WeatherStream::GetEventTime
		\retval	ERROR_GRID_UNINITIALIZED	No object in the grid layering to forward the request to.
		\retval	S_OK	Successful.
	*/
	virtual NO_THROW HRESULT GetEventTime(std::uint32_t flags, const HSS_Time::WTime &from_time, HSS_Time::WTime *next_event);
	/**
		There are two basic formats supported by this weather stream object: a day's values can be specified as hourly conditions or daily observations.  If daily observations are
		provided, then hourly conditions are subsequently calculated.  If the day's values are already specified as hourly conditions, then there is no change.  This method may also
		append a preceding or following day to existing data.
		\param	time	Time identifying the day to modify, provided as a count of seconds since Midnight January 1, 1600 GMT time.
		\sa ICWFGM_WeatherStream::MakeHourlyObservations
		\retval	S_OK	Successful.
		\retval	ERROR_SEVERITY_WARNING	The operation failed (usually because the requested day is not present or adjacent to existing data).
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Changes cannot be made if the simulation is running.
	*/
	virtual NO_THROW HRESULT MakeHourlyObservations( const HSS_Time::WTime &time);
	/**
		There are two basic formats supported by this weather stream object: a day's values can be specified as hourly conditions or daily observations.  If hourly conditions are
		provided, then daily observations are subsequently calculated.  If the day's values are already specified as daily observations, then there is no change.  This method may also
		append a preceding or following day to existing data.
		\param	time	Time identifying the day to modify, provided as a count of seconds since Midnight January 1, 1600 GMT time.
		\sa ICWFGM_WeatherStream::MakeDailyObservations
		\retval	S_OK	Successful.
		\retval	ERROR_SEVERITY_WARNING	The operation failed (usually because the requested day is not present or adjacent to existing data).
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Changes cannot be made if the simulation is running.
	*/
	virtual NO_THROW HRESULT MakeDailyObservations( const HSS_Time::WTime &time);
	/**
		Returns the representation of a given day.
		\param	time	Time identifying the day to inspect, provided as a count of seconds since Midnight January 1, 1600 GMT time.
		\sa ICWFGM_WeatherStream::IsDailyObservations
		\retval	S_OK	The day is represented using daily observations.
		\retval	ERROR_NOT_ENOUGH_MEMORY	The day is not present in the data stream.
		\retval	ERROR_INVALID_TIME	The day is not present in the data stream.
		\retval	ERROR_SEVERITY_WARNING	The day is represented using hourly readings.
	*/
	virtual NO_THROW HRESULT IsDailyObservations( const HSS_Time::WTime &time);
	virtual NO_THROW HRESULT IsAnyDailyObservations();
	virtual NO_THROW HRESULT IsAnyModified();
	virtual NO_THROW HRESULT IsModified(const HSS_Time::WTime &time);
	/**
		Returns the daily observations for the specified day.  If the day is represented as daily observations, this method returns these specified values.  If
		the day is represented as hourly readings, then it returns values calculated from the hourly readings.
		\param	time	Time identifying the day to inspect, provided as a count of seconds since Midnight January 1, 1600 GMT time.
		\param	min_temp	Returned minimum temperature.
		\param	max_temp	Returned maximum temperature.
		\param	min_ws	Returned minimum windspeed.
		\param	max_ws	Returned maximum windspeed.
		\param	rh	Returned minimum relative humidity.
		\param	precip	Returned precipitation for the day.
		\param	wd	Returned mean wind direction, provided in Cartesian radians.
		\sa ICWFGM_WeatherStream::GetDailyValues
		\retval	E_POINTER	The address provided for min_temp, max_temp, min_ws, max_ws, rh, precip or wd is invalid.
		\retval	S_OK	Successful.
		\retval	ERROR_SEVERITY_WARNING	The day is not present on the weather stream.
	*/
	virtual NO_THROW HRESULT GetDailyValues(const HSS_Time::WTime &time, double *min_temp, double *max_temp, double *min_ws,
	    double *max_ws, double *min_gust, double *max_gust, double *min_rh, double *precip, double *wd);
	/**
		Sets the daily observations for the specified day.  If the day is represented as hourly readings, then this method will fail.
		\param	time	Time identifying the day to set, provided as a count of seconds since Midnight January 1, 1600 GMT time.
		\param	min_temp	Minimum temperature.
		\param	max_temp	Maximum temperature.
		\param	min_ws	Minimum windspeed.
		\param	max_ws	Maximum windspeed.
		\param	min_rh	Minimum relative humidity.
		\param	precip	Daily precipitation.
		\param	wd	Mean wind direction.
		\sa ICWFGM_WeatherStream::SetDailyValues
		\retval	S_OK	Successful.
		\retval	ERROR_SEVERITY_WARNING	The day is not present in the weather stream, or the day is represented as hourly observations.
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Cannot set daily values while the simulation is running.
	*/
	virtual NO_THROW HRESULT SetDailyValues(const HSS_Time::WTime &time, double min_temp, double max_temp, double min_ws, double max_ws,
	    double min_gust, double max_gust, double min_rh, double precip, double wd);
	virtual NO_THROW HRESULT GetCumulativePrecip(const HSS_Time::WTime& time, const HSS_Time::WTimeSpan& duration, double* rain);
	/**
		Gets the instantaneous values for Temperature, DewPointTemperature, RH, Precipitation, WindSpeed and WindDirection (and saves these values in the data structure wx). It
		also gets FFMC, ISI and FWI (and saves theses values in the data structure ifwi) and similarly gets the values for dFFMC, dDMC, dDC and dBUI (and saves them to the data structure dfwi).
		\param	time	Time identifying the day and hour to inspect, provided as a count of seconds since Midnight January 1, 1600 GMT time.
		\param	interpolation_method  Valid bit-flag values are:
		<ul>
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_TEMPORAL</code> Boolean.  When true, temporal weather interpolation is turned on, for all of the WX and hourly/instantantaneous FWI calculations.
		</ul>
		\param	wx	The data structure that stores the values for Temperature, Dew DewPointTemperature, RH, Precipitation, WindSpeed and WindDirection.
		\param	ifwi	The data structure that stores the values for FFMC, ISI and FWI.
		\param	dfwi	The data structure that stores the values for dFFMC, dDMC, dDC and dBUI.
		\sa ICWFGM_WeatherStream::GetInstantaneousValues
		\retval	S_OK	This day's data was loaded from a file.
		\retval	E_POINTER	Invalid parameters.
		\retval	ERROR_SEVERITY_WARNING	Unspecified error.
	*/
	virtual NO_THROW HRESULT GetInstantaneousValues(const HSS_Time::WTime &time, std::uint64_t interpolation_method, IWXData *wx, IFWIData *ifwi, DFWIData *dfwi);
	/**
		Sets the instantaneous values for Temperature, DewPointTemperature, RH, Precipitation, WindSpeed and WindDirection via the IWXData data structure.
		\param	time	Time identifying the day and hour to inspect, provided as a count of seconds since Midnight January 1, 1600 GMT time.
		\param	wx	The data structure that stores the values for Temperature, Dew DewPointTemperature, RH, Precipitation, WindSpeed and WindDirection.
		\sa ICWFGM_WeatherStream::SetInstantaneousValues
		\retval	S_OK	This day's data was has been stored.
		\retval	E_POINTER	Invalid parameters.
		\retval	ERROR_SEVERITY_WARNING	The day is not present in the weather stream, or the day is represented as daily observations.
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Cannot set values if the scenario simulation is running.
	*/
	virtual NO_THROW HRESULT SetInstantaneousValues(const HSS_Time::WTime &time, IWXData *wx);

protected:
	virtual NO_THROW HRESULT newCondition(DailyCondition** cond);

public:
	/**
		Retrieves the daily standard (Van Wagner) FFMC for the specified day.  Note that other functions typically return the hourly FFMC value.  Note that daily FFMC values change at noon LST, not at midnight.
		\param	time	A GMT time.
		\param	ffmc	Return value for daily FFMC.
		\sa ICWFGM_WeatherStream::DailyStandardFFMC
		\retval	E_POINTER	The address provided for ffmc is invalid.
		\retval	S_OK	Successful.
		\retval	ERROR_SEVERITY_WARNING Time is invalid.
		\retval ERROR_INVALID_TIME	FFMC is invalid.
	*/
	virtual NO_THROW HRESULT DailyStandardFFMC(const HSS_Time::WTime &time, double *ffmc);
	/**
		Retrieves information regarding whether a particular day's data was loaded from a file.
		\param	time	Time identifying the day and hour to inspect, provided as a count of seconds since Midnight January 1, 1600 GMT time.
		\sa ICWFGM_WeatherStream::IsImportedFromFile
		\retval	S_OK	This day's data was loaded from a file.
		\retval	ERROR_SEVERITY_WARNING Time is invalid.
		\retval ERROR_INVALID_TIME	Time is invalid.
	*/
	virtual NO_THROW HRESULT IsImportedFromFile(const HSS_Time::WTime &time);
	virtual NO_THROW HRESULT IsImportedFromEnsemble(const HSS_Time::WTime& time);

#ifndef DOXYGEN_IGNORE_CODE
public:
	virtual std::int32_t serialVersionUid(const SerializeProtoOptions& options) const noexcept override;
	virtual WISE::WeatherProto::CwfgmWeatherStream* serialize(const SerializeProtoOptions& options) override;
	virtual CCWFGM_WeatherStream *deserialize(const google::protobuf::Message& proto, std::shared_ptr<validation::validation_object> valid, const std::string& name) override;
	virtual std::optional<bool> isdirty(void) const noexcept override { return m_bRequiresSave; }

protected:
	WeatherCondition	m_weatherCondition;
	std::uint16_t			m_gridCount;			// number of WeatherGrids using this stream
	CRWThreadSemaphore	m_lock, m_mt_calc_lock;
	std::string			m_loadWarning;
	bool				m_bRequiresSave;

	WeatherBaseCache_MT m_cache;
#endif
};

#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(pop)
#endif
