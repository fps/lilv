/* SLV2
 * Copyright (C) 2007 Dave Robillard <http://drobilla.net>
 *  
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <rasqal.h>
#include <slv2/private_types.h>
#include <slv2/plugin.h>
#include <slv2/types.h>
#include <slv2/query.h>
#include "util.h"


SLV2Plugin*
slv2_plugin_duplicate(const SLV2Plugin* p)
{
	assert(p);
	struct _Plugin* result = malloc(sizeof(struct _Plugin));
	result->plugin_uri = p->plugin_uri;
	result->bundle_url = p->bundle_url;
	result->data_url = p->data_url;
	result->lib_url = p->lib_url;
	return result;
}


const char*
slv2_plugin_get_uri(const SLV2Plugin* p)
{
	assert(p);
	return p->plugin_uri;
}


const char*
slv2_plugin_get_data_url(const SLV2Plugin* p)
{
	assert(p);
	return p->data_url;
}


const char*
slv2_plugin_get_data_path(const SLV2Plugin* p)
{
	assert(p);
	if (!strncmp((char*)p->data_url, "file://", (size_t)7))
		return (p->data_url) + 7;
	else
		return NULL;
}


const char*
slv2_plugin_get_library_url(const SLV2Plugin* p)
{
	assert(p);
	return p->lib_url;
}


const char*
slv2_plugin_get_library_path(const SLV2Plugin* p)
{
	assert(p);
	if (!strncmp((char*)p->lib_url, "file://", (size_t)7))
		return (p->lib_url) + 7;
	else
		return NULL;
}


bool
slv2_plugin_verify(const SLV2Plugin* plugin)
{
	// FIXME: finish this (properly)
	
	size_t num_values = 0;
	
	struct _Value* prop = slv2_plugin_get_value(plugin, "doap:name");
	if (prop) {
		num_values = prop->num_values;
		free(prop);
	}
	if (num_values < 1)
		return false;
/*
	prop = slv2_plugin_get_value(plugin, "doap:license");
	num_values = prop->num_values;
	free(prop);
	if (num_values < 1)
		return false;
*/	
	return true;
}


char*
slv2_plugin_get_name(const SLV2Plugin* plugin)
{
// FIXME: leak
	char*    result = NULL;
	struct _Value* prop   = slv2_plugin_get_value(plugin, "doap:name");
	
	// FIXME: guaranteed to be the untagged one?
	if (prop && prop->num_values >= 1)
		result = prop->values[0];

	return result;
}


SLV2Value
slv2_plugin_get_value(const SLV2Plugin* p,
                      const char*       predicate)
{
	assert(predicate);

	/*
	char* header = slv2_query_header(p);
	char* lang_filter = slv2_query_lang_filter("?value");
	
	char* query_string = strjoin(
		header,
		"SELECT DISTINCT ?value FROM data: WHERE { \n",
		"plugin: ", property, " ?value . \n",
		((lang_filter != NULL) ? lang_filter : ""),
		"}", 0);
	
	free(header);
	free(lang_filter);*/

    char* query = strjoin(
		"SELECT DISTINCT ?value FROM data: WHERE {\n"
		"plugin: ", predicate, " ?value .\n"
		"}\n", NULL);

	SLV2Value result = slv2_plugin_simple_query(p, query, "value");
	
	free(query);

	return result;
}


SLV2Value
slv2_plugin_get_properties(const SLV2Plugin* p)
{
	return slv2_plugin_get_value(p, "lv2:pluginProperty");
}


SLV2Value
slv2_plugin_get_hints(const SLV2Plugin* p)
{
	return slv2_plugin_get_value(p, "lv2:pluginHint");
}


uint32_t
slv2_plugin_get_num_ports(const SLV2Plugin* p)
{
    const char* const query =
		"SELECT DISTINCT ?value FROM data: WHERE {\n"
		"plugin: lv2:port ?value .\n"
		"}\n";

	SLV2Value results = slv2_plugin_simple_query(p, query, "value");
	
	size_t count = results->num_values;
	
	slv2_value_free(results);

	return count;
}


bool
slv2_plugin_has_latency(const SLV2Plugin* p)
{
    const char* const query = 
		"SELECT DISTINCT ?port FROM data: WHERE {\n"
		"	plugin: lv2:port     ?port .\n"
		"	?port   lv2:portHint lv2:reportsLatency .\n"
		"}\n";

	SLV2Value results = slv2_plugin_simple_query(p, query, "port");
	
	bool exists = (results->num_values > 0);
	
	slv2_value_free(results);

	return exists;
}


uint32_t
slv2_plugin_get_latency_port(const SLV2Plugin* p)
{
    const char* const query = 
		"SELECT DISTINCT ?value FROM data: WHERE {\n"
		"	plugin: lv2:port     ?port .\n"
		"	?port   lv2:portHint lv2:reportsLatency ;\n"
		"           lv2:index    ?index .\n"
		"}\n";

	SLV2Value result = slv2_plugin_simple_query(p, query, "index");
	
	// FIXME: need a sane error handling strategy
	assert(result->num_values == 1);
	char* endptr = 0;
	uint32_t index = strtol(result->values[0], &endptr, 10);
	// FIXME: check.. stuff..

	return index;
}


SLV2Value
slv2_plugin_get_supported_features(const SLV2Plugin* p)
{
    const char* const query = 
		"SELECT DISTINCT ?feature FROM data: WHERE {\n"
		"	{ plugin:  lv2:optionalHostFeature  ?feature }\n"
		"		UNION\n"
		"	{ plugin:  lv2:requiredHostFeature  ?feature }\n"
		"}\n";

	SLV2Value result = slv2_plugin_simple_query(p, query, "feature");
	
	return result;
}


SLV2Value
slv2_plugin_get_optional_features(const SLV2Plugin* p)
{
    const char* const query = 
		"SELECT DISTINCT ?feature FROM data: WHERE {\n"
		"	plugin:  lv2:optionalHostFeature  ?feature .\n"
		"}\n";

	SLV2Value result = slv2_plugin_simple_query(p, query, "feature");
	
	return result;
}


SLV2Value
slv2_plugin_get_required_features(const SLV2Plugin* p)
{
    const char* const query = 
		"SELECT DISTINCT ?feature FROM data: WHERE {\n"
		"	plugin:  lv2:requiredHostFeature  ?feature .\n"
		"}\n";

	SLV2Value result = slv2_plugin_simple_query(p, query, "feature");
	
	return result;
}

