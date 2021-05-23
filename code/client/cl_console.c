/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

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
// console.c

#include "client.h"
#ifdef __ANDROID__
#include <SDL_screenkeyboard.h>
#endif


int g_console_field_width = 39; //78;
int g_console_text_input_toggled_ui = 0;
extern char g_console_text_input_buffer[MAX_STRING_CHARS - 16] = "";

#define	NUM_CON_TIMES 5

#define		CON_TEXTSIZE	512 //32768 // No need for big scrollbuffer on Android, because we cannot scroll back

typedef struct {
	qboolean	initialized;

	short	text[CON_TEXTSIZE];
	int		current;		// line where next message will be printed
	int		x;				// offset in current line for next print
	int		display;		// bottom of console displays this line

	int 	linewidth;		// characters across screen
	int		totallines;		// total lines in console scrollback

	float	xadjust;		// for wide aspect screens

	float	displayFrac;	// aproaches finalFrac at scr_conspeed
	float	finalFrac;		// 0.0 to 1.0 lines of console to display
	float	userFrac;		// 0.0 to 1.0 - for user Configurations. Don't want to mess with finalFrac - marky
	int		vislines;		// in scanlines

	int		times[NUM_CON_TIMES];	// cls.realtime time the line was generated
								// for transparent notify lines
	vec4_t	color;
} console_t;

extern	console_t	con;

console_t	con;

cvar_t		*con_conspeed;
cvar_t		*con_notifytime;

#define	DEFAULT_CONSOLE_WIDTH	39 //78


/*
================
Con_ToggleConsole_f
================
*/
void Con_ToggleConsole_f (void) {
	// Can't toggle the console when it's the only thing available
	if ( clc.state == CA_DISCONNECTED && Key_GetCatcher( ) == KEYCATCH_CONSOLE ) {
		return;
	}

	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;

	Con_ClearNotify ();
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_CONSOLE );
}

/*
================
Con_MessageMode_f
================
*/
void Con_MessageMode_f (void) {
	chat_playerNum = -1;
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;

	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );

#ifdef __ANDROID__
	if ( !SDL_IsScreenKeyboardShown ( NULL ) ) {
		Q_strncpyz( g_console_text_input_buffer, "", sizeof(g_console_text_input_buffer) );
		SDL_ANDROID_GetScreenKeyboardTextInputAsync( g_console_text_input_buffer, sizeof(g_console_text_input_buffer) );
	}
#endif
}

/*
================
Con_MessageMode2_f
================
*/
void Con_MessageMode2_f (void) {
	chat_playerNum = -1;
	chat_team = qtrue;
	Field_Clear( &chatField );
	chatField.widthInChars = 25;
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );

#ifdef __ANDROID__
	if ( !SDL_IsScreenKeyboardShown ( NULL ) ) {
		Q_strncpyz( g_console_text_input_buffer, "", sizeof(g_console_text_input_buffer) );
		SDL_ANDROID_GetScreenKeyboardTextInputAsync( g_console_text_input_buffer, sizeof(g_console_text_input_buffer) );
	}
#endif
}

/*
================
Con_MessageMode3_f
================
*/
void Con_MessageMode3_f (void) {
	chat_playerNum = VM_Call( cgvm, CG_CROSSHAIR_PLAYER );
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );

#ifdef __ANDROID__
	if ( !SDL_IsScreenKeyboardShown ( NULL ) ) {
		Q_strncpyz( g_console_text_input_buffer, "", sizeof(g_console_text_input_buffer) );
		SDL_ANDROID_GetScreenKeyboardTextInputAsync( g_console_text_input_buffer, sizeof(g_console_text_input_buffer) );
	}
#endif
}

