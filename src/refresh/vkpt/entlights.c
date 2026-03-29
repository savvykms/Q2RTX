#ifdef SAVVY_ENTLIGHTS

#include "entlights.h"

cvar_t    *cvar_show_entlights = NULL;
cvar_t    *cvar_entlights_enabled = NULL;
cvar_t    *cvar_entlights_scale = NULL;
static     dlight_t entlights[MAX_ENTLIGHTS];
static int num_entlights = 0;

typedef struct EntityKVPair_s {
	//	char* key;
	//	char* value;
	char key[256];
	char value[256];
} EntityKVPair_t;

void entlights_init_cvars(void)
{
	cvar_show_entlights = Cvar_Get("cl_show_entlights", "0", 0); //Show/hide entity light positions using particle effect
	cvar_entlights_scale = Cvar_Get("cl_entlights_scale", "0.05", 0); //Overall modifier; ideally tunable per-map to match original lighting
	cvar_entlights_enabled = Cvar_Get("cl_entlights_enabled", "1", 0); //enable feature?
}

void entlights_preload_entlight_data(const bsp_t* bsp)
{
	num_entlights = 0;
	memset(entlights, 0, sizeof(entlights));

	if (cvar_entlights_enabled != NULL && cvar_entlights_enabled->integer != 0) {
		bsp_add_entlights(bsp);
	}
}

