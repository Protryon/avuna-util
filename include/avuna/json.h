/*
 * json.h
 *
 *  Created on: May 20, 2016
 *      Author: root
 */

#ifndef JSON_H_
#define JSON_H_

#include <stdint.h>
#include <stdlib.h>
#include <avuna/pmem.h>
#include <avuna/hash.h>

#define JSON_STRING 0
#define JSON_NUMBER 1
#define JSON_OBJECT 2
#define JSON_ARRAY 3
#define JSON_TRUE 4
#define JSON_FALSE 5
#define JSON_NULL 6

union json_data {
	char* string;
	double number;
};

struct json_object {
	struct mempool* pool;
	char* name;
	uint8_t type;
	union json_data data;
	struct hashmap* children;
	struct llist* children_list;
};

struct json_object* json_make_object(struct mempool* pool, char* name, uint8_t type);

ssize_t json_parse(struct mempool* parent, struct json_object** root, char* json);

struct json_object* json_get(struct json_object* parent, char* name);

#endif /* JSON_H_ */
