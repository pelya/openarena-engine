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
// cl.input.c  -- builds an intended movement command to send to the server

#include "client.h"

unsigned	frame_msec;
int			old_com_frameTime;

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as argv(1) so it can be matched up with the release.

argv(2) will be set to the time the event happened, which allows exact
control even at low framerates when the down and up events may both get qued
at the same time.

===============================================================================
*/


kbutton_t	in_left, in_right, in_forward, in_back;
kbutton_t	in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t	in_strafe, in_speed;
kbutton_t	in_up, in_down;
static short in_androidCameraYawSpeed, in_androidCameraPitchSpeed, in_androidCameraMultitouchYawSpeed, in_androidWeaponSelectionBarActive;
static short in_swipeActivated, in_joystickJumpTriggerTime, in_attackButtonReleased, in_mouseSwipingActive, in_multitouchActive;
static int in_mouseX, in_mouseY, in_multitouchX, in_multitouchY, in_tapMouseX, in_tapMouseY, in_swipeTime;
static float in_swipeAngleRotate, in_swipeAngleRotatePitch;
static qboolean in_railgunZoomActive;
static qboolean in_deferShooting;
static const float in_swipeSpeed = 0.2f;
#define TOUCHSCREEN_TAP_AREA (cls.glconfig.vidHeight / 6)
#ifdef USE_VOIP
kbutton_t	in_voiprecord;
#endif
kbutton_t	in_buttons[16];
qboolean	in_mlooking;
enum		{ GYRO_AXES_SWAP_X = 1, GYRO_AXES_SWAP_Y = 2, GYRO_AXES_SWAP_XY = 4 };

static void CL_AdjustCrosshairPosNearEdges( int * dx, int * dy );

void IN_MLookDown( void ) {
	in_mlooking = qtrue;
}

void IN_MLookUp( void ) {
	in_mlooking = qfalse;
	if ( !cl_freelook->integer ) {
		IN_CenterViewDown ();
		IN_CenterViewUp ();
	}
}

void IN_KeyDown( kbutton_t *b ) {
	int		k;
	char	*c;
	
	c = Cmd_Argv(1);
	if ( c[0] ) {
		k = atoi(c);
	} else {
		k = -1;		// typed manually at the console for continuous down
	}

	if ( k == b->down[0] || k == b->down[1] ) {
		return;		// repeating key
	}
	
	if ( !b->down[0] ) {
		b->down[0] = k;
	} else if ( !b->down[1] ) {
		b->down[1] = k;
	} else {
		Com_Printf ("Three keys down for a button!\n");
		return;
	}
	
	if ( b->active ) {
		return;		// still down
	}

	// save timestamp for partial frame summing
	c = Cmd_Argv(2);
	b->downtime = atoi(c);

	b->active = qtrue;
	b->wasPressed = qtrue;
}

void IN_KeyUp( kbutton_t *b ) {
	int		k;
	char	*c;
	unsigned	uptime;

	c = Cmd_Argv(1);
	if ( c[0] ) {
		k = atoi(c);
	} else {
		// typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->active = qfalse;
		return;
	}

	if ( b->down[0] == k ) {
		b->down[0] = 0;
	} else if ( b->down[1] == k ) {
		b->down[1] = 0;
	} else {
		return;		// key up without coresponding down (menu pass through)
	}
	if ( b->down[0] || b->down[1] ) {
		return;		// some other key is still holding it down
	}

	b->active = qfalse;

	// save timestamp for partial frame summing
	c = Cmd_Argv(2);
	uptime = atoi(c);
	if ( uptime ) {
		b->msec += uptime - b->downtime;
	} else {
		b->msec += frame_msec / 2;
	}

	b->active = qfalse;
}



/*
===============
CL_KeyState

Returns the fraction of the frame that the key was down
===============
*/
float CL_KeyState( kbutton_t *key ) {
	float		val;
	int			msec;

	msec = key->msec;
	key->msec = 0;

	if ( key->active ) {
		// still down
		if ( !key->downtime ) {
			msec = com_frameTime;
		} else {
			msec += com_frameTime - key->downtime;
		}
		key->downtime = com_frameTime;
	}

#if 0
	if (msec) {
		Com_Printf ("%i ", msec);
	}
#endif

	val = (float)msec / frame_msec;
	if ( val < 0 ) {
		val = 0;
	}
	if ( val > 1 ) {
		val = 1;
	}

	return val;
}



void IN_UpDown(void) {IN_KeyDown(&in_up);}
void IN_UpUp(void) {IN_KeyUp(&in_up);}
void IN_DownDown(void) {IN_KeyDown(&in_down);}
void IN_DownUp(void) {IN_KeyUp(&in_down);}
void IN_LeftDown(void) {IN_KeyDown(&in_left);}
void IN_LeftUp(void) {IN_KeyUp(&in_left);}
void IN_RightDown(void) {IN_KeyDown(&in_right);}
void IN_RightUp(void) {IN_KeyUp(&in_right);}
void IN_ForwardDown(void) {IN_KeyDown(&in_forward);}
void IN_ForwardUp(void) {IN_KeyUp(&in_forward);}
void IN_BackDown(void) {IN_KeyDown(&in_back);}
void IN_BackUp(void) {IN_KeyUp(&in_back);}
void IN_LookupDown(void) {IN_KeyDown(&in_lookup);}
void IN_LookupUp(void) {IN_KeyUp(&in_lookup);}
void IN_LookdownDown(void) {IN_KeyDown(&in_lookdown);}
void IN_LookdownUp(void) {IN_KeyUp(&in_lookdown);}
void IN_MoveleftDown(void) {IN_KeyDown(&in_moveleft);}
void IN_MoveleftUp(void) {IN_KeyUp(&in_moveleft);}
void IN_MoverightDown(void) {IN_KeyDown(&in_moveright);}
void IN_MoverightUp(void) {IN_KeyUp(&in_moveright);}

void IN_SpeedDown(void) {IN_KeyDown(&in_speed);}
void IN_SpeedUp(void) {IN_KeyUp(&in_speed);}
void IN_StrafeDown(void) {IN_KeyDown(&in_strafe);}
void IN_StrafeUp(void) {IN_KeyUp(&in_strafe);}

#ifdef USE_VOIP
void IN_VoipRecordDown(void)
{
	IN_KeyDown(&in_voiprecord);
	Cvar_Set("cl_voipSend", "1");
}

void IN_VoipRecordUp(void)
{
	IN_KeyUp(&in_voiprecord);
	Cvar_Set("cl_voipSend", "0");
}
#endif

