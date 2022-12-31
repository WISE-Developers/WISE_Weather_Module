/**
 * WISE_Weather_Module: WeatherUtilities.h
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

#include "angles.h"
#include "WTime.h"
#include "linklist.h"
#include "GridCom_ext.h"
#include "FwiCom.h"
#include "valuecache_mt.h"
#include "objectcache_mt.h"
#include "CoordinateConverter.h"
#include <map>
#include "CWFGM_LayerManager.h"

#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(push, 4)			// force to go to 4-byte packing rules,
#endif					// to make this class as small as possible

using namespace HSS_Time;


struct WeatherKeyBase {
	const HSS_Time::WTime time;
	std::uint64_t interpolate_method;

	WeatherKeyBase(const HSS_Time::WTime &t) : time(t) { }
};


struct WeatherKey {
	WeatherKey(std::uint16_t _x, std::uint16_t _y, const HSS_Time::WTime &_time, std::uint64_t _interpolate_method, Layer * _layerThread) : time(_time)
		{ x = _x; y = _y; interpolate_method = _interpolate_method; layerThread = _layerThread; }
	std::uint16_t x, y;
	std::uint64_t interpolate_method;
	const HSS_Time::WTime time;
	Layer *layerThread;
};


struct WeatherData {
	HRESULT hr;
	IWXData wx;
	IFWIData ifwi;
	DFWIData dfwi;
	bool wx_valid;
};


struct HIWXData {
	HRESULT hr;
	IWXData wx;
	bool wx_valid;
};


struct HIFWIData {
	HRESULT hr;
	IFWIData ifwi;
	bool wx_valid;
};


struct HDFWIData {
	HRESULT hr;
	DFWIData dfwi;
	bool wx_valid;
};


class WeatherBaseCache {
	ValueCacheTempl<WeatherKeyBase, WeatherData> m_cacheDay, m_cacheNoon, m_cacheHour, m_cacheSec;
	ValueCacheTempl<WeatherKeyBase, HIWXData> m_iwxDay, m_iwxNoon, m_iwxHour, m_iwxSec;
	ValueCacheTempl<WeatherKeyBase, HIFWIData> m_ifwiDay, m_ifwiNoon, m_ifwiHour, m_ifwiSec;
	ValueCacheTempl<WeatherKeyBase, HDFWIData> m_dfwiDay, m_dfwiNoon, m_dfwiHour, m_dfwiSec;

	ValueCacheTempl<WeatherKeyBase, WeatherData> *getCache(const HSS_Time::WTime &time, const WTimeManager *tm);
	ValueCacheTempl<WeatherKeyBase, HIWXData> *getCacheWx(const HSS_Time::WTime &time, const WTimeManager *tm);
	ValueCacheTempl<WeatherKeyBase, HIFWIData> *getCacheIfwi(const HSS_Time::WTime &time, const WTimeManager *tm);
	ValueCacheTempl<WeatherKeyBase, HDFWIData> *getCacheDfwi(const HSS_Time::WTime &time, const WTimeManager *tm);

public:
	WeatherBaseCache() :	m_cacheDay(4), m_cacheNoon(4), m_cacheHour(28), m_cacheSec(8),
				m_iwxDay(4), m_iwxNoon(4), m_iwxHour(28), m_iwxSec(8),
				m_ifwiDay(4), m_ifwiNoon(4), m_ifwiHour(28), m_ifwiSec(8),
				m_dfwiDay(4), m_dfwiNoon(4), m_dfwiHour(28), m_dfwiSec(8) { m_createdIndex = (std::uint32_t)-1; };

	void Store(const WeatherKeyBase *_key, const WeatherData *_answer, const WTimeManager *tm);
	void Store(const WeatherKeyBase *_key, const HIWXData *_answer, const WTimeManager *tm);
	void Store(const WeatherKeyBase *_key, const HIFWIData *_answer, const WTimeManager *tm);
	void Store(const WeatherKeyBase *_key, const HDFWIData *_answer, const WTimeManager *tm);

	WeatherData *Retrieve(const WeatherKeyBase *_key, WeatherData *_to_fill, const WTimeManager *tm);
	HIWXData *Retrieve(const WeatherKeyBase *_key, HIWXData *_to_fill, const WTimeManager *tm);
	HIFWIData *Retrieve(const WeatherKeyBase *_key, HIFWIData *_to_fill, const WTimeManager *tm);
	HDFWIData *Retrieve(const WeatherKeyBase *_key, HDFWIData *_to_fill, const WTimeManager *tm);

	void Clear();
	bool Purge(const HSS_Time::WTime &time);

	std::uint32_t m_createdIndex;

	DECLARE_OBJECT_CACHE_MT(WeatherBaseCache, WeatherBaseCache)
};


class WeatherBaseCache_MT : WeatherBaseCache {
	CThreadSemaphore m_lock;

public:
	WeatherBaseCache_MT() = default;

	void Store(const WeatherKeyBase *_key, const WeatherData *_answer, const WTimeManager *tm)	{ m_lock.Lock(); WeatherBaseCache::Store(_key, _answer, tm); m_lock.Unlock(); };
	void Store(const WeatherKeyBase *_key, const HIWXData *_answer, const WTimeManager *tm)		{ m_lock.Lock(); WeatherBaseCache::Store(_key, _answer, tm); m_lock.Unlock(); };
	void Store(const WeatherKeyBase *_key, const HIFWIData *_answer, const WTimeManager *tm)		{ m_lock.Lock(); WeatherBaseCache::Store(_key, _answer, tm); m_lock.Unlock(); };
	void Store(const WeatherKeyBase *_key, const HDFWIData *_answer, const WTimeManager *tm)		{ m_lock.Lock(); WeatherBaseCache::Store(_key, _answer, tm); m_lock.Unlock(); };

	WeatherData *Retrieve(const WeatherKeyBase *_key, WeatherData *_to_fill, const WTimeManager *tm)	{ m_lock.Lock(); WeatherData *wd = WeatherBaseCache::Retrieve(_key, _to_fill, tm); m_lock.Unlock(); return wd; };
	HIWXData *Retrieve(const WeatherKeyBase *_key, HIWXData *_to_fill, const WTimeManager *tm)	{ m_lock.Lock(); HIWXData *wd = WeatherBaseCache::Retrieve(_key, _to_fill, tm); m_lock.Unlock(); return wd; };
	HIFWIData *Retrieve(const WeatherKeyBase *_key, HIFWIData *_to_fill, const WTimeManager *tm)	{ m_lock.Lock(); HIFWIData *wd = WeatherBaseCache::Retrieve(_key, _to_fill, tm); m_lock.Unlock(); return wd; };
	HDFWIData *Retrieve(const WeatherKeyBase *_key, HDFWIData *_to_fill, const WTimeManager *tm)	{ m_lock.Lock(); HDFWIData *wd = WeatherBaseCache::Retrieve(_key, _to_fill, tm); m_lock.Unlock(); return wd; };
	
	void Clear()											{ m_lock.Lock(); WeatherBaseCache::Clear(); m_lock.Unlock(); };
	bool Purge(const HSS_Time::WTime &time)									{ m_lock.Lock(); bool b = WeatherBaseCache::Purge(time); m_lock.Unlock(); return b; };

	DECLARE_OBJECT_CACHE_MT(WeatherBaseCache_MT, WeatherBaseCache_MT)
};

#ifdef HSS_SHOULD_PRAGMA_PACK
#pragma pack(pop)
#endif


class WeatherLayerCache {
	struct WEntry {
		std::uint16_t x, y;

#ifdef _DEBUG
		WeatherBaseCache *m_ptr;
#endif

	};

private:
	CThreadSemaphore m_lock;
	WeatherBaseCache **m_cacheArray;
	WEntry *m_created;
	std::uint16_t m_xsize, m_ysize;
	std::uint32_t m_begin, m_end, m_max;


	__INLINE std::uint32_t arrayIndex(std::uint16_t x, std::uint16_t y) const {
		weak_assert(x < m_xsize);
		weak_assert(y < m_ysize);
		return y * m_xsize + x;
	}
	WeatherBaseCache *cache(std::uint16_t x, std::uint16_t y);

public:
	WeatherLayerCache(std::uint16_t x, std::uint16_t y, std::uint32_t max_cache_entries, WTimeManager *tm);
	~WeatherLayerCache();

	void Store(const WeatherKey *_key, const WeatherData *_answer, const WTimeManager *tm);
	void Store(const WeatherKey *_key, const HIWXData *_answer, const WTimeManager *tm);
	void Store(const WeatherKey *_key, const HIFWIData *_answer, const WTimeManager *tm);
	void Store(const WeatherKey *_key, const HDFWIData *_answer, const WTimeManager *tm);

	WeatherData *Retrieve(const WeatherKey *_key, WeatherData *_to_fill, const WTimeManager *tm);
	HIWXData *Retrieve(const WeatherKey *_key, HIWXData *_to_fill, const WTimeManager *tm);
	HIFWIData *Retrieve(const WeatherKey *_key, HIFWIData *_to_fill, const WTimeManager *tm);
	HDFWIData *Retrieve(const WeatherKey *_key, HDFWIData *_to_fill, const WTimeManager *tm);

	void Clear();
	void PurgeOld(const HSS_Time::WTime &time);
	bool Exists(std::uint16_t x, std::uint16_t y);

	HSS_Time::WTime m_equilibriumTime;

	DECLARE_OBJECT_CACHE_MT(WeatherLayerCache, WeatherLayerCache)

	std::atomic<std::uint32_t> m_refCount = 0;
};


class WeatherCache {
private:
	std::map<Layer *, WeatherLayerCache *> m_weatherLayerMap[2];

	WeatherLayerCache *cache(Layer *layerThread, std::uint16_t cacheIndex);

	HSS_Time::WTimeManager *m_tm;

public:
	WeatherCache(HSS_Time::WTimeManager *_tm) { m_tm = _tm; };
	~WeatherCache();

	void Store(std::uint16_t cacheIndex, const WeatherKey *_key, const WeatherData *_answer, const WTimeManager *tm);
	void Store(std::uint16_t cacheIndex, const WeatherKey *_key, const HIWXData *_answer, const WTimeManager *tm);
	void Store(std::uint16_t cacheIndex, const WeatherKey *_key, const HIFWIData *_answer, const WTimeManager *tm);
	void Store(std::uint16_t cacheIndex, const WeatherKey *_key, const HDFWIData *_answer, const WTimeManager *tm);

	WeatherData *Retrieve(std::uint16_t cacheIndex, const WeatherKey *_key, WeatherData *_to_fill, const WTimeManager *tm);
	HIWXData *Retrieve(std::uint16_t cacheIndex, const WeatherKey *_key, HIWXData *_to_fill, const WTimeManager *tm);
	HIFWIData *Retrieve(std::uint16_t cacheIndex, const WeatherKey *_key, HIFWIData *_to_fill, const WTimeManager *tm);
	HDFWIData *Retrieve(std::uint16_t cacheIndex, const WeatherKey *_key, HDFWIData *_to_fill, const WTimeManager *tm);

	void Add(Layer *layerThread, std::uint16_t cacheIndex, std::uint16_t x_size, std::uint16_t y_size);
	bool Exists(Layer *layerThread, std::uint16_t cacheIndex);
	void Remove(Layer *layerThread, std::uint16_t cacheIndex);
	void Clear(Layer *layerThread, std::uint16_t cacheIndex);
	void PurgeOld(Layer *layerThread, std::uint16_t cacheIndex, const HSS_Time::WTime &time);
	bool Exists(Layer *layerThread, std::uint16_t cacheIndex, std::uint16_t x, std::uint16_t y);

	std::uint32_t Increment(Layer* layerThread, std::uint16_t cacheIndex);
	std::uint32_t Decrement(Layer* layerThread, std::uint16_t cacheIndex);

	void EquilibriumDepth(Layer *layerThread, std::uint16_t cacheIndex, const HSS_Time::WTime &time);
	HSS_Time::WTime EquilibriumDepth(Layer *layerThread, std::uint16_t cacheIndex);

	std::uint32_t CacheEntries() const;

	void SetTimeManager(WTimeManager* tm);
};


class WeatherUtilities {
public:
	WeatherUtilities(WTimeManager *tm);
	virtual HRESULT GetRawWxValues(ICWFGM_GridEngine *gridEngine, Layer *layerThread, const HSS_Time::WTime &time, const XY_Point &pt, std::uint64_t interpolate_method, IWXData *wx, bool *wx_valid) = 0;
	virtual HRESULT GetRawDFWIValues(ICWFGM_GridEngine *gridEngine, Layer *layerThread, const HSS_Time::WTime &time, const XY_Point &pt, std::uint64_t interpolate_method, std::uint32_t WX_SpecifiedBits, DFWIData *dfwi, bool *wx_valid) = 0;
	virtual HRESULT GetRawIFWIValues(ICWFGM_GridEngine *gridEngine, Layer *layerThread, const HSS_Time::WTime &time, const XY_Point &pt, std::uint64_t interpolate_method, std::uint32_t WX_SpecifiedBits, IFWIData *ifwi, bool *wx_valid) = 0;

	HRESULT GetCalculatedValues(ICWFGM_GridEngine *gridEngine, Layer *layerThread, const XY_Point &pt, WeatherKey &key, WeatherData &data);
	HRESULT GetCalculatedDFWIValues(ICWFGM_GridEngine *gridEngine, Layer *layerThread, const HSS_Time::WTime &time, const XY_Point &pt, double lat, double lon, std::uint64_t interpolate_method, const IWXData *wx, DFWIData *t_dfwi, DFWIData *p_dfwi = NULL);
	HRESULT GetCalculatedIFWIValues(ICWFGM_GridEngine *gridEngine, Layer *layerThread, const HSS_Time::WTime &time, const XY_Point &pt, double lat, double lon, std::uint64_t interpolate_method, const IWXData *wx, IFWIData *ifwi);

	void AddCache(Layer *layerThread, std::uint16_t cacheIndex, std::uint16_t x_size, std::uint16_t y_size);
	void RemoveCache(Layer *layerThread, std::uint16_t cacheIndex);
	void ClearCache(Layer *layerThread, std::uint16_t cacheIndex);
	void PurgeOldCache(Layer *layerThread, std::uint16_t cacheIndex, const HSS_Time::WTime &time);
	bool CacheExists(Layer *layerThread, std::uint16_t cacheIndex);

	std::uint32_t IncrementCache(Layer* layerThread, std::uint16_t cacheIndex);
	std::uint32_t DecrementCache(Layer* layerThread, std::uint16_t cacheIndex);

	void SetEquilibriumLimit(Layer *layerThread, std::uint16_t cacheIndex, const HSS_Time::WTime &time);

	CCoordinateConverter m_converter;

	ICWFGM_FWI		m_fwi;

protected:
	WTimeManager *m_tm;

	WeatherCache m_cache;
};


HRESULT SetWorldLocation(ICWFGM_GridEngine *gridEngine, Layer *layerThread, WorldLocation &m_worldLocation);
