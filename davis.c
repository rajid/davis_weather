#define detail_log
#define loop2
#define dofork
// #define dontsend
#define normal_log
// #define openweather

#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/select.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>

// #define WEATHER_DIR "/usr/local/weather"
#define WEATHER_DIR "/home/weather"
#define LOGFILE		"/var/log/davis.log"
#define MINUTES_HOUR	60
#define SECONDS_MINUTE	60
#define SECONDS_HOUR	(MINUTES_HOUR * SECONDS_MINUTE)
#define WUNDER_UPDATE_TIME	3 /* seconds */
#define LOCAL_UPDATE_TIME	30 /* seconds */
#define OPEN_UPDATE_TIME	60 /* seconds */
#define RAIN_ARRAY_SIZE	SECONDS_HOUR / LOCAL_UPDATE_TIME

typedef unsigned int	uint;
typedef unsigned char	uchar;

unsigned char buffer[200];

typedef struct weather_data_ {
    uint	bar_trend;
    float	bar;
    float	in_temp;
    uint	in_humid;
    float	out_temp;
    uint	out_humid;
    float	dew;			/* calculated value */
    uint	wind_speed;
;;    uint	wind_speed_ave;		/* over 10 min. */
    float	wind_speed_ave_10;	/* over 10 min. */
    float	wind_speed_ave_2;	/* over 2 min. */
    float	wind_gust_10;	/* over 10 min. */
    uint	wind_dir;
    uint	wind_gust_dir;
    float	wind_chill;		/* calculated value */
    float	heat_index;
    float	rain_rate;
    float	storm_rain;
    uint	storm_year;
    uint	storm_month;
    uint	storm_day;
    float	rain_daily;
    float	rain_monthly;
    float	rain_yearly;
    float	rain_hourly;
    uint	batt_trans;		/* transmitter battery status */
    float	batt_cons;		/* console voltage */
    time_t	time;			/* unix time stamp */
    uint	year;
    uint	month;
    uint	day;
    uint	hour;
    uint	min;
    uint	sec;
    uint	sunset_hour;
    uint	sunset_min;
  uint	forecast;		/* forecast byte */
} weather_data_t;


typedef struct storm_ {
	uint	year;
	uint	month;
	uint	day;
	float	rain;
} storm_t;
storm_t		last_storm;
int		last_sunset;

weather_data_t	weather;
FILE		*logfile;
int		log_detail=0;


float
f2c (float f) 
{
    return ((f - 32.0) * 5.0 / 9);
}

float
c2f (float c) 
{
    return (((9.0 / 5) * c) + 32.0);
}


/*
 * Routine to calculate the required CRC for the console
 */

unsigned short crc_table [] = {
  0x0,  0x1021,  0x2042,  0x3063,  0x4084,  0x50a5,  0x60c6,  0x70e7,
  0x8108,  0x9129,  0xa14a,  0xb16b,  0xc18c,  0xd1ad,  0xe1ce,  0xf1ef,
  0x1231,   0x210,  0x3273,  0x2252,  0x52b5,  0x4294,  0x72f7,  0x62d6,
  0x9339,  0x8318,  0xb37b,  0xa35a,  0xd3bd,  0xc39c,  0xf3ff,  0xe3de,
  0x2462,  0x3443,   0x420,  0x1401,  0x64e6,  0x74c7,  0x44a4,  0x5485,
  0xa56a,  0xb54b,  0x8528,  0x9509,  0xe5ee,  0xf5cf,  0xc5ac,  0xd58d,
  0x3653,  0x2672,  0x1611,   0x630,  0x76d7,  0x66f6,  0x5695,  0x46b4,
  0xb75b,  0xa77a,  0x9719,  0x8738,  0xf7df,  0xe7fe,  0xd79d,  0xc7bc,
  0x48c4,  0x58e5,  0x6886,  0x78a7,   0x840,  0x1861,  0x2802,  0x3823,
  0xc9cc,  0xd9ed,  0xe98e,  0xf9af,  0x8948,  0x9969,  0xa90a,  0xb92b,
  0x5af5,  0x4ad4,  0x7ab7,  0x6a96,  0x1a71,   0xa50,  0x3a33,  0x2a12,
  0xdbfd,  0xcbdc,  0xfbbf,  0xeb9e,  0x9b79,  0x8b58,  0xbb3b,  0xab1a,
  0x6ca6,  0x7c87,  0x4ce4,  0x5cc5,  0x2c22,  0x3c03,   0xc60,  0x1c41,
  0xedae,  0xfd8f,  0xcdec,  0xddcd,  0xad2a,  0xbd0b,  0x8d68,  0x9d49,
  0x7e97,  0x6eb6,  0x5ed5,  0x4ef4,  0x3e13,  0x2e32,  0x1e51,   0xe70,
  0xff9f,  0xefbe,  0xdfdd,  0xcffc,  0xbf1b,  0xaf3a,  0x9f59,  0x8f78,
  0x9188,  0x81a9,  0xb1ca,  0xa1eb,  0xd10c,  0xc12d,  0xf14e,  0xe16f,
  0x1080,    0xa1,  0x30c2,  0x20e3,  0x5004,  0x4025,  0x7046,  0x6067,
  0x83b9,  0x9398,  0xa3fb,  0xb3da,  0xc33d,  0xd31c,  0xe37f,  0xf35e,
  0x2b1,  0x1290,  0x22f3,  0x32d2,  0x4235,  0x5214,  0x6277,  0x7256,
  0xb5ea,  0xa5cb,  0x95a8,  0x8589,  0xf56e,  0xe54f,  0xd52c,  0xc50d,
  0x34e2,  0x24c3,  0x14a0,   0x481,  0x7466,  0x6447,  0x5424,  0x4405,
  0xa7db,  0xb7fa,  0x8799,  0x97b8,  0xe75f,  0xf77e,  0xc71d,  0xd73c,
  0x26d3,  0x36f2,   0x691,  0x16b0,  0x6657,  0x7676,  0x4615,  0x5634,
  0xd94c,  0xc96d,  0xf90e,  0xe92f,  0x99c8,  0x89e9,  0xb98a,  0xa9ab,
  0x5844,  0x4865,  0x7806,  0x6827,  0x18c0,   0x8e1,  0x3882,  0x28a3,
  0xcb7d,  0xdb5c,  0xeb3f,  0xfb1e,  0x8bf9,  0x9bd8,  0xabbb,  0xbb9a,
  0x4a75,  0x5a54,  0x6a37,  0x7a16,   0xaf1,  0x1ad0,  0x2ab3,  0x3a92,
  0xfd2e,  0xed0f,  0xdd6c,  0xcd4d,  0xbdaa,  0xad8b,  0x9de8,  0x8dc9,
  0x7c26,  0x6c07,  0x5c64,  0x4c45,  0x3ca2,  0x2c83,  0x1ce0,   0xcc1,
  0xef1f,  0xff3e,  0xcf5d,  0xdf7c,  0xaf9b,  0xbfba,  0x8fd9,  0x9ff8,
  0x6e17,  0x7e36,  0x4e55,  0x5e74,  0x2e93,  0x3eb2,   0xed1,  0x1ef0,
};

