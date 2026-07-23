#include <dolphin/os.h>
#include <limits.h>

static void OSStopwatchClearStats(OSStopwatch* sw) {
    sw->total = 0;
    sw->min = LLONG_MAX;
    sw->max = 0;
    sw->last = 0;
    sw->start = 0;
    sw->hits = 0;
    sw->running = FALSE;
}

void OSInitStopwatch(OSStopwatch* sw, char* name) {
    if (!sw) {
        return;
    }

    sw->name = name;
    OSStopwatchClearStats(sw);
}

void OSStartStopwatch(OSStopwatch* sw) {
    if (!sw) {
        return;
    }

    if (sw->running) {
        return;
    }

    sw->start = OSGetTime();
    sw->running = TRUE;
}

void OSStopStopwatch(OSStopwatch* sw) {
    OSTime now;
    OSTime elapsed;

    if (!sw) {
        return;
    }

    if (!sw->running) {
        return;
    }

    now = OSGetTime();
    elapsed = now - sw->start;
    if (elapsed < 0) {
        elapsed = 0;
    }

    sw->running = FALSE;
    sw->last = elapsed;
    sw->total += elapsed;
    sw->hits += 1;

    if (elapsed < sw->min) {
        sw->min = elapsed;
    }
    if (elapsed > sw->max) {
        sw->max = elapsed;
    }
}

OSTime OSCheckStopwatch(OSStopwatch* sw) {
    if (!sw) {
        return 0;
    }

    if (!sw->running) {
        return sw->total;
    }

    return sw->total + (OSGetTime() - sw->start);
}

void OSResetStopwatch(OSStopwatch* sw) {
    if (!sw) {
        return;
    }

    OSStopwatchClearStats(sw);
}

void OSDumpStopwatch(OSStopwatch* sw) {
    OSTime total;
    OSTime minv;
    OSTime maxv;
    OSTime avg;
    OSTime last;
    u32 hits;
    const char* name;

    if (!sw) {
        OSReport("OSDumpStopwatch: (null)\n");
        return;
    }

    total = OSCheckStopwatch(sw);
    minv = (sw->hits > 0) ? sw->min : 0;
    maxv = (sw->hits > 0) ? sw->max : 0;
    avg = (sw->hits > 0) ? (total / (OSTime)sw->hits) : 0;
    last = sw->last;
    hits = sw->hits;
    name = sw->name ? sw->name : "(unnamed)";

    OSReport("Stopwatch \"%s\":\n", name);
    OSReport("  hits  : %u\n", hits);
    OSReport("  total : %lld ticks (%lld us, %lld ms)\n",
             (long long)total,
             (long long)OSTicksToMicroseconds(total),
             (long long)OSTicksToMilliseconds(total));
    OSReport("  avg   : %lld ticks (%lld us)\n",
             (long long)avg,
             (long long)OSTicksToMicroseconds(avg));
    OSReport("  min   : %lld ticks (%lld us)\n",
             (long long)minv,
             (long long)OSTicksToMicroseconds(minv));
    OSReport("  max   : %lld ticks (%lld us)\n",
             (long long)maxv,
             (long long)OSTicksToMicroseconds(maxv));
    OSReport("  last  : %lld ticks (%lld us)\n",
             (long long)last,
             (long long)OSTicksToMicroseconds(last));
    OSReport("  state : %s\n", sw->running ? "running" : "stopped");
}

