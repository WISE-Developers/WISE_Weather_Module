/**
 * WISE_Weather_Module: WeatherCache.cpp
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

#include "WeatherCom_ext.h"
#include "WeatherUtilities.h"
#include "propsysreplacement.h"
#include "points.h"
#include "FireEngine_ext.h"
#include <stdio.h>
#include <stdlib.h>


IMPLEMENT_OBJECT_CACHE_MT_NO_TEMPLATE(WeatherBaseCache, WeatherBaseCache, 4 * 1024 * 1024 / sizeof(WeatherBaseCache), true, 16)
IMPLEMENT_OBJECT_CACHE_MT_NO_TEMPLATE(WeatherBaseCache_MT, WeatherBaseCache_MT, 1024 / sizeof(WeatherBaseCache_MT), true, 16)


ValueCacheTempl<WeatherKeyBase, WeatherData> *WeatherBaseCache::getCache(const HSS_Time::WTime &time, const WTimeManager *tm) {
	WTime t(time), pt(t);

	pt.PurgeToDay(WTIME_FORMAT_AS_LOCAL);
	if (t == pt)
		return &m_cacheDay;
	else {
		pt += WTimeSpan(12 * 60 * 60);
		if (t == pt)
			return &m_cacheNoon;
		else {
			pt = t;
			pt.PurgeToHour(WTIME_FORMAT_AS_LOCAL);
			if (t == pt)
				return &m_cacheHour;
			else	return &m_cacheSec;
		}
	}
}


ValueCacheTempl<WeatherKeyBase, HIWXData> *WeatherBaseCache::getCacheWx(const HSS_Time::WTime &time, const WTimeManager *tm) {
	WTime t(time), pt(t);

	pt.PurgeToDay(WTIME_FORMAT_AS_LOCAL);
	if (t == pt)
		return &m_iwxDay;
	else {
		pt += WTimeSpan(12 * 60 * 60);
		if (t == pt)
			return &m_iwxNoon;
		else {
			pt = t;
			pt.PurgeToHour(WTIME_FORMAT_AS_LOCAL);
			if (t == pt)
				return &m_iwxHour;
			else	return &m_iwxSec;
		}
	}
}


ValueCacheTempl<WeatherKeyBase, HIFWIData> *WeatherBaseCache::getCacheIfwi(const HSS_Time::WTime &time, const WTimeManager *tm) {
	WTime t(time), pt(t);

	pt.PurgeToDay(WTIME_FORMAT_AS_LOCAL);
	if (t == pt)
		return &m_ifwiDay;
	else {
		pt += WTimeSpan(12 * 60 * 60);
		if (t == pt)
			return &m_ifwiNoon;
		else {
			pt = t;
			pt.PurgeToHour(WTIME_FORMAT_AS_LOCAL);
			if (t == pt)
				return &m_ifwiHour;
			else	return &m_ifwiSec;
		}
	}
}


ValueCacheTempl<WeatherKeyBase, HDFWIData> *WeatherBaseCache::getCacheDfwi(const HSS_Time::WTime &time, const WTimeManager *tm) {
	WTime t(time), pt(t);

	pt.PurgeToDay(WTIME_FORMAT_AS_LOCAL);
	if (t == pt)
		return &m_dfwiDay;
	else {
		pt += WTimeSpan(12 * 60 * 60);
		if (t == pt)
			return &m_dfwiNoon;
		else {
			pt = t;
			pt.PurgeToHour(WTIME_FORMAT_AS_LOCAL);
			if (t == pt)
				return &m_dfwiHour;
			else	return &m_dfwiSec;
		}
	}
}


void WeatherBaseCache::Store(const WeatherKeyBase *_key, const WeatherData *_answer, const WTimeManager *tm) {
	ValueCacheTempl<WeatherKeyBase, WeatherData> *cache = getCache(_key->time, tm);
	cache->Store(_key, _answer);
}


void WeatherBaseCache::Store(const WeatherKeyBase *_key, const HIWXData *_answer, const WTimeManager *tm) {
	ValueCacheTempl<WeatherKeyBase, HIWXData> *cache = getCacheWx(_key->time, tm);
	cache->Store(_key, _answer);
}


void WeatherBaseCache::Store(const WeatherKeyBase *_key, const HIFWIData *_answer, const WTimeManager *tm) {
	ValueCacheTempl<WeatherKeyBase, HIFWIData> *cache = getCacheIfwi(_key->time, tm);
	cache->Store(_key, _answer);
}


void WeatherBaseCache::Store(const WeatherKeyBase *_key, const HDFWIData *_answer, const WTimeManager *tm) {
	ValueCacheTempl<WeatherKeyBase, HDFWIData> *cache = getCacheDfwi(_key->time, tm);
	cache->Store(_key, _answer);
}


void WeatherBaseCache::Clear() {
	m_cacheDay.Clear();
	m_cacheNoon.Clear();
	m_cacheHour.Clear();
	m_cacheSec.Clear();

	m_iwxDay.Clear();
	m_iwxNoon.Clear();
	m_iwxHour.Clear();
	m_iwxSec.Clear();

	m_ifwiDay.Clear();
	m_ifwiNoon.Clear();
	m_ifwiHour.Clear();
	m_ifwiSec.Clear();

	m_dfwiDay.Clear();
	m_dfwiNoon.Clear();
	m_dfwiHour.Clear();
	m_dfwiSec.Clear();
}


WeatherData *WeatherBaseCache::Retrieve(const WeatherKeyBase *_key, WeatherData *_to_fill, const WTimeManager *tm) {
	ValueCacheTempl<WeatherKeyBase, WeatherData> *cache = getCache(_key->time, tm);
	WeatherData *tf = cache->Retrieve(_key);
	if (tf) {
		*_to_fill = *tf;
		return _to_fill;
	}
	return NULL;
}


HIWXData *WeatherBaseCache::Retrieve(const WeatherKeyBase *_key, HIWXData *_to_fill, const WTimeManager *tm) {
	ValueCacheTempl<WeatherKeyBase, HIWXData> *cache = getCacheWx(_key->time, tm);
	HIWXData *tf = cache->Retrieve(_key);
	if (tf) {
		*_to_fill = *tf;
		return _to_fill;
	}
	return NULL;
}


HIFWIData *WeatherBaseCache::Retrieve(const WeatherKeyBase *_key, HIFWIData *_to_fill, const WTimeManager *tm) {
	ValueCacheTempl<WeatherKeyBase, HIFWIData> *cache = getCacheIfwi(_key->time, tm);
	HIFWIData *tf = cache->Retrieve(_key);
	if (tf) {
		*_to_fill = *tf;
		return _to_fill;
	}
	return NULL;
}


HDFWIData *WeatherBaseCache::Retrieve(const WeatherKeyBase *_key, HDFWIData *_to_fill, const WTimeManager *tm) {
	ValueCacheTempl<WeatherKeyBase, HDFWIData> *cache = getCacheDfwi(_key->time, tm);
	HDFWIData *tf = cache->Retrieve(_key);
	if (tf) {
		*_to_fill = *tf;
		return _to_fill;
	}
	return NULL;
}


IMPLEMENT_OBJECT_CACHE_MT_NO_TEMPLATE(WeatherLayerCache, WeatherLayerCache, 256 * 1024 / sizeof(WeatherLayerCache), false, 16)


WeatherLayerCache::WeatherLayerCache(std::uint16_t x, std::uint16_t y, std::uint32_t max_cache_entries, WTimeManager *tm) : m_equilibriumTime(0ULL, tm) {
	m_xsize = x;
	m_ysize = y;
	std::uint32_t size = ((std::uint32_t)m_xsize) * ((std::uint32_t)m_ysize);
	size_t allocsize = (size_t)size * sizeof(WeatherBaseCache *);
	m_cacheArray = (WeatherBaseCache **)malloc(allocsize);
	if (m_cacheArray)
		memset(m_cacheArray, 0, allocsize);

	m_begin = m_end = 0;
	m_max = max_cache_entries;
	allocsize = (size_t)m_max * sizeof(WEntry);
	m_created = (WEntry *)malloc(allocsize);
	if (m_created)
		memset(m_created, -1, allocsize);
}


WeatherLayerCache::~WeatherLayerCache() {
	Clear();
	if (m_cacheArray)
		free(m_cacheArray);
	if (m_created)
		free(m_created);
}


WeatherBaseCache *WeatherLayerCache::cache(std::uint16_t x, std::uint16_t y) {
	weak_assert(x < m_xsize);
	weak_assert(y < m_ysize);
	std::uint32_t index = arrayIndex(x, y);

	try {
		if (!m_cacheArray[index]) {
			m_cacheArray[index] = new WeatherBaseCache();

			WEntry we;
			we.x = x;
			we.y = y;

			m_created[m_begin] = we;
			m_cacheArray[index]->m_createdIndex = m_begin;
			m_begin = (m_begin + 1) % m_max;
			if (m_begin == m_end) {
				if ((m_created[m_end].x != (std::uint16_t)-1) && (m_created[m_end].y != (std::uint16_t)-1)) {
					std::uint32_t dindex = arrayIndex(m_created[m_end].x, m_created[m_end].y);

					m_created[m_end].x = (std::uint16_t)-1;
					m_created[m_end].y = (std::uint16_t)-1;

					delete m_cacheArray[dindex];
					m_cacheArray[dindex] = nullptr;
				}

				m_end = (m_end + 1) % m_max;
			}
		}
	} catch (std::bad_alloc& cme) {
		weak_assert(false);
		m_cacheArray[index] = NULL;
	}

	WeatherBaseCache *c = m_cacheArray[index];
	return c;
}


void WeatherLayerCache::Store(const WeatherKey *_key, const WeatherData *_answer, const WTimeManager *tm) {
	m_lock.Lock();
	WeatherBaseCache *c = cache(_key->x, _key->y);

	if (c) {
		WeatherKeyBase key(_key->time);
		key.interpolate_method = _key->interpolate_method;
		c->Store(&key, _answer, tm);
	}

	m_lock.Unlock();
}


void WeatherLayerCache::Store(const WeatherKey *_key, const HIWXData *_answer, const WTimeManager *tm) {
	m_lock.Lock();
	WeatherBaseCache *c = cache(_key->x, _key->y);

	if (c) {
		WeatherKeyBase key(_key->time);
		key.interpolate_method = _key->interpolate_method;
		c->Store(&key, _answer, tm);
	}

	m_lock.Unlock();
}


void WeatherLayerCache::Store(const WeatherKey *_key, const HIFWIData *_answer, const WTimeManager *tm) {
	m_lock.Lock();
	WeatherBaseCache *c = cache(_key->x, _key->y);

	if (c) {
		WeatherKeyBase key(_key->time);
		key.interpolate_method = _key->interpolate_method;
		c->Store(&key, _answer, tm);
	}

	m_lock.Unlock();
}


void WeatherLayerCache::Store(const WeatherKey *_key, const HDFWIData *_answer, const WTimeManager *tm) {
	m_lock.Lock();
	WeatherBaseCache *c = cache(_key->x, _key->y);

	if (c) {
		WeatherKeyBase key(_key->time);
		key.interpolate_method = _key->interpolate_method;
		c->Store(&key, _answer, tm);
	}

	m_lock.Unlock();
}


WeatherData *WeatherLayerCache::Retrieve(const WeatherKey *_key, WeatherData *_to_fill, const WTimeManager *tm) {
	m_lock.Lock();
	WeatherBaseCache *c = cache(_key->x, _key->y);
	WeatherData *wd = NULL;

	if (c) {
		WeatherKeyBase key(_key->time);
		key.interpolate_method = _key->interpolate_method;
		wd = c->Retrieve(&key, _to_fill, tm);
	}

	m_lock.Unlock();
	return wd;
}


HIWXData *WeatherLayerCache::Retrieve(const WeatherKey *_key, HIWXData *_to_fill, const WTimeManager *tm) {
	m_lock.Lock();
	WeatherBaseCache *c = cache(_key->x, _key->y);
	HIWXData *wd = NULL;

	if (c) {
		WeatherKeyBase key(_key->time);
		key.interpolate_method = _key->interpolate_method;
		wd = c->Retrieve(&key, _to_fill, tm);
	}

	m_lock.Unlock();
	return wd;
}


HIFWIData *WeatherLayerCache::Retrieve(const WeatherKey *_key, HIFWIData *_to_fill, const WTimeManager *tm) {
	m_lock.Lock();
	WeatherBaseCache *c = cache(_key->x, _key->y);
	HIFWIData *wd = NULL;

	if (c) {
		WeatherKeyBase key(_key->time);
		key.interpolate_method = _key->interpolate_method;
		wd = c->Retrieve(&key, _to_fill, tm);
	}

	m_lock.Unlock();
	return wd;
}


HDFWIData *WeatherLayerCache::Retrieve(const WeatherKey *_key, HDFWIData *_to_fill, const WTimeManager *tm) {
	m_lock.Lock();
	WeatherBaseCache *c = cache(_key->x, _key->y);
	HDFWIData *wd = NULL;

	if (c) {
		WeatherKeyBase key(_key->time);
		key.interpolate_method = _key->interpolate_method;
		wd = c->Retrieve(&key, _to_fill, tm);
	}

	m_lock.Unlock();
	return wd;
}


void WeatherLayerCache::Clear() {			// really, the above lock/unlock locations should be changed - after the actual Store/Retrieve, but we are going
	m_lock.Lock();					// to make the assumption that this routine will never be called asynchronously to the others
	std::uint32_t i, size = ((std::uint32_t)m_xsize) * ((std::uint32_t)m_ysize);
	for (i = 0; i < size; i++)
		if (m_cacheArray[i]) {
			m_created[m_cacheArray[i]->m_createdIndex].x = (std::uint16_t)-1;
			m_created[m_cacheArray[i]->m_createdIndex].y = (std::uint16_t)-1;

			delete m_cacheArray[i];
			m_cacheArray[i] = NULL;
		}

		m_begin = m_end = 0;

#ifdef _DEBUG
		size_t allocsize = (size_t)m_max * sizeof(WEntry);
		memset(m_created, -1, allocsize);
#endif


	m_lock.Unlock();
}


struct iterateStruct {
	const HSS_Time::WTime time;
	std::uint64_t cnt;

	iterateStruct(const WTime &t) : time(t) { };
};


static bool iterateMe(APTR parm, WeatherKeyBase *key) {
	struct iterateStruct *is = (struct iterateStruct *)parm;
	if (key->time >= is->time) {
		is->cnt++;
		return false;
	}
	return true;
}


bool WeatherBaseCache::Purge(const HSS_Time::WTime &time) {
	struct iterateStruct is(time - WTimeSpan(2 * 60 * 60));	// keep caches around for at least 2 hours after points move on, so we can deal with joins, etc.
	is.cnt = 0;
	m_cacheSec.Iterate(iterateMe, &is);
	if (is.cnt)
		return false;
	m_cacheHour.Iterate(iterateMe, &is);
	if (is.cnt)
		return false;
	m_cacheNoon.Iterate(iterateMe, &is);
	if (is.cnt)
		return false;
	m_cacheDay.Iterate(iterateMe, &is);
	if (is.cnt)
		return false;
	return true;
}


void WeatherLayerCache::PurgeOld(const HSS_Time::WTime &time) {
	m_lock.Lock();					// to make the assumption that this routine will never be called asynchronously to the others
	std::uint32_t i, size = ((std::uint32_t)m_xsize) * ((std::uint32_t)m_ysize);
	for (i = 0; i < size; i++)
		if (m_cacheArray[i]) {
			if (m_cacheArray[i]->Purge(time)) {
				m_created[m_cacheArray[i]->m_createdIndex].x = (std::uint16_t)-1;
				m_created[m_cacheArray[i]->m_createdIndex].y = (std::uint16_t)-1;

				delete m_cacheArray[i];
				m_cacheArray[i] = NULL;
			}
		}
	m_lock.Unlock();
}


bool WeatherLayerCache::Exists(std::uint16_t x, std::uint16_t y) {
	bool retval;
	m_lock.Lock();
	weak_assert(x < m_xsize);
	weak_assert(y < m_ysize);
	std::uint32_t index = arrayIndex(x, y);
	retval = (m_cacheArray[index]) ? true : false;
	m_lock.Unlock();
	return retval;
}


WeatherCache::~WeatherCache() {
	weak_assert(CacheEntries() == 0);

	std::map<Layer *, WeatherLayerCache *>::iterator it;
	for (it = m_weatherLayerMap[0].begin(); it != m_weatherLayerMap[0].end(); it++) {
		delete it->second;
	}
	for (it = m_weatherLayerMap[1].begin(); it != m_weatherLayerMap[1].end(); it++) {
		delete it->second;
	}
	
}


std::uint32_t WeatherCache::CacheEntries() const {
	return (std::uint32_t)m_weatherLayerMap[0].size();
}


WeatherLayerCache *WeatherCache::cache(Layer *layerThread, std::uint16_t cacheIndex) {
	if (layerThread) {
		std::map<Layer *, WeatherLayerCache *>::iterator it;
		if ((it = m_weatherLayerMap[cacheIndex].find(layerThread)) == m_weatherLayerMap[(std::uint16_t)cacheIndex].end())
			return NULL;
		return it->second;
	}
	return NULL;
}


void WeatherCache::Store(std::uint16_t cacheIndex, const WeatherKey *_key, const WeatherData *_answer, const WTimeManager *tm) {
	if (_key->layerThread) {
		WeatherLayerCache *c = cache(_key->layerThread, cacheIndex);
		weak_assert(c);
		if (c)
			c->Store(_key, _answer, tm);
	}
}


void WeatherCache::Store(std::uint16_t cacheIndex, const WeatherKey *_key, const HIWXData *_answer, const WTimeManager *tm) {
	if (_key->layerThread) {
		WeatherLayerCache *c = cache(_key->layerThread, cacheIndex);
		weak_assert(c);
		if (c)
			c->Store(_key, _answer, tm);
	}
}


void WeatherCache::Store(std::uint16_t cacheIndex, const WeatherKey *_key, const HIFWIData *_answer, const WTimeManager *tm) {
	if (_key->layerThread) {
		WeatherLayerCache *c = cache(_key->layerThread, cacheIndex);
		weak_assert(c);
		if (c)
			c->Store(_key, _answer, tm);
	}
}


void WeatherCache::Store(std::uint16_t cacheIndex, const WeatherKey *_key, const HDFWIData *_answer, const WTimeManager *tm) {
	if (_key->layerThread) {
		WeatherLayerCache *c = cache(_key->layerThread, cacheIndex);
		weak_assert(c);
		if (c)
			c->Store(_key, _answer, tm);
	}
}


WeatherData *WeatherCache::Retrieve(std::uint16_t cacheIndex, const WeatherKey *_key, WeatherData *_to_fill, const WTimeManager *tm) {
	if (_key->layerThread) {
		WeatherLayerCache *c = cache(_key->layerThread, cacheIndex);
		weak_assert(c);
		if (c)
			return c->Retrieve(_key, _to_fill, tm);
	}
	return NULL;
}


HIWXData *WeatherCache::Retrieve(std::uint16_t cacheIndex, const WeatherKey *_key, HIWXData *_to_fill, const WTimeManager *tm) {
	if (_key->layerThread) {
		WeatherLayerCache *c = cache(_key->layerThread, cacheIndex);
		weak_assert(c);
		if (c)
			return c->Retrieve(_key, _to_fill, tm);
	}
	return NULL;
}


HIFWIData *WeatherCache::Retrieve(std::uint16_t cacheIndex, const WeatherKey *_key, HIFWIData *_to_fill, const WTimeManager *tm) {
	if (_key->layerThread) {
		WeatherLayerCache *c = cache(_key->layerThread, cacheIndex);
		weak_assert(c);
		if (c)
			return c->Retrieve(_key, _to_fill, tm);
	}
	return NULL;
}


HDFWIData *WeatherCache::Retrieve(std::uint16_t cacheIndex, const WeatherKey *_key, HDFWIData *_to_fill, const WTimeManager *tm) {
	if (_key->layerThread) {
		WeatherLayerCache *c = cache(_key->layerThread, cacheIndex);
		weak_assert(c);
		if (c)
			return c->Retrieve(_key, _to_fill, tm);
	}
	return NULL;
}


void WeatherCache::Add(Layer *layerThread, std::uint16_t cacheIndex, std::uint16_t x_size, std::uint16_t y_size) {
	if (layerThread) {
		WeatherLayerCache* c1 = nullptr;
		try {
			std::map<Layer *, WeatherLayerCache *>::iterator it;
			if ((it = m_weatherLayerMap[0].find(layerThread)) == m_weatherLayerMap[0].end()) {
				c1 = new WeatherLayerCache(x_size, y_size, (cacheIndex) ? 50 : 7500, m_tm);
				(m_weatherLayerMap[cacheIndex])[layerThread] = c1;
			}
		} catch (std::bad_alloc& cme) {
			if (c1)
				delete c1;
		}
	}
}


void WeatherCache::Remove(Layer *layerThread, std::uint16_t cacheIndex) {
	if (layerThread == (Layer *)-1) {
		std::map<Layer *, WeatherLayerCache*>::iterator it;
		for (it = m_weatherLayerMap[cacheIndex].begin(); it != m_weatherLayerMap[cacheIndex].end(); it++) {
			weak_assert(false);
			delete it->second;
		}
	}
	else if (layerThread) {
		std::map<Layer *, WeatherLayerCache *>::iterator it;
		if ((it = m_weatherLayerMap[cacheIndex].find(layerThread)) == m_weatherLayerMap[cacheIndex].end())
			return;
		if (it->second)
			delete it->second;
		m_weatherLayerMap[cacheIndex].erase(it);
	}
}


void WeatherCache::Clear(Layer *layerThread, std::uint16_t cacheIndex) {
	if (layerThread) {
		WeatherLayerCache *c = cache(layerThread, cacheIndex);
		if (c)
			c->Clear();
	}
}


bool WeatherCache::Exists(Layer *layerThread, std::uint16_t cacheIndex) {
	if (layerThread) {
		WeatherLayerCache *c = cache(layerThread, cacheIndex);
		if (c)
			return true;
	}
	return false;
}


std::uint32_t WeatherCache::Increment(Layer* layerThread, std::uint16_t cacheIndex) {
	std::uint32_t ii;
	if (layerThread) {
		WeatherLayerCache* c = cache(layerThread, cacheIndex);
		if (c)
			ii = c->m_refCount++;
	}
	return ii;
}


std::uint32_t WeatherCache::Decrement(Layer* layerThread, std::uint16_t cacheIndex) {
	std::uint32_t ii = (std::uint32_t)-2;
	if (layerThread) {
		WeatherLayerCache* c = cache(layerThread, cacheIndex);
		if (c)
			ii = --(c->m_refCount);
	}
	return ii;
}


void WeatherCache::PurgeOld(Layer *layerThread, std::uint16_t cacheIndex, const HSS_Time::WTime &time) {
	if (layerThread) {
		WeatherLayerCache *c = cache(layerThread, cacheIndex);
		if (c)
			c->PurgeOld(time);
	}
}


bool WeatherCache::Exists(Layer *layerThread, std::uint16_t cacheIndex, std::uint16_t x, std::uint16_t y) {
	if (layerThread) {
		WeatherLayerCache *c = cache(layerThread, cacheIndex);
		if (c)
			return c->Exists(x, y);
	}
	return false;
}


void WeatherCache::EquilibriumDepth(Layer *layerThread, std::uint16_t cacheIndex, const HSS_Time::WTime &time) {
	if (layerThread) {
		WeatherLayerCache *c = cache(layerThread, cacheIndex);
		if (c) {
			c->m_equilibriumTime = time - WTimeSpan(53 * 24 * 60 * 60);
		}
	}
}


void WeatherCache::SetTimeManager(WTimeManager* tm) {
	std::map<Layer*, WeatherLayerCache*>::iterator it;
	for (it = m_weatherLayerMap[0].begin(); it != m_weatherLayerMap[0].end(); it++) {
		it->second->m_equilibriumTime.SetTimeManager(tm);
	}
	for (it = m_weatherLayerMap[1].begin(); it != m_weatherLayerMap[1].end(); it++) {
		it->second->m_equilibriumTime.SetTimeManager(tm);
	}
}


HSS_Time::WTime WeatherCache::EquilibriumDepth(Layer *layerThread, std::uint16_t cacheIndex) {
	if (layerThread) {
		WeatherLayerCache *c = cache(layerThread, cacheIndex);
		if (c)
			return c->m_equilibriumTime;
	}
	return WTime(0ULL, m_tm);
}
