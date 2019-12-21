#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <strings.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Intrinsic.h>

#include <Xm/Xm.h>

#include "alarm.h"
#include "bell.xbm"

/*
 *  Various defines
 */
#define ALARMBELLSEC    (1000000L / TEXTSCROLLTIME)
#define PAD             3
#define RECHECKTIME     (5 * 60)
#define SECONDSPERDAY   (60 * 60 * 24)
#define TEXTSCROLLTIME  250000L

/*
 *  Alarm structure
 */
typedef struct _Alarm {
    long    alarmTime;
    long    alarmSec;
    char    *alarmAnnounce;
} Alarm;

#define TRUE  1
#define FALSE 0

/*
 *  Globals (ugh!)
 */
static char         alarmBuf[BUFSIZ];
static int          alarmBufLen;

static char         alarmFile[BUFSIZ];

static XFontStruct *alarmFontInfo;

static int          alarmIndex;
static int          alarmLen;
static int          alarmBell;
static int          alarmBellCnt;
static Boolean     *alarmOn;
static Boolean      alarmRun;
static Boolean     *alarmState;
static int          alarmWidth;

static int          alarmX;
static int          alarmY;

static Boolean      bellFlash;

static int          bellX;
static int          bellY;

static Alarm        nextAlarm;
static Window       w;

static Pixmap       BellPixmap;
static GC           eraseGC = 0;

extern Display     *dpy;
extern int          screen;
extern Window       root;
extern GC           gc;

static XmFontList   xmFontList;

static void HideBell(void);
static void TextScroll(int);
static void Set_Alarm(void);

void InitBellAlarm(win, width, height, fontInfo, fontList, fg, bg, state, on)
    Window        win;
    int           width, height;
    XFontStruct  *fontInfo;
    XmFontList    fontList;
    Pixel         fg, bg;
    Boolean      *state, *on;
{
    xmFontList = fontList;

    bellX = PAD;
    bellY = PAD;

    alarmX = bell_width + 2 * PAD + fontInfo->max_bounds.width;
    alarmY = PAD;

    w = win;

    alarmFontInfo = fontInfo;
    alarmWidth = width - alarmX;

    alarmLen = (alarmWidth + (fontInfo->max_bounds.width - 1)) /
                   fontInfo->max_bounds.width;
    alarmOn    = on;
    alarmState = state;
    
    if (!eraseGC) {
        XGCValues xgcv;
    
        eraseGC = XCreateGC(dpy, w, 0,  &xgcv);
        XCopyGC(dpy, gc, ~0, eraseGC);

        xgcv.foreground = bg;
        xgcv.background = fg;
        XChangeGC(dpy, eraseGC, GCForeground | GCBackground, &xgcv);
    }
}

void DrawBell(flash)
    int  flash;
{
    Boolean  i;
    
    if (!nextAlarm.alarmTime) {
        return;
    }
    
    i = flash ? (bellFlash = (bellFlash == TRUE) ? FALSE : TRUE) : TRUE;
    
    if (i == TRUE) {
        if (!BellPixmap) {
            BellPixmap = XCreateBitmapFromData(dpy, w,
                                               bell_bits,
                                               bell_width, bell_height);
        }
        XCopyPlane(dpy, BellPixmap, w, gc,
                   0, 0,
                   bell_width, bell_height,
                   bellX, bellY, 0x1);
    } else {
        HideBell();
    }
}

static void HideBell()
{
    XFillRectangle(dpy, w, eraseGC,
                   bellX, bellY, bell_width, bell_height);
    XFlush(dpy);
}

void AlarmAnnounce()
{
    int               i, w;
    char              buf[BUFSIZ];
    struct itimerval  tv;
    
    strcpy(buf, nextAlarm.alarmAnnounce ? nextAlarm.alarmAnnounce : "alarm");
    strcat(buf, " ... ");
    alarmBufLen = (int)strlen(buf);

    w = XTextWidth(alarmFontInfo, buf, alarmBufLen);

    i = alarmWidth;
    strcpy(alarmBuf, buf);

    do {
        strcat(alarmBuf, buf);
    } while ((i -= w) > 0);

    alarmIndex = -alarmLen;
    signal(SIGALRM, TextScroll);
    
    bzero((char *)&tv, sizeof(tv));
    tv.it_interval.tv_usec = tv.it_value.tv_usec = TEXTSCROLLTIME;
    setitimer(ITIMER_REAL, &tv, (struct itimerval *)NULL);
    
    alarmRun = TRUE;

    if (alarmOn) {
        *alarmOn = TRUE;
    }
    if (alarmBell) {
        XBell(dpy, 25);
        alarmBellCnt = ALARMBELLSEC * alarmBell;
    }
}

static void TextScroll(val)
    int  val;
{
    int       x, index;
    XmString  xmString;
    
    if (alarmRun == FALSE) {
        return;
    }
    if (++alarmIndex < 0) {
        x = alarmX - alarmIndex * alarmFontInfo->max_bounds.width;
        index = 0;
    } else {
        if (alarmIndex >= alarmBufLen) {
            alarmIndex -= alarmBufLen;
        }
        x = alarmX;
        index = alarmIndex;
    }
    
    DrawBell(TRUE);
    
    xmString = XmStringCreate(&alarmBuf[index], XmSTRING_ISO8859_1);
    XmStringDrawImage(dpy, w, xmFontList, xmString, gc,
                      x, alarmY, 1024,
                      XmALIGNMENT_BEGINNING,
                      XmSTRING_DIRECTION_L_TO_R, NULL);
    XmStringFree(xmString);
    
    if (alarmBell && --alarmBellCnt <= 0) {
        XBell(dpy, 25);
        alarmBellCnt = ALARMBELLSEC * alarmBell;
    }
    XFlush(dpy);
}