void IN_Button0Down(void)
{
	int weaponX = in_mouseX * 640 / cls.glconfig.vidWidth;

	if (	cg_holdingUsableItem->integer &&
			in_mouseY < cls.glconfig.vidHeight / 6 &&
			in_mouseX > cls.glconfig.vidWidth * 5 / 6 ) {
		// Use item
		IN_KeyDown(&in_buttons[2]);
		IN_KeyUp(&in_buttons[2]);
	} else if (	in_androidWeaponSelectionBarActive && ( (
				!cg_weaponBarAtBottom->integer &&
				weaponX > 320 - cg_weaponBarActiveWidth->integer &&
				weaponX < 320 + cg_weaponBarActiveWidth->integer ) || (
				cg_weaponBarAtBottom->integer &&
				weaponX > 640 - cg_weaponBarActiveWidth->integer * 2 ) ) ) {
		char cmd[64] = "weapon ";
		int count = ( !cg_weaponBarAtBottom->integer ? weaponX - 320 + cg_weaponBarActiveWidth->integer : weaponX - 640 + cg_weaponBarActiveWidth->integer * 2 ) / 40;
		char * c = cg_weaponBarActiveWeapons->string, * c2;
		int i;

		for ( i = 0; i < count; i++ ) {
			c = strchr ( c, '/' );
			if ( c == NULL )
				return;
			c++;
		}
		c2 = strchr ( c, '/' );
		if ( c2 == NULL )
			return;
		strncat ( cmd, c, c2 - c );
		Cbuf_AddText( cmd );
	} else {
		if ( cg_touchscreenControls->integer == TOUCHSCREEN_FLOATING_CROSSHAIR ) {
			IN_KeyDown(&in_buttons[0]);
		} else {
			// Ignore mouse keypresses, process only keyboard
			int k = -1;
			if ( Cmd_Argv(1)[0] )
				k = atoi(Cmd_Argv(1));
			if ( k != K_MOUSE1 )
				IN_KeyDown(&in_buttons[0]);
			else {
				in_mouseSwipingActive = 1;
				if ( cg_touchscreenControls->integer != TOUCHSCREEN_SHOOT_UNDER_FINGER ) {
					in_swipeTime = 0;
					in_swipeAngleRotate = cl.viewangles[YAW];
					in_swipeActivated = 0;
				}
				if ( cg_touchscreenControls->integer == TOUCHSCREEN_TAP_TO_FIRE ||
					 cg_touchscreenControls->integer == TOUCHSCREEN_AIM_UNDER_FINGER ) {
					int tapArea = TOUCHSCREEN_TAP_AREA / 2;
					if (	cl.touchscreenAttackButtonPos[4] > 0.0f &&
							in_mouseX > in_tapMouseX - tapArea &&
							in_mouseX < in_tapMouseX + tapArea &&
							in_mouseY > in_tapMouseY - tapArea &&
							in_mouseY < in_tapMouseY + tapArea ) { // TODO: make it octagon, not square
						IN_KeyDown(&in_buttons[0]);
					}
					cl.touchscreenAttackButtonPos[4] = 0.0f;
				}
				if ( !in_buttons[0].active && (
					 cg_touchscreenControls->integer == TOUCHSCREEN_SHOOT_UNDER_FINGER ||
					 cg_touchscreenControls->integer == TOUCHSCREEN_AIM_UNDER_FINGER ) ) {
					float yaw = -RAD2DEG( atanf((in_mouseX - cls.glconfig.vidWidth/2) * 2.0f / cls.glconfig.vidWidth) ) * cl.cgameSensitivity;
					float pitch = RAD2DEG( atanf((in_mouseY - cls.glconfig.vidHeight/2) * 2.0f / cls.glconfig.vidWidth) ) * cl.cgameSensitivity;

					//Com_Printf( "angles diff %f %f\n",
					//	-RAD2DEG( atanf((in_mouseX - cls.glconfig.vidWidth/2) * 2.0f / cls.glconfig.vidWidth) ) * cl.cgameSensitivity,
					//	RAD2DEG( atanf((in_mouseY - cls.glconfig.vidHeight/2) * 2.0f / cls.glconfig.vidWidth) ) * cl.cgameSensitivity);

					if ( cg_touchscreenControls->integer == TOUCHSCREEN_SHOOT_UNDER_FINGER ) {
						IN_KeyDown(&in_buttons[0]);
						in_deferShooting = qtrue; // Because cgame needs to process touch event before starting to shoot
						CL_AdjustCrosshairPosNearEdges( &in_mouseX, &in_mouseY );
						if ( !( in_androidCameraYawSpeed || in_androidCameraPitchSpeed ) ) {
							// Do not adjust angles instantly if we touched near the edge, rotate camera instead
							cl.viewangles[YAW] += yaw;
							cl.viewangles[PITCH] += pitch;
						}
					} else {
						//cl.viewangles[YAW] += yaw;
						//cl.viewangles[PITCH] += pitch;
						in_swipeActivated = 1;
						in_swipeAngleRotate = yaw;
						in_swipeAngleRotatePitch = pitch;
					}
				}
			}
		}
	}
}
void IN_Button0Up(void)
{
	cl.touchscreenAttackButtonPos[4] = 0.0f;
	if ( in_buttons[0].active )
		in_attackButtonReleased = 1;
	if ( cg_touchscreenControls->integer == TOUCHSCREEN_FLOATING_CROSSHAIR ) {
		IN_KeyUp(&in_buttons[0]);
	} else {
		// Ignore mouse keypresses, process only keyboard
		int k = -1;
		if ( Cmd_Argv(1)[0] )
			k = atoi(Cmd_Argv(1));
		if ( k != K_MOUSE1 )
			IN_KeyUp(&in_buttons[0]);
		else {
			float angleDiff = AngleSubtract( cl.viewangles[YAW], in_swipeAngleRotate ); // It will normalize the resulting angle
			in_mouseSwipingActive = 0;
			if ( in_swipeTime < 300 && fabs(angleDiff) > in_swipeSensitivity->value && in_swipeAngle->value &&
				 cg_touchscreenControls->integer != TOUCHSCREEN_SHOOT_UNDER_FINGER &&
				 cg_touchscreenControls->integer != TOUCHSCREEN_AIM_UNDER_FINGER ) {
				in_swipeAngleRotate = angleDiff > 0 ? in_swipeAngle->value - angleDiff : -in_swipeAngle->value - angleDiff;
				in_swipeActivated = 1;
			}

			if ( cg_touchscreenControls->integer == TOUCHSCREEN_TAP_TO_FIRE ||
				 cg_touchscreenControls->integer == TOUCHSCREEN_AIM_UNDER_FINGER ) {
				int weaponX = in_mouseX * 640 / cls.glconfig.vidWidth;
				IN_KeyUp(&in_buttons[0]);
				in_tapMouseX = in_mouseX;
				in_tapMouseY = in_mouseY;
				cl.touchscreenAttackButtonPos[2] = cl.touchscreenAttackButtonPos[3] = TOUCHSCREEN_TAP_AREA;
				cl.touchscreenAttackButtonPos[0] = in_mouseX - cl.touchscreenAttackButtonPos[2] * 0.5f;
				cl.touchscreenAttackButtonPos[1] = in_mouseY - cl.touchscreenAttackButtonPos[3] * 0.5f;
				cl.touchscreenAttackButtonPos[4] = 0.75f;
				if ( ( Key_GetCatcher( ) & ~KEYCATCH_CGAME || clc.state != CA_ACTIVE ) || (
					in_androidWeaponSelectionBarActive && ( (
					!cg_weaponBarAtBottom->integer &&
					weaponX > 320 - cg_weaponBarActiveWidth->integer &&
					weaponX < 320 + cg_weaponBarActiveWidth->integer ) || (
					cg_weaponBarAtBottom->integer &&
					weaponX > 640 - cg_weaponBarActiveWidth->integer * 2 ) ) ) ) {
					// We're inside UI, or toggling weapons, disable on-screen button
					cl.touchscreenAttackButtonPos[4] = 0.0f;
				}
			}
			if ( cg_touchscreenControls->integer == TOUCHSCREEN_SHOOT_UNDER_FINGER )
				IN_KeyUp(&in_buttons[0]);
		}
	}
}
void IN_Button1Down(void) {IN_KeyDown(&in_buttons[1]);}
void IN_Button1Up(void) {IN_KeyUp(&in_buttons[1]);}
void IN_Button2Down(void) {IN_KeyDown(&in_buttons[2]);}
void IN_Button2Up(void) {IN_KeyUp(&in_buttons[2]);}
void IN_Button3Down(void) {IN_KeyDown(&in_buttons[3]);}
void IN_Button3Up(void) {IN_KeyUp(&in_buttons[3]);}
void IN_Button4Down(void) {IN_KeyDown(&in_buttons[4]);}
void IN_Button4Up(void) {IN_KeyUp(&in_buttons[4]);}
void IN_Button5Down(void) {IN_KeyDown(&in_buttons[5]);}
void IN_Button5Up(void) {IN_KeyUp(&in_buttons[5]);}
void IN_Button6Down(void) {IN_KeyDown(&in_buttons[6]);}
void IN_Button6Up(void) {IN_KeyUp(&in_buttons[6]);}
void IN_Button7Down(void) {IN_KeyDown(&in_buttons[7]);}
void IN_Button7Up(void) {IN_KeyUp(&in_buttons[7]);}
void IN_Button8Down(void) {IN_KeyDown(&in_buttons[8]);}
void IN_Button8Up(void) {IN_KeyUp(&in_buttons[8]);}
void IN_Button9Down(void) {IN_KeyDown(&in_buttons[9]);}
void IN_Button9Up(void) {IN_KeyUp(&in_buttons[9]);}
void IN_Button10Down(void) {IN_KeyDown(&in_buttons[10]);}
void IN_Button10Up(void) {IN_KeyUp(&in_buttons[10]);}
void IN_Button11Down(void) {IN_KeyDown(&in_buttons[11]);}
void IN_Button11Up(void) {IN_KeyUp(&in_buttons[11]);}
void IN_Button12Down(void) {IN_KeyDown(&in_buttons[12]);}
void IN_Button12Up(void) {IN_KeyUp(&in_buttons[12]);}
void IN_Button13Down(void) {IN_KeyDown(&in_buttons[13]);}
void IN_Button13Up(void) {IN_KeyUp(&in_buttons[13]);}
void IN_Button14Down(void) {IN_KeyDown(&in_buttons[14]);}
void IN_Button14Up(void) {IN_KeyUp(&in_buttons[14]);}
void IN_Gesture(void) {in_buttons[3].wasPressed = qtrue;}