short int
calc_crc (char *buf, int bufsiz) {
  ushort crc;
  int i;

  for (i = 0 , crc = 0 ; i < bufsiz ; i++) {
    crc = crc_table[((crc >> 8) & 0xFF) ^ buf[i]] ^ (crc << 8);
  }

  return(crc);
}

void
set_time_stamp (weather_data_t *weather, struct tm *tm) 
{

    weather->year = tm->tm_year + 1900;
    weather->month = tm->tm_mon + 1;
    weather->day = tm->tm_mday;
    weather->hour = tm->tm_hour;
    weather->min = tm->tm_min;
    weather->sec = tm->tm_sec;

    if (log_detail) {
	fprintf(logfile, "Timestamp: %d/%d/%d %2d:%02d:%02d\n",
		weather->year, weather->month, weather->day,
		weather->hour, weather->min, weather->sec);
    }
}



uchar *
printhex (unsigned char *buf, int size, int force) 
{
    uchar *src;
    uchar *dst;
    static uchar	buffer[220];
    int i;

    for(src = buf , dst = buffer; (src-buf) < size; src++ , dst++) {
	i = *src >> 4;
	if (i >= 0 && i <= 9)
	    *dst++ = i + '0';
	else
	    *dst++ = (i-10) + 'A';
	i = *src & 0xF;
	if (i >= 0 && i <= 9)
	    *dst = i + '0';
	else
	    *dst = (i-10) + 'A';
    }
    *dst = '\0';
    if (log_detail || force) {
	fprintf(logfile, "(%ld chars)='%s'\n", src - buf, buffer);
    }

    return buffer;
}



int
read_controller (const char	*cmd,
		 int		fd,
		 unsigned char	*buf,
		 int		size,
		 int		wait,
		 int		once)
{
    fd_set		fds;
    struct timeval	timeout;
    int			ret;
    int			nc;
    int			i;
    int			tries;
    int			timeouts;
    

    i = 0;
    timeouts = 0;
    *buf = '\0';

	    nc = write(fd, cmd, strlen(cmd));
	    if (log_detail) {
		fprintf(logfile, "wrote cmd '%s' nc=%d\n", cmd, nc);
	    }
	    timeouts = 1;

    while (wait || ((!wait && timeouts < 3) && i < size)) {
//	if (i == 0) {
#ifdef notdef
	if ((timeouts % 10) == 0) {
	    nc = write(fd, cmd, strlen(cmd));
	    if (log_detail) {
		fprintf(logfile, "wrote cmd '%s' nc=%d\n", cmd, nc);
	    }
	    timeouts = 0;
	}
#endif
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	bzero(&timeout, sizeof(timeout));
	timeout.tv_sec=1;
	timeout.tv_usec=0;
	
	ret = select(fd+1, &fds, NULL, NULL, &timeout);
	switch ( ret ) {
	case -1:
	    fprintf(logfile, "Error(%d) in select\n", errno);
	    return 0;
	    
	case 0:
	    if (log_detail) {
		fprintf(logfile, "Timeout\n");
	    }
	    timeouts++;
	    break;
	    
	default:
	    nc = read(fd, &buf[i], size - i);
	    i += nc;
	    buf[i+1] = '\0';
	    if (log_detail) {
		fprintf(logfile, "nc = %d, size left = %d\n", nc, size - i);
		printhex(buf, i, 0);
	    }
	    if (i >= size)
		return i;
	}
    }
    return i;
}


static int
incr_rain_index (int index) 
{
    if (++index >= RAIN_ARRAY_SIZE)
	index = 0;
    return (index);
}


