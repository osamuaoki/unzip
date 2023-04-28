/* Low-level Amiga routines shared between Zip and UnZip.
 *
 * Contains:  FileDate()
 *            tzset()           [replaces bad C library versions]
 *            gmtime()          [ditto]
 *            localtime()       [ditto]
 *            time()            [ditto]
 *            sendpkt()
 *            Agetch()
 *
 * The first two are used by all Info-Zip programs except fUnZip.  The last two
 * are used by all except the non-CRYPT version of fUnZip.  Probably some of
 * the stuff in here is unused by ZipNote and ZipSplit too...  sendpkt() is
 * used by Agetch() and FileDate(), and by windowheight() in amiga/amiga.c
 * (UnZip).
 */


/* HISTORY/CHANGES
 *  2 Sep 92, Greg Roelofs, Original coding.
 *  6 Sep 92, John Bush, Incorporated into UnZip 5.1
 *  6 Sep 92, John Bush, Interlude "FileDate()" defined, which calls or
 *            redefines SetFileDate() depending upon AMIGADOS2 definition.
 * 11 Oct 92, John Bush, Eliminated AMIGADOS2 switch by determining
 *            revision via OpenLibrary() call.  Now only one version of
 *            the program runs on both platforms (1.3.x vs. 2.x)
 * 11 Oct 92, John Bush, Merged with Zip version and changed arg passing
 *            to take time_t input instead of struct DateStamp.
 *            Arg passing made to conform with utime().
 * 22 Nov 92, Paul Kienitz, fixed includes for Aztec and cleaned up some
 *            lint-ish errors; simplified test for AmigaDOS version.
 * 11 Nov 95, Paul Kienitz, added Agetch() for crypt password input and
 *            UnZip's "More" prompt -- simplifies crypt.h and avoids
 *            use of library code redundant with sendpkt().  Made it
 *            available to FUnZip, which does not use FileDate().
 * 22 Nov 95, Paul Kienitz, created a new tzset() that gets the current
 *            timezone from the Locale preferences.  These exist only under
 *            AmigaDOS 2.1 and up, but it is probably correctly set on more
 *            Amigas than the TZ environment variable is.  We check that
 *            only if TZ is not validly set.  We do not parse daylight
 *            savings syntax except to check for presence vs. absence of a
 *            DST part; United States rules are assumed.  This is better
 *            than the tzset()s in the Amiga compilers' libraries do.
 * 15 Jan 96, Chr. Spieler, corrected the logic when to select low level
 *            sendpkt() (when FileDate(), Agetch() or windowheight() is used),
 *            and AMIGA's Agetch() (CRYPT, and UnZip(SFX)'s UzpMorePause()).
 * 10 Feb 96, Paul Kienitz, re-fiddled that selection logic again, moved
 *            stuff around for clarity.
 * 16 Mar 96, Paul Kienitz, created a replacement localtime() to go with the
 *            new tzset(), because Aztec's is hopelessly broken.  Also
 *            gmtime(), which localtime() calls.
 * 12 Apr 96, Paul Kienitz, daylight savings was being handled incorrectly.
 * 21 Apr 96, Paul Kienitz, had to replace time() as well, Aztec's returns
 *            local time instead of GMT.  That's why their localtime() was bad,
 *            because it assumed time_t was already local, and gmtime() was
 *            the one that checked TZ.
 * 23 Apr 96, Chr. Spieler, deactivated time() replacement for UnZip stuff.
 *            Currently, the UnZip sources do not make use of time() (and do
 *            not supply the working mktime() replacement, either!).
 */

#include <ctype.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <libraries/dos.h>
#include <libraries/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/locale_protos.h>

#ifdef AZTEC_C
#  include <pragmas/exec_lib.h>
#  include <pragmas/dos_lib.h>
#  include <pragmas/locale_lib.h>
#  define ESRCH  ENOENT
#  define EOSERR EIO
#endif

#if defined(LATTICE) || defined(__SASC)
#  include <proto/exec.h>
#  include <proto/dos.h>
#endif

#define OF(x) x               /* so crypt.h prototypes compile okay */
#if defined(ZIP) || defined(FUNZIP)
#  define near                /* likewise */
   typedef unsigned long ulg; /* likewise */
   typedef size_t extent;     /* likewise */
   typedef void zvoid;        /* likewise */
#endif
#include "crypt.h"            /* just so we can tell if CRYPT is defined */


