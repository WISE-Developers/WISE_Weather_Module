/**
 * WISE_Weather_Module: CWFGM_WeatherGridFilter.Serialize.h
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

#include "types.h"
#include "angles.h"
#include "CWFGM_WeatherGridFilter.h"
#include "XYPoly.h"
#include "results.h"
#include "GridCom_ext.h"
#include "FireEngine_ext.h"
#include "WeatherCom_ext.h"
#include "CoordinateConverter.h"
#include "gdalclient.h"
#include <boost/scoped_ptr.hpp>
#include "url.h"
#include "doubleBuilder.h"
#include "geo_poly.h"
#include "internal/WTimeProto.h"

#include "filesystem.hpp"


#define CWFGM_WEATHER_GRID_FILTER_VERSION	15

/////////////////////////////////////////////////////////////////////////////
//


HRESULT CCWFGM_WeatherGridFilter::ImportPolygons(const std::string & file_path, const std::vector<std::string> *permissible_drivers) {
	if (!file_path.length())							return E_INVALIDARG;
	if (!m_gridEngine(nullptr))							return ERROR_GRID_UNINITIALIZED;

	std::string csPath(file_path);

	const char **pd;
	if (permissible_drivers && permissible_drivers->size() > 0) {
		pd = (const char **)malloc(sizeof(char *) * (size_t)(permissible_drivers->size() + 1));
		std::uint32_t i;
		for (i = 0; i < (std::uint32_t)permissible_drivers->size(); i++) {
			pd[i] = strdup((*permissible_drivers)[i].c_str());
		}
		pd[i] = NULL;
	}
	else pd = NULL;

	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine;
	HRESULT hr;
	double gridResolution;//, gridXLL, gridYLL;
	std::uint16_t xdim, ydim;
	OGRSpatialReferenceH oSourceSRS = NULL;

	CSemaphoreEngage lock(GDALClient::GDALClient::getGDALMutex(), true);

	if ((gridEngine = m_gridEngine(nullptr)) != NULL) {
		if (FAILED(hr = gridEngine->GetDimensions(0, &xdim, &ydim)))					{ if (pd) { std::uint32_t i = 0; while (pd[i]) free((APTR)pd[i++]); free(pd); } return hr; }

		PolymorphicAttribute var;
		if (FAILED(hr = gridEngine->GetAttribute(0, CWFGM_GRID_ATTRIBUTE_PLOTRESOLUTION, &var)))	{ if (pd) { std::uint32_t i = 0; while (pd[i]) free((APTR)pd[i++]); free(pd); } return hr; }
		try {
			gridResolution = std::get<double>(var);
		}
		catch (std::bad_variant_access&) {
			weak_assert(0);
			if (pd)
				free(pd);
			return E_FAIL;
		}

		/*POLYMORPHIC CHECK*/
		if (FAILED(hr = gridEngine->GetAttribute(0, CWFGM_GRID_ATTRIBUTE_SPATIALREFERENCE, &var)))	{ if (pd) { std::uint32_t i = 0; while (pd[i]) free((APTR)pd[i++]); free(pd); } return hr; } 
		std::string projection;
		
		try { projection = std::get<std::string>(var); } catch (std::bad_variant_access&) { weak_assert(0); return ERROR_PROJECTION_UNKNOWN; };

		oSourceSRS = CCoordinateConverter::CreateSpatialReferenceFromWkt(projection.c_str());
	} else {
		weak_assert(0);
//		if (oSourceSRS)
//			OSRDestroySpatialReference(oSourceSRS);
		if (pd) { std::uint32_t i = 0; while (pd[i]) free((APTR)pd[i++]); free(pd); }
		return ERROR_GRID_UNINITIALIZED;
	}