void
decode_loop (unsigned char *buffer, weather_data_t *weather, struct tm *tm) 
{
    uint i;
    int	year, month, day;
    float f;
    
    if (log_detail) {
	fprintf(logfile, "Bar trend = %d\n", buffer[3]);
    }
    weather->bar_trend = buffer[3];

    i = buffer[7] | (buffer[8] << 8);
    f = i / 1000.0;
    if (log_detail) {
	fprintf(logfile, "Barometer=%2.2f\n", f);
    }
    weather->bar = f;

    i = (buffer[9] & 0xFF) | (buffer[10] << 8);
    f = i / 10.0;
    if (log_detail) {
	fprintf(logfile, "inside temp value is %3.2f\n", f);
    }
    weather->in_temp = f;

    if (log_detail) {
	fprintf(logfile, "inside humidity is %d\n", buffer[11]);
    }
    weather->in_humid = buffer[11];

    i = buffer[12] | (buffer[13] << 8);
    f = i / 10.0;
    if (log_detail) {
	fprintf(logfile, "outside temp value is %3.2f\n", f);
    }
    weather->out_temp = f;
    
    if (log_detail) {
	fprintf(logfile, "wind speed is %d\n", buffer[14]);
    }
    weather->wind_speed = buffer[14];

#ifndef loop2
    if (log_detail) {
	fprintf(logfile, "10 min. ave. wind speed is %d\n", buffer[15]);
    }
    weather->wind_speed_ave = buffer[15];
#endif
    
    i = buffer[16] | (buffer[17] << 8);
    if (log_detail) {
	fprintf(logfile, "wind direction is %d\n", i);
    }
    weather->wind_dir = i;
    
    if (log_detail) {
	fprintf(logfile, "outside humidity is %d%%\n", buffer[33]);
    }
    weather->out_humid = buffer[33];

    i = buffer[41] | (buffer[42] << 8);
    f = i / 100.0;
    if (log_detail) {
	fprintf(logfile, "Rain rate is %1.2f in/hr\n", f);
    }
    weather->rain_rate = f;

    if (log_detail) {
	fprintf(logfile, "UV index is %d\n", buffer[43]);
    }

    i = buffer[44] | (buffer[45] << 8);
    if (log_detail) {
	fprintf(logfile, "Solar radiation value is %d\n", i);
    }

    i = buffer[46] | (buffer[47] << 8);
    f = i / 100.0;
    if (log_detail) {
	fprintf(logfile, "Storm rain so far %2.2f\n", f);
    }
    weather->storm_rain = f;
    
    i = buffer[48] | (buffer[49] << 8);
    year = (i & 0x7F) + 2000; i >>= 7;
    day = i & 0x1F; i >>= 5;
    month = i & 0xF;
    if (log_detail) {
	fprintf(logfile, "Storm start date: %d, %d, %d\n", year, month, day);
    }
    weather->storm_year = year;
    weather->storm_month = month;
    weather->storm_day = day;

    i = buffer[50] | (buffer[51] << 8);
    f = i / 100.0;
    if (log_detail) {
	fprintf(logfile, "Daily rain so far %2.2f\n", f);
    }
    weather->rain_daily = f;
    
    i = buffer[52] | (buffer[53] << 8);
    f = i / 100.0;
    if (log_detail) {
	fprintf(logfile, "Month rain so far %2.2f\n", f);
    }
    weather->rain_monthly = f;
    
    i = buffer[54] | (buffer[55] << 8);
    f = i / 100.0;
    if (log_detail) {
	fprintf(logfile, "Yearly rain so far %2.2f\n", f);
    }
    weather->rain_yearly = f;
    
    if (log_detail) {
	fprintf(logfile, "Transmitter battery status is %d\n", buffer[86]);
    }
    weather->batt_trans = buffer[86];

    i = buffer[87] | (buffer[88] << 8);
    f = ((i * 300.0)/512.0)/100.0;
    if (log_detail) {
	fprintf(logfile, "Console battery %2.2f volts\n", f);
    }
    weather->batt_cons = f;

    weather->forecast = buffer[89];

    i = buffer[93] | (buffer[94] << 8);
    weather->sunset_hour = i / 100;
    weather->sunset_min = i % 100;
    if (log_detail) {
	fprintf(logfile, "Sunset time is %02d:%02d\n",
		weather->sunset_hour, weather->sunset_min);
    }

//#ifndef loop2
    /* Calculated values */
    {
	double	t1, t2;
	double	out_temp;
	double	dew;
	double air2pow;
	double chill;

	out_temp = (weather->out_temp - 32.0) * 5.0/9.0;
	t2 = log(weather->out_humid / 100.0) +
		 (17.67 * out_temp) / (243.5 + out_temp);
	dew = (243.5 * t2) / (17.67 - t2);
	dew = dew * (9.0/5) + 32.0;
	if (log_detail) {
	    fprintf(logfile, "Dew point (calculated) is %2.2lf F\n", dew);
	}
	weather->dew = dew;

	if (weather->wind_speed < 3 || weather->out_temp > 50.0) {
		weather->wind_chill = weather->out_temp;
	} else {
	air2pow = pow((double)weather->wind_speed, 0.16);
	chill = 35.74 + (0.6215 * weather->out_temp) -
	    (35.75 * air2pow) +
	    (0.4275 * weather->out_temp * air2pow);
	weather->wind_chill = chill;
	}
	if (log_detail) {
	    fprintf(logfile, "Wind chill %2.2f F\n", weather->wind_chill);
	}

	{
	    float tc;
	    float wc;
	    float e;
	    float at;
	    float wm;
	    
	    tc = f2c(weather->out_temp);
	    wm = weather->wind_speed * 0.44704;
	    e = ((weather->out_humid * 6.105) / 100) *
		exp((17.27 * tc) / (237.7 + tc));
	    at = tc + (0.33 * e) - (0.7 * wm) - 4.0;

	    if (log_detail) {
		fprintf(logfile, "Australian wind chill %2.2f F\n", c2f(at));
	    }
	}
    }
//#endif
}


void
decode_loop2 (unsigned char *buffer, weather_data_t *weather) 
{
    uint i;
    int	year, month, day;
    float f;
    
    i = buffer[18] | (buffer[19] << 8);
    f = i / 10.0;
    weather->wind_speed_ave_10 = f;
    if (log_detail) {
	fprintf(logfile, "10 min. ave. wind speed is %.1f\n",
	    weather->wind_speed_ave_10);
    }
    
    i = buffer[22] | (buffer[23] << 8);
//    f = i / 10.0; /* I think this is a bug in their firmware */
    weather->wind_gust_10 = i;
    if (log_detail) {
	fprintf(logfile, "10 min. wind gust is %.1f\n",
	    weather->wind_gust_10);
    }
    
    i = buffer[24] | (buffer[25] << 8);
    weather->wind_gust_dir = i;
    if (log_detail) {
	fprintf(logfile, "wind gust direction is %d\n", i);
    }
    
#ifdef davis_bug
    i = buffer[35] | (buffer[36] << 8);
    f = i / 10.0;
    weather->heat_index = f;
    if (log_detail) {
	fprintf(logfile, "heat index is %.1f\n", f);
    }

    i = buffer[37] | (buffer[38] << 8);
    f = i / 10.0;
    weather->wind_chill = f;
    if (log_detail) {
	fprintf(logfile, "wind chill is %.1f\n", f);
    }

    i = buffer[30] | (buffer[31] << 8);
    f = i / 10.0;
    weather->dew = f;
    if (log_detail) {
	fprintf(logfile, "dew point is %.1f\n", f);
    }
#endif
}


