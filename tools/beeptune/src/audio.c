/* Beeptune - sound output, with no libraries:
     Windows   waveOut (winmm, part of Windows)
     Unix      pipes the samples to aplay, pw-cat or paplay (whichever is
               installed), from a child process so the editor never waits
   Build with -DBEEPTUNE_NOAUDIO for none (tests). */
#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(BEEPTUNE_NOAUDIO)

/* silent: the editor "plays" (follows the rows) without sound (tests) */
int audio_start(const int16_t *s, size_t n, int rate, char *msg, size_t msglen) {
    (void)s; (void)n; (void)rate; (void)msg; (void)msglen;
    return 0;
}
void audio_stop(void) {}
const char *audio_backend(void) { return "none (silent build)"; }

#elif defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

static HWAVEOUT wo;
static WAVEHDR hdr;
static int16_t *buf;

void audio_stop(void) {
    if (!wo) return;
    waveOutReset(wo);
    waveOutUnprepareHeader(wo, &hdr, sizeof(hdr));
    waveOutClose(wo);
    wo = NULL;
    free(buf); buf = NULL;
}

int audio_start(const int16_t *s, size_t n, int rate, char *msg, size_t msglen) {
    WAVEFORMATEX fmt;
    audio_stop();
    memset(&fmt, 0, sizeof(fmt));
    fmt.wFormatTag = WAVE_FORMAT_PCM; fmt.nChannels = 1; fmt.nSamplesPerSec = (DWORD)rate;
    fmt.wBitsPerSample = 16; fmt.nBlockAlign = 2; fmt.nAvgBytesPerSec = (DWORD)rate * 2;
    if (waveOutOpen(&wo, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        wo = NULL;
        snprintf(msg, msglen, "can't open the sound device");
        return 1;
    }
    buf = (int16_t *)malloc(n * 2);
    memcpy(buf, s, n * 2);
    memset(&hdr, 0, sizeof(hdr));
    hdr.lpData = (LPSTR)buf; hdr.dwBufferLength = (DWORD)(n * 2);
    waveOutPrepareHeader(wo, &hdr, sizeof(hdr));
    waveOutWrite(wo, &hdr, sizeof(hdr));
    return 0;
}

const char *audio_backend(void) { return "waveOut"; }

#else

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t child;
static const char *player;
static char player_name[16];

/* which command-line player is there? (once) */
static const char *find_player(void) {
    static const struct { const char *name, *cmd; } p[] = {
        {"aplay", "aplay -q -t raw -f S16_LE -c 1 -r %d -"},
        {"pw-cat", "pw-cat -p --format s16 --channels 1 --rate %d -"},
        {"paplay", "paplay --raw --format=s16le --channels=1 --rate=%d"},
    };
    static int done;
    size_t i;
    if (done) return player;
    done = 1;
    for (i = 0; i < sizeof(p) / sizeof(p[0]); i++) {
        char test[64];
        snprintf(test, sizeof(test), "command -v %s >/dev/null 2>&1", p[i].name);
        if (system(test) == 0) { player = p[i].cmd; snprintf(player_name, sizeof(player_name), "%s", p[i].name); break; }
    }
    return player;
}

void audio_stop(void) {
    if (child > 0) {
        kill(-child, SIGTERM);          /* the whole group: sh and the player */
        waitpid(child, NULL, 0);
        child = 0;
    }
}

int audio_start(const int16_t *s, size_t n, int rate, char *msg, size_t msglen) {
    audio_stop();
    if (!find_player()) {
        snprintf(msg, msglen, "no sound: install aplay (alsa-utils), pw-cat or paplay; or export a WAV");
        return 1;
    }
    child = fork();
    if (child < 0) { child = 0; snprintf(msg, msglen, "can't start the player"); return 1; }
    if (child == 0) {
        char cmd[128];
        FILE *f;
        setpgid(0, 0);
        signal(SIGPIPE, SIG_DFL);
        snprintf(cmd, sizeof(cmd), player, rate);
        f = popen(cmd, "w");
        if (f) { fwrite(s, 2, n, f); pclose(f); }
        _exit(0);
    }
    setpgid(child, child);
    return 0;
}

const char *audio_backend(void) { find_player(); return player ? player_name : "none"; }

#endif