//	XY_Point ll(0.0 - gridXLL, 0.0 - gridYLL);

	XY_PolyLLSet set;
	set.SetCacheScale(m_resolution);

	if (SUCCEEDED(hr = set.ImportPoly(pd, csPath.c_str(), oSourceSRS))) {
//		set.TranslateXY(ll);
//		set.ScaleXY(1.0 / gridResolution);

		m_polySet.RemoveAllPolys();
		XY_PolyLL *p;
		while ((p = set.RemHead()) != NULL)
			m_polySet.AddPoly(p);
		m_bRequiresSave = true;
	}
	if (oSourceSRS)
		OSRDestroySpatialReference(oSourceSRS);
	if (pd) { std::uint32_t i = 0; while (pd[i]) free((APTR)pd[i++]); free(pd); }
	return hr;
}


#ifndef DOXYGEN_IGNORE_CODE

static std::string prepareUri(const std::string& uri)
{
	remote::url u;
	u.setUrl(uri);
	u.addParam("SERVICE", "WFS");
	u.addParam("REQUEST", "GetCapabilities");
	return u.build();
}

#endif


HRESULT CCWFGM_WeatherGridFilter::ImportPolygonsWFS(const std::string & url, const std::string & layer, const std::string & username, const std::string & password) {
	if (!url.length())										return E_INVALIDARG;
	if (!layer.length())										return E_INVALIDARG;
	if (!m_gridEngine(nullptr))								return ERROR_GRID_UNINITIALIZED;

	std::string csURL(url), csLayer(layer), csUserName(username), csPassword(password);

	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine;
	HRESULT hr;
	double gridResolution;
	std::uint16_t xdim, ydim;
	OGRSpatialReferenceH oSourceSRS = NULL;

	if ((gridEngine = m_gridEngine(nullptr)) != NULL) {
		if (FAILED(hr = gridEngine->GetDimensions(0, &xdim, &ydim))) { return hr; }

		PolymorphicAttribute var;
		if (FAILED(hr = gridEngine->GetAttribute(0, CWFGM_GRID_ATTRIBUTE_PLOTRESOLUTION, &var))) { return hr; }
		try {
			gridResolution = std::get<double>(var);
		}
		catch (std::bad_variant_access&) {
			weak_assert(0);
			return E_FAIL;
		}

		/*POLYMORPHIC CHECK*/
		if (FAILED(hr = gridEngine->GetAttribute(0, CWFGM_GRID_ATTRIBUTE_SPATIALREFERENCE, &var))) { return hr; }
		std::string projection;
		
		try { projection = std::get<std::string>(var); } catch (std::bad_variant_access&) { weak_assert(0); return ERROR_PROJECTION_UNKNOWN; };

		oSourceSRS = CCoordinateConverter::CreateSpatialReferenceFromWkt(projection.c_str());
	} else {
		weak_assert(0);
		return ERROR_GRID_UNINITIALIZED;
	}

	XY_PolyLLSet set;
	set.SetCacheScale(m_resolution);

	std::vector<std::string> layers;
	layers.push_back(csLayer);
	XY_PolyLLSet pset;
	std::string URI = prepareUri(csURL);
	if (SUCCEEDED(hr = set.ImportPoly(NULL, URI.c_str(), oSourceSRS, NULL, &layers))) {
		m_polySet.RemoveAllPolys();
		XY_PolyLL *p;
		while ((p = set.RemHead()) != NULL)
			m_polySet.AddPoly(p);

		m_gisURL = csURL;
		m_gisLayer = csLayer;
		m_gisUID = csUserName;
		m_gisPWD = csPassword;

		m_bRequiresSave = true;
	}
	if (oSourceSRS)
		OSRDestroySpatialReference(oSourceSRS);
	return hr;
}