/*
* Only call this once every LOCAL_DATE_TIME
*/
void
hourly_rain (weather_data_t *weather) {
	static struct rain_values_ 
	{
	    time_t	time;
	    float	rain;
	} rain_values[RAIN_ARRAY_SIZE];
	static int rain_end_index=0;
	static int rain_start_index=0;
	static float rain_last_hour=0;
	static float last_rain_total=0;
	int	new_rain;

	if (last_rain_total == 0) {
	    last_rain_total = weather->rain_yearly; /* init */
	}

	/*
	 * Move rain_start_index up until it points at a
	 * value which is <= 1 hour ago
	 * Subtract off the rain values we see as we go
	 */
	for ( ; rain_values[rain_start_index].time > 0 &&
		  (weather->time - rain_values[rain_start_index].time)
		  > SECONDS_HOUR;
	      rain_start_index = incr_rain_index(rain_start_index)) {
	    rain_last_hour -= rain_values[rain_start_index].rain;
	}
	if (rain_last_hour < 0) {
	    rain_last_hour = 0;
	    fprintf(logfile, "rain_last_hour reset - was below zero (%.2)\n",
		    rain_last_hour);
	    rain_last_hour = 0;
	}

	/*
	 * If we have seen more rain, tally it
	 */
	if (weather->rain_yearly > last_rain_total) {
	    new_rain = weather->rain_yearly - last_rain_total;
	} else {
	    new_rain = 0;
	}

	/* Save this rain total for future reference */
	last_rain_total = weather->rain_yearly;

	/*
	 * Store away current time and new rain seen
	 */
	rain_values[rain_end_index].time = weather->time;
	rain_values[rain_end_index].rain = new_rain;
	rain_end_index = incr_rain_index(rain_end_index);

	/* Add in the new rain value */
	rain_last_hour += new_rain;

	/*
	 * Now store the latest 'rain over last hour'
	 */
	weather->rain_hourly = rain_last_hour;
	if (log_detail)
	fprintf(logfile, "Hourly rain total is %2.2f in\n", weather->rain_hourly);
	if (log_detail)
	fprintf(logfile, "Rain index is start=%d, end=%d\n", rain_start_index, rain_end_index);
}

void
set_baud (int fd) 
{
    struct termios	termios;

    /*
     * Set speed
     */
    if (tcgetattr(fd, &termios) < 0) {
//    fprintf(log_file, "! Error getting speed errno=%d\n", errno);
	fprintf(logfile, "Error getting attributes - errno=%d\n", errno);
	return;
    }
    
#ifdef notdef
//  termios.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD | HUPCL | CRTSCTS);
//  termios.c_cflag |= (CLOCAL | CREAD);
//  termios.c_cflag |= (bits | stopbits | parity | CLOCAL | CREAD);
    
    // Ignore bytes with parity errors and make terminal raw and dumb.
    termios.c_iflag &= ~(PARMRK | ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);
    termios.c_iflag |= INPCK | IGNPAR | IGNBRK;
//n    termios.c_iflag &= ~(PARMRK | ISTRIP | IGNCR | ICRNL | INLCR | INPCK);
//n    termios.c_iflag |= IXOFF;
    
    // Raw output.
    termios.c_oflag &= ~(OPOST | ONLCR);
//n    termios.c_oflag &= ~(OPOST);
//n    termios.c_oflag |= ONLCR;
    
    // Don't echo characters. Don't generate signals.
    // Don't process any characters.
//    termios.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN | NOFLSH | TOSTOP | FLUSHO | PENDIN);
    termios.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN | NOFLSH | TOSTOP | PENDIN);
    
    termios.c_cflag &= ~(CCAR_OFLOW|CDSR_OFLOW|CDTR_IFLOW|CRTS_IFLOW|ECHO);
    termios.c_cflag |= CLOCAL | CREAD;
#else
    //    termios.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD | CRTSCTS|CDSR_OFLOW|CDTR_IFLOW|MDMBUF);
    termios.c_cflag |= (CS8 | CLOCAL | CREAD | HUPCL);

        // Ignore bytes with parity errors and make terminal raw and dumb.
    termios.c_iflag &= ~(PARMRK | ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON | IXANY);
//    termios.c_iflag |= INPCK | IGNPAR | IGNBRK ;
    termios.c_iflag &= ~(INPCK | IGNPAR | IGNBRK) ;
 
        // Raw output.
    termios.c_oflag &= ~(OPOST | ONLCR);

        // Don't echo characters. Don't generate signals. 
        // Don't process any characters.
    termios.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN | NOFLSH | TOSTOP |FLUSHO|PENDIN);
//    termios.c_lflag |= (FLUSHO|PENDIN);
    termios.c_lflag &= ~(FLUSHO|PENDIN);
#endif
    cfsetspeed(&termios, B19200);
    
    if (tcsetattr(fd, TCSANOW, &termios) < 0) {
	fprintf(logfile, "Error setting attributes - errno=%d\n", errno);
//    fprintf(log_file, "! Error getting speed errno=%d\n", errno);
	return;
    }
}


char *
day_name (weather_data_t *weather, char *type) {
    static char	name[100];

    snprintf(name, sizeof(name), "%s/%s_%04d%02d%02d.dat",
	     WEATHER_DIR, type,
	     weather->year, weather->month, weather->day);

    return (name);
}


