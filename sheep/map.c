/*
 * sheep/map.c
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#include <sheep/util.h>
#include <string.h>

#include <sheep/map.h>

struct sheep_map_entry {
	const char *name;
	void *value;
	struct sheep_map_entry *next;
};

static int hash(const char *name)
{
	int key = 0;

	while (*name)
		key += (key << 5) + *name++;
	return key < 0 ? -key : key;
}

static struct sheep_map_entry **find(struct sheep_map *map,
				     const char *name,
				     int *create)
{
	struct sheep_map_entry **pentry, *entry;
	int index;

	index = hash(name) % SHEEP_MAP_SIZE;
	pentry = &map->entries[index];
	while (*pentry) {
		if (!strcmp((*pentry)->name, name))
			return pentry;
		pentry = &(*pentry)->next;
	}
	if (!create)
		return NULL;
	entry = sheep_malloc(sizeof(*entry));
	entry->name = sheep_strdup(name);
	entry->next = map->entries[index];
	map->entries[index] = entry;
	*create = 1;
	return &map->entries[index];
}

int sheep_map_set(struct sheep_map *map, const char *name, void *value)
{
	struct sheep_map_entry **pentry;
	int create = 0;

	pentry = find(map, name, &create);
	(*pentry)->value = value;
	return !create;
}

int sheep_map_get(struct sheep_map *map, const char *name, void **valuep)
{
	struct sheep_map_entry **pentry;

	pentry = find(map, name, NULL);
	if (!pentry)
		return -1;
	*valuep = (*pentry)->value;
	return 0;
}

int sheep_map_del(struct sheep_map *map, const char *name)
{
	struct sheep_map_entry **pentry, *entry;

	pentry = find(map, name, NULL);
	if (!pentry)
		return -1;
	entry = *pentry;
	*pentry = entry->next;
	sheep_free(entry->name);
	sheep_free(entry);
	return 0;
}

void sheep_map_drain(struct sheep_map *map)
{
	struct sheep_map_entry *entry, *next;
	unsigned int i;

	for (i = 0; i < SHEEP_MAP_SIZE; i++)
		for (entry = map->entries[i]; entry; entry = next) {
			next = entry->next;
			sheep_free(entry->name);
			sheep_free(entry);
		}
}
