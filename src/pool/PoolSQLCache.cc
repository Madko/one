/* -------------------------------------------------------------------------- */
/* Copyright 2002-2018, OpenNebula Project, OpenNebula Systems                */
/*                                                                            */
/* Licensed under the Apache License, Version 2.0 (the "License"); you may    */
/* not use this file except in compliance with the License. You may obtain    */
/* a copy of the License at                                                   */
/*                                                                            */
/* http://www.apache.org/licenses/LICENSE-2.0                                 */
/*                                                                            */
/* Unless required by applicable law or agreed to in writing, software        */
/* distributed under the License is distributed on an "AS IS" BASIS,          */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   */
/* See the License for the specific language governing permissions and        */
/* limitations under the License.                                             */
/* -------------------------------------------------------------------------- */

#include "PoolSQLCache.h"
#include "Nebula.h"

PoolSQLCache::PoolSQLCache()
{
    max_elements = 0;

    pthread_mutex_init(&mutex, 0);
};

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void PoolSQLCache::lock_line(int oid)
{
    std::map<int, CacheLine *>::iterator it;

    CacheLine * cl;

    lock();

    it = cache.find(oid);

    if ( it == cache.end() )
    {
        cl = new CacheLine(0);

        cl->in_use = true;

        cache.insert(make_pair(oid, cl));

        unlock();

        cl->lock();

        return;
    }

    cl = it->second;

    cl->in_use = true;

    unlock();

    cl->lock();

    if ( cl->object != 0 )
    {
        cl->object->lock();

        delete cl->object;
    }

    cl->object = 0;

    return;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int PoolSQLCache::set_line(int oid, PoolObjectSQL * object)
{
    std::map<int, CacheLine *>::iterator it;

    lock();

    it = cache.find(oid);

    if ( it == cache.end() )
    {
        unlock();

        return -1;
    }

    it->second->in_use = false;

    it->second->object = object;

    it->second->unlock();

    unlock();

    return 0;
};

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

void PoolSQLCache::flush_cache_lines()
{
    for (std::map<int,CacheLine *>::iterator it=cache.begin(); it!=cache.end();)
    {
        CacheLine * cl = it->second;

        int rc = cl->trylock();

        if ( rc == EBUSY ) // cache line locked
        {
            ++it;
            continue;
        }
        else
        {
            if ( cl->in_use ) // cache line being set
            {
                cl->unlock();

                ++it;
                continue;
            }
            else if ( cl->object != 0 )
            {
                rc =  cl->object->trylock();

                if ( rc == EBUSY ) //object int use
                {
                    ++it;
                    continue;
                }
            }
        }

        delete it->second; // cache line & pool object locked

        it = cache.erase(it);
    }
};