/*
================
Con_MessageMode4_f
================
*/
void Con_MessageMode4_f (void) {
	chat_playerNum = VM_Call( cgvm, CG_LAST_ATTACKER );
	if ( chat_playerNum < 0 || chat_playerNum >= MAX_CLIENTS ) {
		chat_playerNum = -1;
		return;
	}
	chat_team = qfalse;
	Field_Clear( &chatField );
	chatField.widthInChars = 30;
	Key_SetCatcher( Key_GetCatcher( ) ^ KEYCATCH_MESSAGE );

#ifdef __ANDROID__
	if ( !SDL_IsScreenKeyboardShown ( NULL ) ) {
		Q_strncpyz( g_console_text_input_buffer, "", sizeof(g_console_text_input_buffer) );
		SDL_ANDROID_GetScreenKeyboardTextInputAsync( g_console_text_input_buffer, sizeof(g_console_text_input_buffer) );
	}
#endif
}

/*
================
Con_Clear_f
================
*/
void Con_Clear_f (void) {
	int		i;

	for ( i = 0 ; i < CON_TEXTSIZE ; i++ ) {
		con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
	}

	Con_Bottom();		// go to end
}

						
/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
void Con_Dump_f (void)
{
	int		l, x, i;
	short	*line;
	fileHandle_t	f;
	char	buffer[1024];

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: condump <filename>\n");
		return;
	}

	Com_Printf ("Dumped console text to %s.\n", Cmd_Argv(1) );

	f = FS_FOpenFileWrite( Cmd_Argv( 1 ) );
	if (!f)
	{
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	// skip empty lines
	for (l = con.current - con.totallines + 1 ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for (x=0 ; x<con.linewidth ; x++)
			if ((line[x] & 0xff) != ' ')
				break;
		if (x != con.linewidth)
			break;
	}

	// write the remaining lines
	buffer[con.linewidth] = 0;
	for ( ; l <= con.current ; l++)
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for(i=0; i<con.linewidth; i++)
			buffer[i] = line[i] & 0xff;
		for (x=con.linewidth-1 ; x>=0 ; x--)
		{
			if (buffer[x] == ' ')
				buffer[x] = 0;
			else
				break;
		}
		strcat( buffer, "\n" );
		FS_Write(buffer, strlen(buffer), f);
	}

	FS_FCloseFile( f );
}


// Dump console to Android text input control