void IN_CenterViewDown (void) {
	//cl.viewangles[PITCH] = -SHORT2ANGLE(cl.snap.ps.delta_angles[PITCH]);

	// User released joystick, then pressed the centerview button - it will rotate to the last joystick direction
	if ( cl.joystickAxis[JOY_AXIS_SCREENJOY_X] == 0 && cl.joystickAxis[JOY_AXIS_SCREENJOY_Y] == 0 ) {
		in_swipeActivated = 1;
		in_joystickJumpTriggerTime = 0; // Do not jump if user rotated view and immediately put finger back on joystick
	}
}

void IN_CenterViewUp (void) {
}

//==========================================================================

cvar_t	*cl_yawspeed;
cvar_t	*cl_pitchspeed;
cvar_t	*cl_pitchAutoCenter;

cvar_t	*cl_run;

cvar_t	*cl_anglespeedkey;


/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
void CL_AdjustAngles( void ) {
	float right = CL_KeyState (&in_right), left = CL_KeyState (&in_left);
	float up = CL_KeyState (&in_lookup), down = CL_KeyState (&in_lookdown);
	float speed = cls.unscaledFrametime * cl_sensitivity->value * cl.cgameSensitivity * 0.04f;

	if ( left > 0 || right > 0 || up > 0 || down > 0 ) {
		cl.viewangles[YAW] -= speed * right;
		cl.viewangles[YAW] += speed * left;
		cl.viewangles[PITCH] -= speed * up;
		cl.viewangles[PITCH] += speed * down;
	}

	speed /= 32767.0f;

	if ( abs(cl.joystickAxis[JOY_AXIS_GAMEPADRIGHT_X]) > 4096 ) {
		int rescaled = (abs(cl.joystickAxis[JOY_AXIS_GAMEPADRIGHT_X]) - 4096) *
						(cl.joystickAxis[JOY_AXIS_GAMEPADRIGHT_X] > 0 ? 1 : -1);
		cl.viewangles[YAW] -= speed * rescaled;
	}
	// Slower vertical movement
	if ( abs(cl.joystickAxis[JOY_AXIS_GAMEPADRIGHT_Y]) > 12288 ) {
		int rescaled = (abs(cl.joystickAxis[JOY_AXIS_GAMEPADRIGHT_Y]) - 12288) *
						(cl.joystickAxis[JOY_AXIS_GAMEPADRIGHT_Y] > 0 ? 1 : -1) *
						(m_pitch->value < 0 ? -1 : 1);
		cl.viewangles[PITCH] += speed * rescaled;
	}

	// Gyroscope
	if ( in_gyroscope->integer ) {
		float x = cl.gyroscope[0], y = cl.gyroscope[1];
		if ( in_gyroscopeAxesSwap->integer & GYRO_AXES_SWAP_X ) {
			x = -x;
		}
		if ( in_gyroscopeAxesSwap->integer & GYRO_AXES_SWAP_Y ) {
			y = -y;
		}
		if ( in_gyroscopeAxesSwap->integer & GYRO_AXES_SWAP_XY ) {
			float xy = x;
			x = y;
			y = xy;
		}
		if ( x != 0 || y != 0 || cl.gyroscope[2] != 0 ) {
			cl.viewangles[YAW] += x * (1.0f / 16384.0f) * cl.cgameSensitivity * in_gyroscopeSensitivity->value;
			cl.viewangles[PITCH] += y * (1.0f / 16384.0f) * cl.cgameSensitivity * in_gyroscopeSensitivity->value;
			cl.viewangles[ROLL] -= cl.gyroscope[2] * (1.0f / 16384.0f);
		}
		if( fabs(cl.viewangles[ROLL]) > speed * 2000.0f ) {
			cl.viewangles[ROLL] -= ( cl.viewangles[ROLL] > 0 ) ? speed * 2000.0f : speed * -2000.0f;
			if( fabs(cl.viewangles[ROLL]) > 8.0f )
				cl.viewangles[ROLL] = ( cl.viewangles[ROLL] > 0 ) ? 8.0f :  -8.0f;
		}
		// Clear it
		cl.gyroscope[0] = cl.gyroscope[1] = cl.gyroscope[2] = 0;
	} else {
		cl.viewangles[ROLL] = 0;
	}

	// Swipe touchscreen gesture
	if ( in_swipeActivated ) {
		float diff = cls.unscaledFrametime * in_swipeSpeed * ( ( in_swipeAngleRotate > 0 ) ? 1 : -1 );
		float diffPitch = cls.unscaledFrametime * in_swipeSpeed * ( ( in_swipeAngleRotatePitch > 0 ) ? 1 : -1 );

		if ( fabs( in_swipeAngleRotate ) <= fabs( diff ) ) {
			diff = in_swipeAngleRotate;
			in_swipeAngleRotate = 0;
		} else {
			in_swipeAngleRotate -= diff;
		}

		if ( fabs( in_swipeAngleRotatePitch ) <= fabs( diffPitch ) ) {
			diffPitch = in_swipeAngleRotatePitch;
			in_swipeAngleRotatePitch = 0;
		} else {
			in_swipeAngleRotatePitch -= diffPitch;
		}

		if ( in_swipeAngleRotate == 0 && in_swipeAngleRotatePitch == 0 )
			in_swipeActivated = 0;

		cl.viewangles[YAW] += diff;
		cl.viewangles[PITCH] += diffPitch;
	}
	in_swipeTime += cls.unscaledFrametime;
}

/*
================
CL_KeyMove

Sets the usercmd_t based on key states
================
*/
void CL_KeyMove( usercmd_t *cmd ) {
	int		movespeed;
	int		forward, side, up;

	//
	// adjust for speed key / running
	// the walking flag is to keep animations consistant
	// even during acceleration and develeration
	//
	if ( in_speed.active ^ cl_run->integer ) {
		movespeed = 127;
		cmd->buttons &= ~BUTTON_WALKING;
	} else {
		cmd->buttons |= BUTTON_WALKING;
		movespeed = 64;
	}

	forward = 0;
	side = 0;
	up = 0;
	if ( in_strafe.active ) {
		side += movespeed * CL_KeyState (&in_right);
		side -= movespeed * CL_KeyState (&in_left);
	}

	side += movespeed * CL_KeyState (&in_moveright);
	side -= movespeed * CL_KeyState (&in_moveleft);


	up += movespeed * CL_KeyState (&in_up);
	up -= movespeed * CL_KeyState (&in_down);

	forward += movespeed * CL_KeyState (&in_forward);
	forward -= movespeed * CL_KeyState (&in_back);

	cmd->forwardmove = ClampChar( forward );
	cmd->rightmove = ClampChar( side );
	cmd->upmove = ClampChar( up );
}

#define SCALE( src, border, from, to ) src = ( (border) + ( (src) - (border) ) * ( (to) - (border) ) / ( (from) - (border) ) )
static void CL_AdjustCrosshairPosNearEdges( int * dx, int * dy ) {
	int x = *dx;
	int y = *dy;
	// TODO: hardcoded values, make them configurable
	int border = cls.glconfig.vidHeight / 6;
	int offset = border / 2 * in_swipeFreeCrosshairOffset->value;

	if ( cl_runningOnOuya->integer )
		return;

	in_androidCameraYawSpeed = in_androidCameraPitchSpeed = in_androidWeaponSelectionBarActive = 0;

	if ( cg_touchscreenControls->integer != TOUCHSCREEN_FLOATING_CROSSHAIR &&
		 cg_touchscreenControls->integer != TOUCHSCREEN_SHOOT_UNDER_FINGER ) {
		if ( !cg_weaponBarAtBottom->integer ) {
			if ( y < border )
				in_androidWeaponSelectionBarActive = 1;
		} else {
			if ( y > cls.glconfig.vidHeight - border )
				in_androidWeaponSelectionBarActive = 1;
		}
		return;
	}

	if ( x < border * 3 ) {
		if ( x < border * 2 )
			in_androidCameraYawSpeed = 1;
		if ( in_swipeFreeStickyEdges->integer )
			SCALE( x, border * 3, border * 2, offset );
	} else if ( x > cls.glconfig.vidWidth - border * 2 ) {
		if ( x > cls.glconfig.vidWidth - border )
			in_androidCameraYawSpeed = -1;
		if ( in_swipeFreeStickyEdges->integer )
			SCALE( x, cls.glconfig.vidWidth - border * 2, cls.glconfig.vidWidth - border, cls.glconfig.vidWidth + offset );
	}

	if ( y < border * 2 ) {
		if ( y < border ) {
			in_androidCameraPitchSpeed = -1;
			if ( !cg_weaponBarAtBottom->integer )
				in_androidWeaponSelectionBarActive = 1;
		}
		if ( in_swipeFreeStickyEdges->integer )
			SCALE( y, border * 2, border, offset );
	} else if ( y > cls.glconfig.vidHeight - border * 2 ) {
		if ( y > cls.glconfig.vidHeight - border ) {
			in_androidCameraPitchSpeed = 1;
			if ( cg_weaponBarAtBottom->integer )
				in_androidWeaponSelectionBarActive = 1;
		}
		if ( in_swipeFreeStickyEdges->integer )
			SCALE( y, cls.glconfig.vidHeight - border * 2, cls.glconfig.vidHeight - border, cls.glconfig.vidHeight + offset );
	}

	if ( cg_touchscreenControls->integer == TOUCHSCREEN_SHOOT_UNDER_FINGER )
		return;

	// Offset crosshair, so it won't be right under finger
	x = x - offset;
	y = y - offset;

	// Boundary checks
	if ( x < 0 )
		x = 0;
	if ( y < 0 )
		y = 0;
	if ( x >= cls.glconfig.vidWidth )
		x = cls.glconfig.vidWidth - 1;
	if ( y >= cls.glconfig.vidHeight )
		y = cls.glconfig.vidHeight - 1;

	// Return the values
	*dx = x;
	*dy = y;
}

