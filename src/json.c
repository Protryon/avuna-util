/*
 * json.c
 *
 *  Created on: May 20, 2016
 *      Author: root
 */


#include <avuna/json.h>
#include <avuna/string.h>
#include <avuna/llist.h>
#include <avuna/hash.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

char* __json_recur_string(struct mempool* pool, char** json) {
	int state = 0;
	size_t ret_index = 0;
	size_t ret_cap = 32;
	char* ret = pmalloc(pool, ret_cap);
#define RET_CHECK() if (ret_index + 4 >= ret_cap) { ret_cap *= 2; ret = prealloc(pool, ret, ret_cap); }
	char c;
	while ((c = ((*json)++)[0]) != 0) {
		if (state == 0 && c == '\"' ) {
			state = 1;
		} else if (state == 1) {
			if (c == '\"') {
				errno = 0;
				RET_CHECK();
				ret[ret_index] = 0;
				return ret;
			} else if (c == '\\') {
				c = ((*json)++)[0];
				if (c == 0) break;
				RET_CHECK();
				if (c == '\"') ret[ret_index++] = '\"';
				else if (c == '\\') ret[ret_index++] = '\\';
				else if (c == '/') ret[ret_index++] = '/';
				else if (c == 'b') ret[ret_index++] = '\b';
				else if (c == 'f') ret[ret_index++] = '\f';
				else if (c == 'n') ret[ret_index++] = '\n';
				else if (c == 'r') ret[ret_index++] = '\r';
				else if (c == 't') ret[ret_index++] = '\t';
				else if (c == 'u') {
					++(*json); // we passthrough unicode escapes because we don't deal with them yet
					goto uce;
				}
				ret[ret_index] = 0;
				continue;
				uce: ;
			} else {
				RET_CHECK();
				ret[ret_index++] = c;
				ret[ret_index] = 0;
			}
		}
	}
#undef RET_CHECK
	// string ended unexpectedly
	errno = EINVAL;
	return NULL;
}

struct json_object* __json_recur_object(struct mempool* pool, char* name, char** json);

struct json_object* __json_recur_array(struct mempool* pool, char* name, char** json);

struct json_object* json_make_object(struct mempool* pool, char* name, uint8_t type) {
	struct json_object* node = pcalloc(pool, sizeof(struct json_object));
	node->pool = pool;
	node->name = name;
	node->type = type;
	if (type == JSON_OBJECT || type == JSON_ARRAY) {
		node->children_list = llist_new(pool);
	}
	if (type == JSON_OBJECT) {
		node->children = hashmap_new(8, pool);
	}
	return node;
}

struct json_object* __json_recur_value(struct mempool* pool, char* name, char** json) {
	char c;
	while ((c = ((*json)++)[0]) != 0) {
		if (c == '\"') {
			struct json_object* value = json_make_object(pool, name, JSON_STRING);
			value->data.string = __json_recur_string(pool, json);
			return value;
		} else if (c == '-' || (c >= '0' && c <= '9')) {
			struct json_object* value = json_make_object(pool, name, JSON_NUMBER);
			value->data.number = strtod(*json, json);
			return value;
		} else if (c == '{') {
			--(*json);
			return __json_recur_object(pool, name, json);
		} else if (c == '[') {
			--(*json);
			return __json_recur_array(pool, name, json);
		} else if (c == 't' && str_prefixes_case(*json, "rue")) {
			(*json) += 3;
			return json_make_object(pool, name, JSON_TRUE);
		} else if (c == 'f' && str_prefixes_case(*json, "alse")) {
			(*json) += 4;
			return json_make_object(pool, name, JSON_FALSE);
		} else if (c == 'n' && str_prefixes_case(*json, "ull")) {
			(*json) += 3;
			return json_make_object(pool, name, JSON_NULL);
		} else break;
	}
	errno = 0;
	--(*json);
	return 0;
}

struct json_object* __json_recur_array(struct mempool* pool, char* name, char** json) {
	struct json_object* array = json_make_object(pool, name, JSON_ARRAY);
	int state = 0; // FSM parser (0 = expecting start, 1 = expecting value, 2 = , to restart or terminator)
	char c;
	while ((c = ((*json)++)[0]) != 0) {
		if (state == 0 && c == '[') {
			state = 1;
		} else if (state == 1) {
			if (c == ']') {
				if (array->children->entry_count > 0) {
					errno = EINVAL;
					return NULL;
				}
				break;
			}
			struct json_object* new_child = __json_recur_value(pool, NULL, json);
			if (new_child == NULL) {
				return NULL;
			}
			llist_append(array->children_list, new_child);
			state = 2;
		} else if (state == 2) {
			if (c == ',') {
				state = 1;
			} else if (c == ']')  {
				break;
			} else {
				errno = EINVAL;
				return NULL;
			}
		} else {
			errno = EINVAL;
			return NULL;
		}
	}
	return array;
}

struct json_object* __json_recur_object(struct mempool* pool, char* name, char** json) {
	struct json_object* obj = json_make_object(pool, name, JSON_OBJECT);
	int state = 0; // FSM parser (0 = expecting start, 1 = terminator or key start, 2 = expecting colon, 3 = expecting value, 4 = , to restart or terminator)
	char* new_name = NULL;
	char c;
	while ((c = ((*json)++)[0]) != 0) {
		if (state == 0 && c == '{') {
			state = 1;
		} else if (state == 1) {
			if (c == '}') {
				if (obj->children->entry_count > 0) {
					errno = EINVAL;
					return NULL;
				}
				break;
			} else if (c == '\"') {
				new_name = __json_recur_string(pool, json);
				if (new_name == NULL) {
					errno = EINVAL;
					return NULL;
				}
				state = 2;
			}
		} else if (state == 2 && c == ':') {
			state = 3;
		} else if (state == 3) {
			struct json_object* new_child = __json_recur_value(pool, new_name, json);
			if (new_child == NULL) {
				return NULL;
			}
			hashmap_put(obj->children, new_child->name, new_child);
			llist_append(obj->children_list, new_child);
			state = 4;
		} else if (state == 4) {
			if (c == ',') {
				state = 1;
			} else if (c == '}')  {
				break;
			} else {
				errno = EINVAL;
				return NULL;
			}
		} else {
			errno = EINVAL;
			return NULL;
		}
	}
	return obj;
}

ssize_t json_parse(struct mempool* parent, struct json_object** root, char* json) {
	if (strlen(json) < 2 || (json[0] != '{' && json[0] != '[')) {
		memset(root, 0, sizeof(struct json_object));
		errno = EINVAL;
		return -1;
	}
	struct mempool* pool = mempool_new();
	pchild(parent, pool);
	char* original_json = json;
	*root = __json_recur_object(pool, NULL, &json);
	return json - original_json;
}

struct json_object* json_get(struct json_object* parent, char* name) {
	if (parent->children == NULL) return NULL;
	return hashmap_get(parent->children, name);
}
