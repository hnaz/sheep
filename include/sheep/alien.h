#ifndef _SHEEP_ALIEN_H
#define _SHEEP_ALIEN_H

#include <sheep/object.h>
#include <sheep/vm.h>

typedef sheep_t (*sheep_alien_t)(struct sheep_vm *, unsigned int);

extern const struct sheep_type sheep_alien_type;

sheep_t sheep_alien(struct sheep_vm *, sheep_alien_t);

#endif /* _SHEEP_ALIEN_H */
