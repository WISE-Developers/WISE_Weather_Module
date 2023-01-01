/**
 * WISE_Weather_Module: CWFGM_WindSpeedGrid.Serialize.h
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
#include "WeatherCom_ext.h"
#include "FireEngine_ext.h"
#include "propsysreplacement.h"
#include "CWFGM_WeatherGridFilter.h"
#include "CWFGM_WindSpeedGrid.h"
#include "CoordinateConverter.h"
#include "GDALExporter.h"
#include "GDALImporter.h"
#include "gdalclient.h"
#include "GDALextras.h"
#include "doubleBuilder.h"
#include "boost_compression.h"
#include "WTime.h"
#include "str_printf.h"
#include "internal/WTimeProto.h"

#include <ctime>

using namespace GDALExtras;


HRESULT CCWFGM_WindSpeedGrid::Import(const std::uint16_t sector, const double speed, const std::string & prj_file_name, const std::string & grid_file_name)
{
	HRESULT hr = S_OK;
	if (!grid_file_name.length())							return E_INVALIDARG;
	SEM_BOOL engaged;
	CRWThreadSemaphoreEngage engage(m_lock, SEM_TRUE, &engaged, 1000000LL);
	if (!engaged)								return ERROR_SCENARIO_SIMULATION_RUNNING;

	boost::intrusive_ptr<ICWFGM_GridEngine> engine;
	if (!(engine = m_gridEngine(nullptr)))					{ weak_assert(0); return ERROR_GRID_UNINITIALIZED; }

	GDALImporter importer;
	if (importer.Import(grid_file_name.c_str(), nullptr) != GDALImporter::ImportResult::OK)
		return E_FAIL;

	CSemaphoreEngage lock(GDALClient::GDALClient::getGDALMutex(), true);

	if (strlen(importer.projection()))
	{
		OGRSpatialReferenceH sourceSRS = CCoordinateConverter::CreateSpatialReferenceFromStr(importer.projection());

		PolymorphicAttribute v;
		if (FAILED(engine->GetAttribute(nullptr, CWFGM_GRID_ATTRIBUTE_SPATIALREFERENCE, &v)))
			return hr;

		/*POLYMORPHIC CHECK*/
		std::string csProject;
		
		try { csProject = std::get<std::string>(v); } catch (std::bad_variant_access&) { weak_assert(0); return ERROR_PROJECTION_UNKNOWN; };

		OGRSpatialReferenceH m_sourceSRS = CCoordinateConverter::CreateSpatialReferenceFromWkt(csProject.c_str());

		if (m_sourceSRS) {
			if (!sourceSRS)
				return ERROR_GRID_LOCATION_OUT_OF_RANGE;
			else {
				if (!OSRIsSame(m_sourceSRS, sourceSRS, false))
					return ERROR_GRID_LOCATION_OUT_OF_RANGE;
			}
		} else
			return E_FAIL;

		OSRDestroySpatialReference(sourceSRS);
		OSRDestroySpatialReference(m_sourceSRS);
	}

	std::uint16_t x, y;
	if ((sector != (std::uint16_t)-1) && (sector >= m_sectors.size()))
	{
		return ERROR_SECTOR_INVALID_INDEX;
	}

	GDALImporter::ImportType data = importer.importType();
	if ((data != GDALImporter::ImportType::LONG) &&
		(data != GDALImporter::ImportType::SHORT) &&
		(data != GDALImporter::ImportType::USHORT) &&
		(data != GDALImporter::ImportType::ULONG) &&
		(data != GDALImporter::ImportType::FLOAT32) &&
		(data != GDALImporter::ImportType::FLOAT64))
		return E_FAIL;

	double	noData;
	double xllcorner,yllcorner,resolution;
	double gridXLL, gridYLL, gridResolution;
	std::uint16_t gridXDim, gridYDim;

	x = importer.xSize();
	y = importer.ySize();
	xllcorner = importer.lowerLeftX();
	yllcorner = importer.lowerLeftY();
	resolution = importer.xPixelSize();
	noData = importer.nodata();

	/*POLYMORPHIC CHECK*/
	PolymorphicAttribute var;

	try {
		if (FAILED(hr = engine->GetAttribute(nullptr, CWFGM_GRID_ATTRIBUTE_PLOTRESOLUTION, &var))) return hr;
		gridResolution = std::get<double>(var);

		if (FAILED(hr = engine->GetAttribute(nullptr, CWFGM_GRID_ATTRIBUTE_XLLCORNER, &var))) return hr;
		gridXLL = std::get<double>(var);

		if (FAILED(hr = engine->GetAttribute(nullptr, CWFGM_GRID_ATTRIBUTE_YLLCORNER, &var))) return hr;
		gridYLL = std::get<double>(var);
	} catch (std::bad_variant_access&) { 
		weak_assert(0); 
		return ERROR_GRID_UNINITIALIZED; 
	}


	if (FAILED(hr = engine->GetDimensions(0, &gridXDim, &gridYDim)))
		return hr;
	if ((gridXDim != (x)) ||
		(gridYDim != (y)))
		return ERROR_GRID_SIZE_INCORRECT;
	if (fabs(gridResolution - resolution) > 0.0001)
		return ERROR_GRID_UNSUPPORTED_RESOLUTION;
	if ((fabs(gridXLL - xllcorner) > 0.001) ||
		(fabs(gridYLL - yllcorner) > 0.001))
		return ERROR_GRID_LOCATION_OUT_OF_RANGE;
	
	std::uint16_t *WSArray, *aa1;
	bool *NDArray, *nd1;
	std::uint32_t i, index = x * y;

	try {
		WSArray = new std::uint16_t[index];
		NDArray = new bool[index];
		for (i = 0; i < index; i++) {
			WSArray[i] = 0;
			NDArray[i] = false;
		}
	} catch (std::bad_alloc& cme) {
		return E_OUTOFMEMORY;
	}
							//-------- Read Aspect Data ------------------------
	for (i = 0, aa1 = WSArray, nd1 = NDArray; i < index; i++, aa1++, nd1++) {
		std::uint16_t ws;
		double fws;
		bool nd = true;
		fws = importer.doubleData(1, i);
		if (fws == noData)
			nd = false;			// flag for no data
		else if (fws > 250.0) {				// a kph over 250...seems pretty unlikely...
			delete [] WSArray;
			delete [] NDArray;
			return ERROR_SEVERITY_WARNING;
		}
		else
			ws = (std::uint16_t)floor((fws * 10.0) + 0.5);
		if (nd)
			*aa1 = ws;
		*nd1 = nd;
	}

	if (SUCCEEDED(hr))
	{
		{
			if (sector == (std::uint16_t)-1) {
				if (m_defaultSectorFilename.length())	m_defaultSectorFilename = "";
				if (m_defaultSectorData)				delete [] m_defaultSectorData;
				if (m_defaultSectorDataValid)			delete [] m_defaultSectorDataValid;
				m_defaultSectorFilename = grid_file_name;
				m_defaultSectorData = WSArray;
				m_defaultSectorDataValid = NDArray;
			} else {
				std::uint16_t index1 = m_sectors[sector]->GetSpeedIndex(speed);
				if (index1 != (std::uint16_t)-1)
				{
					m_sectors[sector]->RemoveIndex(index1);
				}
				m_sectors[sector]->AddSpeed(speed, grid_file_name, WSArray, NDArray);
			}
			m_bRequiresSave = true;
		}
	}

	m_resolution = gridResolution;
	m_iresolution = 1.0 / m_resolution;
	m_xllcorner = gridYLL;
	m_yllcorner = gridYLL;

	return hr;
}


