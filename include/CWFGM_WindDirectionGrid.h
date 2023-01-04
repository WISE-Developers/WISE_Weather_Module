/**
 * WISE_Weather_Module: CWFGM_WindDirectionGrid.h
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
#include "WeatherCom_ext.h"
#include "WTime.h"
#include "WindGrid.h"
#include "ICWFGM_GridEngine.h"
#include "semaphore.h"
#include "ISerializeProto.h"
#include "windGrid.pb.h"

using namespace HSS_Time;

#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(push, 8)
#endif


#ifndef DOXYGEN_IGNORE_CODE
struct direction_entry
{
	direction_entry() { m_data = nullptr; m_datavalid = nullptr; m_speed = -1.0; }
	direction_entry(const direction_entry &toCopy, std::uint16_t xsize, std::uint16_t ysize);
	direction_entry(double speed, const std::string &fn, std::uint16_t *d, bool *v) : filename(fn) { m_speed = speed; m_data = d; m_datavalid = v; }
	std::string filename;
	double m_speed;
	std::uint16_t *m_data;
	bool *m_datavalid;
};

class NewDirectionSector : public Sector
{
	public:
		NewDirectionSector(double mn, double mx, const std::string &l) : Sector(mn, mx, l) { }
		NewDirectionSector(const NewDirectionSector &toCopy, std::uint16_t xsize, std::uint16_t ysize);
		~NewDirectionSector()
		{
			m_entries.clear();
		}
		void Cleanup()
		{
			for (std::uint16_t i=0; i<m_entries.size(); i++)
			{
				m_entries[i].filename.clear();
				if (m_entries[i].m_data)	delete [] m_entries[i].m_data;
			}
		}
		void CleanupFilenames()
		{
			for (std::uint16_t i = 0; i < m_entries.size(); i++)
				m_entries[i].filename.clear();
		}
		void AddSpeed(double speed, const std::string &filename, std::uint16_t* d, bool *v)
		{
			direction_entry entry(speed, filename, d, v);
			m_entries.push_back(entry);
		}
		void RemoveIndex(std::uint16_t index)
		{
			if (m_entries[index].m_data)		delete [] m_entries[index].m_data;
			if (m_entries[index].m_datavalid)	delete [] m_entries[index].m_datavalid;
			m_entries.erase(m_entries.begin() + index);
		}
		std::uint16_t GetSpeedIndex(double speed)
		{
			for (std::uint16_t i=0; i<m_entries.size(); i++)
				if ((speed == m_entries[i].m_speed))
					return i;
			return (std::uint16_t)-1;
		}
		std::uint16_t GetLowerSpeedIndex(double speed)
		{
			double s;
			std::uint16_t i, index = (std::uint16_t)-1;
			for (i=0; i<m_entries.size(); i++) {
				if (speed > m_entries[i].m_speed) {
					index = i;
					s = m_entries[i].m_speed;
					break;
				}
			}
			for (; i<m_entries.size(); i++) {
				if ((speed > m_entries[i].m_speed) && (s < m_entries[i].m_speed)) {
					index = i;
					s = m_entries[i].m_speed;
				}
			}
			return index;
		}
		std::uint16_t GetHigherSpeedIndex(double speed)
		{
			double s;
			std::uint16_t i, index = (std::uint16_t)-1;
			for (i=0; i<m_entries.size(); i++) {
				if (speed < m_entries[i].m_speed) {
					index = i;
					s = m_entries[i].m_speed;
					break;
				}
			}
			for (; i<m_entries.size(); i++) {
				if ((speed < m_entries[i].m_speed) && (s > m_entries[i].m_speed)) {
					index = i;
					s = m_entries[i].m_speed;
				}
			}
			return index;
		}
		NewDirectionSector* ShallowCopy()
		{
			NewDirectionSector* ss = new NewDirectionSector(m_minAngle, m_maxAngle, m_label);
			for (std::uint16_t i = 0; i < m_entries.size(); i++)
			{
				ss->AddSpeed(m_entries[i].m_speed, m_entries[i].filename, m_entries[i].m_data, m_entries[i].m_datavalid);
			}
			return ss;
		}
		bool IsValid()
		{
			if (m_entries.size() < 2)
				return true;
			for (std::uint16_t i = 0; i < m_entries.size() - 1; i++)
			{
				for (std::uint16_t j = i + 1; j < m_entries.size(); j++)
				{
					if (m_entries[i].m_speed == m_entries[j].m_speed)
						return false;
				}
			}
			return true;
		}
		std::vector<direction_entry> m_entries;
};

#endif


/**
	This object manages a collection weather data for a simulation engine's scenario.  It implements the GridEngine interface for communication with the simulation engine.
	Since it only handles weather data, its GridEngine property should refer to an object that can provide fuel type and elevation data, latitude, longitude, etc.
	In addition to the GridEngine interface, it also implements its own interface so that a client application may specify a collection of wind direction grids for use in a simulation.
	It can be configured to handle any number of different arcs (sectors) representing different wind directions that can be encountered.  Then, any number of these sectors can have
	wind direction grid information loaded into it.  This object is unconcerned about
	how other weather data has been provided: if times and other input data are valid, then this object will apply the updated wind direction data according to the
	defined rules.  \n\n This object will also update any FWI calculations required based on options and input values. \n\n No weather streams or
	weather stations are associated with this object since it simply updates and replaces wind direction values based on the grid rules.
*/
class WEATHERCOM_API CCWFGM_WindDirectionGrid : public ICWFGM_GridEngine, public ISerializeProto {

public:
#ifndef DOXYGEN_IGNORE_CODE

