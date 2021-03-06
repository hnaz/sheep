/*
 * include/sheep/map.h
 *
 * Copyright (c) 2009 Johannes Weiner <hannes@cmpxchg.org>
 */
#ifndef _SHEEP_MAP_H
#define _SHEEP_MAP_H

#define SHEEP_MAP_SIZE		64

struct sheep_map_entry;
struct sheep_map {
	struct sheep_map_entry *entries[SHEEP_MAP_SIZE];
};

#define SHEEP_DEFINE_MAP(name)			\
	struct sheep_map name = { { 0 } }

int sheep_map_set(struct sheep_map *, const char *, void *);
int sheep_map_get(struct sheep_map *, const char *, void **);
int sheep_map_del(struct sheep_map *, const char *);

void sheep_map_drain(struct sheep_map *);

#endif /* _SHEEP_MAP_H */