HRESULT CCWFGM_WindSpeedGrid::Export(const std::uint16_t sector, const double speed, const std::string & prj_file_name, const std::string & grid_file_name) {
	if (!grid_file_name.length())							return E_INVALIDARG;

	CRWThreadSemaphoreEngage engage(m_lock, SEM_FALSE);

	boost::intrusive_ptr<ICWFGM_GridEngine> engine;
	if (!(engine = m_gridEngine(nullptr)))					{ weak_assert(0); return ERROR_GRID_UNINITIALIZED; }

	if ((sector != (std::uint16_t)-1) && (sector >= m_sectors.size()))
		return ERROR_SECTOR_INVALID_INDEX;

	std::uint16_t index = m_sectors[sector]->GetSpeedIndex(speed);
	std::uint16_t *data;
	bool *nodata;
	if (index != (std::uint16_t)-1) {
		data = m_sectors[sector]->m_entries[index].m_data;
		nodata = m_sectors[sector]->m_entries[index].m_datavalid;
	}
	else {
		data = m_defaultSectorData;
		nodata = m_defaultSectorDataValid;
	}

	if (!data)
		return ERROR_SECTOR_INVALID_INDEX;

  #ifdef _DEBUG
	std::uint16_t xsize = 0, ysize = 0;
	engine->GetDimensions(0, &xsize, &ysize);
	weak_assert(xsize == m_xsize);
	weak_assert(ysize == m_ysize);
  #endif

	double* l_array = new double[m_xsize * m_ysize];
	double* l_pointer = l_array;
	for(std::uint16_t i=m_ysize - 1; i < m_ysize; i--)
	{
		for(std::uint16_t j = 0; j < m_xsize; j++)
		{
			*l_pointer = ((!nodata[ArrayIndex(j,i)]) ? -9999 : data[ArrayIndex(j,i)]) / 10.0;
			l_pointer++;
		}
	}

	CSemaphoreEngage lock(GDALClient::GDALClient::getGDALMutex(), true);

	GDALExporter exporter;
	exporter.AddTag("TIFFTAG_SOFTWARE", "Prometheus");
	exporter.AddTag("TIFFTAG_GDAL_NODATA", "-9999");
	char mbstr[100];
	std::time_t t = std::time(nullptr);
	std::strftime(mbstr, sizeof(mbstr), "%Y-%m-%d %H:%M:%S %Z", std::localtime(&t));
	exporter.AddTag("TIFFTAG_DATETIME", mbstr);
	PolymorphicAttribute v;
	HRESULT hr;
	if (FAILED(hr = engine->GetAttribute(nullptr, CWFGM_GRID_ATTRIBUTE_SPATIALREFERENCE, &v)))
		return hr;

	/*POLYMORPHIC CHECK*/
	std::string ref;
	
	try { ref= std::get<std::string>(v); } catch (std::bad_variant_access&) {return ERROR_PROJECTION_UNKNOWN; };

	const char *cref = ref.c_str();
	exporter.setProjection(cref);
	exporter.setSize(m_xsize, m_ysize);
	exporter.setPrecision(1);
	exporter.setWidth(8);
	exporter.setPixelResolution(m_resolution, m_resolution);
	exporter.setLowerLeft(m_xllcorner, m_yllcorner);
	GDALExporter::ExportResult res = exporter.Export(l_array, grid_file_name.c_str(), "Wind Spd");
	
	if (l_array)
		delete [] l_array;
	
	if (res == GDALExporter::ExportResult::ERROR_ACCESS)
		return E_ACCESSDENIED;

	return S_OK;
}