static void ReadAlarmFile(file)
    char  *file;
{
    char       *cp, *tp, *dp;
    int         hour, minute, pm, day;
    FILE       *fp;
    struct tm  *tm;
    long        l, l0;
    time_t      clock;
    char        buf[BUFSIZ];
    
    if (nextAlarm.alarmAnnounce) {
        free((void *)(nextAlarm.alarmAnnounce));
    }
    bzero((char *)&nextAlarm, sizeof(nextAlarm));

    if (!file || !(fp = fopen(file, "r"))) {
        return;
    }

    time(&clock);
    tm = localtime(&clock);
    
    l0 = clock - (60 * (60 * tm->tm_hour + tm->tm_min) + tm->tm_sec);
    
    while (fgets(buf, sizeof(buf), fp)) {
        if (*buf == '#' || !(cp = index(buf, ':')) ||
            !(tp = index(buf, '\t')) || tp < cp) {
            continue;
        }
    
        dp = buf;
        while (isspace(*dp)) {
            dp++;
        }
    
        switch (*dp) {
            case 'S':
            case 's': {
                day = (dp[1] == 'a' || dp[1] == 'A') ? 6 : 0;
                break;
            }
            case 'M':
            case 'm': {
                day = 1;
                break;
            }
            case 'T':
            case 't': {
                day = (dp[1] == 'h' || dp[1] == 'H') ? 4 : 2;
                break;
            }
            case 'W':
            case 'w': {
                day = 3;
                break;
            }
            case 'F':
            case 'f': {
                day = 5;
                break;
            }
            default: {
                day = -1;
                break;
            }
        }

        while (!isdigit(*dp) && dp < cp) {
            dp++;
        }
        *cp++ = 0;
        *tp++ = 0;

        while (isspace(*cp)) {
            cp++;
        }

        if ((minute = atoi(cp)) < 0) {
            minute = 0;
        } else if (minute > 59) {
            minute = 59;
        }

        while (isdigit(*cp)) {
            cp++;
        }

        while (isspace(*cp)) {
            cp++;
        }

        pm = (*cp == 'p' || *cp == 'P');

        if ((hour = atoi(dp)) < 0) {
            hour = 0;
        } else if (hour < 12) {
            if (pm) {
                hour += 12;
            }
        } else if (hour == 12) {
            if (!pm) {
                hour = 0;
            }
        } else if (hour > 23) {
            hour = 23;
        }

        l = l0 + 60 * (60 * hour + minute);

        if (day < 0) {
            if (l < clock) {
                l += SECONDSPERDAY;
            }
        } else {
            if (day < tm->tm_wday || (day == tm->tm_wday && l < clock)) {
                day += 7;
            }
            l += (day - tm->tm_wday) * SECONDSPERDAY;
        }
    
        if (nextAlarm.alarmTime && l >= nextAlarm.alarmTime) {
            continue;
        }
        if (nextAlarm.alarmAnnounce) {
            free((void *)(nextAlarm.alarmAnnounce));
        }
        nextAlarm.alarmTime = l;
    
        while (isspace(*tp)) {
            tp++;
        }

        if ((cp = index(tp, '\n'))) {
            *cp = 0;
        }

        for (cp = tp + strlen(tp) - 1 ; cp >= tp && isspace(*cp) ; ) {
            *cp-- = 0;
        }

        if (!*tp) {
            continue;
        }

        if ((nextAlarm.alarmAnnounce = malloc(strlen(tp) + 1))) {
            strcpy(nextAlarm.alarmAnnounce, tp);
        }
    }

    fclose(fp);
    
    if (nextAlarm.alarmTime) {
        nextAlarm.alarmSec = nextAlarm.alarmTime - clock;
    }
}

void SetAlarm(file)
    char  *file;
{
    ReadAlarmFile(file);
    
    if (!nextAlarm.alarmTime) {
        *alarmFile = 0;
        if (alarmState) {
            *alarmState = FALSE;
        }
        AlarmOff();
        HideBell();
    
        return;
    }
    
    strcpy(alarmFile, file);
    
    DrawBell(FALSE);
    Set_Alarm();
}

static void ResetAlarm()
{
    if (!*alarmFile) {
        return;
    }
    ReadAlarmFile(alarmFile);
    
    if (!nextAlarm.alarmTime) {
        *alarmFile = 0;
        if (alarmState) {
            *alarmState = FALSE;
        }
        HideBell();

        return;
    }

    Set_Alarm();
}

static void Set_Alarm()
{
    struct itimerval tv;
    
    if (alarmState) {
        *alarmState = TRUE;
    }
    if (nextAlarm.alarmSec > RECHECKTIME + 60) {
        nextAlarm.alarmSec = RECHECKTIME;
        signal(SIGALRM, ResetAlarm);
    } else {
        signal(SIGALRM, AlarmAnnounce);
    }
    
    bzero((char *)&tv, sizeof(tv));
    tv.it_value.tv_sec = nextAlarm.alarmSec;
    setitimer(ITIMER_REAL, &tv, (struct itimerval *)NULL);
}

void GetBellSize(bw, bh)
    int *bw, *bh;
{
    *bw = bell_width +  2 * PAD;
    *bh = bell_height + 2 * PAD;
}

void SetBell(seconds)
    int seconds;
{
    alarmBell = seconds;
}

void AlarmOff(void)
{
    struct itimerval tv;
    
    alarmRun = FALSE;

    if (alarmOn) {
        *alarmOn = FALSE;
    }

    bzero((char *)&tv, sizeof(tv));
    setitimer(ITIMER_REAL, &tv, (struct itimerval *)NULL);
}