/*
=================
CL_MouseEvent
=================
*/
void CL_MouseEvent( int dx, int dy, int time ) {
	in_mouseX = dx;
	in_mouseY = dy;
	if ( Key_GetCatcher( ) & KEYCATCH_UI ) {
		// Hack to make UI downloaded from random server work - move to upper-left corner, then move to desired position
		VM_Call( uivm, UI_MOUSE_EVENT, -10000, -10000);
		VM_Call( uivm, UI_MOUSE_EVENT, dx * SCREEN_WIDTH / cls.glconfig.vidWidth, dy * SCREEN_HEIGHT / cls.glconfig.vidHeight); 
	} else if( cgvm ) {
		CL_AdjustCrosshairPosNearEdges( &dx, &dy );
		VM_Call( cgvm, CG_MOUSE_EVENT, dx, dy );
	}
}

/*
=================
CL_Mouse2Event
=================
*/
void CL_Mouse2Event( int x, int y, int time ) {
	if ( (cg_touchscreenControls->integer != TOUCHSCREEN_FLOATING_CROSSHAIR) && in_multitouchActive ) {
		int dx = x - in_multitouchX;
		int dy = y - in_multitouchY;
		cl.viewangles[YAW] -= (float)dx * cl_sensitivity->value * cl.cgameSensitivity * 0.05f;
		cl.viewangles[PITCH] += (float)dy * cl_sensitivity->value * cl.cgameSensitivity * 0.05f;
	}
	in_multitouchX = x;
	in_multitouchY = y;
}

void IN_MultitouchDown(void) {
	if ( cg_touchscreenControls->integer == TOUCHSCREEN_FLOATING_CROSSHAIR ) {
		int dx = in_multitouchX - in_mouseX;
		int dy = in_multitouchY - in_mouseY;
		if ( abs( dx ) > abs( dy ) ) {
			in_androidCameraMultitouchYawSpeed = ( dx < 0 ) ? 1 : -1;
		} else {
			Com_QueueEvent( 0, SE_KEY, ( dy < 0 ) ? '/' : K_BACKSPACE , qtrue, 0, NULL );
		}
	} else {
		in_multitouchActive = 1;
	}
}

void IN_MultitouchUp(void) {
	if ( cg_touchscreenControls->integer == TOUCHSCREEN_FLOATING_CROSSHAIR ) {
		in_androidCameraMultitouchYawSpeed = 0;
		Com_QueueEvent( 0, SE_KEY, '/', qfalse, 0, NULL );
		Com_QueueEvent( 0, SE_KEY, K_BACKSPACE, qfalse, 0, NULL );
	} else {
		in_multitouchActive = 0;
	}
}

/*
=================
CL_JoystickEvent

Joystick values stay set until changed
=================
*/
void CL_JoystickEvent( int axis, int value, int time ) {
	if ( axis < 0 || axis >= MAX_JOYSTICK_AXIS ) {
		Com_Error( ERR_DROP, "CL_JoystickEvent: bad axis %i", axis );
	}
	if( in_swapGamepadSticks->integer ) {
		switch ( axis ) {
			case JOY_AXIS_GAMEPADRIGHT_X: axis = JOY_AXIS_GAMEPADLEFT_X; break;
			case JOY_AXIS_GAMEPADRIGHT_Y: axis = JOY_AXIS_GAMEPADLEFT_Y; break;
			case JOY_AXIS_GAMEPADLEFT_X:  axis = JOY_AXIS_GAMEPADRIGHT_X; break;
			case JOY_AXIS_GAMEPADLEFT_Y:  axis = JOY_AXIS_GAMEPADRIGHT_Y; break;
			default: break;
		}
	}
	cl.joystickAxis[axis] = value;
}

void CL_GyroscopeEvent( int axis, int value, int time ) {
	if ( axis < 0 || axis >= 3 ) {
		Com_Error( ERR_DROP, "CL_GyroscopeEvent: bad axis %i", axis );
	}
	cl.gyroscope[axis] += value;
}

void CL_AccelerometerEvent( int axis, int value, int time ) {
	if ( axis < 0 || axis >= 3 ) {
		Com_Error( ERR_DROP, "CL_AccelerometerEvent: bad axis %i", axis );
	}
	if( Key_GetCatcher( ) & ~KEYCATCH_CGAME || clc.state != CA_ACTIVE ) {
		return;
	}
	if ( abs( cl.accelerometer[axis] - value ) > 500 ) { // Accelerometer is noisy, crop the noise
		cl.accelerometerShake += MIN( abs( cl.accelerometer[axis] - value ), 1500 ); // Crop it to prevent random spikes when user drops the phone to the ground
		//Com_Printf("[skipnotify] CL_AccelerometerEvent: axis %6d value old %6d new %6d shake %10d\n", axis, cl.accelerometer[axis], value, cl.accelerometerShake);
	}
	cl.accelerometer[axis] = value;
}

static void CL_ProcessAccelerometer( void ) {
#ifdef USE_VOIP
	cl.accelerometerShake -= cls.unscaledFrametime * cl_voipAccelShakeDecrease->integer;
	if ( cl.accelerometerShake < 0 )
		cl.accelerometerShake = 0;
	if ( in_voiprecord.active ) {
		cl.accelerometerShake = 0;
	} else {
		if ( !cl_voipSend->integer ) {
			if ( cl.accelerometerShake > cl_voipAccelShakeThreshold->integer ) {
				Cvar_Set("cl_voipSend", "1");
				cl.accelerometerShake = cl_voipAccelShakeRecordingTime->integer * cl_voipAccelShakeDecrease->integer;
			}
		} else {
			if ( cl.accelerometerShake <= 0 ) {
				Cvar_Set("cl_voipSend", "0");
			}
			if( cl.accelerometerShake > cl_voipAccelShakeRecordingTime->integer * cl_voipAccelShakeDecrease->integer ) { // Clamp it
				cl.accelerometerShake = cl_voipAccelShakeRecordingTime->integer * cl_voipAccelShakeDecrease->integer;
			}
		}
	}
#endif
}

static void CL_ScaleMovementCmdToMaximiumForStrafeJump( usercmd_t *cmd ) {
	// Normalize them to 127
	if ( abs(cmd->forwardmove) > abs(cmd->rightmove) ) {
		cmd->rightmove = ClampChar( (short)cmd->rightmove * 127 / abs(cmd->forwardmove) );
		cmd->forwardmove = (cmd->forwardmove > 0) ? 127 : -127;
	} else if ( abs(cmd->rightmove) > abs(cmd->forwardmove) ) {
		cmd->forwardmove = ClampChar( (short)cmd->forwardmove * 127 / abs(cmd->rightmove) );
		cmd->rightmove = (cmd->rightmove > 0) ? 127 : -127;
	}
}