FILE *
open_day (char *type, weather_data_t *weather, char *method) {
    FILE	*out_fd;
    char	*name;
    
    name = day_name(weather, type);
    out_fd = fopen(name, method);
    if (out_fd == NULL) {
	fprintf(logfile, "Error opening day file (%s)\n", name);
	return NULL;
    }
    return out_fd;
}

char *
month_name (weather_data_t *weather, char *type) {
    static char	name[100];

    snprintf(name, sizeof(name), "%s/%s_%04d%02d.dat",
	     WEATHER_DIR, type,
	     weather->year, weather->month);

    return (name);
}


FILE *
open_month (char *type, weather_data_t *weather, char *method) {
    char	*name;
    FILE	*out_fd;
    
    name = month_name(weather, type);
    out_fd = fopen(name, method);
    if (out_fd == NULL) {
	fprintf(logfile, "Error opening month temp. file (%s)\n", name);
	return NULL;
    }
    return out_fd;
}

FILE *
open_sunset (weather_data_t *weather, char *method) {
    FILE	*out_fd;
    static char	name[100];

    snprintf(name, sizeof(name), "%s/sunset_%02d.dat",
	     WEATHER_DIR,
	     weather->month);

    out_fd = fopen(name, method);
    if (out_fd == NULL) {
	fprintf(logfile, "Error opening month temp. file (%s)\n", name);
	return NULL;
    }
    return out_fd;
}

void
write_files (weather_data_t *weather) {
    FILE	*out_fd;

/* outside temp & humidity */
    out_fd = open_day("out", weather, "a");
    if (fprintf(out_fd, "%02d/%02d/%02d %02d:%02d:%02d %d %2.2f 0 %d %.2f %.2f\n", 
		weather->year-2000, weather->month, weather->day,
		weather->hour, weather->min, weather->sec,
		weather->forecast,
		weather->out_temp,
		weather->out_humid,
		weather->dew,
		weather->heat_index) <= 0) {
	fprintf(logfile, "Error writing 'out' info - %d\n", errno);
    }
    fclose(out_fd);

/* wind */
    out_fd = open_day("wind", weather, "a");
    if (fprintf(out_fd, "%02d/%02d/%02d %02d:%02d:%02d 0 %d %d 0 %.1f 0 %.2f %0d %.1f\n", 
		weather->year-2000, weather->month, weather->day,
		weather->hour, weather->min, weather->sec,
		weather->wind_dir,
		weather->wind_speed,
		weather->wind_speed_ave_10,
		weather->wind_chill,
		weather->wind_gust_dir,
		weather->wind_gust_10) <= 0) {
	fprintf(logfile, "Error writing 'wind' info - %d\n", errno);
    }
    fclose(out_fd);

/* rain */
    out_fd = open_day("rain", weather, "a");
    if (fprintf(out_fd, "%02d/%02d/%02d %02d:%02d:%02d 0 %2.2f 0 %2.2f 0 0 0 0 %2.2f\n", 
	weather->year-2000, weather->month, weather->day,
	weather->hour, weather->min, weather->sec,
	weather->rain_rate,
	weather->rain_yearly,
	weather->rain_daily) <= 0) {
	fprintf(logfile, "Error writing 'rain' info - %d\n", errno);
    }
    fclose(out_fd);

/* inside temp & humidity */
    out_fd = open_day("in", weather, "a");
    if (fprintf(out_fd, "%02d/%02d/%02d %02d:%02d:%02d 0 %2.2f 0 %d 0 0 %2.2f 0 0\n", 
	weather->year-2000, weather->month, weather->day,
	weather->hour, weather->min, weather->sec,
	weather->in_temp,
	weather->in_humid,
	weather->bar) <= 0) {
	fprintf(logfile, "Error writing 'in' info - %d\n", errno);
    }
    fclose(out_fd);
}


void
write_files_daily (weather_data_t *weather) 
{
    FILE	*fd;
    float	value;
    float	high, low;
    time_t	high_time;
    time_t	low_time;
    time_t	t;
    struct tm	*tm;
    static char	command[100];

    /* Open the relevant file */
    fd = open_day("out", weather, "r");
    if (fd == NULL) {
	return;
    }
    high = -100.0;
    low = 200.0;

    time(&t);
    tm = localtime(&t);
    while (fgets(buffer, sizeof(buffer), fd)) {
	sscanf(buffer, "%d/%d/%d%d:%d:%d%*d%f",
	       &tm->tm_year, &tm->tm_mon, &tm->tm_mday,
	       &tm->tm_hour, &tm->tm_min, &tm->tm_sec,
	       &value);
	tm->tm_year += 100;
	tm->tm_mon--;
	if (value > high) {
	    high = value;
	    high_time = mktime(tm);
	}
	if (value < low) {
	    low = value;
	    low_time = mktime(tm);
	}
    }
    fclose(fd);

    fd = open_month("high", weather, "a");
    if (fd == NULL) {
	return;
    }
    fprintf(fd, "%d %.2f %s", high_time, high, ctime(&high_time));
    fclose(fd);

    fd = open_month("low", weather, "a");
    if (fd == NULL) {
	return;
    }
    fprintf(fd, "%d %.2f %s", low_time, low, ctime(&low_time));
    fclose(fd);

/* rain accumulation w/ timestamp of last accumulation read */
    fd = open_month("accum", weather, "a");
    fprintf(fd, "%.2f %.2f %s", 
	    weather->rain_yearly,
	    weather->rain_daily,
	    asctime(tm));
    fclose(fd);

/*
 * Compress our files
 */
    snprintf(command, sizeof(command), "gzip %s &", day_name(weather, "out")); system(command);
    snprintf(command, sizeof(command), "gzip %s &", day_name(weather, "wind")); system(command);
    snprintf(command, sizeof(command), "gzip %s &", day_name(weather, "rain")); system(command);
    snprintf(command, sizeof(command), "gzip %s &", day_name(weather, "in")); system(command);
    snprintf(command, sizeof(command), "gzip %s &", day_name(weather, "lr")); system(command);
    snprintf(command, sizeof(command), "gzip %s &", day_name(weather, "moisture_kitchen")); system(command);
    snprintf(command, sizeof(command), "gzip %s &", day_name(weather, "moisture_lefler")); system(command);

    /* Reset our time info - absolutely necessary */
    time(&t);
    tm = localtime(&t);
    set_time_stamp(weather, tm);

}