std::int32_t CCWFGM_WindSpeedGrid::serialVersionUid(const SerializeProtoOptions& options) const noexcept {
	return options.fileVersion();
}


WISE::WeatherProto::WindGrid* CCWFGM_WindSpeedGrid::serialize(const SerializeProtoOptions& options) {
	auto grid = new WISE::WeatherProto::WindGrid();
	grid->set_version(serialVersionUid(options));

	grid->set_type(WISE::WeatherProto::WindGrid_GridType::WindGrid_GridType_WindSpeed);
	grid->set_allocated_starttime(HSS_Time::Serialization::TimeSerializer::serializeTime(m_lStartTime, options.fileVersion()));
	grid->set_allocated_endtime(HSS_Time::Serialization::TimeSerializer::serializeTime(m_lEndTime, options.fileVersion()));
	grid->set_allocated_startspan(HSS_Time::Serialization::TimeSerializer::serializeTimeSpan(m_startSpan));
	grid->set_allocated_endspan(HSS_Time::Serialization::TimeSerializer::serializeTimeSpan(m_endSpan));

	bool applySectors = (m_flags & (1 << (CWFGM_WEATHER_GRID_APPLY_FILE_SECTORS - 10560))) != 0;
	bool applyDefaults = (m_flags & (1 << (CWFGM_WEATHER_GRID_APPLY_FILE_DEFAULT - 10560))) != 0;
	if (applySectors)
		grid->set_applyfilesectors(true);
	else if (applyDefaults)
		grid->set_applyfilesectors(true);

	std::uint16_t xsize, ysize;
	GetDimensions(0, &xsize, &ysize);
	auto sz = xsize * ysize;

	if (m_defaultSectorData)
	{
		auto defaults = new WISE::GridProto::wcsData();
		defaults->set_version(1);

		defaults->set_xsize(xsize);
		defaults->set_ysize(ysize);
		
		auto filename = new WISE::GridProto::wcsData_locationFile();
		filename->set_version(1);
		filename->set_filename(m_defaultSectorFilename);
		defaults->set_allocated_file(filename);

		auto data = new WISE::GridProto::wcsData_binaryData();
		if (options.useVerboseOutput() || !options.zipOutput())
		{
			data->set_data(m_defaultSectorData, sz * sizeof(std::uint16_t));
			data->set_datavalid(m_defaultSectorDataValid, sz);
		}
		else
		{
			data->set_allocated_iszipped(createProtobufObject(true));
			data->set_data(Compress::compress(reinterpret_cast<const char*>(m_defaultSectorData), sz * sizeof(std::uint16_t)));
			data->set_datavalid(Compress::compress(reinterpret_cast<const char*>(m_defaultSectorDataValid), sz));
		}
		defaults->set_allocated_binary(data);

		grid->set_allocated_defaultsectordata(defaults);
	}

	for (auto &sector : m_sectors)
	{
		auto sectorData = grid->add_sectordata();
		sectorData->set_version(1);
		sectorData->set_label(sector->m_label);

		auto direction = new WISE::WeatherProto::WindGrid_SectorData_DirectionWrapper();
		auto specifiedDirection = new WISE::WeatherProto::WindGrid_SectorData_DirectionWrapper_SpecificDirection();
		specifiedDirection->set_allocated_maxangle(DoubleBuilder().withValue(sector->m_maxAngle).forProtobuf(options.useVerboseFloats()));
		specifiedDirection->set_allocated_minangle(DoubleBuilder().withValue(sector->m_minAngle).forProtobuf(options.useVerboseFloats()));
		direction->set_allocated_specifieddirection(specifiedDirection);
		sectorData->set_allocated_direction(direction);

		for (auto &entry : sector->m_entries)
		{
			auto sectorEntry = sectorData->add_sectorentries();
			sectorEntry->set_version(1);
			sectorEntry->set_allocated_speed(DoubleBuilder().withValue(entry.m_speed).forProtobuf(options.useVerboseFloats()));

			auto wcsData = new WISE::GridProto::wcsData();
			wcsData->set_version(1);

			auto filename = new WISE::GridProto::wcsData_locationFile();
			filename->set_version(1);
			filename->set_filename(entry.filename);
			wcsData->set_allocated_file(filename);

			auto data = new WISE::GridProto::wcsData_binaryData();

			if (entry.m_data)
			{
				wcsData->set_xsize(xsize);
				wcsData->set_ysize(ysize);

				if (options.useVerboseOutput() || !options.zipOutput())
				{
					data->set_data(entry.m_data, sz * sizeof(std::uint16_t));
					data->set_datavalid(entry.m_datavalid, sz);
				}
				else
				{
					data->set_allocated_iszipped(createProtobufObject(true));
					data->set_data(Compress::compress(reinterpret_cast<const char*>(entry.m_data), sz * sizeof(std::uint16_t)));
					data->set_datavalid(Compress::compress(reinterpret_cast<const char*>(entry.m_datavalid), sz));
				}
			}
			else
			{
				wcsData->set_xsize(0);
				wcsData->set_ysize(0);
			}

			wcsData->set_allocated_binary(data);

			sectorEntry->set_allocated_data(wcsData);
		}
	}

	return grid;
}

