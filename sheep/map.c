#include <sheep/util.h>
#include <string.h>

#include <sheep/map.h>

static int hash(const char *name)
{
	size_t len = strlen(name);
	int key = 0;

	while (len--)
		key += (key << 5) + *name++;
	return key < 0 ? -key : key;
}

enum access {
	MAP_READ,
	MAP_CREATE,
	MAP_DELETE,
};

static struct sheep_map_entry **
find(struct sheep_map *map, const char *name, int create)
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
	/* XXX: drop this when reader returns protected constants */
	entry->name = sheep_strdup(name);
	entry->next = map->entries[index];
	map->entries[index] = entry;
	return &map->entries[index];
}

void sheep_map_set(struct sheep_map *map, const char *name, void *value)
{
	struct sheep_map_entry **pentry;

	pentry = find(map, name, 1);
	(*pentry)->value = value;
}

int sheep_map_get(struct sheep_map *map, const char *name, void **valuep)
{
	struct sheep_map_entry **pentry;

	pentry = find(map, name, 0);
	if (!pentry)
		return -1;
	*valuep = (*pentry)->value;
	return 0;
}

int sheep_map_del(struct sheep_map *map, const char *name)
{
	struct sheep_map_entry **pentry, *entry;

	pentry = find(map, name, 0);
	if (!pentry)
		return -1;
	entry = *pentry;
	*pentry = entry->next;
	sheep_free(entry);
	return 0;
}