write_sunset (weather_data_t *weather) {
    FILE *fd;
    time_t	t;
    struct tm *tm;

    time(&t);
    tm = localtime(&t);

/* next sunset time */
    if (tm->tm_mday == 1) {
      fd = open_sunset(weather, "w"); /* recreate it */
    } else {
      fd = open_sunset(weather, "a");
    }
    fprintf(fd, "%02d %02d:%02d\n", 
	    tm->tm_mday,
	    weather->sunset_hour,
	    weather->sunset_min);
    fclose(fd);

    last_sunset = tm->tm_mday;
}


void
write_storm_file(storm_t *storm) {
    FILE	*out_fd;
    char	name[100];
    
    snprintf(name, sizeof(name), "%s/storms.dat",
	     WEATHER_DIR);
    out_fd = fopen(name, "a");
    if (out_fd == NULL) {
	fprintf(logfile, "Error opening storm file (%s)\n", name);
	return;
    }
    
    fprintf(out_fd, "%02d/%02d/%02d %2.2f\n", 
	    storm->year,
	    storm->month,
	    storm->day,
	    storm->rain);
    fclose(out_fd);
}

void
save_storm_data (weather_data_t *weather)
{

    if (weather->storm_year != last_storm.year ||
	weather->storm_month != last_storm.month ||
	weather->storm_day != last_storm.day) {
	if (last_storm.year != 0 && last_storm.year != 2127) {
	    write_storm_file(&last_storm);
	}
	last_storm.year = weather->storm_year;
	last_storm.month = weather->storm_month;
	last_storm.day = weather->storm_day;
	last_storm.rain = weather->storm_rain;
    } else {
	last_storm.rain = weather->storm_rain;
    }
}

char			buf[400];

int
read_net (int sock, char *buf, int bufsize) {
	struct timeval		timeout;
	fd_set			fds;
	int			ret;

	FD_ZERO(&fds);
	FD_SET(sock, &fds);
	bzero(&timeout, sizeof(timeout));
	timeout.tv_sec=5;
	timeout.tv_usec=0;

	ret = select(sock+1, &fds, NULL, NULL, &timeout);
	switch ( ret ) {
	case -1:
	    if (log_detail) {
		fprintf(logfile, "Error(%d) in select on wunderground socket\n", errno);
	    }
	    break;

        case 0:
//	  fprintf(logfile, "Timeout\n");
	  break;

        default:
            ret = read(sock, buf, bufsize);
            buf[ret] = '\0';
	    if (log_detail) {
		fprintf(logfile, "ret = %d\n", ret);
	    }
	}

	return ret;
}

int
write_wunderground (weather_data_t *weather, int rapid) {
	int			sock;
	struct addrinfo		hints;
	struct addrinfo		*addrinfo;
	struct protoent		*proto;
	int			size;
	struct tm		*tm;
	int			err;
	time_t			t2;

//	fprintf(logfile, "sending to wunderground\n");
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if (!rapid) {
	    err = getaddrinfo("weatherstation.wunderground.com", "http", &hints, &addrinfo);
	} else {
	    err = getaddrinfo("rtupdate.wunderground.com", "http", &hints, &addrinfo);
	}
	if (err) {
		fprintf(logfile, "Error getting address info (rapid=%d)\n", rapid);
		return(0);
	}

#ifdef detail_log
	time(&t2); fprintf(logfile, "  Creating socket - %s", ctime(&t2));
#endif
	sock = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
	if (sock < 0) {
		fprintf(logfile, "Error getting wunderground socket\n");
		return(0);
	}
#ifdef detail_log
	time(&t2); fprintf(logfile, "  Connecting - %s", ctime(&t2));
#endif
	if (connect(sock, addrinfo->ai_addr, addrinfo->ai_addrlen) < 0) {
		fprintf(logfile, "Error connecting to wunderground errno=%d\n", errno);
		close(sock);
		return(0);
	}
	freeaddrinfo(addrinfo);

	tm = gmtime(&weather->time);
	if (!rapid) {
	    snprintf(buf, sizeof(buf), "GET /weatherstation/updateweatherstation.php?"
		"ID=KCALAMES14&PASSWORD=fd1185f7&"
		"dateutc=%04d-%02d-%02d%%20%02d:%02d:%02d&"
		"tempf=%.1f&humidity=%d&dewptf=%.2f&baromin=%.2f&"
		"winddir=%d&windspeedmph=%d&"
#ifdef loop2
		"windgustmph=%.1f&"
		"windgustmph_10m=%.1f&windgustdir_10m=%d&"
#endif
		"dailyrainin=%.2f&"
		"rainin=%.2f&"
		"softwaretype=raj"
		" HTTP/1.1"
		"\r\n",
		tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec,
		weather->out_temp, weather->out_humid, weather->dew,
		 weather->bar,
		weather->wind_dir, weather->wind_speed,
#ifdef loop2
 weather->wind_gust_10,
 weather->wind_gust_10, weather->wind_gust_dir,
#endif
		weather->rain_daily,
		weather->rain_rate
		);
	} else {
	    snprintf(buf, sizeof(buf), "GET /weatherstation/updateweatherstation.php?"
		"ID=KCALAMES14&PASSWORD=wunderRj58&"
		"dateutc=%04d-%02d-%02d%%20%02d:%02d:%02d&"
		"tempf=%.1f&humidity=%d&dewptf=%.2f&baromin=%.2f&"
		"winddir=%d&windspeedmph=%d&"
#ifdef loop2
		"windgustmph=%.1f&"
		"windgustmph_10m=%.1f&windgustdir_10m=%d&"
#endif
		"dailyrainin=%.2f&"
		"rainin=%.2f&"
		"softwaretype=raj"
		"&realtime=1&rtfreq=3"
		" HTTP/1.1"
		"\r\n",
		tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec,
		weather->out_temp, weather->out_humid, weather->dew,
		 weather->bar,
		weather->wind_dir, weather->wind_speed,
#ifdef loop2
 weather->wind_gust_10,
weather->wind_gust_10, weather->wind_gust_dir,
#endif
		weather->rain_daily,
		weather->rain_rate
		);
	}
#ifdef detail_log
	time(&t2); fprintf(logfile, "  Writing URL - %s - %s", buf, ctime(&t2));
#endif
	size = write(sock, buf, strlen(buf));
	if (log_detail) {
		fprintf(logfile, "Writing '%s'\n", buf);
		fprintf(logfile, "write returns %d\n", size);
	}
	snprintf(buf, sizeof(buf), "Host: weatherstation.wunderground.com\r\n\r\n");
	size = write(sock, buf, strlen(buf));
	if (log_detail) {
		fprintf(logfile, "write returns %d\n", size);
	}

#ifdef detail_log
	time(&t2); fprintf(logfile, "  Reading reply - %s", ctime(&t2));
#endif
	while ((size = read_net(sock, buf, sizeof(buf)-1)) > 0) {
//		if (log_detail) {
			fprintf(logfile, "%d chars='%*s'", size, size, buf);
//		}
	}

	close(sock);
	return(1);
}