static char com_token_internal[COM_PARSE_OLD_MAX_TOKEN_CHARS];
static char* COM_Parse_old(char** data_p)
{
	int   c;
	int   len;
	char* data;

	data = *data_p;
	len = 0;
	com_token_internal[0] = 0;

	if (!data)
	{
		*data_p = NULL;
		return "";
	}

	// skip whitespace
skipwhite:
	while ((c = *data) <= ' ')
	{
		if (c == 0)
		{
			*data_p = NULL;
			return "";
		}
		data++;
	}

	// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}


	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c == '\"' || !c)
			{
				com_token_internal[len] = 0;
				*data_p = data;
				return com_token_internal;
			}
			if (len < COM_PARSE_OLD_MAX_TOKEN_CHARS)
			{
				com_token_internal[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if (len < COM_PARSE_OLD_MAX_TOKEN_CHARS)
		{
			com_token_internal[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c > 32);

	if (len == COM_PARSE_OLD_MAX_TOKEN_CHARS)
	{
		//		Com_Printf ("Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
		len = 0;
	}
	com_token_internal[len] = 0;

	*data_p = data;
	return com_token_internal;
}

static void bsp_add_entlights(const bsp_t* bsp)
{
	char* entString;
	char* com_token;
	int el;
	float entity_scale;

	if (bsp == NULL)
		return;

	//replicate algorithm from qrad3; would be best to be able to set entity_scale dynamically depending upon map
	//so that original qrad parameters could be replicated here
	entity_scale = atof(cvar_entlights_scale->string);
	if (entity_scale == 0.00f) {
		entity_scale = 0.05f;
	}
	entString = bsp->entitystring;

	//replicate SpawnEntities logic of game DLL, with regards to entity parsing

	/*Example:
	{
	"style" "1"
	"_color" "0.97 0.54 0.01"
	"light" "300"
	"origin" "-1632 320 256"
	"classname" "light"
	}
	*/

	//initialize all light values, even if not used
	num_entlights = 0;
	for (el = 0; el < MAX_ENTLIGHTS; el++)
	{
		dlight_t* elight;
		//elight = (dlight_t*)(entlights + el);
		elight = &entlights[el]; // (dlight_t*)(entlights + (el * sizeof(dlight_t)));
		//elight = (dlight_t*)(entlights + (el * sizeof(dlight_t)));
		elight->origin[0] = 0.00f;
		elight->origin[1] = 0.00f;
		elight->origin[2] = 0.00f;
		elight->transformed[0] = 0.00f;
		elight->transformed[1] = 0.00f;
		elight->transformed[2] = 0.00f;
		elight->intensity = QRAD3_DEFAULT_INTENSITY; //original qrad3 default
		elight->radius = elight->intensity * entity_scale;
		elight->color[0] = 1.00f;
		elight->color[1] = 1.00f;
		elight->color[2] = 1.00f;
	}

	for (;;) //loop over entities
	{
		EntityKVPair_t keypairs[BSP_MAX_ENTITY_PROPERTIES];
		int num_keypairs, i;
		qboolean islight;
		char* com_token_inner;
		//char* inner;

		num_keypairs = 0;
		islight = qfalse;

		com_token = COM_Parse_old(&entString);

		if (!entString)
		{
			break;
		}
		if (com_token[0] != '{')
		{
			Com_LPrintf(PRINT_WARNING, "bsp_add_entlights: found %s when expecting {", com_token);
		}

		//inner = entString;
		num_keypairs = 0;

		//replicate ED_ParseEdict() logic of game DLL
		for (;;) //loop over properties
		{
			EntityKVPair_t keypair;

			keypair.key[0] = '\0';
			keypair.value[0] = '\0';
			keypair.key[(sizeof(keypair.key) - 1)] = '\0';
			keypair.value[(sizeof(keypair.value) - 1)] = '\0';

			if (num_keypairs >= BSP_MAX_ENTITY_PROPERTIES)
			{
				break;
			}

			//get key

			com_token_inner = COM_Parse_old(&entString);

			if (com_token_inner[0] == '}')
			{
				break;
			}
			if (!entString)
			{
				Com_LPrintf(PRINT_WARNING, "bsp_add_entlights: EOF without closing brace");
			}

			strncpy(keypair.key, com_token_inner, sizeof(keypair.key) - 1);

			//get value

			com_token_inner = COM_Parse_old(&entString);

			if (!entString)
			{
				Com_LPrintf(PRINT_WARNING, "bsp_add_entlights: EOF without closing brace");
			}

			if (com_token_inner[0] == '}')
			{
				Com_LPrintf(PRINT_WARNING, "bsp_add_entlights: closing brace without data (2)");
			}

			strncpy(keypair.value, com_token_inner, sizeof(keypair.value) - 1);

			keypairs[num_keypairs++] = keypair;
		}

		for (i = 0; i < num_keypairs; i++)
			if (!Q_stricmp(keypairs[i].key, "classname") && !Q_stricmp(keypairs[i].value, "light"))
			{
				islight = qtrue;
			}

		//only process light entities
		if (islight && num_entlights < MAX_ENTLIGHTS)
		{
			dlight_t* elight;
			qboolean parse_error;

			parse_error = qfalse;

			//examples:
			//"_color" "0.97 0.54 0.01"
			//"light" "300" - F_INT
			//"origin" "-1632 320 256" - F_VECTOR

			//elight = (dlight_t*)(entlights + i);
			elight = &entlights[num_entlights]; // (dlight_t*)(entlights + (el * sizeof(dlight_t)));
			//elight = (dlight_t*)(entlights + (num_entlights * sizeof(dlight_t)));
			elight->intensity = QRAD3_DEFAULT_INTENSITY; //original qrad3 default
			elight->radius = elight->intensity * entity_scale;
			elight->color[0] = 1.00f;
			elight->color[1] = 1.00f;
			elight->color[2] = 1.00f;

			for (i = 0; i < num_keypairs; i++)
			{
				if (!Q_stricmp(keypairs[i].key, "origin") || !Q_stricmp(keypairs[i].key, "_color"))
				{
					vec3_t vec;
					//vec3_t* v;

					//v = NULL;
					vec[0] = 0.0f;
					vec[1] = 0.0f;
					vec[2] = 0.0f;

					if (sscanf(keypairs[i].value, "%f %f %f", &vec[0], &vec[1], &vec[2]) != 3) {
						//could not parse
						parse_error = qtrue;
						break;
					}
					if (!Q_stricmp(keypairs[i].key, "origin"))
					{
						VectorCopy(vec, elight->origin);
						//v = &elight->origin;
					}
					else if (!Q_stricmp(keypairs[i].key, "_color"))
					{
						VectorCopy(vec, elight->color);
						//v = &elight->color;
					}
					else
					{
						//should be unreachable
						parse_error = qtrue;
						break;
					}
					//*v[0] = vec[0];
					//*v[1] = vec[1];
					//*v[2] = vec[2];
				}
				else if (!Q_stricmp(keypairs[i].key, "light") || !Q_stricmp(keypairs[i].key, "_light"))
				{
					float f;
					f = atof(keypairs[i].value);
					if (f == 0.0f)
					{
						parse_error = qtrue;
						break;
					}

					elight->intensity = f;
					elight->radius = elight->intensity * entity_scale;
				}
				else if (!Q_stricmp(keypairs[i].key, "light_spot") || !Q_stricmp(keypairs[i].key, "target"))
				{
					//TODO: add in support for spot lights
					//ignore spot lights for now
					//additional spot light properties are "_cone" and "angle"; these should be interpretted like qrad3
					parse_error = qtrue;
				}
			}

			if (parse_error == qfalse && elight->radius != 0.00f)
			{
				//if not correct, redo this light definition with the next we find
				//e.g. effectively ignore bad lights
				num_entlights++;
			}
		}
	}
}



//static void add_elights(dlight_t* lights, int num_lights, QVKUniformBuffer_t* ubo)
void add_entlights(refdef_t* fd, QVKUniformBuffer_t* ubo)
{
	//for (int i = 0; i < num_lights && ubo->num_dyn_lights < MAX_LIGHT_SOURCES; i++)
	for (int i = 0; i < num_entlights && ubo->num_dyn_lights < MAX_LIGHT_SOURCES; i++)
	{
		//dlight_t *elight = lights + i;
		dlight_t* elight = &entlights[i];
		//dlight_t* elight = (dlight_t*)(entlights + (i * sizeof(dlight_t)));

		float* dynlight_data = (float*)(ubo->sphere_light_data + ubo->num_dyn_lights * 2);
		float* center = dynlight_data;
		float* radius = dynlight_data + 3;
		float* color = dynlight_data + 4;
		dynlight_data[7] = 0.f;

		VectorCopy(elight->origin, center);
		VectorScale(elight->color, elight->intensity / QRAD3_DEFAULT_INTENSITY, color);
		//VectorScale(elight->color, elight->intensity / 25.f, color);

		*radius = elight->radius;

		ubo->num_dyn_lights++;

		if (cvar_show_entlights->integer && fd->num_particles < MAX_PARTICLES)
		{
			particle_t* part = &fd->particles[fd->num_particles]; //fd->particles + (fd->num_particles * sizeof(particle_t));

			VectorCopy(elight->origin, part->origin);
			part->radius = elight->radius;
			part->brightness = max(elight->color[0], max(elight->color[1], elight->color[2]));
			part->color = -1;
			part->rgba.u8[0] = (uint8_t)max(0.00f, min(255.00f, elight->color[0] / part->brightness * 255.00f));
			part->rgba.u8[1] = (uint8_t)max(0.00f, min(255.00f, elight->color[1] / part->brightness * 255.00f));
			part->rgba.u8[2] = (uint8_t)max(0.00f, min(255.00f, elight->color[2] / part->brightness * 255.00f));
			part->rgba.u8[3] = 255;
			part->alpha = 1.00f;

			fd->num_particles++;
		}
	}
}

#endif