	friend class CWFGM_WindDirectionGridHelper;
	CCWFGM_WindDirectionGrid();
	CCWFGM_WindDirectionGrid(const CCWFGM_WindDirectionGrid &toCopy);
	~CCWFGM_WindDirectionGrid();

#endif
	/**
		Creates a new wind direction filter with all the same properties and data of the object being called, returns a handle to the new object in 'newFilter'.
		No data is shared between these two objects, an exact copy (including of all loaded wind direction grid data) is created.
		\param	newFilter	A wind direction grid object.
		\sa ICWFGM_WindDirectionGrid::Clone
		\retval	E_POINTER	The address provided for "newFilter" is invalid.
		\retval	S_OK	Successful.
		\retval	E_OUTOFMEMORY	Insufficient memory.
		\retval	ERROR_SEVERITY_WARNING	Unspecified failure.
		\retval	E_NOINTERFACE	Internal serialization issue.
	*/
	virtual NO_THROW HRESULT Clone(boost::intrusive_ptr<ICWFGM_CommonBase> *newObject) const;
	/**
		Imports a Wind Direction Grid to a specific sector and assigned applicable speed ranges.  If the import projection file is unspecified, then it is assumed to match the
		projection for the associated main ICWFGM_GridEngine object.
		\param	sector	Index of sector.
		\param	speed	Wind speed value (kph)
		\param	prj_file_name	Projection File name
		\param	grid_file_name	Grid File name
		\sa ICWFGM_WindDirectionGrid::Import
		\retval	S_OK	Successful.
		\retval	ERROR_GRID_SIZE_INCORRECT	Incorrect grid size.
		\retval	E_POINTER			Error
		\retval	ERROR_GRID_UNINITIALIZED	Grid uninitialized.
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Scenario simulation running.
	*/
	virtual NO_THROW HRESULT Import(const std::uint16_t sector, const double speed, const std::string & prj_file_name, const std::string & grid_file_name);
	/**
		Exports a Wind Direction Grid from a specific sector, for a specific speed.  Specification of the output projection file name is optional.
		\param	sector	Index of sector.
		\param	speed	Wind speed value (kph)
		\param	prj_file_name	Projection File name
		\param	grid_file_name	Grid File name
		\sa ICWFGM_WindDirectionGrid::Export
		\retval	S_OK	Successful.
		\retval	ERROR_GRID_SIZE_INCORRECT	Incorrect grid size.
		\retval	E_POINTER			Error
		\retval	ERROR_GRID_UNINITIALIZED	Grid uninitialized.
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Scenario simulation running.
	*/
	virtual NO_THROW HRESULT Export(const std::uint16_t sector, const double speed, const std::string & prj_file_name, const std::string & grid_file_name);
	/**
		Remove a loaded grid applicable to a given speed from the specified sector.
		\param	sector	Index of sector.
		\param	speed	Speed value (kph)
		\sa ICWFGM_WindSpeedGrid::Remove
		\retval	S_OK	Successful.
		\retval	ERROR_SECTOR_INVALID_INDEX	Invalid sector index error
		\retval	ERROR_SPEED_OUT_OF_RANGE	Speed out of range error
	*/
	virtual NO_THROW HRESULT Remove(const std::uint16_t sector, const double speed);
	/**
		Polymorphic.  This routine retrieves an attribute/option value given the attribute/option index.
		\param option	The attribute of interest.  Valid attributes are:
		<ul>
		<li><code>CWFGM_WEATHER_OPTION_START_TIME</code>	64-bit unsigned integer.  GMT time provided as seconds since Midnight January 1, 1600
		<li><code>CWFGM_WEATHER_OPTION_END_TIME</code>		64-bit unsigned integer.  GMT time provided as seconds since Midnight January 1, 1600
		<li><code>CWFGM_WEATHER_OPTION_START_TIMESPAN</code>	64-bit signed integer.	Units are in seconds.  Specifies the start of the diurnal application period for this weather object.
		<li><code>CWFGM_WEATHER_OPTION_END_TIMESPAN</code>	64-bit signed integer.	Units are in seconds.  Specifies the start of the diurnal application period for this weather object.
		<li><code>CWFGM_GRID_ATTRIBUTE_TIMEZONE_ID</code>	32-bit unsigned integer.  A unique ID for a pre-defined set of timezone settings. The timezone information can be retrieved using <code>WorldLocation::TimeZoneFromId</code>.
		<li><code>CWFGM_GRID_ATTRIBUTE_TIMEZONE</code>		64-bit signed integer.  Units are in seconds, relative to GMT.  For example, MST (Mountain Standard Time) would be -6 * 60 * 60 seconds.  Valid values are from -12 hours to +12 hours.
		<li><code>CWFGM_GRID_ATTRIBUTE_DAYLIGHT_SAVINGS</code>	64-bit signed integer.  Units are in seconds.  Amount of correction to apply for daylight savings time.
		<li><code>CWFGM_GRID_ATTRIBUTE_DST_START</code>	64-bit unsigned integer.  Units are in seconds.  Julian date determining when daylight savings starts within the calendar year.
		<li><code>CWFGM_GRID_ATTRIBUTE_DST_END</code>	64-bit unsigned integer.  Units are in seconds.  Julian date determining when daylight savings ends within the calendar year.
		<li><code>CWFGM_WEATHER_GRID_APPLY_FILE_SECTORS</code>	Boolean.  Determines whether to use per-sector data.
		<li><code>CWFGM_WEATHER_GRID_APPLY_FILE_DEFAULT</code>	Boolean.  Determines whether to use default (shared among all data).  If <code>CWFGM_WEATHER_GRID_APPLY_FILE_SECTORS</code> is true, then per-sector data will conditionally override this default data.
		<li><code>CWFGM_ATTRIBUTE_LOAD_WARNING</code>	BSTR.  Any warnings generated by the COM object when deserializating.
		</ul>
		\param value	Location for the retrieved value to be placed.
		\sa ICWFGM_GridEngine::GetAttribute(std::uint16_t, VARIANT *)
		\retval E_POINTER	value is NULL
		\retval E_INVALIDARG	unknown requested option
		\retval S_OK Success
	*/
	virtual NO_THROW HRESULT GetAttribute(std::uint16_t option, PolymorphicAttribute *value);
	/**
		Sets the value of an "option" to the value of the "value" variable provided.
		\param	option	The weather option of interest.  Valid options are:
		<ul>
		<li><code>CWFGM_WEATHER_OPTION_START_TIME</code>	64-bit unsigned integer.  GMT time provided as seconds since Midnight January 1, 1600
		<li><code>CWFGM_WEATHER_OPTION_END_TIME</code>		64-bit unsigned integer.  GMT time provided as seconds since Midnight January 1, 1600
		<li><code>CWFGM_WEATHER_OPTION_START_TIMESPAN</code>	64-bit signed integer.	Units are in seconds.  Specifies the start of the diurnal application period for this weather object.
		<li><code>CWFGM_WEATHER_OPTION_END_TIMESPAN</code>	64-bit signed integer.	Units are in seconds.  Specifies the start of the diurnal application period for this weather object.
		<li><code>CWFGM_WEATHER_GRID_APPLY_FILE_SECTORS</code>	Boolean.  Determines whether to use per-sector data.
		<li><code>CWFGM_WEATHER_GRID_APPLY_FILE_DEFAULT</code>	Boolean.  Determines whether to use default (shared among all data).  If <code>CWFGM_WEATHER_GRID_APPLY_FILE_SECTORS</code> is true, then per-sector data will conditionally override this default data.
		</ul>
		\param	value	The value to set the option to.  
		\sa ICWFGM_WindDirectionGrid::SetAttribute
		\retval	S_OK	Successful.
		\retval	ERROR_WEATHER_OPTION_INVALID	The weather option referred to is unknown.
		\retval	E_INVALIDARG	If value's value is incorrect.	
		\retval	ERROR_SCENARIO_SIMULATION_RUNNING	Cannot set an attribute if the simulation is running.
		\retval	E_FAIL 	Failed.
	*/
	virtual NO_THROW HRESULT SetAttribute(std::uint16_t option, const PolymorphicAttribute &value);
	/**
		Returns the number of grid files loaded into the specified sector.
		\param	sector	Index of sector.
		\param	count	Number of wind speed ranges.
		\sa ICWFGM_WindSpeedGrid::GetCount
		\retval	S_OK	Successful.
		\retval	ERROR_SECTION_INVALID_INDEX	Invalid section value error
		\retval	E_POINTER	Invalid pointer
	*/
	virtual NO_THROW HRESULT GetCount(const std::uint16_t sector, std::uint16_t *count);
	/**
		Returns the wind speeds for all grid files loaded into the specified sector.
		\param	sector	Index of sector.
		\param	count	Returns number of elements stored in speed_array
		\param	speed_array	2D Speed data for sectors
		\sa ICWFGM_WindSpeedGrid::GetWindSpeeds
		\retval	S_OK	Successful.
		\retval	E_POINTER	Invalid pointer
		\retval	E_INVALIDARG	Invalid argument
		\retval	ERROR_SECTION_INVALID_INDEX	Invalid section index error
	*/
	virtual NO_THROW HRESULT GetWindSpeeds(const std::uint16_t sector, std::uint16_t *count , std::vector<double> *speed_array);
	/**
		Returns an array of filenames for specified wind speed sector
		\param	sector	Index of sector.
		\param	filenames	File names for sector
		\sa ICWFGM_WindSpeedGrid::GetFilenames
		\retval	S_OK	Successful.
		\retval	ERROR_SECTOR_INVALID_INDEX	Invalid sector index.
	*/
	virtual NO_THROW HRESULT GetFilenames(const std::uint16_t sector, std::vector<std::string> *filenames);
	/**
		Gets the number of sectors in this grid object.
		\param	count	Returns the number of sectors for the filter
		\sa ICWFGM_WindDirectionGrid::GetSectorCount
		\retval	S_OK	Successful.
	*/
	virtual NO_THROW HRESULT GetSectorCount(std::uint16_t *count);
	/**
		Get the minimum and maximum sector angles for a specific sector.  The logic applied for determining the correct sector is >= min_angle, and < max_angle.  When
		max_angle < min_angle, it is assumed to cross 0 degrees.
		\param	sector	Index of sector.
		\param	min_angle	Minimum sector angle value, compass degrees
		\param	max_angle	Maximum sector angle value, compass degrees
		\sa ICWFGM_WindDirectionGrid::GetSectorAngles
		\retval	S_OK	Successful.
		\retval	ERROR_SECTOR_INVALID_INDEX	Invalid Sector Index.
	*/
	virtual NO_THROW HRESULT GetSectorAngles(const std::uint16_t sector, double *min_angle, double *max_angle);
	/**
		Adds a sector to filter.  The logic applied for determining the correct sector is >= min_angle, and < max_angle.  When
		max_angle < min_angle, it is assumed to cross 0 degrees (north) in compass rotation.  A sector must be at least 1 degree wide.
		\param	sector_name	Sector Name value, only used by the client application.
		\param	min_angle	Minimum sector angle value, compass degress
		\param	max_angle	Maximum sector angle value, compass degrees
		\param	index		Index of new sector.
		\sa ICWFGM_WindDirectionGrid::AddSector
		\retval	S_OK	Successful.
		\retval	ERROR_DATA_NOT_UNIQUE	 Data provided not unique
		\retval	ERROR_SECTOR_TOO_SMALL	Sector value too small
		\retval	ERROR_NAME_NOT_UNIQUE	Name provided not unique
		\retval	E_POINTER	Invalid pointer.
	*/
	virtual NO_THROW HRESULT AddSector(const std::string & sector_name, double *min_angle, double *max_angle, std::uint16_t *index);
	/**
		Removes sector from filter
		\param	sector	Index of sector.
		\sa ICWFGM_WindDirectionGrid::RemoveSector
		\retval	S_OK
		\retval	ERROR_SECTOR_INVALID_INDEX	Invalid sector index.
	*/
	virtual NO_THROW HRESULT RemoveSector(const std::uint16_t sector);
	/**
		Gets the details of a sector of the grid.  This function has two modes:
		If option is CWFGM_WINDGRID_BYINDEX, then this method keys off '*sector' and populates angle[0], angle[1], and sector_name, with the minimum angle, maximum angle, and name assigned to this sector, respectively.
		\n If option is CWFGM_WINDGRID_BYANGLE, then this method keys off '*angle' and populates sector and sector_name with the index of, and name assigned, to sector.
		\param	option	Option value
		\param	angle	Angle value
		\param	sector	Index of sector.
		\param	sector_name	Sector name
		\sa ICWFGM_WindDirectionGrid::GetSector
		\retval	S_OK	Successful.
		\retval	ERROR_INVALID_INDEX	Error invalid index in 'sector'.
		\retval	ERROR_INVALID_DATA	Error invalid data in 'angle' - no sector exists which contains 'angle'.
		\retval E_INVALIDARG		Unknown option.
	*/
	virtual NO_THROW HRESULT GetSector(std::uint16_t option, double *angle, std::uint16_t *sector, std::string *sector_name);
	/**
		Updates the wind direction ranges in the grid.
		\param set_modifier A list of grid values to modify.
	*/
	virtual NO_THROW HRESULT ModifySectorSet(const std::vector<WeatherGridSetModifier> &set_modifier);