/*
=================
CL_JoystickMove
=================
*/
void CL_JoystickMove( usercmd_t *cmd ) {
#ifdef __ANDROID__

	static int oldRightMove = 0, oldForwardMove = 0, oldJump = 0;
	float angle;

	if ( cl.joystickAxis[JOY_AXIS_SCREENJOY_X] == 0 && cl.joystickAxis[JOY_AXIS_SCREENJOY_Y] == 0 ) {
		oldJump = 0;
		if ( in_joystickJumpTriggerTime > 0 ) {
			in_joystickJumpTriggerTime -= cls.unscaledFrametime;
			if ( in_joystickJumpTriggerTime * 2 > j_androidJoystickJumpTime->integer ) {
				cmd->rightmove = ClampChar( cmd->rightmove + oldRightMove );
				cmd->forwardmove = ClampChar( cmd->forwardmove + oldForwardMove );
			}
		}
		if ( abs(cl.joystickAxis[JOY_AXIS_GAMEPADLEFT_X]) > 8192 || abs(cl.joystickAxis[JOY_AXIS_GAMEPADLEFT_Y]) > 8192 ) {
			angle = RAD2DEG( atan2( cl.joystickAxis[JOY_AXIS_GAMEPADLEFT_X], cl.joystickAxis[JOY_AXIS_GAMEPADLEFT_Y] ) );
			if( !in_swipeActivated ) {
				in_swipeAngleRotate = angle + 180.0f;
				if ( in_swipeAngleRotate > 180.0f )
					in_swipeAngleRotate -= 360.0f;
			}
			angle -= 90.0f;
			if ( cl_touchscreenVmCallbacks->integer && (
				 cg_touchscreenControls->integer == TOUCHSCREEN_FLOATING_CROSSHAIR || cg_thirdPerson->integer ) )
				angle += cl.viewangles[YAW] - SHORT2ANGLE( cl.snap.ps.delta_angles[YAW] ) - cl.aimingangles[YAW];
			angle = DEG2RAD( angle );

			cmd->forwardmove = ClampChar( cmd->forwardmove + sin( angle ) * 127.0f );
			cmd->rightmove = ClampChar( cmd->rightmove + cos( angle ) * 127.0f );
			CL_ScaleMovementCmdToMaximiumForStrafeJump( cmd );
		}
	} else {
		if ( in_joystickJumpTriggerTime > 0 && in_joystickJumpTriggerTime < j_androidJoystickJumpTime->integer ) {
			oldJump = 127;
		}
		cmd->upmove = ClampChar( cmd->upmove + oldJump );
		in_joystickJumpTriggerTime = j_androidJoystickJumpTime->integer;

		angle = RAD2DEG( atan2( cl.joystickAxis[JOY_AXIS_SCREENJOY_X], cl.joystickAxis[JOY_AXIS_SCREENJOY_Y] ) );
		if( !in_swipeActivated && (
			cg_touchscreenControls->integer == TOUCHSCREEN_FLOATING_CROSSHAIR ||
			cg_touchscreenControls->integer == TOUCHSCREEN_SHOOT_UNDER_FINGER ) ) {
			in_swipeAngleRotate = angle + 180.0f;
			if ( in_swipeAngleRotate > 180.0f )
				in_swipeAngleRotate -= 360.0f;
		}
		angle -= 90.0f;
		if ( cl_touchscreenVmCallbacks->integer && (
			 cg_touchscreenControls->integer == TOUCHSCREEN_FLOATING_CROSSHAIR || cg_thirdPerson->integer ) )
			angle += cl.viewangles[YAW] - SHORT2ANGLE( cl.snap.ps.delta_angles[YAW] ) - cl.aimingangles[YAW];
		angle = DEG2RAD( angle );

		cmd->forwardmove = ClampChar( cmd->forwardmove + sin( angle ) * 127.0f );
		cmd->rightmove = ClampChar( cmd->rightmove + cos( angle ) * 127.0f );
		CL_ScaleMovementCmdToMaximiumForStrafeJump( cmd );
		oldForwardMove = cmd->forwardmove;
		oldRightMove = cmd->rightmove;
	}

	if ( cl.joystickAxis[JOY_AXIS_GAMEPADLEFT_TRIGGER] > 20000 )
		cmd->upmove = ClampChar( cmd->upmove + 127 );

	if ( cl.cgameUserCmdValue == WP_RAILGUN ) {
		if ( cl.joystickAxis[JOY_AXIS_GAMEPADRIGHT_TRIGGER] > 10000 )
			cmd->buttons |= BUTTON_ATTACK; // Zoom
		if ( cl.joystickAxis[JOY_AXIS_GAMEPADRIGHT_TRIGGER] > 30000 )
			in_attackButtonReleased = 1; // Fire
			
	} else if ( cl.joystickAxis[JOY_AXIS_GAMEPADRIGHT_TRIGGER] > 20000 ) // Fire button pressed
		cmd->buttons |= BUTTON_ATTACK;
#else

	float	anglespeed;

	if ( !(in_speed.active ^ cl_run->integer) ) {
		cmd->buttons |= BUTTON_WALKING;
	}

	if ( in_speed.active ) {
		anglespeed = 0.001 * cls.frametime * cl_anglespeedkey->value;
	} else {
		anglespeed = 0.001 * cls.frametime;
	}

	if ( !in_strafe.active ) {
		cl.viewangles[YAW] += anglespeed * j_yaw->value * cl.joystickAxis[j_yaw_axis->integer];
		cmd->rightmove = ClampChar( cmd->rightmove + (int) (j_side->value * cl.joystickAxis[j_side_axis->integer]) );
	} else {
		cl.viewangles[YAW] += anglespeed * j_side->value * cl.joystickAxis[j_side_axis->integer];
		cmd->rightmove = ClampChar( cmd->rightmove + (int) (j_yaw->value * cl.joystickAxis[j_yaw_axis->integer]) );
	}

	if ( in_mlooking ) {
		cl.viewangles[PITCH] += anglespeed * j_forward->value * cl.joystickAxis[j_forward_axis->integer];
		cmd->forwardmove = ClampChar( cmd->forwardmove + (int) (j_pitch->value * cl.joystickAxis[j_pitch_axis->integer]) );
	} else {
		cl.viewangles[PITCH] += anglespeed * j_pitch->value * cl.joystickAxis[j_pitch_axis->integer];
		cmd->forwardmove = ClampChar( cmd->forwardmove + (int) (j_forward->value * cl.joystickAxis[j_forward_axis->integer]) );
	}

	cmd->upmove = ClampChar( cmd->upmove + (int) (j_up->value * cl.joystickAxis[j_up_axis->integer]) );

#endif
}

/*
=================
CL_MouseMove
=================
*/

void CL_MouseMove(usercmd_t *cmd)
{
	if ( !cgvm )
		return;

	if ( cg_touchscreenControls->integer != TOUCHSCREEN_FLOATING_CROSSHAIR ) {
		static int oldMouseX, oldMouseY;
		int dx = in_mouseX - oldMouseX;
		int dy = in_mouseY - oldMouseY;
		
		if ( in_mouseSwipingActive ) {
			cl.viewangles[YAW] -= (float)dx * cl_sensitivity->value * cl.cgameSensitivity * 0.05f;
			cl.viewangles[PITCH] += (float)dy * cl_sensitivity->value * cl.cgameSensitivity * 0.05f;
		}

		oldMouseX = in_mouseX;
		oldMouseY = in_mouseY;
		// Fade-out the on-screen attack button, until it disappears
		if ( cl.touchscreenAttackButtonPos[4] > 0.0f ) {
			cl.touchscreenAttackButtonPos[4] -= cls.unscaledFrametime * 0.001f;
			if( cl.touchscreenAttackButtonPos[4] < 0.10f )
				cl.touchscreenAttackButtonPos[4] = 0.0f;
		}
		if ( cg_touchscreenControls->integer != TOUCHSCREEN_SHOOT_UNDER_FINGER )
			return;
	}

	if ( ( in_androidCameraYawSpeed || in_androidCameraPitchSpeed || in_androidCameraMultitouchYawSpeed ) && in_buttons[0].active ) {
		float yaw = ( in_androidCameraYawSpeed + in_androidCameraMultitouchYawSpeed ) * cls.unscaledFrametime * 0.15f * cl.cgameSensitivity;
		float pitchSpeed = !cg_thirdPerson->integer ? 0.001f :
							( cl.viewangles[PITCH] < -20 ) ? 0.0015f : ( cl.viewangles[PITCH] < 45 ) ? 0.001f : 0.003f; // More sensitivity near the edges
		float pitch = in_androidCameraPitchSpeed * cls.unscaledFrametime * cl_pitchspeed->value * pitchSpeed * cl.cgameSensitivity;

		cl.viewangles[YAW] += yaw;
		cl.viewangles[PITCH] += pitch;
	}

	if ( cg_touchscreenControls->integer == TOUCHSCREEN_SHOOT_UNDER_FINGER )
		return;

	if ( cl.viewangles[PITCH] != 0 && cl_pitchAutoCenter->integer ) {
		cl.viewangles[PITCH] += j_androidAutoCenterViewSpeed->value * cls.unscaledFrametime * ( ( cl.viewangles[PITCH] > 0 ) ? -1 : 1 );
		if ( fabs( cl.viewangles[PITCH] ) < j_androidAutoCenterViewSpeed->value * cls.unscaledFrametime * 2.0f )
			cl.viewangles[PITCH] = 0;
	}
}