#ifndef FUNZIP

/***********************/
/* Function filedate() */
/***********************/

/*  FileDate() (originally utime.c), by Paul Wells.  Modified by John Bush
 *  and others (see also sendpkt() comments, below); NewtWare SetFileDate()
 *  clone cheaply ripped off from utime().
 */

/* DESCRIPTION
 * This routine chooses between 2 methods to set the file date on AMIGA.
 * Since AmigaDOS 2.x came out, SetFileDate() was available in ROM (v.36
 * and higher).  Under AmigaDOS 1.3.x (less than v.36 ROM), SetFileDate()
 * must be accomplished by constructing a message packet and sending it
 * to the file system handler of the file to be stamped.
 *
 * The system's ROM version is extracted from the external system Library
 * base.
 *
 * NOTE:  although argument passing conforms with utime(), note the
 *        following differences:
 *          - Return value is boolean success/failure.
 *          - If a structure or array is passed, only the first value
 *            is used, which *may* correspond to date accessed and not
 *            date modified.
 */

#  ifndef SUCCESS
#    define SUCCESS (-1L)
#    define FAILURE 0L
#  endif

#  define ReqVers 36L  /* required library version for SetFileDate() */
#  define TZSIZE  40   /* max space allowed for a timezone string    */

extern struct Library *SysBase;

long timezone = 0;     /* should be pretty safe for reentrancy */
int daylight = 0;

LONG FileDate (char *filename, time_t u[]);
void tzset(void);

/* =============================================================== */

LONG FileDate(filename, u)
    char *filename;
    time_t u[];
{
    LONG SetFileDate(UBYTE *filename, struct DateStamp *pDate);
    LONG sendpkt(struct MsgPort *pid, LONG action, LONG *args, LONG nargs);
    struct MsgPort *taskport;
    BPTR dirlock, lock;
    struct FileInfoBlock *fib;
    LONG pktargs[4];
    UBYTE *ptr;
    long ret;

    struct DateStamp pDate;
    time_t mtime;

    tzset();
    mtime = u[0] - timezone;

/* magic number = 2922 = 8 years + 2 leaps between 1970 - 1978 */
    pDate.ds_Days = (mtime / 86400L) - 2922L;
    mtime = mtime % 86400L;
    pDate.ds_Minute = mtime / 60L;
    mtime = mtime % 60L;
    pDate.ds_Tick = mtime * TICKS_PER_SECOND;

    if (SysBase->lib_Version >= ReqVers) {
        return (SetFileDate(filename,&pDate));  /* native routine at 2.0+ */
    }
    else  /* !(SysBase->lib_Version >=ReqVers) */
    {
        if( !(taskport = (struct MsgPort *)DeviceProc(filename)) )
        {
            errno = ESRCH;          /* no such process */
            return FAILURE;
        }

        if( !(lock = Lock(filename,SHARED_LOCK)) )
        {
            errno = ENOENT;         /* no such file */
            return FAILURE;
        }

        if( !(fib = (struct FileInfoBlock *)AllocMem(
            (long)sizeof(struct FileInfoBlock),MEMF_PUBLIC|MEMF_CLEAR)) )
        {
            errno = ENOMEM;         /* insufficient memory */
            UnLock(lock);
            return FAILURE;
        }

        if( Examine(lock,fib)==FAILURE )
        {
            errno = EOSERR;         /* operating system error */
            UnLock(lock);
            FreeMem(fib,(long)sizeof(*fib));
            return FAILURE;
        }

        dirlock = ParentDir(lock);
        ptr = (UBYTE *)AllocMem(64L,MEMF_PUBLIC);
        strcpy((ptr+1),fib->fib_FileName);
        *ptr = strlen(fib->fib_FileName);
        FreeMem(fib,(long)sizeof(*fib));
        UnLock(lock);

        /* now fill in argument array */

        pktargs[0] = 0;
        pktargs[1] = (LONG)dirlock;
        pktargs[2] = (LONG)&ptr[0] >> 2;
        pktargs[3] = (LONG)&pDate;

        errno = ret = sendpkt(taskport,ACTION_SET_DATE,pktargs,4L);

        FreeMem(ptr,64L);
        UnLock(dirlock);

        return SUCCESS;
    }  /* ?(SysBase->lib_Version >= ReqVers) */
} /* FileDate() */


