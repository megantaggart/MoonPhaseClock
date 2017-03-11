#include <Wire.h> 
#include <RtcDS3231.h>
#include <PololuLedStrip.h>
#include <LiquidCrystal.h>

// Create an ledStrip object and specify the pin it will use.
PololuLedStrip<12> ledStrip;

// Create a buffer for holding the colors (3 bytes per color).
#define LED_COUNT 18
rgb_color colors[LED_COUNT];
RtcDS3231<TwoWire> Rtc(Wire);

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(11, 10, 5, 4, 3, 2);

  
#define JD_REMOVAL 2400000

char   g_moon_text[20]="New moon";
double g_zod_ang = 0.0;
int    g_zod_name_id = 0;
double g_moon_norm_phase = 0.0;
double g_sun_lo = 0.0;
double g_moon_lo = 0.0;
double g_moon_dist = 0.0;
double g_moon_illper = 0.0;
double g_jd=0.0;

void setup()
{
  Serial.begin(19200); //choose the serial speed here
  Serial.setTimeout(500);
  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);
  lcd.begin(16, 2);
  // Print a message to the LCD.
  //--------RTC SETUP ------------
  Rtc.Begin();

    // if you are using ESP-01 then uncomment the line below to reset the pins to
    // the available pins for SDA, SCL
    // Wire.begin(0, 2); // due to limited pins, use pin 0 and 2 for SDA, SCL

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    Serial.println();

    if (!Rtc.IsDateTimeValid()) 
    {
        // Common Cuases:
        //    1) first time you ran and the device wasn't running yet
        //    2) the battery on the device is low or even missing

        Serial.println("RTC lost confidence in the DateTime!");

        // following line sets the RTC to the date & time this sketch was compiled
        // it will also reset the valid flag internally unless the Rtc device is
        // having an issue

        Rtc.SetDateTime(compiled);
    }

    if (!Rtc.GetIsRunning())
    {
        Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (now < compiled) 
    {
        Serial.println("RTC is older than compile time!  (Updating DateTime)");
        Rtc.SetDateTime(compiled);
    }
    else if (now > compiled) 
    {
        Serial.println("RTC is newer than compile time. (this is expected)");
    }
    else if (now == compiled) 
    {
        Serial.println("RTC is the same as compile time! (not expected but all is fine)");
    }

    // never assume the Rtc was last configured by you, so
    // just clear them to your needed state
    Rtc.Enable32kHzPin(false);
    Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone); 
    }

int get_max_led_brightness(void)
{
  return 60;
}

void clear_led_strip(void)
{
    for(uint16_t j = 0; j < LED_COUNT; j++)
    {
      colors[j] = rgb_color{0,0,0};
    }
}

void set_segment_brightness(int s, double v)
{
  rgb_color color;
  color.red = get_max_led_brightness()*v;
  color.green = get_max_led_brightness()*v;
  color.blue = get_max_led_brightness()*v;  
  for(uint16_t k  = 0;k<3;k++)
  {
    colors[((5-s)*3)+k] = color;
  }
}

void ShowMoon(double age)
{
  clear_led_strip();
  age *=12;
  if (age < 6)
  {
     int full_seg = abs(age);
     double percentage_next = age-full_seg;
     for (int i=0;i<full_seg;i++)
     {
        set_segment_brightness(i,1);
     }
     set_segment_brightness(full_seg,percentage_next);
  }
  else
  {
     age=12-age;    
     int full_seg = abs(age);
     double percentage_next = age-full_seg;
     for (int i=0;i<full_seg;i++)
     {
        set_segment_brightness(5-i,1);
     }
     set_segment_brightness(5-full_seg,percentage_next);
  }
  ledStrip.write(colors, LED_COUNT);     
}

int msg_num=0;

void loop()
{
  // If any digit is received, we will go into integer parsing mode
  // until all three calls to parseInt return an interger or time out.
  // Read the color from the computer.
 
  calc_astro_data(); 
  ShowMoon(g_moon_norm_phase);

  // set the cursor to column 0, line 1

  char txt[17]="";
  char str_f[16];
  lcd.clear();
  lcd.setCursor((16-strlen(g_moon_text))/2, 0);
  lcd.print(g_moon_text);
  
  RtcDateTime nw = Rtc.GetDateTime();
  switch(msg_num)
  {
    case 4:
        sprintf(txt,"%d/%d/%d %02d:%02d",nw.Day(),nw.Month(),nw.Year(),nw.Hour(),nw.Minute());
      break;
    case 3:
        dtostrf(g_jd, 8, 2, str_f);
        sprintf(txt,"JD 24%s",str_f);
      break;
    case 2:
        dtostrf(g_moon_illper, 4, 2, str_f);
        sprintf(txt,"Illum %s%%",str_f);
      break;
    case 1:
        dtostrf(g_moon_dist, 4, 2, str_f);
        sprintf(txt,"Dst %s EarthR",str_f);
      break;
    case 0:
    default:
        dtostrf(g_moon_norm_phase*29.53, 4, 2, str_f);
        sprintf(txt,"Age %s days",str_f);
      break;
  }

  lcd.setCursor((16-strlen(txt))/2, 1);
  lcd.print(txt);

  msg_num++;
  if (msg_num>4)
  {
    msg_num=0;
  }
  delay(3000);
   
 
}

