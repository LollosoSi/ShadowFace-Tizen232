#include <tizen.h>
#include "shadowface-tizen232.h"

#include <device/battery.h>
#include <device/callback.h>

#include <efl_extension.h>

#include <player.h>
#include <app_common.h>
//#include <app_resource_manager.h>

#define PI 3.14159265
double
to_degrees (double radians)
{
  return radians * (180.0 / PI);
}

double
to_radians (double degrees)
{
  return degrees * (PI / 180.0);
}

typedef struct appdata
{
  Evas_Object *win;
  Evas_Object *conform;

  player_h *playerTick1;
  player_h *playerTick2;

  Evas_Object *hand_hour;
  Evas_Object *hand_minute;
  Evas_Object *hand_minute_battery;
  Evas_Object *hand_second;
  Evas_Object *circle_obj;
  Evas_Object *ticks[60];
  Evas_Object *numTicks[12];
} appdata_s;

short window_center_x, window_center_y, window_width, window_height;

bool playerTickSwap = 0;

short GeneralGap;
short hourCenterDistance, secondCenterDistance, hand_hour_length,
    hand_minute_length, hand_second_length;
short borderCommonDistance;

// Uncomment this line for a "picture ready" static watchface.
// #define picture_mode

// Pick one
#define pack1
//#define pack2

#ifdef pack1
const char** tick1 = "tick1.ogg";
const char** tick2 = "tick2.ogg";
const float tickVolume = 0.3;
#endif

#ifdef pack2
const char** tick1 = "tick1set2.ogg";
const char** tick2 = "tick2set2.ogg";
const float tickVolume = 0.15;
#endif

bool enable_antialiasing = true;

/* Ticks */
int charsizeX = 27, charsizeY = 20, borderDistance = 26, tickBorderDistance = 2;
bool showingMinutes[12];
char ticktextbuf[30];
uint8_t shownElementsPerSide = 6; // 6 is the minimum value for visibility of both current/next digit


short shadow_coeff;
uint8_t shrink_coeff = 3;
uint8_t daily_border_distance; // Runtime variable
uint8_t day_of_week = 0;

int error_code;

/* 		Colors						  R   G	  B 	*/
//uint8_t tickColor[3] = 				{255,255,255}; 	/* White */
//uint8_t tickSecondColor[3] = 		{235,88,84};   	/* Somewhat red */
//uint8_t tickNumberSecondColor[3] = 	{255,156,98};	/* Somewhat brighter red */
//uint8_t tickMinuteColor[3] =		{131,241,124}; 	/* Probably green */
//uint8_t tickNumberMinuteColor[3] = 	{237,251,66}; 	/* Bright readable yellow */
//uint8_t tickHourColor[3] = 			{153,168,200}; 	/* What's this */
//uint8_t tickNumberHourColor[3] = 	{255,255,255}; 	/* White */

/* 		Colors
 * 0-1 AOD
 *
 * 0-7
   0 - Tick color
   1 - Second Hand Color
   2 - Number Color for second hand
   3 - Minute Hand Color
   4 - Number Color for minute hand
   5 - Hour Hand Color
   6 - Number Color for hour hand

   0-2 RGB
 */
#define total_available_colors 7 // Update if more colors are added
uint8_t colors[2][7][3];
uint8_t resultColor[3];
uint8_t aod_dim_intensity = 0; // Disabled ATM: Requires testing
//uint8_t resultNumberColor[3];

/* rotates an EVAS object */
static void
view_rotate_object (Evas_Object *object_to_rotate, double degree, Evas_Coord cx,
		    Evas_Coord cy)
{
  Evas_Map *m = NULL;

  if (object_to_rotate == NULL)
    {
      dlog_print (DLOG_ERROR, LOG_TAG, "object  is NULL");
      return;
    }

  m = evas_map_new (4);
  evas_map_util_points_populate_from_object (m, object_to_rotate);
  evas_map_util_rotate (m, degree, cx, cy);
  evas_object_map_set (object_to_rotate, m);
  evas_object_map_enable_set (object_to_rotate, EINA_TRUE);
  evas_map_free (m);
}

void
setTimeAngles (int *hour, int *minute, int *second, unsigned short *hourAngle,
	       unsigned short *minuteAngle, unsigned short *secondAngle) {

  *secondAngle = (unsigned short)(*second * 6.0)/*+(addMillis*0.000016)*/;
  *minuteAngle = (unsigned short)(*minute * 6.0) + (*secondAngle * 0.016666);
  *hourAngle = (unsigned short)(*hour * 30.0) + (*minuteAngle * 0.0833);

}

void
setTimeAnglesNoSecond (int *hour, int *minute, unsigned short *hourAngle,
	       unsigned short *minuteAngle) {

  *minuteAngle = (unsigned short)(*minute * 6.0);
  *hourAngle = (unsigned short)(*hour * 30.0) + (*minuteAngle * 0.0833);

}

int
distance (int alpha, int beta)
{
  uint8_t dst = abs (alpha - beta);
  return MIN(dst, 60 - dst);
}

int
cyclicity (int val, int cycl)
{
  return val > cycl ? val % cycl : val < 0 ? cycl + val : val;
}

/* interpolates between 2 colors in the form of array[3] - percent to array2[3] */
void
gradiate (uint8_t result_rgb[], uint8_t first_color[], uint8_t second_color[],
	  double percent)
{

  double resultRed = first_color[0] + percent * (second_color[0] - first_color[0]);
  double resultGreen = first_color[1] + percent * (second_color[1] - first_color[1]);
  double resultBlue = first_color[2] + percent * (second_color[2] - first_color[2]);

  result_rgb[0] = resultRed;
  result_rgb[1] = resultGreen;
  result_rgb[2] = resultBlue;

}

/* Refer to colors[] declaration for what colorpos means */
void
assign_color_array (uint8_t aod_int, uint8_t colorpos, uint8_t R, uint8_t G, uint8_t B)
{
  colors[aod_int][colorpos][0] = R;
  colors[aod_int][colorpos][1] = G;
  colors[aod_int][colorpos][2] = B;
}

uint8_t positive_subtract(uint8_t value, uint8_t sub_value){
  if(value <= sub_value)
      return 0;
  else
    return value-sub_value;
}

void
dim_color_arrays(){

  for(int i = 0; i < total_available_colors; i++){
      assign_color_array(1, i,
			 positive_subtract(colors[0][i][0], aod_dim_intensity),
			 positive_subtract(colors[0][i][1], aod_dim_intensity),
			 positive_subtract(colors[0][i][2], aod_dim_intensity)
			 );
  }

}