void CL_SetAimingAngles( const vec3_t angles )
{
	VectorCopy( angles, cl.aimingangles );
}

void CL_SetCameraAngles( const vec3_t angles )
{
	//Com_Printf ("CL_SetCameraAngles %f %f -> %f %f\n", cl.viewangles[YAW], cl.viewangles[PITCH], angles[YAW], angles[PITCH]);
	VectorCopy( angles, cl.viewangles );
}

/*
==============
CL_CmdButtons
==============
*/
void CL_CmdButtons( usercmd_t *cmd ) {
	int		i;
	static int zoomDeferred = 0;

	//
	// figure button bits
	// send a button bit even if the key was pressed and released in
	// less than a frame
	//	
	for (i = 0 ; i < 15 ; i++) {
		if ( in_buttons[i].active || in_buttons[i].wasPressed ) {
			cmd->buttons |= 1 << i;
		}
		in_buttons[i].wasPressed = qfalse;
	}
	// Do not zoom out and fire immediately, it will skew aiming, because input events not passed to cgame yet
	if ( zoomDeferred > 0 ) {
		zoomDeferred--;
		if ( !zoomDeferred )
			Cbuf_AddText( "+zoom\n" );
	}
	if ( zoomDeferred < 0 ) {
		zoomDeferred++;
		if ( !zoomDeferred )
			Cbuf_AddText( "-zoom\n" );
	}
	// Auto-zoom for railgun
	if ( cg_railgunAutoZoom->integer ) {
		if ( cl.cgameUserCmdValue == WP_RAILGUN && (cmd->buttons & BUTTON_ATTACK) ) {
			if ( !in_railgunZoomActive &&
				( ( cg_touchscreenControls->integer != TOUCHSCREEN_FLOATING_CROSSHAIR &&
					cg_touchscreenControls->integer != TOUCHSCREEN_SHOOT_UNDER_FINGER ) || // Prevent zooming in if we're rotating view
				! ( in_androidCameraYawSpeed || in_androidCameraPitchSpeed ||
					in_androidCameraMultitouchYawSpeed || in_androidWeaponSelectionBarActive ))) {
				in_railgunZoomActive = qtrue;
				zoomDeferred = 1;
			}
		} else if ( in_railgunZoomActive ) {
			in_railgunZoomActive = qfalse;
			zoomDeferred = -1;
		}
		if ( cl.cgameUserCmdValue == WP_RAILGUN ) {
			if ( cmd->buttons & BUTTON_ATTACK )
				cmd->buttons &= ~BUTTON_ATTACK; // Do not fire immediately when railgun selected, zoom instead
			if ( in_attackButtonReleased )
				cmd->buttons |= BUTTON_ATTACK; // Fire when button is released
		}
		in_attackButtonReleased = 0;
	}

	if ( cg_touchscreenControls->integer == TOUCHSCREEN_FLOATING_CROSSHAIR ||
		 cg_touchscreenControls->integer == TOUCHSCREEN_SHOOT_UNDER_FINGER ) {
		if ( in_androidCameraYawSpeed || in_androidCameraPitchSpeed ||
				in_androidCameraMultitouchYawSpeed || in_androidWeaponSelectionBarActive ||
				in_deferShooting ) {
			cmd->buttons &= ~BUTTON_ATTACK; // Stop firing when we are rotating camera
			in_deferShooting = qfalse;
		}
	}

	if ( Key_GetCatcher( ) ) {
		cmd->buttons |= BUTTON_TALK;
	}

	// allow the game to know if any key at all is
	// currently pressed, even if it isn't bound to anything
	if ( anykeydown && Key_GetCatcher( ) == 0 ) {
		cmd->buttons |= BUTTON_ANY;
	}
}


/*
==============
CL_FinishMove
==============
*/
void CL_FinishMove( usercmd_t *cmd ) {
	int		i;

	// copy the state that the cgame is currently sending
	cmd->weapon = cl.cgameUserCmdValue;

	// send the current server time so the amount of movement
	// can be determined without allowing cheating
	cmd->serverTime = cl.serverTime;

	for (i=0 ; i<3 ; i++) {
		cmd->angles[i] = ANGLE2SHORT(cl.aimingangles[i]);
	}
}


/*
=================
CL_CreateCmd
=================
*/
usercmd_t CL_CreateCmd( void ) {
	usercmd_t	cmd;
	vec3_t		oldAngles;

	VectorCopy( cl.viewangles, oldAngles );

	// keyboard angle adjustment
	CL_AdjustAngles ();
	
	Com_Memset( &cmd, 0, sizeof( cmd ) );

	// get basic movement from keyboard
	CL_KeyMove( &cmd );

	// get basic movement from mouse
	CL_MouseMove( &cmd );

	// get basic movement from joystick
	CL_JoystickMove( &cmd );

	CL_CmdButtons( &cmd );

	CL_ProcessAccelerometer();

	// check to make sure the angles haven't wrapped
	if ( cl.viewangles[PITCH] - oldAngles[PITCH] > 90 ) {
		cl.viewangles[PITCH] = oldAngles[PITCH] + 90;
	} else if ( oldAngles[PITCH] - cl.viewangles[PITCH] > 90 ) {
		cl.viewangles[PITCH] = oldAngles[PITCH] - 90;
	}

	if ( cl.viewangles[YAW] > 180.0f )
		cl.viewangles[YAW] -= 360.0f;
	else if ( cl.viewangles[YAW] < -180.0f )
		cl.viewangles[YAW] += 360.0f;
	if ( cl.viewangles[PITCH] > 180.0f )
		cl.viewangles[PITCH] = 180.0f;
	else if ( cl.viewangles[PITCH] < -180.0f )
		cl.viewangles[PITCH] = -180.0f;
	if ( !cg_thirdPerson->integer ) {
		if (  cl.viewangles[PITCH] < -90.0f )
			cl.viewangles[PITCH] = -90.0f;
		if (  cl.viewangles[PITCH] > 90.0f )
			cl.viewangles[PITCH] = 90.0f;
	}

	if ( ( cg_touchscreenControls->integer == TOUCHSCREEN_FLOATING_CROSSHAIR || cg_thirdPerson->integer ) &&
		 cgvm && cl_touchscreenVmCallbacks->integer ) {
		if ( cg_thirdPerson->integer && cl.viewangles[PITCH] < -90.0f )
			cl.viewangles[PITCH] = -90.0f;
		VM_Call( cgvm, CG_ADJUST_CAMERA_ANGLES, (int) (cl.viewangles[YAW] * 1000), (int) (cl.viewangles[PITCH] * 1000) );
	} else {
		VectorCopy( cl.viewangles, cl.aimingangles );
		cl.aimingangles[PITCH] -= SHORT2ANGLE( cl.snap.ps.delta_angles[PITCH] );
	}

	// store out the final values
	CL_FinishMove( &cmd );

	// draw debug graphs of turning for mouse testing
	if ( cl_debugMove->integer ) {
		if ( cl_debugMove->integer == 1 ) {
			SCR_DebugGraph( abs(cl.viewangles[YAW] - oldAngles[YAW]) );
		}
		if ( cl_debugMove->integer == 2 ) {
			SCR_DebugGraph( abs(cl.viewangles[PITCH] - oldAngles[PITCH]) );
		}
	}

	return cmd;
}


/*
=================
CL_CreateNewCommands

Create a new usercmd_t structure for this frame
=================
*/
void CL_CreateNewCommands( void ) {
	int			cmdNum;

	// no need to create usercmds until we have a gamestate
	if ( clc.state < CA_PRIMED ) {
		return;
	}

	frame_msec = com_frameTime - old_com_frameTime;

	// if running less than 5fps, truncate the extra time to prevent
	// unexpected moves after a hitch
	if ( frame_msec > 200 ) {
		frame_msec = 200;
	}
	old_com_frameTime = com_frameTime;


	// generate a command for this frame
	cl.cmdNumber++;
	cmdNum = cl.cmdNumber & CMD_MASK;
	cl.cmds[cmdNum] = CL_CreateCmd ();
}