HRESULT CCWFGM_WeatherGridFilter::ExportPolygons(const std::string & driver_name, const std::string & projection, const std::string & file_path) {
	if ((!driver_name.length()) || (!file_path.length()))
		return E_INVALIDARG;
	std::string csPath(file_path), csDriverName(driver_name), csProjection(projection);

	if (!m_polySet.NumPolys())
		return E_FAIL;

	CSemaphoreEngage lock(GDALClient::GDALClient::getGDALMutex(), true);

	HRESULT hr;
	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine;
	OGRSpatialReferenceH oSourceSRS = NULL;
	OGRSpatialReferenceH oTargetSRS = CCoordinateConverter::CreateSpatialReferenceFromStr(csProjection.c_str());

	if ((gridEngine = m_gridEngine(nullptr)) != nullptr) {
		PolymorphicAttribute var;

		/*POLYMORPHIC CHECK*/
		if (FAILED(hr = gridEngine->GetAttribute(0, CWFGM_GRID_ATTRIBUTE_SPATIALREFERENCE, &var)))	{ if (oTargetSRS) OSRDestroySpatialReference(oTargetSRS); return hr; }
		std::string projection;
		
		try { projection = std::get<std::string>(var); } catch (std::bad_variant_access&) { weak_assert(0); return ERROR_PROJECTION_UNKNOWN; };

		oSourceSRS = CCoordinateConverter::CreateSpatialReferenceFromWkt(projection.c_str());
	} else {
		weak_assert(0);
		if (oTargetSRS)
			OSRDestroySpatialReference(oTargetSRS);
		return ERROR_GRID_UNINITIALIZED;
	}

	XY_PolyLLSet set;
	XY_PolyLL *pc = m_polySet.LH_Head();
	while (pc->LN_Succ()) {
		XY_PolyLL *p = new XY_PolyLL(*pc);
		p->m_publicFlags &= (~(XY_PolyLL_BaseTempl<double>::Flags::INTERPRET_POLYMASK));
		p->m_publicFlags |= XY_PolyLL_BaseTempl<double>::Flags::INTERPRET_POLYGON;
		set.AddPoly(p);
		pc = pc->LN_Succ();
	}

	set.SetCacheScale(m_resolution);
	hr = set.ExportPoly(csDriverName.c_str(), csPath.c_str(), oSourceSRS, oTargetSRS);
	if (oSourceSRS)
		OSRDestroySpatialReference(oSourceSRS);
	if (oTargetSRS)
		OSRDestroySpatialReference(oTargetSRS);
	return hr;
}


HRESULT CCWFGM_WeatherGridFilter::ExportPolygonsWFS(const std::string & url, const std::string & layer, const std::string & username, const std::string & password) {
	return E_NOTIMPL;
}


std::int32_t CCWFGM_WeatherGridFilter::serialVersionUid(const SerializeProtoOptions& options) const noexcept {
	return options.fileVersion();
}


