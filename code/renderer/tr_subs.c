/*
===========================================================================
Copyright (C) 2010 James Canete (use.less01@gmail.com)

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_subs.c - common function replacements for modular renderer

#include "tr_local.h"

void QDECL Com_Printf( const char *msg, ... )
{
	va_list         argptr;
	char            text[1024];

	va_start(argptr, msg);
	Q_vsnprintf(text, sizeof(text), msg, argptr);
	va_end(argptr);

	ri.Printf(PRINT_ALL, "%s", text);
}

void QDECL Com_Error( int level, const char *error, ... )
{
	va_list         argptr;
	char            text[1024];

	va_start(argptr, error);
	Q_vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	ri.Error(level, "%s", text);
}

/**
 * @brief Dumps OpenGL state for debugging - typically every capability set with glEnable().
 */
void R_DumpOpenGlState (void)
{
#define CAPABILITY( X ) {GL_ ## X, # X}
	/* List taken from here: http://www.khronos.org/opengles/sdk/1.1/docs/man/glIsEnabled.xml */
	const struct { GLenum idx; const char * text; } openGLCaps[] = {
		CAPABILITY(ALPHA_TEST),
		CAPABILITY(BLEND),
		CAPABILITY(COLOR_ARRAY),
		CAPABILITY(COLOR_LOGIC_OP),
		CAPABILITY(COLOR_MATERIAL),
		CAPABILITY(CULL_FACE),
		CAPABILITY(DEPTH_TEST),
		CAPABILITY(DITHER),
		CAPABILITY(FOG),
		CAPABILITY(LIGHTING),
		CAPABILITY(LINE_SMOOTH),
		CAPABILITY(MULTISAMPLE),
		CAPABILITY(NORMAL_ARRAY),
		CAPABILITY(NORMALIZE),
		CAPABILITY(POINT_SMOOTH),
		CAPABILITY(POLYGON_OFFSET_FILL),
		CAPABILITY(RESCALE_NORMAL),
		CAPABILITY(SAMPLE_ALPHA_TO_COVERAGE),
		CAPABILITY(SAMPLE_ALPHA_TO_ONE),
		CAPABILITY(SAMPLE_COVERAGE),
		CAPABILITY(SCISSOR_TEST),
		CAPABILITY(STENCIL_TEST),
		CAPABILITY(VERTEX_ARRAY)
	};
#undef CAPABILITY

	char s[1024] = "";
	GLint i;
	GLint maxTexUnits = 0;
	GLint activeTexUnit = 0;
	GLint activeClientTexUnit = 0;
	GLint activeTexId = 0;
	GLfloat texEnvMode = 0;
	const char * texEnvModeStr = "UNKNOWN";
	GLfloat color[4];

	for (i = 0; i < sizeof(openGLCaps)/sizeof(openGLCaps[0]); i++) {
		if (glIsEnabled(openGLCaps[i].idx)) {
			Q_strcat(s, sizeof(s), openGLCaps[i].text);
			Q_strcat(s, sizeof(s), " ");
		}
	}
	glGetFloatv(GL_CURRENT_COLOR, color);

	Com_Printf("OpenGL enabled caps: %s color %f %f %f %f \n", s, color[0], color[1], color[2], color[3]);

	glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexUnit);
	glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &activeClientTexUnit);

	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &maxTexUnits);
	for (i = GL_TEXTURE0; i < GL_TEXTURE0 + maxTexUnits; i++) {
		glActiveTexture(i);
		glClientActiveTexture(i);

		strcpy(s, "");
		if (glIsEnabled (GL_TEXTURE_2D))
			strcat(s, "enabled, ");
		if (glIsEnabled (GL_TEXTURE_COORD_ARRAY))
			strcat(s, "with texcoord array, ");
		if (i == activeTexUnit)
			strcat(s, "active, ");
		if (i == activeClientTexUnit)
			strcat(s, "client active, ");

		glGetIntegerv(GL_TEXTURE_BINDING_2D, &activeTexId);
		glGetTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &texEnvMode);
		if (fabs(texEnvMode - GL_ADD) < 0.1f)
			texEnvModeStr = "ADD";
		if (fabs(texEnvMode - GL_MODULATE) < 0.1f)
			texEnvModeStr = "MODULATE";
		if (fabs(texEnvMode - GL_DECAL) < 0.1f)
			texEnvModeStr = "DECAL";
		if (fabs(texEnvMode - GL_BLEND) < 0.1f)
			texEnvModeStr = "BLEND";
		if (fabs(texEnvMode - GL_REPLACE) < 0.1f)
			texEnvModeStr = "REPLACE";
		if (fabs(texEnvMode - GL_COMBINE) < 0.1f)
			texEnvModeStr = "COMBINE";

		Com_Printf("Texunit: %d texID %d %s texEnv mode %s\n", i - GL_TEXTURE0, activeTexId, s, texEnvModeStr);
	}

	glActiveTexture(activeTexUnit);
	glClientActiveTexture(activeClientTexUnit);
}