static std::tuple<double, double, int> getDefaultWindAngles(WISE::WeatherProto::WindGrid_SectorData_DirectionWrapper_WindDirection dir)
{
	switch (dir)
	{
	case WISE::WeatherProto::WindGrid_SectorData_DirectionWrapper_WindDirection::WindGrid_SectorData_DirectionWrapper_WindDirection_North:
		return std::make_tuple(337.5, 22.5, 0);
	case WISE::WeatherProto::WindGrid_SectorData_DirectionWrapper_WindDirection::WindGrid_SectorData_DirectionWrapper_WindDirection_Northeast:
		return std::make_tuple(22.5, 67.5, 1);
	case WISE::WeatherProto::WindGrid_SectorData_DirectionWrapper_WindDirection::WindGrid_SectorData_DirectionWrapper_WindDirection_East:
		return std::make_tuple(67.5, 112.5, 2);
	case WISE::WeatherProto::WindGrid_SectorData_DirectionWrapper_WindDirection::WindGrid_SectorData_DirectionWrapper_WindDirection_Southeast:
		return std::make_tuple(112.5, 157.5, 3);
	case WISE::WeatherProto::WindGrid_SectorData_DirectionWrapper_WindDirection::WindGrid_SectorData_DirectionWrapper_WindDirection_South:
		return std::make_tuple(157.5, 202.5, 4);
	case WISE::WeatherProto::WindGrid_SectorData_DirectionWrapper_WindDirection::WindGrid_SectorData_DirectionWrapper_WindDirection_Southwest:
		return std::make_tuple(202.5, 247.5, 5);
	case WISE::WeatherProto::WindGrid_SectorData_DirectionWrapper_WindDirection::WindGrid_SectorData_DirectionWrapper_WindDirection_West:
		return std::make_tuple(257.5, 292.5, 6);
	case WISE::WeatherProto::WindGrid_SectorData_DirectionWrapper_WindDirection::WindGrid_SectorData_DirectionWrapper_WindDirection_Northwest:
		return std::make_tuple(292.5, 337.5, 7);
    default:
	    return std::make_tuple(0.0, 0.0, -1);
	}
}