WISE::WeatherProto::WeatherGridFilter* CCWFGM_WeatherGridFilter::serialize(const SerializeProtoOptions& options) {
	auto filter = new WISE::WeatherProto::WeatherGridFilter();
	filter->set_version(serialVersionUid(options));

	PolymorphicAttribute var;
	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine;
	if (!(gridEngine = m_gridEngine(nullptr))) { weak_assert(0); delete filter; throw std::runtime_error("No grid engine"); }

	if (FAILED(gridEngine->GetAttribute(0, CWFGM_GRID_ATTRIBUTE_SPATIALREFERENCE, &var))) {
		delete filter;
		throw std::exception();
	}

	std::string projection;
	projection = std::get<std::string>(var);

	boost::scoped_ptr<CCoordinateConverter> convert(new CCoordinateConverter());
	convert->SetSourceProjection(projection.c_str());

	if (m_landscape)
		filter->set_landscape(true); 
	if (m_polySet.NumPolys())
	{
		auto geo = new GeoPoly(&m_polySet);
		geo->setStoredUnits(GeoPoly::UTM);
		filter->set_allocated_polygons(geo->getProtobuf(options.useVerboseFloats()));
		delete geo;
		filter->clear_landscape();
	}

	filter->set_allocated_starttime(HSS_Time::Serialization::TimeSerializer::serializeTime(m_lStartTime, options.fileVersion()));
	filter->set_allocated_endtime(HSS_Time::Serialization::TimeSerializer::serializeTime(m_lEndTime, options.fileVersion()));

	{
		WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation operation;
		switch (m_poly_temp_op)
		{
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
			operation = (WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation)m_poly_temp_op;
			break;
		default:
			operation = WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation_Disable;
			break;
		}

		if (operation != WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation_Disable)
		{
			auto grid = new WISE::WeatherProto::WeatherGridFilter_GridTypeOne();
			grid->set_version(1);
			grid->set_allocated_value(DoubleBuilder().withValue(m_poly_temp_val).forProtobuf(options.useVerboseFloats()));
			grid->set_operation(operation);
			filter->set_allocated_temperature(grid);
		}
	}
	{
		WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation operation;
		switch (m_poly_rh_op)
		{
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
			operation = (WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation)m_poly_rh_op;
			break;
		default:
			operation = WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation::WeatherGridFilter_GridTypeOne_Operation_Disable;
			break;
		}

		if (operation != WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation_Disable)
		{
			auto grid = new WISE::WeatherProto::WeatherGridFilter_GridTypeOne();
			grid->set_version(1);
			grid->set_allocated_value(DoubleBuilder().withValue(m_poly_rh_val).forProtobuf(options.useVerboseFloats()));
			grid->set_operation(operation);
			filter->set_allocated_rh(grid);
		}
	}
	{
		WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation operation;
		switch (m_poly_precip_op)
		{
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
			operation = (WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation)m_poly_precip_op;
			break;
		default:
			operation = WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation::WeatherGridFilter_GridTypeOne_Operation_Disable;
			break;
		}

		if (operation != WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation_Disable)
		{
			auto grid = new WISE::WeatherProto::WeatherGridFilter_GridTypeOne();
			grid->set_version(1);
			grid->set_allocated_value(DoubleBuilder().withValue(m_poly_precip_val).forProtobuf(options.useVerboseFloats()));
			grid->set_operation(operation);
			filter->set_allocated_precipitation(grid);
		}
	}
	{
		WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation operation;
		switch (m_poly_ws_op)
		{
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
			operation = (WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation)m_poly_ws_op;
			break;
		default:
			operation = WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation::WeatherGridFilter_GridTypeOne_Operation_Disable;
			break;
		}

		if (operation != WISE::WeatherProto::WeatherGridFilter_GridTypeOne_Operation_Disable)
		{
			auto grid = new WISE::WeatherProto::WeatherGridFilter_GridTypeOne();
			grid->set_version(1);
			grid->set_allocated_value(DoubleBuilder().withValue(m_poly_ws_val).forProtobuf(options.useVerboseFloats()));
			grid->set_operation(operation);
			filter->set_allocated_windspeed(grid);
		}
	}
	{
		WISE::WeatherProto::WeatherGridFilter_GridTypeTwo_Operation operation;
		switch (m_poly_wd_op)
		{
		case 0:
		case 1:
		case 2:
			operation = (WISE::WeatherProto::WeatherGridFilter_GridTypeTwo_Operation)m_poly_wd_op;
			break;
		default:
			operation = WISE::WeatherProto::WeatherGridFilter_GridTypeTwo_Operation::WeatherGridFilter_GridTypeTwo_Operation_Disable;
			break;
		}

		if (operation != WISE::WeatherProto::WeatherGridFilter_GridTypeTwo_Operation_Disable)
		{
			auto grid = new WISE::WeatherProto::WeatherGridFilter_GridTypeTwo();
			grid->set_version(1);
			grid->set_operation(operation);
			if (!m_poly_wd_op)
				grid->set_allocated_value(DoubleBuilder().withValue(CARTESIAN_TO_COMPASS_DEGREE(RADIAN_TO_DEGREE(m_poly_wd_val))).forProtobuf(options.useVerboseFloats()));
			else
				grid->set_allocated_value(DoubleBuilder().withValue(RADIAN_TO_DEGREE(m_poly_wd_val)).forProtobuf(options.useVerboseFloats()));
			filter->set_allocated_winddirection(grid);
		}
	}

	return filter;
}


