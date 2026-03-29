#pragma once

#ifdef SAVVY_ENTLIGHTS

#include "vkpt.h"

#define QRAD3_DEFAULT_INTENSITY 300.0f
#define BSP_MAX_ENTITY_PROPERTIES 50
#define COM_PARSE_OLD_MAX_TOKEN_CHARS 128

void entlights_init_cvars(void);
void entlights_preload_entlight_data(const bsp_t* bsp);
void add_entlights(refdef_t* fd, QVKUniformBuffer_t* ubo);

#endif