	/**
		Changes the state of the object with respect to access rights.  When the object is used by an active simulation, it must not be modified.
		When the object is somehow modified, it must be done so in an atomic action to prevent concerns with arising from multithreading.
		Note that these locks are primarily needed to ensure that data contributing during a simulation is not modified while the simulation is executing.\n\n All routines in the
		ICWFGM_GridEngine interface are necessarily NOT multithreading safe (for performance) but other interfaces for a given COM object
		implementing this interface must be by specification.  Locking request is forwarded to the next lower object in the 'layerThread' layering.\n\n
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
	virtual NO_THROW HRESULT MT_Lock(Layer *layerThread, bool exclusive, std::uint16_t obtain) override;
	/**
		This method validates weather grid based on its start time, duration, and other properties.  It then forwards the call to the next lower GIS layer determined by layerThread.
		\param	layerThread		Handle for scenario layering/stack access, allocated from an ICWFGM_LayerManager COM object.  Needed.  It is designed to allow nested layering analogous to the GIS layers.
		\param	start_time	Start time of grid data
		\param	duration	Duration of observational data in grid
		\param option	Determines type of Valid request.  Valid values are:
		<ul>
		<li><code>0</code>
		<li><code>CWFGM_WEATHER_WXGRID_WD_DIURNALTIMES</code>	Identifier to count wind speed grids associated with a scenario.  Private.
		</ul>
		\param application_count	Optional (dependent on option).  Array of counts for how often a particular type of grid occurs in the set of ICWFGM_GridEngine objects associated with a scenario.
		\sa ICWFGM_GridEngine::Valid
		\retval S_OK		Successful.
		\retval	ERROR_GRID_UNINITIALIZED	Grid not initialized.
		\retval ERROR_GRID_TIME_OUT_OF_RANGE  Start, end application times are invalid for this grid.
	*/
	virtual NO_THROW HRESULT Valid(Layer *layerThread, const HSS_Time::WTime &start_time, const HSS_Time::WTimeSpan &duration, std::uint32_t option, std::vector<uint16_t> *application_count) override;
	/**
		Polymorphic.  If layerThread is non-zero, then this filter object simply forwards the call to the next lower GIS
		layer determined by layerThread.  If layerthread is zero, then this object will interpret the call.
		\param	layerThread		Handle for scenario layering/stack access, allocated from an ICWFGM_LayerManager COM object.  Needed.  It is designed to allow nested layering analogous to the GIS layers.
		\param option	The attribute of interest.  Valid attributes are:
		<ul>
		<li><code>CWFGM_WEATHER_OPTION_START_TIME</code>	64-bit unsigned integer.  GMT time provided as seconds since Midnight January 1, 1600
		<li><code>CWFGM_WEATHER_OPTION_END_TIME</code>		64-bit unsigned integer.  GMT time provided as seconds since Midnight January 1, 1600
		<li><code>CWFGM_WEATHER_OPTION_START_TIMESPAN</code>	64-bit signed integer.	Units are in seconds.  Specifies the start of the diurnal application period for this weather object.
		<li><code>CWFGM_WEATHER_OPTION_END_TIMESPAN</code>	64-bit signed integer.	Units are in seconds.  Specifies the start of the diurnal application period for this weather object.
		<li><code>CWFGM_GRID_ATTRIBUTE_TIMEZONE_ID</code>	32-bit unsigned integer.  A unique ID for a pre-defined set of timezone settings. The timezone information can be retrieved using <code>WorldLocation::TimeZoneFromId</code>.
		<li><code>CWFGM_GRID_ATTRIBUTE_TIMEZONE</code>		64-bit signed integer.  Units are in seconds, relative to GMT.  For example, MST (Mountain Standard Time) would be -6 * 60 * 60 seconds.  Valid values are from -12 hours to +12 hours.
		<li><code>CWFGM_GRID_ATTRIBUTE_DAYLIGHT_SAVINGS</code>	64-bit signed integer.  Units are in seconds.  Amount of correction to apply for daylight savings time.
		<li><code>CWFGM_GRID_ATTRIBUTE_DST_START</code>	64-bit unsigned integer.  Units are in seconds.  Julian date determining when daylight savings starts within the calendar year.
		<li><code>CWFGM_GRID_ATTRIBUTE_DST_END</code>	64-bit unsigned integer.  Units are in seconds.  Julian date determining when daylight savings ends within the calendar year.
		<li><code>CWFGM_WEATHER_GRID_APPLY_FILE_SECTORS</code>	Boolean.  Determines whether to use per-sector data.
		<li><code>CWFGM_WEATHER_GRID_APPLY_FILE_DEFAULT</code>	Boolean.  Determines whether to use default (shared among all data).  If <code>CWFGM_WEATHER_GRID_APPLY_FILE_SECTORS</code> is true, then per-sector data will conditionally override this default data.
		<li><code>CWFGM_ATTRIBUTE_LOAD_WARNING</code>	BSTR.  Any warnings generated by the COM object when deserializating.
		</ul>
		\param value	Location for the retrieved value to be placed.
		\sa ICWFGM_GridEngine::GetAttribute(__int64, std::uint16_t, VARIANT *)
		\retval E_POINTER	value is NULL
		\retval	ERROR_GRID_UNINITIALIZED	No object in the grid layering to forward the request to.
		\retval S_OK Success
	*/
	virtual NO_THROW HRESULT GetAttribute(Layer *layerThread, std::uint16_t option,  PolymorphicAttribute *value) override;
	/**
		This object conditionally updates weather at location (x, y) at time 'time'.  'interpolate_method' determines various rules for how FWI calculations take place: if FWI calculations
		are being recursively calculated back in time, etc.  All weather and fwi calculations are performed (as requested and determined).  Note that some modes require a potentially long-
		duration recursive calculation to take place, which may take some time (and stack space) to perform.
		\param	layerThread		Handle for scenario layering/stack access, allocated from an ICWFGM_LayerManager COM object.  Needed.  It is designed to allow nested layering analogous to the GIS layers.
		\param	pt			Location.
		\param	time	A GMT time.
		\param	interpolate_method		Interpolation method identifier.  Valid bit-flag identifiers are:
		<ul>
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code> Boolean.  When true, spatial weather interpolation is turned on.  This option applies to both WX and FWI values, and will work whether there is 1 or more weather stations assigned to the scenario.  If false, then there should only be one weather stream.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_HISTORY</code> Boolean.  Conditional on <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code> and <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_CALCFWI</code>.  If true, then historical FWI values are calculated to try to attain equilibrium on FWI values.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_TEMPORAL</code> Boolean.  When true, temporal weather interpolation is turned on, for all of the WX and hourly/instantantaneous FWI calculations.
		</ul>
		\param	wx			Weather information.
		\param	ifwi		IFWI Information.
		\param	dfwi		DFWI Information.
		\sa ICWFGM_GridEngine::GetWeatherData
		\retval	CWFGM_WEATHER_INITIAL_VALUES_ONLY	Given time, only initial weather values (used for starting FWI calculations) could be returned.	
		\retval ERROR_INVALID_DATA			Invalid latitude/longitude available for calculations.
		\retval E_INVALIDARG				Error in calculations (likely FWI calculations).
		\retval E_POINTER				Invalid wx, ifwi, or dfwi pointers.
		\retval S_OK					Calculations are successful
	*/
	virtual NO_THROW HRESULT GetWeatherData(Layer *layerThread, const XY_Point &pt, const HSS_Time::WTime &time, std::uint64_t interpolate_method, IWXData *wx, IFWIData *ifwi, DFWIData *dfwi, bool *wx_valid, XY_Rectangle *bbox_cache) override;
	/**
		This object conditionally updates weather at the specified location for storage in the provided array(s) at time 'time'.  'interpolate_method' determines various rules for how FWI calculations take place: if FWI calculations
		are being recursively calculated back in time, etc.  All weather and fwi calculations are performed (as requested and determined).  Note that some modes require a potentially long-
		duration recursive calculation to take place, which may take some time (and stack space) to perform.
		\param	layerThread		Handle for scenario layering/stack access, allocated from an ICWFGM_LayerManager COM object.  Needed.  It is designed to allow nested layering analogous to the GIS layers.
		\param	min_pt		Minimum value (inclusive).
		\param	max_pt		Maximum value (inclusive).
		\param	scale		Scale (meters) that the array is defined for
		\param	time	A GMT time.
		\param	interpolate_method		Interpolation method identifier.  Valid bit-flag identifiers are:
		<ul>
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code> Boolean.  When true, spatial weather interpolation is turned on.  This option applies to both WX and FWI values, and will work whether there is 1 or more weather stations assigned to the scenario.  If false, then there should only be one weather stream.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_HISTORY</code> Boolean.  Conditional on <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_SPATIAL</code> and <code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_CALCFWI</code>.  If true, then historical FWI values are calculated to try to attain equilibrium on FWI values.
		<li><code>CWFGM_SCENARIO_OPTION_WEATHER_INTERPOLATE_TEMPORAL</code> Boolean.  When true, temporal weather interpolation is turned on, for all of the WX and hourly/instantantaneous FWI calculations.
		</ul>
		\param	wx		Array of Weather information.
		\param	ifwi		Array of Instantaneous FWI codes.
		\param	dfwi		Array of Daily FWI codes.
		\sa ICWFGM_GridEngine::GetWeatherDataArray
		\retval	ERROR_GRID_UNINITIALIZED	No object in the grid layering to forward the request to.
		\retval E_INVALIDARG	The array is not 2D, or is insufficient in size to contain the requested data	
	*/
	virtual NO_THROW HRESULT GetWeatherDataArray(Layer *layerThread, const XY_Point &min_pt,const XY_Point &max_pt, double scale,const HSS_Time::WTime &time, std::uint64_t interpolate_method, 
			IWXData_2d *wx, IFWIData_2d *ifwi, DFWIData_2d *dfwi, bool_2d *wx_valid) override;
	/**
		This method will examine start and end dates and diurnal periods to determine the time at which the next event for known change in weather data occurs.
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
		\sa ICWFGM_GridEngine::GetEventTime
		\retval	ERROR_GRID_UNINITIALIZED	No object in the grid layering to forward the request to.
	*/
	virtual NO_THROW HRESULT GetEventTime(Layer *layerThread, const XY_Point& pt, std::uint32_t flags, const HSS_Time::WTime &from_time,  HSS_Time::WTime *next_event, bool* event_valid) override;
	virtual NO_THROW HRESULT PutGridEngine(Layer *layerThread, ICWFGM_GridEngine * newVal) override;
	virtual NO_THROW HRESULT PutCommonData(Layer* layerThread, ICWFGM_CommonData* pVal) override;

#ifndef DOXYGEN_IGNORE_CODE
		std::uint32_t ArrayIndex(std::uint16_t x, std::uint16_t y)
		{
			if ((m_ysize == (std::uint16_t)-1) && (m_xsize == (std::uint16_t)-1)) {
				m_gridEngine(nullptr)->GetDimensions(0, &m_xsize, &m_ysize);
			}
			return (m_ysize - (y + 1)) * m_xsize + x;
		};

protected:
	WTimeManager					*m_timeManager;
	std::vector<NewDirectionSector*>	m_sectors;
	std::string					m_defaultSectorFilename;
	std::uint16_t				*m_defaultSectorData;
	bool						*m_defaultSectorDataValid;