/*
=================
CL_ReadyToSendPacket

Returns qfalse if we are over the maxpackets limit
and should choke back the bandwidth a bit by not sending
a packet this frame.  All the commands will still get
delivered in the next packet, but saving a header and
getting more delta compression will reduce total bandwidth.
=================
*/
qboolean CL_ReadyToSendPacket( void ) {
	int		oldPacketNum;
	int		delta;

	// don't send anything if playing back a demo
	if ( clc.demoplaying || clc.state == CA_CINEMATIC ) {
		return qfalse;
	}

	// If we are downloading, we send no less than 50ms between packets
	if ( *clc.downloadTempName &&
		cls.realtime - clc.lastPacketSentTime < 50 ) {
		return qfalse;
	}

	// if we don't have a valid gamestate yet, only send
	// one packet a second
	if ( clc.state != CA_ACTIVE && 
		clc.state != CA_PRIMED && 
		!*clc.downloadTempName &&
		cls.realtime - clc.lastPacketSentTime < 1000 ) {
		return qfalse;
	}

	// send every frame for loopbacks
	if ( clc.netchan.remoteAddress.type == NA_LOOPBACK ) {
		return qtrue;
	}

	// send every frame for LAN
	if ( cl_lanForcePackets->integer && Sys_IsLANAddress( clc.netchan.remoteAddress ) ) {
		return qtrue;
	}

	// check for exceeding cl_maxpackets
	if ( cl_maxpackets->integer < 15 ) {
		Cvar_Set( "cl_maxpackets", "15" );
	} else if ( cl_maxpackets->integer > 125 ) {
		Cvar_Set( "cl_maxpackets", "125" );
	}
	oldPacketNum = (clc.netchan.outgoingSequence - 1) & PACKET_MASK;
	delta = cls.realtime -  cl.outPackets[ oldPacketNum ].p_realtime;
	if ( delta < 1000 / cl_maxpackets->integer ) {
		// the accumulated commands will go out in the next packet
		return qfalse;
	}

	return qtrue;
}

/*
===================
CL_WritePacket

Create and send the command packet to the server
Including both the reliable commands and the usercmds

During normal gameplay, a client packet will contain something like:

4	sequence number
2	qport
4	serverid
4	acknowledged sequence number
4	clc.serverCommandSequence
<optional reliable commands>
1	clc_move or clc_moveNoDelta
1	command count
<count * usercmds>

===================
*/
void CL_WritePacket( void ) {
	msg_t		buf;
	byte		data[MAX_MSGLEN];
	int			i, j;
	usercmd_t	*cmd, *oldcmd;
	usercmd_t	nullcmd;
	int			packetNum;
	int			oldPacketNum;
	int			count, key;

	// don't send anything if playing back a demo
	if ( clc.demoplaying || clc.state == CA_CINEMATIC ) {
		return;
	}

	Com_Memset( &nullcmd, 0, sizeof(nullcmd) );
	oldcmd = &nullcmd;

	MSG_Init( &buf, data, sizeof(data) );

	MSG_Bitstream( &buf );
	// write the current serverId so the server
	// can tell if this is from the current gameState
	MSG_WriteLong( &buf, cl.serverId );

	// write the last message we received, which can
	// be used for delta compression, and is also used
	// to tell if we dropped a gamestate
	MSG_WriteLong( &buf, clc.serverMessageSequence );

	// write the last reliable message we received
	MSG_WriteLong( &buf, clc.serverCommandSequence );

	// write any unacknowledged clientCommands
	for ( i = clc.reliableAcknowledge + 1 ; i <= clc.reliableSequence ; i++ ) {
		MSG_WriteByte( &buf, clc_clientCommand );
		MSG_WriteLong( &buf, i );
		MSG_WriteString( &buf, clc.reliableCommands[ i & (MAX_RELIABLE_COMMANDS-1) ] );
	}

	// we want to send all the usercmds that were generated in the last
	// few packet, so even if a couple packets are dropped in a row,
	// all the cmds will make it to the server
	if ( cl_packetdup->integer < 0 ) {
		Cvar_Set( "cl_packetdup", "0" );
	} else if ( cl_packetdup->integer > 5 ) {
		Cvar_Set( "cl_packetdup", "5" );
	}
	oldPacketNum = (clc.netchan.outgoingSequence - 1 - cl_packetdup->integer) & PACKET_MASK;
	count = cl.cmdNumber - cl.outPackets[ oldPacketNum ].p_cmdNumber;
	if ( count > MAX_PACKET_USERCMDS ) {
		count = MAX_PACKET_USERCMDS;
		Com_Printf("MAX_PACKET_USERCMDS\n");
	}

#ifdef USE_VOIP
	if (clc.voipOutgoingDataSize > 0)
	{
		if((clc.voipFlags & VOIP_SPATIAL) || Com_IsVoipTarget(clc.voipTargets, sizeof(clc.voipTargets), -1))
		{
			MSG_WriteByte (&buf, clc_voip);
			MSG_WriteByte (&buf, clc.voipOutgoingGeneration);
			MSG_WriteLong (&buf, clc.voipOutgoingSequence);
			MSG_WriteByte (&buf, clc.voipOutgoingDataFrames);
			MSG_WriteData (&buf, clc.voipTargets, sizeof(clc.voipTargets));
			MSG_WriteByte(&buf, clc.voipFlags);
			MSG_WriteShort (&buf, clc.voipOutgoingDataSize);
			MSG_WriteData (&buf, clc.voipOutgoingData, clc.voipOutgoingDataSize);

			// If we're recording a demo, we have to fake a server packet with
			//  this VoIP data so it gets to disk; the server doesn't send it
			//  back to us, and we might as well eliminate concerns about dropped
			//  and misordered packets here.
			if(clc.demorecording && !clc.demowaiting)
			{
				const int voipSize = clc.voipOutgoingDataSize;
				msg_t fakemsg;
				byte fakedata[MAX_MSGLEN];
				MSG_Init (&fakemsg, fakedata, sizeof (fakedata));
				MSG_Bitstream (&fakemsg);
				MSG_WriteLong (&fakemsg, clc.reliableAcknowledge);
				MSG_WriteByte (&fakemsg, svc_voip);
				MSG_WriteShort (&fakemsg, clc.clientNum);
				MSG_WriteByte (&fakemsg, clc.voipOutgoingGeneration);
				MSG_WriteLong (&fakemsg, clc.voipOutgoingSequence);
				MSG_WriteByte (&fakemsg, clc.voipOutgoingDataFrames);
				MSG_WriteShort (&fakemsg, clc.voipOutgoingDataSize );
				MSG_WriteBits (&fakemsg, clc.voipFlags, VOIP_FLAGCNT);
				MSG_WriteData (&fakemsg, clc.voipOutgoingData, voipSize);
				MSG_WriteByte (&fakemsg, svc_EOF);
				CL_WriteDemoMessage (&fakemsg, 0);
			}

			clc.voipOutgoingSequence += clc.voipOutgoingDataFrames;
			clc.voipOutgoingDataSize = 0;
			clc.voipOutgoingDataFrames = 0;
		}
		else
		{
			// We have data, but no targets. Silently discard all data
			clc.voipOutgoingDataSize = 0;
			clc.voipOutgoingDataFrames = 0;
		}
	}
#endif

	if ( count >= 1 ) {
		if ( cl_showSend->integer ) {
			Com_Printf( "(%i)", count );
		}

		// begin a client move command
		if ( cl_nodelta->integer || !cl.snap.valid || clc.demowaiting
			|| clc.serverMessageSequence != cl.snap.messageNum ) {
			MSG_WriteByte (&buf, clc_moveNoDelta);
		} else {
			MSG_WriteByte (&buf, clc_move);
		}

		// write the command count
		MSG_WriteByte( &buf, count );

		// use the checksum feed in the key
		key = clc.checksumFeed;
		// also use the message acknowledge
		key ^= clc.serverMessageSequence;
		// also use the last acknowledged server command in the key
		key ^= MSG_HashKey(clc.serverCommands[ clc.serverCommandSequence & (MAX_RELIABLE_COMMANDS-1) ], 32);

		// write all the commands, including the predicted command
		for ( i = 0 ; i < count ; i++ ) {
			j = (cl.cmdNumber - count + i + 1) & CMD_MASK;
			cmd = &cl.cmds[j];
			MSG_WriteDeltaUsercmdKey (&buf, key, oldcmd, cmd);
			oldcmd = cmd;
		}
	}

	//
	// deliver the message
	//
	packetNum = clc.netchan.outgoingSequence & PACKET_MASK;
	cl.outPackets[ packetNum ].p_realtime = cls.realtime;
	cl.outPackets[ packetNum ].p_serverTime = oldcmd->serverTime;
	cl.outPackets[ packetNum ].p_cmdNumber = cl.cmdNumber;
	clc.lastPacketSentTime = cls.realtime;

	if ( cl_showSend->integer ) {
		Com_Printf( "%i ", buf.cursize );
	}

	CL_Netchan_Transmit (&clc.netchan, &buf);	
}

