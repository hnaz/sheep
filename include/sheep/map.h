#ifndef _SHEEP_MAP_H
#define _SHEEP_MAP_H

#define SHEEP_MAP_SIZE		64

struct sheep_map_entry;
struct sheep_map {
	struct sheep_map_entry *entries[SHEEP_MAP_SIZE];
};

void sheep_map_set(struct sheep_map *, const char *, void *);
int sheep_map_get(struct sheep_map *, const char *, void **);
int sheep_map_del(struct sheep_map *, const char *);

#endif /* _SHEEP_MAP_H */