#ifdef openweather
int
write_openweather (weather_data_t *weather) {
	int			size;
	struct tm		*tm;
	int			err;

	tm = gmtime(&weather->time);
	snprintf(buf, sizeof(buf), "curl -d '"
		"temp=%.1f&humidity=%d&"
		"wind_dir=%d&"
		"pressure=%.1f&"
		"wind_speed=%.3f&"
		"wind_gust=%.3f&"
		"rain_today=%.1f&"
		"dewpoint=%.2f&"
		"rain_1h=%.2f&"
		"lat=32.756&long=-116.957&"
		"name=Mt Helix SE side' "
		"--user 'rajohnson:openrj58' http://openweathermap.org/data/post >>/tmp/openweather.log 2>&1 &\n",
		f2c(weather->out_temp), weather->out_humid,
		weather->wind_dir,
		weather->bar * 33.8638815,
		weather->wind_speed * 0.44704,
		weather->wind_gust_10 * 0.44704,
		weather->rain_daily * 25.4,
		f2c(weather->dew),
		weather->rain_hourly * 25.4
		);

fprintf(logfile, "%s\n", buf);
	system(buf);
	return(1);
}
#endif


void
incr_log (int i) 
{
    log_detail++;
}

void
decr_log (int i) 
{
    log_detail--;
    if (log_detail < 0)
	log_detail = 0;
}


/*
 * See if we should change to rapid fire updates
 */
int
change_to_rapid (weather_data_t *weather) {

return(0);

  if (weather->wind_speed_ave_10 < 500 && weather->wind_speed_ave_10 > 7) {
    fprintf(logfile, "Rapid mode due to wind speed (%f)\n",
	    weather->wind_speed_ave_10);
    return (1);
  }
  if (weather->wind_gust_10 > 10) {
    fprintf(logfile, "Rapid mode due to wind gust (%f)\n",
	    weather->wind_gust_10);
    return (1);
  }
  if (weather->rain_rate > 0.25) {
    fprintf(logfile, "Rapid mode due to rain rate (%f)\n",
	    weather->rain_rate);
    return (1);
  }

  return (0);
}


void
dead_man (int it) { }


void
set_controller_time(int fd, struct tm *tm) {
  char timebuf[10];
  ushort crc;

  /* Issue SETTIME command and read the ACK */
  read_controller("\n", fd, buffer, 10, 0, 0);
  read_controller("\n", fd, buffer, 10, 0, 0);
  read_controller("SETTIME\n", fd, buffer, 1, 0, 0);
  fprintf(logfile, "SETTIME cmd ACK of 0x%x\n", (ulong)buffer[0]);

  /* Encode the time */
  timebuf[0] = tm->tm_sec;
  timebuf[1] = tm->tm_min;
  timebuf[2] = tm->tm_hour;
  timebuf[3] = tm->tm_mday;
  timebuf[4] = tm->tm_mon + 1;
  timebuf[5] = tm->tm_year;

  /* Calculate the CRC */
  crc = calc_crc(timebuf, 6); /* over first 6 chars */
  timebuf[6] = (char)(crc >> 8);
  timebuf[7] = (char)(crc & 0xFF);
  timebuf[8] = '\0';

  /* Send it */
  read_controller(timebuf, fd, buffer, 1, 0, 0);
  fprintf(logfile, "Time set ACK of 0x%x\n", (ulong)buffer[0]);
}

void
check_controller_time(int fd, struct tm *tm) {
  char timebuf[10];
  static char string[100];
  ushort crc;
  int tries=0;

  do {
    /* Issue SETTIME command and read the ACK */
    read_controller("\n", fd, buffer, 10, 0, 0);
    read_controller("\n", fd, buffer, 10, 0, 0);
    read_controller("GETTIME\n", fd, buffer, 10, 0, 0);
  } while (buffer[0] != '\06' && ++tries < 5);

  if (buffer[0] == '\06' && buffer[2] != tm->tm_min &&
      tm->tm_sec < 45 && tm->tm_sec > 15) {
    fprintf(logfile, "GETTIME=");
    printhex(buffer, 9, 1);
    fprintf(logfile, "\n");
    fflush(logfile);
    snprintf(string, sizeof(string), "mail -s 'weather time sync (cont=%d vs comp=%d)' raj@mischievous.us </dev/null >/dev/null 2>&1",
	     buffer[2], tm->tm_min);
    system(string);
    set_controller_time(fd, tm);
  }
}