/*
=================
CL_SendCmd

Called every frame to builds and sends a command packet to the server.
=================
*/
void CL_SendCmd( void ) {
	// don't send any message if not connected
	if ( clc.state < CA_CONNECTED ) {
		return;
	}

	// don't send commands if paused
	if ( com_sv_running->integer && sv_paused->integer && cl_paused->integer ) {
		return;
	}

	// we create commands even if a demo is playing,
	CL_CreateNewCommands();

	// don't send a packet if the last packet was sent too recently
	if ( !CL_ReadyToSendPacket() ) {
		if ( cl_showSend->integer ) {
			Com_Printf( ". " );
		}
		return;
	}

	CL_WritePacket();
}

/*
============
CL_InitInput
============
*/
void CL_InitInput( void ) {
	Cmd_AddCommand ("+centerview",IN_CenterViewDown);
	Cmd_AddCommand ("-centerview",IN_CenterViewUp);
	Cmd_AddCommand ("+moveup",IN_UpDown);
	Cmd_AddCommand ("-moveup",IN_UpUp);
	Cmd_AddCommand ("+movedown",IN_DownDown);
	Cmd_AddCommand ("-movedown",IN_DownUp);
	Cmd_AddCommand ("+left",IN_LeftDown);
	Cmd_AddCommand ("-left",IN_LeftUp);
	Cmd_AddCommand ("+right",IN_RightDown);
	Cmd_AddCommand ("-right",IN_RightUp);
	Cmd_AddCommand ("+forward",IN_ForwardDown);
	Cmd_AddCommand ("-forward",IN_ForwardUp);
	Cmd_AddCommand ("+back",IN_BackDown);
	Cmd_AddCommand ("-back",IN_BackUp);
	Cmd_AddCommand ("+lookup", IN_LookupDown);
	Cmd_AddCommand ("-lookup", IN_LookupUp);
	Cmd_AddCommand ("+lookdown", IN_LookdownDown);
	Cmd_AddCommand ("-lookdown", IN_LookdownUp);
	Cmd_AddCommand ("+strafe", IN_StrafeDown);
	Cmd_AddCommand ("-strafe", IN_StrafeUp);
	Cmd_AddCommand ("+moveleft", IN_MoveleftDown);
	Cmd_AddCommand ("-moveleft", IN_MoveleftUp);
	Cmd_AddCommand ("+moveright", IN_MoverightDown);
	Cmd_AddCommand ("-moveright", IN_MoverightUp);
	Cmd_AddCommand ("+speed", IN_SpeedDown);
	Cmd_AddCommand ("-speed", IN_SpeedUp);
	Cmd_AddCommand ("+attack", IN_Button0Down);
	Cmd_AddCommand ("-attack", IN_Button0Up);
	Cmd_AddCommand ("+button0", IN_Button0Down);
	Cmd_AddCommand ("-button0", IN_Button0Up);
	Cmd_AddCommand ("+button1", IN_Button1Down);
	Cmd_AddCommand ("-button1", IN_Button1Up);
	Cmd_AddCommand ("+button2", IN_Button2Down);
	Cmd_AddCommand ("-button2", IN_Button2Up);
	Cmd_AddCommand ("+button3", IN_Button3Down);
	Cmd_AddCommand ("-button3", IN_Button3Up);
	Cmd_AddCommand ("+button4", IN_Button4Down);
	Cmd_AddCommand ("-button4", IN_Button4Up);
	Cmd_AddCommand ("+button5", IN_Button5Down);
	Cmd_AddCommand ("-button5", IN_Button5Up);
	Cmd_AddCommand ("+button6", IN_Button6Down);
	Cmd_AddCommand ("-button6", IN_Button6Up);
	Cmd_AddCommand ("+button7", IN_Button7Down);
	Cmd_AddCommand ("-button7", IN_Button7Up);
	Cmd_AddCommand ("+button8", IN_Button8Down);
	Cmd_AddCommand ("-button8", IN_Button8Up);
	Cmd_AddCommand ("+button9", IN_Button9Down);
	Cmd_AddCommand ("-button9", IN_Button9Up);
	Cmd_AddCommand ("+button10", IN_Button10Down);
	Cmd_AddCommand ("-button10", IN_Button10Up);
	Cmd_AddCommand ("+button11", IN_Button11Down);
	Cmd_AddCommand ("-button11", IN_Button11Up);
	Cmd_AddCommand ("+button12", IN_Button12Down);
	Cmd_AddCommand ("-button12", IN_Button12Up);
	Cmd_AddCommand ("+button13", IN_Button13Down);
	Cmd_AddCommand ("-button13", IN_Button13Up);
	Cmd_AddCommand ("+button14", IN_Button14Down);
	Cmd_AddCommand ("-button14", IN_Button14Up);
	Cmd_AddCommand ("+mlook", IN_MLookDown);
	Cmd_AddCommand ("-mlook", IN_MLookUp);
	Cmd_AddCommand ("+multitouch", IN_MultitouchDown);
	Cmd_AddCommand ("-multitouch", IN_MultitouchUp);
	Cmd_AddCommand ("gesture", IN_Gesture);

#ifdef USE_VOIP
	Cmd_AddCommand ("+voiprecord", IN_VoipRecordDown);
	Cmd_AddCommand ("-voiprecord", IN_VoipRecordUp);
#endif

	cl_nodelta = Cvar_Get ("cl_nodelta", "0", 0);
	cl_debugMove = Cvar_Get ("cl_debugMove", "0", 0);
}

/*
============
CL_ShutdownInput
============
*/
void CL_ShutdownInput(void)
{
	Cmd_RemoveCommand("+centerview");
	Cmd_RemoveCommand("-centerview");
	Cmd_RemoveCommand("+moveup");
	Cmd_RemoveCommand("-moveup");
	Cmd_RemoveCommand("+movedown");
	Cmd_RemoveCommand("-movedown");
	Cmd_RemoveCommand("+left");
	Cmd_RemoveCommand("-left");
	Cmd_RemoveCommand("+right");
	Cmd_RemoveCommand("-right");
	Cmd_RemoveCommand("+forward");
	Cmd_RemoveCommand("-forward");
	Cmd_RemoveCommand("+back");
	Cmd_RemoveCommand("-back");
	Cmd_RemoveCommand("+lookup");
	Cmd_RemoveCommand("-lookup");
	Cmd_RemoveCommand("+lookdown");
	Cmd_RemoveCommand("-lookdown");
	Cmd_RemoveCommand("+strafe");
	Cmd_RemoveCommand("-strafe");
	Cmd_RemoveCommand("+moveleft");
	Cmd_RemoveCommand("-moveleft");
	Cmd_RemoveCommand("+moveright");
	Cmd_RemoveCommand("-moveright");
	Cmd_RemoveCommand("+speed");
	Cmd_RemoveCommand("-speed");
	Cmd_RemoveCommand("+attack");
	Cmd_RemoveCommand("-attack");
	Cmd_RemoveCommand("+button0");
	Cmd_RemoveCommand("-button0");
	Cmd_RemoveCommand("+button1");
	Cmd_RemoveCommand("-button1");
	Cmd_RemoveCommand("+button2");
	Cmd_RemoveCommand("-button2");
	Cmd_RemoveCommand("+button3");
	Cmd_RemoveCommand("-button3");
	Cmd_RemoveCommand("+button4");
	Cmd_RemoveCommand("-button4");
	Cmd_RemoveCommand("+button5");
	Cmd_RemoveCommand("-button5");
	Cmd_RemoveCommand("+button6");
	Cmd_RemoveCommand("-button6");
	Cmd_RemoveCommand("+button7");
	Cmd_RemoveCommand("-button7");
	Cmd_RemoveCommand("+button8");
	Cmd_RemoveCommand("-button8");
	Cmd_RemoveCommand("+button9");
	Cmd_RemoveCommand("-button9");
	Cmd_RemoveCommand("+button10");
	Cmd_RemoveCommand("-button10");
	Cmd_RemoveCommand("+button11");
	Cmd_RemoveCommand("-button11");
	Cmd_RemoveCommand("+button12");
	Cmd_RemoveCommand("-button12");
	Cmd_RemoveCommand("+button13");
	Cmd_RemoveCommand("-button13");
	Cmd_RemoveCommand("+button14");
	Cmd_RemoveCommand("-button14");
	Cmd_RemoveCommand("+mlook");
	Cmd_RemoveCommand("-mlook");
	Cmd_RemoveCommand ("+multitouch");
	Cmd_RemoveCommand ("-multitouch");
	Cmd_RemoveCommand ("gesture");

	Cmd_AddCommand ("gesture", IN_Gesture);

#ifdef USE_VOIP
	Cmd_RemoveCommand("+voiprecord");
	Cmd_RemoveCommand("-voiprecord");
#endif
}
