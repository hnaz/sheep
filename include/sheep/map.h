#ifndef _SHEEP_MAP_H
#define _SHEEP_MAP_H

#define SHEEP_MAP_SIZE		128

struct sheep_map_entry {
	const char *name;
	unsigned int value;
	struct sheep_map_entry *next;
};

struct sheep_map {
	struct sheep_map_entry *entries[SHEEP_MAP_SIZE];
};

void sheep_map_set(struct sheep_map *, const char *, unsigned int);
int sheep_map_get(struct sheep_map *, const char *);
int sheep_map_del(struct sheep_map *, const char *);

#endif /* _SHEEP_MAP_H */