void tzset(void)         /* check Locale and TZ -- the environment overrides */
{
    struct Library *LocaleBase;
    struct Locale *ll;
    char TZstring[TZSIZE], *p;
    struct Process *me = (void *) FindTask(NULL);
    void *old_window = me->pr_WindowPtr;
    BPTR eh;
    int z, valid = FALSE;
    static zone_is_set = FALSE;         /* should be okayish with reentrancy */

    if (zone_is_set)
        return;
    timezone = 5 * 60;                                            /* default */
    me->pr_WindowPtr = (void *) -1;   /* suppress any "Please insert" popups */
    if (SysBase->lib_Version >= ReqVers) {
        if (GetVar("TZ", TZstring, TZSIZE - 1, /* GVF_GLOBAL_ONLY */ 0) <= 0)
            TZstring[0] = 0;
    } else {                    /* early AmigaDOS, get env var the crude way */
        z = 0;
        if (eh = Open("ENV:TZ", MODE_OLDFILE)) {
            z = Read(eh, TZstring, TZSIZE - 1);
            Close(eh);
            if (z < 0)
                z = 0;
        }
        TZstring[z] = '\0';
    }
    if (TZstring[0]) {
        z = 3600;
        for (p = TZstring; *p && !isdigit(*p) && *p != '-'; p++) ;
        if (*p == '-')
            z = -3600, p++;
        if (*p) {
            timezone = 0;
            do {
                while (isdigit(*p))
                    timezone = timezone * 10 + z * (*p++ - '0'), valid = TRUE;
                if (*p == ':') p++;
            } while (isdigit(*p) && (z /= 60) > 0);
        }
        while (isspace(*p)) p++;                      /* probably not needed */
        if (valid)
            zone_is_set = TRUE, daylight = !!*p;   /* a DST name part exists */
    }
    if (!valid && (LocaleBase = OpenLibrary("locale.library", 0))) {
        if (ll = OpenLocale(NULL)) {
            z = ll->loc_GMTOffset;
            if (z == -5) {
                if (eh = Lock("ENV:sys/locale.prefs", ACCESS_READ))
                    UnLock(eh);
                else
                    z = 5; /* bug: locale not initialized, default is bogus! */
            } else
                zone_is_set = TRUE;
            timezone = z * 60;
            daylight = (z >= 4*60 && z <= 9*60);    /* apply in the Americas */
            CloseLocale(ll);
        }
        CloseLibrary(LocaleBase);
    }
    me->pr_WindowPtr = old_window;
}


struct tm *gmtime(const time_t *when)
{
    static struct tm tbuf;   /* this function is intrinsically non-reentrant */
    static short smods[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    long days = *when / 86400;
    long secs = *when % 86400;
    short yell, yday;

    tbuf.tm_wday = (days + 4) % 7;                   /* 1/1/70 is a Thursday */
    tbuf.tm_year = 70 + 4 * (days / 1461);
    yday = days % 1461;
    while (yday >= (yell = (tbuf.tm_year & 3 ? 365 : 366)))
        yday -= yell, tbuf.tm_year++;
    smods[1] = (tbuf.tm_year & 3 ? 28 : 29);
    tbuf.tm_mon = 0;
    tbuf.tm_yday = yday;
    while (yday >= smods[tbuf.tm_mon])
        yday -= smods[tbuf.tm_mon++];
    tbuf.tm_mday = yday + 1;
    tbuf.tm_isdst = 0;
    tbuf.tm_sec = secs % 60;
    tbuf.tm_min = (secs / 60) % 60;
    tbuf.tm_hour = secs / 3600;
#ifdef AZTEC_C
    tbuf.tm_hsec = 0;
#endif
    return &tbuf;
}


struct tm *localtime(const time_t *when)
{
    struct tm *t;
    time_t localwhen;
    int dst = FALSE, sundays, lastweekday;

