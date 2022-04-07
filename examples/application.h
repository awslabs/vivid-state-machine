#ifndef APPLICATION_H
#define APPLICATION_H

#include <vivid/binding.h>

#ifdef __cplusplus
extern "C" {
#endif

void *application_create(vivid_binding_t *binding);
void application_destroy(void *application);
void application_trigger(void *application, const char *value, size_t len);

#ifdef __cplusplus
}
#endif

#endif