	std::uint16_t					m_xsize, m_ysize;
	double					m_resolution, m_iresolution, m_xllcorner, m_yllcorner;

	std::string					m_loadWarning;
	CRWThreadSemaphore			m_lock, m_calcLock;

	WTime					m_lStartTime;
	WTime					m_lEndTime;
	WTimeSpan				m_startSpan;
	WTimeSpan				m_endSpan;
	std::uint32_t					m_flags;
	bool					m_bRequiresSave;

	std::uint16_t convertX(double x, XY_Rectangle *bbox);
	std::uint16_t convertY(double y, XY_Rectangle *bbox);
	double invertX(double x)			{ return x * m_resolution + m_xllcorner; }
	double invertY(double y)			{ return y * m_resolution + m_yllcorner; }
	HRESULT fixResolution();

public:
	virtual std::int32_t serialVersionUid(const SerializeProtoOptions& options) const noexcept override;
	virtual WISE::WeatherProto::WindGrid* serialize(const SerializeProtoOptions& options) override;
	virtual CCWFGM_WindDirectionGrid *deserialize(const google::protobuf::Message& proto, std::shared_ptr<validation::validation_object> valid, const std::string& name) override;
	virtual std::optional<bool> isdirty(void) const noexcept override { return m_bRequiresSave; }

	private:
		HRESULT getWeatherData(ICWFGM_GridEngine *gridEngine, Layer *layerThread, const XY_Point &pt, const HSS_Time::WTime &time, std::uint64_t interpolate_method, IWXData *wx, IFWIData *ifwi, DFWIData *dfwi, bool *wx_valid, XY_Rectangle *bbox_cache);
#endif

};

#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(pop)
#endif