CCWFGM_WeatherGridFilter *CCWFGM_WeatherGridFilter::deserialize(const google::protobuf::Message& proto, std::shared_ptr<validation::validation_object> valid, const std::string& name)
{
	return deserialize(proto, valid, name, nullptr);
}

CCWFGM_WeatherGridFilter *CCWFGM_WeatherGridFilter::deserialize(const google::protobuf::Message& proto, std::shared_ptr<validation::validation_object> valid, const std::string& name, ISerializationData* data)
{
	boost::intrusive_ptr<ICWFGM_GridEngine> gridEngine;
	if (!(gridEngine = m_gridEngine(nullptr))) {
		if (valid)
			valid->add_child_validation("WISE.WeatherProto.WeatherGridFilter", name, validation::error_level::WARNING, validation::id::initialization_incomplete, "gridengine");
		weak_assert(0);
		m_loadWarning = "Error: WISE.WeatherProto.CwfgmWeatherGridFilter: No grid engine";
		throw ISerializeProto::DeserializeError("WISE.GridProto.CwfgmPolyReplaceGridFilter: Incomplete initialization");
	}

	auto sdata = dynamic_cast<SerializeWeatherGridFilterData*>(data);
	auto filter = dynamic_cast_assert<const WISE::WeatherProto::WeatherGridFilter*>(&proto);
	WTime ullvalue(m_timeManager);

	if (!filter)
	{
		if (valid)
			valid->add_child_validation("WISE.WeatherProto.WeatherGridFilter", name, validation::error_level::SEVERE, validation::id::object_invalid, proto.GetDescriptor()->name());
		weak_assert(0);
		m_loadWarning = "Error: WISE.WeatherProto.CwfgmWeatherGridFilter: Protobuf object invalid";
		throw ISerializeProto::DeserializeError("WISE.WeatherProto.CwfgmWeatherGridFilter: Protobuf object invalid", ERROR_PROTOBUF_OBJECT_INVALID);
	}

	if ((filter->version() != 1) && (filter->version() != 2))
	{
		if (valid)
			valid->add_child_validation("WISE.WeatherProto.WeatherGridFilter", name, validation::error_level::SEVERE, validation::id::version_mismatch, std::to_string(filter->version()));
		weak_assert(0);
		m_loadWarning = "Error: WISE.WeatherProto.CwfgmWeatherGridFilter: Version is invalid";
		throw ISerializeProto::DeserializeError("WISE.WeatherProto.CwfgmWeatherGridFilter: Version is invalid", ERROR_PROTOBUF_OBJECT_VERSION_INVALID);
	}

	PolymorphicAttribute var;

	if (FAILED(gridEngine->GetAttribute(0, CWFGM_GRID_ATTRIBUTE_SPATIALREFERENCE, &var))) {
		if (valid)
			/// <summary>
			/// The plot resolution is not readable but should be by this time in deserialization.
			/// </summary>
			/// <type>internal</type>
			valid->add_child_validation("WISE.WeatherProto.WeatherGridFilter", name, validation::error_level::SEVERE,
				validation::id::initialization_incomplete, "projection");
		weak_assert(0);
		m_loadWarning = "Error: WISE.WeatherProto.WeatherGridFilter: Incomplete initialization";
		throw ISerializeProto::DeserializeError("WISE.WeatherProto.WeatherGridFilter: Incomplete initialization");
	}

	auto vt = validation::conditional_make_object(valid, "WISE.WeatherProto.WeatherGridFilter", name);
	auto v = vt.lock();

	std::string projection;
	projection = std::get<std::string>(var);

	boost::scoped_ptr<CCoordinateConverter> convert(new CCoordinateConverter());
	convert->SetSourceProjection(projection.c_str());

	if (filter->shape_case() == WISE::WeatherProto::WeatherGridFilter::kLandscape)
		m_landscape = true; 
	else if (filter->shape_case() == WISE::WeatherProto::WeatherGridFilter::kPolygons)
	{
		if (filter->has_polygons())
		{
			GeoPoly geo(filter->polygons(), GeoPoly::TYPE_LINKED_LIST);
			geo.setStoredUnits(GeoPoly::UTM);
			geo.setConverter([&convert](std::uint8_t type, double x, double y, double z) -> std::tuple<double, double, double>
			{
				XY_Point loc = convert->start()
					.fromPoints(x, y, z)
					.asLatLon()
					.endInUTM()
					.to2DPoint();
				return std::make_tuple(loc.x, loc.y, 0.0);
			});
			m_polySet.RemoveAllPolys();
			XY_PolyLLSet* set = geo.getLinkedList(true, v, "polygons");
			if (set) {
				XY_PolyLL* p;
				while (p = set->RemHead())
					m_polySet.AddPoly(p);
				delete set;
			}
		}
	}
	else if (filter->shape_case() == WISE::WeatherProto::WeatherGridFilter::kFilename)
	{
		if (fs::exists(fs::relative(filter->filename())) && sdata)
		{
			HRESULT hr;
			auto vt2 = validation::conditional_make_object(v, "WISE.WeatherProto.WeatherGridFilter.shape", name);
			auto v2 = vt2.lock();

			if (FAILED(hr = ImportPolygons(filter->filename(), sdata->permissible_drivers))) {
				if (v2) {
					switch (hr) {
					case E_POINTER:						v2->add_child_validation("string", "shape.filename", validation::error_level::SEVERE, validation::id::e_pointer, filter->filename()); break;
					case E_INVALIDARG:					v2->add_child_validation("string", "shape.filename", validation::error_level::SEVERE, validation::id::e_invalidarg, filter->filename()); break;
					case E_OUTOFMEMORY:					v2->add_child_validation("string", "shape.filename", validation::error_level::SEVERE, validation::id::out_of_memory, filter->filename()); break;
					case ERROR_GRID_UNINITIALIZED:		v2->add_child_validation("string", "shape.filename", validation::error_level::SEVERE, validation::id::initialization_incomplete, "grid_engine"); break;
					case ERROR_FILE_NOT_FOUND:			v2->add_child_validation("string", "shape.filename", validation::error_level::SEVERE, validation::id::file_not_found, filter->filename()); break;
					case ERROR_TOO_MANY_OPEN_FILES:		v2->add_child_validation("string", "shape.filename", validation::error_level::SEVERE, validation::id::too_many_open_files, filter->filename()); break;
					case ERROR_ACCESS_DENIED:			v2->add_child_validation("string", "shape.filename", validation::error_level::SEVERE, validation::id::access_denied, filter->filename()); break;
					case ERROR_INVALID_HANDLE:			v2->add_child_validation("string", "shape.filename", validation::error_level::SEVERE, validation::id::invalid_handle, filter->filename()); break;
					case ERROR_HANDLE_DISK_FULL:		v2->add_child_validation("string", "shape.filename", validation::error_level::SEVERE, validation::id::disk_full, filter->filename()); break;
					case ERROR_FILE_EXISTS:				v2->add_child_validation("string", "shape.filename", validation::error_level::SEVERE, validation::id::file_exists, filter->filename()); break;
					case ERROR_SEVERITY_WARNING:
					case ERROR_FIREBREAK_NOT_FOUND:
					case S_FALSE:
					default:							v2->add_child_validation("string", "shape.filename", validation::error_level::SEVERE, validation::id::unspecified, filter->filename()); break;
					}
				}
				else
					return nullptr;
			}
		}
	}

	if (filter->has_starttime())
	{
		auto time = HSS_Time::Serialization::TimeSerializer::deserializeTime(filter->starttime(), m_timeManager, valid, "startTime");
		m_lStartTime = HSS_Time::WTime(*time);
		delete time;
	}
	if ((m_lStartTime < WTime::GlobalMin(m_timeManager)) || (m_lStartTime > WTime::GlobalMax(m_timeManager)))
	{
		m_loadWarning = "Error: WISE.WeatherProto.WeatherGridFilter: Invalid start time";
		if (v)
			/// <summary>
			/// The start time is out of range or invalid.
			/// </summary>
			/// <type>user</type>
			v->add_child_validation("HSS.Times.WTime", "startTime", validation::error_level::WARNING, validation::id::time_invalid, m_lStartTime.ToString(WTIME_FORMAT_STRING_ISO8601), { true, WTime::GlobalMin().ToString(WTIME_FORMAT_STRING_ISO8601) }, { true, WTime::GlobalMax().ToString(WTIME_FORMAT_STRING_ISO8601) });
		else
			throw ISerializeProto::DeserializeError("WISE.WeatherProto.WeatherGridFilter: Invalid start time");;
	}
	if (m_lStartTime.GetMicroSeconds(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)) {
		m_loadWarning += "Warning: fractions of seconds on the start time will be purged to the start of the minute.";
		if (v)
			/// <summary>
			/// The start time contains fractions of seconds.
			/// </summary>
			/// <type>user</type>
			v->add_child_validation("HSS.Times.WTime", "startTime", validation::error_level::WARNING, validation::id::time_invalid, m_lStartTime.ToString(WTIME_FORMAT_STRING_ISO8601), "Fractions of seconds will be purged.");
		m_lStartTime.PurgeToSecond(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	}

	if (filter->has_endtime())
	{
		auto time = HSS_Time::Serialization::TimeSerializer::deserializeTime(filter->endtime(), m_timeManager, valid, "endTime");
		m_lEndTime = HSS_Time::WTime(*time);
		delete time;
	}
	if ((m_lEndTime < WTime::GlobalMin(m_timeManager)) || (m_lEndTime > WTime::GlobalMax(m_timeManager)))
	{
		m_loadWarning = "Error: WISE.WeatherProto.WeatherGridFilter: Invalid end time";
		if (v)
			/// <summary>
			/// The end time is out of range or invalid.
			/// </summary>
			/// <type>user</type>
			v->add_child_validation("HSS.Times.WTime", "endTime", validation::error_level::WARNING, validation::id::time_invalid, m_lEndTime.ToString(WTIME_FORMAT_STRING_ISO8601), { true, WTime::GlobalMin().ToString(WTIME_FORMAT_STRING_ISO8601) }, { true, WTime::GlobalMax().ToString(WTIME_FORMAT_STRING_ISO8601) });
		else
			throw ISerializeProto::DeserializeError("WISE.WeatherProto.WeatherGridFilter: Invalid end time");;
	}
	if (m_lEndTime.GetMicroSeconds(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST)) {
		m_loadWarning += "Warning: fractions of seconds on the end time will be purged to the start of the minute.";
		if (v)
			/// <summary>
			/// The end time contains fractions of seconds.
			/// </summary>
			/// <type>user</type>
			v->add_child_validation("HSS.Times.WTime", "endTime", validation::error_level::WARNING, validation::id::time_invalid, m_lEndTime.ToString(WTIME_FORMAT_STRING_ISO8601), "Fractions of seconds will be purged.");
		m_lEndTime.PurgeToSecond(WTIME_FORMAT_AS_LOCAL | WTIME_FORMAT_WITHDST);
	}

	if (m_lStartTime > m_lEndTime)
	{
		m_loadWarning = "Error: WISE.WeatherProto.WeatherGridFiltero: Invalid times";
		if (v) {
			if (m_lStartTime > m_lEndTime) {
				/// <summary>
				/// The start time occurs after the end time.
				/// </summary>
				/// <type>user</type>
				v->add_child_validation("HSS.Times.WTime", { "startTime", "endTime" }, validation::error_level::WARNING, validation::id::time_invalid, { m_lStartTime.ToString(WTIME_FORMAT_STRING_ISO8601), m_lEndTime.ToString(WTIME_FORMAT_STRING_ISO8601) });
			}
		}
		else
			throw ISerializeProto::DeserializeError("WISE.WeatherProto.WeatherGridFilter: Invalid times");
	}

	if (filter->has_temperature())
	{
		m_poly_temp_op = filter->temperature().operation();
		if (filter->temperature().has_value())
			m_poly_temp_val = DoubleBuilder().withProtobuf(filter->temperature().value()).getValue();
	}
	else
		m_poly_temp_op = (std::uint16_t)-1;

	if (filter->has_rh())
	{
		m_poly_rh_op = filter->rh().operation();
		if (filter->rh().has_value())
			m_poly_rh_val = DoubleBuilder().withProtobuf(filter->rh().value()).getValue();
	}
	else
		m_poly_rh_op = (std::uint16_t)-1;

	if (filter->has_precipitation())
	{
		m_poly_precip_op = filter->precipitation().operation();
		if (filter->precipitation().has_value())
			m_poly_precip_val = DoubleBuilder().withProtobuf(filter->precipitation().value()).getValue();
	}
	else
		m_poly_precip_op = (std::uint16_t)-1;

	if (filter->has_windspeed())
	{
		m_poly_ws_op = filter->windspeed().operation();
		if (filter->windspeed().has_value())
			m_poly_ws_val = DoubleBuilder().withProtobuf(filter->windspeed().value()).getValue();
	}
	else
		m_poly_ws_op = (std::uint16_t)-1;

	if (filter->has_winddirection())
	{
		m_poly_wd_op = filter->winddirection().operation();
		if (filter->winddirection().val_case() == WISE::WeatherProto::WeatherGridFilter_GridTypeTwo::kValue) {
			m_poly_wd_val = DEGREE_TO_RADIAN(DoubleBuilder().withProtobuf(filter->winddirection().value()).getValue());
		}
		else if (filter->winddirection().val_case() == WISE::WeatherProto::WeatherGridFilter_GridTypeTwo::kDirection)
		{
			switch (filter->winddirection().direction())
			{
			case WISE::WeatherProto::WeatherGridFilter_GridTypeTwo_Direction_NORTH:
				m_poly_wd_val = DEGREE_TO_RADIAN(0.0);
				break;
			case WISE::WeatherProto::WeatherGridFilter_GridTypeTwo_Direction_NORTH_EAST:
				m_poly_wd_val = DEGREE_TO_RADIAN(45.0);
				break;
			case WISE::WeatherProto::WeatherGridFilter_GridTypeTwo_Direction_EAST:
				m_poly_wd_val = DEGREE_TO_RADIAN(90.0);
				break;
			case WISE::WeatherProto::WeatherGridFilter_GridTypeTwo_Direction_SOUTH_EAST:
				m_poly_wd_val = DEGREE_TO_RADIAN(135.0);
				break;
			case WISE::WeatherProto::WeatherGridFilter_GridTypeTwo_Direction_SOUTH:
				m_poly_wd_val = DEGREE_TO_RADIAN(180.0);
				break;
			case WISE::WeatherProto::WeatherGridFilter_GridTypeTwo_Direction_SOUTH_WEST:
				m_poly_wd_val = DEGREE_TO_RADIAN(225.0);
				break;
			case WISE::WeatherProto::WeatherGridFilter_GridTypeTwo_Direction_WEST:
				m_poly_wd_val = DEGREE_TO_RADIAN(270.0);
				break;
			case WISE::WeatherProto::WeatherGridFilter_GridTypeTwo_Direction_NORTH_WEST:
				m_poly_wd_val = DEGREE_TO_RADIAN(315.0);
				break;
            default:
                weak_assert(0);
                break;
			}
		}
		if (!m_poly_wd_op)	// if it's a basic assignment, not + or -
			m_poly_wd_val = COMPASS_TO_CARTESIAN_RADIAN(m_poly_wd_val);
	}
	else
		m_poly_wd_op = (std::uint16_t)-1;

	if (m_resolution != 1.0)
		m_polySet.SetCacheScale(m_resolution);

	return this;
}