/*
 * The MAIN Event!
 */
main (int argc, char **argv) {
    int fd;
    int nc;
    int i;
    int offset;
    int last_mday;
    time_t	t, t2;
    struct tm	*tm;
    int rapid=0;
    time_t	last_local=0;
    time_t	last_wunder=0;
    time_t	last_open=0;
    int wunder_update_time=WUNDER_UPDATE_TIME;
    
#ifdef normal_log
    logfile = fopen("/var/log/davis.log", "a");
#else
    logfile = fdopen(1, "a");
    log_detail = 1;
#endif

#ifdef dofork
    switch ( fork() ) {
    case -1:				/* error */
	fprintf(logfile, "Error in fork - errno=%d\n", errno);
	exit(1);

    case 0:				/* child */
	break;

    default:				/* parent */
	exit(0);
    }
#endif

    signal(SIGUSR1, incr_log);
    signal(SIGUSR2, decr_log);

    //    fd = open("/dev/cu.usbserial-AE01AN5Q", O_RDWR) ;
//    fd = open("/dev/cu.USA19H2642P1.1", O_RDWR) ;
    fd = open("/dev/ttyUSB0", O_RDWR) ;
    if (fd <= 0) {
	fprintf(logfile, "Error (%d) opening device\n", errno);
	exit(1);
    }
    
    set_baud(fd);

/*
 * Init.
 */
    time(&t);
    tm = localtime(&t);
    last_mday = tm->tm_mday;
    last_sunset = tm->tm_mday; // Assume we've already written this
    memset(&last_storm, 0, sizeof(last_storm));
    memset(&weather, 0, sizeof(weather));
    
    read_controller("\n", fd, buffer, 2, 0, 0);
    read_controller("\n", fd, buffer, 2, 0, 0);
    
    set_controller_time(fd, tm);	/* initial set time */

    signal(SIGALRM, dead_man);

    while (1) {
	int i;

        time(&t);
	tm = localtime(&t);

	check_controller_time(fd, tm);

	/*
	 * If we just crossed midnight - write previous day's rain value
	 */
	//	if (tm->tm_mday != last_mday) {
	if (tm->tm_hour == 3 && tm->tm_min == 0) {
	  set_controller_time(fd, tm);
	}
	if (tm->tm_hour == 0 && tm->tm_min == 0 &&
	    tm->tm_mday != last_mday) {
	  write_files_daily(&weather);
	  last_mday = tm->tm_mday; /* record this was done for today */
/* compress the previous day files */
	}

	/*
	 * When we have passed 3am, write the sunset time.
	 * (This avoids problems with daylight savings time changes.)
	 */
	if (tm->tm_hour == 3 && tm->tm_min < 5 && tm->tm_mday != last_sunset) {
	    write_sunset(&weather);
	}

#ifdef detail_log
	time(&t2); fprintf(logfile, "Reading controller - %s", ctime(&t2));
#endif

	read_controller("\n", fd, buffer, 2, 0, 0);
	read_controller("\n", fd, buffer, 2, 0, 0);
	i = read_controller("LPS 1 1\n", fd, buffer, 100, 0, 1);
	if (i != 100 || strncmp("LOO", &buffer[1], 3) != 0 ||
		buffer[5] != 0) {
		sleep(10);
		continue; /* error in the packet somewhere */
	}
	decode_loop(&buffer[1], &weather, tm);

#ifdef loop2
	read_controller("\n", fd, buffer, 2, 0, 0);
	read_controller("\n", fd, buffer, 2, 0, 0);
	i = read_controller("LPS 2 1\n", fd, buffer, 100, 0, 1);
	if (i != 100 || strncmp("LOO", &buffer[1], 3) != 0 ||
		buffer[5] != 1) {
		sleep(10);
		continue; /* error in the packet somewhere */
	}
	decode_loop2(&buffer[1], &weather);
#endif

        set_time_stamp(&weather, tm);
	weather.time = t;

#ifndef dontsend
	save_storm_data(&weather);

	alarm(15);		/* dead man timer */

	if (t - last_local >= LOCAL_UPDATE_TIME) {
#ifdef detail_log
	  time(&t2);   fprintf(logfile, "Writing files - %s", ctime(&t2));
#endif
		hourly_rain(&weather);
		write_files(&weather);
		last_local = t;
	}
	if (t - last_wunder >= wunder_update_time) {
#ifdef detail_log
	  time(&t2);   fprintf(logfile, "Writing wunderground - %s", ctime(&t2));
#endif
		write_wunderground(&weather, rapid);
		last_wunder = t;
	}
#endif
#ifdef openweather
	if (t - last_open >= OPEN_UPDATE_TIME) {
		write_openweather(&weather);
		last_open = t;
	}
#endif

	/*
	 * See if we should change Weather Underground
	 * update intervals between 30 seconds and 3 seconds.
	 */
	if ((rapid = change_to_rapid(&weather)) == 1) {
	    if (wunder_update_time != WUNDER_UPDATE_TIME) {
	      fprintf(logfile, "Changing to rapid mode at %s", ctime(&t));
	    }
	    wunder_update_time = WUNDER_UPDATE_TIME;
	} else {
	    if (wunder_update_time != LOCAL_UPDATE_TIME) {
	      fprintf(logfile, "Changing to normal mode at %s", ctime(&t));
	    }
	    wunder_update_time = LOCAL_UPDATE_TIME;
	}

#ifdef detail_log
	time(&t2); fprintf(logfile, "Sleep for %d seconds - %s",
			   wunder_update_time, ctime(&t2));
#endif
	fflush(logfile);

	alarm(0);		/* cancel the dead man timer */
	sleep(wunder_update_time);
    }
}