// normalize values to range 0...1
double normalize( double v)
{
  v = v - floor( v ) ;
  if (v < 0)
  {
    v = v + 1;
  }
  return v;
}

void calc_astro_data() {  // calculate the current phase of the moon
  double AG, IP;                      // based on the current date
  byte phase;                         // algorithm adapted from Stephen R. Schmitt's
                                      // Lunar Phase Computation program, originally
  long YY, MM, K1, K2, K3    ;        // written in the Zeno programming language
                                      // http://home.att.net/~srschmitt/lunarphasecalc.html
  double JD;
  RtcDateTime now = Rtc.GetDateTime();
  int D = now.Day();
  int M = now.Month();
  int Y = now.Year();
  int TME_H = now.Hour();
  int TME_M = now.Minute();

  // calculate julian date
  YY = Y - floor((12 - M) / 10);
  MM = M + 9;
  if(MM >= 12)
  {
    MM = MM - 12;
  }
  
  K1 = floor(365.25 * (YY + 4712));
  K2 = floor(30.6 * MM + 0.5);
  K3 = floor(floor((YY / 100) + 49) * 0.75) - 38;

  JD = K1 + K2 + D + 59;
  if(JD > 2299160)
  {
    JD = JD -K3;
  }
  JD -= JD_REMOVAL;
  double hf=(double)TME_M;
  hf/=60;
  hf+=(double)TME_H;
  hf /=24;
  JD += hf;
  g_jd = JD;
  
  IP = normalize((JD - 51550.1) / 29.530588853);
  g_moon_norm_phase = IP;
  AG = IP*29.53; // age in days
  //phase = IP*39;
  

  if (AG <  1.84566)
  {
    strcpy(g_moon_text,"New moon");
  }
  else if( AG <  5.53699)
  {
    strcpy(g_moon_text,"Waxing crescent");
  }
  else if( AG <  9.22831)
  {
    strcpy(g_moon_text,"First quarter");
  }
  else if( AG <  12.91963)
  {
    strcpy(g_moon_text,"Waxing gibbous");
  }
  else if( AG <  16.61096)
  {
    strcpy(g_moon_text,"Full moon");
  }
  else if( AG <  20.30228)
  {
    strcpy(g_moon_text,"Waning gibbous");
  }
  else if( AG <  23.99361)
  {
    strcpy(g_moon_text,"Last quarter");
  }
  else if( AG <  27.68493)
  {
    strcpy(g_moon_text,"Waning crescent");
  }
  else
  {
    strcpy(g_moon_text,"New moon");
  }

  // Convert phase to radians
  IP = IP*2*3.1415926535897932385;

  g_moon_illper = 50*(1-cos(IP));

  //calculate moon's distance
  double DP = 2*PI*normalize( ( JD - 2451562.2 ) / 27.55454988 );
  g_moon_dist = 60.4 - 3.3*cos( DP ) - 0.6*cos( 2*IP - DP ) - 0.5*cos( 2*IP );

  // calculate moon's ecliptic latitude
  //NP := 2*PI*normalize( ( JD - 2451565.2 ) / 27.212220817 )
  //LA := 5.1*sin( NP )

  //calculate moon's ecliptic longitude
  double RP = normalize( ( JD - 2451555.8 ) / 27.321582241 );
  double LO = 360*RP + 6.3*sin( DP ) + 1.3*sin( 2*IP - DP ) + 0.7*sin( 2*IP );

  double sun_n = JD-2451545.0;
  double sun_L = 360*normalize((280.460 + (0.9856474 * sun_n))/360);
  double sun_g = 2*PI*normalize((357.528 + (0.9856003 * sun_n))/360);

  g_sun_lo = 360*normalize((sun_L + (1.915)*sin(sun_g) + 0.020 * sin(2*sun_g))/360);
  g_moon_lo = 360*normalize(LO/360);
}