void
arraycpy (uint8_t dest[], uint8_t source[])
{
  dest[0] = source[0];
  dest[1] = source[1];
  dest[2] = source[2];
}
/* THE FOLLOWING IS KEPT FOR REFERENCE */
//watch_time_h lastWatchTime;

//bool ambientSwitching = false;
//static void
//update_watch (appdata_s *ad, watch_time_h watch_time, int ambient, int addmillis)
//{
//
//  error_code = player_stop (ad->playerTick1);
//  if (error_code != PLAYER_ERROR_NONE)
//    dlog_print (DLOG_ERROR, LOG_TAG, "failed to stop player: error code = %d",
//		error_code);
//  error_code = player_stop (ad->playerTick2);
//  if (error_code != PLAYER_ERROR_NONE)
//    dlog_print (DLOG_ERROR, LOG_TAG, "failed to stop player: error code = %d",
//		error_code);
//
//  int hour, minute, second;
//
// // if(watch_time==NULL)
/////    watch_time = lastWatchTime;
//
//  if (watch_time != NULL) {
//     // lastWatchTime = watch_time;
//
//
//      watch_time_get_hour (watch_time, &hour);
//      watch_time_get_minute (watch_time, &minute);
//      watch_time_get_second (watch_time, &second);
//
//      float angle_hour, angle_minute, angle_second;
//      setTimeAngles (&hour, &minute, &second, &angle_hour, &angle_minute,
//		     &angle_second, addmillis);
//
//      short distanceZoneHour = angle_hour * 0.167f, distanceZoneMinute =
//	  angle_minute * 0.167f, distanceZoneSecond =
//	  ambient ? 0 : angle_second * 0.167f;
//
//      short distSec = 999, distMin, distHr, shortestDistance;
//
//      short opresult = (distanceZoneMinute % 5);
//
//      int closestTickMinute =
//	  opresult > 2 ?
//	      distanceZoneMinute + (5 - opresult) :
//	      distanceZoneMinute - opresult;
//
//      /*uint8_t range1sec = !ambient ? cyclicity(distanceZoneSecond - shownElementsPerSide,60) : 0,
//       range2sec = !ambient ? cyclicity(distanceZoneSecond + shownElementsPerSide, 60) : 0,
//       range1min = cyclicity(distanceZoneMinute - shownElementsPerSide, 60),
//       range2min = cyclicity(distanceZoneMinute + shownElementsPerSide,60),
//       range1hr = cyclicity(distanceZoneHour - shownElementsPerSide, 60),
//       range2hr = cyclicity(distanceZoneHour + shownElementsPerSide, 60);*/
//
//      int winningdist;
//      for (int i = 0; i < 60; i++) {
//	  distHr = distance (distanceZoneHour, i);
//	  distMin = distance (distanceZoneMinute, i);
//	  if (!ambient)
//	    distSec = distance (distanceZoneSecond, i);
//	  else
//	    distSec = 999;
//
//	  if (ambientSwitching || distHr <= shownElementsPerSide
//	      || distMin <= shownElementsPerSide
//	      || distSec <= shownElementsPerSide)
//	    {
//
//	      /*uint8_t distHrMin = distance (distanceZoneHour,
//					    distanceZoneMinute);
//	      uint8_t distHrSec = distance (distanceZoneHour,
//					    distanceZoneSecond);
//	      uint8_t distMinSec = distance (distanceZoneMinute,
//					     distanceZoneSecond);
//
//*/
//	      winningdist = MIN(ambient ? distMin : MIN(distSec, distMin),
//				distHr);
//	      shortestDistance = shownElementsPerSide
//		  - (winningdist == distSec ? 1 : 0) - winningdist;
//	      if (shortestDistance < 0)
//		shortestDistance = 0;
//	      else
//		shortestDistance *= shadow_coeff;
//	      if (shortestDistance < 100)
//		shortestDistance = 0;
//
//
//	      uint8_t resultingColor[3];
//	      uint8_t resultingNumberColor[3];
//
//	      /*if(distHrMin < (shownElementsPerSide) && (winningdist == distMin || winningdist == distHr)){
//	       int dist = abs(distHrMin-distMin);
//	       if(dist==0) dist=distHrMin;
//	       float percA = 1.0-(dist/distHrMin);
//	       gradiate(resultingColor, tickHourColor, tickMinuteColor, percA);
//	       gradiate(resultingNumberColor, tickNumberHourColor, tickNumberMinuteColor, percA);
//
//	       }else if(distMinSec < (shownElementsPerSide) && (winningdist == distMin || winningdist == distSec)){
//	       int dist = abs(distMinSec-distMin);
//	       if(dist==0) dist=distMinSec;
//	       float percA = 1.0-(dist/distMinSec);
//	       gradiate(resultingColor, tickMinuteColor, tickSecondColor, percA);
//	       gradiate(resultingNumberColor, tickNumberMinuteColor, tickNumberSecondColor, percA);
//
//
//	       }else if(distHrSec < (shownElementsPerSide) && (winningdist == distHr || winningdist == distSec) ){
//	       int dist = abs(distHrSec-distHr);
//	       if(dist==0) dist=distHrSec;
//	       float percA = 1.0-(dist/distHrSec);
//	       gradiate(resultingColor, tickHourColor, tickSecondColor, percA);
//	       gradiate(resultingNumberColor, tickNumberHourColor, tickNumberSecondColor, percA);
//
//	       }
//	       */
//
//	      if (winningdist == distSec)
//		{
//
//		  arraycpy (resultingColor, tickSecondColor);
//		  arraycpy (resultingNumberColor, tickNumberSecondColor);
//
//		  if (i % 5 == 0 && !showingMinutes[i / 5])
//		    {
//		      char watch_text_buf[50];
//		      snprintf (watch_text_buf, 50,
//				"<font_size=27><align=left>%2d</align></font>",
//				i == 0 ? 12 : i);
//		      elm_object_text_set(ad->numTicks[i / 5], watch_text_buf);
//		      showingMinutes[i / 5] = 1;
//		    }
//
//		}
//	      else if (winningdist == distMin)
//		{
//
//		  if (ambient)
//		    {
//		      arraycpy (resultingColor, tickColor);
//		    }
//		  else
//		    {
//		      arraycpy (resultingColor, tickMinuteColor);
//		    }
//
//		  arraycpy (resultingNumberColor, tickNumberMinuteColor);
//		  if (i == closestTickMinute && i != 0)
//		    {
//		      if (!showingMinutes[i / 5])
//			{
//			  char watch_text_buf[50];
//			  snprintf (
//			      watch_text_buf, 50,
//			      "<font_size=27><align=left>%2d</align></font>",
//			      i);
//			  elm_object_text_set(ad->numTicks[i / 5],
//					      watch_text_buf);
//			  showingMinutes[i / 5] = 1;
//			}
//		    }
//		}
//	      else if (winningdist == distHr)
//		{
//
//		  arraycpy (resultingColor, tickHourColor);
//		  arraycpy (resultingNumberColor, tickNumberHourColor);
//
//		  if (i % 5 == 0 && showingMinutes[i / 5])
//		    {
//		      char watch_text_buf[50];
//		      snprintf (watch_text_buf, 50,
//				"<font_size=27><align=left>%2d</align></font>",
//				i == 0 ? 12 : i / 5);
//		      elm_object_text_set(ad->numTicks[i / 5], watch_text_buf);
//		      showingMinutes[i / 5] = 0;
//		    }
//		}
//
//	      if (i % 5 == 0)
//		evas_object_color_set (ad->numTicks[i / 5],
//				       resultingNumberColor[0],
//				       resultingNumberColor[1],
//				       resultingNumberColor[2],
//				       shortestDistance);
//	      evas_object_color_set (ad->ticks[i], resultingColor[0],
//				     resultingColor[1], resultingColor[2],
//				     shortestDistance);
//
//	    }
//	}
//      if (ambientSwitching)
//	ambientSwitching = false;
//
//      /*
//       free(&shortestDistance); free(&winningdist);
//       free(&range1sec); free(&range2sec);
//       free(&range1min); free(&range2min);
//       free(&range1hr); free(&range2hr);
//       free(&closestTickMinute);*/
//
//      view_rotate_object (ad->hand_hour, angle_hour, window_center_x,
//			  window_center_y);
//      view_rotate_object (ad->hand_minute, angle_minute, window_center_x,
//			  window_center_y);
//
//      int batlev;
//      device_battery_get_percent (&batlev);
//      short subtract_min_bat_length = (short) (hand_minute_length
//	  * (1.0f - (batlev / 100.0f)));
//      evas_object_resize (ad->hand_minute_battery, 4,
//			  hand_minute_length - subtract_min_bat_length);
//      evas_object_move (ad->hand_minute_battery, window_center_x - 2,
//			GeneralGap + subtract_min_bat_length);
//
//      view_rotate_object (ad->hand_minute_battery, angle_minute, window_center_x,
//			  window_center_y);
//
//      if (!ambient)
//	view_rotate_object (ad->hand_second, angle_second, window_center_x,
//			    window_center_y);
//
//      if (playerTickSwap)
//	{
//	  error_code = player_start (ad->playerTick1);
//	}
//      else
//	{
//	  error_code = player_start (ad->playerTick2);
//	}
//      playerTickSwap = !playerTickSwap;
//
//
//    }
//}