#ifdef __ANDROID__
enum { ANDROID_SHOW_LINES = 7 }; // Text input field may be very small even on big devices, it actually shows this number minus one
static void Con_AndroidTextInputShowLastMessages (void) {
	int		l, x, i, f;
	short	*line;
	char	buffer[(CON_TEXTSIZE * 11) / 10]; // Small extra space for \n symbols
	int		lastLines[ANDROID_SHOW_LINES];

	Com_Memset( lastLines, 0, sizeof(lastLines) );
	// skip empty lines
	for ( l = con.current - con.totallines + 1 ; l <= con.current ; l++ )
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for (x=0 ; x<con.linewidth ; x++)
			if ((line[x] & 0xff) != ' ')
				break;
		if (x != con.linewidth)
			break;
	}

	// write the remaining lines
	for ( f = 0; l <= con.current ; l++, f++ )
	{
		line = con.text + (l%con.totallines)*con.linewidth;
		for( i=0; i<con.linewidth; i++, f++ )
			buffer[f] = line[i] & 0xff;
		for ( x=con.linewidth-1, f-- ; x>=0 ; x--, f-- )
		{
			if (buffer[f] == ' ')
				buffer[f] = 0;
			else
				break;
		}
		f++;
		buffer[f] = '\n';
		for( x = 1; x < ANDROID_SHOW_LINES; x++ )
			lastLines[x-1] = lastLines[x];
		lastLines[ANDROID_SHOW_LINES-1] = f + 1;
	}
	if( f > 0 )
		f--;
	buffer[ f ] = 0;

	if( cl_runningOnOuya && !cl_runningOnOuya->integer )
		SDL_ANDROID_SetScreenKeyboardHintMesage( buffer + lastLines[0] ); // Hide console logs on Ouya - you need USB keyboard for console anyway
}
#endif

						
/*
================
Con_ClearNotify
================
*/
void Con_ClearNotify( void ) {
	int		i;
	
	for ( i = 0 ; i < NUM_CON_TIMES ; i++ ) {
		con.times[i] = 0;
	}
}

						

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize (void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	short	tbuf[CON_TEXTSIZE];

	width = (SCREEN_WIDTH / BIGCHAR_WIDTH) - 2;
	if (r_cardboardStereo && r_cardboardStereo->integer) {
		width /= 2;
	}

	if (width == con.linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = DEFAULT_CONSOLE_WIDTH;
		con.linewidth = width;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		for(i=0; i<CON_TEXTSIZE; i++)
			con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
	}
	else
	{
		oldwidth = con.linewidth;
		con.linewidth = width;
		oldtotallines = con.totallines;
		con.totallines = CON_TEXTSIZE / con.linewidth;
		numlines = oldtotallines;

		if (con.totallines < numlines)
			numlines = con.totallines;

		numchars = oldwidth;
	
		if (con.linewidth < numchars)
			numchars = con.linewidth;

		Com_Memcpy (tbuf, con.text, CON_TEXTSIZE * sizeof(short));
		for(i=0; i<CON_TEXTSIZE; i++)
			con.text[i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';

		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con.text[(con.totallines - 1 - i) * con.linewidth + j] =
						tbuf[((con.current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con.current = con.totallines - 1;
	con.display = con.current;
}

/*
==================
Cmd_CompleteTxtName
==================
*/
void Cmd_CompleteTxtName( char *args, int argNum ) {
	if( argNum == 2 ) {
		Field_CompleteFilename( "", "txt", qfalse, qtrue );
	}
}


/*
================
Con_Init
================
*/
void Con_Init (void) {
	int		i;

	con_notifytime = Cvar_Get ("con_notifytime", "6", 0);
	con_conspeed = Cvar_Get ("scr_conspeed", "3", 0);

	Field_Clear( &g_consoleField );
	g_consoleField.widthInChars = g_console_field_width;
	for ( i = 0 ; i < COMMAND_HISTORY ; i++ ) {
		Field_Clear( &historyEditLines[i] );
		historyEditLines[i].widthInChars = g_console_field_width;
	}
	CL_LoadConsoleHistory( );

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("messagemode3", Con_MessageMode3_f);
	Cmd_AddCommand ("messagemode4", Con_MessageMode4_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	Cmd_AddCommand ("condump", Con_Dump_f);
	Cmd_SetCommandCompletionFunc( "condump", Cmd_CompleteTxtName );
}

/*
================
Con_Shutdown
================
*/
void Con_Shutdown(void)
{
	Cmd_RemoveCommand("toggleconsole");
	Cmd_RemoveCommand("messagemode");
	Cmd_RemoveCommand("messagemode2");
	Cmd_RemoveCommand("messagemode3");
	Cmd_RemoveCommand("messagemode4");
	Cmd_RemoveCommand("clear");
	Cmd_RemoveCommand("condump");
}

/*
===============
Con_Linefeed
===============
*/
void Con_Linefeed (qboolean skipnotify)
{
	int		i;

	// mark time for transparent overlay
	if (con.current >= 0)
	{
    if (skipnotify)
		  con.times[con.current % NUM_CON_TIMES] = 0;
    else
		  con.times[con.current % NUM_CON_TIMES] = cls.realtime;
	}

	con.x = 0;
	if (con.display == con.current)
		con.display++;
	con.current++;
	for(i=0; i<con.linewidth; i++)
		con.text[(con.current%con.totallines)*con.linewidth+i] = (ColorIndex(COLOR_WHITE)<<8) | ' ';
}

/*
================
CL_ConsolePrint

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be logged to disk
If no console is visible, the text will appear at the top of the game window
================
*/
void CL_ConsolePrint( char *txt ) {
	int		y, l;
	unsigned char	c;
	unsigned short	color;
	qboolean skipnotify = qfalse;		// NERVE - SMF
	int prev;							// NERVE - SMF

	// TTimo - prefix for text that shows up in console but not in notify
	// backported from RTCW
	if ( !Q_strncmp( txt, "[skipnotify]", 12 ) ) {
		skipnotify = qtrue;
		txt += 12;
	}
	
	// for some demos we don't want to ever show anything on the console
	if ( cl_noprint && cl_noprint->integer ) {
		return;
	}
	
	if (!con.initialized) {
		con.color[0] = 
		con.color[1] = 
		con.color[2] =
		con.color[3] = 1.0f;
		con.linewidth = -1;
		Con_CheckResize ();
		con.initialized = qtrue;
	}

	color = ColorIndex(COLOR_WHITE);

	while ( (c = *((unsigned char *) txt)) != 0 ) {
		if ( Q_IsColorString( txt ) ) {
			color = ColorIndex( *(txt+1) );
			txt += 2;
			continue;
		}

		// count word length
		for (l=0 ; l< con.linewidth ; l++) {
			if ( txt[l] <= ' ') {
				break;
			}

		}

		// word wrap
		if (l != con.linewidth && (con.x + l >= con.linewidth) ) {
			Con_Linefeed(skipnotify);

		}

		txt++;

		switch (c)
		{
		case '\n':
			Con_Linefeed (skipnotify);
			break;
		case '\r':
			con.x = 0;
			break;
		default:	// display character and advance
			y = con.current % con.totallines;
			con.text[y*con.linewidth+con.x] = (color << 8) | c;
			con.x++;
			if(con.x >= con.linewidth)
				Con_Linefeed(skipnotify);
			break;
		}
	}


	// mark time for transparent overlay
	if (con.current >= 0) {
		// NERVE - SMF
		if ( skipnotify ) {
			prev = con.current % NUM_CON_TIMES - 1;
			if ( prev < 0 )
				prev = NUM_CON_TIMES - 1;
			con.times[prev] = 0;
		}
		else
		// -NERVE - SMF
			con.times[con.current % NUM_CON_TIMES] = cls.realtime;
	}

#ifdef __ANDROID__
	if( !skipnotify )
		Con_AndroidTextInputShowLastMessages( );
#endif
}


/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

Draw the editline after a ] prompt
================
*/
void Con_DrawInput (void) {
	int		y;

	if ( clc.state != CA_DISCONNECTED && !(Key_GetCatcher( ) & KEYCATCH_CONSOLE ) ) {
		return;
	}

	y = con.vislines - ( SMALLCHAR_HEIGHT * 2 );

	re.SetColor( con.color );

	SCR_DrawSmallChar( con.xadjust + 1 * SMALLCHAR_WIDTH, y, ']' );

	Field_Draw( &g_consoleField, con.xadjust + 2 * SMALLCHAR_WIDTH, y,
		SCREEN_WIDTH - 3 * SMALLCHAR_WIDTH, qtrue, qtrue );
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		x, v;
	short	*text;
	int		i;
	int		time;
	int		skip;
	int		currentColor;
	int		count;
	int		xOffset = 20;
	int		screenSide;

	for (screenSide = 0; screenSide <= r_cardboardStereo->integer; screenSide++, xOffset = SCREEN_WIDTH / 2) {
		currentColor = 7;
		re.SetColor( g_color_table[currentColor] );

		v = cg_weaponBarActiveWidth->integer ? 60 : 0;
		if (r_cardboardStereo && r_cardboardStereo->integer) {
			v += 20;
		}
		count = cg_weaponBarActiveWidth->integer ? NUM_CON_TIMES - 2 : NUM_CON_TIMES; // Actual amount of lines to print
		for (i= con.current-NUM_CON_TIMES+1 ; i<=con.current ; i++)
		{
			if (i < 0)
				continue;
			time = con.times[i % NUM_CON_TIMES];
			if (time == 0)
				continue;
			time = cls.realtime - time;
			if (time > con_notifytime->value*1000)
				continue;
			text = con.text + (i % con.totallines)*con.linewidth;
			if( i < con.current-count+1 )
				continue;

			if (cl.snap.ps.pm_type != PM_INTERMISSION && Key_GetCatcher( ) & (KEYCATCH_UI | KEYCATCH_CGAME) ) {
				continue;
			}

			for (x = 0 ; x < con.linewidth ; x++) {
				char buf[2];
				if ( ( text[x] & 0xff ) == ' ' ) {
					continue;
				}
				if ( ( (text[x]>>8) % NUMBER_OF_COLORS ) != currentColor ) {
					currentColor = (text[x]>>8) % NUMBER_OF_COLORS;
					re.SetColor( g_color_table[currentColor] );
				}
				buf[0] = text[x] & 0xff;
				buf[1] = 0;
				SCR_DrawBigStringColor( xOffset + con.xadjust + (x+1)*BIGCHAR_WIDTH, v, buf, g_color_table[currentColor], qtrue );
			}

			v += BIGCHAR_HEIGHT;
		}

		re.SetColor( NULL );

		if (Key_GetCatcher( ) & (KEYCATCH_UI | KEYCATCH_CGAME) ) {
			return;
		}

		// draw the chat line
		if ( Key_GetCatcher( ) & KEYCATCH_MESSAGE )
		{
			if (chat_team)
			{
				SCR_DrawBigString (8, v, "say_team:", 1.0f, qfalse );
				skip = 10;
			}
			else
			{
				SCR_DrawBigString (8, v, "say:", 1.0f, qfalse );
				skip = 5;
			}

			Field_BigDraw( &chatField, skip * BIGCHAR_WIDTH, v,
				SCREEN_WIDTH - ( skip + 1 ) * BIGCHAR_WIDTH, qtrue, qtrue );

			v += BIGCHAR_HEIGHT;
		}
	}

}

/*
================
Con_DrawSolidConsole

Draws the console with the solid background
================
*/
void Con_DrawSolidConsole( float frac ) {
	int				i, x, y;
	int				rows;
	short			*text;
	int				row;
	int				lines;
//	qhandle_t		conShader;
	int				currentColor;
	vec4_t			color;

	lines = cls.glconfig.vidHeight * frac;
	if (lines <= 0)
		return;

	if (lines > cls.glconfig.vidHeight )
		lines = cls.glconfig.vidHeight;

	// on wide screens, we will center the text
	con.xadjust = 0;
	SCR_AdjustFrom640( &con.xadjust, NULL, NULL, NULL );

	// draw the background
	y = frac * SCREEN_HEIGHT;
	if ( y < 1 ) {
		y = 0;
	}
	else {
		if ( cl_consoleType->integer ) {
			color[0] = cl_consoleType->integer > 1 ? cl_consoleColor[0]->value : 1.0f ;
			color[1] = cl_consoleType->integer > 1 ? cl_consoleColor[1]->value : 1.0f ;
			color[2] = cl_consoleType->integer > 1 ? cl_consoleColor[2]->value : 1.0f ;
			color[3] = cl_consoleColor[3]->value;
			re.SetColor( color );
		}
		if ( cl_consoleType->integer == 2 ) {
			SCR_DrawPic( 0, 0, SCREEN_WIDTH, y, cls.whiteShader );
		} else {
			SCR_DrawPic( 0, 0, SCREEN_WIDTH, y, cls.consoleShader );
		}
	}

	color[0] = 1;
	color[1] = 0;
	color[2] = 0;
	if( !cl_consoleType->integer )
		color[3] = 1;
	SCR_FillRect( 0, y, SCREEN_WIDTH, 2, color );


	// draw the version number

	re.SetColor( g_color_table[ColorIndex(COLOR_RED)] );

	i = strlen( Q3_VERSION );

	for (x=0 ; x<i ; x++) {
		SCR_DrawSmallChar( cls.glconfig.vidWidth - ( i - x + 1 ) * SMALLCHAR_WIDTH,
			lines - SMALLCHAR_HEIGHT, Q3_VERSION[x] );
	}


	// draw the text
	con.vislines = lines;
	rows = (lines-SMALLCHAR_WIDTH)/SMALLCHAR_WIDTH;		// rows of text to draw

	y = lines - (SMALLCHAR_HEIGHT*3);

	// draw from the bottom up
	if (con.display != con.current)
	{
	// draw arrows to show the buffer is backscrolled
		re.SetColor( g_color_table[ColorIndex(COLOR_RED)] );
		for (x=0 ; x<con.linewidth ; x+=4)
			SCR_DrawSmallChar( con.xadjust + (x+1)*SMALLCHAR_WIDTH, y, '^' );
		y -= SMALLCHAR_HEIGHT;
		rows--;
	}
	
	row = con.display;

	if ( con.x == 0 ) {
		row--;
	}

	currentColor = 7;
	re.SetColor( g_color_table[currentColor] );

	for (i=0 ; i<rows ; i++, y -= SMALLCHAR_HEIGHT, row--)
	{
		if (row < 0)
			break;
		if (con.current - row >= con.totallines) {
			// past scrollback wrap point
			continue;	
		}

		text = con.text + (row % con.totallines)*con.linewidth;

		for (x=0 ; x<con.linewidth ; x++) {
			if ( ( text[x] & 0xff ) == ' ' ) {
				continue;
			}

			if ( ( (text[x]>>8) % NUMBER_OF_COLORS ) != currentColor ) {
				currentColor = (text[x]>>8) % NUMBER_OF_COLORS;
				re.SetColor( g_color_table[currentColor] );
			}
			SCR_DrawSmallChar(  con.xadjust + (x+1)*SMALLCHAR_WIDTH, y, text[x] & 0xff );
		}
	}

	// draw the input prompt, user text, and cursor if desired
	Con_DrawInput ();

	re.SetColor( NULL );
}



/*
==================
Con_DrawConsole
==================
*/
void Con_DrawConsole( void ) {
	// check for console width changes from a vid mode change
	Con_CheckResize ();

	// if disconnected, render console full screen
	if ( clc.state == CA_DISCONNECTED ) {
		if ( !( Key_GetCatcher( ) & (KEYCATCH_UI | KEYCATCH_CGAME)) ) {
			Con_DrawSolidConsole( 1.0 );
			return;
		}
	}

	if ( con.displayFrac ) {
		Con_DrawSolidConsole( con.displayFrac );
	} else {
		// draw notify lines
		if ( clc.state == CA_ACTIVE ) {
			Con_DrawNotify ();
		}
	}
}

//================================================================

/*
==================
Con_RunConsole

Scroll it up or down
==================
*/
void Con_RunConsole (void) {
	// decide on the destination height of the console
	if ( Key_GetCatcher( ) & KEYCATCH_CONSOLE )
		con.finalFrac = con.userFrac;
	else
		con.finalFrac = 0;				// none visible
	
	// scroll towards the destination height
	if (con.finalFrac < con.displayFrac)
	{
		con.displayFrac -= con_conspeed->value*cls.realFrametime*0.001;
		if (con.finalFrac > con.displayFrac)
			con.displayFrac = con.finalFrac;

	}
	else if (con.finalFrac > con.displayFrac)
	{
		con.displayFrac += con_conspeed->value*cls.realFrametime*0.001;
		if (con.finalFrac < con.displayFrac)
			con.displayFrac = con.finalFrac;
	}

#ifdef __ANDROID__
	// Process Android text input
	//Com_Printf("clc.state %d CA_ACTIVE %d Key_GetCatcher %d &~ %d\n", clc.state, clc.state == CA_ACTIVE, Key_GetCatcher(), (Key_GetCatcher( ) & ~(KEYCATCH_CGAME | KEYCATCH_MESSAGE)) == 0);
	if ( SDL_IsScreenKeyboardShown ( NULL ) )
	{
		if ( SDL_ANDROID_GetScreenKeyboardTextInputAsync( g_console_text_input_buffer, sizeof(g_console_text_input_buffer) ) == SDL_ANDROID_TEXTINPUT_ASYNC_FINISHED )
		{
			if ( g_console_text_input_buffer[0] != 0 && g_console_text_input_buffer[strlen(g_console_text_input_buffer) - 1] == '\n' )
			{
				g_console_text_input_buffer[strlen(g_console_text_input_buffer) - 1] = 0;
			}
			if ( Key_GetCatcher( ) & KEYCATCH_CONSOLE )
			{
				// leading slash is an explicit command
				if ( g_console_text_input_buffer[0] == '\\' || g_console_text_input_buffer[0] == '/' )
				{
					Cbuf_AddText( g_console_text_input_buffer + 1 );	// valid command
					Cbuf_AddText( "\n" );
				}
				else
				{
					Cbuf_AddText( g_console_text_input_buffer );	// valid command
					Cbuf_AddText( "\n" );
				}

				g_console_text_input_buffer[0] = 0;
				Con_Close();
			}
			else if (g_console_text_input_toggled_ui)
			{
				// Translate to key input events somehow
				for (int i = 0; i < strlen(g_console_text_input_buffer); i++)
				{
					char key = g_console_text_input_buffer[i];
					//CL_CharEvent(g_console_text_input_buffer[i]);
					if ((key >= 'a' && key <= 'z') || (key >= '0' && key <= '9'))
					{
						CL_KeyEvent( key, qtrue, 0 );
						CL_KeyEvent( key, qfalse, 0 );
					}
					if (key >= 'A' && key <= 'Z')
					{
						CL_KeyEvent( K_SHIFT, qtrue, 0 );
						CL_KeyEvent( key + 'a' - 'A', qtrue, 0 );
						CL_KeyEvent( key + 'a' - 'A', qfalse, 0 );
						CL_KeyEvent( K_SHIFT, qfalse, 0 );
					}
					if (key == ' ' || key == '-' || key == '=' || key == '\\' ||
						key == '[' || key == ']' || key == ';' || key == '\'' ||
						key == ',' || key == '.' || key == '/')
					{
						CL_KeyEvent( key, qtrue, 0 );
						CL_KeyEvent( key, qfalse, 0 );
					}
					// TODO: this is ugly!
					if (key == '_')
					{
						CL_KeyEvent( K_SHIFT, qtrue, 0 );
						CL_KeyEvent( '-', qtrue, 0 );
						CL_KeyEvent( '-', qfalse, 0 );
						CL_KeyEvent( K_SHIFT, qfalse, 0 );
					}
					if (key == '+')
					{
						CL_KeyEvent( K_SHIFT, qtrue, 0 );
						CL_KeyEvent( '=', qtrue, 0 );
						CL_KeyEvent( '=', qfalse, 0 );
						CL_KeyEvent( K_SHIFT, qfalse, 0 );
					}
					if (key == '|')
					{
						CL_KeyEvent( K_SHIFT, qtrue, 0 );
						CL_KeyEvent( '\\', qtrue, 0 );
						CL_KeyEvent( '\\', qfalse, 0 );
						CL_KeyEvent( K_SHIFT, qfalse, 0 );
					}
					if (key == '{')
					{
						CL_KeyEvent( K_SHIFT, qtrue, 0 );
						CL_KeyEvent( '[', qtrue, 0 );
						CL_KeyEvent( '[', qfalse, 0 );
						CL_KeyEvent( K_SHIFT, qfalse, 0 );
					}
					if (key == '}')
					{
						CL_KeyEvent( K_SHIFT, qtrue, 0 );
						CL_KeyEvent( ']', qtrue, 0 );
						CL_KeyEvent( ']', qfalse, 0 );
						CL_KeyEvent( K_SHIFT, qfalse, 0 );
					}
					if (key == ':')
					{
						CL_KeyEvent( K_SHIFT, qtrue, 0 );
						CL_KeyEvent( ';', qtrue, 0 );
						CL_KeyEvent( ';', qfalse, 0 );
						CL_KeyEvent( K_SHIFT, qfalse, 0 );
					}
					if (key == '"')
					{
						CL_KeyEvent( K_SHIFT, qtrue, 0 );
						CL_KeyEvent( '\'', qtrue, 0 );
						CL_KeyEvent( '\'', qfalse, 0 );
						CL_KeyEvent( K_SHIFT, qfalse, 0 );
					}
					if (key == '<')
					{
						CL_KeyEvent( K_SHIFT, qtrue, 0 );
						CL_KeyEvent( ',', qtrue, 0 );
						CL_KeyEvent( ',', qfalse, 0 );
						CL_KeyEvent( K_SHIFT, qfalse, 0 );
					}
					if (key == '>')
					{
						CL_KeyEvent( K_SHIFT, qtrue, 0 );
						CL_KeyEvent( '.', qtrue, 0 );
						CL_KeyEvent( '.', qfalse, 0 );
						CL_KeyEvent( K_SHIFT, qfalse, 0 );
					}
					if (key == '?')
					{
						CL_KeyEvent( K_SHIFT, qtrue, 0 );
						CL_KeyEvent( '/', qtrue, 0 );
						CL_KeyEvent( '/', qfalse, 0 );
						CL_KeyEvent( K_SHIFT, qfalse, 0 );
					}
				}

				g_console_text_input_toggled_ui = 0;
			}
			else
			{
				if ( g_console_text_input_buffer[0] == '/' || g_console_text_input_buffer[0] == '\\' )
				{
					// Console command
					Cbuf_AddText( g_console_text_input_buffer + 1 );
					Cbuf_AddText( "\n" );
				}
				else if ( g_console_text_input_buffer[0] && clc.state == CA_ACTIVE )
				{
					char buffer[MAX_STRING_CHARS];

					if (chat_playerNum != -1 )
						Com_sprintf( buffer, sizeof( buffer ), "tell %i \"%s\"\n", chat_playerNum, g_console_text_input_buffer );
					else if (chat_team)
						Com_sprintf( buffer, sizeof( buffer ), "say_team \"%s\"\n", g_console_text_input_buffer );
					else
						Com_sprintf( buffer, sizeof( buffer ), "say \"%s\"\n", g_console_text_input_buffer );

					CL_AddReliableCommand(buffer, qfalse);
				}
				else if ( !g_console_text_input_buffer[0] && clc.state == CA_ACTIVE )
				{
					Cbuf_AddText( "gesture\n" );
				}

				Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_MESSAGE );
			}
		}
	}
	else
	{
		// If screen keyboard shown in UI - switch to console
		if ( Key_GetCatcher( ) & KEYCATCH_CONSOLE )
		{
			if ( !SDL_IsScreenKeyboardShown ( NULL ) )
			{
				g_console_text_input_buffer[0] = 0;
				SDL_ANDROID_GetScreenKeyboardTextInputAsync( g_console_text_input_buffer, sizeof(g_console_text_input_buffer) );
			}
		}
	}
#endif
}

/*
==================
Con_SetFrac
==================
*/
void Con_SetFrac(const float conFrac)
{
	// clamp the cvar value
	if (conFrac < .1f) {	// don't let the console be hidden
		con.userFrac = .1f;
	} else if (conFrac > 1.0f) {
		con.userFrac = 1.0f;
	} else {
		con.userFrac = conFrac;
	}
}

void Con_PageUp( void ) {
	con.display -= 2;
	if ( con.current - con.display >= con.totallines ) {
		con.display = con.current - con.totallines + 1;
	}
}

void Con_PageDown( void ) {
	con.display += 2;
	if (con.display > con.current) {
		con.display = con.current;
	}
}

void Con_Top( void ) {
	con.display = con.totallines;
	if ( con.current - con.display >= con.totallines ) {
		con.display = con.current - con.totallines + 1;
	}
}

void Con_Bottom( void ) {
	con.display = con.current;
}


void Con_Close( void ) {
	if ( !com_cl_running->integer ) {
		return;
	}
	Field_Clear( &g_consoleField );
	Con_ClearNotify ();
	Key_SetCatcher( Key_GetCatcher( ) & ~KEYCATCH_CONSOLE );
	con.finalFrac = 0;				// none visible
	con.displayFrac = 0;
}