    tzset();
    localwhen = *when - timezone;
    t = gmtime(&localwhen);
    /* So far we support daylight savings correction by the USA rule only: */
    if (daylight && t->tm_mon >= 3 && t->tm_mon <= 9) {
        if (t->tm_mon > 3 && t->tm_mon < 9)      /* May Jun Jul Aug Sep: yes */
            dst = TRUE;
        else {
            sundays = (t->tm_mday + 6 - t->tm_wday) / 7;
            if (t->tm_wday == 0 && t->tm_hour < 2 && sundays)
                sundays--;           /* a Sunday does not count until 2:00am */
            if (t->tm_mon == 3 && sundays > 0)      /* first sunday in April */
                dst = TRUE;
            else if (t->tm_mon == 9) {
                lastweekday = (t->tm_wday + 31 - t->tm_mday) % 7;
                if (sundays < (37 - lastweekday) / 7)
                    dst = TRUE;                    /* last sunday in October */
            }
        }
        if (dst) {
            localwhen += 3600;
            t = gmtime(&localwhen);                   /* crude but effective */
            t->tm_isdst = 1;
        }
    }
    return t;
}


#ifdef ZIP
time_t time(time_t *tp)
{
    time_t t;
    struct DateStamp ds;
    DateStamp(&ds);
    t = ds.ds_Tick / TICKS_PER_SECOND + ds.ds_Minute * 60
                                      + (ds.ds_Days + 2922) * 86400;
    t = mktime(gmtime(&t));
    /* gmtime leaves ds in the local timezone, mktime converts it to GMT */
    if (tp) *tp = t;
    return t;
}
#endif /* ZIP */

#endif /* !FUNZIP */


#if defined(CRYPT) || !defined(FUNZIP)

/*  sendpkt.c
 *  by A. Finkel, P. Lindsay, C. Sheppner
 *  returns Res1 of the reply packet
 */
/*
#include <exec/types.h>
#include <exec/memory.h>
#include <libraries/dos.h>
#include <libraries/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
*/

LONG sendpkt(pid,action,args,nargs)
struct MsgPort *pid;           /* process identifier (handler message port) */
LONG action,                   /* packet type (desired action)              */
     *args,                    /* a pointer to argument list                */
     nargs;                    /* number of arguments in list               */
{

    struct MsgPort *replyport, *CreatePort(UBYTE *, long);
    void DeletePort(struct MsgPort *);
    struct StandardPacket *packet;
    LONG count, *pargs, res1;

    replyport = CreatePort(NULL,0L);
    if( !replyport ) return(0);

    packet = (struct StandardPacket *)AllocMem(
            (long)sizeof(struct StandardPacket),MEMF_PUBLIC|MEMF_CLEAR);
    if( !packet )
    {
        DeletePort(replyport);
        return(0);
    }

    packet->sp_Msg.mn_Node.ln_Name  = (char *)&(packet->sp_Pkt);
    packet->sp_Pkt.dp_Link          = &(packet->sp_Msg);
    packet->sp_Pkt.dp_Port          = replyport;
    packet->sp_Pkt.dp_Type          = action;

    /* copy the args into the packet */
    pargs = &(packet->sp_Pkt.dp_Arg1);      /* address of 1st argument */
    for( count=0; count<nargs; count++ )
        pargs[count] = args[count];

    PutMsg(pid,(struct Message *)packet);   /* send packet */

    WaitPort(replyport);
    GetMsg(replyport);

    res1 = packet->sp_Pkt.dp_Res1;

    FreeMem((char *)packet,(long)sizeof(*packet));
    DeletePort(replyport);

    return(res1);

} /* sendpkt() */

#endif /* CRYPT || !FUNZIP */


#if defined(CRYPT) || (defined(UNZIP) && !defined(FUNZIP))

/* Agetch() reads one raw keystroke -- uses sendpkt() */

int Agetch(void)
{
    LONG sendpkt(struct MsgPort *pid, LONG action, LONG *args, LONG nargs);
    struct Task *me = FindTask(NULL);
    struct CommandLineInterface *cli = BADDR(((struct Process *) me)->pr_CLI);
    BPTR fh = cli->cli_StandardInput;   /* this is immune to < redirection */
    void *conp = ((struct FileHandle *) BADDR(fh))->fh_Type;
    char longspace[8];
    long *flag = (long *) ((ULONG) &longspace[4] & ~3); /* LONGWORD ALIGNED! */
    UBYTE c;

    *flag = 1;
    sendpkt(conp, ACTION_SCREEN_MODE, flag, 1);         /* assume success */
    Read(fh, &c, 1);
    *flag = 0;
    sendpkt(conp, ACTION_SCREEN_MODE, flag, 1);
    if (c == 3)                                         /* ^C in input */
        Signal(me, SIGBREAKF_CTRL_C);
    return c;
}

#endif /* CRYPT || (UNZIP && !FUNZIP) */
