/*
 *  alarm.h -- externs
 */
extern void InitBellAlarm(Window, int, int, XFontStruct *, XmFontList, Pixel, Pixel, Boolean *, Boolean *);
extern void SetAlarm(char *);
extern void SetBell(int);
extern void GetBellSize(int *, int *);
extern void AlarmOff(void);
extern void DrawBell(int);