CCWFGM_WindSpeedGrid* CCWFGM_WindSpeedGrid::deserialize(const google::protobuf::Message& proto, std::shared_ptr<validation::validation_object> valid, const std::string& name)
{
	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine;
	if (!(gridEngine = m_gridEngine(nullptr))) {
		if (valid)
			valid->add_child_validation("WISE.WeatherProto.WindGrid", name, validation::error_level::WARNING, validation::id::initialization_incomplete, "gridengine");
		weak_assert(0);
		m_loadWarning = "Error: WISE.WeatherProto.WindGrid: No grid engine";
		throw ISerializeProto::DeserializeError("WISE.GridProto.WindGrid: Incomplete initialization");
	}

	auto grid = dynamic_cast_assert<const WISE::WeatherProto::WindGrid*>(&proto);
	WTime ullvalue(m_timeManager);
	WTimeSpan w;

	if (!grid)
	{
		if (valid)
			/// <summary>
			/// The object passed as a wind speed grid is invalid. An incorrect object type was passed to the parser.
			/// </summary>
			/// <type>internal</type>
			valid->add_child_validation("WISE.WeatherProto.WindGrid", name, validation::error_level::SEVERE,
				validation::id::object_invalid, proto.GetDescriptor()->name());
		weak_assert(0);
		m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Protobuf object invalid";
		throw ISerializeProto::DeserializeError("WISE.WeatherProto.CwfgmWindSpeedGrid: Protobuf object invalid", ERROR_PROTOBUF_OBJECT_INVALID);
	}

	if ((grid->version() != 1) && (grid->version() != 2))
	{
		if (valid)
			/// <summary>
			/// The object version is not supported. The wind speed grid is not supported by this version of Prometheus.
			/// </summary>
			/// <type>user</type>
			valid->add_child_validation("WISE.WeatherProto.WindGrid", name, validation::error_level::SEVERE,
				validation::id::version_mismatch, std::to_string(grid->version()));
		weak_assert(0);
		m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Version is invalid";
		throw ISerializeProto::DeserializeError("WISE.WeatherProto.CwfgmWindSpeedGrid: Version is invalid", ERROR_PROTOBUF_OBJECT_VERSION_INVALID);
	}
	if (grid->type() != WISE::WeatherProto::WindGrid_GridType::WindGrid_GridType_WindSpeed)
	{
		if (valid)
			/// <summary>
			/// A wind direction grid has been specified in place of a wind speed grid.
			/// </summary>
			/// <type>user</type>
			valid->add_child_validation("WISE.WeatherProto.WindGrid", name, validation::error_level::WARNING,
				validation::id::grid_type_invalid, std::to_string(grid->type()));
		weak_assert(0);
		m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Non wind-speed grid passed to the speed grid deserializer";
		throw ISerializeProto::DeserializeError("WISE.WeatherProto.CwfgmWindSpeedGrid: Non wind-speed grid passed to the speed grid deserializer");
	}

	/// <summary>
	/// Child validations for a wind speed grid.
	/// </summary>
	auto vt = validation::conditional_make_object(valid, "WISE.WeatherProto.WindGrid", name);
	auto myValid = vt.lock();

	if (grid->has_starttime())
	{
		auto time = HSS_Time::Serialization::TimeSerializer::deserializeTime(grid->starttime(), m_timeManager, myValid, "startTime");
		m_lStartTime = HSS_Time::WTime(*time);
		m_lStartTime.PurgeToSecond(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
		delete time;
	}
	if (grid->has_endtime())
	{
		auto time = HSS_Time::Serialization::TimeSerializer::deserializeTime(grid->endtime(), m_timeManager, myValid, "endTime");
		m_lEndTime = HSS_Time::WTime(*time);
		m_lEndTime.PurgeToSecond(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
		delete time;
	}
	if (grid->has_startspan())
	{
		auto time = HSS_Time::Serialization::TimeSerializer::deserializeTimeSpan(grid->startspan(), myValid, "startSpan");
		w = HSS_Time::WTimeSpan(*time);

		if ((WTimeSpan(0, 0, 0, 0) > w) || (w > WTimeSpan(0, 23, 59, 59))) {
			if (myValid)
				/// <summary>
				/// The start time span for the wind speed grid is not in [00:00:00, 23:59:59].
				/// </summary>
				/// <type>user</type>
				myValid->add_child_validation("HSS.Times.WTimeSpan", "startSpan", validation::error_level::SEVERE,
					validation::id::time_range_invalid, m_startSpan.ToString(WTIME_FORMAT_STRING_ISO8601),
					{ true, "PT0M" }, { true, "PT23H59M59S" });
			m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Invalid start span value";
			throw ISerializeProto::DeserializeError("WISE.WeatherProto.CwfgmWindSpeedGrid: Invalid end span value");
		}

		m_startSpan = w;
		delete time;
	}
	if (grid->has_endspan())
	{
		auto time = HSS_Time::Serialization::TimeSerializer::deserializeTimeSpan(grid->endspan(), myValid, "endSpan");
		w = HSS_Time::WTimeSpan(*time);

		if ((WTimeSpan(0, 0, 0, 0) > w) || (w > WTimeSpan(0, 23, 59, 59))) {
			if (myValid)
				/// <summary>
				/// The end time span for the wind speed grid is not in [00:00:00, 23:59:59].
				/// </summary>
				/// <type>user</type>
				myValid->add_child_validation("HSS.Times.WTimeSpan", "endSpan", validation::error_level::SEVERE,
					validation::id::time_range_invalid, m_endSpan.ToString(WTIME_FORMAT_STRING_ISO8601),
					{ true, "PT0M" }, { true, "PT23H59M59S" });
			m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Invalid end span value";
			throw ISerializeProto::DeserializeError("WISE.WeatherProto.CwfgmWindSpeedGrid: Invalid end span value");
		}

		if (grid->has_startspan()) {
			if (w < m_startSpan) {
				if (myValid)
					/// <summary>
					/// The end time span for the wind speed grid is earlier than the start time span.
					/// </summary>
					/// <type>user</type>
					myValid->add_child_validation("HSS.Times.WTimeSpan", { "startSpan", "endSpan" }, validation::error_level::SEVERE,
						validation::id::time_invalid, { m_startSpan.ToString(WTIME_FORMAT_STRING_ISO8601), m_endSpan.ToString(WTIME_FORMAT_STRING_ISO8601) });
				m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Invalid end span value";
				throw ISerializeProto::DeserializeError("WISE.WeatherProto.CwfgmWindSpeedGrid: Invalid end span value");
			} 
		}

		m_endSpan = w;
		delete time;
	}

	if (grid->ApplyFile_case() == WISE::WeatherProto::WindGrid::kApplyFileSectors)
	{
		if (grid->applyfilesectors())
			m_flags |= 1 << (CWFGM_WEATHER_GRID_APPLY_FILE_SECTORS - 10560);
	}
	else if (grid->ApplyFile_case() == WISE::WeatherProto::WindGrid::kApplyFileDefaults)
	{
		if (grid->applyfiledefaults())
			m_flags |= 1 << (CWFGM_WEATHER_GRID_APPLY_FILE_DEFAULT - 10560);
	}

	if (grid->has_defaultsectordata())
	{
		auto defaults = grid->defaultsectordata();
		if (defaults.version() != 1)
		{
			if (myValid)
				/// <summary>
				/// The object version is not supported. The default sector data is not supported by this version of Prometheus.
				/// </summary>
				/// <type>user</type>
				myValid->add_child_validation("WISE.GridProto.wcsData", "defaultSectorData", validation::error_level::SEVERE,
					validation::id::version_mismatch, std::to_string(defaults.version()));
			weak_assert(0);
			m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Version is invalid";
			throw ISerializeProto::DeserializeError("WISE.WeatherProto.CwfgmWindSpeedGrid: Version is invalid");
		}

		/// <summary>
		/// Child validations for default sector data.
		/// </summary>
		auto vt2 = validation::conditional_make_object(myValid, "WISE.GridProto.wcsData", "defaultSectorData");
		auto defaultsValid = vt2.lock();

		if (defaults.has_binary())
		{
			auto xsize = defaults.xsize();
			auto ysize = defaults.ysize();


			USHORT gridXDim, gridYDim;
			HRESULT hr;
			if (FAILED(hr = gridEngine->GetDimensions(0, &gridXDim, &gridYDim)))
			{
				if (defaultsValid)
					/// <summary>
					/// The grid dimensions is not readable but should be by this time in deserialization.
					/// </summary>
					/// <type>internal</type>
					defaultsValid->add_child_validation("WISE.WeatherProto.WindGrid", name, validation::error_level::SEVERE,
						validation::id::initialization_incomplete, "dimensions");
				weak_assert(0);
				m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Incomplete initialization";
				throw ISerializeProto::DeserializeError("WISE.GridProto.WindGrid: Incomplete initialization");
			}

			if (xsize != gridXDim) {
				m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Invalid grid dimensions";
				if (defaultsValid)
					/// <summary>
					/// The specified grid is a different resolution than the elevation grid and fuelmap.
					/// </summary>
					/// <type>user</type>
					defaultsValid->add_child_validation("uint32", "xSize", validation::error_level::SEVERE,
						validation::id::grid_resolution_mismatch, std::to_string(xsize));
				else
					throw ISerializeProto::DeserializeError("WISE.GridProto.WindGrid: Invalid dimensions");
			}
			if (ysize != gridYDim) {
				m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Invalid grid dimensions";
				if (defaultsValid)
					/// <summary>
					/// The specified grid is a different resolution than the elevation grid and fuelmap.
					/// </summary>
					/// <type>user</type>
					defaultsValid->add_child_validation("uint32", "ySize", validation::error_level::SEVERE,
						validation::id::grid_resolution_mismatch, std::to_string(ysize));
				else
					throw ISerializeProto::DeserializeError("WISE.GridProto.WindGrid: Invalid dimensions");
			}

			auto sz = xsize * ysize;
			m_xsize = xsize;
			m_ysize = ysize;
			if (defaults.has_file())
				m_defaultSectorFilename = defaults.file().filename();
			m_defaultSectorData = new std::uint16_t[sz];
			m_defaultSectorDataValid = new bool[sz];

			if (defaults.binary().has_iszipped() && defaults.binary().iszipped().value())
			{
				std::string data = Compress::decompress(defaults.binary().data());
				std::string valid = Compress::decompress(defaults.binary().datavalid());
				if (data.length() != (valid.length() * sizeof(std::uint16_t)) || valid.length() != (size_t)sz) {
					if (defaultsValid)
						/// <summary>
						/// The size of the default sector data valid archive doesn't match the size of the default sector data grid archive.
						/// </summary>
						/// <type>user</type>
						defaultsValid->add_child_validation("WISE.GridProto.wcsData.binaryData", "binary", validation::error_level::SEVERE,
							validation::id::archive_decompress, strprintf("%d != %d", (int)data.length(), (int)valid.length()));
					m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Invalid default wind speed grid in imported file.";
					throw ISerializeProto::DeserializeError("WISE.WeatherProto.CwfgmWindSpeedGrid: Invalid default wind speed grid in imported file.");
				}
				std::copy(data.begin(), data.end(), reinterpret_cast<std::uint8_t*>(m_defaultSectorData));
				std::copy(valid.begin(), valid.end(), m_defaultSectorDataValid);
			}
			else
			{
				std::copy(defaults.binary().data().begin(), defaults.binary().data().end(), reinterpret_cast<std::uint8_t*>(m_defaultSectorData));
				std::copy(defaults.binary().datavalid().begin(), defaults.binary().datavalid().end(), m_defaultSectorDataValid);
			}
		}
		else if (defaults.has_file())
		{
			if (!defaults.file().has_projectionfilename())
			{
				if (defaultsValid)
					/// <summary>
					/// A file has been provided for the default sector data of a wind speed grid but no projection was provided with it.
					/// </summary>
					/// <type>user</type>
					defaultsValid->add_child_validation("WISE.GridProto.wcsData.locationFile", "file", validation::error_level::SEVERE,
						validation::id::projection_missing, defaults.file().filename());
				weak_assert(0);
				m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Wind speed grid file import without projection.";
				throw ISerializeProto::DeserializeError("WISE.WeatherProto.CwfgmWindSpeedGrid: Wind speed grid file import without projection.");
			}

			if ((m_ysize == (std::uint16_t)-1) && (m_xsize == (std::uint16_t)-1))
				m_gridEngine(nullptr)->GetDimensions(0, &m_xsize, &m_ysize);
			HRESULT hr1;
			if (FAILED(hr1 = Import(-1, 0.0, defaults.file().projectionfilename().value(), defaults.file().filename()))) {
				m_loadWarning = strprintf("Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Import error code: %x.", hr1);
				throw ISerializeProto::DeserializeError(m_loadWarning);
			}
		}
		//assume no default sector data
	}

	if (grid->sectordata_size() > 0)
	{
		int i = 0;
		for (auto &sector : grid->sectordata())
		{
			/// <summary>
			/// Child validations for a wind speed grid sector.
			/// </summary>
			auto vt2 = validation::conditional_make_object(myValid, "WISE.WeatherProto.WindGrid.SectorData", strprintf("sectorData[%d]", i));
			auto sectorValid = vt2.lock();

			if (!sector.has_direction()) {
				if (sectorValid)
					/// <summary>
					/// The wind speed sector data doesn't have a wind direction specified with it.
					/// </summary>
					/// <type>user</type>
					sectorValid->add_child_validation("WISE.WeatherProto.WindGrid.SectorData.DirectionWrapper", "direction", validation::error_level::WARNING,
						validation::id::wind_direction_missing, "direction");
				break;
			}
			double min_d, max_d;
			std::uint16_t sector_i;
			if (sector.direction().has_specifieddirection())
			{
				min_d = DoubleBuilder().withProtobuf(sector.direction().specifieddirection().minangle(), sectorValid, "minAngle").getValue();
				max_d = DoubleBuilder().withProtobuf(sector.direction().specifieddirection().maxangle(), sectorValid, "maxAngle").getValue();
			}
			else if (sector.direction().direction_case() == WISE::WeatherProto::WindGrid_SectorData_DirectionWrapper::DirectionCase::kCardinalDirection)
			{
				auto parts = getDefaultWindAngles(sector.direction().cardinaldirection());
				min_d = std::get<0>(parts);
				max_d = std::get<1>(parts);
				sector_i = std::get<2>(parts);
			}

			if (sector.sectorentries_size() > 0)
			{
				SpeedSector *s = nullptr;
				bool add = false;

				int j = 0;
				for (auto& entry : sector.sectorentries())
				{
					/// <summary>
					/// Child validations for sector entries in a wind speed grid.
					/// </summary>
					auto vt3 = validation::conditional_make_object(sectorValid, "WISE.WeatherProto.WindGrid.SectorData.GridData", strprintf("sectorEntries[%d]", j++));
					auto entryValid = vt3.lock();

					if (entry.version() == 1 && entry.has_speed() && entry.has_data() && entry.data().version() == 1)
					{
						auto vt4 = validation::conditional_make_object(entryValid, "WISE.GridProto.wcsData", "data");
						auto dataValid = vt4.lock();

						double speed = DoubleBuilder().withProtobuf(entry.speed()).getValue();

						if (entry.data().has_binary())
						{
							//check for existing data in this range of directions
							for (size_t ii = 0; ii < m_sectors.size(); ii++)
							{
								if (EQUAL_ANGLES_DEGREE(m_sectors[ii]->m_minAngle, min_d) && EQUAL_ANGLES_DEGREE(m_sectors[ii]->m_maxAngle, max_d))
								{
									s = m_sectors[ii];
									break;
								}
							}

							if (!s)
							{
								s = new SpeedSector(min_d, max_d, sector.label());
								add = true;
							}

							std::string f;
							if (entry.data().has_file())
								f = entry.data().file().filename();
							std::uint16_t *data = nullptr;
							bool *datavalid = nullptr;
							auto xsize = entry.data().xsize();
							auto ysize = entry.data().ysize();

							if (xsize > 0 && ysize > 0)
							{
								m_xsize = xsize;
								m_ysize = ysize;

								USHORT gridXDim, gridYDim;
								HRESULT hr;
								if (FAILED(hr = gridEngine->GetDimensions(0, &gridXDim, &gridYDim)))
								{
									if (dataValid)
										/// <summary>
										/// The grid dimensions is not readable but should be by this time in deserialization.
										/// </summary>
										/// <type>internal</type>
										dataValid->add_child_validation("WISE.WeatherProto.WindGrid", name, validation::error_level::SEVERE,
											validation::id::initialization_incomplete, "dimensions");
									weak_assert(0);
									m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Incomplete initialization";
									if (add)
										delete s;
									throw ISerializeProto::DeserializeError("WISE.GridProto.WindGrid: Incomplete initialization");
								}

								if (m_xsize != gridXDim) {
									m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Invalid grid dimensions";
									if (dataValid)
										/// <summary>
										/// The specified grid is a different resolution than the elevation grid and fuelmap.
										/// </summary>
										/// <type>user</type>
										dataValid->add_child_validation("uint32", "xSize", validation::error_level::SEVERE,
											validation::id::grid_resolution_mismatch, std::to_string(m_xsize));
									else {
										if (add)
											delete s;
										throw ISerializeProto::DeserializeError("WISE.GridProto.WindGrid: Invalid dimensions");
									}
								}
								if (m_ysize != gridYDim) {
									m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Invalid grid dimensions";
									if (dataValid)
										/// <summary>
										/// The specified grid is a different resolution than the elevation grid and fuelmap.
										/// </summary>
										/// <type>user</type>
										dataValid->add_child_validation("uint32", "ySize", validation::error_level::SEVERE,
											validation::id::grid_resolution_mismatch, std::to_string(m_ysize));
									else {
										if (add)
											delete s;
										throw ISerializeProto::DeserializeError("WISE.GridProto.WindGrid: Invalid dimensions");
									}
								}

								auto sz = xsize * ysize;
								data = new std::uint16_t[sz];
								datavalid = new bool[sz];

								if (entry.data().binary().has_iszipped() && entry.data().binary().iszipped().value())
								{
									std::string arr = Compress::decompress(entry.data().binary().data());
									std::string valid = Compress::decompress(entry.data().binary().datavalid());
									if (arr.length() != (valid.length() * sizeof(std::uint16_t)) || valid.length() != sz)
									{
										if (dataValid)
											/// <summary>
											/// The size of the sector data valid archive doesn't match the size of the sector data archive.
											/// </summary>
											/// <type>user</type>
											dataValid->add_child_validation("WISE.GridProto.binaryData", "binary",
												validation::error_level::SEVERE, validation::id::archive_decompress, strprintf("%d != %d", (int)arr.length(), (int)valid.length()));
										delete[] data;
										delete[] datavalid;
										if (add)
											delete s;
										m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Invalid wind speed grid in imported file.";
										throw ISerializeProto::DeserializeError("WISE.WeatherProto.CwfgmWindSpeedGrid: Invalid wind speed grid in imported file.");
									}
									std::copy(arr.begin(), arr.end(), reinterpret_cast<std::uint8_t*>(data));
									std::copy(valid.begin(), valid.end(), datavalid);
								}
								else
								{
									std::copy(entry.data().binary().data().begin(), entry.data().binary().data().end(), reinterpret_cast<std::uint8_t*>(data));
									std::copy(entry.data().binary().datavalid().begin(), entry.data().binary().datavalid().end(), datavalid);
								}
							}
							s->AddSpeed(speed, f, data, datavalid);
						}
						else if (entry.data().has_file())
						{
							if (!entry.data().file().has_projectionfilename())
							{
								if (dataValid)
									/// <summary>
									/// A file has been provided for the sector data of a wind speed grid but no projection was provided with it.
									/// </summary>
									/// <type>user</type>
									dataValid->add_child_validation("WISE.GridProto.wcsData.locationFile", "file",
										validation::error_level::SEVERE, validation::id::projection_missing, entry.data().file().filename());
								weak_assert(0);
								m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Wind speed grid file import without projection.";
								if (add)
									delete s;
								throw ISerializeProto::DeserializeError("WISE.WeatherProto.CwfgmWindSpeedGrid: Wind speed grid file import without projection.");
							}

							if ((m_ysize == (std::uint16_t)-1) && (m_xsize == (std::uint16_t)-1))
								m_gridEngine(nullptr)->GetDimensions(0, &m_xsize, &m_ysize);
							HRESULT hr1;
							if (FAILED(hr1 = Import(sector_i, speed, entry.data().file().projectionfilename().value(), entry.data().file().filename()))) {
								if (entryValid)
								{
									switch (hr1)
									{
									case E_FAIL:
										/// <summary>
										/// The specified wind speed grid is not a valid file, it cannot be imported by GDAL.
										/// </summary>
										/// <type>user</type>
										dataValid->add_child_validation("WISE.GridProto.wcsData.locationFile", "file", validation::error_level::SEVERE,
											validation::id::wind_grid_invalid, entry.data().file().filename());
										break;
									case ERROR_GRID_LOCATION_OUT_OF_RANGE:
										/// <summary>
										/// The projection of the specified wind speed grid does not match that of the elevation grid and fuelmap.
										/// </summary>
										/// <type>user</type>
										dataValid->add_child_validation("WISE.GridProto.wcsData.locationFile", "file", validation::error_level::SEVERE,
											validation::id::grid_projection_mismatch, entry.data().file().filename());
										break;
									case ERROR_SECTOR_INVALID_INDEX:
										/// <summary>
										/// The sector specified for this wind grid is not valid.
										/// </summary>
										/// <type>user</type>
										dataValid->add_child_validation("WISE.GridProto.wcsData.locationFile", "file", validation::error_level::SEVERE,
											validation::id::wind_grid_sector, entry.data().file().filename());
										break;
									case ERROR_GRID_SIZE_INCORRECT:
										/// <summary>
										/// The specified wind speed grid is a different size than the elevation grid and fuelmap.
										/// </summary>
										/// <type>user</type>
										myValid->add_child_validation("WISE.GridProto.wcsData.locationFile", "file", validation::error_level::SEVERE,
											validation::id::grid_size_mismatch, entry.data().file().filename());
										break;
									case ERROR_GRID_UNSUPPORTED_RESOLUTION:
										/// <summary>
										/// The specified wind speed grid is a different size than the elevation grid and fuelmap.
										/// </summary>
										/// <type>user</type>
										myValid->add_child_validation("WISE.GridProto.wcsData.locationFile", "file", validation::error_level::SEVERE,
											validation::id::grid_resolution_mismatch, entry.data().file().filename());
										break;
									case ((HRESULT)ERROR_SEVERITY_WARNING):
										/// <summary>
										/// A wind speed in the wind speed grid is not valid. Wind speeds must be in [0, 250].
										/// </summary>
										/// <type>user</type>
										dataValid->add_child_validation("WISE.GridProto.wcsData.locationFile", "file", validation::error_level::SEVERE,
											validation::id::wind_grid_speed, entry.data().file().filename(),
											{ true, 0.0 }, { true, 250.0 });
										break;
									}
								}
								m_loadWarning = "Error: WISE.WeatherProto.CwfgmWindSpeedGrid: Wind speed grid file import failed.";
								if (add)
									delete s;
								throw std::invalid_argument("Wind speed grid file import failed.");
							}
						}
					}
				}

				if ((s) && (add))
					m_sectors.push_back(s);
			}
			i++;
		}

		if (m_sectors.size() != 8)
		{
			for (size_t iii = 0; iii < m_sectors.size(); iii++)
			{
				SpeedSector *ss = m_sectors[iii];
				if (!ss->m_entries.size())
				{
					m_sectors.erase(m_sectors.begin() + iii);
					delete ss;
				}
			}
		}
	}

	return this;
}