/* this function might look identical to create_gui. IT IS NOT */
void reset_elements_settings(appdata_s *ad){


    daily_border_distance = shrink_coeff*(day_of_week-1);

    /* Tick & number repositioning */
    for (int i = 0; i < 60; i++) {
        short angle = (i / 0.167f) - 90;
        if (angle < 0) {
  	  angle += 360;
  	}

        /* If it's a number (includes different tick size) */
        if (i % 5 == 0) {

  	  /* Number x y positioning values */
  	  double xcoeff = (i <= 15 || i > 45 ? (i > 45 ? (i - 45) : (i + 15)) : (45 - i)) / 30.0;
  	  double ycoeff = (i <= 30 ? i : 60 - i) / 30.0;

  	  evas_object_size_hint_weight_set (ad->numTicks[i / 5], EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  	  evas_object_resize (ad->numTicks[i / 5], charsizeX, charsizeY);
  	  evas_object_move (ad->numTicks[i / 5],
  	      window_center_x
  	      + ((window_center_x - borderDistance - tickBorderDistance - daily_border_distance) * cos (to_radians (angle)))
  		  - ((i < 10 ? charsizeX / 2.0 : charsizeX) * (xcoeff)),
  	      window_center_y
  		  + ((window_center_y - borderDistance - tickBorderDistance - daily_border_distance) * sin (to_radians (angle))) - (charsizeY * (ycoeff)));

  	  evas_object_anti_alias_set (ad->ticks[i / 5], enable_antialiasing);

  	  evas_object_resize (ad->ticks[i], 2, 16);
  	  evas_object_move (ad->ticks[i], (window_width / 2) - 1, tickBorderDistance + daily_border_distance);

  	  view_rotate_object (ad->ticks[i], angle + 90, window_center_x,
  			      window_center_y);

  	} else {
  	  evas_object_resize (ad->ticks[i], 2, 10);
  	  evas_object_move (ad->ticks[i], (window_width / 2) - 1, tickBorderDistance + daily_border_distance);
  	  view_rotate_object (ad->ticks[i], angle + 90, window_center_x,  window_center_y);
  	}
        evas_object_anti_alias_set (ad->ticks[i], enable_antialiasing);
      }

    /* Hand Hour */
    evas_object_resize (ad->hand_hour, 4, hand_hour_length);
    evas_object_move (ad->hand_hour, (window_width / 2) - 2, GeneralGap + daily_border_distance);
    evas_object_anti_alias_set (ad->hand_hour, enable_antialiasing);

    /* Hand Minute */
    evas_object_resize (ad->hand_minute, 4, hand_minute_length);
    evas_object_move (ad->hand_minute, (window_width / 2) - 2, GeneralGap + daily_border_distance);
    evas_object_anti_alias_set (ad->hand_minute, enable_antialiasing);

    /* Hand Minute Battery */
    evas_object_resize (ad->hand_minute_battery, 4, hand_minute_length);
    evas_object_move (ad->hand_minute_battery, (window_width / 2) - 2, GeneralGap + daily_border_distance);
    evas_object_anti_alias_set (ad->hand_minute_battery, enable_antialiasing);

    /* Circle Hand Seconds */
    evas_object_move (ad->hand_second, (window_width / 2) - 9, GeneralGap + daily_border_distance);
    evas_object_resize (ad->hand_second, 18, 18);
    evas_object_anti_alias_set (ad->hand_second, enable_antialiasing);

}


bool needFullLoop = false;
void active_tick(appdata_s *ad, watch_time_h watch_time){
  int hour, minute, second;

  if (watch_time != NULL) {
      watch_time_get_hour (watch_time, &hour);
      watch_time_get_minute (watch_time, &minute);
      watch_time_get_second (watch_time, &second);

      #ifdef picture_mode
	hour = 10;minute = 10;second = 35;
      #endif
      /* Angles for rotating the hands later on */
      /*float angle_hour, angle_minute, angle_second;
      setTimeAngles (&hour, &minute, &second, &angle_hour, &angle_minute, &angle_second);*/
      /* Converted to unsigned short for simpler calcs
       * 1 degree precision is more than sufficient
       * Might want to switch back to float when I figure out how to update more times in a second
       */
      unsigned short angle_hour, angle_minute, angle_second;
      setTimeAngles (&hour, &minute, &second, &angle_hour, &angle_minute, &angle_second);

      /* Very high precision but it's not really needed atm */
      /*short distanceZoneHour = angle_hour * 0.167f, distanceZoneMinute =
	  angle_minute * 0.167f, distanceZoneSecond =
	  ambient ? 0 : angle_second * 0.167f;*/

      /* Using simpler types because we want to save battery */
      uint8_t distanceZoneHour = (angle_hour/6),
	      distanceZoneMinute = minute,
	      distanceZoneSecond = second;

      uint8_t distSec = 250,
	      distMin,
	      distHr,
	      winningdist;
      short shortestDistance;

      short opresult = distanceZoneMinute%5;
      int closestTickMinute = opresult > 2 ? distanceZoneMinute + (5 - opresult) : distanceZoneMinute - opresult;

      short alphaValue = 0;
      /* Tick - number update loop
       * please note that this is looping for 60 positions, many are going to be immediately skipped.
       * best case scenario will be shownElementsPerSide*2 / 60 updates
       * A more elaborate range to avoid considering each tick might be possible
       * */
      for (int i = 0; i < 60; i++) {
	  short numberPointerDyn = i/5;
	  bool isMinuteNumber = i % 5 == 0;

	  distHr = distance (distanceZoneHour, i);
	  distMin = distance (distanceZoneMinute, i);
	  distSec = distance (distanceZoneSecond, i);


	  /* Range check first. Might want to update all after switching to/from AOD */
	  if (needFullLoop || distHr <= shownElementsPerSide || distMin <= shownElementsPerSide || distSec <= shownElementsPerSide){

	      winningdist = MIN(MIN(distSec, distMin), distHr);

	      /* Find out which one is closer */
	      bool shortestIsHour, shortestIsMinute, shortestIsSecond;
	      shortestIsHour = winningdist==distHr;
	      shortestIsMinute = !shortestIsHour ? winningdist==distMin : 0;
	      shortestIsSecond = !shortestIsHour && !shortestIsMinute;

	      /* Find the alpha value */
	      shortestDistance = shownElementsPerSide - (shortestIsSecond ? 1 : 0) - winningdist;
	      if (shortestDistance < 0)
		alphaValue = 0;
	      else{
		  /* any response can be implemented here.
		   * find the curve you like online then implement it */
		alphaValue = (shortestDistance * shadow_coeff);
		//if (shortestDistance < 100)
		//  alphaValue = 0;
	      }

	      if (alphaValue == 0)
		{
		  if (isMinuteNumber)
		    evas_object_hide (ad->numTicks[numberPointerDyn]);

		  evas_object_hide (ad->ticks[i]);

		}
	      else
		{

		  /* Set numbers if needed */
		  if (shortestIsMinute || shortestIsSecond) {
		      if (isMinuteNumber)
			if (!showingMinutes[numberPointerDyn]) {
			    char watch_text_buf[50];
			    snprintf (watch_text_buf, 50, "<font_size=27><align=left>%2d</align></font>", i == 0 && (minute > shownElementsPerSide)  ? 60 : i);
			    elm_object_text_set(ad->numTicks[numberPointerDyn], watch_text_buf);
			    showingMinutes[numberPointerDyn] = 1;
			  }

		    } else if (shortestIsHour)
		    if (isMinuteNumber && showingMinutes[numberPointerDyn]) {
			char watch_text_buf[50];
			snprintf (watch_text_buf, 50, "<font_size=27><align=left>%2d</align></font>", i == 0 ? 12 : numberPointerDyn);
			elm_object_text_set(ad->numTicks[numberPointerDyn], watch_text_buf);
			showingMinutes[numberPointerDyn] = false;
		      }

		  /* Finding which color should be picked in the color array */
		  uint8_t arraypos = shortestIsHour ? 5 : shortestIsMinute ? 3 : 0,
		      numarraypos = (arraypos == 0 ? 0 :  arraypos == 3 && closestTickMinute != i ?  0 : arraypos + 1);

		  /* Calculate gradient for conflicting hands */
		  bool useInterpolatedColor = true;
		  if((distance (distanceZoneHour, distanceZoneMinute) < shownElementsPerSide) && (shortestIsHour||shortestIsMinute)){
		      gradiate(resultColor,colors[0][5],colors[0][3],((double)distHr)/(double)(shownElementsPerSide));
		     // gradiate(resultNumberColor,colors[6],colors[4],((double)distMin)/(double)(doubleShownElements));
		  } else if((distance (distanceZoneMinute,distanceZoneSecond) < shownElementsPerSide) && (shortestIsSecond||shortestIsMinute)){
		      gradiate(resultColor,colors[0][3],colors[0][0],((double)distMin)/(double)(shownElementsPerSide));
		     // gradiate(resultNumberColor,colors[4],colors[2],((double)distSec)/(double)(doubleShownElements));
		  } else if((distance (distanceZoneHour,distanceZoneSecond) < shownElementsPerSide) && (shortestIsHour||shortestIsSecond)){
		      gradiate(resultColor,colors[0][0],colors[0][5],((double)distSec)/(double)(shownElementsPerSide));
		      //gradiate(resultNumberColor,colors[2],colors[6],((double)distHr)/(double)(doubleShownElements));
		  }else {
		      useInterpolatedColor=false;
		  }

		  if (useInterpolatedColor){

		      evas_object_color_set (ad->ticks[i], resultColor[0],
					     resultColor[1],
					     resultColor[2],
					     alphaValue);
		      evas_object_show (ad->ticks[i]);
		  }else{
		      evas_object_color_set (ad->ticks[i], colors[0][arraypos][0], colors[0][arraypos][1], colors[0][arraypos][2], alphaValue);
		      evas_object_show (ad->ticks[i]);
		  }
		  if (isMinuteNumber) {
		      evas_object_color_set (ad->numTicks[numberPointerDyn], colors[0][numarraypos][0], colors[0][numarraypos][1], colors[0][numarraypos][2], alphaValue);
		      evas_object_show (ad->numTicks[numberPointerDyn]);
		  }

		}
	    }
	}




      view_rotate_object (ad->hand_hour, angle_hour, window_center_x, window_center_y);
      view_rotate_object (ad->hand_minute, angle_minute, window_center_x, window_center_y);

      int batlev;
      device_battery_get_percent (&batlev);
      #ifdef picture_mode
        batlev=85;
      #endif
      short subtract_min_bat_length = (short) (hand_minute_length * (1.0f - (batlev / 100.0f)));
      evas_object_resize (ad->hand_minute_battery, 4, hand_minute_length - subtract_min_bat_length);
      evas_object_move (ad->hand_minute_battery, window_center_x - 2, GeneralGap + daily_border_distance + subtract_min_bat_length);

      view_rotate_object (ad->hand_minute_battery, angle_minute, window_center_x, window_center_y);

      view_rotate_object (ad->hand_second, angle_second, window_center_x, window_center_y);

      if (playerTickSwap)
      	  player_start (ad->playerTick1);
      else
      	  player_start (ad->playerTick2);
        playerTickSwap = !playerTickSwap;

#ifdef picture_mode
                      day_of_week+=1;
                      if(day_of_week>7)
                	day_of_week=1;
                      reset_elements_settings(ad);

#endif
#ifndef picture_mode
	  uint8_t weekday;
	  watch_time_get_day_of_week(watch_time, &weekday);
          if(day_of_week != weekday){
              day_of_week=weekday;
              reset_elements_settings(ad);
          }
#endif

        if(needFullLoop)
          needFullLoop=0;
  }


}


void ambient_tick(appdata_s *ad, watch_time_h watch_time){
    int hour, minute;

    if (watch_time != NULL) {
        watch_time_get_hour (watch_time, &hour);
        watch_time_get_minute (watch_time, &minute);

	#ifdef picture_mode
	  hour = 10; minute = 10;
	#endif


        /* Angles for rotating the hands later on */
        /*float angle_hour, angle_minute, angle_second;
        setTimeAngles (&hour, &minute, &second, &angle_hour, &angle_minute, &angle_second);*/
        /* Converted to unsigned short for simpler calcs
         * 1 degree precision is more than sufficient
         * Might want to switch back to float when I figure out how to update more times in a second
         */
        unsigned short angle_hour, angle_minute;
        setTimeAnglesNoSecond (&hour, &minute, &angle_hour, &angle_minute);

        /* Very high precision but it's not really needed atm */
        /*short distanceZoneHour = angle_hour * 0.167f, distanceZoneMinute =
  	  angle_minute * 0.167f, distanceZoneSecond =
  	  ambient ? 0 : angle_second * 0.167f;*/

        /* Using simpler types because we want to save battery */
        uint8_t distanceZoneHour = (angle_hour/6),
  	      distanceZoneMinute = minute;

        uint8_t distMin,
		distHr,
		winningdist;
        short shortestDistance;

        short opresult = distanceZoneMinute%5;
        // Unused yet
        // int closestTickMinute = opresult > 2 ? distanceZoneMinute + (5 - opresult) : distanceZoneMinute - opresult;

        short alphaValue = 0;
        for (int i = 0; i < 60; i++) {
       	  short numberPointerDyn = i/5;
       	  bool isMinuteNumber = i % 5 == 0;
  	  distHr = distance (distanceZoneHour, i);
  	  distMin = distance (distanceZoneMinute, i);


  	  /* Range check first. Might want to update all after switching to/from AOD */
  	  if (needFullLoop || distHr <= shownElementsPerSide || distMin <= shownElementsPerSide){

  	      winningdist = MIN(distMin, distHr);

  	      /* Find out which one is closer */
  	      bool shortestIsHour, shortestIsMinute;
  	      shortestIsHour = winningdist==distHr;
  	      shortestIsMinute = !shortestIsHour;

  	      /* Find the alpha value */
  	      shortestDistance = shownElementsPerSide - winningdist;
  	      if (shortestDistance < 0)
  		alphaValue = 0;
  	      else{
  		  alphaValue = (shortestDistance*shadow_coeff);
  		//  if (shortestDistance < 100)
  		//      alphaValue = 0;
  	      }

  	      if(alphaValue == 0){
  		if (isMinuteNumber)
  		   evas_object_hide(ad->numTicks[numberPointerDyn]);
  		 evas_object_hide(ad->ticks[i]);

  	      }else{

  	      /* Set numbers if needed */
  	      if (shortestIsMinute)
  		{
  		  if(isMinuteNumber)
  		      if (!showingMinutes[numberPointerDyn]){
  			  char watch_text_buf[50];
  			  snprintf (watch_text_buf, 50, "<font_size=27><align=left>%2d</align></font>", i==0 && (minute > shownElementsPerSide) ? 60 : i);
  			  elm_object_text_set(ad->numTicks[numberPointerDyn], watch_text_buf);
  			  showingMinutes[numberPointerDyn] = 1;
  			}

  		}
  	      else if (shortestIsHour)
  		if (isMinuteNumber && showingMinutes[numberPointerDyn]) {
  		      char watch_text_buf[50];
  		      snprintf (watch_text_buf, 50, "<font_size=27><align=left>%2d</align></font>", i == 0 ? 12 : numberPointerDyn);
  		      elm_object_text_set(ad->numTicks[numberPointerDyn], watch_text_buf);
  		      showingMinutes[numberPointerDyn] = 0;
  		    }

		  /* Finding which color should be picked in the color array */
		  uint8_t arraypos = shortestIsHour ? 5 : 3, numarraypos =
		      arraypos + 1;

		  bool useInterpolatedColor = true;
		  if ((distance (distanceZoneHour, distanceZoneMinute) < shownElementsPerSide) && (shortestIsHour || shortestIsMinute)) {
		      gradiate (resultColor, colors[1][5], colors[1][3], ((double) distHr) / (double) (shownElementsPerSide));
		      // gradiate(resultNumberColor,colors[6],colors[4],((double)distMin)/(double)(doubleShownElements));
		    } else {
		      useInterpolatedColor = false;
		    }

		  if (useInterpolatedColor) {
		      evas_object_color_set (ad->ticks[i], resultColor[0], resultColor[1], resultColor[2], alphaValue);
		      evas_object_show (ad->ticks[i]);
		    } else {
		      evas_object_color_set (ad->ticks[i], colors[1][arraypos][0], colors[1][arraypos][1], colors[1][arraypos][2], alphaValue);
		      evas_object_show (ad->ticks[i]);
		    }

		  if (isMinuteNumber) {
		      evas_object_color_set (ad->numTicks[numberPointerDyn], colors[1][numarraypos][0], colors[1][numarraypos][1], colors[1][numarraypos][2], alphaValue);
		      evas_object_show (ad->numTicks[numberPointerDyn]);
		  }

  	    }
  	  }
  	}

        view_rotate_object (ad->hand_hour, angle_hour, window_center_x,
  			  window_center_y);
        view_rotate_object (ad->hand_minute, angle_minute, window_center_x,
  			  window_center_y);

        uint8_t batlev;
        device_battery_get_percent (&batlev);
	#ifdef picture_mode
	  batlev=85;
	#endif
        short subtract_min_bat_length = (short) (hand_minute_length * (1.0f - (batlev / 100.0f)));
        evas_object_resize (ad->hand_minute_battery, 4, hand_minute_length - subtract_min_bat_length);
        evas_object_move (ad->hand_minute_battery, window_center_x - 2, GeneralGap + daily_border_distance + subtract_min_bat_length);

        view_rotate_object (ad->hand_minute_battery, angle_minute, window_center_x, window_center_y);

        if(needFullLoop)
                  needFullLoop=0;
    }
  }

/* Will be useful when multiple updates in a second will be enabled */
void
requestUpdate(bool ambient, watch_time_h *watch_time, appdata_s *ad){
  if(watch_time == NULL){
    int ret = watch_time_get_current_time (watch_time);
    if (ret != APP_ERROR_NONE)
      dlog_print (DLOG_ERROR, LOG_TAG, "failed to get current time. err = %d", ret);

    if (!ambient)
   	active_tick (ad, watch_time);
         else
   	ambient_tick(ad, watch_time);

       watch_time_delete (watch_time);
  }else if (!ambient)
 	active_tick (ad, watch_time);
       else
 	ambient_tick(ad, watch_time);

}


static void
create_base_gui (appdata_s *ad)
{


  int ret;
  watch_time_h watch_time = NULL;

  // Will delete the object at the end of function
  ret = watch_time_get_current_time (&watch_time);
  if (ret != APP_ERROR_NONE)
    dlog_print (DLOG_ERROR, LOG_TAG, "failed to get current time. err = %d", ret);

  watch_time_get_day_of_week(watch_time, &day_of_week);
  daily_border_distance = shrink_coeff*(day_of_week-1);

  window_center_x = window_width / 2;
  window_center_y = window_height / 2;

  for (int i = 0; i < 12; i++)
    showingMinutes[i] = 0;



  /* Window */
  ret = watch_app_get_elm_win (&ad->win);
  if (ret != APP_ERROR_NONE) {
      dlog_print (DLOG_ERROR, LOG_TAG, "failed to get window. err = %d", ret);
      return;
    }

  evas_object_resize (ad->win, window_width, window_height);

  /* Conformant */
  ad->conform = elm_conformant_add (ad->win);
  evas_object_size_hint_weight_set (ad->conform, EVAS_HINT_EXPAND,
  EVAS_HINT_EXPAND);
  elm_win_resize_object_add (ad->win, ad->conform);
  evas_object_show (ad->conform);

  //char *user_style = "DEFAULT='font=Sans:style=Regular font_size=10'";

  /* Tick & number instantiation and positioning */
  char watch_text_buf[50];
  for (int i = 0; i < 60; i++) {
      short angle = (i / 0.167f) - 90;
      if (angle < 0) {
	  angle += 360;
	}

      /* If it's a number (includes different tick size) */
      if (i % 5 == 0) {

	  /* Number x y positioning values */
	  double xcoeff = (i <= 15 || i > 45 ? (i > 45 ? (i - 45) : (i + 15)) : (45 - i)) / 30.0;
	  double ycoeff = (i <= 30 ? i : 60 - i) / 30.0;

	  ad->numTicks[i / 5] = elm_label_add (ad->conform);

	  evas_object_size_hint_weight_set (ad->numTicks[i / 5], EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	  evas_object_resize (ad->numTicks[i / 5], charsizeX, charsizeY);
	  evas_object_move (ad->numTicks[i / 5],
	      window_center_x
	      + ((window_center_x - borderDistance - tickBorderDistance - daily_border_distance) * cos (to_radians (angle)))
		  - ((i < 10 ? charsizeX / 2.0 : charsizeX) * (xcoeff)),
	      window_center_y
		  + ((window_center_y - borderDistance - tickBorderDistance - daily_border_distance) * sin (to_radians (angle))) - (charsizeY * (ycoeff)));
	  evas_object_color_set (ad->numTicks[i / 5], 250, 250, 250, 0);

	  snprintf (watch_text_buf, 50,
		    "<font_size=27><align=left>%2d</align></font>",
		    i == 0 ? 12 : (int) (i / 5));

	  elm_object_text_set(ad->numTicks[i / 5], watch_text_buf);

	  //elm_entry_text_style_user_push(ad->numTicks[i/5], user_style);

	  evas_object_show (ad->numTicks[i / 5]);
	  evas_object_anti_alias_set (ad->ticks[i / 5], enable_antialiasing);

	  ad->ticks[i] = evas_object_rectangle_add (ad->conform);

	  evas_object_resize (ad->ticks[i], 2, 16);
	  evas_object_move (ad->ticks[i], (window_width / 2) - 1, tickBorderDistance + daily_border_distance);
	  evas_object_color_set (ad->ticks[i], 200, 200, 200, 0);
	  evas_object_show (ad->ticks[i]);

	  view_rotate_object (ad->ticks[i], angle + 90, window_center_x,
			      window_center_y);

	} else {
	  ad->ticks[i] = evas_object_rectangle_add (ad->conform);

	  evas_object_resize (ad->ticks[i], 2, 10);
	  evas_object_move (ad->ticks[i], (window_width / 2) - 1, tickBorderDistance + daily_border_distance);
	  evas_object_color_set (ad->ticks[i], 200, 200, 200, 0);
	  evas_object_show (ad->ticks[i]);

	  view_rotate_object (ad->ticks[i], angle + 90, window_center_x,  window_center_y);
	}
      evas_object_anti_alias_set (ad->ticks[i], enable_antialiasing);
    }

  /* Hand Hour */
  ad->hand_hour = evas_object_rectangle_add (ad->conform);
  evas_object_resize (ad->hand_hour, 4, hand_hour_length);
  evas_object_move (ad->hand_hour, (window_width / 2) - 2, GeneralGap + daily_border_distance);
  evas_object_color_set (ad->hand_hour, 200, 200, 200, 120);
  evas_object_anti_alias_set (ad->hand_hour, enable_antialiasing);
  evas_object_show (ad->hand_hour);

  /* Hand hour LINE
   * not planned to be used anytime soon
   */
  /*ad->hand_hour = evas_object_line_add (ad->conform);
  evas_object_line_xy_set(ad->hand_hour,  (window_width / 2)-(cos(0)*GeneralGap), (window_width / 2)-(sin(0)*GeneralGap), (window_width / 2)-(cos(0)*hand_hour_length), (window_width / 2)-(sin(0)*hand_hour_length));
  //evas_object_resize (ad->hand_hour, 4, hand_hour_length);
  //evas_object_move (ad->hand_hour, (window_width / 2) - 2, GeneralGap);
  evas_object_color_set (ad->hand_hour, 200, 200, 200, 120);
  evas_object_anti_alias_set (ad->hand_hour, enable_antialiasing);
  evas_object_show (ad->hand_hour);*/


  /* Hand Minute */
  ad->hand_minute = evas_object_rectangle_add (ad->conform);
  evas_object_resize (ad->hand_minute, 4, hand_minute_length);
  evas_object_move (ad->hand_minute, (window_width / 2) - 2, GeneralGap + daily_border_distance);
  evas_object_color_set (ad->hand_minute, 200, 200, 200, 120);
  evas_object_anti_alias_set (ad->hand_minute, enable_antialiasing);
  evas_object_show (ad->hand_minute);

  /* Hand Minute Battery */
  ad->hand_minute_battery = evas_object_rectangle_add (ad->conform);
  evas_object_resize (ad->hand_minute_battery, 4, hand_minute_length);
  evas_object_move (ad->hand_minute_battery, (window_width / 2) - 2, GeneralGap + daily_border_distance);
  evas_object_color_set (ad->hand_minute_battery, 250, 195, 77, 250);
  evas_object_anti_alias_set (ad->hand_minute_battery, enable_antialiasing);
  evas_object_show (ad->hand_minute_battery);

  /* Hand Seconds
   * Replaced by the circle
   */
  /*ad->hand_second = evas_object_rectangle_add(ad->conform);
   evas_object_resize(ad->hand_second, 2, hand_second_length);
   evas_object_move(ad->hand_second, (window_width / 2) - 1, GeneralGap);
   evas_object_color_set(ad->hand_second, 200, 100, 100, 200);
   evas_object_anti_alias_set(ad->hand_second, true);
   evas_object_show(ad->hand_second);*/

  /* Circle Hand Seconds */
   char image_filepath[256];
   char *source_filename = "ring2.png";
   char *resource_path = app_get_shared_resource_path();
   snprintf(image_filepath, 256, "%s%s", resource_path, source_filename);
   //dlog_print(DLOG_ERROR, "SHADOWFACE", "RESOURCEPATH:  %s", image_filepath);
   free(resource_path);

  /* Tizen 3 for wearable  implementation. Cannot be used because the minimum version is 2.3.2
   * char *img_path = NULL;
  app_resource_manager_get (APP_RESOURCE_TYPE_IMAGE, "ring2.png", &img_path);
  */
  Evas* canvas = evas_object_evas_get (ad->conform);
  ad->hand_second = evas_object_image_filled_add (canvas);
  if (true /* TODO: implement NULL check */) {
      evas_object_image_load_size_set (ad->hand_second, 15, 15);
      //evas_object_image_file_set (ad->hand_second, image_filepath, NULL);

      evas_object_image_file_set(ad->hand_second, image_filepath, NULL);
      //int err = evas_object_image_load_error_get(ad->hand_second);
      //dlog_print(DLOG_ERROR, "SHADOWFACE", "Error message:  %s", evas_load_error_str(err));

      //int err = evas_object_image_load_error_get(ad->hand_second);
      //dlog_print(DLOG_ERROR, "SHADOWFACE", "Error message:  %s", get_error_message(err));
      //evas_object_image_filled_set(ad->hand_second, img_path);
      evas_object_move (ad->hand_second, (window_width / 2) - 9, GeneralGap + daily_border_distance);
      evas_object_resize (ad->hand_second, 18, 18);
      evas_object_show (ad->hand_second);
      //free (image_filepath); WILL CAUSE CRASH
    }
  evas_object_anti_alias_set (ad->hand_second, enable_antialiasing);

  active_tick (ad, watch_time);
  watch_time_delete (watch_time);

  /* Show window after base gui is set up */
  evas_object_show (ad->win);
}

static bool
app_create (int width, int height, void *data)
{

  window_width = width;
  window_height = height;
  /* Hook to take necessary actions before main event loop starts
   Initialize UI resources and application's data
   If this function returns true, the main loop of application starts
   If this function returns false, the application is terminated */
  appdata_s *ad = data;

  char audio_filepath[256];
  char *source_filename = tick1;
  char audio_filepath2[256];
  char *source_filename2 = tick2;
  char *resource_path = app_get_resource_path();
  snprintf(audio_filepath, 256, "%s/%s", resource_path, source_filename);
  snprintf(audio_filepath2, 256, "%s/%s", resource_path, source_filename2);
  free(resource_path);

  /*error_code = app_resource_manager_init ();
  if (error_code != APP_RESOURCE_ERROR_NONE)
    dlog_print (DLOG_ERROR, LOG_TAG,
		"failed to prepare player: error code = %d", error_code);*/

  error_code = player_create (&ad->playerTick1);
  if (error_code != PLAYER_ERROR_NONE)
    dlog_print (DLOG_ERROR, LOG_TAG, "failed to create");

  char *sound_path = NULL;

  //app_resource_manager_get (APP_RESOURCE_TYPE_SOUND, audio_filepath, &sound_path);
  //dlog_print (DLOG_ERROR, LOG_TAG, "SOUND PATH %s", sound_path);

  error_code = player_set_uri (ad->playerTick1, audio_filepath);
  if (error_code != PLAYER_ERROR_NONE)
    dlog_print (DLOG_ERROR, LOG_TAG, "failed to set URI: error code = %d",
		error_code);

  error_code = player_prepare (ad->playerTick1);
  if (error_code != PLAYER_ERROR_NONE)
    dlog_print (DLOG_ERROR, LOG_TAG,
		"failed to prepare player: error code = %d", error_code);

  /*error_code = app_resource_manager_init ();
  if (error_code != APP_RESOURCE_ERROR_NONE)
    dlog_print (DLOG_ERROR, LOG_TAG,
		"failed to prepare player: error code = %d", error_code);*/

  error_code = player_create (&ad->playerTick2);
  if (error_code != PLAYER_ERROR_NONE)
    dlog_print (DLOG_ERROR, LOG_TAG, "failed to create");
/*
  *sound_path = NULL;
  app_resource_manager_get (APP_RESOURCE_TYPE_SOUND, tick2, &sound_path);
  // dlog_print(DLOG_ERROR, LOG_TAG, "SOUND PATH %s", sound_path);*/

  error_code = player_set_uri (ad->playerTick2, audio_filepath2);
  if (error_code != PLAYER_ERROR_NONE)
    dlog_print (DLOG_ERROR, LOG_TAG, "failed to set URI: error code = %d",
		error_code);

  error_code = player_prepare (ad->playerTick2);
  if (error_code != PLAYER_ERROR_NONE)
    dlog_print (DLOG_ERROR, LOG_TAG,
		"failed to prepare player: error code = %d", error_code);

  player_set_volume (ad->playerTick1, tickVolume, tickVolume);
  player_set_volume (ad->playerTick2, tickVolume, tickVolume);


  shadow_coeff = 255.0f / shownElementsPerSide;
  GeneralGap = 65;
  hand_second_length = 90;
  hand_minute_length = 80;
  hand_hour_length = 60;

  /* Assigning hands&numbers color arrays
   * Refer to colors array declaration
   */

  /* Using color palette */
  assign_color_array (0, 0, 255, 255, 255);

  //assign_color_array(tickSecondColor, 255, 255, 255);
  assign_color_array (0, 1, 255, 66, 148);

  assign_color_array (0, 2, 255, 255, 255);
  //assign_color_array(tickNumberSecondColor, 255, 66, 224);

  assign_color_array (0, 3, 66, 255, 135);

  assign_color_array (0, 4, 255, 248, 69);

  assign_color_array (0, 5, 66, 110, 255);

  assign_color_array (0, 6, 255, 255, 255);

  /* Process AOD dimmed versions */
  dim_color_arrays();

  create_base_gui (ad);

  return true;
}

static void
app_control (app_control_h app_control, void *data)
{
  /* Handle the launch request. */
}

static void
app_pause (void *data)
{
  /* Take necessary actions when application becomes invisible. */
}

static void
app_resume (void *data)
{
  /* Take necessary actions when application becomes visible. */
  appdata_s *ad = data;
  needFullLoop=1;
  requestUpdate(false, NULL, ad);
}

static void
app_terminate (void *data)
{
  /* Release all resources. */
  appdata_s *ad = data;

  player_stop (ad->playerTick1);
  player_unprepare (ad->playerTick1);
  player_destroy (ad->playerTick1);

  player_stop (ad->playerTick2);
  player_unprepare (ad->playerTick2);
  player_destroy (ad->playerTick2);

  //app_resource_manager_release ();

}

static void
app_time_tick (watch_time_h watch_time, void *data)
{
  /* Called at each second while your app is visible. Update watch UI. */
  appdata_s *ad = data;

  player_stop (ad->playerTick1);
  player_stop (ad->playerTick2);

  requestUpdate(false, watch_time, ad);
  //active_tick (ad, watch_time);

}

static void
app_ambient_tick (watch_time_h watch_time, void *data)
{
  /* Called at each minute while the device is in ambient mode. Update watch UI. */
  appdata_s *ad = data;
  requestUpdate(true, watch_time, ad);
  //ambient_tick (ad, watch_time);

}

static void
app_ambient_changed (bool ambient_mode, void *data)
{
  /* Update your watch UI to conform to the ambient mode */
  appdata_s *ad = data;
  if (ambient_mode)
      evas_object_hide (ad->hand_second);
  else
      evas_object_show (ad->hand_second);

  needFullLoop=1;
  requestUpdate(ambient_mode, NULL, ad);
}

static void
watch_app_lang_changed (app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_LANGUAGE_CHANGED*/
  char *locale = NULL;
  app_event_get_language (event_info, &locale);
  elm_language_set (locale);
  free (locale);
  return;
}

static void
watch_app_region_changed (app_event_info_h event_info, void *user_data)
{
  /*APP_EVENT_REGION_FORMAT_CHANGED*/
}

int
main (int argc, char *argv[])
{
  appdata_s ad =
    { 0, };
  int ret = 0;

  watch_app_lifecycle_callback_s event_callback =
    { 0, };
  app_event_handler_h handlers[5] =
    { NULL, };

  event_callback.create = app_create;
  event_callback.terminate = app_terminate;
  event_callback.pause = app_pause;
  event_callback.resume = app_resume;
  event_callback.app_control = app_control;
  event_callback.time_tick = app_time_tick;
  event_callback.ambient_tick = app_ambient_tick;
  event_callback.ambient_changed = app_ambient_changed;

  watch_app_add_event_handler (&handlers[APP_EVENT_LANGUAGE_CHANGED],
			       APP_EVENT_LANGUAGE_CHANGED,
			       watch_app_lang_changed, &ad);
  watch_app_add_event_handler (&handlers[APP_EVENT_REGION_FORMAT_CHANGED],
			       APP_EVENT_REGION_FORMAT_CHANGED,
			       watch_app_region_changed, &ad);

  ret = watch_app_main (argc, argv, &event_callback, &ad);
  if (ret != APP_ERROR_NONE)
    {
      dlog_print (DLOG_ERROR, LOG_TAG, "watch_app_main() is failed. err = %d",
		  ret);
    }

  return ret;
}
